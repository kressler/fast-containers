# Project: Fast Containers

## Project Description
Fast container library for C++ on x86-64, focusing on SIMD-optimized data structures for high-performance applications.

## Tech Stack
- Language: C++23
- Testing: Catch2 v3.11.0
- Benchmarking: Google Benchmark v1.9.4
- Build System: CMake 3.30+
- Code Formatting: clang-format (Google C++ Style)

## Code Conventions
- Follow Google C++ Style Guide (enforced by clang-format)
- Use C++23 features and concepts
- 80-character line limit
- 2-space indentation
- Left-aligned pointers (`int* ptr`)
- Header-only template implementations where appropriate

## Project Structure
```
.
├── src/
│   ├── ordered_array.hpp       # Main container implementations
│   ├── tests/                  # Catch2 test files
│   └── benchmarks/             # Google Benchmark files
├── third_party/
│   ├── catch2/                 # Testing framework (submodule)
│   └── benchmark/              # Benchmarking framework (submodule)
├── hooks/                      # Git hooks (pre-commit formatting)
├── CMakeLists.txt              # Root build configuration
└── install-hooks.sh            # Hook installation script
```

## Development Workflow

### Setting Up
```bash
# Clone with submodules
git clone --recursive <repo-url>

# Or initialize submodules after cloning
git submodule update --init --recursive

# Install pre-commit hook for auto-formatting
./install-hooks.sh
```

### Building

**Standard Debug Build:**
```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug --parallel
```

**Debug Build with AVX2 (for debugging SIMD code):**
```bash
# Enable AVX2 in Debug builds for testing/debugging SIMD optimizations
cmake -S . -B cmake-build-debug -DENABLE_AVX2=ON
cmake --build cmake-build-debug --parallel
```

**Optimized Release Build:**
```bash
# Release build with AVX2 enabled by default (Intel Haswell 2013+, AMD Excavator 2015+)
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --parallel
```

**Release Build Without AVX2 (for older CPUs):**
```bash
# Disable AVX2 if targeting pre-2013 Intel or pre-2015 AMD CPUs
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release -DENABLE_AVX2=OFF
cmake --build cmake-build-release --parallel
```

**Build Options:**
- `CMAKE_BUILD_TYPE=Release`: Enables aggressive optimizations (-O3, -ffast-math, -funroll-loops)
- `ENABLE_AVX2`:
  - **ON by default for Release builds** - adds AVX2 SIMD instructions (-mavx2, -mfma, -march=haswell)
  - **OFF by default for Debug builds** - can be enabled with `-DENABLE_AVX2=ON` for debugging SIMD code
- `--parallel`: Use all available CPU cores for faster builds
- Override defaults with `-DENABLE_AVX2=ON` (Debug) or `-DENABLE_AVX2=OFF` (Release)

**Note:** Using separate build directories (`cmake-build-debug` and `cmake-build-release`) allows you to maintain both debug and release builds simultaneously.

### Testing
```bash
# Debug build
ctest --test-dir cmake-build-debug --output-on-failure

# Release build
ctest --test-dir cmake-build-release --output-on-failure
```

### Benchmarking
```bash
# Build benchmarks in Release mode for accurate results (AVX2 enabled by default)
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --parallel

# Run specific benchmark
./cmake-build-release/src/sample_benchmark
./cmake-build-release/src/ordered_array_search_benchmark

# Or build specific benchmark target
cmake --build cmake-build-release --parallel --target ordered_array_search_benchmark
```

**Note:** Always benchmark in Release mode with optimizations enabled for accurate performance measurements. AVX2 optimizations are enabled by default.

### Code Formatting
```bash
# Format all C++ files (works from any build directory)
cmake --build cmake-build-debug --target format

# Or let pre-commit hook handle it automatically
git commit -m "Your message"  # Auto-formats staged C++ files
```

## Architecture and Design Decisions

### ordered_array Container

#### Design Overview
`ordered_array<Key, Value, Length>` is a fixed-size container that maintains key-value pairs in sorted order by key. It uses separate arrays for keys and values to enable SIMD optimizations.

