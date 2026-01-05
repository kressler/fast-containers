# Orderbook Benchmark Results

Real-world performance testing using a multi-threaded orderbook simulation processing one full day of NASDAQ market data.

## Benchmark Overview

### System Configuration

**Hardware:**
- AMD Ryzen 9 5950X (16 cores, 32 threads)
- 128GB RAM
- 2× 32MB L3 cache (one per core complex)

**Core Isolation:**
- **Isolated worker cores**: 4, 5, 6, 7, 12, 13, 14, 15 (8 cores total)
  - Split evenly across both core complexes (4 cores each)
  - Full 64MB of L3 cache available (32MB per core complex)
  - Isolated from kernel scheduling via `isolcpus` boot parameter
- **Reader core**: 0 (non-isolated)
  - Dedicated to feeding event queues to worker threads
  - Prevents reader I/O from interfering with orderbook processing

### Architecture

**Threading model:**
- 8 worker threads (one per isolated core)
- Single reader thread parses event file, distributes to worker queues
- Each worker maintains independent orderbooks for assigned symbols

**Orderbook structure:**
```cpp
struct Orderbook {
  // Two btrees for bid/ask sides
  btree<BookKey, OrderData, std::less<BookKey>>  asks;   // ascending prices
  btree<BookKey, OrderData, std::greater<BookKey>> bids; // descending prices

  // Lookup table: order_id -> (price, sequence_number)
  ankerl::unordered_dense::map<OrderId, BookKey> order_map;
};
```

**Key structure:**
```cpp
struct BookKey {
  int64_t price;            // Price in integer representation
  uint64_t sequence_number; // Monotonically increasing per order
};
```

**Value structure:**
- **24-byte values**: Order size + timestamp + other metadata
- **256-byte values**: Order size + timestamp + other metadata

**Allocator configuration:**
- Hugepage configs use **one shared pooling allocator per thread/core**
- Each allocator pre-allocates from 2MB hugepages
- Separate pools for btree nodes and unordered_dense buckets

### Operations

| Operation | Description | Tree Operations | Ordermap Operations |
|-----------|-------------|-----------------|---------------------|
| **ADD** | Create new order | `btree.insert(book_key, order_data)` | `order_map.insert(order_id, book_key)` |
| **MODIFY** | Update existing order in-place | `btree.find()` + value modification | `order_map.find()` for lookup |
| **REPLACE** | Cancel old order, create new one | `btree.erase()` + `btree.insert()` | `order_map.erase()` + `order_map.insert()` |
| **CANCEL** | Remove existing order | `btree.erase()` | `order_map.find()` + `order_map.erase()` |

### Dataset

- **Source**: NASDAQ ITCH 5.0 feed (2023-08-22)
- **Symbols**: All NASDAQ-listed symbols (~8,000 symbols)
- **Duration**: Full trading day (9:30 AM - 4:00 PM ET)
- **Total events**: ~500 million order events
- **Event distribution**:
  - ADD: ~35%
  - MODIFY: ~15%
  - REPLACE: ~10%
  - CANCEL: ~40%

### Methodology

- **Interleaved A/B testing**: 9 passes (5 forward, 4 reverse) to eliminate thermal/boost bias
- **Timing**: Per-operation cycle counts measured with RDTSC
- **Metrics**: P50 (median), P99.9 (tail latency) in CPU cycles
- **Configurations tested**:
  - `btree_k16_v{24,256}_l{32,8}_i{36,32}_hp`: Our btree with hugepages
  - `btree_k16_v{24,256}_l{32,8}_i{36,32}`: Our btree with standard allocator
  - `absl_btree_k16_v{24,256}_hp`: Abseil btree with `MultiSizeHugePageAllocator`
  - `absl_btree_k16_v{24,256}`: Abseil btree with standard allocator
  - `std_map_k16_v{24,256}`: Standard library `std::map`

---

## Results Summary

### 24-Byte Values (Small Values)

