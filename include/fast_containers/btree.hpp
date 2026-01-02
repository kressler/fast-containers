// Copyright (c) 2025 Bryan Kressler
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#include "dense_map.hpp"

namespace kressler::fast_containers {

/**
 * Heuristic to calculate a reasonable internal node size based on key size.
 * Targets ~1KB memory footprint (16 cache lines) for cache efficiency.
 *
 * @tparam Key The key type
 * @return Number of entries for internal nodes
 *
 * Formula validated empirically across 8, 16, 24, 32-byte keys:
 *   Target: 1024 bytes
 *   Entry size: sizeof(Key) + sizeof(void*) (child pointer)
 *   Rounded to multiple of 8 for SIMD efficiency
 *   Clamped to [16, 64] to prevent degenerate trees and bound search cost
 *
 * See Issue #90 for empirical validation.
 */
template <typename Key>
constexpr std::size_t default_internal_node_size() {
  constexpr std::size_t target_bytes = 1024;  // 16 cache lines
  constexpr std::size_t entry_size = sizeof(Key) + sizeof(void*);
  constexpr std::size_t calculated = target_bytes / entry_size;

  // Round to nearest multiple of 8 for SIMD efficiency
  constexpr std::size_t rounded = ((calculated + 4) / 8) * 8;

  // Clamp to reasonable bounds
  // Min 16: Prevents degenerate trees with huge keys
  // Max 64: Keeps binary search cost bounded (6 comparisons)
  return std::clamp(rounded, static_cast<std::size_t>(16),
                    static_cast<std::size_t>(64));
}

/**
 * Heuristic to calculate a reasonable leaf node size based on key and value
 * sizes. Targets ~1KB memory footprint (16 cache lines) for cache efficiency.
 *
 * @tparam Key The key type
 * @tparam Value The value type
 * @return Optimal number of entries for leaf nodes
 *
 * Formula targets 2048 bytes (32 cache lines), empirically validated:
 *   Entry size: sizeof(Key) + sizeof(Value)
 *   Rounded to multiple of 8 for SIMD efficiency
 *   Clamped to [8, 64] to prevent extremes
 *
 * Rationale (Issue #90, Phase 3 testing):
 * - Leaf nodes move entire values during splits (expensive)
 * - Larger nodes reduce split frequency → amortizes data movement cost
 * - Internal nodes move only 8-byte pointers → can use smaller 1KB target
 * - Empirical testing shows 2KB better for values 24-128 bytes
 */
template <typename Key, typename Value>
constexpr std::size_t default_leaf_node_size() {
  constexpr std::size_t target_bytes = 2048;  // 32 cache lines
  constexpr std::size_t entry_size = sizeof(Key) + sizeof(Value);
  constexpr std::size_t calculated = target_bytes / entry_size;

  // Round to nearest multiple of 8 for SIMD efficiency
  constexpr std::size_t rounded = ((calculated + 4) / 8) * 8;

  // Clamp to reasonable bounds
  // Min 8: Prevents tiny nodes with huge values
  // Max 64: Keeps data movement cost reasonable during splits
  return std::clamp(rounded, static_cast<std::size_t>(8),
                    static_cast<std::size_t>(64));
}

/**
 * A B+ Tree that uses dense_map as the underlying storage for nodes.
 * All data is stored in leaf nodes, which form a doubly-linked list for
 * efficient sequential traversal. Internal nodes store only keys and pointers
 * to guide searches.
 *
 * @tparam Key The key type (must be ComparatorCompatible with Compare)
 * @tparam Value The value type
 * @tparam LeafNodeSize Maximum number of key-value pairs in each leaf node
 *         (defaults to heuristic size based on key+value size, targeting ~2KB)
 * @tparam InternalNodeSize Maximum number of children in each internal node
 *         (defaults to heuristic size based on key size, targeting ~1KB)
 * @tparam Compare The comparison function object type (defaults to
 *         std::less<Key>)
 * @tparam SearchModeT Search mode passed through to dense_map
 * @tparam Allocator The allocator type (defaults to std::allocator<value_type>)
 *
 * ## Allocator Rebinding
 *
 * The btree uses std::allocator_traits::rebind to create separate allocators
 * for leaf_node and internal_node types from the provided value_type allocator.
 * This enables performance optimizations like separate hugepage pools:
 *
 * - `leaf_alloc_`: rebind_alloc<leaf_node> - allocates leaf nodes
 * - `internal_alloc_`: rebind_alloc<internal_node> - allocates internal nodes
 *
 * When using PolicyBasedHugePageAllocator with TwoPoolPolicy, the policy
 * automatically routes allocations to the appropriate pool via type
 * introspection (leaf_node has `next_leaf` member, internal_node has
 * `children_are_leaves` member). This allows different pool sizes and
 * configurations for leaf vs internal nodes.
 *
 * Example with two separate pools:
 * @code
 * auto alloc = make_two_pool_allocator<Key, Value>(
 *     64ul * 1024ul * 1024ul,  // 64MB leaf pool
 *     64ul * 1024ul * 1024ul); // 64MB internal pool
 * btree<Key, Value, 32, 64, std::less<Key>, SearchMode::Linear,
 *       decltype(alloc)> tree{alloc};
 * @endcode
 */
template <typename Key, typename Value,
          std::size_t LeafNodeSize = default_leaf_node_size<Key, Value>(),
          std::size_t InternalNodeSize = default_internal_node_size<Key>(),
          typename Compare = std::less<Key>,
          SearchMode SearchModeT = SearchMode::Linear,
          typename Allocator = std::allocator<std::pair<Key, Value>>>
  requires ComparatorCompatible<Key, Compare>
class btree {
 public:
  // Type aliases
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<Key, Value>;
  using size_type = std::size_t;
  using allocator_type = Allocator;
  using key_compare = Compare;

