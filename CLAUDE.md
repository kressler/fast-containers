# Fast Containers - C++ SIMD Containers

## Repository Split (November 2025)

**IMPORTANT**: This repository has been split into two separate repositories to minimize dependencies for the core library:

1. **fast-containers** (this repository): Core library with unit tests
   - Dependencies: Catch2 only (1 submodule)
   - Contains: ordered_array, btree, allocators, and tests

2. **fast-containers-benchmarks** (separate repository): Benchmarks and stress tests
   - Dependencies: benchmark, abseil-cpp, lyra, unordered_dense, histograms (5 submodules)
   - Uses fast-containers as a git submodule
   - Contains: All Google Benchmark tests, btree_benchmark, btree_stress, etc.

**Reference**: The original combined repository state is preserved in the `pre-split-backup` branch.

## Stack
- C++23, CMake 3.30+, Catch2 v3.11.0
- Style: Google C++ (clang-format), 80 chars, 2 spaces, `int* ptr`

## Structure
```
src/
  ordered_array.hpp (interface)      # 554 lines
  ordered_array.ipp (implementation) # 958 lines
  btree.hpp (interface)              # 667 lines
  btree.ipp (implementation)         # 1532 lines
  hugepage_allocator.hpp, policy_based_hugepage_allocator.hpp
  tests/
third_party/catch2/  # submodule
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

## Byte Array SIMD: Why It Doesn't Work (PR #61 Investigation)

**Context**: Attempted to re-add byte array SIMD support after PR #60's internal node traversal fix, hypothesizing that was the root cause of poor performance in PR #54/#55.

### Approaches Tested

| Approach | 16-byte Find | 32-byte Find | vs Binary |
|----------|--------------|--------------|-----------|
| Byte-level (1 key/iter) | 456B cycles | 467B cycles | 2.9-3.8× slower ❌ |
| Parallel chunked (4 keys/iter, 2×uint64) | 56B cycles | - | 1.81× slower ❌ |
| Parallel chunked (2 keys/iter, 4×uint64) | - | 93B cycles | 2.91× slower ❌ |
| **Binary search** | **31B cycles** | **32B cycles** | **baseline** ✅ |

**Micro-benchmark (misleading)**: Showed 36% improvement for 16-byte chunked vs Binary (114ms vs 177ms)

**Real-world benchmark**: Showed 81-191% regression due to tree traversal, cache misses, realistic access patterns

### Root Cause: Scalar Encoding Overhead

**Per 4-key iteration (16-byte chunked approach):**
```cpp
// 8 chunks total (4 keys × 2 chunks each)
8× std::memcpy()              // Extract chunks from byte arrays
8× __builtin_bswap64()         // Big-endian → native endianness
8× XOR with 0x8000000000000000 // Flip sign bit for unsigned comparison
4× _mm256_set_epi64x()         // Build SIMD vectors
// Finally: SIMD comparison
```

**Compare to native int64_t SIMD:**
```cpp
1× _mm256_load_si256(&keys[i]) // Aligned load
1× _mm256_xor_si256()          // Optional: flip sign bit for unsigned
// Done: ready to compare
```

**Impact**: 8 scalar operations per iteration completely negate parallelism benefits

### Key Learnings

- ✅ **Unsigned byte comparison**: XOR bytes with `0x80` before `_mm_cmpgt_epi8` to convert unsigned → signed comparison space
- ✅ **-ffast-math incompatibility**: Breaks `std::isinf()` but not actual encoding/decoding
- ❌ **Micro-benchmarks mislead**: Cache locality and simplified access patterns don't reflect real workloads
- ❌ **Parallel SIMD can't overcome encoding overhead**: Even 4-keys-at-a-time loses to Binary search
- ❌ **PR #60 wasn't the bottleneck**: Internal node traversal fix helped, but encoding cost is the real issue

### Final Conclusion

**Byte array SIMD is not viable.** The original decision in PR #54 → PR #55 to remove it was correct.

**Recommendation**: Use `SearchMode::Binary` or `SearchMode::Linear` for encoded byte arrays. SIMD only benefits native primitive types (int32_t, uint32_t, int64_t, uint64_t, float, double) where no encoding overhead exists.

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

## Pooled Values: Reducing Data Movement (Issue #89)

### Problem Statement

From profiling btree cache misses and instructions in orderbook benchmarks, a large amount of time is spent moving data during insertions and deletions. This is particularly expensive for large values.

**Empirical Evidence**: With 16-byte keys and 24-byte values:
- **Internal nodes**: 64 entries optimal
- **Leaf nodes**: 16-32 entries optimal

This **2-4× divergence** indicates that value movement cost dominates the optimal node size trade-off. Internal nodes only move 8-byte pointers, while leaf nodes move the full 24-byte values during insert/erase/split/merge operations.

### Solution: Three-Pool Allocator Architecture

Store pointers to values instead of values directly in leaf nodes, allocating values from a separate hugepage-backed pool.

**Key Benefits**:
1. **Symmetric node layout**: Both leaf and internal nodes store Key + 8-byte pointer
2. **Unified tuning**: Optimal node sizes converge (both around 48-64 entries)
3. **Reduced data movement**: Only move keys and pointers during insert/erase/split/merge
4. **Stable addresses**: Values have stable memory locations

### Usage

#### Creating a Three-Pool Allocator

```cpp
#include "policy_based_hugepage_allocator.hpp"

