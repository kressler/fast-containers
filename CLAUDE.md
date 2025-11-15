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
cmake -S . -B build
cmake --build build
```

**Optimized Release Build:**
```bash
# Standard Release build (optimized for native CPU)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Release Build with AVX2 Optimizations:**
```bash
# For CPUs with AVX2 support (Intel Haswell or later, AMD Excavator or later)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_AVX2=ON
cmake --build build
```

**Build Options:**
- `CMAKE_BUILD_TYPE=Release`: Enables aggressive optimizations (-O3, -ffast-math, -funroll-loops, -march=native)
- `ENABLE_AVX2=ON`: Adds AVX2 SIMD instructions (-mavx2, -mfma, -march=haswell)
- Use AVX2 build for maximum performance on compatible CPUs
- Check CPU support: `grep -o 'avx2' /proc/cpuinfo | head -1` (Linux)

### Testing
```bash
ctest --test-dir build --output-on-failure
```

### Benchmarking
```bash
# Build benchmarks in Release mode for accurate results
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_AVX2=ON
cmake --build build

# Run specific benchmark
./build/src/sample_benchmark
./build/src/ordered_array_search_benchmark

# Or build specific benchmark target
cmake --build build --target ordered_array_search_benchmark
```

**Note:** Always benchmark in Release mode with optimizations enabled for accurate performance measurements.

### Code Formatting
```bash
# Format all C++ files
cmake --build build --target format

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

**For AVX2-capable CPUs (recommended):**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_AVX2=ON
cmake --build build
```

This enables:
- `-O3`: Aggressive optimizations
- `-mavx2`, `-mfma`: AVX2 SIMD instructions
- `-march=haswell`: Target Haswell (AVX2) architecture or later
- `-ffast-math`: Fast floating-point math
- `-funroll-loops`: Loop unrolling
- `-mtune=native`: Tune for current CPU

**For general-purpose builds:**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This uses `-march=native` to optimize for your specific CPU without forcing AVX2.

### Check AVX2 Support
Before enabling AVX2, verify your CPU supports it:
```bash
# Linux
grep -o 'avx2' /proc/cpuinfo | head -1

# macOS
sysctl -a | grep machdep.cpu.features | grep AVX2

# Or in C++ at runtime
#include <immintrin.h>
// AVX2 intrinsics available: _mm256_* functions
```

**Compatible CPUs:**
- Intel: Haswell (2013) or later
- AMD: Excavator (2015) or later

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
1. **Alignment**: Ensure 32-byte alignment for AVX2 loads
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

## Open Questions / TODO

- [ ] Should we support move semantics for insert operations?
- [ ] Add alignment attributes for AVX2 compatibility?
- [ ] Implement custom allocator support?
- [ ] Add statistics tracking (inserts, removals, searches)?
- [ ] Create dedicated benchmark suite for ordered_array?

---

*This document should be updated as the project evolves and new patterns are discovered.*