  // Compile-time check: SIMD search mode requires a SIMD-searchable key type
  static_assert(
      SearchModeT != SearchMode::SIMD || SIMDSearchable<Key>,
      "SIMD search mode requires a key type that satisfies SIMDSearchable. "
      "Supported types: int32_t, uint32_t, int64_t, uint64_t, float, double. "
      "For other types, use SearchMode::Binary or SearchMode::Linear.");

  // Compile-time check: Minimum node sizes prevent edge cases
  // With hysteresis, underflow_threshold = NodeSize/2 - NodeSize/4 = NodeSize/4
  // For a node with size=1 to not trigger underflow: 1 >= NodeSize/4
  // This holds only when NodeSize <= 4, which creates empty node edge cases
  // during merge operations. Enforcing NodeSize >= 8 eliminates this
  // complexity.
  static_assert(
      LeafNodeSize >= 8,
      "LeafNodeSize must be at least 8 to avoid empty node edge cases "
      "during merge operations.");
  static_assert(InternalNodeSize >= 8,
                "InternalNodeSize must be at least 8 to avoid empty node edge "
                "cases during merge operations.");

  /**
   * Compares value_type (pairs) by their keys.
   * Used by value_comp() to provide pair comparison.
   */
  class value_compare {
   public:
    bool operator()(const value_type& lhs, const value_type& rhs) const {
      return key_compare()(lhs.first, rhs.first);
    }
  };

  /**
   * Leaf node - stores actual key-value pairs in an dense_map.
   * Forms a doubly-linked list with other leaf nodes for iteration.
   */
  // Forward declaration for leaf_node to use in internal_node
  struct internal_node;

  struct leaf_node {
    dense_map<Key, Value, LeafNodeSize, Compare, SearchModeT> data;
    leaf_node* next_leaf;
    leaf_node* prev_leaf;
    internal_node* parent;  // Parent is always internal (or nullptr for root)

    leaf_node() : next_leaf(nullptr), prev_leaf(nullptr), parent(nullptr) {}
  };

  /**
   * Internal node - stores keys and pointers to child nodes.
   * The children_are_leaves flag indicates whether children are leaf_node* or
   * internal_node* (all children at the same level have the same type).
   */
  struct internal_node {
    // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
    // Union used for performance - avoids std::variant overhead
    union {
      dense_map<Key, leaf_node*, InternalNodeSize, Compare, SearchModeT>
          leaf_children;
      dense_map<Key, internal_node*, InternalNodeSize, Compare, SearchModeT>
          internal_children;
    };
    // NOLINTEND(cppcoreguidelines-pro-type-union-access)
    const bool children_are_leaves;
    internal_node* parent;  // Parent is always internal (or nullptr for root)

    // Constructor for leaf children
    explicit internal_node(bool has_leaf_children)
        : children_are_leaves(has_leaf_children), parent(nullptr) {
      // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
      if (children_are_leaves) {
        new (&leaf_children) decltype(leaf_children)();
      } else {
        new (&internal_children) decltype(internal_children)();
      }
      // NOLINTEND(cppcoreguidelines-pro-type-union-access)
    }

    // Destructor - must explicitly destroy the active union member
    ~internal_node() {
      // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
      if (children_are_leaves) {
        leaf_children.~dense_map();
      } else {
        internal_children.~dense_map();
      }
      // NOLINTEND(cppcoreguidelines-pro-type-union-access)
    }

    // Delete copy constructor and assignment (non-copyable due to union)
    internal_node(const internal_node&) = delete;
    internal_node& operator=(const internal_node&) = delete;

    // Delete move constructor and assignment (nodes are managed in-place by
    // btree)
    internal_node(internal_node&&) = delete;
    internal_node& operator=(internal_node&&) = delete;
  };

  /**
   * Default constructor - creates an empty B+ tree.
   *
   * @param alloc Allocator to use for node allocation
   */
  explicit btree(const Allocator& alloc = Allocator());

  /**
   * Destructor - deallocates all nodes.
   */
  ~btree();

