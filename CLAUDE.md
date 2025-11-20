# Fast Containers - C++ SIMD Containers

## Stack
- C++23, CMake 3.30+, Catch2 v3.11.0, Google Benchmark v1.9.4, Lyra 1.6.1
- Style: Google C++ (clang-format), 80 chars, 2 spaces, `int* ptr`

## Structure
```
src/
  ordered_array.hpp (interface)      # 554 lines
  ordered_array.ipp (implementation) # 958 lines
  btree.hpp (interface)              # 667 lines
  btree.ipp (implementation)         # 1532 lines
  tests/, benchmarks/, binary/
third_party/{catch2/, benchmark/, lyra/}  # submodules
hooks/, install-hooks.sh
```

**Header-Implementation Separation**: Template implementations are in `.ipp` files (included at end of `.hpp`) for cleaner interfaces.

## Build Commands

| Build Type | Command | Flags Enabled |
|------------|---------|---------------|
| Debug | `cmake -S . -B cmake-build-debug && cmake --build cmake-build-debug --parallel` | None |
| Debug+AVX2 | `cmake -S . -B cmake-build-debug -DENABLE_AVX2=ON && cmake --build cmake-build-debug --parallel` | AVX2 |
| Debug+ASAN | `cmake -S . -B cmake-build-asan -DENABLE_ASAN=ON && cmake --build cmake-build-asan --parallel` | -fsanitize=address, -fno-omit-frame-pointer, -g, -O1 |
| Release (default) | `cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release && cmake --build cmake-build-release --parallel` | -O3, -mavx2, -mfma, -march=haswell, -ffast-math, -funroll-loops, -mtune=native |
| Release (no AVX2) | `cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release -DENABLE_AVX2=OFF && cmake --build cmake-build-release --parallel` | -O3, -march=native |

- Test: `ctest --test-dir cmake-build-{debug,release,asan} --output-on-failure`
- Format: `cmake --build cmake-build-debug --target format`
- AVX2 CPUs: Intel Haswell 2013+, AMD Excavator 2015+

### AddressSanitizer (ASAN)
ASAN detects memory errors at runtime (buffer overflows, use-after-free, memory leaks).

**Use when**:
- Debugging crashes or unexpected behavior
- Testing SIMD code for out-of-bounds access
- Validating array boundary checks
- Finding memory leaks

**Performance**: ~2x slowdown, ~3x memory usage (debug builds only)

## ordered_array<Key, Value, Length, SearchMode, MoveMode>

### Core Design
- **Storage**: Separate `std::array<Key, Length>` and `std::array<Value, Length>` (SoA layout)
- **Rationale**: Enable SIMD key scans, cache-friendly searches, independent prefetch
- **Constraint**: Fixed compile-time size, keys always const
- **Complexity**: Find O(log n), Insert/Remove O(n)

### Iterator Limitations (proxy pattern)
```cpp
// ❌ for (auto& pair : arr)      // Won't compile
// ✅ for (auto pair : arr)       // Correct - proxy still modifies array
// ❌ it->first = val;            // first is const
// ✅ it->second = val;           // second is mutable
```

### Comparable Concept
```cpp
template<typename T>
concept Comparable = requires(T a, T b) {
  { a < b } -> std::convertible_to<bool>;
  { a == b } -> std::convertible_to<bool>;
};
```

## Performance: SearchMode Selection

| SearchMode | Use When | Size | Workload |
|------------|----------|------|----------|
| **SIMD** | Read-heavy | >32 | Negative lookups, find-dominant |
| **Linear** | Write-heavy | <32 | Frequent insert/erase, early exit benefits |
| **Binary** | Balanced | >64 | Mixed read/write, or no AVX2 |

### SIMD Find Performance (vs Linear)
- Size 8: 1.57ns (7% faster)
- Size 16: 1.68ns (24% faster)
- Size 32: 2.05ns (45% faster)
- Size 64: 2.43ns (59% faster)