| Metric | Our btree + HP | Abseil + HP | Std::map | Our advantage |
|--------|----------------|-------------|----------|---------------|
| **Overall P99.9** | 4,877 cycles | 5,307 cycles (+8.82%) | 8,455 cycles (+73.36%) | **8.8% faster** |
| **Overall P50** | 778 cycles | 817 cycles (+5.01%) | 990 cycles (+27.25%) | **5.0% faster** |
| **ADD P99.9** | 3,499 cycles | 4,060 cycles (+16.03%) | 9,472 cycles (+170.71%) | **16.0% faster** |
| **MODIFY P99.9** | 3,866 cycles | 4,235 cycles (+9.54%) | 6,166 cycles (+59.49%) | **9.5% faster** |
| **REPLACE P99.9** | 4,580 cycles | 5,141 cycles (+12.25%) | 8,885 cycles (+94.00%) | **12.3% faster** |
| **CANCEL P99.9** | 4,168 cycles | 4,596 cycles (+10.27%) | 6,663 cycles (+59.86%) | **10.3% faster** |

### 256-Byte Values (Large Values)

| Metric | Our btree + HP | Abseil + HP | Std::map | Our advantage |
|--------|----------------|-------------|----------|---------------|
| **Overall P99.9** | 5,496 cycles | 7,467 cycles (+35.86%) | 13,302 cycles (+142.03%) | **35.9% faster** |
| **Overall P50** | 880 cycles | 1,132 cycles (+28.64%) | 1,131 cycles (+28.52%) | **28.6% faster** |
| **ADD P99.9** | 4,062 cycles | 6,017 cycles (+48.13%) | 14,995 cycles (+269.15%) | **48.1% faster** |
| **MODIFY P99.9** | 4,278 cycles | 5,752 cycles (+34.46%) | 6,719 cycles (+57.06%) | **34.5% faster** |
| **REPLACE P99.9** | 5,213 cycles | 7,913 cycles (+51.79%) | 9,887 cycles (+89.66%) | **51.8% faster** |
| **CANCEL P99.9** | 4,770 cycles | 6,391 cycles (+33.98%) | 7,393 cycles (+54.99%) | **34.0% faster** |

---

## 24-Byte Values - Detailed Results

### Overall P99.9 Latency Comparison

| Config | Passes | P99.9 Median | Min | Max | StdDev | vs Best |
|--------|--------|--------------|-----|-----|--------|---------|
| ⭐ **btree_k16_v24_l32_i36_hp** | 9 | **4,877** | 4,841 | 5,081 | 89.76 | baseline |
| absl_btree_k16_v24_hp | 9 | 5,307 | 5,246 | 5,517 | 82.76 | +8.82% |
| btree_k16_v24_l32_i36 | 9 | 5,685 | 5,647 | 5,976 | 116.04 | +16.57% |
| absl_btree_k16_v24 | 9 | 5,896 | 5,820 | 6,119 | 88.32 | +20.89% |
| std_map_k16_v24 | 9 | 8,455 | 8,316 | 9,037 | 239.20 | +73.36% |

**Winner:** btree_k16_v24_l32_i36_hp (Variance: Range 240.0 cycles / 4.92%, StdDev 89.76 cycles / 1.84%)

### Overall P50 (Median) Latency Comparison

| Config | Passes | P50 Median | Min | Max | StdDev | vs Best |
|--------|--------|------------|-----|-----|--------|---------|
| ⭐ **btree_k16_v24_l32_i36_hp** | 9 | **778** | 751 | 780 | 9.14 | baseline |
| btree_k16_v24_l32_i36 | 9 | 778 | 751 | 816 | 16.51 | +0.00% |
| absl_btree_k16_v24_hp | 9 | 817 | 783 | 849 | 16.52 | +5.01% |
| absl_btree_k16_v24 | 9 | 823 | 821 | 849 | 14.01 | +5.78% |
| std_map_k16_v24 | 9 | 990 | 987 | 1,022 | 15.76 | +27.25% |

**Winner:** btree_k16_v24_l32_i36_hp (Variance: Range 29.0 cycles / 3.73%, StdDev 9.14 cycles / 1.18%)

### P99.9 Latency by Operation Type

#### ADD Operations

| Config | P99.9 Median | StdDev | vs Best |
|--------|--------------|--------|---------|
| ⭐ **btree_k16_v24_l32_i36_hp** | **3,499** | 47.06 | baseline |
| absl_btree_k16_v24_hp | 4,060 | 51.14 | +16.03% |
| absl_btree_k16_v24 | 5,552 | 38.95 | +58.67% |
| std_map_k16_v24 | 9,472 | 84.93 | +170.71% |
| btree_k16_v24_l32_i36 | 12,988 | 207.83 | +271.19% |

