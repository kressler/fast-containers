# Fast Containers - C++ SIMD Containers

## Overview

High-performance header-only container library for C++23 providing:
- **btree**: Cache-friendly B+tree with SIMD search and hugepage support
- **dense_map**: Fixed-size sorted array with SIMD operations
- **Hugepage allocators**: Pooling allocators for TLB optimization

## Stack
- C++23, CMake 3.30+, Catch2 v3.11.0
- Style: Google C++ (clang-format), 80 chars, 2 spaces, `int* ptr`

## Repository Structure

```
include/fast_containers/        # Public API headers
  dense_map.hpp, dense_map.ipp, dense_map_simd.ipp
  btree.hpp, btree.ipp
  hugepage_allocator.hpp, policy_based_hugepage_allocator.hpp, hugepage_pool.hpp
tests/                          # Unit tests (Catch2)
  test_dense_map.cpp, test_btree.cpp, test_hugepage_allocator.cpp, test_policy_based_allocator.cpp
src/
  benchmarks/                   # Google Benchmark microbenchmarks
    dense_map_search_benchmark.cpp, hugepage_allocator_benchmark.cpp
  binary/                       # btree_benchmark, btree_stress
scripts/
  interleaved_btree_benchmark.py  # A/B testing harness for rigorous benchmarking
third_party/                    # Git submodules (catch2, benchmark, histograms, abseil-cpp, lyra, unordered_dense)
```

**Header-Implementation Separation**: Template implementations are in `.ipp` files (included at end of `.hpp`) for cleaner interfaces.

## Build Commands

| Build Type | Command | Key Flags |
|------------|---------|-----------|
| Debug | `cmake -S . -B cmake-build-debug && cmake --build cmake-build-debug --parallel` | None |
| Debug+AVX2 | `cmake -S . -B cmake-build-debug -DENABLE_AVX2=ON && cmake --build cmake-build-debug --parallel` | -mavx2 -mfma |
| Debug+ASAN | `cmake -S . -B cmake-build-asan -DENABLE_ASAN=ON && cmake --build cmake-build-asan --parallel` | -fsanitize=address |
| Release | `cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release && cmake --build cmake-build-release --parallel` | -O3 -mavx2 -march=haswell |

**Quick commands**:
- Test: `ctest --test-dir cmake-build-{debug,release,asan} --output-on-failure`
- Format: `cmake --build cmake-build-debug --target format`
- Clang-tidy: `cmake --build build --target clang-tidy`

**CMake Presets**: Use `cmake --list-presets` to see available presets, then:
```bash
cmake --preset release && cmake --build --preset release && ctest --preset release
```

## dense_map<Key, Value, Length, Compare, SearchMode>

### Core Design
- **Storage**: Separate `std::array<Key, Length>` and `std::array<Value, Length>` (SoA layout)
- **Rationale**: Enable SIMD key scans, cache-friendly searches
- **Constraint**: Fixed compile-time size, keys always const
- **Complexity**: Find O(log n), Insert/Remove O(n)

### Iterator Limitations (proxy pattern)
```cpp
// ❌ for (auto& pair : arr)      // Won't compile
// ✅ for (auto pair : arr)       // Correct - proxy still modifies array
// ❌ it->first = val;            // first is const
// ✅ it->second = val;           // second is mutable
```

## Performance: SearchMode Selection

| SearchMode | Use When | Size | Workload |
|------------|----------|------|----------|
| **SIMD** | Read-heavy | >32 | Negative lookups, find-dominant |
| **Linear** | Write-heavy | <32 | Frequent insert/erase, early exit benefits |
| **Binary** | Balanced | >64 | Mixed read/write, or no AVX2 |

### SIMD Find Performance (vs Linear)
- Size 32: 45% faster
- Size 64: 59% faster

### Why SIMD Loses on Small Arrays
**Root Cause**: IPC bottleneck, not cache misses
- Linear: IPC 5.76, 26.0% cache miss
- SIMD: IPC 3.68, 28.5% cache miss
- Simple scalar ops pipeline better than complex SIMD on small data

