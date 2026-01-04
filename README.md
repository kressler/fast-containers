# Fast Containers

![CI](https://github.com/kressler/fast-containers/workflows/CI/badge.svg)

High-performance header-only container library for C++23 on x86-64.

## What's Included

* **B+Tree** (`fast_containers::btree`) - Cache-friendly B+tree with SIMD search and hugepage support
* **Dense Map** (`fast_containers::ordered_array`) - Fixed-size sorted array used internally by btree nodes
* **Hugepage Allocator** - Pooling allocator that reduces TLB misses and allocation overhead

## Why This Library?

The B+tree implementation provides significant performance improvements over industry standards for large trees. For some workloads with large trees, we've observed:

- **vs Abseil B+tree:** 2-5× faster across insert/find/erase operations
- **vs std::map:** 2-5× faster across insert/find/erase operations

See [benchmark results](docs/btree_benchmark_results.md) for detailed performance analysis.

**Important qualifications:**
- Performance advantages are most significant for large tree sizes where TLB misses and allocation costs dominate
- Benchmarks currently focus on 10M element trees; smaller tree sizes have not been comprehensively tested
- Results are specific to the tested configurations (8-byte keys, 32-byte and 256-byte values)

**Key advantages over Abseil's btree:**
- **Hugepage allocator integration:** 3-5× speedup from reduced TLB misses and pooled allocations
- **SIMD-accelerated search:** 3-10% faster node searches using AVX2 instructions
- **Tunable node sizes:** Optimize cache behavior for your specific key/value sizes

### Notes

**Work in progress** This is a work in progress. I don't have plans for major changes to the B+tree currently, but am actively cleaning up the implementation.

**Platforms** This library is really only built and tested on Linux, on x86-64 CPUs with AVX2 support. In theory, it could be built for Windows, though that hasn't been tested. The SIMD implementations are x86-64 specific. Timing in the custom benchmarks is also x86-64 specific (via use of `rdtscp`).

**History/Motivations** This project started as an exploration of using AI agents for software development. Based on experience tuning systems using Abseil's B+tree, I was curious if performance could be improved through SIMD instructions, a customized allocator, and tunable node sizes. Claude proved surprisingly adept at helping implement this quickly, and the resulting B+tree showed compelling performance improvements, so I'm making it available here.

---

## Quick Start

### Installation

**Prerequisites:**
- C++23 compiler (GCC 14+, Clang 19+)
- CMake 3.30+
- AVX2-capable CPU (Intel Haswell 2013+, AMD Excavator 2015+)

**Include in your project:**

1. Add as a git submodule:
   ```bash
   git submodule add https://github.com/kressler/fast-containers.git third_party/fast-containers
   ```

2. Link in CMakeLists.txt:
   ```cmake
   add_subdirectory(third_party/fast-containers)
   target_link_libraries(your_target PRIVATE fast_containers::fast_containers)
   ```

3. Include headers:
   ```cpp
   #include <fast_containers/btree.hpp>
   #include <fast_containers/hugepage_allocator.hpp>
   ```

---

## Usage

### Basic B+tree Example

```cpp
#include <fast_containers/btree.hpp>
#include <cstdint>
#include <iostream>

int main() {
  // Create a btree mapping int64_t keys to int32_t values
  // Using defaults: auto-computed node sizes, Linear search
  using Tree = kressler::fast_containers::btree<int64_t, int32_t>;

  Tree tree;

  // Insert key-value pairs
  tree.insert(42, 100);
  tree.insert(17, 200);
  tree.insert(99, 300);

  // Find a value
  auto it = tree.find(42);
  if (it != tree.end()) {
    std::cout << "Found: " << it->second << std::endl;  // Prints: 100
  }

  // Iterate over all elements (sorted by key)
  for (const auto& [key, value] : tree) {
    std::cout << key << " -> " << value << std::endl;
  }

  // Erase an element
  tree.erase(17);

  // Check size
  std::cout << "Size: " << tree.size() << std::endl;  // Prints: 2
}
```

### B+tree with Hugepage Allocator (Recommended for Performance)

```cpp
#include <fast_containers/btree.hpp>
#include <fast_containers/hugepage_allocator.hpp>
#include <cstdint>
#include <cassert>

int main() {
  // Use the hugepage allocator for 3-5× performance improvement
  // Allocator type must match the btree's value_type (std::pair<Key, Value>)
  using Allocator = kressler::fast_containers::HugePageAllocator<
      std::pair<int64_t, int32_t>>;

  using Tree = kressler::fast_containers::btree<
    int64_t,                                 // Key type
    int32_t,                                 // Value type
    96,                                      // Leaf node size
    128,                                     // Internal node size
    std::less<int64_t>,                      // Comparator
    kressler::fast_containers::SearchMode::SIMD,  // SIMD search
    Allocator                                // Hugepage allocator
  >;

  // Tree will default-construct the allocator (256MB initial pool, 64MB growth)
  // The btree automatically creates separate pools for leaf and internal nodes
  Tree tree;

  // Insert 10 million elements - hugepages reduce TLB misses
  for (int64_t i = 0; i < 10'000'000; ++i) {
    tree.insert(i, i * 2);
  }

  // Find operations are much faster with hugepages
  auto it = tree.find(5'000'000);
  assert(it != tree.end() && it->second == 10'000'000);
}
```

### Advanced: Policy-Based Allocator for Shared Pools

For multiple trees or fine-grained control over pool sizes, use `PolicyBasedHugePageAllocator`:

```cpp
#include <fast_containers/btree.hpp>
#include <fast_containers/policy_based_hugepage_allocator.hpp>
#include <cstdint>

int main() {
  // Create separate pools for leaf and internal nodes with custom sizes
  auto leaf_pool = std::make_shared<kressler::fast_containers::HugePagePool>(
      512 * 1024 * 1024, true);  // 512MB for leaves
  auto internal_pool = std::make_shared<kressler::fast_containers::HugePagePool>(
      256 * 1024 * 1024, true);  // 256MB for internals

  // Create policy that routes types to appropriate pools
  kressler::fast_containers::TwoPoolPolicy policy{leaf_pool, internal_pool};

  // Create allocator with the policy
  using Allocator = kressler::fast_containers::PolicyBasedHugePageAllocator<
      std::pair<int64_t, int32_t>,
      kressler::fast_containers::TwoPoolPolicy>;

  Allocator alloc(policy);

  using Tree = kressler::fast_containers::btree<
    int64_t, int32_t, 96, 128, std::less<int64_t>,
    kressler::fast_containers::SearchMode::SIMD, Allocator>;

  // Multiple trees can share the same pools
  Tree tree1(alloc);
  Tree tree2(alloc);

  // Both trees share leaf_pool for leaves and internal_pool for internals
  tree1.insert(1, 100);
  tree2.insert(2, 200);
}
```

**When to use each allocator:**
- **`HugePageAllocator`**: Simple, automatic separate pools per type (recommended for single tree)
- **`PolicyBasedHugePageAllocator`**: Fine-grained control, shared pools across trees, custom pool sizes

### API Overview

The `btree` class provides an API similar to `std::map`:

**Insertion:**
- `std::pair<iterator, bool> insert(const Key& key, const Value& value)`
- `std::pair<iterator, bool> emplace(Args&&... args)`
- `Value& operator[](const Key& key)`

**Lookup:**
- `iterator find(const Key& key)`
- `const_iterator find(const Key& key) const`
- `iterator lower_bound(const Key& key)`
- `iterator upper_bound(const Key& key)`
- `std::pair<iterator, iterator> equal_range(const Key& key)`

**Removal:**
- `size_type erase(const Key& key)`
- `iterator erase(iterator pos)`

**Iteration:**
- `iterator begin()` / `const_iterator begin() const`
- `iterator end()` / `const_iterator end() const`
- Range-based for loops supported

**Capacity:**
- `size_type size() const`
- `bool empty() const`
- `void clear()`

**Other:**
- `void swap(btree& other) noexcept`
- `key_compare key_comp() const`
- `value_compare value_comp() const`

### Template Parameters

```cpp
template <
  typename Key,
  typename Value,
  std::size_t LeafNodeSize = default_leaf_node_size<Key, Value>(),
  std::size_t InternalNodeSize = default_internal_node_size<Key>(),
  typename Compare = std::less<Key>,
  SearchMode SearchModeT = SearchMode::Linear,
  typename Allocator = std::allocator<std::pair<Key, Value>>
>
class btree;
```

**Parameters:**

- `Key`, `Value`: The key and value types

- `LeafNodeSize`: Number of key-value pairs per leaf node
  - **Default**: Auto-computed heuristic targeting ~2KB (32 cache lines)
  - Formula: `2048 / (sizeof(Key) + sizeof(Value))`, rounded to multiple of 8, clamped to [8, 64]
  - Manual tuning: Larger values (64-96) for small values, smaller values (8-16) for large values

- `InternalNodeSize`: Number of child pointers per internal node
  - **Default**: Auto-computed heuristic targeting ~1KB (16 cache lines)
  - Formula: `1024 / (sizeof(Key) + sizeof(void*))`, rounded to multiple of 8, clamped to [16, 64]
  - Generally leave at default (stores only 8-byte pointers)

- `Compare`: Comparison function (must satisfy `ComparatorCompatible<Key, Compare>`)
  - **Default**: `std::less<Key>`
  - Also supports `std::greater<Key>` for descending order

- `SearchMode`: How to search within a node
  - **Default**: `SearchMode::Linear` (scalar linear search)
  - `SearchMode::SIMD`: AVX2-accelerated search (3-10% faster, requires AVX2 CPU and SIMD-compatible keys: int32_t, uint32_t, int64_t, uint64_t, float, double)
  - `SearchMode::Binary`: Binary search

- `Allocator`: Memory allocation strategy
  - **Default**: `std::allocator<std::pair<Key, Value>>`
  - **Recommended for performance**: `HugePageAllocator<std::pair<Key, Value>>` for working sets >1GB (3-5× faster)
    - Automatically creates separate pools for leaf and internal nodes via rebind
    - Default: 256MB initial pool, 64MB growth per pool
    - Requires hugepages configured: `sudo sysctl -w vm.nr_hugepages=<num_pages>`
    - Falls back to regular pages if unavailable
  - **Advanced**: `PolicyBasedHugePageAllocator<std::pair<Key, Value>, TwoPoolPolicy>`
    - Fine-grained control over pool sizes
    - Share pools across multiple trees
    - Separate pools for leaf and internal nodes with custom sizes

---

## Performance

Benchmarks comparing against Abseil's `btree_map` and `std::map` are available in [docs/btree_benchmark_results.md](docs/btree_benchmark_results.md).

**Highlights** (8-byte keys, 32-byte values, 10M elements):
- **INSERT P99.9**: 939.9 ns (vs 3171.1 ns Abseil, 3517.1 ns std::map)
- **FIND P99.9**: 857.5 ns (vs 1291.3 ns Abseil, 2177.9 ns std::map)
- **ERASE P99.9**: 1059.9 ns (vs 1616.1 ns Abseil, 2288.0 ns std::map)

The hugepage allocator provides the largest performance gains by reducing TLB misses (benefits find operations) and making allocations extremely cheap through pooling (benefits insert/erase operations).

---

## Building from Source

### Using CMake Presets (Recommended)

```bash
# List available presets
cmake --list-presets

# Configure, build, and test in one workflow
cmake --preset release
cmake --build --preset release
ctest --preset release

# Common presets:
cmake --preset debug          # Debug build
cmake --preset release        # Release with AVX2 (default)
cmake --preset asan           # AddressSanitizer build
cmake --preset release-no-avx2  # Release without AVX2
```

### Manual Build (Alternative)

```bash
# Clone with submodules
git clone --recursive https://github.com/kressler/fast-containers.git
cd fast-containers

# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_AVX2` | `ON` (Release), `OFF` (Debug) | Enable AVX2 SIMD optimizations |
| `ENABLE_ASAN` | `OFF` | Enable AddressSanitizer |
| `ENABLE_ALLOCATOR_STATS` | `OFF` | Enable allocator statistics |
| `ENABLE_LTO` | `ON` | Enable Link-Time Optimization |
| `ENABLE_NUMA` | Auto-detected | Enable NUMA support (requires libnuma) |

---

## Development

### Setup

1. Clone with submodules:
   ```bash
   git clone --recursive https://github.com/kressler/fast-containers.git
   cd fast-containers
   ```

2. One-time development setup:
   ```bash
   ./setup-dev.sh
   ```
   This installs pre-commit hooks and configures clang-tidy.

### Code Quality Tools

**Automatic formatting and checks** (via pre-commit hook):
```bash
git commit  # Automatically formats code and runs clang-tidy
```

The pre-commit hook will:
- Format all staged C++ files with clang-format
- Check production headers with clang-tidy
- Fail the commit if warnings are found
- Auto-create build directories if missing

**Manual formatting:**
```bash
cmake --build build --target format
```

**Manual static analysis:**
```bash
cmake --build build --target clang-tidy
# Or manually:
clang-tidy-19 -p cmake-build-clang-tidy include/fast_containers/*.hpp
```

**Requirements:**
- clang-format (for code formatting)
- clang-tidy-19 (for static analysis)
- cmake (to auto-create build directories)

**Bypass hook** (when needed):
```bash
git commit --no-verify
```

### Development Workflow

1. Make your changes

2. Build and test:
   ```bash
   cmake --build build && ctest --test-dir build
   ```

3. Commit (auto-formatted and checked):
   ```bash
   git add .
   git commit -m "Your changes"
   # Pre-commit hook runs automatically
   ```

### Code Conventions

- Follow Google C++ Style Guide (enforced by clang-format)
- Use C++23 features
- Write tests for new functionality using Catch2
- Production code must be clang-tidy clean (enforced in CI and pre-commit)
- Run `cmake --build build --target format` before submitting PRs

---

## Project Structure

```
.
├── include/
│   └── fast_containers/         # Public header files
│       ├── btree.hpp, btree.ipp
│       ├── dense_map.hpp, dense_map.ipp
│       ├── hugepage_allocator.hpp
│       ├── policy_based_hugepage_allocator.hpp
│       └── hugepage_pool.hpp
├── tests/                       # Unit tests (Catch2)
│   ├── test_btree.cpp
│   ├── test_dense_map.cpp
│   ├── test_hugepage_allocator.cpp
│   └── test_policy_based_allocator.cpp
├── src/
│   ├── benchmarks/              # Google Benchmark performance tests
│   │   ├── dense_map_search_benchmark.cpp
│   │   └── hugepage_allocator_benchmark.cpp
│   └── binary/                  # Standalone benchmark executables
│       ├── btree_benchmark.cpp
│       └── btree_stress.cpp
├── scripts/
│   └── interleaved_btree_benchmark.py  # A/B testing harness
├── docs/
│   └── btree_benchmark_results.md      # Performance analysis
├── third_party/                 # Git submodules
│   ├── catch2/                  # Unit testing framework
│   ├── benchmark/               # Google Benchmark
│   ├── histograms/              # Latency histogram library
│   ├── abseil-cpp/              # Comparison baseline
│   ├── lyra/                    # Command-line parsing
│   └── unordered_dense/         # Dense hash map
├── hooks/                       # Git hooks (install with setup-dev.sh)
│   └── pre-commit               # Auto-format and clang-tidy
└── CMakeLists.txt               # Build configuration
```

## Tech Stack

- **Language**: C++23
- **Build System**: CMake 3.30+
- **Testing**: Catch2 v3.11.0
- **Code Formatting**: clang-format (Google C++ Style)
- **Static Analysis**: clang-tidy-19

## License

This project is licensed under the **BSD 3-Clause License**.

Copyright (c) 2025, Bryan Kressler

See the [LICENSE](LICENSE) file for full details.

### Summary

You are free to use, modify, and distribute this software in source and binary forms, with or without modification, provided that:
- Redistributions of source code retain the copyright notice, list of conditions, and disclaimer
- Redistributions in binary form reproduce the copyright notice in the documentation
- Neither the name of the copyright holder nor contributors may be used to endorse derived products without permission

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
