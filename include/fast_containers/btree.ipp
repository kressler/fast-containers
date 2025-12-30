// Implementation file for btree.hpp
// This file contains all method implementations for the btree class.

namespace kressler::fast_containers {

// Constructor
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::btree(const Allocator& alloc)
    : root_is_leaf_(true),
      size_(0),
      leaf_alloc_(alloc),
      internal_alloc_(alloc) {
  // Always allocate an empty root leaf to simplify insert/erase logic
  leaf_root_ = allocate_leaf_node();
  leftmost_leaf_ = leaf_root_;
  rightmost_leaf_ = leaf_root_;
}

// Destructor
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::~btree() {
  // Unconditionally deallocate all nodes including root
  deallocate_tree();
}

// Copy constructor
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::btree(const btree& other)
    : root_is_leaf_(true),
      size_(0),
      leaf_alloc_(std::allocator_traits<decltype(leaf_alloc_)>::
                      select_on_container_copy_construction(other.leaf_alloc_)),
      internal_alloc_(
          std::allocator_traits<decltype(internal_alloc_)>::
              select_on_container_copy_construction(other.internal_alloc_)) {
  // Always allocate an empty root leaf
  leaf_root_ = allocate_leaf_node();
  leftmost_leaf_ = leaf_root_;
  rightmost_leaf_ = leaf_root_;

  // Copy all elements using insert (use const iterators)
  for (const_iterator it = other.begin(); it != other.end(); ++it) {
    insert(it->first, it->second);
  }
}

// Copy assignment operator
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>&
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::operator=(const btree& other) {
  if (this != &other) {
    // Clear existing contents (keeps root allocated)
    clear();

    // Copy all elements from other (use const iterators)
    for (const_iterator it = other.begin(); it != other.end(); ++it) {
      insert(it->first, it->second);
    }
  }
  return *this;
}

// Move constructor
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::btree(btree&& other) noexcept
    : root_is_leaf_(other.root_is_leaf_),
      size_(other.size_),
      leftmost_leaf_(other.leftmost_leaf_),
      rightmost_leaf_(other.rightmost_leaf_),
      leaf_alloc_(std::move(other.leaf_alloc_)),
      internal_alloc_(std::move(other.internal_alloc_)) {
  // Move the correct root pointer based on type
  if (root_is_leaf_) {
    leaf_root_ = other.leaf_root_;
  } else {
    internal_root_ = other.internal_root_;
  }

  // Leave other in a valid empty state with a new empty root
  other.root_is_leaf_ = true;
  other.leaf_root_ = other.allocate_leaf_node();
  other.size_ = 0;
  other.leftmost_leaf_ = other.leaf_root_;
  other.rightmost_leaf_ = other.leaf_root_;
}

// Move assignment operator
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>&
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::operator=(btree&& other) noexcept {
  if (this != &other) {
    // Deallocate existing tree (including root)
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

    // Leave other in a valid empty state with a new empty root
    other.root_is_leaf_ = true;
    other.leaf_root_ = other.allocate_leaf_node();
    other.size_ = 0;
    other.leftmost_leaf_ = other.leaf_root_;
    other.rightmost_leaf_ = other.leaf_root_;
  }
  return *this;
}

// Initializer list constructor
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::btree(std::initializer_list<value_type> init,
                                    const Allocator& alloc)
    : btree(alloc) {
  for (const auto& elem : init) {
    insert(elem.first, elem.second);
  }
}

// Range constructor
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename InputIt>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::btree(InputIt first, InputIt last,
                                    const Allocator& alloc)
    : btree(alloc) {
  for (auto it = first; it != last; ++it) {
    insert(it->first, it->second);
  }
}

// swap
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
           Allocator>::swap(btree& other) noexcept {
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
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
               Allocator>::iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT, Allocator>::find(
    const Key& key) {
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
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
               Allocator>::const_iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT, Allocator>::find(
    const Key& key) const {
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
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
               Allocator>::iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::lower_bound(const Key& key) {
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
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
               Allocator>::iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::upper_bound(const Key& key) {
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
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
std::pair<typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare,
                         SearchModeT, Allocator>::iterator,
          typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare,
                         SearchModeT, Allocator>::iterator>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::equal_range(const Key& key) {
  // Optimized: single tree traversal instead of two
  // For unique keys (btree is like std::map):
  // - If key exists: upper_bound is just next position after lower_bound
  // - If key doesn't exist: lower_bound == upper_bound
  auto lb = lower_bound(key);

  // Check if key exists: equivalent comparison (!comp(a,b) && !comp(b,a))
  if (lb != end() && !comp_(key, lb->first) && !comp_(lb->first, key)) {
    // Key exists - upper_bound is next position
    auto ub = lb;
    ++ub;
    return {lb, ub};
  } else {
    // Key doesn't exist - lower_bound == upper_bound
    return {lb, lb};
  }
}

// find_leaf_for_key
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
               Allocator>::LeafNode*
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::find_leaf_for_key(const Key& key) const {
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

// insert
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
std::pair<typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare,
                         SearchModeT, Allocator>::iterator,
          bool>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::insert(const Key& key, const Value& value) {
  return insert_impl(
      key,
      [this](LeafNode* leaf, auto pos) {
        // Key exists - don't modify, just return
        return std::pair{iterator(leaf, pos), false};
      },
      [&value]() -> const Value& {
        // Return reference to value (no copy needed)
        return value;
      });
}

// insert(value_type)
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
std::pair<typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare,
                         SearchModeT, Allocator>::iterator,
          bool>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::insert(const value_type& value) {
  return insert(value.first, value.second);
}

// emplace
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename... Args>
std::pair<typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare,
                         SearchModeT, Allocator>::iterator,
          bool>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::emplace(Args&&... args) {
  // Construct the pair from the arguments
  value_type pair(std::forward<Args>(args)...);
  // Extract key and value, then insert
  return insert(pair.first, pair.second);
}

// emplace_hint
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename... Args>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
               Allocator>::iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::emplace_hint(const_iterator hint, Args&&... args) {
  // For now, ignore the hint and just call emplace
  // In the future, we could use the hint to optimize the search
  (void)hint;  // Suppress unused parameter warning
  return emplace(std::forward<Args>(args)...).first;
}

// try_emplace
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename... Args>
std::pair<typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare,
                         SearchModeT, Allocator>::iterator,
          bool>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::try_emplace(const Key& key, Args&&... args) {
  return insert_impl(
      key,
      [this](LeafNode* leaf, auto pos) {
        // Key exists - CRITICAL: don't construct value, just return
        return std::pair{iterator(leaf, pos), false};
      },
      [&args...]() -> Value {
        // Construct value on-demand (only if actually inserting)
        // This preserves try_emplace's optimization - value only constructed
        // when key doesn't exist (checked before calling this lambda)
        return Value(std::forward<Args>(args)...);
      });
}

// insert_or_assign
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename M>
std::pair<typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare,
                         SearchModeT, Allocator>::iterator,
          bool>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::insert_or_assign(const Key& key, M&& value) {
  return insert_impl(
      key,
      [this, &value](LeafNode* leaf, auto pos) {
        // Key exists - ASSIGN the new value (unique behavior)
        pos->second = std::forward<M>(value);
        return std::pair{iterator(leaf, pos), false};
      },
      [&value]() {
        // Forward the value for insertion
        return std::forward<M>(value);
      });
}

// operator[]
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
Value& btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
             Allocator>::operator[](const Key& key) {
  // Try to insert with default-constructed value
  // If key exists, insert returns the existing element
  auto [it, inserted] = insert(key, Value{});
  return it->second;
}

// at (non-const)
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
Value& btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
             Allocator>::at(const Key& key) {
  auto it = find(key);
  if (it == end()) {
    throw std::out_of_range("btree::at: key not found");
  }
  return it->second;
}

// at (const)
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
const Value&
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::at(const Key& key) const {
  auto it = find(key);
  if (it == end()) {
    throw std::out_of_range("btree::at: key not found");
  }
  return it->second;
}

// erase (by key)
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
               Allocator>::size_type
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::erase(const Key& key) {
  // Empty tree - nothing to remove
  if (size_ == 0) {
    return 0;
  }

  // Find the key and delegate to iterator-based erase
  // This ensures consistent behavior and avoids code duplication
  auto it = find(key);
  if (it == end()) {
    return 0;  // Key not found
  }

  // Delegate to iterator erase (handles all rebalancing logic)
  erase(it);
  return 1;
}

// erase (by iterator)
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
               Allocator>::iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::erase(iterator pos) {
  assert(pos != end() && "Cannot erase end iterator");

  // Extract leaf and leaf iterator from btree iterator
  LeafNode* leaf = pos.leaf_node_;
  auto leaf_it = *pos.leaf_it_;  // Dereference optional

  // Check if we're erasing from beginning (will need parent update)
  const bool erasing_from_beginning = (leaf_it == leaf->data.begin());

  // Check if rebalancing will be needed BEFORE erasing
  // This allows us to avoid saving next_key when not needed (important for
  // large keys) Early return if root is leaf (no rebalancing ever needed)
  if (root_is_leaf_ && leaf == leaf_root_) {
    auto next_in_leaf = leaf->data.erase(leaf_it);
    size_--;
    // Root can have any size, no underflow handling needed
    // Return iterator to next element (no search needed!)
    if (next_in_leaf != leaf->data.end()) {
      return iterator(leaf, next_in_leaf);
    }
    return end();
  }

  // Predict if rebalancing will be needed after erase (size - 1)
  const size_type underflow_threshold =
      min_leaf_size() > leaf_hysteresis() ? min_leaf_size() - leaf_hysteresis()
                                          : 0;
  const size_type size_after_erase = leaf->data.size() - 1;
  const bool needs_rebalancing =
      size_after_erase == 0 || size_after_erase < underflow_threshold;

  // Capture next element BEFORE erasing (for O(1) tracking through rebalancing)
  std::optional<size_t> next_index;
  bool next_in_next_leaf = false;
  if (needs_rebalancing) {
    // Calculate index of element after the one being erased
    const size_t erase_index = std::distance(leaf->data.begin(), leaf_it);
    if (erase_index + 1 < leaf->data.size()) {
      // Next element is in this leaf - track its index (O(1) during rebalancing)
      // After erase, it will shift left by 1, so index becomes erase_index
      next_index = erase_index;
    } else if (leaf->next_leaf != nullptr) {
      // Next element is first element of next leaf
      // Special handling: if we borrow from right or merge with right,
      // that element will move into this leaf at the old_size position
      next_in_next_leaf = true;
    }
    // else: at end of tree, no next element
  }

  // Erase from leaf using iterator (no search needed!)
  // Returns iterator to next element in this leaf
  auto next_in_leaf = leaf->data.erase(leaf_it);
  size_--;

  // Update parent key if minimum changed (only if erased from beginning)
  // Do this BEFORE rebalancing to ensure parent keys are correct
  if (erasing_from_beginning && !leaf->data.empty() &&
      leaf->parent != nullptr) {
    const Key& new_min = leaf->data.begin()->first;
    update_parent_key_recursive(leaf, new_min);
  }

  if (needs_rebalancing) {
    // handle_underflow uses O(1) index tracking when available
    auto [result_leaf, next_iter] = handle_underflow(leaf, next_index, next_in_next_leaf);
    if (next_iter.has_value()) {
      return *next_iter;
    }
    // Next element was not in this leaf, check next leaf
    if (result_leaf->next_leaf != nullptr) {
      return iterator(result_leaf->next_leaf,
                      result_leaf->next_leaf->data.begin());
    }
    return end();
  }

  // No underflow - can use next_in_leaf directly (no search needed!)
  if (next_in_leaf != leaf->data.end()) {
    // Next element is in same leaf
    return iterator(leaf, next_in_leaf);
  } else if (leaf->next_leaf != nullptr) {
    // Erased last element in leaf - move to next leaf
    return iterator(leaf->next_leaf, leaf->next_leaf->data.begin());
  }
  return end();
}

// erase (range)
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
               Allocator>::iterator
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::erase(iterator first, iterator last) {
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
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
               Allocator>::LeafNode*
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::allocate_leaf_node() {
  using leaf_alloc_traits = std::allocator_traits<decltype(leaf_alloc_)>;
  LeafNode* node = leaf_alloc_.allocate(1);
  leaf_alloc_traits::construct(leaf_alloc_, node);
  return node;
}

// allocate_internal_node
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
               Allocator>::InternalNode*
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::allocate_internal_node(bool leaf_children) {
  using internal_alloc_traits =
      std::allocator_traits<decltype(internal_alloc_)>;
  InternalNode* node = internal_alloc_.allocate(1);
  internal_alloc_traits::construct(internal_alloc_, node, leaf_children);
  return node;
}

// deallocate_leaf_node
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
           Allocator>::deallocate_leaf_node(LeafNode* node) {
  using leaf_alloc_traits = std::allocator_traits<decltype(leaf_alloc_)>;
  leaf_alloc_traits::destroy(leaf_alloc_, node);
  leaf_alloc_.deallocate(node, 1);
}

// deallocate_internal_node
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
           Allocator>::deallocate_internal_node(InternalNode* node) {
  using internal_alloc_traits =
      std::allocator_traits<decltype(internal_alloc_)>;
  internal_alloc_traits::destroy(internal_alloc_, node);
  internal_alloc_.deallocate(node, 1);
}

// clear - removes all elements but keeps root allocated
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
           Allocator>::clear() {
  if (size_ == 0) {
    return;  // Already empty
  }

  if (root_is_leaf_) {
    // Just clear the root leaf's data
    leaf_root_->data.clear();
  } else {
    // Deallocate internal tree and reset to empty leaf root
    deallocate_internal_subtree(internal_root_);

    // Allocate new empty leaf root
    leaf_root_ = allocate_leaf_node();
    root_is_leaf_ = true;
    leftmost_leaf_ = leaf_root_;
    rightmost_leaf_ = leaf_root_;
  }

  size_ = 0;
}

// deallocate_tree - deallocates entire tree including root (for destructor)
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
           Allocator>::deallocate_tree() {
  if (root_is_leaf_) {
    // Tree has only one node (leaf root) - deallocate it
    deallocate_leaf_node(leaf_root_);
  } else {
    // Tree has internal nodes - use recursive deallocation
    deallocate_internal_subtree(internal_root_);
  }

  // Reset state to nullptr (for destructor)
  root_is_leaf_ = true;
  leaf_root_ = nullptr;
  size_ = 0;
  leftmost_leaf_ = nullptr;
  rightmost_leaf_ = nullptr;
}

// deallocate_internal_subtree
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
           Allocator>::deallocate_internal_subtree(InternalNode* node) {
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

// split_leaf_and_insert
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename GetValue>
std::pair<typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare,
                         SearchModeT, Allocator>::iterator,
          bool>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::split_leaf_and_insert(LeafNode* leaf, const Key& key,
                                        GetValue&& get_value) {
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

  // Determine target leaf
  LeafNode* target_leaf = comp_(key, promoted_key) ? leaf : new_leaf;

  // Get value on-demand after determining target leaf
  // This preserves try_emplace's optimization
  auto value = get_value();

  // Insert the new key-value pair into appropriate leaf
  auto [leaf_it, inserted] = target_leaf->data.insert(key, std::move(value));

  // Key is guaranteed not to exist (checked by insert_impl before calling)
  assert(inserted && "split_leaf_and_insert: key should not exist in tree");

  size_++;

  // Insert promoted key into parent
  insert_into_parent(leaf, promoted_key, new_leaf);

  // If we inserted into the left half, update parent key if necessary
  if (target_leaf == leaf) {
    const Key& new_left_min = leaf->data.begin()->first;
    update_parent_key_recursive(leaf, new_left_min);
  }

  // Update leftmost_leaf_ if the leaf became the new leftmost
  if (target_leaf == leaf &&
      (leftmost_leaf_ == nullptr ||
       comp_(leaf->data.begin()->first, leftmost_leaf_->data.begin()->first))) {
    leftmost_leaf_ = leaf;
  }

  return {iterator(target_leaf, leaf_it), true};
}

// insert_impl
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename OnExists, typename GetValue>
std::pair<typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare,
                         SearchModeT, Allocator>::iterator,
          bool>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::insert_impl(const Key& key, OnExists&& on_exists,
                              GetValue&& get_value) {
  // Find the appropriate leaf for the key
  LeafNode* leaf = find_leaf_for_key(key);

  // Use lower_bound to find position - single search for both existence check
  // and insertion point
  auto pos = leaf->data.lower_bound(key);

  // Check if key already exists - delegate to on_exists callback
  if (pos != leaf->data.end() && pos->first == key) {
    return on_exists(leaf, pos);
  }

  // If leaf is full, split it with on-demand value construction
  if (leaf->data.size() >= LeafNodeSize) {
    return split_leaf_and_insert(leaf, key, std::forward<GetValue>(get_value));
  }

  // Leaf has space - get value on-demand and insert using hint
  auto value = get_value();
  auto [leaf_it, inserted] = leaf->data.insert_hint(pos, key, std::move(value));

  // Key is guaranteed not to exist (checked above)
  assert(inserted && "insert_impl: key should not exist");

  size_++;

  // If we inserted at the beginning and leaf has a parent, update parent key
  if (leaf_it == leaf->data.begin() && leaf->parent != nullptr) {
    update_parent_key_recursive(leaf, key);
  }

  return {iterator(leaf, leaf_it), true};
}

// insert_into_parent
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename NodeType>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
           Allocator>::insert_into_parent(NodeType* left_child, const Key& key,
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
    InternalNode* target_parent = comp_(key, promoted_key) ? parent : new_parent;
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
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
std::pair<const Key&, typename btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare,
                                     SearchModeT, Allocator>::InternalNode*>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::split_internal(InternalNode* node) {
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
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename ChildNodeType>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
           Allocator>::update_parent_key_recursive(ChildNodeType* child,
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
      // Update key in place - position doesn't change since new_min is still
      // within this child's range (between left and right sibling mins)
      children.unsafe_update_key(it, new_min);
    }
  } else if (it != children.begin()) {
    // Check the previous entry
    --it;
    if (it->second == child && it->first != new_min) {
      is_leftmost = (it == children.begin());
      // Update key in place - position doesn't change since new_min is still
      // within this child's range (between left and right sibling mins)
      children.unsafe_update_key(it, new_min);
    }
  }

  // If this child is leftmost in parent, recursively update grandparent
  // The parent's minimum key changed, so we need to update it too
  if (is_leftmost && parent->parent != nullptr) {
    update_parent_key_recursive(parent, new_min);
  }
}

// find_left_sibling
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename NodeType>
NodeType* btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
                Allocator>::find_left_sibling(NodeType* node) const {
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
    node_min = &(node->data.begin()->first);
    return find_sibling.template operator()<LeafNode*>(parent->leaf_children);
  } else {
    // InternalNode
    if (node->children_are_leaves) {
      node_min = &(node->leaf_children.begin()->first);
    } else {
      node_min = &(node->internal_children.begin()->first);
    }
    return find_sibling.template operator()<InternalNode*>(
        parent->internal_children);
  }
}

// find_right_sibling
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename NodeType>
NodeType* btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
                Allocator>::find_right_sibling(NodeType* node) const {
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
    node_min = &(node->data.begin()->first);
    return find_sibling.template operator()<LeafNode*>(parent->leaf_children);
  } else {
    // InternalNode
    if (node->children_are_leaves) {
      node_min = &(node->leaf_children.begin()->first);
    } else {
      node_min = &(node->internal_children.begin()->first);
    }
    return find_sibling.template operator()<InternalNode*>(
        parent->internal_children);
  }
}

// handle_parent_after_merge
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename NodeType, typename ChildrenArray>
void btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
           Allocator>::handle_parent_after_merge(InternalNode* parent,
                                                  ChildrenArray& parent_children) {
  // Check if parent underflowed (use hysteresis threshold)
  const bool parent_is_root = (parent == internal_root_ && !root_is_leaf_);
  const size_type parent_underflow_threshold =
      min_internal_size() > internal_hysteresis()
          ? min_internal_size() - internal_hysteresis()
          : 0;

  if (!parent_is_root &&
      parent_children.size() < parent_underflow_threshold) {
    // Recursive underflow - discard iterator (internal node)
    handle_underflow(parent, std::nullopt, false);
  } else if (parent_is_root && parent_children.size() == 1) {
    // Root has only one child left - make that child the new root
    NodeType* new_root = parent_children.begin()->second;
    new_root->parent = nullptr;

    if constexpr (std::is_same_v<NodeType, LeafNode>) {
      root_is_leaf_ = true;
      leaf_root_ = new_root;
    } else {
      // InternalNode becomes new root
      internal_root_ = new_root;
    }

    deallocate_internal_node(parent);
  }
}

// borrow_from_left_sibling
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename NodeType>
std::pair<NodeType*, std::optional<typename btree<Key, Value, LeafNodeSize,
                                                   InternalNodeSize, Compare,
                                                   SearchModeT, Allocator>::iterator>>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::borrow_from_left_sibling(NodeType* node,
                                           const std::optional<size_t>& next_index,
                                           bool next_in_next_leaf) {
  NodeType* left_sibling = find_left_sibling(node);

  // Can't borrow if no left sibling
  if (left_sibling == nullptr) {
    return {nullptr, std::nullopt};
  }

  // Track how many elements we actually borrow for index adjustment
  size_type actual_borrow = 0;

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
    actual_borrow = std::min(target_borrow, can_borrow);

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
      return {nullptr, std::nullopt};
    }
    bool success =
        borrow_impl.template operator()<std::pair<Key, Value>, false>(
            node->data, left_sibling->data, min_leaf_size(), leaf_hysteresis());

    if (!success) {
      return {nullptr, std::nullopt};
    }

    // Calculate iterator to next element
    std::optional<iterator> next_iter;
    if (next_in_next_leaf) {
      // Special case: next is in next leaf, stays there (elements inserted at beginning)
      // Return nullopt, let caller use next_leaf pointer
      return {node, std::nullopt};
    } else if (next_index.has_value()) {
      // Next was in this node, index shifts right by borrowed count
      const size_t new_index = *next_index + actual_borrow;
      if (new_index < node->data.size()) {
        auto it = node->data.begin();
        std::advance(it, new_index);
        next_iter = iterator(node, it);
      }
    }
    // else: no next element to track
    return {node, next_iter};
  } else {
    // Internal node borrowing - we don't track iterators through internal nodes
    bool success;
    if (node->children_are_leaves) {
      success = borrow_impl.template operator()<LeafNode*, true>(
          node->leaf_children, left_sibling->leaf_children, min_internal_size(),
          internal_hysteresis());
    } else {
      success = borrow_impl.template operator()<InternalNode*, true>(
          node->internal_children, left_sibling->internal_children,
          min_internal_size(), internal_hysteresis());
    }
    return {success ? node : nullptr, std::nullopt};
  }
}

// borrow_from_right_sibling
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename NodeType>
std::pair<NodeType*, std::optional<typename btree<Key, Value, LeafNodeSize,
                                                   InternalNodeSize, Compare,
                                                   SearchModeT, Allocator>::iterator>>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::borrow_from_right_sibling(NodeType* node,
                                            const std::optional<size_t>& next_index,
                                            bool next_in_next_leaf) {
  NodeType* right_sibling = find_right_sibling(node);

  // Can't borrow if no right sibling
  if (right_sibling == nullptr) {
    return {nullptr, std::nullopt};
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
      return {nullptr, std::nullopt};
    }

    // Capture old size before borrowing (needed if next_in_next_leaf)
    const size_type old_size = node->data.size();

    bool success =
        borrow_impl.template operator()<std::pair<Key, Value>, false>(
            node->data, right_sibling->data, min_leaf_size(),
            leaf_hysteresis());

    if (!success) {
      return {nullptr, std::nullopt};
    }

    // Calculate iterator to next element
    std::optional<iterator> next_iter;
    if (next_in_next_leaf) {
      // Special case: next was in right sibling (next leaf), now at index old_size
      auto it = node->data.begin();
      std::advance(it, old_size);
      next_iter = iterator(node, it);
    } else if (next_index.has_value() && *next_index < node->data.size()) {
      // Next was in this node, index unchanged (elements appended to end)
      auto it = node->data.begin();
      std::advance(it, *next_index);
      next_iter = iterator(node, it);
    }
    // else: no next element to track
    return {node, next_iter};
  } else {
    // Internal node borrowing - we don't track iterators through internal nodes
    bool success;
    if (node->children_are_leaves) {
      success = borrow_impl.template operator()<LeafNode*, true>(
          node->leaf_children, right_sibling->leaf_children,
          min_internal_size(), internal_hysteresis());
    } else {
      success = borrow_impl.template operator()<InternalNode*, true>(
          node->internal_children, right_sibling->internal_children,
          min_internal_size(), internal_hysteresis());
    }
    return {success ? node : nullptr, std::nullopt};
  }
}

// merge_with_left_sibling
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename NodeType>
std::pair<NodeType*, std::optional<typename btree<Key, Value, LeafNodeSize,
                                                   InternalNodeSize, Compare,
                                                   SearchModeT, Allocator>::iterator>>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::merge_with_left_sibling(NodeType* node,
                                          const std::optional<size_t>& next_index,
                                          bool next_in_next_leaf) {
  NodeType* left_sibling = find_left_sibling(node);
  assert(left_sibling != nullptr &&
         "merge_with_left_sibling requires left sibling");

  if constexpr (std::is_same_v<NodeType, LeafNode>) {
    // Capture node's minimum key BEFORE transferring data
    const Key &node_min = node->data.begin()->first;

    // Capture left_sibling's size BEFORE merge for index calculation
    const size_type left_old_size = left_sibling->data.size();

    // Remove this leaf from parent
    InternalNode* parent = node->parent;
    assert(parent != nullptr && "Merging non-root leaf should have parent");

    // Find and remove the entry for this leaf in parent
    auto& parent_children = parent->leaf_children;
    auto it = parent_children.find(node_min);
    assert(it != parent_children.end() && it->second == node &&
           "Node's minimum key should map to node in parent");
    parent_children.erase(it->first);

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

    // Deallocate this leaf
    deallocate_leaf_node(node);

    // Handle parent underflow and potential root reduction
    handle_parent_after_merge<LeafNode>(parent, parent_children);

    // Calculate iterator to next element
    std::optional<iterator> next_iter;
    if (next_in_next_leaf) {
      // Special case: next is in next leaf of result, stays there
      // Return nullopt, let caller use next_leaf pointer
      return {left_sibling, std::nullopt};
    } else if (next_index.has_value()) {
      // Next was in this node, now at left_old_size + index
      const size_t new_index = left_old_size + *next_index;
      if (new_index < left_sibling->data.size()) {
        auto it = left_sibling->data.begin();
        std::advance(it, new_index);
        next_iter = iterator(left_sibling, it);
      }
    }
    // else: no next element to track
    return {left_sibling, next_iter};
  } else {
    // Capture node's minimum key BEFORE transferring children
    const Key &node_min = node->children_are_leaves
                             ? node->leaf_children.begin()->first
                             : node->internal_children.begin()->first;

    // Remove this node from parent
    InternalNode* parent = node->parent;
    assert(parent != nullptr && "Merging non-root node should have parent");

    // Find and remove the entry for this node in parent
    auto& parent_children = parent->internal_children;
    auto it = parent_children.find(node_min);
    assert(it != parent_children.end() && it->second == node &&
           "Node's minimum key should map to node in parent");
    parent_children.erase(it->first);

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

    // Deallocate this node
    deallocate_internal_node(node);

    // Handle parent underflow and potential root reduction
    handle_parent_after_merge<InternalNode>(parent, parent_children);

    // Internal nodes don't track iterators
    return {left_sibling, std::nullopt};
  }
}

// merge_with_right_sibling
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename NodeType>
std::pair<NodeType*, std::optional<typename btree<Key, Value, LeafNodeSize,
                                                   InternalNodeSize, Compare,
                                                   SearchModeT, Allocator>::iterator>>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::merge_with_right_sibling(NodeType* node,
                                           const std::optional<size_t>& next_index,
                                           bool next_in_next_leaf) {
  NodeType* right_sibling = find_right_sibling(node);
  assert(right_sibling != nullptr &&
         "merge_with_right_sibling requires right sibling");

  if constexpr (std::is_same_v<NodeType, LeafNode>) {
    // Capture right sibling's minimum key BEFORE transferring data
    const Key &right_sibling_min = right_sibling->data.begin()->first;

    // Capture old size before merging (needed if next_in_next_leaf)
    const size_type old_size = node->data.size();

    // Remove right sibling from parent
    InternalNode* parent = node->parent;
    assert(parent != nullptr && "Merging non-root leaf should have parent");

    // Find and remove the entry for right sibling in parent
    auto& parent_children = parent->leaf_children;
    auto it = parent_children.find(right_sibling_min);
    assert(it != parent_children.end() && it->second == right_sibling &&
           "Right sibling's minimum key should map to right sibling in parent");
    parent_children.erase(it->first);

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

    // Deallocate right sibling
    deallocate_leaf_node(right_sibling);

    // Handle parent underflow and potential root reduction
    handle_parent_after_merge<LeafNode>(parent, parent_children);

    // Calculate iterator to next element
    std::optional<iterator> next_iter;
    if (next_in_next_leaf) {
      // Special case: next was in right sibling (next leaf), now at index old_size
      auto it = node->data.begin();
      std::advance(it, old_size);
      next_iter = iterator(node, it);
    } else if (next_index.has_value() && *next_index < node->data.size()) {
      // Next was in this node, index unchanged (elements appended to end)
      auto it = node->data.begin();
      std::advance(it, *next_index);
      next_iter = iterator(node, it);
    }
    // else: no next element to track
    return {node, next_iter};
  } else {
    // Capture right sibling's minimum key BEFORE transferring children
    const Key &right_sibling_min =
        right_sibling->children_are_leaves
            ? right_sibling->leaf_children.begin()->first
            : right_sibling->internal_children.begin()->first;

    // Remove right sibling from parent
    InternalNode* parent = node->parent;
    assert(parent != nullptr && "Merging non-root node should have parent");

    // Find and remove the entry for right sibling in parent
    auto& parent_children = parent->internal_children;
    auto it = parent_children.find(right_sibling_min);
    assert(it != parent_children.end() && it->second == right_sibling &&
           "Right sibling's minimum key should map to right sibling in parent");
    parent_children.erase(it->first);

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

    // Deallocate right sibling
    deallocate_internal_node(right_sibling);

    // Handle parent underflow and potential root reduction
    handle_parent_after_merge<InternalNode>(parent, parent_children);

    // Internal nodes don't track iterators
    return {node, std::nullopt};
  }
}

// handle_underflow
template <typename Key, typename Value, std::size_t LeafNodeSize,
          std::size_t InternalNodeSize, typename Compare, SearchMode SearchModeT,
          typename Allocator>
  requires ComparatorCompatible<Key, Compare>
template <typename NodeType>
std::pair<NodeType*, std::optional<typename btree<Key, Value, LeafNodeSize,
                                                   InternalNodeSize, Compare,
                                                   SearchModeT, Allocator>::iterator>>
btree<Key, Value, LeafNodeSize, InternalNodeSize, Compare, SearchModeT,
      Allocator>::handle_underflow(NodeType* node,
                                   const std::optional<size_t>& next_index,
                                   bool next_in_next_leaf) {
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
  auto [result_node, next_iter] = borrow_from_left_sibling(node, next_index, next_in_next_leaf);
  if (result_node != nullptr) {
    return {result_node, next_iter};
  }

  // Try to borrow from right sibling
  std::tie(result_node, next_iter) = borrow_from_right_sibling(node, next_index, next_in_next_leaf);
  if (result_node != nullptr) {
    return {result_node, next_iter};
  }

  // Can't borrow - must merge
  // Prefer merging with left sibling if it exists
  NodeType* left_sibling = find_left_sibling(node);
  if (left_sibling != nullptr) {
    return merge_with_left_sibling(node, next_index, next_in_next_leaf);
  } else {
    // No left sibling - merge with right
    return merge_with_right_sibling(node, next_index, next_in_next_leaf);
  }
}

}  // namespace kressler::fast_containers
