#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ordered_array.hpp"

namespace fast_containers {

/**
 * A B+ Tree that uses ordered_array as the underlying storage for nodes.
 * All data is stored in leaf nodes, which form a doubly-linked list for
 * efficient sequential traversal. Internal nodes store only keys and pointers
 * to guide searches.
 *
 * @tparam Key The key type (must be Comparable)
 * @tparam Value The value type
 * @tparam LeafNodeSize Maximum number of key-value pairs in each leaf node
 * @tparam InternalNodeSize Maximum number of children in each internal node
 * @tparam SearchModeT Search mode passed through to ordered_array
 * @tparam MoveModeT Move mode passed through to ordered_array
 */
template <Comparable Key, typename Value, std::size_t LeafNodeSize = 64,
          std::size_t InternalNodeSize = 64,
          SearchMode SearchModeT = SearchMode::Binary,
          MoveMode MoveModeT = MoveMode::SIMD>
class btree {
 public:
  // Type aliases
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<Key, Value>;
  using size_type = std::size_t;

  /**
   * Leaf node - stores actual key-value pairs in an ordered_array.
   * Forms a doubly-linked list with other leaf nodes for iteration.
   */
  // Forward declaration for LeafNode to use in InternalNode
  struct InternalNode;

  struct LeafNode {
    ordered_array<Key, Value, LeafNodeSize, SearchModeT, MoveModeT> data;
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
      ordered_array<Key, LeafNode*, InternalNodeSize, SearchModeT, MoveModeT>
          leaf_children;
      ordered_array<Key, InternalNode*, InternalNodeSize, SearchModeT,
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
   */
  btree();

  /**
   * Destructor - deallocates all nodes.
   */
  ~btree();

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
   * Forward iterator for btree.
   * Iterates through elements in sorted order using the leaf node chain.
   */
  class iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = btree::value_type;
    using pointer = value_type*;
    using reference =
        typename ordered_array<Key, Value, LeafNodeSize, SearchModeT,
                               MoveModeT>::iterator::reference;

    iterator() : leaf_node_(nullptr) {}

    reference operator*() const {
      assert(leaf_node_ != nullptr && "Dereferencing end iterator");
      return *leaf_it_.value();
    }