  /**
   * Copy constructor - creates a deep copy of the tree.
   *
   * Implementation: Iterates through other and calls insert() for each element,
   * building the tree incrementally.
   *
   * Complexity: O(m log m) where m = other.size()
   *
   * @note A node-by-node copy could achieve O(m) complexity but would add
   * significant implementation complexity.
   */
  btree(const btree& other);

  /**
   * Copy assignment operator - replaces contents with a deep copy.
   *
   * Implementation: Calls clear() to deallocate existing nodes, then iterates
   * through other and calls insert() for each element.
   *
   * Complexity: O(n + m log m) where n = this.size(), m = other.size()
   *             - O(n) to clear existing tree
   *             - O(m log m) to insert m elements into growing tree
   *
   * @note A node-by-node copy could achieve O(n + m) complexity but would add
   * significant implementation complexity.
   */
  btree& operator=(const btree& other);

  /**
   * Move constructor - takes ownership of another tree's resources.
   * Leaves other in a valid but empty state.
   * Complexity: O(1)
   */
  btree(btree&& other) noexcept;

  /**
   * Move assignment operator - replaces contents by taking ownership.
   * Leaves other in a valid but empty state.
   * Complexity: O(n) where n is this tree's size (due to deallocation)
   */
  btree& operator=(btree&& other) noexcept;

  /**
   * Constructs the tree from an initializer list.
   * Enables syntax like: btree<int, string> tree = {{1, "a"}, {2, "b"}};
   * Complexity: O(n log n) where n is the number of elements
   */
  btree(std::initializer_list<value_type> init,
        const Allocator& alloc = Allocator());

  /**
   * Constructs the tree from a range of elements.
   * Complexity: O(n log n) where n is the distance between first and last
   */
  template <typename InputIt>
  btree(InputIt first, InputIt last, const Allocator& alloc = Allocator());

  /**
   * Returns the number of elements in the tree.
   * Complexity: O(1)
   */
  [[nodiscard]] size_type size() const { return size_; }

  /**
   * Returns true if the tree is empty.
   * Complexity: O(1)
   */
  [[nodiscard]] bool empty() const { return size_ == 0; }

  /**
   * Returns the key comparison object.
   * Uses Compare for key comparisons.
   * Complexity: O(1)
   */
  key_compare key_comp() const { return key_compare(); }

  /**
   * Returns the value comparison object.
   * Compares value_type (pairs) by their keys using key_compare.
   * Complexity: O(1)
   */
  value_compare value_comp() const { return value_compare(); }

  /**
   * Returns the allocator associated with the container.
   * Note: Returns a copy constructed from leaf_alloc_ via rebind.
   * Complexity: O(1)
   */
  allocator_type get_allocator() const { return allocator_type(leaf_alloc_); }

  /**
   * Forward iterator for btree.
   * Iterates through elements in sorted order using the leaf node chain.
   */
  class iterator {
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = btree::value_type;
    using pointer = value_type*;
    using reference = typename dense_map<Key, Value, LeafNodeSize, Compare,
                                         SearchModeT>::iterator::reference;

    iterator() : leaf_node_(nullptr) {}

    reference operator*() const {
      assert(leaf_node_ != nullptr && "Dereferencing end iterator");
      return *leaf_it_.value();
    }

    typename dense_map<Key, Value, LeafNodeSize, Compare,
                       SearchModeT>::iterator::arrow_proxy
    operator->() const {
      assert(leaf_node_ != nullptr && "Dereferencing end iterator");
      return leaf_it_.value().operator->();
    }

    iterator& operator++() {
      assert(leaf_node_ != nullptr && "Incrementing end iterator");
      ++(*leaf_it_);
      // If we've reached the end of this leaf, move to next leaf
      if (*leaf_it_ == leaf_node_->data.end()) {
        if (leaf_node_->next_leaf != nullptr) {
          leaf_node_ = leaf_node_->next_leaf;
          leaf_it_ = leaf_node_->data.begin();
        }
        // else: stay at end() position (rightmost_leaf, data.end())
      }
      return *this;
    }

    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    iterator& operator--() {
      assert(leaf_node_ != nullptr &&
             "Cannot decrement default-constructed iterator");

      // Check if we're at end() (one past the last element)
      if (*leaf_it_ == leaf_node_->data.end()) {
        // Move to the last element of the current leaf
        auto end_it = leaf_node_->data.end();
        --end_it;
        leaf_it_ = end_it;
        return *this;
      }

      // Check if we're at the beginning of the current leaf
      if (*leaf_it_ == leaf_node_->data.begin()) {
        // Move to previous leaf
        leaf_node_ = leaf_node_->prev_leaf;
        assert(leaf_node_ != nullptr && "Decrementing past begin()");
        // Move to the last element of the previous leaf
        auto end_it = leaf_node_->data.end();
        --end_it;
        leaf_it_ = end_it;
      } else {
        // Decrement within current leaf
        --(*leaf_it_);
      }
      return *this;
    }

    iterator operator--(int) {
      iterator tmp = *this;
      --(*this);
      return tmp;
    }

