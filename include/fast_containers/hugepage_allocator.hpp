#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>

#include "hugepage_pool.hpp"

namespace kressler::fast_containers {

/**
 * Pool-based allocator using explicit hugepages (2MB pages on Linux x86-64).
 *
 * This allocator uses mmap with MAP_HUGETLB to allocate memory from explicit
 * hugepages, which provides better TLB performance for large data structures.
 *
 * Benefits:
 * - Reduced TLB misses (2MB pages vs 4KB pages)
 * - Better cache locality within hugepages
 * - Cache-line aligned allocations (64-byte boundaries)
 * - Separate pools per type (via rebind mechanism)
 * - Dynamic growth: pools expand automatically as needed
 * - Optional NUMA awareness (allocate on local NUMA node)
 *
 * Requirements:
 * - Explicit hugepages must be configured on the system:
 *   sudo sysctl -w vm.nr_hugepages=<num_pages>
 * - Falls back to regular pages if hugepages unavailable
 * - For NUMA: requires libnuma library (compile with -DENABLE_NUMA=ON)
 *
 * Implementation notes:
 * - Each instantiation (via rebind) creates a separate pool
 * - Pool grows by allocating additional regions when exhausted
 * - Only supports allocating/deallocating 1 object at a time (throws for n!=1)
 * - Uses intrusive free list (freed blocks store next pointer in-place)
 * - Requires sizeof(T) >= sizeof(void*) for intrusive free list
 * - Optional statistics tracking (define ALLOCATOR_STATS at compile time)
 * - Optional NUMA awareness (define HAVE_NUMA at compile time)
 * - Not thread-safe (add locking if needed for concurrent use)
 *
 * @tparam T The type to allocate
 */
template <typename T>
class HugePageAllocator {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;
  using is_always_equal = std::false_type;

  // Since n==1 is required, these are compile-time constants
  static constexpr size_type object_size = sizeof(T);
  static constexpr size_type object_alignment = alignof(T);

  // Cache line size for alignment (typical x86-64 cache line)
  static constexpr size_type cache_line_size = 64;

  // Use cache-line alignment for better cache utilization
  // Aligns to max of object's natural alignment and cache line boundary
  static constexpr size_type allocation_alignment =
      object_alignment > cache_line_size ? object_alignment : cache_line_size;

  // Intrusive free list requires sufficient size to store a pointer
  static_assert(sizeof(T) >= sizeof(void*),
                "Type T must be at least sizeof(void*) bytes for intrusive "
                "free list");

  // Rebind support for STL containers
  template <typename U>
  struct rebind {
    using other = HugePageAllocator<U>;
  };

  /**
   * Construct allocator with specified pool size.
   *
   * @param initial_pool_size Initial size of memory pool in bytes (default
   * 256MB)
   * @param use_hugepages If true, attempt to use hugepages; otherwise use
   * regular pages (default true)
   * @param growth_size Size of additional regions when pool grows (default
   * 64MB)
   * @param use_numa If true and NUMA available, allocate on local NUMA node
   * (default false)
   */
  explicit HugePageAllocator(size_type initial_pool_size = 256 * 1024 * 1024,
                             bool use_hugepages = true,
                             size_type growth_size = 64 * 1024 * 1024,
                             bool use_numa = false)
      : pool_(std::make_shared<HugePagePool>(initial_pool_size, use_hugepages,
                                             growth_size, use_numa)) {}

  /**
   * Copy constructor (shares pool with other allocator).
   */
  HugePageAllocator(const HugePageAllocator& other) = default;

  /**
   * Rebind constructor (creates separate pool for different type).
   * This allows LeafNode and InternalNode allocators to have independent
   * pools, which can be sized differently based on node sizes and counts.
   */
  template <typename U>
  explicit HugePageAllocator(const HugePageAllocator<U>& other)
      : pool_(std::make_shared<HugePagePool>(
            other.pool_->initial_size(), other.pool_->is_hugepages_enabled(),
            other.pool_->growth_size(), other.pool_->is_numa_enabled())) {}

  /**
   * Allocate n objects of type T.
   *
   * @param n Number of objects to allocate (must be 1)
   * @return Pointer to allocated memory
   * @throws std::invalid_argument if n != 1
   * @throws std::bad_alloc if unable to grow pool
   */
  T* allocate(size_type n) {
    if (n == 0)
      return nullptr;

    if (n != 1) {
      throw std::invalid_argument(
          "HugePageAllocator only supports allocating 1 object at a time");
    }

    // Delegate to pool with required alignment
    void* ptr = pool_->allocate(object_size, allocation_alignment);
    return static_cast<T*>(ptr);
  }

  /**
   * Deallocate n objects at pointer p.
   *
   * @param p Pointer to memory to deallocate
   * @param n Number of objects (must be 1)
   */
  void deallocate(T* p, size_type n) {
    if (p == nullptr || n == 0)
      return;

    if (n != 1) {
      throw std::invalid_argument(
          "HugePageAllocator only supports deallocating 1 object at a time");
    }

    // Delegate to pool
    pool_->deallocate(p, object_size);
  }

  /**
   * Compare allocators for equality (same pool = equal).
   */
  template <typename U>
  friend bool operator==(const HugePageAllocator& lhs,
                         const HugePageAllocator<U>& rhs) noexcept {
    return lhs.pool_ == rhs.pool_;
  }

  template <typename U>
  friend bool operator!=(const HugePageAllocator& lhs,
                         const HugePageAllocator<U>& rhs) noexcept {
    return !(lhs == rhs);
  }

  /**
   * Check if allocator is using hugepages.
   */
  bool using_hugepages() const { return pool_->using_hugepages(); }

  /**
   * Check if allocator is using NUMA-aware allocations.
   */
  bool using_numa() const { return pool_->using_numa(); }

  /**
   * Get remaining bytes in pool.
   */
  size_type bytes_remaining() const { return pool_->bytes_remaining(); }

#ifdef ALLOCATOR_STATS
  /**
   * Get total number of allocations (compile-time optional).
   */
  size_type get_allocations() const { return pool_->get_allocations(); }

  /**
   * Get total number of deallocations (compile-time optional).
   */
  size_type get_deallocations() const { return pool_->get_deallocations(); }

  /**
   * Get number of pool growth events (compile-time optional).
   */
  size_type get_growth_events() const { return pool_->get_growth_events(); }

  /**
   * Get lifetime total bytes allocated (compile-time optional).
   */
  size_type get_bytes_allocated() const { return pool_->get_bytes_allocated(); }

  /**
   * Get current bytes in use (compile-time optional).
   */
  size_type get_current_usage() const { return pool_->get_current_usage(); }

  /**
   * Get peak bytes in use (compile-time optional).
   */
  size_type get_peak_usage() const { return pool_->get_peak_usage(); }
#endif

 private:
  std::shared_ptr<HugePagePool> pool_;

  // Allow rebind to access pool_
  template <typename>
  friend class HugePageAllocator;
};

}  // namespace kressler::fast_containers
