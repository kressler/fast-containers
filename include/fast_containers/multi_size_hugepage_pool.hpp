// Copyright (c) 2025 Bryan Kressler
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <bit>
#include <cstddef>
#include <memory>
#include <unordered_map>

#include "hugepage_pool.hpp"

namespace kressler::fast_containers {

/**
 * Multi-size memory pool using explicit hugepages with size-class routing.
 *
 * This pool maintains separate HugePagePool instances for different size
 * classes, enabling efficient pooling for containers that allocate multiple
 * node sizes (e.g., absl::btree_map which has different sizes for leaf and
 * internal nodes).
 *
 * Features:
 * - Size-class based routing: allocations rounded up to nearest size class
 * - Automatic pool creation: pools created on-demand for each size class
 * - Hugepage benefits: reduced TLB misses for all size classes
 * - Per-class pooling: each size class uses uniform-sized blocks, enabling
 *   efficient free list management
 *
 * Size class strategy:
 * - 0-512 bytes: Round up to nearest 64 bytes (cache line alignment)
 * - 513-2048 bytes: Round up to nearest 256 bytes
 * - 2049+ bytes: Round up to next power of 2
 *
 * Thread safety: Not thread-safe (same as HugePagePool)
 *
 * Example usage:
 * @code
 * auto pool = std::make_shared<MultiSizeHugePagePool>(
 *     64UL * 1024 * 1024,  // 64MB initial size per pool
 *     true,                // use hugepages
 *     64UL * 1024 * 1024   // 64MB growth size per pool
 * );
 *
 * // Allocate different sizes - automatically routed to appropriate pools
 * void* small = pool->allocate(100, 8);   // → 128-byte pool
 * void* medium = pool->allocate(600, 8);  // → 768-byte pool
 * void* large = pool->allocate(3000, 8);  // → 4096-byte pool
 *
 * pool->deallocate(small, 100);
 * pool->deallocate(medium, 600);
 * pool->deallocate(large, 3000);
 * @endcode
 */
class MultiSizeHugePagePool {  // NOLINT(readability-identifier-naming)
 public:
  using size_type = std::size_t;

  /**
   * Construct multi-size pool with specified configuration.
   *
   * Each size class will have its own HugePagePool created on-demand with
   * the specified parameters.
   *
   * @param initial_size_per_pool Initial size for each size class pool
   * (default 64MB)
   * @param use_hugepages If true, attempt to use hugepages for all pools
   * (default true)
   * @param growth_size_per_pool Growth size for each pool when it needs to
   * expand (default 64MB)
   */
  explicit MultiSizeHugePagePool(size_type initial_size_per_pool = 64UL * 1024 *
                                                                   1024,
                                 bool use_hugepages = true,
                                 size_type growth_size_per_pool = 64UL * 1024 *
                                                                  1024)
      : initial_size_per_pool_(initial_size_per_pool),
        use_hugepages_(use_hugepages),
        growth_size_per_pool_(growth_size_per_pool) {}

  /**
   * Calculate size class for a given allocation size.
   *
   * Size class strategy:
   * - 0-512 bytes: Round up to nearest 64 bytes (cache line)
   * - 513-2048 bytes: Round up to nearest 256 bytes
   * - 2049+ bytes: Round up to next power of 2
   *
   * @param bytes Requested allocation size
   * @return Size class (rounded up size)
   */
  [[nodiscard]] static constexpr size_type get_size_class(size_type bytes) {
    if (bytes == 0) {
      return 0;
    }
    if (bytes <= 512) {
      // Round up to nearest 64 bytes (cache line)
      return (bytes + 63) & ~static_cast<size_type>(63);
    }
    if (bytes <= 2048) {
      // Round up to nearest 256 bytes
      return (bytes + 255) & ~static_cast<size_type>(255);
    }
    // Round up to next power of 2
    return std::bit_ceil(bytes);
  }

  /**
   * Allocate memory of specified size and alignment.
   *
   * Routes allocation to the appropriate size class pool, creating the pool
   * on first use.
   *
   * @param bytes Number of bytes to allocate
   * @param alignment Required alignment (must be power of 2)
   * @return Pointer to allocated memory
   * @throws std::bad_alloc if unable to allocate
   */
  void* allocate(size_type bytes, size_type alignment) {
    if (bytes == 0) {
      return nullptr;
    }

    const size_type size_class = get_size_class(bytes);

    // Get or create pool for this size class
    auto& pool = pools_[size_class];
    if (!pool) {
      pool = std::make_unique<HugePagePool>(
          initial_size_per_pool_, use_hugepages_, growth_size_per_pool_);
    }

    // Allocate from size-specific pool
    // Each pool only handles uniform-sized blocks, so free list works!
    return pool->allocate(size_class, alignment);
  }

  /**
   * Deallocate memory previously allocated from this pool.
   *
   * Routes deallocation to the appropriate size class pool.
   *
   * @param ptr Pointer to memory to deallocate
   * @param bytes Original allocation size (used to determine size class)
   */
  void deallocate(void* ptr, size_type bytes) {
    if (ptr == nullptr || bytes == 0) {
      return;
    }

    const size_type size_class = get_size_class(bytes);

    auto it = pools_.find(size_class);
    if (it != pools_.end()) {
      it->second->deallocate(ptr, size_class);
    }
  }

  /**
   * Check if any pool is using hugepages.
   *
   * @return true if hugepages were successfully enabled
   */
  [[nodiscard]] bool using_hugepages() const { return use_hugepages_; }

  /**
   * Get number of active size classes (pools created).
   *
   * @return Number of size class pools currently active
   */
  [[nodiscard]] size_type active_size_classes() const { return pools_.size(); }

  /**
   * Get statistics for a specific size class.
   *
   * @param size_class Size class to query
   * @return Pointer to pool stats, or nullptr if pool doesn't exist
   */
  [[nodiscard]] const HugePagePool* get_pool(size_type size_class) const {
    auto it = pools_.find(size_class);
    return it != pools_.end() ? it->second.get() : nullptr;
  }

 private:
  std::unordered_map<size_type, std::unique_ptr<HugePagePool>> pools_;
  const size_type
      initial_size_per_pool_;  // NOLINT(readability-identifier-naming)
  const bool use_hugepages_;   // NOLINT(readability-identifier-naming)
  const size_type
      growth_size_per_pool_;  // NOLINT(readability-identifier-naming)
};

}  // namespace kressler::fast_containers