### RemoveInsert Performance (Size 8-32)
| Size | Binary | Linear | SIMD | Winner |
|------|--------|--------|------|--------|
| 8 | 10.5ns | **9.17ns** | 18.2ns | Linear (99% faster) |
| 16 | 16.6ns | **13.3ns** | 20.8ns | Linear (56% faster) |
| 32 | **18.9ns** | 14.7ns | 22.8ns | Binary (20% faster) |
| 64 | **17.6ns** | 24.2ns | 23.3ns | Binary (32% faster) |

### Why SIMD Loses on Small Arrays (Root Cause: IPC, Not Cache)
- **perf stat** (Size 32, 1B iterations):
  - Linear: 7.47B cycles, 43.1B instr, **IPC 5.76**, 26.0% cache miss
  - SIMD: 7.96B cycles, 29.3B instr, **IPC 3.68**, 28.5% cache miss
- **Bottleneck**: Linear has 56% better IPC despite 47% more instructions
- **Cause**: Simple scalar ops pipeline better than complex SIMD on small data

### SIMD Performance: Large Keys (16/32 bytes) with Large Values

**Context**: B+tree benchmarks with 100K elements, 16/32-byte keys, 256-byte values

| Key Size | Operation | Linear | SIMD | Difference |
|----------|-----------|--------|------|------------|
| 16 bytes | Find | 9.64s | 9.65s | ~0% (equivalent) |
| 16 bytes | Erase | 7.96s | 8.18s | +2.7% slower |
| 32 bytes | Find | 10.78s | 10.70s | -0.8% faster |
| 32 bytes | Erase | 8.95s | 8.84s | -1.2% faster |

**Key findings**:
- SIMD provides **minimal benefit** (~1% either direction) with large values
- Performance dominated by:
  - Value movement during splits/merges (256-byte copies)
  - Cache misses (256-byte values thrash cache)
  - Memory bandwidth, not comparison speed

**When SIMD helps more**:
- ✅ Small values (<64 bytes) - comparison cost is significant
- ✅ Read-heavy workloads (find-dominant, minimal splits)
- ✅ Smaller node sizes (more comparisons per operation)
- ⚠️ Large values (>128 bytes) - memory-bound, minimal benefit

**Recommendation**: Keep SIMD for API consistency across all key sizes (4, 8, 16, 32 bytes). No performance regression vs Linear, and may help with different node configurations.

## SIMD Implementation Details

### MoveMode: AVX2 Data Movement (32→16→1 byte fallback)
```cpp
// Progressive chunking: AVX2 (32B) → SSE (16B) → scalar (1B)
while (num_bytes >= 32) _mm256_storeu_si256(dst, _mm256_loadu_si256(src));
if (num_bytes >= 16) _mm_storeu_si128(dst, _mm_loadu_si128(src));
while (num_bytes > 0) *dst++ = *src++;
```
- Requires: `std::is_trivially_copyable_v<T>`
- Use unaligned loads (`loadu`), byte-based arithmetic
- Insert gains: 8-20% faster (Size 8-64)
- Cache line alignment: `alignas(64)` for 4.3% cache improvement

### SearchMode: SIMD Linear Search
- AVX2: `_mm256_cmpeq_epi32`, `_mm256_movemask_epi8`
- Scans 8 int32 keys in parallel
- Best on read-heavy, large arrays (>32 elements)

## btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchMode, MoveMode>

### Implementation Learnings (PR #35)

#### Bulk Transfer Operations (O(n²) → O(n))
```cpp
// ❌ Element-by-element (shifts array N times for N elements)
for (auto& elem : source) {
  dest.insert(elem.first, elem.second);
}

// ✅ Bulk transfer (shifts array once)
dest.transfer_prefix_from(source, count);
dest.transfer_suffix_from(source, count);
```

#### Template Methods Eliminate Duplication (~300+ lines saved)
```cpp
// ✅ Single templated method handles both LeafNode and InternalNode
template <typename NodeType>
void merge_with_left_sibling(NodeType* node) {
  if constexpr (std::is_same_v<NodeType, LeafNode>) {
    // Leaf-specific logic
  } else {
    // InternalNode-specific logic
  }
}
```