// Create three-pool allocator: leaf nodes, internal nodes, and values
auto alloc = make_three_pool_allocator<int, std::string>(
    64ul * 1024ul * 1024ul,  // leaf node pool: 64MB
    64ul * 1024ul * 1024ul,  // internal node pool: 64MB
    32ul * 1024ul * 1024ul,  // value pool: 32MB (new!)
    true);                   // use hugepages (false for testing)

// Create btree with pooled values
using TreeType = btree<int, std::string, 16, 64, std::less<int>,
                       SearchMode::Linear, decltype(alloc)>;
TreeType tree(alloc);

// Use normally - pooling is transparent
tree.insert(42, "value");
auto it = tree.find(42);
tree.erase(42);
```

#### Backward Compatibility with Two-Pool Allocator

```cpp
// Two-pool allocator (inline values - current behavior)
auto alloc_inline = make_two_pool_allocator<int, std::string>(
    64ul * 1024ul * 1024ul,  // leaf node pool
    64ul * 1024ul * 1024ul); // internal node pool

using TreeInline = btree<int, std::string, 16, 64, std::less<int>,
                         SearchMode::Linear, decltype(alloc_inline)>;
TreeInline tree_inline(alloc_inline);
// Values stored inline in leaf nodes
```

### Implementation Details

#### Conditional Value Storage

The btree uses SFINAE to detect if the allocator provides a value pool:

```cpp
// Type trait to detect value pool capability
template <typename T, typename = void>
struct allocator_provides_value_pool : std::false_type {};

template <typename T>
struct allocator_provides_value_pool<
    T, std::void_t<decltype(T::provides_value_pool)>> : std::true_type {};

// Conditional value storage in LeafNode
using stored_value_type = std::conditional_t<
    allocator_provides_value_pool<Allocator>::value,
    Value*,  // Pooled: store pointer to value
    Value>;  // Inline: store value directly