#### Key Design Decisions

**1. Separate Key-Value Arrays**
- **Storage**: `std::array<Key, Length>` and `std::array<Value, Length>`
- **Rationale**:
  - Enables AVX2 vectorized key searches
  - Better cache locality for key-only operations
  - Allows independent prefetching of keys and values
  - Prepares for SIMD bulk operations
- **Trade-off**: Requires custom iterator with proxy pattern

**2. Iterator Proxy Pattern**
- **Problem**: Can't return `std::pair<Key, Value>&` from separate arrays
- **Solution**: `pair_proxy<IsConst>` class with references to array elements
- **Implementation**:
  ```cpp
  template <bool IsConst>
  class pair_proxy {
    const Key& first;        // Always const to protect sorted order
    Value& second;           // Mutable for non-const iterators
  };
  ```
- **Limitation**: Range-based for loops must use `auto` not `auto&`
  - Same limitation as `std::vector<bool>`
  - Modifications still work: `proxy.second = value` updates array

**3. Const Key References**
- **Design**: Keys are always `const` even in non-const iterators
- **Rationale**: Modifying keys would break sorted order invariant
- **Enforcement**: `using key_ref_type = const Key&;`

#### Performance Characteristics
- **Find**: O(log n) - binary search on key array (SIMD-ready)
- **Insert**: O(n) - binary search + array shifts
- **Remove**: O(n) - binary search + array shifts
- **Access**: O(log n) via find, O(1) with iterator

#### Future SIMD Optimizations
The separate-array design enables:

1. **Vectorized Binary Search**
   - Use AVX2 to compare 4-8 keys in parallel
   - `_mm256_cmpgt_epi32` for integer key comparisons
   - Potential 4-8x throughput improvement

2. **Bulk Operations**
   - `_mm256_loadu/storeu_si256` for moving keys/values
   - Vectorized shifts during insert/remove
   - Parallel key comparisons

3. **Cache Optimization**
   - Sequential key scans are cache-friendly
   - Prefetch keys independently from values
   - Aligned array access for SIMD loads

### Memory Layout

**Before Refactoring**:
```
[K0,V0][K1,V1][K2,V2]...  (pairs interleaved)
```

**After Refactoring**:
```
Keys:   [K0][K1][K2][K3]...
Values: [V0][V1][V2][V3]...
```

**Benefits**:
- Key searches don't load unused value data
- AVX2 can load contiguous keys efficiently
- Better CPU cache utilization

## Key Implementation Details

### Comparable Concept
```cpp
template<typename T>
concept Comparable = requires(T a, T b) {
  { a < b } -> std::convertible_to<bool>;
  { a == b } -> std::convertible_to<bool>;
};
```
Enforces that Key type supports comparison operations at compile time.

### Binary Search Strategy
Uses `std::lower_bound` on key array for O(log n) searches:
- Returns iterator to first key not less than search key
- Check if key matches to determine found/not found
- Can be replaced with AVX2 implementation in future

### Iterator Arrow Operator
Uses `arrow_proxy` helper for proper pointer semantics:
```cpp
struct arrow_proxy {
  pair_proxy<IsConst> ref;
  pair_proxy<IsConst>* operator->() { return &ref; }
};
```
This allows `it->first` and `it->second` syntax.

## Testing Strategy

### Unit Tests (Catch2)
- Comprehensive coverage of all operations
- Edge cases: empty, full, duplicates
- Iterator correctness (forward, reverse, const)
- Exception safety (full array, duplicate keys)
- Type parameterization (int, string, double)

### Benchmarks (Google Benchmark)
- Performance regression detection
- Comparison of different implementations
- SIMD vs non-SIMD measurements
- Different data sizes and patterns

## Benchmarking Best Practices

### Template-Based Benchmark Consolidation

**Problem**: Duplicated benchmark code for different configurations (SearchMode, MoveMode, sizes)

**Solution**: Use templated benchmark functions with compile-time parameters

