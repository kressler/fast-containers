#pragma once

#include <sys/mman.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace fast_containers {

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
 *
 * Requirements:
 * - Explicit hugepages must be configured on the system:
 *   sudo sysctl -w vm.nr_hugepages=<num_pages>
 * - Falls back to regular pages if hugepages unavailable
 *
 * Implementation notes:
 * - Each instantiation (via rebind) creates a separate pool
 * - Pool grows by allocating additional regions when exhausted
 * - Only supports allocating/deallocating 1 object at a time (throws for n!=1)
 * - Uses intrusive free list (freed blocks store next pointer in-place)
 * - Requires sizeof(T) >= sizeof(void*) for intrusive free list
 * - Optional statistics tracking (define ALLOCATOR_STATS at compile time)
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
   */
  explicit HugePageAllocator(size_type initial_pool_size = 256 * 1024 * 1024,
                             bool use_hugepages = true,
                             size_type growth_size = 64 * 1024 * 1024)
      : pool_(std::make_shared<Pool>(initial_pool_size, use_hugepages,
                                     growth_size)) {}

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
      : pool_(std::make_shared<Pool>(other.pool_->initial_size_,
                                     other.pool_->using_hugepages_,
                                     other.pool_->growth_size_)) {}

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

    // Try to allocate from free list first
    // Intrusive free list: each freed block stores pointer to next free block
    if (pool_->free_list_head_ != nullptr) {
      void* ptr = pool_->free_list_head_;
      pool_->free_list_head_ = *reinterpret_cast<void**>(ptr);

#ifdef ALLOCATOR_STATS
      pool_->stats_.record_allocation(object_size);
#endif

      return static_cast<T*>(ptr);
    }

    // Allocate from pool
    // Align next_free to cache-line boundary (or larger if required by type)
    uintptr_t current = reinterpret_cast<uintptr_t>(pool_->next_free_);
    uintptr_t aligned = (current + allocation_alignment - 1) &
                        ~(static_cast<uintptr_t>(allocation_alignment) - 1);
    size_type padding = aligned - current;

    // Grow pool if needed
    if (pool_->bytes_remaining_ < object_size + padding) {
      pool_->grow();
      // After growth, recalculate alignment from new next_free_
      current = reinterpret_cast<uintptr_t>(pool_->next_free_);
      aligned = (current + allocation_alignment - 1) &
                ~(static_cast<uintptr_t>(allocation_alignment) - 1);
      padding = aligned - current;
    }

    pool_->next_free_ = reinterpret_cast<std::byte*>(aligned);
    void* result = pool_->next_free_;
    pool_->next_free_ += object_size;
    pool_->bytes_remaining_ -= (object_size + padding);

#ifdef ALLOCATOR_STATS
    pool_->stats_.record_allocation(object_size);
#endif

    return static_cast<T*>(result);
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

    // Add to intrusive free list
    // Store current head pointer in the freed block, then update head
    *reinterpret_cast<void**>(p) = pool_->free_list_head_;
    pool_->free_list_head_ = p;

#ifdef ALLOCATOR_STATS
    pool_->stats_.record_deallocation(object_size);
#endif
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
  bool using_hugepages() const { return pool_->using_hugepages_; }

  /**
   * Get remaining bytes in pool.
   */
  size_type bytes_remaining() const { return pool_->bytes_remaining_; }

#ifdef ALLOCATOR_STATS
  /**
   * Get total number of allocations (compile-time optional).
   */
  size_type get_allocations() const {
    return pool_->stats_.allocations.load(std::memory_order_relaxed);
  }

  /**
   * Get total number of deallocations (compile-time optional).
   */
  size_type get_deallocations() const {
    return pool_->stats_.deallocations.load(std::memory_order_relaxed);
  }

  /**
   * Get number of pool growth events (compile-time optional).
   */
  size_type get_growth_events() const {
    return pool_->stats_.growth_events.load(std::memory_order_relaxed);
  }

  /**
   * Get lifetime total bytes allocated (compile-time optional).
   */
  size_type get_bytes_allocated() const {
    return pool_->stats_.bytes_allocated.load(std::memory_order_relaxed);
  }

  /**
   * Get current bytes in use (compile-time optional).
   */
  size_type get_current_usage() const {
    return pool_->stats_.current_usage.load(std::memory_order_relaxed);
  }

  /**
   * Get peak bytes in use (compile-time optional).
   */
  size_type get_peak_usage() const {
    return pool_->stats_.peak_usage.load(std::memory_order_relaxed);
  }