#### Templated Lambdas with Compile-Time Branching
```cpp
// ✅ Single lambda handles multiple child types
auto process = [&]<typename ChildPtrType, bool UpdateParents>(auto& children) {
  // ... common logic ...
  if constexpr (UpdateParents) {
    it->second->parent = node;  // Only compiled when UpdateParents=true
  }
};
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

#### Const Correctness
```cpp
// Mark variables const when never modified after initialization
const size_type count = source.size();
const bool needs_merge = size < threshold;
const Key& min_key = node->begin()->first;
```

#### Code Clarity
```cpp
// ❌ Unnecessary intermediate variables
auto next_it = it;
++next_it;
return next_it->second;

// ✅ Direct manipulation
++it;
return it->second;
```

### B+ Tree Structure
- **Leaves**: Store actual data, linked list for range queries
- **Internal nodes**: Store child pointers, guide searches
- **Rebalancing**: Hysteresis thresholds prevent thrashing
- **Underflow handling**: Borrow from siblings → Merge → Cascade up tree

### Phases 10-15: std::map API Compatibility (PRs #40-45)

#### Phase 10: operator[] - Insert/Access with Default Construction
**Implementation**:
```cpp
Value& operator[](const Key& key) {
  auto [it, inserted] = insert(key, Value{});
  return it->second;
}
```

**Critical Bug Fixed**: `insert()` was checking if leaf was full BEFORE checking if key exists
```cpp
// ❌ WRONG: Unnecessary splits on repeated operator[] calls
if (leaf->data.size() >= LeafNodeSize) {
  return split_leaf(leaf, key, value);
}
auto existing = leaf->data.find(key);

// ✅ CORRECT: Check existence first
auto pos = leaf->data.lower_bound(key);
if (pos != leaf->data.end() && pos->first == key) {
  return {iterator(leaf, pos), false};
}
if (leaf->data.size() >= LeafNodeSize) {
  return split_leaf(leaf, key, value);
}
```

**Optimization**: Eliminate duplicate search using `insert_hint()`
```cpp
// Before: find() + insert() → 2 searches
auto existing = leaf->data.find(key);
if (existing != leaf->data.end()) return ...;
auto [it, inserted] = leaf->data.insert(key, value);  // searches again

// After: lower_bound() + insert_hint() → 1 search
auto pos = leaf->data.lower_bound(key);
if (pos != leaf->data.end() && pos->first == key) return ...;
auto [it, inserted] = leaf->data.insert_hint(pos, key, value);  // uses pos
```

**Added to ordered_array**:
```cpp
std::pair<iterator, bool> insert_hint(iterator hint, const Key& key, const Value& value) {
  // Assumes hint from lower_bound(), skips re-searching
  assert(idx == size_ || keys_[idx] >= key);
  assert(idx == 0 || keys_[idx - 1] < key);
  // Direct insertion at hinted position
}
```

#### Phase 11: emplace() - In-Place Construction
**Implementation**: Constructs pair, delegates to insert()
```cpp
template <typename... Args>
std::pair<iterator, bool> emplace(Args&&... args) {
  value_type pair(std::forward<Args>(args)...);
  return insert(pair.first, pair.second);
}
```

**Design Decision**: Keep simple approach vs full emplace
- **Issue**: Need key to search before constructing value in-place
- **Options**:
  1. Construct temp pair → move (current, simple)
  2. Key + value_args variant (complex, marginal benefit)
- **Choice**: (1) - Less complexity, key construction unavoidable anyway

#### Phase 12: swap() - O(1) Container Exchange
**Challenge**: Union member (leaf_root_ vs internal_root_) requires careful swapping
```cpp
void swap(btree& other) noexcept {
  if (root_is_leaf_ && other.root_is_leaf_) {
    std::swap(leaf_root_, other.leaf_root_);
  } else if (!root_is_leaf_ && !other.root_is_leaf_) {
    std::swap(internal_root_, other.internal_root_);
  } else {
    // Mixed: manual swap with temp variable
    if (root_is_leaf_) {
      LeafNode* temp = leaf_root_;
      internal_root_ = other.internal_root_;
      other.leaf_root_ = temp;
    } else { /* reverse */ }
  }
  std::swap(root_is_leaf_, other.root_is_leaf_);
  std::swap(size_, other.size_);
  std::swap(leftmost_leaf_, other.leftmost_leaf_);
  std::swap(rightmost_leaf_, other.rightmost_leaf_);
}
```

#### Phase 13: equal_range() - Range Queries
**Simple delegation pattern**:
```cpp
std::pair<iterator, iterator> equal_range(const Key& key) {
  return {lower_bound(key), upper_bound(key)};
}
```
- For unique keys: range contains ≤1 element
- Leverages existing lower_bound/upper_bound implementations

#### Phase 14: key_comp() and value_comp() - Comparison Objects
**Type definitions**:
```cpp
using key_compare = std::less<Key>;