```

#### Zero-Overhead Abstraction

All branching is resolved at compile-time using `if constexpr`:

```cpp
template <typename Allocator, typename Key, typename Value>
void insert_into_leaf(LeafNode* leaf, const Key& key, const Value& value) {
  if constexpr (allocator_provides_value_pool<Allocator>::value) {
    // Pooled path: allocate value from pool
    Value* val_ptr = allocator_.allocate_value();
    new (val_ptr) Value(value);
    leaf->data.insert_hint(pos, key, val_ptr);
  } else {
    // Inline path: store value directly
    leaf->data.insert_hint(pos, key, value);
  }
}
```

**Result**: No runtime overhead. Compiler generates completely separate code paths.

#### Value Ownership and Cleanup

- **Owner**: The leaf node containing the Value* pointer owns the value
- **Cleanup**: Values are deallocated when:
  - `erase()` removes the entry
  - `clear()` empties the tree
  - Leaf node is destroyed (destructor)
  - Transfer operations preserve ownership

```cpp
// Erase operation
if constexpr (allocator_provides_value_pool<Allocator>::value) {
  Value* val_ptr = pos->second;
  allocator_.deallocate_value(val_ptr);  // Destroy and return to pool
  leaf->data.erase(pos);                  // Remove key + pointer
} else {
  leaf->data.erase(pos);  // Value destroyed automatically
}
```

#### Bulk Transfer Operations

Split/merge operations are **highly efficient** with pooled values:

```cpp
void split_leaf(LeafNode* node, const Key& key, Value* value) {
  LeafNode* new_node = allocate_leaf_node();

  // Transfer half the entries to new node
  // Pooled: Only moves keys and 8-byte pointers (cheap!)
  // Inline: Moves keys and full values (expensive for large values)
  node->data.transfer_suffix_to(new_node->data, LeafNodeSize / 2);

  // Values never move - they stay in their original memory locations
}
```

**Critical advantage**: During splits and merges, values stay in place. Only pointers are transferred between nodes.

### Performance Trade-offs

#### Costs
1. **Indirection overhead**: One pointer dereference per value access
2. **Memory overhead**: +8 bytes per entry for pointer
3. **Allocation overhead**: More frequent allocations (mitigated by pooling)

#### Benefits
1. **Reduced movement**: Values never move during insert/erase/split/merge
2. **Efficient splits/merges**: Only move keys + pointers (not values)
3. **Larger leaf nodes**: Can increase from 16-32 to 48-64 entries
4. **Shallower trees**: More entries per node → fewer traversals

#### Value Size Threshold

Not all value types benefit from pooling:

```cpp
constexpr bool should_use_value_pool() {
  constexpr size_t threshold = 16;  // Bytes
  return sizeof(Value) > threshold;
}
```

**Rationale**:
- **≤ 8 bytes**: Pointer overhead (8 bytes) equals or exceeds value size - always inline
- **8-16 bytes**: Transition zone - indirection overhead may dominate
- **> 16 bytes**: Movement cost dominates - use pool

**Workload dependency**:
- **Find-heavy** (90%+ finds): Higher threshold (~24-32 bytes) due to indirection cost
- **Modify-heavy** (frequent insert/erase): Lower threshold (~12-16 bytes) - movement cost dominates
- **Orderbooks** (~60-80% finds): Threshold around **16-20 bytes** optimal

### Testing

Comprehensive test coverage (200+ test cases) validates:

```cpp
TEST_CASE("btree with pooled values - basic operations") {
  auto alloc = make_three_pool_allocator<int, int>(
      1024 * 1024, 1024 * 1024, 1024 * 1024, false);

  using TreeType = btree<int, int, 16, 64, std::less<int>,
                         SearchMode::Linear, decltype(alloc)>;
  TreeType tree(alloc);

  // Test insert, find, erase, clear, iteration
  // Test split operations (100+ elements)
  // Test with non-trivial values (std::string)
  // Test edge cases (empty tree, single element)
}
```

**Run tests**:
```bash
cmake --build cmake-build-debug --target test_btree
./cmake-build-debug/src/test_btree "[pooled]"
```

### Future Benchmarking (fast-containers-benchmarks repo)

Phase 6 performance validation will measure:

1. **Optimal leaf node sizes with pooled values** (hypothesis: 48-64 entries)
2. **Value size threshold** (8, 12, 16, 20, 24, 32, 64 bytes)
3. **Cache behavior** (L1, L3 miss rates with callgrind)
4. **Orderbook performance** (compare btree_16_64_hp vs btree_48_64_pooled_hp)

**Expected results for 24-byte values**:
- Leaf nodes: 16-32 → 48-64 entries
- Split/merge cost: 30-50% reduction
- Net performance: 5-20% improvement

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
- [x] Custom comparators (PRs #64, #65, #66)
- [x] Pooled values to reduce data movement (Issue #89)
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

## SIMD Support for std::greater (PR #66)

### Problem
SIMD search mode only worked with `std::less` (ascending order). Users needing descending order had to use slower Binary or Linear search modes.

### Solution
Extended all SIMD implementations to support both `std::less` and `std::greater` using compile-time dispatch:

```cpp
constexpr bool is_ascending = std::is_same_v<Compare, std::less<Key>>;