#### MODIFY Operations

| Config | P99.9 Median | StdDev | vs Best |
|--------|--------------|--------|---------|
| ⭐ **btree_k16_v24_l32_i36_hp** | **3,866** | 67.17 | baseline |
| btree_k16_v24_l32_i36 | 4,067 | 116.93 | +5.20% |
| absl_btree_k16_v24_hp | 4,235 | 68.94 | +9.54% |
| absl_btree_k16_v24 | 4,659 | 74.35 | +20.51% |
| std_map_k16_v24 | 6,166 | 231.06 | +59.49% |

#### REPLACE Operations

| Config | P99.9 Median | StdDev | vs Best |
|--------|--------------|--------|---------|
| ⭐ **btree_k16_v24_l32_i36_hp** | **4,580** | 59.89 | baseline |
| btree_k16_v24_l32_i36 | 4,988 | 99.85 | +8.91% |
| absl_btree_k16_v24_hp | 5,141 | 59.80 | +12.25% |
| absl_btree_k16_v24 | 5,725 | 68.77 | +25.00% |
| std_map_k16_v24 | 8,885 | 182.72 | +94.00% |

#### CANCEL Operations

| Config | P99.9 Median | StdDev | vs Best |
|--------|--------------|--------|---------|
| ⭐ **btree_k16_v24_l32_i36_hp** | **4,168** | 84.80 | baseline |
| btree_k16_v24_l32_i36 | 4,432 | 108.46 | +6.33% |
| absl_btree_k16_v24_hp | 4,596 | 67.75 | +10.27% |
| absl_btree_k16_v24 | 4,994 | 79.07 | +19.82% |
| std_map_k16_v24 | 6,663 | 238.94 | +59.86% |

### Detailed Percentiles - btree_k16_v24_l32_i36_hp (Winner)

| Percentile | Latency (cycles) |
|------------|------------------|
| P0 (min) | 170 |
| P50 (median) | 776 |
| P95 | 2,395 |
| P99 | 3,507 |
| P99.9 | 4,877 |

---

## 256-Byte Values - Detailed Results

### Overall P99.9 Latency Comparison

| Config | Passes | P99.9 Median | Min | Max | StdDev | vs Best |
|--------|--------|--------------|-----|-----|--------|---------|
| ⭐ **btree_k16_v256_l8_i32_hp** | 9 | **5,496** | 5,357 | 5,545 | 54.12 | baseline |
| absl_btree_k16_v256_hp | 9 | 7,467 | 7,396 | 7,527 | 39.25 | +35.86% |
| std_map_k16_v256 | 9 | 13,302 | 13,162 | 13,383 | 72.59 | +142.03% |
| absl_btree_k16_v256 | 9 | 14,688 | 14,533 | 14,810 | 98.82 | +167.25% |
| btree_k16_v256_l8_i32 | 9 | 15,262 | 15,032 | 15,392 | 117.18 | +177.69% |

**Winner:** btree_k16_v256_l8_i32_hp (Variance: Range 188.0 cycles / 3.42%, StdDev 54.12 cycles / 0.98%)

### Overall P50 (Median) Latency Comparison

| Config | Passes | P50 Median | Min | Max | StdDev | vs Best |
|--------|--------|------------|-----|-----|--------|---------|
| ⭐ **btree_k16_v256_l8_i32_hp** | 9 | **880** | 852 | 885 | 14.36 | baseline |
| btree_k16_v256_l8_i32 | 9 | 913 | 887 | 915 | 8.77 | +3.75% |
| std_map_k16_v256 | 9 | 1,131 | 1,128 | 1,152 | 7.52 | +28.52% |
| absl_btree_k16_v256_hp | 9 | 1,132 | 1,129 | 1,134 | 1.56 | +28.64% |
| absl_btree_k16_v256 | 9 | 1,152 | 1,131 | 1,158 | 11.35 | +30.91% |

**Winner:** btree_k16_v256_l8_i32_hp (Variance: Range 33.0 cycles / 3.75%, StdDev 14.36 cycles / 1.63%)