    bool operator==(const iterator& other) const {
      // If both are end iterators (leaf_node_ == nullptr), they're equal
      if (leaf_node_ == nullptr && other.leaf_node_ == nullptr) {
        return true;
      }
      // If only one is end iterator, they're not equal
      if (leaf_node_ == nullptr || other.leaf_node_ == nullptr) {
        return false;
      }
      // Both valid - compare leaf node and position
      return leaf_node_ == other.leaf_node_ && *leaf_it_ == *other.leaf_it_;
    }

    bool operator!=(const iterator& other) const { return !(*this == other); }

   private:
    friend class btree;

    iterator(leaf_node* node,
             typename dense_map<Key, Value, LeafNodeSize, Compare,
                                SearchModeT>::iterator it)
        : leaf_node_(node), leaf_it_(it), tree_(nullptr) {}

    iterator(const btree* tree, leaf_node* node,
             typename dense_map<Key, Value, LeafNodeSize, Compare,
                                SearchModeT>::iterator it)
        : leaf_node_(node), leaf_it_(it), tree_(tree) {}

    leaf_node* leaf_node_;
    std::optional<typename dense_map<Key, Value, LeafNodeSize, Compare,
                                     SearchModeT>::iterator>
        leaf_it_;
    const btree* tree_;
  };

  using const_iterator = iterator;  // For now, treat as const

  /**
   * Reverse iterator for btree.
   * Iterates through elements in reverse sorted order using the leaf node
   * chain.
   */
  class reverse_iterator {
   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = btree::value_type;
    using pointer = value_type*;
    using reference = typename dense_map<Key, Value, LeafNodeSize, Compare,
                                         SearchModeT>::iterator::reference;

    reverse_iterator() : leaf_node_(nullptr), tree_(nullptr) {}

    reference operator*() const {
      assert(leaf_node_ != nullptr && "Dereferencing end iterator");
      return *leaf_it_.value();
    }

    typename dense_map<Key, Value, LeafNodeSize, Compare,
                       SearchModeT>::iterator::arrow_proxy
    operator->() const {
      assert(leaf_node_ != nullptr && "Dereferencing end iterator");
      return leaf_it_.value().operator->();
    }

    reverse_iterator& operator++() {
      assert(leaf_node_ != nullptr && "Incrementing end iterator");
      // Check if we're at the beginning of the current leaf
      if (*leaf_it_ == leaf_node_->data.begin()) {
        // Move to previous leaf
        leaf_node_ = leaf_node_->prev_leaf;
        if (leaf_node_ != nullptr) {
          // Start at the end of the previous leaf
          auto end_it = leaf_node_->data.end();
          --end_it;  // Move to last valid element
          leaf_it_ = end_it;
        } else {
          leaf_it_.reset();
        }
      } else {
        // Decrement within current leaf
        --(*leaf_it_);
      }
      return *this;
    }

    reverse_iterator operator++(int) {
      reverse_iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    reverse_iterator& operator--() {
      // Handle decrementing from rend() iterator
      if (leaf_node_ == nullptr) {
        assert(tree_ != nullptr &&
               "Cannot decrement default-constructed iterator");
        // Move to the first element (smallest in sorted order)
        leaf_node_ = tree_->leftmost_leaf_;
        leaf_it_ = leaf_node_->data.begin();
        return *this;
      }

      // Check if we're at the end of the current leaf
      auto end_it = leaf_node_->data.end();
      --end_it;  // Move to last valid element
      if (*leaf_it_ == end_it) {
        // Move to next leaf
        leaf_node_ = leaf_node_->next_leaf;
        assert(leaf_node_ != nullptr && "Decrementing past rbegin()");
        // Move to the first element of the next leaf
        leaf_it_ = leaf_node_->data.begin();
      } else {
        // Increment within current leaf (moving forward in sorted order)
        ++(*leaf_it_);
      }
      return *this;
    }

    reverse_iterator operator--(int) {
      reverse_iterator tmp = *this;
      --(*this);
      return tmp;
    }

    bool operator==(const reverse_iterator& other) const {
      // If both are end iterators (leaf_node_ == nullptr), they're equal
      if (leaf_node_ == nullptr && other.leaf_node_ == nullptr) {
        return true;
      }
      // If only one is end iterator, they're not equal
      if (leaf_node_ == nullptr || other.leaf_node_ == nullptr) {
        return false;
      }
      // Both valid - compare leaf node and position
      return leaf_node_ == other.leaf_node_ && *leaf_it_ == *other.leaf_it_;
    }

    bool operator!=(const reverse_iterator& other) const {
      return !(*this == other);
    }

   private:
    friend class btree;

    reverse_iterator(leaf_node* node,
                     typename dense_map<Key, Value, LeafNodeSize, Compare,
                                        SearchModeT>::iterator it)
        : leaf_node_(node), leaf_it_(it), tree_(nullptr) {}

    reverse_iterator(const btree* tree, leaf_node* node,
                     typename dense_map<Key, Value, LeafNodeSize, Compare,
                                        SearchModeT>::iterator it)
        : leaf_node_(node), leaf_it_(it), tree_(tree) {}

    reverse_iterator(const btree* tree, leaf_node* node)
        : leaf_node_(node), leaf_it_(std::nullopt), tree_(tree) {}

    leaf_node* leaf_node_;
    std::optional<typename dense_map<Key, Value, LeafNodeSize, Compare,
                                     SearchModeT>::iterator>
        leaf_it_;
    const btree* tree_;
  };

