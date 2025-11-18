#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
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
   * Complexity: O(1)
   */
  iterator begin() {
    if (size_ == 0) {
      return end();
    }
    // Find the leftmost leaf
    LeafNode* leftmost = find_leftmost_leaf();
    return iterator(leftmost, leftmost->data.begin());
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

 private:
  //
  // TEST-ONLY PUBLIC MEMBERS
  // These members are exposed for testing.
  // They will be made private once insert() is implemented in Phase 3.
  //
 public:
  /**
   * Allocate a new leaf node.
   * @warning For testing only - will be private in Phase 3
   */
  LeafNode* allocate_leaf_node();

  /**
   * Allocate a new internal node.
   * @param leaf_children If true, children are LeafNode*, otherwise
   * InternalNode*
   * @warning For testing only - will be private in Phase 3
   */
  InternalNode* allocate_internal_node(bool leaf_children);

  /**
   * Deallocate a leaf node.
   * @warning For testing only - will be private in Phase 3
   */
  void deallocate_leaf_node(LeafNode* node);

  /**
   * Deallocate an internal node.
   * @warning For testing only - will be private in Phase 3
   */
  void deallocate_internal_node(InternalNode* node);

  // Root and size members - exposed for testing
  // @warning Will be private in Phase 3
  bool root_is_leaf_;
  union {
    LeafNode* leaf_root_;
    InternalNode* internal_root_;
  };
  size_type size_;

 private:
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
   * Finds the leftmost (first) leaf in the tree.
   * Assumes tree is non-empty.
   */
  LeafNode* find_leftmost_leaf() const {
    if (root_is_leaf_) {
      return leaf_root_;
    }

    // Traverse down always taking the first child
    InternalNode* node = internal_root_;
    while (!node->children_are_leaves) {
      auto it = node->internal_children.begin();
      node = it->second;
    }

    // Now node has leaf children - return the first one
    auto it = node->leaf_children.begin();
    return it->second;
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
  root_is_leaf_ = true;
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
