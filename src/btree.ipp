// Implementation file for btree.hpp
// This file contains all method implementations for the btree class.

namespace fast_containers {

// Constructor
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

// Destructor
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::~btree() {
  deallocate_tree();
}

// Copy constructor
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::btree(const btree& other)
    : root_is_leaf_(true),
      leaf_root_(nullptr),
      size_(0),
      leftmost_leaf_(nullptr),
      rightmost_leaf_(nullptr) {
  // Copy all elements using insert (use const iterators)
  for (const_iterator it = other.begin(); it != other.end(); ++it) {
    insert(it->first, it->second);
  }
}

// Copy assignment operator
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT, MoveModeT>&
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::operator=(const btree& other) {
  if (this != &other) {
    // Clear existing contents
    deallocate_tree();

    // Copy all elements from other (use const iterators)
    for (const_iterator it = other.begin(); it != other.end(); ++it) {
      insert(it->first, it->second);
    }
  }
  return *this;
}

// Move constructor
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::btree(btree&& other) noexcept
    : root_is_leaf_(other.root_is_leaf_),
      size_(other.size_),
      leftmost_leaf_(other.leftmost_leaf_),
      rightmost_leaf_(other.rightmost_leaf_) {
  // Move the correct root pointer based on type
  if (root_is_leaf_) {
    leaf_root_ = other.leaf_root_;
  } else {
    internal_root_ = other.internal_root_;
  }

  // Leave other in a valid empty state
  other.root_is_leaf_ = true;
  other.leaf_root_ = nullptr;
  other.size_ = 0;
  other.leftmost_leaf_ = nullptr;
  other.rightmost_leaf_ = nullptr;
}

// Move assignment operator
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT, MoveModeT>&
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::operator=(btree&& other) noexcept {
  if (this != &other) {
    // Deallocate existing tree
    deallocate_tree();

    // Move other's resources to this
    root_is_leaf_ = other.root_is_leaf_;
    if (root_is_leaf_) {
      leaf_root_ = other.leaf_root_;
    } else {
      internal_root_ = other.internal_root_;
    }
    size_ = other.size_;
    leftmost_leaf_ = other.leftmost_leaf_;
    rightmost_leaf_ = other.rightmost_leaf_;

    // Leave other in a valid empty state
    other.root_is_leaf_ = true;
    other.leaf_root_ = nullptr;
    other.size_ = 0;
    other.leftmost_leaf_ = nullptr;
    other.rightmost_leaf_ = nullptr;
  }
  return *this;
}

// swap
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::swap(btree& other) noexcept {
  // Swap root pointer (handle union carefully)
  if (root_is_leaf_ && other.root_is_leaf_) {
    std::swap(leaf_root_, other.leaf_root_);
  } else if (!root_is_leaf_ && !other.root_is_leaf_) {
    std::swap(internal_root_, other.internal_root_);
  } else {
    // One is leaf, one is internal - need to swap carefully
    if (root_is_leaf_) {
      LeafNode* temp_leaf = leaf_root_;
      internal_root_ = other.internal_root_;
      other.leaf_root_ = temp_leaf;
    } else {
      InternalNode* temp_internal = internal_root_;
      leaf_root_ = other.leaf_root_;
      other.internal_root_ = temp_internal;
    }
  }

  // Swap other members
  std::swap(root_is_leaf_, other.root_is_leaf_);
  std::swap(size_, other.size_);
  std::swap(leftmost_leaf_, other.leftmost_leaf_);
  std::swap(rightmost_leaf_, other.rightmost_leaf_);
}

// find (non-const)
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
               MoveModeT>::iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::find(const Key& key) {
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

// find (const)
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
               MoveModeT>::const_iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::find(const Key& key) const {
  if (size_ == 0) {
    return end();
  }

  // Find the leaf that should contain the key
  LeafNode* leaf = find_leaf_for_key(key);

  // Search within the leaf
  auto it = leaf->data.find(key);
  if (it != leaf->data.end()) {
    return const_iterator(leaf, it);
  }
  return end();
}

// lower_bound
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
               MoveModeT>::iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::lower_bound(const Key& key) {
  if (size_ == 0) {
    return end();
  }

  // Find the leaf that should contain the key
  LeafNode* leaf = find_leaf_for_key(key);

  // Search within the leaf
  auto it = leaf->data.lower_bound(key);
  if (it != leaf->data.end()) {
    return iterator(leaf, it);
  }

  // If we reached the end of this leaf, move to the next leaf
  // (the next leaf will have all elements greater than this leaf's max)
  if (leaf->next_leaf != nullptr) {
    return iterator(leaf->next_leaf, leaf->next_leaf->data.begin());
  }

  return end();
}

// upper_bound
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
               MoveModeT>::iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::upper_bound(const Key& key) {
  if (size_ == 0) {
    return end();
  }

  // Find the leaf that should contain the key
  LeafNode* leaf = find_leaf_for_key(key);

  // Search within the leaf
  auto it = leaf->data.upper_bound(key);
  if (it != leaf->data.end()) {
    return iterator(leaf, it);
  }

  // If we reached the end of this leaf, move to the next leaf
  // (the next leaf will have all elements greater than this leaf's max)
  if (leaf->next_leaf != nullptr) {
    return iterator(leaf->next_leaf, leaf->next_leaf->data.begin());
  }

  return end();
}

// equal_range
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
std::pair<typename btree<Key, Value, LeafNodeSize, InternalNodeSize,
                         SearchModeT, MoveModeT>::iterator,
          typename btree<Key, Value, LeafNodeSize, InternalNodeSize,
                         SearchModeT, MoveModeT>::iterator>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::equal_range(const Key& key) {
  return {lower_bound(key), upper_bound(key)};
}

// insert
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

  // Use lower_bound to find position - single search for both existence check
  // and insertion point
  auto pos = leaf->data.lower_bound(key);

  // Check if key already exists at the position found by lower_bound
  if (pos != leaf->data.end() && pos->first == key) {
    return {iterator(leaf, pos), false};
  }

  // If leaf is full, split it
  if (leaf->data.size() >= LeafNodeSize) {
    return split_leaf(leaf, key, value);
  }

  // Leaf has space - insert using hint to avoid re-searching
  // Track if this will become the new minimum
  bool will_be_new_min = leaf->data.empty() || key < leaf->data.begin()->first;

  auto [leaf_it, inserted] = leaf->data.insert_hint(pos, key, value);
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

// insert(value_type)
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
std::pair<typename btree<Key, Value, LeafNodeSize, InternalNodeSize,
                         SearchModeT, MoveModeT>::iterator,
          bool>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::insert(const value_type& value) {
  return insert(value.first, value.second);
}

// emplace
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
template <typename... Args>
std::pair<typename btree<Key, Value, LeafNodeSize, InternalNodeSize,
                         SearchModeT, MoveModeT>::iterator,
          bool>
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::emplace(Args&&... args) {
  // Construct the pair from the arguments
  value_type pair(std::forward<Args>(args)...);
  // Extract key and value, then insert
  return insert(pair.first, pair.second);
}

// emplace_hint
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
template <typename... Args>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
               MoveModeT>::iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::emplace_hint(const_iterator hint, Args&&... args) {
  // For now, ignore the hint and just call emplace
  // In the future, we could use the hint to optimize the search
  (void)hint;  // Suppress unused parameter warning
  return emplace(std::forward<Args>(args)...).first;
}

// operator[]
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
Value& btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
             MoveModeT>::operator[](const Key& key) {
  // Try to insert with default-constructed value
  // If key exists, insert returns the existing element
  auto [it, inserted] = insert(key, Value{});
  return it->second;
}

// erase (by key)
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

  // Remove from leaf (returns 0 if not found, 1 if removed)
  const size_type removed = leaf->data.erase(key);
  if (removed == 0) {
    return 0;  // Key not found
  }
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
  if (root_is_leaf_ && leaf == leaf_root_) {
    // Root can have any size, no underflow handling needed
    // But still check if minimum key changed (though root has no parent)
    return 1;
  }

  // Update parent key if minimum changed (do this BEFORE checking underflow)
  // This ensures parent keys are always correct before any rebalancing
  if (!leaf->data.empty() && leaf->parent != nullptr) {
    const Key& new_min = leaf->data.begin()->first;
    update_parent_key_recursive(leaf, new_min);
  }

  const size_type underflow_threshold =
      min_leaf_size() > leaf_hysteresis() ? min_leaf_size() - leaf_hysteresis()
                                          : 0;
  if (leaf->data.size() < underflow_threshold) {
    handle_underflow(leaf);
  }

  return 1;
}

// erase (by iterator)
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
               MoveModeT>::iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::erase(iterator pos) {
  assert(pos != end() && "Cannot erase end iterator");

  // Get the next element before erasing
  // We need to save the key because the iterator will be invalidated
  iterator next = pos;
  ++next;

  std::optional<Key> next_key;
  if (next != end()) {
    next_key = (*next).first;
  }

  // Get the key to erase
  const Key erase_key = (*pos).first;

  // Perform the erase operation
  erase(erase_key);

  // Reconstruct iterator to next element
  // If the next element existed, find it; otherwise return end()
  if (next_key.has_value()) {
    return find(*next_key);
  }
  return end();
}

// erase (range)
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
               MoveModeT>::iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::erase(iterator first, iterator last) {
  // Save the key of the 'last' element to detect when to stop
  // We can't rely on iterator comparison because iterators may be invalidated
  // during node merges
  std::optional<Key> last_key;
  if (last != end()) {
    last_key = (*last).first;
  }

  // Erase elements one by one until we reach 'last'
  while (first != end()) {
    // Check if we've reached the 'last' position by comparing keys
    if (last_key.has_value() && (*first).first == *last_key) {
      break;
    }
    first = erase(first);
  }

  // Return iterator to the element following the last erased element
  // This is the original 'last' position
  if (last_key.has_value()) {
    return find(*last_key);
  }
  return end();
}

// allocate_leaf_node
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
               MoveModeT>::LeafNode*
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::allocate_leaf_node() {
  return new LeafNode();
}

// allocate_internal_node
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
               MoveModeT>::InternalNode*
btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
      MoveModeT>::allocate_internal_node(bool leaf_children) {
  return new InternalNode(leaf_children);
}

// deallocate_leaf_node
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::deallocate_leaf_node(LeafNode* node) {
  delete node;
}

// deallocate_internal_node
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::deallocate_internal_node(InternalNode* node) {
  delete node;
}

// deallocate_tree
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

// deallocate_internal_subtree
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

// split_leaf
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
  const size_type split_point = (LeafNodeSize + 1) / 2;

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

// insert_into_parent
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

// split_internal
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
  const size_type split_point = (InternalNodeSize + 1) / 2;