**Example Pattern**:
```cpp
// Instead of separate functions for each mode:
// BM_OrderedArray_Insert_Binary, BM_OrderedArray_Insert_Linear, etc.

// Use a single template:
template <std::size_t Size, SearchMode search_mode,
          MoveMode move_mode = MoveMode::SIMD>
static void BM_OrderedArray_Insert(benchmark::State& state) {
  auto keys = GenerateUniqueKeys<Size>();

  for (auto _ : state) {
    ordered_array<int, int, Size, search_mode, move_mode> arr;
    for (std::size_t i = 0; i < Size; ++i) {
      arr.insert(keys[i], i);
    }
    benchmark::DoNotOptimize(arr);
  }
  state.SetItemsProcessed(state.iterations() * Size);
}

// Register specific instantiations:
BENCHMARK(BM_OrderedArray_Insert<8, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Insert<8, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_Insert<8, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_Insert<16, SearchMode::Binary, MoveMode::Standard>);
```

**Benefits**:
- Reduces code duplication significantly (43% reduction in our case)
- Easier to add new configurations (just add BENCHMARK() registration)
- Ensures consistent implementation across all variants
- Type-safe compile-time configuration
- Less maintenance burden

**Key Learnings**:
1. **Default template parameters**: Use defaults for optional configurations (e.g., `MoveMode = MoveMode::SIMD`)
2. **Template registrations**: Each `BENCHMARK()` call instantiates the template with specific parameters
3. **Compile-time validation**: Template errors caught at compile time, not runtime
4. **Code organization**: Keep template definitions before registration macros
5. **Naming**: Benchmark names auto-generated from template instantiation

**When to consolidate**:
- ✅ Multiple benchmarks with only parameter differences
- ✅ Same measurement logic across variants
- ✅ Configurations representable as compile-time constants
- ❌ Fundamentally different measurement approaches
- ❌ Runtime-only configuration differences

### Benchmark Timing Overhead: PauseTiming/ResumeTiming

**Problem**: Using `state.PauseTiming()` and `state.ResumeTiming()` to exclude setup code can introduce significant overhead that dominates the actual operation being measured.

**Discovery**: When measuring insert operations with PauseTiming:
```cpp
for (auto _ : state) {
  state.PauseTiming();
  ordered_array<int, int, Size, search_mode> arr;
  // Pre-populate array...
  state.ResumeTiming();

  arr.insert(key, value);  // Measure only this
}
```

**Results showed constant timing (~313-343ns) regardless of array size**:
- Size 1: 313ns
- Size 8: 343ns
- Size 64: 343ns (should be slower!)

**Root cause**: PauseTiming/ResumeTiming add ~300ns overhead per iteration, completely masking the actual operation time (1-23ns for these small arrays).

**Solution**: Pre-populate once outside the timing loop
```cpp
// Pre-populate once (outside timing)
ordered_array<int, int, Size, search_mode, move_mode> arr;
for (std::size_t i = 0; i < Size - 1; ++i) {
  arr.insert(keys[i], i);
}

for (auto _ : state) {
  // Measure remove+insert cycle (no PauseTiming overhead)
  if (arr.size() == Size) {
    arr.remove(keys[Size - 1]);
  }
  arr.insert(keys[Size - 1], Size - 1);
  benchmark::DoNotOptimize(arr);
}
```

**Corrected Results** (Size 8, RemoveInsert):
- Binary: 10.5ns
- Linear: 9.17ns
- SIMD: 18.2ns

**Key Learnings**:
1. **PauseTiming overhead is significant**: ~300ns on modern CPUs, dominates operations <100ns
2. **Pre-populate outside loop**: Setup arrays once before `for (auto _ : state)` when possible
3. **Measure realistic cycles**: For insert/remove, measure the full operation including the array state that triggers data movement
4. **Verify results scale**: If timing is constant across sizes, you're measuring overhead, not the operation
5. **DoNotOptimize**: Always use `benchmark::DoNotOptimize()` to prevent compiler from optimizing away work

