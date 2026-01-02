// Copyright (c) 2025 Bryan Kressler
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <memory>
#include <type_traits>

#include "hugepage_pool.hpp"

namespace kressler::fast_containers {

/**
 * Type trait to detect if a type has next_leaf member (LeafNode
 * characteristic).
 */
template <typename T, typename = void>
struct has_next_leaf : std::false_type {};

template <typename T>
struct has_next_leaf<T, std::void_t<decltype(std::declval<T>().next_leaf)>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_next_leaf_v = has_next_leaf<T>::value;

/**
 * Type trait to detect if a type has children_are_leaves member (InternalNode
 * characteristic).
 */
template <typename T, typename = void>
struct has_children_are_leaves : std::false_type {};

template <typename T>
struct has_children_are_leaves<
    T, std::void_t<decltype(std::declval<T>().children_are_leaves)>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_children_are_leaves_v =
    has_children_are_leaves<T>::value;

/**
 * Two-pool policy: separate pools for leaf and internal nodes.
 *
 * This policy maintains two separate HugePagePool instances:
 * - leaf_pool: for types that have next_leaf member (leaf nodes)
 * - internal_pool: for types that have children_are_leaves member (internal
 * nodes)
 * - All other types use leaf_pool by default
 *
 * Benefits:
 * - Separate sizing: leaf and internal pools can have different sizes
 * - Better cache locality: similar node types allocated together
 * - Pool sharing: multiple btrees can share the same pools
 */
struct TwoPoolPolicy {  // NOLINT(readability-identifier-naming)
  std::shared_ptr<HugePagePool> leaf_pool_;
  std::shared_ptr<HugePagePool> internal_pool_;

  /**
   * Construct policy with two separate pools.
   *
   * @param leaf_pool Pool for leaf nodes
   * @param internal_pool Pool for internal nodes
   */
  TwoPoolPolicy(std::shared_ptr<HugePagePool> leaf_pool,
                std::shared_ptr<HugePagePool> internal_pool)
      : leaf_pool_(std::move(leaf_pool)),
        internal_pool_(std::move(internal_pool)) {}

  /**
   * Get the appropriate pool for type T.
   * - If T has next_leaf member → leaf_pool
   * - If T has children_are_leaves member → internal_pool
   * - Otherwise → leaf_pool (default)
   */
  template <typename T>
  [[nodiscard]] std::shared_ptr<HugePagePool> get_pool_for_type() const {
    // NOLINTNEXTLINE(bugprone-branch-clone)
    if constexpr (has_next_leaf_v<T>) {
      return leaf_pool_;
    } else if constexpr (has_children_are_leaves_v<T>) {
      return internal_pool_;
    } else {
      // Default: use leaf pool for value_type
      return leaf_pool_;
    }
  }
};

/**
 * Policy-based allocator using HugePagePool with configurable pool selection.
 *
 * This allocator uses a policy to determine which pool to use for each type,
 * enabling pool sharing across multiple btrees while maintaining separate
 * pools for different node types.
 *
 * Example usage with TwoPoolPolicy:
 * ```cpp
 * // Create two pools (one for leaves, one for internals)
 * auto leaf_pool = std::make_shared<HugePagePool>(512 * 1024 * 1024, true);
 * auto internal_pool = std::make_shared<HugePagePool>(256 * 1024 * 1024,
 * true);
 *
 * // Create policy that routes types to appropriate pools
 * TwoPoolPolicy policy{leaf_pool, internal_pool};
 * PolicyBasedHugePageAllocator<std::pair<int, std::string>, TwoPoolPolicy>
 *     alloc(policy);
 *
 * // All btrees using this allocator share the same 2 pools
 * btree<int, std::string, ..., decltype(alloc)> tree1(alloc);
 * btree<int, std::string, ..., decltype(alloc)> tree2(alloc);
 * // tree1 and tree2 share leaf_pool for leaves, internal_pool for internals
 *
 * // Access statistics from pools directly
 * std::cout << "Leaf allocations: " << leaf_pool->get_allocations() << "\n";
 * std::cout << "Internal allocations: " << internal_pool->get_allocations()
 *           << "\n";
 * ```
 *
 * @tparam T The type to allocate
 * @tparam PoolPolicy The policy for selecting pools (e.g., TwoPoolPolicy)
 */
template <typename T, typename PoolPolicy>
// NOLINTNEXTLINE(readability-identifier-naming,cppcoreguidelines-special-member-functions)
class PolicyBasedHugePageAllocator {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;
  using is_always_equal = std::false_type;

  // Cache line size for alignment (typical x86-64 cache line)
  static constexpr size_type cache_line_size = 64;
  static constexpr size_type object_alignment = alignof(T);

  // Use cache-line alignment for better cache utilization
  static constexpr size_type allocation_alignment =
      object_alignment > cache_line_size ? object_alignment : cache_line_size;

  // Intrusive free list requires sufficient size to store a pointer
  static_assert(sizeof(T) >= sizeof(void*),
                "Type T must be at least sizeof(void*) bytes for intrusive "
                "free list");

  // Rebind support for STL containers
  template <typename U>
  struct rebind {
    using other = PolicyBasedHugePageAllocator<U, PoolPolicy>;
  };

  /**
   * Construct allocator with a policy.
   *
   * @param policy Policy that determines which pool to use for each type
   */
  explicit PolicyBasedHugePageAllocator(const PoolPolicy& policy)
      : policy_(policy) {}

  /**
   * Copy constructor (shares policy with other allocator).
   */
  PolicyBasedHugePageAllocator(const PolicyBasedHugePageAllocator& other) =
      default;