### P99.9 Latency by Operation Type

#### ADD Operations

| Config | P99.9 Median | StdDev | vs Best |
|--------|--------------|--------|---------|
| ⭐ **btree_k16_v256_l8_i32_hp** | **4,062** | 28.09 | baseline |
| absl_btree_k16_v256_hp | 6,017 | 36.03 | +48.13% |
| std_map_k16_v256 | 14,995 | 87.65 | +269.15% |
| absl_btree_k16_v256 | 16,508 | 133.12 | +306.40% |
| btree_k16_v256_l8_i32 | 16,616 | 249.44 | +309.06% |

#### MODIFY Operations

| Config | P99.9 Median | StdDev | vs Best |
|--------|--------------|--------|---------|
| ⭐ **btree_k16_v256_l8_i32_hp** | **4,278** | 60.24 | baseline |
| btree_k16_v256_l8_i32 | 4,945 | 16.98 | +15.59% |
| absl_btree_k16_v256_hp | 5,752 | 32.37 | +34.46% |
| absl_btree_k16_v256 | 6,570 | 21.26 | +53.58% |
| std_map_k16_v256 | 6,719 | 57.91 | +57.06% |

#### REPLACE Operations

| Config | P99.9 Median | StdDev | vs Best |
|--------|--------------|--------|---------|
| ⭐ **btree_k16_v256_l8_i32_hp** | **5,213** | 32.10 | baseline |
| absl_btree_k16_v256_hp | 7,913 | 34.64 | +51.79% |
| std_map_k16_v256 | 9,887 | 69.75 | +89.66% |
| absl_btree_k16_v256 | 11,934 | 45.17 | +128.93% |
| btree_k16_v256_l8_i32 | 13,785 | 166.87 | +164.44% |

#### CANCEL Operations

| Config | P99.9 Median | StdDev | vs Best |
|--------|--------------|--------|---------|
| ⭐ **btree_k16_v256_l8_i32_hp** | **4,770** | 40.38 | baseline |
| btree_k16_v256_l8_i32 | 5,904 | 34.84 | +23.77% |
| absl_btree_k16_v256_hp | 6,391 | 34.61 | +33.98% |
| std_map_k16_v256 | 7,393 | 64.25 | +54.99% |
| absl_btree_k16_v256 | 7,478 | 28.64 | +56.77% |

### Detailed Percentiles - btree_k16_v256_l8_i32_hp (Winner)

| Percentile | Latency (cycles) |
|------------|------------------|
| P0 (min) | 170 |
| P50 (median) | 880 |
| P95 | 2,821 |
| P99 | 4,037 |
| P99.9 | 5,508 |

---

## Key Findings

**How Orderbook Benchmarks Differ from Standard Btree Benchmarks:**

This workload presents unique performance characteristics compared to standard btree benchmarks:

1. **Large number of trees → very large working set**: Instead of a few large trees, we maintain ~8,000 independent orderbooks (16,000 btrees total for bid/ask sides). While individual trees are shallower, the aggregate working set is massive, stressing TLB and cache hierarchies differently.

2. **Highly variable tree size**: Low-activity symbols may have only a handful of orders, while high-volume stocks maintain tens of thousands of orders. This creates a mix of hot (frequently accessed, cache-resident) and cold (infrequently accessed) trees.

3. **Unbalanced tree activity**: Most operations concentrate near the top of the book (best bid/ask prices). Orders at competitive prices see frequent modifications and cancellations, while orders far from the market price remain relatively static. This creates hot spots at the beginning of each tree.

4. **16-byte struct keys → no SIMD search optimization**: Keys are `(int64_t price, uint64_t sequence_number)` structs. Unlike the 4-byte or 8-byte primitive keys in standard benchmarks, these compound keys cannot easily leverage SIMD comparison instructions. While SIMD comparisons are possible for struct keys, the added complexity was not worth the performance gain.

5. **Mixed data structure performance**: Measurements include both btree operations and `ankerl::unordered_dense::map` lookups (for order_id → book_key mapping). The overall latencies reflect the combined cost of both structures, though we are not measuring queuing latency or similar threading effects.

---

### 1. Hugepage Allocators Are Critical for Real-World Performance

