// Copyright (c) 2025 Bryan Kressler
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <memory>

#include "multi_size_hugepage_pool.hpp"

namespace kressler::fast_containers {

/**
 * STL-compatible allocator using MultiSizeHugePagePool.
 *
 * This allocator provides std::allocator interface while routing allocations
 * to a shared MultiSizeHugePagePool. Designed to work with containers that
 * allocate multiple node sizes, such as absl::btree_map.
 *
 * Features:
 * - STL-compatible: works with std::vector, std::map, absl::btree_map, etc.
 * - Shared pool: multiple allocator instances can share the same pool
 * - Stateful: stores reference to shared pool
 * - Copy/move semantics: allocators sharing a pool compare equal
 *
 * Thread safety: Not thread-safe (same as underlying MultiSizeHugePagePool)
 *
 * Example usage with absl::btree_map:
 * @code
 * auto pool = std::make_shared<MultiSizeHugePagePool>(
 *     64UL * 1024 * 1024);  // 64MB per size class
 *
 * using Alloc = MultiSizeHugePageAllocator<std::pair<const int64_t, Value>>;
 * absl::btree_map<int64_t, Value, std::less<int64_t>, Alloc> tree(Alloc(pool));
 *
 * tree[42] = value;  // Allocations routed to appropriate size class
 * @endcode
 */
template <typename T>
class MultiSizeHugePageAllocator {  // NOLINT(readability-identifier-naming)
 public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;
  using is_always_equal = std::false_type;  // Stateful allocator

  /**
   * Construct allocator with shared pool.
   *
   * @param pool Shared pointer to MultiSizeHugePagePool
   */
  explicit MultiSizeHugePageAllocator(
      std::shared_ptr<MultiSizeHugePagePool> pool)
      : pool_(std::move(pool)) {}

  /**
   * Copy constructor (different type).
   *
   * Required for STL allocator rebinding.
   */
  template <typename U>
  explicit MultiSizeHugePageAllocator(
      const MultiSizeHugePageAllocator<U>& other)
      : pool_(other.pool_) {}

  /**
   * Allocate memory for n objects of type T.
   *
   * Routes allocation to appropriate size class in the shared pool.
   *
   * @param n Number of objects to allocate
   * @return Pointer to allocated memory
   * @throws std::bad_alloc if allocation fails
   */
  [[nodiscard]] T* allocate(size_type n) {
    if (n == 0) {
      return nullptr;
    }

    const size_type bytes = n * sizeof(T);
    void* ptr = pool_->allocate(bytes, alignof(T));

    if (ptr == nullptr) {
      throw std::bad_alloc();
    }

    return static_cast<T*>(ptr);
  }

  /**
   * Deallocate memory previously allocated by this allocator.
   *
   * Routes deallocation to appropriate size class in the shared pool.
   *
   * @param ptr Pointer to memory to deallocate
   * @param n Number of objects (used to calculate original allocation size)
   */
  void deallocate(T* ptr, size_type n) {
    if (ptr == nullptr || n == 0) {
      return;
    }

    const size_type bytes = n * sizeof(T);
    pool_->deallocate(ptr, bytes);
  }

  /**
   * Get reference to underlying pool.
   *
   * @return Shared pointer to MultiSizeHugePagePool
   */
  [[nodiscard]] std::shared_ptr<MultiSizeHugePagePool> get_pool() const {
    return pool_;
  }

  /**
   * Compare allocators for equality.
   *
   * Allocators are equal if they share the same pool instance.
   *
   * @param other Allocator to compare with
   * @return true if allocators share the same pool
   */
  template <typename U>
  bool operator==(const MultiSizeHugePageAllocator<U>& other) const {
    return pool_ == other.pool_;
  }

  /**
   * Compare allocators for inequality.
   *
   * @param other Allocator to compare with
   * @return true if allocators use different pools
   */
  template <typename U>
  bool operator!=(const MultiSizeHugePageAllocator<U>& other) const {
    return pool_ != other.pool_;
  }

  // Allow other instantiations to access private members for rebind
  template <typename U>
  friend class MultiSizeHugePageAllocator;

 private:
  std::shared_ptr<MultiSizeHugePagePool> pool_;
};

/**
 * Helper function to create a MultiSizeHugePageAllocator with a new pool.
 *
 * @tparam T Value type for the allocator
 * @param initial_size_per_pool Initial size for each size class pool (default
 * 64MB)
 * @param use_hugepages Whether to use hugepages (default true)
 * @param growth_size_per_pool Growth size for pools (default 64MB)
 * @return Allocator instance with a new shared pool
 */
template <typename T>
MultiSizeHugePageAllocator<T> make_multi_size_hugepage_allocator(
    std::size_t initial_size_per_pool = 64UL * 1024 * 1024,
    bool use_hugepages = true,
    std::size_t growth_size_per_pool = 64UL * 1024 * 1024) {
  auto pool = std::make_shared<MultiSizeHugePagePool>(
      initial_size_per_pool, use_hugepages, growth_size_per_pool);
  return MultiSizeHugePageAllocator<T>(pool);
}

}  // namespace kressler::fast_containers