    typename ordered_array<Key, Value, LeafNodeSize, SearchModeT,
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
        leaf_node_ = leaf_node_->next_leaf;
        if (leaf_node_ != nullptr) {
          leaf_it_ = leaf_node_->data.begin();
        } else {
          leaf_it_.reset();
        }
      }
      return *this;
    }

    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
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
             typename ordered_array<Key, Value, LeafNodeSize, SearchModeT,
                                    MoveModeT>::iterator it)
        : leaf_node_(node), leaf_it_(it) {}

    LeafNode* leaf_node_;
    std::optional<typename ordered_array<Key, Value, LeafNodeSize, SearchModeT,
                                         MoveModeT>::iterator>
        leaf_it_;
  };

  using const_iterator = iterator;  // For now, treat as const

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
  iterator end() { return iterator(); }

  /**
   * Finds an element with the given key.
   * Returns an iterator to the element if found, end() otherwise.
   * Complexity: O(log n)
   */
  iterator find(const Key& key) {
    if (size_ == 0) {
      return end();
    }

    // Find the leaf that should contain the key
    LeafNode* leaf = find_leaf_for_key(key);

    // Search within the leaf
    auto it = leaf->data.find(key);
    if (it != leaf->data.end()) {
      return iterator(leaf, it);
    }
    return end();
  }

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
   * Removes the element with the given key from the tree.
   * Returns the number of elements removed (0 or 1).
   *
   * Handles node underflow by borrowing from siblings or merging nodes.
   *
   * Complexity: O(log n)
   */
  size_type erase(const Key& key);

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
      // Find the child to follow
      // In a B+ tree, we use lower_bound to find the right subtree
      // The key in internal nodes represents the minimum key in the subtree
      auto it = node->internal_children.begin();
      auto end = node->internal_children.end();

      // Find first child whose key is > search key
      while (it != end && it->first < key) {
        ++it;
      }

      // If we found such a child and it's not the first, go to previous
      if (it != node->internal_children.begin() &&
          (it == end || it->first != key)) {
        --it;
      }

      node = it->second;
    }

    // Now node has leaf children - find the appropriate leaf
    auto it = node->leaf_children.begin();
    auto end = node->leaf_children.end();

    while (it != end && it->first < key) {
      ++it;
    }

    if (it != node->leaf_children.begin() && (it == end || it->first != key)) {
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
   * Handle underflow in a leaf node after removal.
   * Attempts to borrow from siblings or merge with them.
   */
  void handle_leaf_underflow(LeafNode* leaf);

  /**
   * Handle underflow in an internal node after removal/merge.
   * Attempts to borrow from siblings or merge with them.
   */
  void handle_internal_underflow(InternalNode* node);

  /**
   * Try to borrow an element from the left sibling of a leaf.
   * Returns true if successful.
   */
  bool borrow_from_left_leaf_sibling(LeafNode* leaf);

  /**
   * Try to borrow an element from the right sibling of a leaf.
   * Returns true if successful.
   */
  bool borrow_from_right_leaf_sibling(LeafNode* leaf);

  /**
   * Merge a leaf with its left sibling.
   * The left sibling absorbs all elements from 'leaf'.
   */
  void merge_with_left_leaf_sibling(LeafNode* leaf);

  /**
   * Merge a leaf with its right sibling.
   * 'leaf' absorbs all elements from its right sibling.
   */
  void merge_with_right_leaf_sibling(LeafNode* leaf);

  /**
   * Try to borrow a child from the left sibling of an internal node.
   * Returns true if successful.
   */
  bool borrow_from_left_internal_sibling(InternalNode* node);

  /**
   * Try to borrow a child from the right sibling of an internal node.
   * Returns true if successful.
   */
  bool borrow_from_right_internal_sibling(InternalNode* node);

  /**
   * Merge an internal node with its left sibling.
   * The left sibling absorbs all children from 'node'.
   */
  void merge_with_left_internal_sibling(InternalNode* node);

  /**
   * Merge an internal node with its right sibling.
   * 'node' absorbs all children from its right sibling.
   */
  void merge_with_right_internal_sibling(InternalNode* node);

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

// Implementation

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::btree()
    : root_is_leaf_(true),
      leaf_root_(nullptr),
      size_(0),
      leftmost_leaf_(nullptr),
      rightmost_leaf_(nullptr) {}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::~btree() {
  deallocate_tree();
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
               MoveModeT>::LeafNode*
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::allocate_leaf_node() {
  return new LeafNode();
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
               MoveModeT>::InternalNode*
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::allocate_internal_node(bool leaf_children) {
  return new InternalNode(leaf_children);
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::deallocate_leaf_node(LeafNode* node) {
  delete node;
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::deallocate_internal_node(InternalNode* node) {
  delete node;
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::deallocate_tree() {
  if (size_ == 0) {
    return;  // Empty tree
  }

  if (root_is_leaf_) {
    // Tree has only one node (leaf root)
    deallocate_leaf_node(leaf_root_);
  } else {
    // Tree has internal nodes - use recursive deallocation
    deallocate_internal_subtree(internal_root_);
  }

  // Reset state
  root_is_leaf_ = true;
  leaf_root_ = nullptr;
  size_ = 0;
  leftmost_leaf_ = nullptr;
  rightmost_leaf_ = nullptr;
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::deallocate_internal_subtree(InternalNode* node) {
  if (node->children_are_leaves) {
    // Children are leaves - deallocate them
    for (auto it = node->leaf_children.begin(); it != node->leaf_children.end();
         ++it) {
      deallocate_leaf_node(it->second);
    }
  } else {
    // Children are internal nodes - recurse
    for (auto it = node->internal_children.begin();
         it != node->internal_children.end(); ++it) {
      deallocate_internal_subtree(it->second);
    }
  }
  // Deallocate this internal node
  deallocate_internal_node(node);
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
std::pair<typename btree<Key, Value, LeafNodeSize, InternalNodeSize,
                         SearchModeT, MoveModeT>::iterator,
          bool>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::insert(const Key& key, const Value& value) {
  // Case 1: Empty tree - create first leaf
  if (size_ == 0) {
    LeafNode* leaf = allocate_leaf_node();
    auto [leaf_it, inserted] = leaf->data.insert(key, value);
    assert(inserted && "First insert into empty leaf should always succeed");

    root_is_leaf_ = true;
    leaf_root_ = leaf;
    leftmost_leaf_ = leaf;
    rightmost_leaf_ = leaf;
    size_ = 1;

    return {iterator(leaf, leaf_it), true};
  }

  // Case 2: Tree has elements - find appropriate leaf
  LeafNode* leaf = find_leaf_for_key(key);

  // If leaf is full, split it
  if (leaf->data.size() >= LeafNodeSize) {
    return split_leaf(leaf, key, value);
  }

  // Leaf has space - insert normally
  // Track if this will become the new minimum
  bool will_be_new_min = leaf->data.empty() || key < leaf->data.begin()->first;

  auto [leaf_it, inserted] = leaf->data.insert(key, value);
  if (inserted) {
    size_++;

    // If we inserted a new minimum and leaf has a parent, update parent key
    if (will_be_new_min && leaf->parent != nullptr) {
      update_parent_key_recursive(leaf, key);
    }

    // Update leftmost_leaf_ if necessary
    if (leftmost_leaf_ == nullptr ||
        key < leftmost_leaf_->data.begin()->first) {
      leftmost_leaf_ = leaf;
    }
  }

  return {iterator(leaf, leaf_it), inserted};
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
std::pair<typename btree<Key, Value, LeafNodeSize, InternalNodeSize,
                         SearchModeT, MoveModeT>::iterator,
          bool>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::split_leaf(LeafNode* leaf, const Key& key,
                             const Value& value) {
  // Create new leaf for right half
  LeafNode* new_leaf = allocate_leaf_node();

  // Calculate split point (ceil(LeafNodeSize / 2))
  size_type split_point = (LeafNodeSize + 1) / 2;

  // Use split_at() to efficiently move second half to new leaf
  auto split_it = leaf->data.begin();
  std::advance(split_it, split_point);
  leaf->data.split_at(split_it, new_leaf->data);

  // Update leaf chain pointers
  new_leaf->next_leaf = leaf->next_leaf;
  new_leaf->prev_leaf = leaf;
  if (leaf->next_leaf) {
    leaf->next_leaf->prev_leaf = new_leaf;
  }
  leaf->next_leaf = new_leaf;

  // Update rightmost_leaf if necessary
  if (rightmost_leaf_ == leaf) {
    rightmost_leaf_ = new_leaf;
  }

  // Get first key of new leaf for promotion
  const Key& promoted_key = new_leaf->data.begin()->first;

  // Insert the new key-value pair into appropriate leaf
  LeafNode* target_leaf = (key < promoted_key) ? leaf : new_leaf;
  auto [leaf_it, inserted] = target_leaf->data.insert(key, value);

  // Handle duplicate key case
  if (!inserted) {
    return {iterator(target_leaf, leaf_it), false};
  }

  size_++;

  // Insert promoted key into parent
  insert_into_parent(leaf, promoted_key, new_leaf);

  // If we inserted into the left half, update parent key if necessary
  // Note: Parent keys can be stale from previous non-split inserts
  if (target_leaf == leaf) {
    const Key& new_left_min = leaf->data.begin()->first;
    update_parent_key_recursive(leaf, new_left_min);
  }

  // Update leftmost_leaf_ if the leaf became the new leftmost
  if (target_leaf == leaf &&
      (leftmost_leaf_ == nullptr ||
       leaf->data.begin()->first < leftmost_leaf_->data.begin()->first)) {
    leftmost_leaf_ = leaf;
  }

  return {iterator(target_leaf, leaf_it), true};
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
template <typename NodeType>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::insert_into_parent(NodeType* left_child, const Key& key,
                                          NodeType* right_child) {
  // Helper lambda to get appropriate children array reference
  auto get_children = [](InternalNode* node) -> auto& {
    if constexpr (std::is_same_v<NodeType, LeafNode>) {
      return node->leaf_children;
    } else {
      return node->internal_children;
    }
  };

  // Helper lambda to get the left key for creating new root
  auto get_left_key = [](NodeType* node) -> const Key& {
    if constexpr (std::is_same_v<NodeType, LeafNode>) {
      return node->data.begin()->first;
    } else {
      if (node->children_are_leaves) {
        return node->leaf_children.begin()->first;
      } else {
        return node->internal_children.begin()->first;
      }
    }
  };

  // Case 1: left_child is root - create new root
  if (left_child->parent == nullptr) {
    constexpr bool children_are_leaves = std::is_same_v<NodeType, LeafNode>;
    InternalNode* new_root = allocate_internal_node(children_are_leaves);

    auto& children = get_children(new_root);
    auto [it, inserted] = children.insert(get_left_key(left_child), left_child);
    assert(inserted);
    auto [it2, inserted2] = children.insert(key, right_child);
    assert(inserted2);

    left_child->parent = new_root;
    right_child->parent = new_root;

    root_is_leaf_ = false;
    internal_root_ = new_root;
    return;
  }

  // Case 2: Parent exists - insert into it
  InternalNode* parent = left_child->parent;
  auto& parent_children = get_children(parent);

  // Check if parent is full
  if (parent_children.size() >= InternalNodeSize) {
    // Need to split parent
    auto [promoted_key, new_parent] = split_internal(parent);

    // Determine which parent to insert into
    InternalNode* target_parent = (key < promoted_key) ? parent : new_parent;
    right_child->parent = target_parent;

    auto& target_children = get_children(target_parent);
    auto [it, inserted] = target_children.insert(key, right_child);
    assert(inserted && "Insert into non-full parent should succeed");

    // Recursively insert promoted key into grandparent
    insert_into_parent(parent, promoted_key, new_parent);
  } else {
    // Parent has space - insert normally
    right_child->parent = parent;
    auto [it, inserted] = parent_children.insert(key, right_child);
    assert(inserted && "Insert into non-full parent should succeed");
  }
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
std::pair<const Key&, typename btree<Key, Value, LeafNodeSize, InternalNodeSize,
                                     SearchModeT, MoveModeT>::InternalNode*>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::split_internal(InternalNode* node) {
  // Create new internal node
  InternalNode* new_node = allocate_internal_node(node->children_are_leaves);

  // Calculate split point
  size_type split_point = (InternalNodeSize + 1) / 2;

  if (node->children_are_leaves) {
    // Get split iterator
    auto split_it = node->leaf_children.begin();
    std::advance(split_it, split_point);

    // Use split_at() to efficiently move second half to new node
    node->leaf_children.split_at(split_it, new_node->leaf_children);

    // Update parent pointers for moved children
    for (auto it = new_node->leaf_children.begin();
         it != new_node->leaf_children.end(); ++it) {
      it->second->parent = new_node;
    }

    // Return reference to first key in new node (the promoted key)
    return {new_node->leaf_children.begin()->first, new_node};
  } else {
    // Get split iterator
    auto split_it = node->internal_children.begin();
    std::advance(split_it, split_point);

    // Use split_at() to efficiently move second half to new node
    node->internal_children.split_at(split_it, new_node->internal_children);

    // Update parent pointers for moved children
    for (auto it = new_node->internal_children.begin();
         it != new_node->internal_children.end(); ++it) {
      it->second->parent = new_node;
    }

    // Return reference to first key in new node (the promoted key)
    return {new_node->internal_children.begin()->first, new_node};
  }
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
template <typename ChildNodeType>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::update_parent_key_recursive(ChildNodeType* child,
                                                   const Key& new_min) {
  if (child->parent == nullptr) {
    return;
  }

  InternalNode* parent = child->parent;

  // Get reference to appropriate children array based on child type
  auto& children = [&]() -> auto& {
    if constexpr (std::is_same_v<ChildNodeType, LeafNode>) {
      return parent->leaf_children;
    } else {
      return parent->internal_children;
    }
  }();

  // Use lower_bound to efficiently find the entry (O(log n))
  auto it = children.lower_bound(new_min);

  // Track if we're at position 0 (leftmost child)
  bool is_leftmost = false;

  // The entry pointing to this child should be at 'it' or one position before
  // Check 'it' first
  if (it != children.end() && it->second == child) {
    // Found it at lower_bound position
    is_leftmost = (it == children.begin());
    if (it->first != new_min) {
      children.remove(it->first);
      auto [new_it, ins] = children.insert(new_min, child);
      assert(ins && "Re-inserting child with new minimum key should succeed");
    }
  } else if (it != children.begin()) {
    // Check the previous entry
    --it;
    if (it->second == child && it->first != new_min) {
      is_leftmost = (it == children.begin());
      children.remove(it->first);
      auto [new_it, ins] = children.insert(new_min, child);
      assert(ins && "Re-inserting child with new minimum key should succeed");
    }
  }

  // If this child is leftmost in parent, recursively update grandparent
  // The parent's minimum key changed, so we need to update it too
  if (is_leftmost && parent->parent != nullptr) {
    update_parent_key_recursive(parent, new_min);
  }
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
               MoveModeT>::size_type
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::erase(const Key& key) {
  // Empty tree - nothing to remove
  if (size_ == 0) {
    return 0;
  }

  // Find the leaf containing the key
  LeafNode* leaf = find_leaf_for_key(key);

  // Check if key exists in the leaf
  auto it = leaf->data.find(key);
  if (it == leaf->data.end()) {
    return 0;  // Key not found
  }

  // Remove from leaf
  leaf->data.remove(key);
  size_--;

  // Special case: tree is now empty
  if (size_ == 0) {
    // Tree had only one element in root leaf
    assert(root_is_leaf_ && "Empty tree should have leaf root");
    deallocate_leaf_node(leaf_root_);
    leaf_root_ = nullptr;
    leftmost_leaf_ = nullptr;
    rightmost_leaf_ = nullptr;
    return 1;
  }

  // Update leftmost_leaf_ if we removed from it and it's now empty
  if (leftmost_leaf_ == leaf && leaf->data.empty()) {
    leftmost_leaf_ = leaf->next_leaf;
  }

  // Update rightmost_leaf_ if we removed from it and it's now empty
  if (rightmost_leaf_ == leaf && leaf->data.empty()) {
    rightmost_leaf_ = leaf->prev_leaf;
  }

  // Check if leaf underflowed (but root can have any size)
  // Use hysteresis: only rebalance when size drops below min - hysteresis
  bool is_root = (root_is_leaf_ && leaf == leaf_root_);
  size_type underflow_threshold = min_leaf_size() > leaf_hysteresis()
                                      ? min_leaf_size() - leaf_hysteresis()
                                      : 0;
  if (!is_root && leaf->data.size() < underflow_threshold) {
    handle_leaf_underflow(leaf);
  } else if (!leaf->data.empty() && leaf->parent != nullptr) {
    // No underflow, but check if minimum key changed
    const Key& new_min = leaf->data.begin()->first;
    // Only need to update parent if this leaf is still around
    // (might have been merged away in underflow handling)
    update_parent_key_recursive(leaf, new_min);
  }

  return 1;
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
template <typename NodeType>
NodeType* btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
                MoveModeT>::find_left_sibling(NodeType* node) const {
  if (node->parent == nullptr) {
    return nullptr;  // Root has no siblings
  }

  InternalNode* parent = node->parent;

  // Get node's minimum key and children array reference
  const Key* node_min = nullptr;

  if constexpr (std::is_same_v<NodeType, LeafNode>) {
    if (node->data.empty()) {
      return nullptr;  // Empty leaf, can't find it
    }
    node_min = &(node->data.begin()->first);
    auto& children = parent->leaf_children;

    // Use lower_bound to efficiently find this node's position
    auto it = children.lower_bound(*node_min);

    // The entry pointing to this node should be at 'it' or one position before
    if (it != children.end() && it->second == node) {
      if (it == children.begin()) {
        return nullptr;  // This is the leftmost child
      }
      auto prev_it = it;
      --prev_it;
      return prev_it->second;
    } else if (it != children.begin()) {
      --it;
      if (it->second == node) {
        if (it == children.begin()) {
          return nullptr;  // This is the leftmost child
        }
        auto prev_it = it;
        --prev_it;
        return prev_it->second;
      }
    }
  } else {
    // InternalNode
    if (node->children_are_leaves) {
      if (node->leaf_children.empty()) {
        return nullptr;  // Empty node, can't find it
      }
      node_min = &(node->leaf_children.begin()->first);
    } else {
      if (node->internal_children.empty()) {
        return nullptr;  // Empty node, can't find it
      }
      node_min = &(node->internal_children.begin()->first);
    }

    auto& children = parent->internal_children;

    // Use lower_bound to efficiently find this node's position
    auto it = children.lower_bound(*node_min);

    // The entry pointing to this node should be at 'it' or one position before
    if (it != children.end() && it->second == node) {
      if (it == children.begin()) {
        return nullptr;  // This is the leftmost child
      }
      auto prev_it = it;
      --prev_it;
      return prev_it->second;
    } else if (it != children.begin()) {
      --it;
      if (it->second == node) {
        if (it == children.begin()) {
          return nullptr;  // This is the leftmost child
        }
        auto prev_it = it;
        --prev_it;
        return prev_it->second;
      }
    }
  }

  assert(false && "Node not found in parent's children");
  return nullptr;
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
template <typename NodeType>
NodeType* btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
                MoveModeT>::find_right_sibling(NodeType* node) const {
  if (node->parent == nullptr) {
    return nullptr;  // Root has no siblings
  }

  InternalNode* parent = node->parent;

  // Get node's minimum key and children array reference
  const Key* node_min = nullptr;

  if constexpr (std::is_same_v<NodeType, LeafNode>) {
    if (node->data.empty()) {
      return nullptr;  // Empty leaf, can't find it
    }
    node_min = &(node->data.begin()->first);
    auto& children = parent->leaf_children;

    // Use lower_bound to efficiently find this node's position
    auto it = children.lower_bound(*node_min);

    // The entry pointing to this node should be at 'it' or one position before
    if (it != children.end() && it->second == node) {
      auto next_it = it;
      ++next_it;
      if (next_it == children.end()) {
        return nullptr;  // This is the rightmost child
      }
      return next_it->second;
    } else if (it != children.begin()) {
      --it;
      if (it->second == node) {
        auto next_it = it;
        ++next_it;
        if (next_it == children.end()) {
          return nullptr;  // This is the rightmost child
        }
        return next_it->second;
      }
    }
  } else {
    // InternalNode
    if (node->children_are_leaves) {
      if (node->leaf_children.empty()) {
        return nullptr;  // Empty node, can't find it
      }
      node_min = &(node->leaf_children.begin()->first);
    } else {
      if (node->internal_children.empty()) {
        return nullptr;  // Empty node, can't find it
      }
      node_min = &(node->internal_children.begin()->first);
    }

    auto& children = parent->internal_children;

    // Use lower_bound to efficiently find this node's position
    auto it = children.lower_bound(*node_min);

    // The entry pointing to this node should be at 'it' or one position before
    if (it != children.end() && it->second == node) {
      auto next_it = it;
      ++next_it;
      if (next_it == children.end()) {
        return nullptr;  // This is the rightmost child
      }
      return next_it->second;
    } else if (it != children.begin()) {
      --it;
      if (it->second == node) {
        auto next_it = it;
        ++next_it;
        if (next_it == children.end()) {
          return nullptr;  // This is the rightmost child
        }
        return next_it->second;
      }
    }
  }

  assert(false && "Node not found in parent's children");
  return nullptr;
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
bool btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::borrow_from_left_leaf_sibling(LeafNode* leaf) {
  LeafNode* left_sibling = find_left_sibling(leaf);

  // Can't borrow if no left sibling or sibling would underflow
  if (left_sibling == nullptr || left_sibling->data.size() <= min_leaf_size()) {
    return false;
  }

  // Try to borrow at least leaf_hysteresis() elements for efficiency
  // But don't leave sibling below min_leaf_size()
  size_type target_borrow = leaf_hysteresis() > 0 ? leaf_hysteresis() : 1;
  size_type can_borrow = left_sibling->data.size() > min_leaf_size()
                             ? left_sibling->data.size() - min_leaf_size()
                             : 0;
  size_type actual_borrow = std::min(target_borrow, can_borrow);

  if (actual_borrow == 0) {
    return false;
  }

  // Transfer suffix from left sibling to beginning of this leaf
  // This does a bulk copy and shifts arrays only once
  leaf->data.transfer_suffix_from(left_sibling->data, actual_borrow);

  // Update parent key for this leaf (minimum changed to first element)
  const Key& new_min = leaf->data.begin()->first;
  update_parent_key_recursive(leaf, new_min);

  return true;
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
bool btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::borrow_from_right_leaf_sibling(LeafNode* leaf) {
  LeafNode* right_sibling = find_right_sibling(leaf);

  // Can't borrow if no right sibling or sibling would underflow
  if (right_sibling == nullptr ||
      right_sibling->data.size() <= min_leaf_size()) {
    return false;
  }

  // Try to borrow at least leaf_hysteresis() elements for efficiency
  // But don't leave sibling below min_leaf_size()
  size_type target_borrow = leaf_hysteresis() > 0 ? leaf_hysteresis() : 1;
  size_type can_borrow = right_sibling->data.size() > min_leaf_size()
                             ? right_sibling->data.size() - min_leaf_size()
                             : 0;
  size_type actual_borrow = std::min(target_borrow, can_borrow);

  if (actual_borrow == 0) {
    return false;
  }

  // Transfer prefix from right sibling to end of this leaf
  // This does a bulk copy and shifts arrays only once
  leaf->data.transfer_prefix_from(right_sibling->data, actual_borrow);

  // Update parent key for right sibling (its minimum changed)
  const Key& new_right_min = right_sibling->data.begin()->first;
  update_parent_key_recursive(right_sibling, new_right_min);

  return true;
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::merge_with_left_leaf_sibling(LeafNode* leaf) {
  LeafNode* left_sibling = find_left_sibling(leaf);
  assert(left_sibling != nullptr &&
         "merge_with_left_leaf_sibling requires left sibling");

  // Move all elements from this leaf to left sibling
  for (auto it = leaf->data.begin(); it != leaf->data.end(); ++it) {
    auto [inserted_it, inserted] =
        left_sibling->data.insert(it->first, it->second);
    assert(inserted && "Merging should not have duplicates");
  }

  // Update leaf chain pointers
  left_sibling->next_leaf = leaf->next_leaf;
  if (leaf->next_leaf) {
    leaf->next_leaf->prev_leaf = left_sibling;
  }

  // Update rightmost_leaf_ if necessary
  if (rightmost_leaf_ == leaf) {
    rightmost_leaf_ = left_sibling;
  }

  // Remove this leaf from parent
  InternalNode* parent = leaf->parent;
  assert(parent != nullptr && "Merging non-root leaf should have parent");

  // Find and remove the entry for this leaf in parent
  auto& parent_children = parent->leaf_children;
  for (auto it = parent_children.begin(); it != parent_children.end(); ++it) {
    if (it->second == leaf) {
      parent_children.remove(it->first);
      break;
    }
  }

  // Deallocate this leaf
  deallocate_leaf_node(leaf);

  // Check if parent underflowed (use hysteresis threshold)
  bool parent_is_root = (parent == internal_root_ && !root_is_leaf_);
  size_type parent_underflow_threshold =
      min_internal_size() > internal_hysteresis()
          ? min_internal_size() - internal_hysteresis()
          : 0;
  if (!parent_is_root && parent_children.size() < parent_underflow_threshold) {
    handle_internal_underflow(parent);
  } else if (parent_is_root && parent_children.size() == 1) {
    // Root has only one child left - make that child the new root
    LeafNode* new_root = parent_children.begin()->second;
    new_root->parent = nullptr;
    root_is_leaf_ = true;
    leaf_root_ = new_root;
    deallocate_internal_node(parent);
  }
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::merge_with_right_leaf_sibling(LeafNode* leaf) {
  LeafNode* right_sibling = find_right_sibling(leaf);
  assert(right_sibling != nullptr &&
         "merge_with_right_leaf_sibling requires right sibling");

  // Move all elements from right sibling to this leaf
  for (auto it = right_sibling->data.begin(); it != right_sibling->data.end();
       ++it) {
    auto [inserted_it, inserted] = leaf->data.insert(it->first, it->second);
    assert(inserted && "Merging should not have duplicates");
  }

  // Update leaf chain pointers
  leaf->next_leaf = right_sibling->next_leaf;
  if (right_sibling->next_leaf) {
    right_sibling->next_leaf->prev_leaf = leaf;
  }

  // Update rightmost_leaf_ if necessary
  if (rightmost_leaf_ == right_sibling) {
    rightmost_leaf_ = leaf;
  }

  // Remove right sibling from parent
  InternalNode* parent = leaf->parent;
  assert(parent != nullptr && "Merging non-root leaf should have parent");

  // Find and remove the entry for right sibling in parent
  auto& parent_children = parent->leaf_children;
  for (auto it = parent_children.begin(); it != parent_children.end(); ++it) {
    if (it->second == right_sibling) {
      parent_children.remove(it->first);
      break;
    }
  }

  // Deallocate right sibling
  deallocate_leaf_node(right_sibling);

  // Check if parent underflowed (use hysteresis threshold)
  bool parent_is_root = (parent == internal_root_ && !root_is_leaf_);
  size_type parent_underflow_threshold =
      min_internal_size() > internal_hysteresis()
          ? min_internal_size() - internal_hysteresis()
          : 0;
  if (!parent_is_root && parent_children.size() < parent_underflow_threshold) {
    handle_internal_underflow(parent);
  } else if (parent_is_root && parent_children.size() == 1) {
    // Root has only one child left - make that child the new root
    LeafNode* new_root = parent_children.begin()->second;
    new_root->parent = nullptr;
    root_is_leaf_ = true;
    leaf_root_ = new_root;
    deallocate_internal_node(parent);
  }
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::handle_leaf_underflow(LeafNode* leaf) {
  size_type underflow_threshold = min_leaf_size() > leaf_hysteresis()
                                      ? min_leaf_size() - leaf_hysteresis()
                                      : 0;
  assert(leaf->data.size() < underflow_threshold &&
         "handle_leaf_underflow called on non-underflowed leaf");
  assert(leaf->parent != nullptr && "Root leaf cannot underflow");

  // Try to borrow from left sibling first
  if (borrow_from_left_leaf_sibling(leaf)) {
    return;
  }

  // Try to borrow from right sibling
  if (borrow_from_right_leaf_sibling(leaf)) {
    return;
  }

  // Can't borrow - must merge
  // Prefer merging with left sibling if it exists
  LeafNode* left_sibling = find_left_sibling(leaf);
  if (left_sibling != nullptr) {
    merge_with_left_leaf_sibling(leaf);
  } else {
    // No left sibling - merge with right
    merge_with_right_leaf_sibling(leaf);
  }
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
bool btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::borrow_from_left_internal_sibling(InternalNode* node) {
  InternalNode* left_sibling = find_left_sibling(node);

  // Can't borrow if no left sibling or sibling would underflow
  if (left_sibling == nullptr) {
    return false;
  }

  if (node->children_are_leaves) {
    auto& left_children = left_sibling->leaf_children;

    // Try to borrow at least internal_hysteresis() children for efficiency
    // But don't leave sibling below min_internal_size()
    size_type target_borrow =
        internal_hysteresis() > 0 ? internal_hysteresis() : 1;
    size_type can_borrow = left_children.size() > min_internal_size()
                               ? left_children.size() - min_internal_size()
                               : 0;
    size_type actual_borrow = std::min(target_borrow, can_borrow);

    if (actual_borrow == 0) {
      return false;
    }

    // Transfer suffix from left sibling to beginning of this node
    // This does a bulk copy and shifts arrays only once
    auto& children = node->leaf_children;
    children.transfer_suffix_from(left_children, actual_borrow);

    // Update parent pointers for all transferred children (from beginning)
    auto it = children.begin();
    for (size_type i = 0; i < actual_borrow; ++i, ++it) {
      it->second->parent = node;
    }

    // Update parent key for this node (minimum changed to first child's key)
    const Key& new_min = children.begin()->first;
    update_parent_key_recursive(node, new_min);

    return true;
  } else {
    auto& left_children = left_sibling->internal_children;

    // Try to borrow at least internal_hysteresis() children for efficiency
    // But don't leave sibling below min_internal_size()
    size_type target_borrow =
        internal_hysteresis() > 0 ? internal_hysteresis() : 1;
    size_type can_borrow = left_children.size() > min_internal_size()
                               ? left_children.size() - min_internal_size()
                               : 0;
    size_type actual_borrow = std::min(target_borrow, can_borrow);

    if (actual_borrow == 0) {
      return false;
    }

    // Transfer suffix from left sibling to beginning of this node
    // This does a bulk copy and shifts arrays only once
    auto& children = node->internal_children;
    children.transfer_suffix_from(left_children, actual_borrow);

    // Update parent pointers for all transferred children (from beginning)
    auto it = children.begin();
    for (size_type i = 0; i < actual_borrow; ++i, ++it) {
      it->second->parent = node;
    }

    // Update parent key for this node (minimum changed to first child's key)
    const Key& new_min = children.begin()->first;
    update_parent_key_recursive(node, new_min);

    return true;
  }
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
bool btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::borrow_from_right_internal_sibling(InternalNode* node) {
  InternalNode* right_sibling = find_right_sibling(node);

  // Can't borrow if no right sibling or sibling would underflow
  if (right_sibling == nullptr) {
    return false;
  }

  if (node->children_are_leaves) {
    auto& right_children = right_sibling->leaf_children;

    // Try to borrow at least internal_hysteresis() children for efficiency
    // But don't leave sibling below min_internal_size()
    size_type target_borrow =
        internal_hysteresis() > 0 ? internal_hysteresis() : 1;
    size_type can_borrow = right_children.size() > min_internal_size()
                               ? right_children.size() - min_internal_size()
                               : 0;
    size_type actual_borrow = std::min(target_borrow, can_borrow);

    if (actual_borrow == 0) {
      return false;
    }

    // Transfer prefix from right sibling to end of this node
    // This does a bulk copy and shifts arrays only once
    auto& children = node->leaf_children;
    size_type old_size = children.size();
    children.transfer_prefix_from(right_children, actual_borrow);

    // Update parent pointers for all transferred children (from old_size)
    auto it = children.begin();
    std::advance(it, old_size);
    for (size_type i = 0; i < actual_borrow; ++i, ++it) {
      it->second->parent = node;
    }

    // Update parent key for right sibling (its minimum changed to first
    // remaining child)
    const Key& new_right_min = right_children.begin()->first;
    update_parent_key_recursive(right_sibling, new_right_min);

    return true;
  } else {
    auto& right_children = right_sibling->internal_children;

    // Try to borrow at least internal_hysteresis() children for efficiency
    // But don't leave sibling below min_internal_size()
    size_type target_borrow =
        internal_hysteresis() > 0 ? internal_hysteresis() : 1;
    size_type can_borrow = right_children.size() > min_internal_size()
                               ? right_children.size() - min_internal_size()
                               : 0;
    size_type actual_borrow = std::min(target_borrow, can_borrow);

    if (actual_borrow == 0) {
      return false;
    }

    // Transfer prefix from right sibling to end of this node
    // This does a bulk copy and shifts arrays only once
    auto& children = node->internal_children;
    size_type old_size = children.size();
    children.transfer_prefix_from(right_children, actual_borrow);

    // Update parent pointers for all transferred children (from old_size)
    auto it = children.begin();
    std::advance(it, old_size);
    for (size_type i = 0; i < actual_borrow; ++i, ++it) {
      it->second->parent = node;
    }

    // Update parent key for right sibling (its minimum changed to first
    // remaining child)
    const Key& new_right_min = right_children.begin()->first;
    update_parent_key_recursive(right_sibling, new_right_min);

    return true;
  }
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::merge_with_left_internal_sibling(InternalNode* node) {
  InternalNode* left_sibling = find_left_sibling(node);
  assert(left_sibling != nullptr &&
         "merge_with_left_internal_sibling requires left sibling");

  if (node->children_are_leaves) {
    // Move all children from this node to left sibling
    auto& children = node->leaf_children;
    auto& left_children = left_sibling->leaf_children;

    for (auto it = children.begin(); it != children.end(); ++it) {
      auto [inserted_it, inserted] =
          left_children.insert(it->first, it->second);
      assert(inserted && "Merging should not have duplicates");
      // Update parent pointer
      it->second->parent = left_sibling;
    }
  } else {
    // Move all children from this node to left sibling
    auto& children = node->internal_children;
    auto& left_children = left_sibling->internal_children;

    for (auto it = children.begin(); it != children.end(); ++it) {
      auto [inserted_it, inserted] =
          left_children.insert(it->first, it->second);
      assert(inserted && "Merging should not have duplicates");
      // Update parent pointer
      it->second->parent = left_sibling;
    }
  }

  // Remove this node from parent
  InternalNode* parent = node->parent;
  assert(parent != nullptr && "Merging non-root node should have parent");

  // Find and remove the entry for this node in parent
  auto& parent_children = parent->internal_children;
  for (auto it = parent_children.begin(); it != parent_children.end(); ++it) {
    if (it->second == node) {
      parent_children.remove(it->first);
      break;
    }
  }

  // Deallocate this node
  deallocate_internal_node(node);

  // Check if parent underflowed (use hysteresis threshold)
  bool parent_is_root = (parent == internal_root_ && !root_is_leaf_);
  size_type parent_underflow_threshold =
      min_internal_size() > internal_hysteresis()
          ? min_internal_size() - internal_hysteresis()
          : 0;
  if (!parent_is_root && parent_children.size() < parent_underflow_threshold) {
    handle_internal_underflow(parent);
  } else if (parent_is_root && parent_children.size() == 1) {
    // Root has only one child left - make that child the new root
    InternalNode* new_root = parent_children.begin()->second;
    new_root->parent = nullptr;
    internal_root_ = new_root;
    deallocate_internal_node(parent);
  }
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::merge_with_right_internal_sibling(InternalNode* node) {
  InternalNode* right_sibling = find_right_sibling(node);
  assert(right_sibling != nullptr &&
         "merge_with_right_internal_sibling requires right sibling");

  if (node->children_are_leaves) {
    // Move all children from right sibling to this node
    auto& children = node->leaf_children;
    auto& right_children = right_sibling->leaf_children;

    for (auto it = right_children.begin(); it != right_children.end(); ++it) {
      auto [inserted_it, inserted] = children.insert(it->first, it->second);
      assert(inserted && "Merging should not have duplicates");
      // Update parent pointer
      it->second->parent = node;
    }
  } else {
    // Move all children from right sibling to this node
    auto& children = node->internal_children;
    auto& right_children = right_sibling->internal_children;

    for (auto it = right_children.begin(); it != right_children.end(); ++it) {
      auto [inserted_it, inserted] = children.insert(it->first, it->second);
      assert(inserted && "Merging should not have duplicates");
      // Update parent pointer
      it->second->parent = node;
    }
  }

  // Remove right sibling from parent
  InternalNode* parent = node->parent;
  assert(parent != nullptr && "Merging non-root node should have parent");

  // Find and remove the entry for right sibling in parent
  auto& parent_children = parent->internal_children;
  for (auto it = parent_children.begin(); it != parent_children.end(); ++it) {
    if (it->second == right_sibling) {
      parent_children.remove(it->first);
      break;
    }
  }

  // Deallocate right sibling
  deallocate_internal_node(right_sibling);

  // Check if parent underflowed (use hysteresis threshold)
  bool parent_is_root = (parent == internal_root_ && !root_is_leaf_);
  size_type parent_underflow_threshold =
      min_internal_size() > internal_hysteresis()
          ? min_internal_size() - internal_hysteresis()
          : 0;
  if (!parent_is_root && parent_children.size() < parent_underflow_threshold) {
    handle_internal_underflow(parent);
  } else if (parent_is_root && parent_children.size() == 1) {
    // Root has only one child left - make that child the new root
    InternalNode* new_root = parent_children.begin()->second;
    new_root->parent = nullptr;
    internal_root_ = new_root;
    deallocate_internal_node(parent);
  }
}

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::handle_internal_underflow(InternalNode* node) {
  // Get size based on children type
  size_type node_size = node->children_are_leaves
                            ? node->leaf_children.size()
                            : node->internal_children.size();

  size_type underflow_threshold =
      min_internal_size() > internal_hysteresis()
          ? min_internal_size() - internal_hysteresis()
          : 0;
  assert(node_size < underflow_threshold &&
         "handle_internal_underflow called on non-underflowed node");
  assert(node->parent != nullptr && "Root internal node cannot underflow");

  // Try to borrow from left sibling first
  if (borrow_from_left_internal_sibling(node)) {
    return;
  }

  // Try to borrow from right sibling
  if (borrow_from_right_internal_sibling(node)) {
    return;
  }

  // Can't borrow - must merge
  // Prefer merging with left sibling if it exists
  InternalNode* left_sibling = find_left_sibling(node);
  if (left_sibling != nullptr) {
    merge_with_left_internal_sibling(node);
  } else {
    // No left sibling - merge with right
    merge_with_right_internal_sibling(node);
  }
}

}  // namespace fast_containers