// For ascending (std::less): find first position where key >= search_key
//   Check: keys_vec < search_vec, find first false
// For descending (std::greater): find first position where key <= search_key
//   Check: keys_vec > search_vec, find first false

if constexpr (is_ascending) {
  cmp = _mm256_cmpgt_epi32(search_vec, keys_vec);  // keys < search
} else {
  cmp = _mm256_cmpgt_epi32(keys_vec, search_vec);  // keys > search
}
```

### Key Points
- **Zero runtime overhead**: All comparator logic resolved at compile-time via `if constexpr`
- **All key sizes supported**: 1, 2, 4, 8 bytes (int8_t through double)
- **Float comparison**: Use `_CMP_LT_OQ` vs `_CMP_GT_OQ` for ascending vs descending
- **Integer comparison**: Swap operands in `_mm256_cmpgt_epi*` calls
- **Updated static_assert**: Now allows both `std::less<Key>` and `std::greater<Key>`

### Performance
SIMD provides same speedup for descending order as ascending:
- Size 32: 45% faster than Linear
- Size 64: 59% faster than Linear

### Files Modified
- `ordered_array.hpp`: Updated static_assert to accept std::greater
- `ordered_array.ipp`: Updated all 4 SIMD implementations (1, 2, 4, 8 byte)
- `test_ordered_array.cpp`: Added 6 comprehensive test sections for std::greater

## Git Submodule Management

### Adding a New Submodule
```bash
git submodule add <repo-url> third_party/<name>
git commit -m "Add <name> as a git submodule"
git push
```

### Updating a Submodule to Latest
```bash
git submodule update --remote third_party/<name>
git add third_party/<name>
git commit -m "Update <name> submodule to latest version"
git push
```

### Using Header-Only Libraries via CMake
When a submodule provides an INTERFACE library target (like histograms):

```cmake
# Add the submodule
add_subdirectory(third_party/histograms)

# Link against it (automatically provides include paths)
target_link_libraries(your_target PRIVATE histograms::histograms)
```

The INTERFACE library pattern provides:
- Include directories (`$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>`)
- Compile features (e.g., `cxx_std_23`)
- Proper namespacing with `::` alias

### Histograms Integration
- **Location**: `third_party/histograms/`
- **Type**: Header-only library
- **Target**: `histograms::histograms` (INTERFACE library)
- **Headers**: `histogram.h`, `log_linear_bucketer.h`
- **Usage**: `#include "histogram.h"` after linking

## Bulk Code Modifications

### Pattern: Updating Lambda Signatures
When refactoring many similar lambda expressions, use `perl` for reliability:

**Task**: Change 143 lambda entries from returning values to taking reference parameters.

**Before**:
```cpp
[&]() -> TimingStats {
  return benchmarker(tree{});
}
```

**After**:
```cpp
[&](TimingStats& stats) -> void {
  benchmarker(tree{}, stats);
}
```

**Commands**:
```bash
# Update lambda signatures
perl -i -pe 's/\[&\]\(\) -> TimingStats \{/[&](TimingStats& stats) -> void {/g' file.cpp

# Remove return keyword
perl -i -pe 's/return benchmarker\(/benchmarker(/g' file.cpp

# Add stats parameter to calls
perl -i -pe 's/>\{\}\);$/>{}, stats);/g' file.cpp
```

### Why Perl Over Sed
- **Perl** handles complex patterns and special characters more reliably
- **Sed** can produce unexpected results with nested parentheses and braces
- For simple substitutions, sed is fine; for complex patterns, use perl

### Verification
```bash
# Count occurrences to verify changes
grep -c "old_pattern" file.cpp  # Should be 0
grep -c "new_pattern" file.cpp  # Should match expected count

# Compile to catch any errors
cmake --build build-dir --target your_target
```