**24-byte values:**
- Our btree: **16.6% slower** without hugepages (overall P99.9)
- Abseil btree: **11.1% slower** without hugepages
- **Critical insight**: Without hugepages, our btree's ADD operation becomes **271% slower** (12,988 vs 3,499 cycles)

**256-byte values:**
- Our btree: **177.7% slower** without hugepages (overall P99.9)
- Abseil btree: **96.7% slower** without hugepages
- Even std::map outperforms our btree without hugepages by 12.9%

**Why hugepages matter more with large values:**
- More frequent node allocations/deallocations (smaller node capacity with 256-byte values)
- TLB miss cost amortized over fewer entries per node
- Memory bandwidth becomes bottleneck without TLB optimization

### 2. Our Implementation Dominates Across All Operations

**With hugepages (fair comparison):**
- 24-byte values: **8.8-16.0% faster** than Abseil btree with hugepages
- 256-byte values: **33.9-51.8% faster** than Abseil btree with hugepages

**Performance gap widens with larger values:**
- Small values (24 bytes): 8.8% overall advantage
- Large values (256 bytes): 35.9% overall advantage
- Reflects optimized bulk transfer operations and node layout

### 3. Real-World Workload Characteristics

**Tail latency patterns:**
- P99.9 latencies are 6-7× higher than P50 medians
- Reflects worst-case scenarios: node splits, tree rebalancing, cache misses
- Hugepages reduce P99.9 variance significantly (lower StdDev)

### 4. std::map Performance

**Competitive on median latency:**
- 24-byte P50: 990 cycles (27% slower than our btree)
- 256-byte P50: 1,131 cycles (29% slower than our btree)

**Falls behind on tail latency:**
- 24-byte P99.9: 8,455 cycles (73% slower)
- 256-byte P99.9: 13,302 cycles (142% slower)

**Root cause:**
- Scattered node allocations cause TLB thrashing under heavy load
- Pointer chasing across non-contiguous memory during tree traversal
- No benefit from hugepages (doesn't use pooling allocators)

### 5. ADD Operation Anomaly (24-byte values)

**Surprising result:** Our btree without hugepages performs **271% worse** on ADD operations compared to with hugepages (12,988 vs 3,499 cycles).

**Tentative explanation** (not deeply investigated):
- ADD operations create new book entries (new keys in btree)
- High allocation rate triggers frequent malloc/free calls
- Standard allocator suffers from:
  - TLB misses on newly allocated pages
  - Kernel involvement for page allocation
  - Fragmentation effects
- Hugepage allocator eliminates these overheads through pre-allocated pools

This explanation is preliminary and requires further investigation to confirm the exact mechanisms.

### 6. Value Size Impact on Node Capacity

**Configuration differences:**
- 24-byte values: Leaf node size 32, Internal node size 36
- 256-byte values: Leaf node size 8, Internal node size 32

**Why smaller leaf nodes for large values:**
- With 256-byte values, smaller leaf nodes (8 entries) limit data movement during insert/erase operations
- During node splits, merges, or borrowing, the btree must physically copy value data
- 8 entries × 256 bytes = 2KB max data movement vs 32 entries × 256 bytes = 8KB
- Trade-off: Reduced split/merge cost vs increased frequency of splits/merges and increased tree depth
- This helps limit tail latencies (P99.9) at the cost of slightly increased latencies at lower percentiles (P50)

**Performance implications:**
- Larger values → smaller node capacity → more frequent splits/merges
- Smaller leaf nodes (8 vs 32) increase tree depth (more pointer chasing)
- More rebalancing operations → higher tail latencies
- Hugepages become even more critical (more allocations per operation)

### 7. Thread-Local Allocators Minimize Contention

**Architecture benefit:**
- Each worker thread has dedicated allocator instance
- No cross-thread synchronization on allocation paths
- Cache-friendly: allocator metadata stays on local core's cache

**Scalability:**
- 8 concurrent threads maintain independent orderbooks
- Linear scaling with core count (no allocator contention)
- Full utilization of 64MB L3 cache across both core complexes

---

*Benchmark executed on: 2026-01-05*
*Compiler: Clang 19 with -O3 -march=native -mavx2*
*Kernel: Linux 6.16.3 with isolcpus=4,5,6,7,12,13,14,15*