  using const_reverse_iterator = reverse_iterator;  // For now, treat as const

  /**
   * Returns an iterator to the first element.
   * Complexity: O(1) - uses cached leftmost leaf pointer
   */
  iterator begin() {
    if (size_ == 0) {
      return end();
    }
    return iterator(leftmost_leaf_, leftmost_leaf_->data.begin());
  }

  /**
   * Returns an iterator to one past the last element.
   * Complexity: O(1)
   */
  iterator end() {
    if (size_ == 0) {
      return iterator();
    }
    return iterator(rightmost_leaf_, rightmost_leaf_->data.end());
  }

  /**
   * Returns a const_iterator to the first element.
   * Complexity: O(1) - uses cached leftmost leaf pointer
   */
  const_iterator begin() const {
    if (size_ == 0) {
      return end();
    }
    return const_iterator(leftmost_leaf_, leftmost_leaf_->data.begin());
  }

  /**
   * Returns a const_iterator to one past the last element.
   * Complexity: O(1)
   */
  const_iterator end() const {
    if (size_ == 0) {
      return const_iterator();
    }
    return const_iterator(rightmost_leaf_, rightmost_leaf_->data.end());
  }

  /**
   * Returns a const_iterator to the first element.
   * Complexity: O(1)
   */
  const_iterator cbegin() const { return begin(); }

  /**
   * Returns a const_iterator to one past the last element.
   * Complexity: O(1)
   */
  const_iterator cend() const { return end(); }

  /**
   * Returns a reverse_iterator to the first element of the reversed container.
   * (i.e., the last element of the non-reversed container)
   * Complexity: O(1) - uses cached rightmost leaf pointer
   */
  reverse_iterator rbegin() {
    if (size_ == 0) {
      return rend();
    }
    auto it = rightmost_leaf_->data.end();
    --it;  // Move to last valid element
    return reverse_iterator(this, rightmost_leaf_, it);
  }

  /**
   * Returns a reverse_iterator to the element following the last element of
   * the reversed container. Complexity: O(1)
   */
  reverse_iterator rend() { return reverse_iterator(this, nullptr); }

  /**
   * Returns a const_reverse_iterator to the first element of the reversed
   * container. Complexity: O(1)
   */
  const_reverse_iterator rbegin() const {
    if (size_ == 0) {
      return rend();
    }
    auto it = rightmost_leaf_->data.end();
    --it;  // Move to last valid element
    return const_reverse_iterator(this, rightmost_leaf_, it);
  }

  /**
   * Returns a const_reverse_iterator to the element following the last element
   * of the reversed container. Complexity: O(1)
   */
  const_reverse_iterator rend() const {
    return const_reverse_iterator(this, nullptr);
  }

  /**
   * Returns a const_reverse_iterator to the first element of the reversed
   * container. Complexity: O(1)
   */
  const_reverse_iterator crbegin() const { return rbegin(); }

  /**
   * Returns a const_reverse_iterator to the element following the last element
   * of the reversed container. Complexity: O(1)
   */
  const_reverse_iterator crend() const { return rend(); }

  /**
   * Finds an element with the given key.
   * Returns an iterator to the element if found, end() otherwise.
   * Complexity: O(log n)
   */
  iterator find(const Key& key);

  /**
   * Finds an element with the given key (const version).
   * Returns a const_iterator to the element if found, end() otherwise.
   * Complexity: O(log n)
   */
  const_iterator find(const Key& key) const;

  /**
   * Returns an iterator to the first element not less than the given key.
   * If all elements are less than key, returns end().
   *
   * Complexity: O(log n)
   */
  iterator lower_bound(const Key& key);

  /**
   * Returns an iterator to the first element greater than the given key.
   * If all elements are less than or equal to key, returns end().
   *
   * Complexity: O(log n)
   */
  iterator upper_bound(const Key& key);

  /**
   * Returns a pair of iterators representing the range of elements with the
   * given key. For a B+ tree with unique keys, this range will contain at most
   * one element. Returns {lower_bound(key), upper_bound(key)}.
   *
   * Complexity: O(log n)
   */
  std::pair<iterator, iterator> equal_range(const Key& key);

  /**
   * Inserts a key-value pair into the tree.
   * Returns a pair with an iterator to the inserted/existing element and a bool
   * indicating whether insertion took place (true) or the key already existed
   * (false).
   *
   * Handles node splitting when nodes are full, growing the tree as needed.
   *
   * Complexity: O(log n)
   */
  std::pair<iterator, bool> insert(const Key& key, const Value& value);