  // Use templated lambda to handle split logic for both child types
  auto perform_split = [&]<typename ChildPtrType>(
                           auto& node_children,
                           auto& new_node_children) -> const Key& {
    // Get split iterator
    auto split_it = node_children.begin();
    std::advance(split_it, split_point);

    // Use split_at() to efficiently move second half to new node
    node_children.split_at(split_it, new_node_children);

    // Update parent pointers for moved children
    for (auto it = new_node_children.begin(); it != new_node_children.end();
         ++it) {
      it->second->parent = new_node;
    }

    // Return reference to first key in new node (the promoted key)
    return new_node_children.begin()->first;
  };

  if (node->children_are_leaves) {
    const Key& promoted_key = perform_split.template operator()<LeafNode*>(
        node->leaf_children, new_node->leaf_children);
    return {promoted_key, new_node};
  } else {
    const Key& promoted_key = perform_split.template operator()<InternalNode*>(
        node->internal_children, new_node->internal_children);
    return {promoted_key, new_node};
  }
}

// update_parent_key_recursive
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
      children.erase(it->first);
      auto [new_it, ins] = children.insert(new_min, child);
      assert(ins && "Re-inserting child with new minimum key should succeed");
    }
  } else if (it != children.begin()) {
    // Check the previous entry
    --it;
    if (it->second == child && it->first != new_min) {
      is_leftmost = (it == children.begin());
      children.erase(it->first);
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

// find_left_sibling
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

  // Use templated lambda to handle search logic for both node types
  auto find_sibling = [&]<typename ChildPtrType>(auto& children) -> NodeType* {
    // Use lower_bound to efficiently find this node's position
    auto it = children.lower_bound(*node_min);

    // The entry pointing to this node should be at 'it' or one position before
    if (it != children.end() && it->second == node) {
      if (it == children.begin()) {
        return nullptr;  // This is the leftmost child
      }
      --it;
      return it->second;
    } else if (it != children.begin()) {
      --it;
      if (it->second == node) {
        if (it == children.begin()) {
          return nullptr;  // This is the leftmost child
        }
        --it;
        return it->second;
      }
    }

    assert(false && "Node not found in parent's children");
    return nullptr;
  };

  if constexpr (std::is_same_v<NodeType, LeafNode>) {
    if (node->data.empty()) {
      return nullptr;  // Empty leaf, can't find it
    }
    node_min = &(node->data.begin()->first);
    return find_sibling.template operator()<LeafNode*>(parent->leaf_children);
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
    return find_sibling.template operator()<InternalNode*>(
        parent->internal_children);
  }

  assert(false && "Node not found in parent's children");
  return nullptr;
}

// find_right_sibling
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

  // Use templated lambda to handle search logic for both node types
  auto find_sibling = [&]<typename ChildPtrType>(auto& children) -> NodeType* {
    // Use lower_bound to efficiently find this node's position
    auto it = children.lower_bound(*node_min);

    // The entry pointing to this node should be at 'it' or one position before
    if (it != children.end() && it->second == node) {
      ++it;
      if (it == children.end()) {
        return nullptr;  // This is the rightmost child
      }
      return it->second;
    } else if (it != children.begin()) {
      --it;
      if (it->second == node) {
        ++it;
        if (it == children.end()) {
          return nullptr;  // This is the rightmost child
        }
        return it->second;
      }
    }

    assert(false && "Node not found in parent's children");
    return nullptr;
  };

  if constexpr (std::is_same_v<NodeType, LeafNode>) {
    if (node->data.empty()) {
      return nullptr;  // Empty leaf, can't find it
    }
    node_min = &(node->data.begin()->first);
    return find_sibling.template operator()<LeafNode*>(parent->leaf_children);
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
    return find_sibling.template operator()<InternalNode*>(
        parent->internal_children);
  }

  assert(false && "Node not found in parent's children");
  return nullptr;
}