**When to use PauseTiming**:
- ✅ Operations >1μs where 300ns overhead is negligible (<30%)
- ✅ When pre-population is impossible (e.g., testing construction itself)
- ❌ Micro-benchmarks <100ns (overhead dominates)
- ❌ When pre-population outside loop is viable

### SIMD Data Movement

**Implementation**: AVX2-accelerated array shifting for insert/remove operations

**Progressive Fallback Strategy** (32→16→1 bytes):
```cpp
// Move data in progressively smaller chunks
while (num_bytes >= 32) {
  // AVX2: 256-bit (32 bytes)
  __m256i data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst), data);
  // ... advance pointers
}

if (num_bytes >= 16) {
  // SSE: 128-bit (16 bytes)
  __m128i data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
  _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), data);
  // ... advance pointers
}

// Remaining 0-15 bytes: simple byte-by-byte loop
while (num_bytes > 0) {
  *dst = *src;
  // ... advance pointers
}
```

**Key Learnings**:
1. **Byte-based arithmetic**: Cast to `char*` and work with byte counts for simplicity
2. **Trivially copyable check**: Only use SIMD for `std::is_trivially_copyable_v<T>`
3. **Manual chunking unnecessary**: After SIMD blocks, simple byte loop performs better than `std::move` for small remainders (0-15 bytes)
4. **Unaligned loads**: Use `_mm256_loadu_si256` (unaligned) not `_mm256_load_si256` (aligned)
5. **Direction matters**: Use `move_backward` for overlapping ranges when shifting right

**Performance Results** (Insert operations, 32-bit keys/values):
- Size 8: SIMD 14% faster than Standard
- Size 16: SIMD 20% faster than Standard
- Size 64: SIMD 8% faster than Standard

**Trade-offs**:
- Best gains on small-medium sizes (8-64 elements)
- Diminishes for very large sizes due to memory bandwidth limits
- Adds code complexity and platform dependencies
- Worth it for hot paths in performance-critical code

### SIMD Performance Trade-offs: When SIMD Wins vs Loses

**Critical Discovery**: SIMD doesn't always win, even when faster at individual operations. The workload characteristics matter more than the raw operation speed.

#### SIMD Wins: Read-Heavy Operations (Find)

**Find Performance** (SearchMode comparison):
- Size 8: SIMD **1.57ns** vs Linear 1.68ns → **7% faster**
- Size 16: SIMD **1.68ns** vs Linear 2.21ns → **24% faster**
- Size 32: SIMD **2.05ns** vs Linear 3.71ns → **45% faster**
- Size 64: SIMD **2.43ns** vs Linear 5.92ns → **59% faster**

**Why SIMD wins**:
1. **No early termination benefit**: Must scan entire array to confirm absence
2. **Parallel comparisons**: Check 8 keys simultaneously vs 1 at a time
3. **No data movement**: Read-only operations don't require shifting data
4. **Predictable access pattern**: Sequential loads are cache-friendly

#### SIMD Loses: Write-Heavy Operations on Small Arrays (RemoveInsert)

**RemoveInsert Performance** (Size 8-32):
- Size 8: Binary 10.5ns, **Linear 9.17ns**, SIMD 18.2ns (Linear wins by 99%)
- Size 16: Binary 16.6ns, **Linear 13.3ns**, SIMD 20.8ns (Linear wins by 56%)
- Size 32: **Binary 18.9ns**, Linear 14.7ns, SIMD 22.8ns (Binary wins by 20%)
- Size 64: **Binary 17.6ns**, SIMD 23.3ns, Linear 24.2ns (Binary wins by 32%)

**Why SIMD loses on small arrays**: CPU Cycle Efficiency, Not Cache

**Initial hypothesis (WRONG)**: Cache misses cause SIMD slowdown

**perf stat analysis revealed the real bottleneck** (Size 32, 1 billion iterations):
```
Linear:  7.47B cycles, 43.1B instructions, IPC 5.76, 26.0% cache miss rate
SIMD:    7.96B cycles, 29.3B instructions, IPC 3.68, 28.5% cache miss rate
```

