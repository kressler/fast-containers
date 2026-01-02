// Copyright (c) 2025 Bryan Kressler
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <sys/mman.h>

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace kressler::fast_containers {

// Compile-time flag for statistics tracking
#ifdef ALLOCATOR_STATS
inline constexpr bool allocator_stats_enabled = true;
#else
inline constexpr bool allocator_stats_enabled = false;
#endif

/**
 * Type-erased memory pool using explicit hugepages (2MB pages on Linux
 * x86-64).
 *
 * This class provides raw memory allocation without type information,
 * making it suitable for use with policy-based allocators that need to share
 * pools across multiple types.
 *
 * Features:
 * - Reduced TLB misses (2MB pages vs 4KB pages)
 * - Cache-line aligned allocations (handled by caller)
 * - Dynamic growth: pools expand automatically as needed
 * - Optional statistics tracking (define ALLOCATOR_STATS at compile time)
 * - NUMA locality via first-touch: create allocator on desired NUMA node
 *
 * Requirements:
 * - Explicit hugepages must be configured on the system:
 *   sudo sysctl -w vm.nr_hugepages=<num_pages>
 * - Falls back to regular pages if hugepages unavailable
 *
 * Implementation notes:
 * - Not thread-safe (add locking if needed for concurrent use)
 * - Intrusive free list: freed blocks store next pointer in-place
 * - Requires allocated size >= sizeof(void*) for free list
 * - Memory is allocated on NUMA node via first-touch policy during pre-faulting
 */
class HugePagePool {
 public:
  using size_type = std::size_t;

 private:
  static constexpr size_type HUGEPAGE_SIZE = 2 * 1024 * 1024;  // 2MB

  struct MemoryRegion {
    std::byte* base = nullptr;
    size_type size = 0;
  };

  /**
   * Statistics tracked by the pool.
   * Only populated when allocator_stats_enabled is true.
   *
   * Note: Not thread-safe (consistent with rest of pool).
   */
  struct Stats {
    size_type allocations{0};      // Total allocations
    size_type deallocations{0};    // Total deallocations
    size_type growth_events{0};    // Number of pool growths
    size_type bytes_allocated{0};  // Lifetime total bytes allocated
    size_type current_usage{0};    // Current bytes in use
    size_type peak_usage{0};       // Peak bytes in use

    void record_allocation(size_type bytes) {
      if constexpr (allocator_stats_enabled) {
        ++allocations;
        bytes_allocated += bytes;
        current_usage += bytes;
        if (current_usage > peak_usage) {
          peak_usage = current_usage;
        }
      }
    }

    void record_deallocation(size_type bytes) {
      if constexpr (allocator_stats_enabled) {
        ++deallocations;
        current_usage -= bytes;
      }
    }

    void record_growth() {
      if constexpr (allocator_stats_enabled) {
        ++growth_events;
      }
    }
  };

  std::vector<MemoryRegion> regions_;
  std::byte* next_free_;
  size_type bytes_remaining_;
  const size_type initial_size_;
  const size_type growth_size_;
  bool using_hugepages_;
  void* free_list_head_;  // Head of intrusive free list
  Stats stats_;

 public:
  /**
   * Construct pool with specified configuration.
   *
   * Memory will be allocated on the NUMA node where this constructor runs
   * (via first-touch policy during pre-faulting).
   *
   * @param initial_size Initial size of memory pool in bytes (default 256MB)
   * @param use_hugepages If true, attempt to use hugepages; otherwise use
   * regular pages (default true)
   * @param growth_size Size of additional regions when pool grows (default
   * 64MB)
   */
  explicit HugePagePool(size_type initial_size = 256 * 1024 * 1024,
                        bool use_hugepages = true,
                        size_type growth_size = 64 * 1024 * 1024)
      : bytes_remaining_(0),
        initial_size_(initial_size),
        growth_size_(growth_size),
        using_hugepages_(false),
        next_free_(nullptr),
        free_list_head_(nullptr) {
    // Allocate initial region
    MemoryRegion initial_region;

    // Try hugepages first if requested
    if (use_hugepages) {
      initial_region = allocate_hugepages_region(initial_size);
      if (initial_region.base != nullptr) {
        using_hugepages_ = true;
      }
    }

    // Fall back to regular pages if hugepages unavailable or not requested
    if (initial_region.base == nullptr) {
      initial_region = allocate_regular_region(initial_size);
    }

    regions_.push_back(initial_region);
    next_free_ = initial_region.base;
    bytes_remaining_ = initial_region.size;
  }

  ~HugePagePool() {
    for (const auto& region : regions_) {
      if (region.base != nullptr) {
        munmap(region.base, region.size);
      }
    }
  }

  // Non-copyable, non-movable
  HugePagePool(const HugePagePool&) = delete;
  HugePagePool& operator=(const HugePagePool&) = delete;

  /**
   * Allocate raw memory with specified alignment.
   *
   * @param bytes Number of bytes to allocate
   * @param alignment Required alignment (must be power of 2)
   * @return Pointer to allocated memory
   * @throws std::bad_alloc if unable to grow pool
   */
  void* allocate(size_type bytes, size_type alignment) {
    if (bytes == 0)
      return nullptr;

    // Try to allocate from free list first
    if (free_list_head_ != nullptr) {
      void* ptr = free_list_head_;
      free_list_head_ = *reinterpret_cast<void**>(ptr);

      stats_.record_allocation(bytes);

      return ptr;
    }

    // Allocate from pool
    // Align next_free to requested boundary
    uintptr_t current = reinterpret_cast<uintptr_t>(next_free_);
    uintptr_t aligned =
        (current + alignment - 1) & ~(static_cast<uintptr_t>(alignment) - 1);
    size_type padding = aligned - current;

    // Grow pool if needed
    if (bytes_remaining_ < bytes + padding) {
      grow();
      // After growth, recalculate alignment from new next_free_
      current = reinterpret_cast<uintptr_t>(next_free_);
      aligned =
          (current + alignment - 1) & ~(static_cast<uintptr_t>(alignment) - 1);
      padding = aligned - current;
    }

    next_free_ = reinterpret_cast<std::byte*>(aligned);
    void* result = next_free_;
    next_free_ += bytes;
    bytes_remaining_ -= (bytes + padding);

    stats_.record_allocation(bytes);

    return result;
  }

  /**
   * Deallocate raw memory.
   *
   * @param ptr Pointer to memory to deallocate
   * @param bytes Number of bytes (for statistics tracking)
   */
  void deallocate(void* ptr, size_type bytes) {
    if (ptr == nullptr || bytes == 0)
      return;

    // Add to intrusive free list
    *reinterpret_cast<void**>(ptr) = free_list_head_;
    free_list_head_ = ptr;

    stats_.record_deallocation(bytes);
  }

  /**
   * Check if pool is using hugepages.
   */
  bool using_hugepages() const { return using_hugepages_; }

  /**
   * Get remaining bytes in pool.
   */
  size_type bytes_remaining() const { return bytes_remaining_; }

  /**
   * Get initial pool size (for rebind constructor).
   */
  size_type initial_size() const { return initial_size_; }

  /**
   * Get growth size (for rebind constructor).
   */
  size_type growth_size() const { return growth_size_; }

  /**
   * Check if pool is configured to use hugepages.
   */
  bool is_hugepages_enabled() const { return using_hugepages_; }

  /**
   * Get total number of allocations.
   * Only tracked when ALLOCATOR_STATS is defined.
   */
  size_type get_allocations() const { return stats_.allocations; }

  /**
   * Get total number of deallocations.
   * Only tracked when ALLOCATOR_STATS is defined.
   */
  size_type get_deallocations() const { return stats_.deallocations; }

  /**
   * Get number of pool growth events.
   * Only tracked when ALLOCATOR_STATS is defined.
   */
  size_type get_growth_events() const { return stats_.growth_events; }

  /**
   * Get lifetime total bytes allocated.
   * Only tracked when ALLOCATOR_STATS is defined.
   */
  size_type get_bytes_allocated() const { return stats_.bytes_allocated; }

  /**
   * Get current bytes in use.
   * Only tracked when ALLOCATOR_STATS is defined.
   */
  size_type get_current_usage() const { return stats_.current_usage; }

  /**
   * Get peak bytes in use.
   * Only tracked when ALLOCATOR_STATS is defined.
   */
  size_type get_peak_usage() const { return stats_.peak_usage; }

 private:
  /**
   * Grow the pool by allocating a new region of growth_size_.
   * Called when current region is exhausted.
   */
  void grow() {
    MemoryRegion new_region;

    if (using_hugepages_) {
      new_region = allocate_hugepages_region(growth_size_);
      if (new_region.base == nullptr) {
        // Hugepages no longer available, fall back to regular
        new_region = allocate_regular_region(growth_size_);
      }
    } else {
      new_region = allocate_regular_region(growth_size_);
    }

    regions_.push_back(new_region);
    next_free_ = new_region.base;
    bytes_remaining_ = new_region.size;

    stats_.record_growth();
  }

  MemoryRegion allocate_hugepages_region(size_type size) {
    // Round up to hugepage boundary
    size_type aligned_size = (size + HUGEPAGE_SIZE - 1) & ~(HUGEPAGE_SIZE - 1);

    void* ptr = mmap(nullptr, aligned_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

    if (ptr == MAP_FAILED) {
      return {nullptr, 0};  // Hugepages not available
    }

    // Advise kernel about access pattern
    madvise(ptr, aligned_size, MADV_NORMAL);

    // Pre-fault pages to ensure hugepages are allocated now
    // This is where first-touch NUMA policy applies - pages will be allocated
    // on the NUMA node where this code runs
    for (size_type i = 0; i < aligned_size; i += HUGEPAGE_SIZE) {
      static_cast<volatile char*>(ptr)[i] = 0;
    }

    return {static_cast<std::byte*>(ptr), aligned_size};
  }

  MemoryRegion allocate_regular_region(size_type size) {
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (ptr == MAP_FAILED) {
      throw std::bad_alloc();
    }

    // Hint to use transparent hugepages if available
    madvise(ptr, size, MADV_HUGEPAGE);

    return {static_cast<std::byte*>(ptr), size};
  }
};

}  // namespace kressler::fast_containers