  /**
   * Copy assignment operator.
   */
  PolicyBasedHugePageAllocator& operator=(
      const PolicyBasedHugePageAllocator& other) = default;

  /**
   * Destructor.
   */
  ~PolicyBasedHugePageAllocator() = default;

  /**
   * Rebind constructor (shares policy across different types).
   * This allows LeafNode and InternalNode allocators to share the same policy
   * (and thus the pools selected by that policy).
   */
  template <typename U>
  explicit PolicyBasedHugePageAllocator(
      const PolicyBasedHugePageAllocator<U, PoolPolicy>& other)
      : policy_(other.policy_) {}

  /**
   * Allocate n objects of type T.
   *
   * @param n Number of objects to allocate (must be 1)
   * @return Pointer to allocated memory
   * @throws std::invalid_argument if n != 1
   * @throws std::bad_alloc if unable to allocate
   */
  T* allocate(size_type n) {
    if (n == 0) {
      return nullptr;
    }

    if (n != 1) {
      throw std::invalid_argument(
          "PolicyBasedHugePageAllocator only supports allocating 1 object at "
          "a time");
    }

    // Get the appropriate pool for this type
    auto pool = policy_.template get_pool_for_type<T>();

    // Allocate from pool with required alignment
    void* ptr = pool->allocate(sizeof(T), allocation_alignment);

    return static_cast<T*>(ptr);
  }

  /**
   * Deallocate n objects at pointer p.
   *
   * @param p Pointer to memory to deallocate
   * @param n Number of objects (must be 1)
   */
  void deallocate(T* p, size_type n) {
    if (p == nullptr || n == 0) {
      return;
    }

    if (n != 1) {
      throw std::invalid_argument(
          "PolicyBasedHugePageAllocator only supports deallocating 1 object "
          "at a time");
    }

    // Get the appropriate pool for this type
    auto pool = policy_.template get_pool_for_type<T>();

    // Deallocate from pool
    pool->deallocate(p, sizeof(T));
  }

  /**
   * Compare allocators for equality (same policy = equal).
   */
  template <typename U>
  friend bool operator==(
      const PolicyBasedHugePageAllocator& lhs,
      const PolicyBasedHugePageAllocator<U, PoolPolicy>& rhs) noexcept {
    // Compare pool pointers for leaf pools (simple equality check)
    return lhs.policy_.template get_pool_for_type<T>() ==
           rhs.policy_.template get_pool_for_type<U>();
  }

  template <typename U>
  friend bool operator!=(
      const PolicyBasedHugePageAllocator& lhs,
      const PolicyBasedHugePageAllocator<U, PoolPolicy>& rhs) noexcept {
    return !(lhs == rhs);
  }

  /**
   * Get the policy (for accessing pools directly).
   */
  [[nodiscard]] const PoolPolicy& get_policy() const { return policy_; }

 private:
  const PoolPolicy policy_;  // NOLINT(readability-identifier-naming)

  // Allow rebind to access policy_
  template <typename, typename>
  friend class PolicyBasedHugePageAllocator;
};

/**
 * Factory function to create a two-pool allocator for btrees.
 *
 * This is a convenience function that simplifies the creation of a
 * PolicyBasedHugePageAllocator with TwoPoolPolicy. It creates two separate
 * pools (one for leaf nodes, one for internal nodes) and wraps them in the
 * appropriate allocator.
 *
 * Example usage:
 * ```cpp
 * auto alloc = make_two_pool_allocator<int, std::string>(
 *     512 * 1024 * 1024,  // leaf pool size
 *     256 * 1024 * 1024,  // internal pool size
 *     true);              // use hugepages
 *
 * btree<int, std::string, 32, 32, std::less<int>, SearchMode::Binary,
 *       MoveMode::Standard, decltype(alloc)> tree(alloc);
 *
 * // Access statistics from pools via get_policy()
 * auto& leaf_pool = alloc.get_policy().leaf_pool_;
 * auto& internal_pool = alloc.get_policy().internal_pool_;
 * std::cout << "Leaf allocations: " << leaf_pool->get_allocations() << "\n";
 * ```
 *
 * @tparam Key The key type for the btree
 * @tparam Value The value type for the btree
 * @param leaf_pool_size Size of the leaf node pool in bytes
 * @param internal_pool_size Size of the internal node pool in bytes
 * @param use_hugepages If true, attempt to use hugepages (default: true)
 * @param leaf_growth_size Size of additional leaf pool regions when pool grows
 * (default: 64MB)
 * @param internal_growth_size Size of additional internal pool regions when
 * pool grows (default: 64MB)
 * @return PolicyBasedHugePageAllocator configured with two pools
 */
template <typename Key, typename Value>
auto make_two_pool_allocator(std::size_t leaf_pool_size,
                             std::size_t internal_pool_size,
                             bool use_hugepages = true,
                             std::size_t leaf_growth_size = 64UL * 1024 * 1024,
                             std::size_t internal_growth_size = 64UL * 1024 *
                                                                1024) {
  auto leaf_pool = std::make_shared<HugePagePool>(leaf_pool_size, use_hugepages,
                                                  leaf_growth_size);
  auto internal_pool = std::make_shared<HugePagePool>(
      internal_pool_size, use_hugepages, internal_growth_size);

  TwoPoolPolicy policy(leaf_pool, internal_pool);
  using ValueType = std::pair<Key, Value>;
  return PolicyBasedHugePageAllocator<ValueType, TwoPoolPolicy>(policy);
}

}  // namespace kressler::fast_containers