**Key metrics**:
- SIMD executes **32% fewer instructions** (29.3B vs 43.1B) ✅
- But SIMD takes **6.5% more cycles** (7.96B vs 7.47B) ❌
- Linear has **56% better IPC** (5.76 vs 3.68) - **This is the bottleneck!**
- Cache miss rates are similar (26% vs 28.5%) - Not the problem

**Root cause: Instruction-Level Parallelism vs SIMD Complexity**
1. **Linear search uses simple instructions**: High IPC, minimal pipeline stalls
2. **SIMD uses complex instructions**: Lower IPC, dependencies between vector operations
3. **Early termination**: Linear can exit after few comparisons on small arrays
4. **Branch prediction**: Works well on small, sorted arrays

**Cache line alignment optimization**:
- Added `alignas(64)` to arrays (64-byte cache line size)
- Changed to aligned SIMD loads: `_mm256_load_si256` vs `_mm256_loadu_si256`
- **Result**: SIMD cache references improved by 4.3% (1,334,705 → 1,277,841)
- **Impact**: Modest improvement, but IPC bottleneck remains dominant

#### Key Learnings: When to Use Each SearchMode

**Use SIMD SearchMode when**:
- ✅ Read-heavy workloads (find operations dominate)
- ✅ Large arrays (>32 elements) where parallelism pays off
- ✅ Negative lookups (must scan entire array)
- ✅ Uniform access patterns

**Use Linear SearchMode when**:
- ✅ Write-heavy workloads (frequent insert/remove)
- ✅ Small arrays (<32 elements) where early exit helps
- ✅ Positive lookups on small arrays (find quickly)
- ✅ Minimizing code complexity

**Use Binary SearchMode when**:
- ✅ Large arrays (>64 elements) with any workload
- ✅ Logarithmic scaling is important
- ✅ Balanced read/write operations
- ✅ When SIMD is unavailable or disabled

**Performance analysis tools used**:
```bash
# CPU cycle analysis with perf stat
perf stat -e cycles,instructions,cache-references,cache-misses,branches \\
  ./ordered_array_search_benchmark --benchmark_filter="RemoveInsert.*Size:32"

# Calculate IPC (Instructions Per Cycle)
IPC = instructions / cycles

# Cache miss rate
Cache miss rate = cache-misses / cache-references * 100
```

## Common Pitfalls and Solutions

### 1. Iterator References
❌ **Wrong**:
```cpp
for (auto& pair : arr) {  // Won't compile - proxy iterator
  pair.second = value;
}
```

✅ **Correct**:
```cpp
for (auto pair : arr) {   // Use auto, not auto&
  pair.second = value;    // Still modifies underlying array
}
```

### 2. Key Modification
❌ **Wrong**:
```cpp
auto it = arr.begin();
it->first = new_key;  // Won't compile - first is const
```

✅ **Correct**:
```cpp
arr.remove(old_key);
arr.insert(new_key, value);  // Proper way to change keys
```

### 3. Submodule Updates
```bash
# Update all submodules to latest
git submodule update --remote

# Update specific submodule
git submodule update --remote third_party/benchmark
```

## Performance Notes

### Build Configuration for Maximum Performance

**Standard Release Build (AVX2 enabled by default):**
```bash
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --parallel
```

This enables by default:
- `-O3`: Aggressive optimizations
- `-mavx2`, `-mfma`: AVX2 SIMD instructions
- `-march=haswell`: Target Haswell (AVX2) architecture or later
- `-ffast-math`: Fast floating-point math
- `-funroll-loops`: Loop unrolling
- `-mtune=native`: Tune for current CPU

**For older CPUs without AVX2 (pre-2013 Intel, pre-2015 AMD):**
```bash
cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release -DENABLE_AVX2=OFF
cmake --build cmake-build-release --parallel
```

This uses `-march=native` to optimize for your specific CPU without requiring AVX2.