// borrow_from_left_sibling
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
template <typename NodeType>
bool btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::borrow_from_left_sibling(NodeType* node) {
  NodeType* left_sibling = find_left_sibling(node);

  // Can't borrow if no left sibling
  if (left_sibling == nullptr) {
    return false;
  }

  // Unified borrow implementation using templated lambda
  auto borrow_impl = [&]<typename ChildPtrType, bool UpdateParents>(
                         auto& children, auto& left_children,
                         size_type min_size, size_type hysteresis) -> bool {
    // Try to borrow at least hysteresis elements for efficiency
    // But don't leave sibling below min_size
    const size_type target_borrow = hysteresis > 0 ? hysteresis : 1;
    const size_type can_borrow = (left_children.size() > min_size)
                                     ? (left_children.size() - min_size)
                                     : 0;
    const size_type actual_borrow = std::min(target_borrow, can_borrow);

    if (actual_borrow == 0) {
      return false;
    }

    // Transfer suffix from left sibling to beginning of this node
    // This does a bulk copy and shifts arrays only once
    children.transfer_suffix_from(left_children, actual_borrow);

    // Update parent pointers for all transferred children (if applicable)
    if constexpr (UpdateParents) {
      auto it = children.begin();
      for (size_type i = 0; i < actual_borrow; ++i, ++it) {
        it->second->parent = node;
      }
    }

    // Update parent key for this node (minimum changed to first element/child)
    const Key& new_min = children.begin()->first;
    update_parent_key_recursive(node, new_min);

    return true;
  };

  if constexpr (std::is_same_v<NodeType, LeafNode>) {
    // Leaf node borrowing
    if (left_sibling->data.size() <= min_leaf_size()) {
      return false;
    }
    return borrow_impl.template operator()<std::pair<Key, Value>, false>(
        node->data, left_sibling->data, min_leaf_size(), leaf_hysteresis());
  } else {
    // Internal node borrowing
    if (node->children_are_leaves) {
      return borrow_impl.template operator()<LeafNode*, true>(
          node->leaf_children, left_sibling->leaf_children, min_internal_size(),
          internal_hysteresis());
    } else {
      return borrow_impl.template operator()<InternalNode*, true>(
          node->internal_children, left_sibling->internal_children,
          min_internal_size(), internal_hysteresis());
    }
  }
}