class value_compare {
 public:
  bool operator()(const value_type& lhs, const value_type& rhs) const {
    return key_compare()(lhs.first, rhs.first);
  }
};
```
- Stateless functors → cheap to copy
- value_compare only compares keys (ignores values)
- Compatible with std::sort and other algorithms

#### Phase 15: get_allocator() - Allocator Access
**API compatibility layer**:
```cpp
using allocator_type = std::allocator<value_type>;
allocator_type get_allocator() const { return allocator_type(); }
```
- Current impl uses raw new/delete for nodes
- Provides std::map API compatibility
- Future: integrate allocator throughout implementation

### Merge Conflict Resolution Pattern (Phases 10-15)

**Test file conflicts**: Multiple phases added tests after same location
```bash
# Extract sections from conflicting regions
sed -n 'START,END p' file > phase_N_tests.txt

# Combine in phase order (14 then 15, not 15 then 14)
cat phase_14_tests.txt phase_15_tests.txt > combined.txt

# Reconstruct file: before + combined + after
cat before_conflict.txt combined.txt after_conflict.txt > file
```

**Header file conflicts**: Type aliases and methods
- **Pattern**: Phase N adds type alias + method, Phase N+1 adds different alias + method
- **Resolution**: Combine ALL aliases, then ALL methods in phase order
- **Order matters**: Dependent types (value_compare uses key_compare) must be ordered correctly

**Verification**:
```bash
cmake --build cmake-build-debug --target test_btree
ctest --test-dir cmake-build-debug --output-on-failure
# Check specific phase: ./test_btree "[tag]"
```

## Benchmark Best Practices

### Template Consolidation (43% code reduction)
```cpp
template <std::size_t Size, SearchMode SM, MoveMode MM = MoveMode::SIMD>
static void BM_OrderedArray_Insert(benchmark::State& state) { /*...*/ }

BENCHMARK(BM_OrderedArray_Insert<8, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Insert<16, SearchMode::SIMD, MoveMode::Standard>);
```

### PauseTiming Overhead (~300ns)
- ❌ Avoid for operations <100ns (overhead dominates)
- ✅ Pre-populate outside `for (auto _ : state)` loop
- ✅ Measure realistic cycles (RemoveInsert, not just Insert)
- ✅ Always `benchmark::DoNotOptimize(result)`
- Verify: Results should scale with size (constant timing = measuring overhead)

### perf Analysis
```bash
perf stat -e cycles,instructions,cache-references,cache-misses,branches \\
  ./ordered_array_search_benchmark --benchmark_filter="RemoveInsert.*Size:32"
# IPC = instructions / cycles
# Cache miss rate = cache-misses / cache-references * 100
```

## Command-Line Parsing with Lyra

**Integration**: Lyra is available as a header-only library via `target_link_libraries(your_target PRIVATE lyra)`

**Example usage**:
```cpp
#include <lyra/lyra.hpp>

