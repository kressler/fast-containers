#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>

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

 private:
  // Forward declarations
  struct InternalNode;

  /**
   * Leaf node - stores actual key-value pairs in an ordered_array.
   * Forms a doubly-linked list with other leaf nodes for iteration.
   */
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
    bool children_are_leaves;
    union {
      ordered_array<Key, LeafNode*, InternalNodeSize, SearchModeT, MoveModeT>
          leaf_children;
      ordered_array<Key, InternalNode*, InternalNodeSize, SearchModeT,
                    MoveModeT>
          internal_children;
    };
    InternalNode* parent;  // Parent is always internal (or nullptr for root)

    // Constructor for leaf children
    explicit InternalNode(bool leaf_children)
        : children_are_leaves(leaf_children), parent(nullptr) {
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

  // Root can be either a leaf or internal node
  bool root_is_leaf_;
  union {
    LeafNode* leaf_root_;
    InternalNode* internal_root_;
  };

  size_type size_;
};

// Implementation

template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::btree()
    : root_is_leaf_(true), leaf_root_(nullptr), size_(0) {}

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
  leaf_root_ = nullptr;
  size_ = 0;
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

}  // namespace fast_containers