### SIMD with Large Values (>128 bytes)
SIMD provides minimal benefit (~1%) when performance is dominated by:
- Value movement during splits/merges
- Cache misses from large values
- Memory bandwidth, not comparison speed

**Best use cases for SIMD**:
- Small values (<64 bytes)
- Read-heavy workloads
- Smaller node sizes (more comparisons per operation)

## SIMD Implementation Details

- AVX2: `_mm256_cmpeq_epi32`, `_mm256_movemask_epi8`
- Scans 8 int32 keys in parallel
- Progressive chunking for data movement: AVX2 (32B) → SSE (16B) → scalar (1B)
- Requires: `std::is_trivially_copyable_v<T>`
- Cache line alignment: `alignas(64)` for 4.3% cache improvement
- **Supports std::greater**: Zero-overhead compile-time dispatch for ascending/descending order

## Byte Array SIMD: Not Viable

**Attempted**: SIMD support for arbitrary byte arrays (16/32-byte keys)

**Result**: 2-3× slower than binary search due to scalar encoding overhead
- Each SIMD comparison requires: memcpy, bswap, XOR, vector construction
- Encoding cost dominates any parallelism benefits

**Recommendation**: Use `SearchMode::Binary` for byte arrays. SIMD only benefits native primitive types (int32_t, uint32_t, int64_t, uint64_t, float, double).

## btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchMode, Allocator>

**Note**: MoveMode parameter was removed - compiler automatically uses AVX instructions.

### Key Implementation Patterns

#### Bulk Transfer Operations (O(n²) → O(n))
```cpp
// ❌ Element-by-element (shifts array N times)
for (auto& elem : source) dest.insert(elem.first, elem.second);

// ✅ Bulk transfer (shifts array once)
dest.transfer_prefix_from(source, count);
```

#### Template Methods Eliminate Duplication
```cpp
template <typename NodeType>
void merge_with_left_sibling(NodeType* node) {
  if constexpr (std::is_same_v<NodeType, LeafNode>) {
    // Leaf-specific logic
  } else {
    // InternalNode-specific logic
  }
}
```

#### Memory Safety: Capture Before Invalidation
```cpp
// ❌ WRONG: Accessing after transfer empties the container
node->data.transfer_prefix_from(sibling->data, count);
const Key min = sibling->data.begin()->first;  // UB: sibling is empty!

// ✅ CORRECT: Capture before transfer
const Key min = sibling->data.begin()->first;
node->data.transfer_prefix_from(sibling->data, count);
```

### Default Node Size Heuristics

Empirically validated formulas for optimal cache efficiency:

**Internal Nodes**: Target 1KB (16 cache lines)
```cpp
optimal_size = clamp(round_to_8(1024 / (sizeof(Key) + sizeof(void*))), 16, 64);
```
Rationale: Internal nodes only move 8-byte pointers → cheap, strict cache alignment optimal

**Leaf Nodes**: Target 2KB (32 cache lines)
```cpp
optimal_size = clamp(round_to_8(2048 / (sizeof(Key) + sizeof(Value))), 8, 64);
```
Rationale: Leaf nodes move entire values → expensive, larger nodes amortize split cost

**Key Findings**:
- 2× target difference (Internal 1KB vs Leaf 2KB) due to data movement cost
- Formula exact for 64-256 byte values, conservative for 24-32 byte values
- ~64 bytes is inflection point

### std::map API Compatibility

**Critical Bug Pattern**: Check key existence BEFORE checking if node is full
```cpp
// ❌ WRONG: Unnecessary splits on repeated operator[] calls
if (leaf->data.size() >= LeafNodeSize) return split_leaf(...);
auto existing = leaf->data.find(key);

// ✅ CORRECT: Check existence first
auto pos = leaf->data.lower_bound(key);
if (pos != leaf->data.end() && pos->first == key) return {iterator(leaf, pos), false};
if (leaf->data.size() >= LeafNodeSize) return split_leaf(...);
```