// borrow_from_right_sibling
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
template <typename NodeType>
bool btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::borrow_from_right_sibling(NodeType* node) {
  NodeType* right_sibling = find_right_sibling(node);

  // Can't borrow if no right sibling
  if (right_sibling == nullptr) {
    return false;
  }

  // Unified borrow implementation using templated lambda
  auto borrow_impl = [&]<typename ChildPtrType, bool UpdateParents>(
                         auto& children, auto& right_children,
                         size_type min_size, size_type hysteresis) -> bool {
    // Try to borrow at least hysteresis elements for efficiency
    // But don't leave sibling below min_size
    const size_type target_borrow = hysteresis > 0 ? hysteresis : 1;
    const size_type can_borrow = (right_children.size() > min_size)
                                     ? (right_children.size() - min_size)
                                     : 0;
    const size_type actual_borrow = std::min(target_borrow, can_borrow);

    if (actual_borrow == 0) {
      return false;
    }

    // Transfer prefix from right sibling to end of this node
    // This does a bulk copy and shifts arrays only once
    const size_type old_size = children.size();
    children.transfer_prefix_from(right_children, actual_borrow);

    // Update parent pointers for all transferred children (if applicable)
    if constexpr (UpdateParents) {
      auto it = children.begin();
      std::advance(it, old_size);
      for (size_type i = 0; i < actual_borrow; ++i, ++it) {
        it->second->parent = node;
      }
    }

    // Update parent key for right sibling (its minimum changed)
    const Key& new_right_min = right_children.begin()->first;
    update_parent_key_recursive(right_sibling, new_right_min);

    return true;
  };

  if constexpr (std::is_same_v<NodeType, LeafNode>) {
    // Leaf node borrowing
    if (right_sibling->data.size() <= min_leaf_size()) {
      return false;
    }
    return borrow_impl.template operator()<std::pair<Key, Value>, false>(
        node->data, right_sibling->data, min_leaf_size(), leaf_hysteresis());
  } else {
    // Internal node borrowing
    if (node->children_are_leaves) {
      return borrow_impl.template operator()<LeafNode*, true>(
          node->leaf_children, right_sibling->leaf_children,
          min_internal_size(), internal_hysteresis());
    } else {
      return borrow_impl.template operator()<InternalNode*, true>(
          node->internal_children, right_sibling->internal_children,
          min_internal_size(), internal_hysteresis());
    }
  }
}