int main(int argc, char** argv) {
  bool show_help = false;
  int size = 64;
  std::string mode = "binary";

  auto cli = lyra::cli()
    | lyra::help(show_help)
    | lyra::opt(size, "size")["-s"]["--size"]("Array size")
    | lyra::opt(mode, "mode")["-m"]["--mode"]("Search mode: binary|linear|simd");

  auto result = cli.parse({argc, argv});
  if (!result || show_help) {
    std::cout << cli << std::endl;
    return show_help ? 0 : 1;
  }

  // Use parsed parameters...
}
```

**Use cases**: Custom benchmark harnesses, performance testing tools, data structure profiling

**Reference**: See `src/binary/lyra_example.cpp` for a complete example

## GitHub Workflow

### Updating PR Descriptions
**Problem**: `gh pr edit <num> --body` fails with GraphQL deprecation error (Projects classic)

**Solution**: Use REST API directly
```bash
# Write description to file first
cat > /tmp/pr_body.md <<'EOF'
Your PR description here...
EOF

# Update PR using REST API
gh api \
  --method PATCH \
  -H "Accept: application/vnd.github+json" \
  repos/kressler/fast-containers/pulls/<PR_NUM> \
  -F body="$(cat /tmp/pr_body.md)"
```

**Key points**:
- `-F body=` handles multi-line text correctly
- `$(cat file)` preserves formatting
- Works when `gh pr edit` fails with GraphQL errors

## Template Interface/Implementation Separation

**Pattern**: `.hpp` files contain interface declarations, `.ipp` files contain implementations.

### Structure
```cpp
// ordered_array.hpp
#pragma once
#include <...>

namespace fast_containers {
  template <...>
  class ordered_array {
   public:
    void method();  // Declaration only
   private:
    T* data_;
  };

  #include "ordered_array.ipp"  // Include at end, before closing namespace
}
```

```cpp
// ordered_array.ipp - NO header guards
namespace fast_containers {
  template <Comparable Key, typename Value, std::size_t Length,
            SearchMode SearchModeT, MoveMode MoveModeT>
  void ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::method() {
    // Full implementation with complete template syntax
  }
}
```

### What stays in .hpp vs moves to .ipp

**Keep in .hpp:**
- Header guards, includes, namespace
- Enums, concepts, type aliases
- Class/struct declarations
- Method declarations with documentation
- Trivial one-liners (e.g., `size()`, `empty()`, `begin()`, `end()`)
- Small nested classes (<20 lines)

**Move to .ipp:**
- All non-trivial method implementations
- Private helper implementations
- Template method specializations
- Complex constructors/destructors

### Benefits
- 🔍 **Readability**: Interface visible at a glance (49-68% smaller headers)
- ⚡ **Compile time**: Implementation changes don't require full header recompilation
- 📖 **Documentation**: Clear separation of what vs how
- 🛠️ **Maintainability**: Easier navigation and code review

### Example Refactoring Results

| Component | Old .hpp | New .hpp | New .ipp | Reduction |
|-----------|----------|----------|----------|-----------|
| ordered_array | 1,129 lines | 554 lines | 958 lines | 51% |
| btree | 2,113 lines | 667 lines | 1,532 lines | 68% |

## Common Pitfalls

| Issue | Wrong | Correct |
|-------|-------|---------|
| Iterator refs | `for (auto& p : arr)` | `for (auto p : arr)` |
| Key modification | `it->first = val` | `arr.erase(old); arr.insert(new, val)` |
| Submodule update | - | `git submodule update --remote` |

## Future Optimizations
- [ ] AVX2 vectorized binary search
- [ ] Prefetch hints
- [ ] AVX-512 support
- [x] 64-byte cache line alignment (PR #22)
- [ ] Custom comparators
- [ ] Move semantics for insert

## Setup
```bash
git clone --recursive <repo> && cd <repo> && ./install-hooks.sh
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release && cmake --build cmake-build-release --parallel
ctest --test-dir cmake-build-release --output-on-failure
```

## Contributing
1. `./install-hooks.sh` (auto-format on commit)
2. Write Catch2 tests + Google Benchmark for perf-critical code
3. Follow Google C++ Style
4. Update this file with learnings

## Limitations
- Fixed compile-time size, no reallocation
- Iterator proxy pattern (use `auto`, not `auto&`)
- Move-only values untested
