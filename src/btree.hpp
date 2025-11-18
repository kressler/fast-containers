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

}  // namespace fast_containers