// merge_with_left_sibling
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
template <typename NodeType>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::merge_with_left_sibling(NodeType* node) {
  NodeType* left_sibling = find_left_sibling(node);
  assert(left_sibling != nullptr &&
         "merge_with_left_sibling requires left sibling");

  if constexpr (std::is_same_v<NodeType, LeafNode>) {
    // Special case: if node is empty, just remove it from parent
    if (node->data.empty()) {
      // Find this node in parent by iterating (can't use key lookup)
      InternalNode* parent = node->parent;
      assert(parent != nullptr && "Merging non-root leaf should have parent");
      auto& parent_children = parent->leaf_children;

      // Linear search for this node
      auto it = parent_children.begin();
      while (it != parent_children.end() && it->second != node) {
        ++it;
      }
      assert(it != parent_children.end() && "Empty node should be in parent");
      parent_children.erase(it->first);

      // Update leaf chain
      if (node->prev_leaf) {
        node->prev_leaf->next_leaf = node->next_leaf;
      }
      if (node->next_leaf) {
        node->next_leaf->prev_leaf = node->prev_leaf;
      }
      if (leftmost_leaf_ == node) {
        leftmost_leaf_ = node->next_leaf;
      }
      if (rightmost_leaf_ == node) {
        rightmost_leaf_ = node->prev_leaf;
      }

      deallocate_leaf_node(node);

      // Check if parent underflowed
      const bool parent_is_root = (parent == internal_root_ && !root_is_leaf_);
      const size_type parent_underflow_threshold =
          min_internal_size() > internal_hysteresis()
              ? min_internal_size() - internal_hysteresis()
              : 0;
      if (!parent_is_root &&
          parent_children.size() < parent_underflow_threshold) {
        handle_underflow(parent);
      } else if (parent_is_root && parent_children.size() == 1) {
        LeafNode* new_root = parent_children.begin()->second;
        new_root->parent = nullptr;
        root_is_leaf_ = true;
        leaf_root_ = new_root;
        deallocate_internal_node(parent);
      }
      return;
    }

    // Capture node's minimum key BEFORE transferring data
    const Key node_min = node->data.begin()->first;

    // Merge leaf data
    const size_type count = node->data.size();
    left_sibling->data.transfer_prefix_from(node->data, count);

    // Update leaf chain pointers
    left_sibling->next_leaf = node->next_leaf;
    if (node->next_leaf) {
      node->next_leaf->prev_leaf = left_sibling;
    }

    // Update rightmost_leaf_ if necessary
    if (rightmost_leaf_ == node) {
      rightmost_leaf_ = left_sibling;
    }

    // Remove this leaf from parent
    InternalNode* parent = node->parent;
    assert(parent != nullptr && "Merging non-root leaf should have parent");

    // Find and remove the entry for this leaf in parent
    auto& parent_children = parent->leaf_children;
    auto it = parent_children.find(node_min);
    assert(it != parent_children.end() && it->second == node &&
           "Node's minimum key should map to node in parent");
    parent_children.erase(it->first);

    // Deallocate this leaf
    deallocate_leaf_node(node);

    // Check if parent underflowed (use hysteresis threshold)
    const bool parent_is_root = (parent == internal_root_ && !root_is_leaf_);
    const size_type parent_underflow_threshold =
        min_internal_size() > internal_hysteresis()
            ? min_internal_size() - internal_hysteresis()
            : 0;
    if (!parent_is_root &&
        parent_children.size() < parent_underflow_threshold) {
      handle_underflow(parent);
    } else if (parent_is_root && parent_children.size() == 1) {
      // Root has only one child left - make that child the new root
      LeafNode* new_root = parent_children.begin()->second;
      new_root->parent = nullptr;
      root_is_leaf_ = true;
      leaf_root_ = new_root;
      deallocate_internal_node(parent);
    }
  } else {
    // Capture node's minimum key BEFORE transferring children
    const Key node_min = node->children_are_leaves
                             ? node->leaf_children.begin()->first
                             : node->internal_children.begin()->first;

    // Merge internal node children using templated lambda
    auto merge_children = [&]<typename ChildPtrType>(auto& children,
                                                     auto& left_children) {
      // Move all children from this node to left sibling using bulk transfer
      const size_type count = children.size();
      const size_type old_size = left_children.size();
      left_children.transfer_prefix_from(children, count);

      // Update parent pointers for all transferred children
      auto it = left_children.begin();
      std::advance(it, old_size);
      for (size_type i = 0; i < count; ++i, ++it) {
        it->second->parent = left_sibling;
      }
    };

    if (node->children_are_leaves) {
      merge_children.template operator()<LeafNode*>(
          node->leaf_children, left_sibling->leaf_children);
    } else {
      merge_children.template operator()<InternalNode*>(
          node->internal_children, left_sibling->internal_children);
    }

    // Remove this node from parent
    InternalNode* parent = node->parent;
    assert(parent != nullptr && "Merging non-root node should have parent");

    // Find and remove the entry for this node in parent
    auto& parent_children = parent->internal_children;
    auto it = parent_children.find(node_min);
    assert(it != parent_children.end() && it->second == node &&
           "Node's minimum key should map to node in parent");
    parent_children.erase(it->first);

    // Deallocate this node
    deallocate_internal_node(node);

    // Check if parent underflowed (use hysteresis threshold)
    const bool parent_is_root = (parent == internal_root_ && !root_is_leaf_);
    const size_type parent_underflow_threshold =
        min_internal_size() > internal_hysteresis()
            ? min_internal_size() - internal_hysteresis()
            : 0;
    if (!parent_is_root &&
        parent_children.size() < parent_underflow_threshold) {
      handle_underflow(parent);
    } else if (parent_is_root && parent_children.size() == 1) {
      // Root has only one child left - make that child the new root
      InternalNode* new_root = parent_children.begin()->second;
      new_root->parent = nullptr;
      internal_root_ = new_root;
      deallocate_internal_node(parent);
    }
  }
}