  /**
   * Inserts a key-value pair into the tree.
   * Equivalent to insert(value.first, value.second).
   *
   * Returns a pair with an iterator to the inserted/existing element and a bool
   * indicating whether insertion took place (true) or the key already existed
   * (false).
   *
   * Complexity: O(log n)
   */
  std::pair<iterator, bool> insert(const value_type& value);

  /**
   * Constructs an element in-place in the tree.
   * The arguments are forwarded to construct a value_type (std::pair<Key,
   * Value>). Returns a pair with an iterator to the inserted/existing element
   * and a bool indicating whether insertion took place.
   *
   * Complexity: O(log n)
   */
  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args);

  /**
   * Constructs an element in-place with a position hint.
   * The hint iterator is currently ignored (for std::map compatibility).
   * The arguments are forwarded to construct a value_type (std::pair<Key,
   * Value>). Returns an iterator to the inserted/existing element.
   *
   * Complexity: O(log n)
   */
  template <typename... Args>
  iterator emplace_hint(const_iterator hint, Args&&... args);

  /**
   * Inserts a new element if the key does not exist.
   * Constructs the value in-place using the provided arguments.
   * If the key already exists, does nothing (does not construct the value).
   *
   * This is more efficient than emplace() when the key might already exist,
   * as it avoids constructing the value unnecessarily.
   *
   * @param key The key to insert
   * @param args Arguments to forward to the Value constructor
   * @return Pair of iterator to inserted/existing element and bool indicating
   * success
   *
   * Complexity: O(log n)
   */
  template <typename... Args>
  std::pair<iterator, bool> try_emplace(const Key& key, Args&&... args);

  /**
   * Inserts a new element or assigns to an existing one.
   * If the key exists, assigns the new value to the existing element.
   * If the key doesn't exist, inserts a new element with the value.
   *
   * This is different from insert() which leaves existing values unchanged.
   *
   * @param key The key to insert or assign
   * @param value The value to insert or assign
   * @return Pair of iterator to inserted/updated element and bool indicating
   * insertion (true) vs assignment (false)
   *
   * Complexity: O(log n)
   */
  template <typename M>
  std::pair<iterator, bool> insert_or_assign(const Key& key, M&& value);

  /**
   * Accesses or inserts an element with the specified key.
   * If the key exists, returns a reference to the associated value.
   * If the key does not exist, inserts a new element with default-constructed
   * value and returns a reference to it.
   *
   * Complexity: O(log n)
   */
  Value& operator[](const Key& key);

  /**
   * Returns a reference to the value associated with the specified key.
   * Throws std::out_of_range if the key does not exist.
   *
   * Complexity: O(log n)
   */
  Value& at(const Key& key);

  /**
   * Returns a const reference to the value associated with the specified key.
   * Throws std::out_of_range if the key does not exist.
   *
   * Complexity: O(log n)
   */
  const Value& at(const Key& key) const;

  /**
   * Removes the element with the given key from the tree.
   * Returns the number of elements removed (0 or 1).
   *
   * Handles node underflow by borrowing from siblings or merging nodes.
   *
   * Complexity: O(log n)
   */
  size_type erase(const Key& key);

  /**
   * Removes the element at the given iterator position.
   * Returns an iterator to the element following the erased element.
   *
   * The iterator pos must be valid and dereferenceable (not end()).
   *
   * Note: Due to potential node merges during underflow handling, the returned
   * iterator is reconstructed using find() if the next element is in a
   * different leaf, adding O(log n) overhead.
   *
   * Complexity: O(log n)
   */
  iterator erase(iterator pos);

  /**
   * Removes elements in the range [first, last).
   * Returns an iterator to the element following the last erased element
   * (i.e., returns last).
   *
   * Complexity: O(k * log n) where k is the number of elements erased
   */
  iterator erase(iterator first, iterator last);

  /**
   * Removes all elements from the tree, leaving it empty.
   * Root node remains allocated.
   * All iterators are invalidated.
   *
   * Complexity: O(n)
   */
  void clear();

  /**
   * Returns the number of elements with the specified key.
   * Since btree does not allow duplicates, this returns either 0 or 1.
   *
   * Complexity: O(log n)
   */
  size_type count(const Key& key) const { return find(key) != end() ? 1 : 0; }

  /**
   * Checks if there is an element with the specified key.
   * Equivalent to find(key) != end() but more readable.
   *
   * Complexity: O(log n)
   */
  bool contains(const Key& key) const { return find(key) != end(); }

  /**
   * Swaps the contents of this tree with another tree.
   * All iterators and references remain valid but now refer to elements in the
   * other container.
   *
   * Complexity: O(1)
   */
  void swap(btree& other) noexcept;

 private:
  /**
   * Allocate a new leaf node.
   */
  leaf_node* allocate_leaf_node();

  /**
   * Allocate a new internal node.
   * @param leaf_children If true, children are leaf_node*, otherwise
   * internal_node*
   */
  internal_node* allocate_internal_node(bool leaf_children);