### Check AVX2 Support
AVX2 is enabled by default for Release builds. To verify your CPU supports it:
```bash
# Linux
grep -o 'avx2' /proc/cpuinfo | head -1

# macOS
sysctl -a | grep machdep.cpu.features | grep AVX2

# Or in C++ at runtime
#include <immintrin.h>
// AVX2 intrinsics available: _mm256_* functions
```

**Compatible CPUs (AVX2 supported):**
- Intel: Haswell (2013) or later
- AMD: Excavator (2015) or later

If your CPU doesn't support AVX2, disable it with `-DENABLE_AVX2=OFF`.

### CPU Scaling
Disable CPU frequency scaling for consistent benchmarks:
```bash
# Linux
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

## Future Optimization Opportunities

### Immediate (Based on Current Design)
1. **AVX2 Binary Search**: Replace `std::lower_bound` with vectorized search
2. **Bulk Insert**: Vectorize array shifting operations
3. **Parallel Comparisons**: Use SIMD for multiple key comparisons

### Medium-Term
1. ~~**Alignment**: Ensure 32-byte alignment for AVX2 loads~~ ✅ **Done**: Implemented 64-byte cache line alignment (PR #22)
2. **Prefetching**: Add software prefetch hints
3. **Branch Prediction**: Optimize hot paths

### Long-Term
1. **AVX-512**: Support for wider SIMD operations
2. **GPU Offload**: Consider GPU-accelerated searches for very large arrays
3. **Hybrid Structures**: Combine with other data structures for better scaling

## Resources

### Documentation
- [Google Benchmark Guide](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
- [Catch2 Tutorial](https://github.com/catchorg/Catch2/blob/devel/docs/tutorial.md)
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/)

### AVX2 References
- Intel Intrinsics Guide: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/
- Agner Fog's Optimization Manuals: https://www.agner.org/optimize/

### C++23 Features Used
- Concepts (Comparable)
- std::conditional_t
- Deduction guides
- Three-way comparison (future)

## Contributing Guidelines

1. Install pre-commit hook: `./install-hooks.sh`
2. Write tests for new features
3. Add benchmarks for performance-critical code
4. Follow Google C++ Style Guide
5. Document public APIs with Doxygen comments
6. Update this file with new learnings!

## Known Limitations

1. **Fixed Size**: Container size is compile-time constant
2. **No Reallocation**: Cannot grow beyond initial capacity
3. **Iterator Proxies**: Range-based for requires `auto` not `auto&`
4. **Move-Only Values**: Not yet tested (may need additional work)

## Recent Improvements

- ✅ **SIMD Search Implementation** (PR #16): Added AVX2-accelerated linear search with progressive fallback
- ✅ **AVX2 Build Configuration** (PR #14): Configurable AVX2 support with Release/Debug defaults
- ✅ **SIMD Data Movement** (PR #18): AVX2-accelerated array shifts for insert/remove operations
- ✅ **Benchmark Consolidation** (PR #20): Reduced benchmark code duplication by 43% using templates
- ✅ **RemoveInsert Benchmark & Cache Line Alignment** (PR #22):
  - Fixed benchmark methodology to avoid PauseTiming overhead masking true performance
  - Discovered SIMD performance trade-offs: wins on reads (7-59% faster), loses on small array modifications due to IPC bottleneck
  - Implemented 64-byte cache line alignment for 4.3% cache efficiency improvement
  - Documented when to use SIMD vs Linear vs Binary search modes

## Open Questions / TODO

- [ ] Should we support move semantics for insert operations?
- [x] Add alignment attributes for AVX2 compatibility? ✅ Done (PR #22: 64-byte cache line alignment)
- [ ] Implement custom allocator support?
- [ ] Add statistics tracking (inserts, removals, searches)?
- [x] Create dedicated benchmark suite for ordered_array? ✅ Done
- [ ] Implement AVX2 vectorized binary search?
- [ ] Add support for custom comparators?

---

*This document should be updated as the project evolves and new patterns are discovered.*