#endif

 private:
  static constexpr size_type HUGEPAGE_SIZE = 2 * 1024 * 1024;  // 2MB

  struct MemoryRegion {
    std::byte* base = nullptr;
    size_type size = 0;
  };

#ifdef ALLOCATOR_STATS
  /**
   * Statistics tracked by the allocator (compile-time optional).
   * Enable by defining ALLOCATOR_STATS at compile time.
   */
  struct Stats {
    std::atomic<size_type> allocations{0};    // Total allocations
    std::atomic<size_type> deallocations{0};  // Total deallocations
    std::atomic<size_type> growth_events{0};  // Number of pool growths
    std::atomic<size_type> bytes_allocated{
        0};                                   // Lifetime total bytes allocated
    std::atomic<size_type> current_usage{0};  // Current bytes in use
    std::atomic<size_type> peak_usage{0};     // Peak bytes in use

    void record_allocation(size_type bytes) {
      allocations.fetch_add(1, std::memory_order_relaxed);
      bytes_allocated.fetch_add(bytes, std::memory_order_relaxed);
      size_type new_usage =
          current_usage.fetch_add(bytes, std::memory_order_relaxed) + bytes;
      // Update peak if needed (may race, but approximate is fine)
      size_type current_peak = peak_usage.load(std::memory_order_relaxed);
      while (new_usage > current_peak &&
             !peak_usage.compare_exchange_weak(current_peak, new_usage,
                                               std::memory_order_relaxed)) {
      }
    }

    void record_deallocation(size_type bytes) {
      deallocations.fetch_add(1, std::memory_order_relaxed);
      current_usage.fetch_sub(bytes, std::memory_order_relaxed);
    }

    void record_growth() {
      growth_events.fetch_add(1, std::memory_order_relaxed);
    }
  };
#endif

  struct Pool {
    std::vector<MemoryRegion> regions_;
    std::byte* next_free_;
    size_type bytes_remaining_;
    size_type initial_size_;
    size_type growth_size_;
    bool using_hugepages_;
    void* free_list_head_;  // Head of intrusive free list

#ifdef ALLOCATOR_STATS
    Stats stats_;
#endif

    Pool(size_type initial_size, bool use_hugepages, size_type growth_size)
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

    ~Pool() {
      for (const auto& region : regions_) {
        if (region.base != nullptr) {
          munmap(region.base, region.size);
        }
      }
    }

    // Non-copyable, non-movable
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

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

#ifdef ALLOCATOR_STATS
      stats_.record_growth();
#endif
    }

   private:
    MemoryRegion allocate_hugepages_region(size_type size) {
      // Round up to hugepage boundary
      size_type aligned_size =
          (size + HUGEPAGE_SIZE - 1) & ~(HUGEPAGE_SIZE - 1);

      void* ptr = mmap(nullptr, aligned_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

      if (ptr == MAP_FAILED) {
        return {nullptr, 0};  // Hugepages not available
      }

      // Advise kernel about access pattern
      // Use MADV_RANDOM for internal nodes (tree-structured access)
      // Use MADV_SEQUENTIAL for leaf nodes (linked list traversal)
      // For now, use MADV_NORMAL (let kernel decide)
      madvise(ptr, aligned_size, MADV_NORMAL);

      // Pre-fault pages to ensure hugepages are allocated now
      // (avoid blocking page faults later)
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

  std::shared_ptr<Pool> pool_;

  // Allow rebind to access pool_
  template <typename>
  friend class HugePageAllocator;
};

}  // namespace fast_containers