  /**
   * Deallocate a leaf node.
   */
  void deallocate_leaf_node(leaf_node* node);

  /**
   * Deallocate an internal node.
   */
  void deallocate_internal_node(internal_node* node);

  // Comparator instance
  [[no_unique_address]] Compare comp_;

  // Allocators for leaf_node and internal_node
  // Each allocator maintains a separate pool via rebind mechanism
  // This allows different pool sizes for different node types
  // NOTE: Allocators must be declared before root pointers so they can be
  // used in constructor initializer lists (allocate_leaf_node requires them)
  [[no_unique_address]]
  typename std::allocator_traits<Allocator>::template rebind_alloc<leaf_node>
      leaf_alloc_;
  [[no_unique_address]]
  typename std::allocator_traits<Allocator>::template rebind_alloc<
      internal_node> internal_alloc_;

  // Root and size members
  bool root_is_leaf_;
  // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
  // Union for root pointer - avoids std::variant overhead for hot path
  union {
    leaf_node* leaf_root_;
    internal_node* internal_root_;
  };
  // NOLINTEND(cppcoreguidelines-pro-type-union-access)
  size_type size_;

  // Cached pointers to first and last leaf for O(1) begin()/rbegin()
  leaf_node* leftmost_leaf_;
  leaf_node* rightmost_leaf_;

  /**
   * Recursively deallocate all nodes in the tree.
   */
  void deallocate_tree();

  /**
   * Helper to recursively deallocate an internal node and its subtree.
   */
  void deallocate_internal_subtree(internal_node* node);

  /**
   * Helper to get the internal root (asserts if root is a leaf).
   */
  internal_node* get_internal_root() const {
    assert(!root_is_leaf_ && "Root is not an internal node");
    return internal_root_;  // NOLINT(cppcoreguidelines-pro-type-union-access)
  }

  /**
   * Helper to get the leaf root (asserts if root is internal).
   */
  leaf_node* get_leaf_root() const {
    assert(root_is_leaf_ && "Root is not a leaf node");
    return leaf_root_;  // NOLINT(cppcoreguidelines-pro-type-union-access)
  }

  /**
   * Finds the leaf node that should contain the given key.
   * Assumes tree is non-empty.
   *
   * @param key The key to search for
   * @return Pointer to the leaf node that should contain the key
   */
  leaf_node* find_leaf_for_key(const Key& key) const;

  /**
   * Common implementation for all insert operations.
   * Handles leaf finding, key existence check, splitting, and insertion logic.
   *
   * @param key Key to insert
   * @param on_exists Callback when key exists: (leaf_node*, pos) ->
   * pair<iterator, bool> Can modify existing value (insert_or_assign) or just
   * return
   * @param get_value Callback to provide value on-demand: () -> Value
   *                  Called only when inserting (not when key exists)
   * @return Pair of iterator to element and bool indicating insertion success
   */
  template <typename OnExists, typename GetValue>
  std::pair<iterator, bool> insert_impl(const Key& key, OnExists&& on_exists,
                                        GetValue&& get_value);

  /**
   * Split a full leaf node with on-demand value construction.
   * Creates a new leaf and moves half the elements to it.
   * Inserts the new key-value pair into the appropriate half.
   *
   * @param leaf The full leaf node to split (size == LeafNodeSize)
   * @param key The key to insert (guaranteed not to exist in tree)
   * @param get_value Lambda providing value on-demand: () -> Value
   *                  Only called after determining target leaf
   * @return {iterator to inserted element, true}
   *
   * @pre leaf->data.size() == LeafNodeSize
   * @pre key does not exist in the tree (checked by insert_impl before calling)
   * @post Returns {iterator, true} - second element is always true
   */
  template <typename GetValue>
  std::pair<iterator, bool> split_leaf_and_insert(leaf_node* leaf,
                                                  const Key& key,
                                                  GetValue&& get_value);

  /**
   * Insert a promoted key and child pointer into a parent node.
   * If parent is full, splits it recursively.
   * If node has no parent, creates a new root.
   *
   * Templated to handle both leaf_node and internal_node children.
   */
  template <typename NodeType>
  void insert_into_parent(NodeType* left_child, const Key& key,
                          NodeType* right_child);

  /**
   * Split a full internal node.
   * Creates a new internal node and moves half the children to it.
   * Returns a reference to the promoted key and pointer to the new node.
   * The key reference points to the first key in the new node's children.
   */
  std::pair<const Key&, internal_node*> split_internal(internal_node* node);

  /**
   * Update parent's key for a child node to match its current minimum.
   * Recursively updates ancestors if the child is leftmost at each level.
   * Template supports both leaf_node and internal_node children.
   */
  template <typename ChildNodeType>
  void update_parent_key_recursive(ChildNodeType* child, const Key& new_min);

