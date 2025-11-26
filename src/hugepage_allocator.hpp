#pragma once

#include <sys/mman.h>

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
 * - Separate pools per type (via rebind mechanism)
 *
 * Requirements:
 * - Explicit hugepages must be configured on the system:
 *   sudo sysctl -w vm.nr_hugepages=<num_pages>
 * - Falls back to regular pages if hugepages unavailable
 *
 * Implementation notes:
 * - Each instantiation (via rebind) creates a separate pool
 * - Pool size is fixed at construction time
 * - Uses simple free list for deallocation
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

  // Rebind support for STL containers
  template <typename U>
  struct rebind {
    using other = HugePageAllocator<U>;
  };

  /**
   * Construct allocator with specified pool size.
   *
   * @param pool_size_bytes Total size of memory pool in bytes (default 256MB)
   * @param use_hugepages If true, attempt to use hugepages; otherwise use
   * regular pages (default true)
   */
  explicit HugePageAllocator(size_type pool_size_bytes = 256 * 1024 * 1024,
                             bool use_hugepages = true)
      : pool_(std::make_shared<Pool>(pool_size_bytes, use_hugepages)) {}

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
      : pool_(std::make_shared<Pool>(other.pool_->total_size_,
                                     other.pool_->using_hugepages_)) {}

  /**
   * Allocate n objects of type T.
   *
   * @param n Number of objects to allocate
   * @return Pointer to allocated memory
   * @throws std::bad_alloc if pool is exhausted
   */
  T* allocate(size_type n) {
    if (n == 0)
      return nullptr;

    const size_type bytes = n * sizeof(T);
    const size_type alignment = alignof(T);

    // Try to allocate from free list first
    if (!pool_->free_list_.empty()) {
      for (auto it = pool_->free_list_.begin(); it != pool_->free_list_.end();
           ++it) {
        if (it->size >= bytes) {
          void* ptr = it->ptr;
          pool_->free_list_.erase(it);
          return static_cast<T*>(ptr);
        }
      }
    }

    // Allocate from pool
    // Align next_free to alignment requirement
    uintptr_t current = reinterpret_cast<uintptr_t>(pool_->next_free_);
    uintptr_t aligned =
        (current + alignment - 1) & ~(static_cast<uintptr_t>(alignment) - 1);
    size_type padding = aligned - current;

    if (pool_->bytes_remaining_ < bytes + padding) {
      throw std::bad_alloc();
    }

    pool_->next_free_ = reinterpret_cast<std::byte*>(aligned);
    void* result = pool_->next_free_;
    pool_->next_free_ += bytes;
    pool_->bytes_remaining_ -= (bytes + padding);

    return static_cast<T*>(result);
  }

  /**
   * Deallocate n objects at pointer p.
   *
   * @param p Pointer to memory to deallocate
   * @param n Number of objects (used for size calculation)
   */
  void deallocate(T* p, size_type n) {
    if (p == nullptr || n == 0)
      return;

    // Add to free list for reuse
    pool_->free_list_.push_back({p, n * sizeof(T)});
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

 private:
  static constexpr size_type HUGEPAGE_SIZE = 2 * 1024 * 1024;  // 2MB

  struct FreeBlock {
    void* ptr;
    size_type size;
  };

  struct Pool {
    std::byte* base_;
    std::byte* next_free_;
    size_type total_size_;
    size_type bytes_remaining_;
    bool using_hugepages_;
    std::vector<FreeBlock> free_list_;

    Pool(size_type size, bool use_hugepages)
        : total_size_(size), bytes_remaining_(size), using_hugepages_(false) {
      // Try hugepages first if requested
      if (use_hugepages) {
        base_ = allocate_hugepages(size);
        if (base_ != nullptr) {
          using_hugepages_ = true;
          next_free_ = base_;
          return;
        }
      }

      // Fall back to regular pages
      base_ = allocate_regular(size);
      next_free_ = base_;
    }

    ~Pool() {
      if (base_ != nullptr) {
        munmap(base_, total_size_);
      }
    }

    // Non-copyable, non-movable
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

   private:
    std::byte* allocate_hugepages(size_type size) {
      // Round up to hugepage boundary
      size_type aligned_size =
          (size + HUGEPAGE_SIZE - 1) & ~(HUGEPAGE_SIZE - 1);

      void* ptr = mmap(nullptr, aligned_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

      if (ptr == MAP_FAILED) {
        return nullptr;  // Hugepages not available
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

      total_size_ = aligned_size;
      bytes_remaining_ = aligned_size;

      return static_cast<std::byte*>(ptr);
    }

    std::byte* allocate_regular(size_type size) {
      void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

      if (ptr == MAP_FAILED) {
        throw std::bad_alloc();
      }

      // Hint to use transparent hugepages if available
      madvise(ptr, size, MADV_HUGEPAGE);

      return static_cast<std::byte*>(ptr);
    }
  };

  std::shared_ptr<Pool> pool_;

  // Allow rebind to access pool_
  template <typename>
  friend class HugePageAllocator;
};

}  // namespace fast_containers
