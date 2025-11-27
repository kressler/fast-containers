#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ordered_array.hpp"

namespace kressler::fast_containers {

/**
 * A B+ Tree that uses ordered_array as the underlying storage for nodes.
 * All data is stored in leaf nodes, which form a doubly-linked list for
 * efficient sequential traversal. Internal nodes store only keys and pointers
 * to guide searches.
 *
 * @tparam Key The key type (must be ComparatorCompatible with Compare)
 * @tparam Value The value type
 * @tparam LeafNodeSize Maximum number of key-value pairs in each leaf node
 * @tparam InternalNodeSize Maximum number of children in each internal node
 * @tparam Compare The comparison function object type (defaults to
 * std::less<Key>)
 * @tparam SearchModeT Search mode passed through to ordered_array
 * @tparam MoveModeT Move mode passed through to ordered_array
 * @tparam Allocator The allocator type (defaults to std::allocator<value_type>)
 */
template <typename Key, typename Value, std::size_t LeafNodeSize = 64,
          std::size_t InternalNodeSize = 64, typename Compare = std::less<Key>,
          SearchMode SearchModeT = SearchMode::Binary,
          MoveMode MoveModeT = MoveMode::SIMD,
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
      "For composite keys, define a struct with operator<=> instead. "
      "For other types, use SearchMode::Binary or SearchMode::Linear.");

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
   * Leaf node - stores actual key-value pairs in an ordered_array.
   * Forms a doubly-linked list with other leaf nodes for iteration.
   */
  // Forward declaration for LeafNode to use in InternalNode
  struct InternalNode;

  struct LeafNode {
    ordered_array<Key, Value, LeafNodeSize, Compare, SearchModeT, MoveModeT>
        data;
    LeafNode* next_leaf;
    LeafNode* prev_leaf;
    InternalNode* parent;  // Parent is always internal (or nullptr for root)

    LeafNode() : next_leaf(nullptr), prev_leaf(nullptr), parent(nullptr) {}
  };

  /**
   * Internal node - stores keys and pointers to child nodes.
   * The children_are_leaves flag indicates whether children are LeafNode* or
   * InternalNode* (all children at the same level have the same type).
   */
  struct InternalNode {
    union {
      ordered_array<Key, LeafNode*, InternalNodeSize, Compare, SearchModeT,
                    MoveModeT>
          leaf_children;
      ordered_array<Key, InternalNode*, InternalNodeSize, Compare, SearchModeT,
                    MoveModeT>
          internal_children;
    };
    bool children_are_leaves;
    InternalNode* parent;  // Parent is always internal (or nullptr for root)

    // Constructor for leaf children
    explicit InternalNode(bool has_leaf_children)
        : children_are_leaves(has_leaf_children), parent(nullptr) {
      if (children_are_leaves) {
        new (&leaf_children) decltype(leaf_children)();
      } else {
        new (&internal_children) decltype(internal_children)();
      }
    }

    // Destructor - must explicitly destroy the active union member
    ~InternalNode() {
      if (children_are_leaves) {
        leaf_children.~ordered_array();
      } else {
        internal_children.~ordered_array();
      }
    }

    // Delete copy constructor and assignment (non-copyable due to union)
    InternalNode(const InternalNode&) = delete;
    InternalNode& operator=(const InternalNode&) = delete;
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
   * Complexity: O(n)
   */
  btree(const btree& other);

  /**
   * Copy assignment operator - replaces contents with a deep copy.
   * Complexity: O(n + m) where n is this tree's size, m is other's size
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
   * Complexity: O(n) where n is this tree's size
   */
  btree& operator=(btree&& other) noexcept;

  /**
   * Returns the number of elements in the tree.
   * Complexity: O(1)
   */
  size_type size() const { return size_; }

  /**
   * Returns true if the tree is empty.
   * Complexity: O(1)
   */
  bool empty() const { return size_ == 0; }

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
    using reference =
        typename ordered_array<Key, Value, LeafNodeSize, Compare, SearchModeT,
                               MoveModeT>::iterator::reference;

    iterator() : leaf_node_(nullptr) {}

    reference operator*() const {
      assert(leaf_node_ != nullptr && "Dereferencing end iterator");
      return *leaf_it_.value();
    }

    typename ordered_array<Key, Value, LeafNodeSize, Compare, SearchModeT,
                           MoveModeT>::iterator::arrow_proxy
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

    iterator(LeafNode* node,
             typename ordered_array<Key, Value, LeafNodeSize, Compare,
                                    SearchModeT, MoveModeT>::iterator it)
        : leaf_node_(node), leaf_it_(it), tree_(nullptr) {}

    iterator(const btree* tree, LeafNode* node,
             typename ordered_array<Key, Value, LeafNodeSize, Compare,
                                    SearchModeT, MoveModeT>::iterator it)
        : leaf_node_(node), leaf_it_(it), tree_(tree) {}

    LeafNode* leaf_node_;
    std::optional<typename ordered_array<Key, Value, LeafNodeSize, Compare,
                                         SearchModeT, MoveModeT>::iterator>
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
    using reference =
        typename ordered_array<Key, Value, LeafNodeSize, Compare, SearchModeT,
                               MoveModeT>::iterator::reference;

    reverse_iterator() : leaf_node_(nullptr), tree_(nullptr) {}

    reference operator*() const {
      assert(leaf_node_ != nullptr && "Dereferencing end iterator");
      return *leaf_it_.value();
    }

    typename ordered_array<Key, Value, LeafNodeSize, Compare, SearchModeT,
                           MoveModeT>::iterator::arrow_proxy
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

    reverse_iterator(
        LeafNode* node,
        typename ordered_array<Key, Value, LeafNodeSize, Compare, SearchModeT,
                               MoveModeT>::iterator it)
        : leaf_node_(node), leaf_it_(it), tree_(nullptr) {}

    reverse_iterator(
        const btree* tree, LeafNode* node,
        typename ordered_array<Key, Value, LeafNodeSize, Compare, SearchModeT,
                               MoveModeT>::iterator it)
        : leaf_node_(node), leaf_it_(it), tree_(tree) {}

    reverse_iterator(const btree* tree, LeafNode* node)
        : leaf_node_(node), leaf_it_(std::nullopt), tree_(tree) {}

    LeafNode* leaf_node_;
    std::optional<typename ordered_array<Key, Value, LeafNodeSize, Compare,
                                         SearchModeT, MoveModeT>::iterator>
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
   * Accesses or inserts an element with the specified key.
   * If the key exists, returns a reference to the associated value.
   * If the key does not exist, inserts a new element with default-constructed
   * value and returns a reference to it.
   *
   * Complexity: O(log n)
   */
  Value& operator[](const Key& key);

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
  LeafNode* allocate_leaf_node();

  /**
   * Allocate a new internal node.
   * @param leaf_children If true, children are LeafNode*, otherwise
   * InternalNode*
   */
  InternalNode* allocate_internal_node(bool leaf_children);

  /**
   * Deallocate a leaf node.
   */
  void deallocate_leaf_node(LeafNode* node);

  /**
   * Deallocate an internal node.
   */
  void deallocate_internal_node(InternalNode* node);

  // Root and size members
  bool root_is_leaf_;
  union {
    LeafNode* leaf_root_;
    InternalNode* internal_root_;
  };
  size_type size_;

  // Cached pointers to first and last leaf for O(1) begin()/rbegin()
  LeafNode* leftmost_leaf_;
  LeafNode* rightmost_leaf_;

  // Comparator instance
  [[no_unique_address]] Compare comp_;

  // Allocators for LeafNode and InternalNode
  // Each allocator maintains a separate pool via rebind mechanism
  // This allows different pool sizes for different node types
  [[no_unique_address]]
  typename std::allocator_traits<Allocator>::template rebind_alloc<LeafNode>
      leaf_alloc_;
  [[no_unique_address]]
  typename std::allocator_traits<Allocator>::template rebind_alloc<InternalNode>
      internal_alloc_;

  /**
   * Recursively deallocate all nodes in the tree.
   */
  void deallocate_tree();

  /**
   * Helper to recursively deallocate an internal node and its subtree.
   */
  void deallocate_internal_subtree(InternalNode* node);

  /**
   * Helper to get the internal root (asserts if root is a leaf).
   */
  InternalNode* get_internal_root() const {
    assert(!root_is_leaf_ && "Root is not an internal node");
    return internal_root_;
  }

  /**
   * Helper to get the leaf root (asserts if root is internal).
   */
  LeafNode* get_leaf_root() const {
    assert(root_is_leaf_ && "Root is not a leaf node");
    return leaf_root_;
  }

  /**
   * Finds the leaf node that should contain the given key.
   * Assumes tree is non-empty.
   */
  LeafNode* find_leaf_for_key(const Key& key) const {
    if (root_is_leaf_) {
      return leaf_root_;
    }

    // Traverse down the tree
    InternalNode* node = internal_root_;
    while (!node->children_are_leaves) {
      // Find the child to follow using optimized lower_bound
      // In a B+ tree, keys represent minimum key in each subtree
      // We want the rightmost child whose minimum <= search key
      auto it = node->internal_children.lower_bound(key);

      // If lower_bound found exact match or went past, back up one
      // (unless we're at the beginning)
      if (it != node->internal_children.begin() &&
          (it == node->internal_children.end() || it->first != key)) {
        --it;
      }

      node = it->second;
    }

    // Now node has leaf children - find the appropriate leaf using lower_bound
    auto it = node->leaf_children.lower_bound(key);

    if (it != node->leaf_children.begin() &&
        (it == node->leaf_children.end() || it->first != key)) {
      --it;
    }

    return it->second;
  }

  /**
   * Split a full leaf node and insert a key-value pair.
   * Creates a new leaf and moves half the elements to it.
   * Returns iterator to the inserted element and true.
   */
  std::pair<iterator, bool> split_leaf(LeafNode* leaf, const Key& key,
                                       const Value& value);

  /**
   * Insert a promoted key and child pointer into a parent node.
   * If parent is full, splits it recursively.
   * If node has no parent, creates a new root.
   *
   * Templated to handle both LeafNode and InternalNode children.
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
  std::pair<const Key&, InternalNode*> split_internal(InternalNode* node);

  /**
   * Update parent's key for a child node to match its current minimum.
   * Recursively updates ancestors if the child is leftmost at each level.
   * Template supports both LeafNode and InternalNode children.
   */
  template <typename ChildNodeType>
  void update_parent_key_recursive(ChildNodeType* child, const Key& new_min);

  /**
   * Handle underflow in a node after removal or merge.
   * Attempts to borrow from siblings or merge with them.
   * Returns pointer to the node where the data ended up (may differ if merged).
   * Works for both LeafNode and InternalNode.
   *
   * @param node The node that underflowed
   * @return Pointer to node containing the data after rebalancing
   */
  template <typename NodeType>
  NodeType* handle_underflow(NodeType* node);

  /**
   * Try to borrow element(s) or child(ren) from the left sibling of a node.
   * Works for both LeafNode and InternalNode.
   * Returns pointer to node if successful, nullptr if borrowing was not
   * possible.
   */
  template <typename NodeType>
  NodeType* borrow_from_left_sibling(NodeType* node);

  /**
   * Try to borrow element(s) or child(ren) from the right sibling of a node.
   * Works for both LeafNode and InternalNode.
   * Returns pointer to node if successful, nullptr if borrowing was not
   * possible.
   */
  template <typename NodeType>
  NodeType* borrow_from_right_sibling(NodeType* node);

  /**
   * Merge a node with its left sibling.
   * The left sibling absorbs all elements/children from 'node'.
   * Returns pointer to the left sibling (where data ended up).
   * Works for both LeafNode and InternalNode.
   *
   * @param node The node to merge (will be deleted)
   * @return Pointer to left sibling containing the merged data
   */
  template <typename NodeType>
  NodeType* merge_with_left_sibling(NodeType* node);

  /**
   * Merge a node with its right sibling.
   * 'node' absorbs all elements/children from its right sibling.
   * Returns pointer to node (where data ended up).
   * Works for both LeafNode and InternalNode.
   *
   * @param node The node that absorbs right sibling's data
   * @return Pointer to node containing the merged data
   */
  template <typename NodeType>
  NodeType* merge_with_right_sibling(NodeType* node);

  /**
   * Template helper to find the left sibling of a node.
   * Works for both LeafNode and InternalNode.
   * Returns nullptr if no left sibling exists.
   */
  template <typename NodeType>
  NodeType* find_left_sibling(NodeType* node) const;

  /**
   * Template helper to find the right sibling of a node.
   * Works for both LeafNode and InternalNode.
   * Returns nullptr if no right sibling exists.
   */
  template <typename NodeType>
  NodeType* find_right_sibling(NodeType* node) const;

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