**Optimization**: Use `insert_hint()` to eliminate duplicate search
```cpp
// After lower_bound(), use hint instead of re-searching
auto [it, inserted] = leaf->data.insert_hint(pos, key, value);
```

## Benchmark Best Practices

### Performance Measurement
- ❌ Avoid `PauseTiming()` for operations <100ns (overhead dominates)
- ✅ Pre-populate outside `for (auto _ : state)` loop
- ✅ Measure realistic operations (RemoveInsert, not just Insert)
- ✅ Always `benchmark::DoNotOptimize(result)`

### perf Analysis
```bash
perf stat -e cycles,instructions,cache-references,cache-misses \
  ./benchmark --benchmark_filter="pattern"
# IPC = instructions / cycles
# Cache miss rate = cache-misses / cache-references * 100
```

### Interleaved A/B Testing
`scripts/interleaved_btree_benchmark.py` runs forward/reverse pass patterns to reduce variance from ~6% to ~1%.

## Template Interface/Implementation Separation

**.hpp files**: Interface declarations only
**.ipp files**: Implementations (NO header guards, included at end of .hpp)

**Keep in .hpp**:
- Header guards, includes, namespace
- Enums, concepts, type aliases
- Class declarations, method declarations
- Trivial one-liners (`size()`, `empty()`, `begin()`, `end()`)

**Move to .ipp**:
- All non-trivial method implementations
- Private helper implementations
- Template method specializations

**Benefits**: 49-68% smaller headers, better compile times, clearer interfaces

## GitHub Workflow Tips

### Updating PR Descriptions
`gh pr edit` fails with GraphQL error? Use REST API:
```bash
cat > /tmp/pr_body.md <<'EOF'
Your PR description...
EOF

gh api --method PATCH \
  -H "Accept: application/vnd.github+json" \
  repos/kressler/fast-containers/pulls/<NUM> \
  -F body="$(cat /tmp/pr_body.md)"
```

## Git Submodule Management

```bash
# Add new submodule
git submodule add <url> third_party/<name>

# Update to latest
git submodule update --remote third_party/<name>

# Use in CMake (INTERFACE library pattern)
add_subdirectory(third_party/<name>)
target_link_libraries(your_target PRIVATE <name>::<name>)
```

## Bulk Code Modifications

For complex refactoring (e.g., 143 lambda signature changes), use `perl` not `sed`:

```bash
# Perl handles complex patterns reliably
perl -i -pe 's/complex_pattern/replacement/g' file.cpp

# Verify changes
grep -c "old_pattern" file.cpp  # Should be 0
grep -c "new_pattern" file.cpp  # Should match expected count

# Compile to catch errors
cmake --build build-dir --target your_target
```

## Common Pitfalls

| Issue | Wrong | Correct |
|-------|-------|---------|
| Iterator refs | `for (auto& p : arr)` | `for (auto p : arr)` |
| Key modification | `it->first = val` | `arr.erase(old); arr.insert(new, val)` |
| Submodule update | `git pull` | `git submodule update --remote` |

## Setup

```bash
git clone --recursive https://github.com/kressler/fast-containers.git
cd fast-containers
./setup-dev.sh  # Installs pre-commit hooks (auto-format + clang-tidy)
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --parallel
ctest --test-dir cmake-build-release --output-on-failure
```

## Contributing

1. Run `./setup-dev.sh` (installs pre-commit hooks for auto-format and clang-tidy)
2. Write Catch2 tests for new functionality
3. Follow Google C++ Style Guide (enforced by clang-format)
4. Production code must be clang-tidy clean (enforced in CI and pre-commit)
5. Update CLAUDE.md with learnings and patterns

## Limitations

- dense_map: Fixed compile-time size, no reallocation
- Iterator proxy pattern (use `auto`, not `auto&`)
- Move-only values untested

## Future Optimizations

- [ ] AVX2 vectorized binary search
- [ ] Prefetch hints
- [ ] AVX-512 support
- [x] 64-byte cache line alignment
- [x] Custom comparators (std::less and std::greater)
- [ ] Move semantics for insert
