# Fast Containers - C++ SIMD Containers

## Stack
- C++23, CMake 3.30+, Catch2 v3.11.0, Google Benchmark v1.9.4
- Style: Google C++ (clang-format), 80 chars, 2 spaces, `int* ptr`

## Structure
```
src/{ordered_array.hpp, tests/, benchmarks/}
third_party/{catch2/, benchmark/}  # submodules
hooks/, install-hooks.sh
```

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
| **Linear** | Write-heavy | <32 | Frequent insert/remove, early exit benefits |
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

## Common Pitfalls

| Issue | Wrong | Correct |
|-------|-------|---------|
| Iterator refs | `for (auto& p : arr)` | `for (auto p : arr)` |
| Key modification | `it->first = val` | `arr.remove(old); arr.insert(new, val)` |
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