  /**
   * Handle underflow in a node after removal or merge.
   * Attempts to borrow from siblings or merge with them.
   * Returns pointer to the node where the data ended up (may differ if merged).
   * Works for both leaf_node and internal_node.
   *
   * @param node The node that underflowed
   * @param next_index Optional index to track during rebalancing (for erase
   * iterator)
   * @return Pair of {node containing data, iterator to next_index location if
   * valid}
   */
  template <typename NodeType>
  std::pair<NodeType*, std::optional<iterator>> handle_underflow(
      NodeType* node, const std::optional<size_t>& next_index,
      bool next_in_next_leaf);

  /**
   * Try to borrow element(s) or child(ren) from the left sibling of a node.
   * Works for both leaf_node and internal_node.
   * Returns pointer to node if successful, nullptr if borrowing was not
   * possible.
   *
   * @param node The node to borrow into
   * @param next_index Optional index to track during borrowing (O(1) path)
   * @param next_in_next_leaf True if next element is in next leaf
   * @return Pair of {node if successful or nullptr, iterator to next_index if
   * valid}
   */
  template <typename NodeType>
  std::pair<NodeType*, std::optional<iterator>> borrow_from_left_sibling(
      NodeType* node, const std::optional<size_t>& next_index,
      bool next_in_next_leaf);

  /**
   * Try to borrow element(s) or child(ren) from the right sibling of a node.
   * Works for both leaf_node and internal_node.
   * Returns pointer to node if successful, nullptr if borrowing was not
   * possible.
   *
   * @param node The node to borrow into
   * @param next_index Optional index to track during borrowing (O(1) path)
   * @param next_in_next_leaf True if next element is in next leaf
   * @return Pair of {node if successful or nullptr, iterator to next_index if
   * valid}
   */
  template <typename NodeType>
  std::pair<NodeType*, std::optional<iterator>> borrow_from_right_sibling(
      NodeType* node, const std::optional<size_t>& next_index,
      bool next_in_next_leaf);

  /**
   * Merge a node with its left sibling.
   * The left sibling absorbs all elements/children from 'node'.
   * Returns pointer to the left sibling (where data ended up).
   * Works for both leaf_node and internal_node.
   *
   * @param node The node to merge (will be deleted)
   * @param next_index Optional index to track during merging (O(1) path)
   * @param next_in_next_leaf True if next element is in next leaf
   * @return Pair of {left sibling with merged data, iterator to next_index if
   * valid}
   */
  template <typename NodeType>
  std::pair<NodeType*, std::optional<iterator>> merge_with_left_sibling(
      NodeType* node, const std::optional<size_t>& next_index,
      bool next_in_next_leaf);

  /**
   * Merge a node with its right sibling.
   * 'node' absorbs all elements/children from its right sibling.
   * Returns pointer to node (where data ended up).
   * Works for both leaf_node and internal_node.
   *
   * @param node The node that absorbs right sibling's data
   * @param next_index Optional index to track during merging (O(1) path)
   * @param next_in_next_leaf True if next element is in next leaf
   * @return Pair of {node with merged data, iterator to next_index if valid}
   */
  template <typename NodeType>
  std::pair<NodeType*, std::optional<iterator>> merge_with_right_sibling(
      NodeType* node, const std::optional<size_t>& next_index,
      bool next_in_next_leaf);

  /**
   * Template helper to find the left sibling of a node.
   * Works for both leaf_node and internal_node.
   * Returns nullptr if no left sibling exists.
   */
  template <typename NodeType>
  NodeType* find_left_sibling(NodeType* node) const;

  /**
   * Template helper to find the right sibling of a node.
   * Works for both leaf_node and internal_node.
   * Returns nullptr if no right sibling exists.
   */
  template <typename NodeType>
  NodeType* find_right_sibling(NodeType* node) const;

  /**
   * Handle parent node after a merge operation.
   * Checks if parent underflowed and handles recursively.
   * Reduces root height if root has only one child.
   *
   * @param parent The parent node that may have underflowed
   * @param parent_children Reference to parent's children array
   */
  template <typename NodeType, typename ChildrenArray>
  void handle_parent_after_merge(internal_node* parent,
                                 ChildrenArray& parent_children);

  /**
   * Minimum number of elements in a non-root leaf node.
   */
  static constexpr size_type min_leaf_size() { return (LeafNodeSize + 1) / 2; }

  /**
   * Minimum number of children in a non-root internal node.
   */
  static constexpr size_type min_internal_size() {
    return (InternalNodeSize + 1) / 2;
  }

  /**
   * Hysteresis amount for leaf node rebalancing.
   * Don't rebalance until size drops below min_leaf_size() - leaf_hysteresis().
   * When borrowing, try to borrow at least this many elements.
   */
  static constexpr size_type leaf_hysteresis() { return min_leaf_size() / 4; }

  /**
   * Hysteresis amount for internal node rebalancing.
   * Don't rebalance until size drops below min_internal_size() -
   * internal_hysteresis(). When borrowing, try to borrow at least this many
   * children.
   */
  static constexpr size_type internal_hysteresis() {
    return min_internal_size() / 4;
  }
};

}  // namespace kressler::fast_containers

// Include implementation
#include "btree.ipp"