// merge_with_right_sibling
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
template <typename NodeType>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::merge_with_right_sibling(NodeType* node) {
  NodeType* right_sibling = find_right_sibling(node);
  assert(right_sibling != nullptr &&
         "merge_with_right_sibling requires right sibling");

  if constexpr (std::is_same_v<NodeType, LeafNode>) {
    // Special case: if node is empty, just remove it from parent
    if (node->data.empty()) {
      // Find this node in parent by iterating (can't use key lookup)
      InternalNode* parent = node->parent;
      assert(parent != nullptr && "Merging non-root leaf should have parent");
      auto& parent_children = parent->leaf_children;

      // Linear search for this node
      auto it = parent_children.begin();
      while (it != parent_children.end() && it->second != node) {
        ++it;
      }
      assert(it != parent_children.end() && "Empty node should be in parent");
      parent_children.erase(it->first);

      // Update leaf chain
      if (node->prev_leaf) {
        node->prev_leaf->next_leaf = node->next_leaf;
      }
      if (node->next_leaf) {
        node->next_leaf->prev_leaf = node->prev_leaf;
      }
      if (leftmost_leaf_ == node) {
        leftmost_leaf_ = node->next_leaf;
      }
      if (rightmost_leaf_ == node) {
        rightmost_leaf_ = node->prev_leaf;
      }

      deallocate_leaf_node(node);

      // Check if parent underflowed
      const bool parent_is_root = (parent == internal_root_ && !root_is_leaf_);
      const size_type parent_underflow_threshold =
          min_internal_size() > internal_hysteresis()
              ? min_internal_size() - internal_hysteresis()
              : 0;
      if (!parent_is_root &&
          parent_children.size() < parent_underflow_threshold) {
        handle_underflow(parent);
      } else if (parent_is_root && parent_children.size() == 1) {
        LeafNode* new_root = parent_children.begin()->second;
        new_root->parent = nullptr;
        root_is_leaf_ = true;
        leaf_root_ = new_root;
        deallocate_internal_node(parent);
      }
      return;
    }

    // Capture right sibling's minimum key BEFORE transferring data
    const Key right_sibling_min = right_sibling->data.begin()->first;

    // Merge leaf data
    const size_type count = right_sibling->data.size();
    node->data.transfer_prefix_from(right_sibling->data, count);

    // Update leaf chain pointers
    node->next_leaf = right_sibling->next_leaf;
    if (right_sibling->next_leaf) {
      right_sibling->next_leaf->prev_leaf = node;
    }

    // Update rightmost_leaf_ if necessary
    if (rightmost_leaf_ == right_sibling) {
      rightmost_leaf_ = node;
    }

    // Remove right sibling from parent
    InternalNode* parent = node->parent;
    assert(parent != nullptr && "Merging non-root leaf should have parent");

    // Find and remove the entry for right sibling in parent
    auto& parent_children = parent->leaf_children;
    auto it = parent_children.find(right_sibling_min);
    assert(it != parent_children.end() && it->second == right_sibling &&
           "Right sibling's minimum key should map to right sibling in parent");
    parent_children.erase(it->first);

    // Deallocate right sibling
    deallocate_leaf_node(right_sibling);

    // Check if parent underflowed (use hysteresis threshold)
    const bool parent_is_root = (parent == internal_root_ && !root_is_leaf_);
    const size_type parent_underflow_threshold =
        min_internal_size() > internal_hysteresis()
            ? min_internal_size() - internal_hysteresis()
            : 0;
    if (!parent_is_root &&
        parent_children.size() < parent_underflow_threshold) {
      handle_underflow(parent);
    } else if (parent_is_root && parent_children.size() == 1) {
      // Root has only one child left - make that child the new root
      LeafNode* new_root = parent_children.begin()->second;
      new_root->parent = nullptr;
      root_is_leaf_ = true;
      leaf_root_ = new_root;
      deallocate_internal_node(parent);
    }
  } else {
    // Capture right sibling's minimum key BEFORE transferring children
    const Key right_sibling_min =
        right_sibling->children_are_leaves
            ? right_sibling->leaf_children.begin()->first
            : right_sibling->internal_children.begin()->first;

    // Merge internal node children using templated lambda
    auto merge_children = [&]<typename ChildPtrType>(auto& children,
                                                     auto& right_children) {
      // Move all children from right sibling to this node using bulk transfer
      const size_type count = right_children.size();
      const size_type old_size = children.size();
      children.transfer_prefix_from(right_children, count);

      // Update parent pointers for all transferred children
      auto it = children.begin();
      std::advance(it, old_size);
      for (size_type i = 0; i < count; ++i, ++it) {
        it->second->parent = node;
      }
    };

    if (node->children_are_leaves) {
      merge_children.template operator()<LeafNode*>(
          node->leaf_children, right_sibling->leaf_children);
    } else {
      merge_children.template operator()<InternalNode*>(
          node->internal_children, right_sibling->internal_children);
    }

    // Remove right sibling from parent
    InternalNode* parent = node->parent;
    assert(parent != nullptr && "Merging non-root node should have parent");

    // Find and remove the entry for right sibling in parent
    auto& parent_children = parent->internal_children;
    auto it = parent_children.find(right_sibling_min);
    assert(it != parent_children.end() && it->second == right_sibling &&
           "Right sibling's minimum key should map to right sibling in parent");
    parent_children.erase(it->first);

    // Deallocate right sibling
    deallocate_internal_node(right_sibling);

    // Check if parent underflowed (use hysteresis threshold)
    const bool parent_is_root = (parent == internal_root_ && !root_is_leaf_);
    const size_type parent_underflow_threshold =
        min_internal_size() > internal_hysteresis()
            ? min_internal_size() - internal_hysteresis()
            : 0;
    if (!parent_is_root &&
        parent_children.size() < parent_underflow_threshold) {
      handle_underflow(parent);
    } else if (parent_is_root && parent_children.size() == 1) {
      // Root has only one child left - make that child the new root
      InternalNode* new_root = parent_children.begin()->second;
      new_root->parent = nullptr;
      internal_root_ = new_root;
      deallocate_internal_node(parent);
    }
  }
}

// handle_underflow
template <Comparable Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, SearchMode SearchModeT,
          MoveMode MoveModeT>
template <typename NodeType>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, SearchModeT,
           MoveModeT>::handle_underflow(NodeType* node) {
  // Compute node size and underflow threshold based on node type
  size_type node_size;
  size_type underflow_threshold;

  if constexpr (std::is_same_v<NodeType, LeafNode>) {
    node_size = node->data.size();
    underflow_threshold = min_leaf_size() > leaf_hysteresis()
                              ? min_leaf_size() - leaf_hysteresis()
                              : 0;
    assert(node_size < underflow_threshold &&
           "handle_underflow called on non-underflowed leaf");
  } else {
    // InternalNode - get size based on children type
    node_size = node->children_are_leaves ? node->leaf_children.size()
                                          : node->internal_children.size();
    underflow_threshold = min_internal_size() > internal_hysteresis()
                              ? min_internal_size() - internal_hysteresis()
                              : 0;
    assert(node_size < underflow_threshold &&
           "handle_underflow called on non-underflowed internal node");
  }

  assert(node->parent != nullptr && "Root node cannot underflow");

  // Try to borrow from left sibling first
  if (borrow_from_left_sibling(node)) {
    return;
  }

  // Try to borrow from right sibling
  if (borrow_from_right_sibling(node)) {
    return;
  }

  // Can't borrow - must merge
  // Prefer merging with left sibling if it exists
  NodeType* left_sibling = find_left_sibling(node);
  if (left_sibling != nullptr) {
    merge_with_left_sibling(node);
  } else {
    // No left sibling - merge with right
    merge_with_right_sibling(node);
  }
}

}  // namespace fast_containers
