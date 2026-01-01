// Copyright (c) 2025 Bryan Kressler
//
// SPDX-License-Identifier: BSD-3-Clause

// ordered_array.ipp - Implementation details for ordered_array
// This file is included at the end of ordered_array.hpp
// DO NOT include this file directly

namespace kressler::fast_containers {

// ============================================================================
// Constructors and Assignment Operators
// ============================================================================

/**
 * Copy constructor - creates a deep copy of another ordered array
 * Complexity: O(n) where n is the size of the other array
 *
 * @param other The ordered array to copy from
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
ordered_array<Key, Value, Length, Compare, SearchModeT>::ordered_array(
    const ordered_array& other)
    : size_(other.size_), comp_(other.comp_) {
  // Copy only the active elements
  if (size_ > 0) {
    std::copy(other.keys_.begin(), other.keys_.begin() + size_, keys_.begin());
    std::copy(other.values_.begin(), other.values_.begin() + size_,
              values_.begin());
  }
}

/**
 * Move constructor - transfers ownership from another ordered array
 * Note: Due to inline storage, this is still O(n) as data must be copied.
 * The move only provides benefit for non-POD Value types.
 *
 * @param other The ordered array to move from (will be left empty)
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
ordered_array<Key, Value, Length, Compare, SearchModeT>::ordered_array(
    ordered_array&& other) noexcept
    : size_(other.size_), comp_(std::move(other.comp_)) {
  // Move the active elements
  if (size_ > 0) {
    std::move(other.keys_.data(), other.keys_.data() + size_,
                      keys_.data());
    std::move(other.values_.data(), other.values_.data() + size_,
                      values_.data());
  }
  // Leave other in valid empty state
  other.size_ = 0;
}

/**
 * Copy assignment operator - replaces contents with a copy of another array
 * Complexity: O(n) where n is the size of the other array
 *
 * @param other The ordered array to copy from
 * @return Reference to this array
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
ordered_array<Key, Value, Length, Compare, SearchModeT>&
ordered_array<Key, Value, Length, Compare, SearchModeT>::operator=(
    const ordered_array& other) {
  if (this != &other) {
    size_ = other.size_;
    // Copy only the active elements
    if (size_ > 0) {
      std::copy(other.keys_.begin(), other.keys_.begin() + size_,
                keys_.begin());
      std::copy(other.values_.begin(), other.values_.begin() + size_,
                values_.begin());
    }
  }
  return *this;
}

/**
 * Move assignment operator - replaces contents by moving from another array
 * Note: Due to inline storage, this is still O(n) as data must be copied.
 * The move only provides benefit for non-POD Value types.
 *
 * @param other The ordered array to move from (will be left empty)
 * @return Reference to this array
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
ordered_array<Key, Value, Length, Compare, SearchModeT>&
ordered_array<Key, Value, Length, Compare, SearchModeT>::operator=(
    ordered_array&& other) noexcept {
  if (this != &other) {
    size_ = other.size_;
    // Move the active elements
    if (size_ > 0) {
      std::move(other.keys_.data(), other.keys_.data() + size_,
                        keys_.data());
      std::move(other.values_.data(), other.values_.data() + size_,
                        values_.data());
    }
    // Leave other in valid empty state
    other.size_ = 0;
  }
  return *this;
}

// ============================================================================
// Insert and Erase Operations
// ============================================================================

/**
 * Insert a new key-value pair into the array in sorted order.
 *
 * @param key The key to insert
 * @param value The value to insert
 * @return A pair consisting of an iterator to the inserted element (or to the
 *         element that prevented insertion) and a bool indicating whether
 *         insertion took place (true if inserted, false if key already exists)
 * @throws std::runtime_error if the array is full
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
std::pair<typename ordered_array<Key, Value, Length, Compare, SearchModeT>::iterator,
          bool>
ordered_array<Key, Value, Length, Compare, SearchModeT>::insert(
    const Key& key, const Value& value) {
  // Early check for better error message (helper also checks)
  if (size_ >= Length) {
    throw std::runtime_error("Cannot insert: array is full");
  }

  // Find the position where the key should be inserted
  const size_type idx = lower_bound_key(key) - keys_.begin();

  return insert_impl(
      idx, key,
      [this](size_type idx) {
        // Key exists - don't modify, just return
        return std::pair{iterator(this, idx), false};
      },
      [this, &value](size_type idx) {
        // Insert value via assignment
        values_[idx] = value;
      });
}

/**
 * Inserts a key-value pair using a position hint.
 * The hint should be an iterator returned by lower_bound(key).
 * If the hint is correct, this avoids searching again.
 *
 * @param hint Iterator hint to the insertion position
 * @param key The key to insert
 * @param value The value to insert
 * @return Pair of iterator to inserted/existing element and bool indicating
 * success
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
std::pair<typename ordered_array<Key, Value, Length, Compare, SearchModeT>::iterator,
          bool>
ordered_array<Key, Value, Length, Compare, SearchModeT>::insert_hint(
    iterator hint, const Key& key, const Value& value) {
  // Early check for better error message (helper also checks)
  if (size_ >= Length) {
    throw std::runtime_error("Cannot insert: array is full");
  }

  // Get the index from the hint
  const size_type idx = hint.index_;

  // Verify the hint is valid (optional debug check)
  // In release builds, we trust the hint for performance
  assert(idx <= size_);
  assert(idx == size_ || !comp_(keys_[idx], key));
  assert(idx == 0 || comp_(keys_[idx - 1], key));

  return insert_impl(
      idx, key,
      [this](size_type idx) {
        // Key exists - don't modify, just return
        return std::pair{iterator(this, idx), false};
      },
      [this, &value](size_type idx) {
        // Insert value via assignment
        values_[idx] = value;
      });
}

/**
 * try_emplace - constructs value in-place if key doesn't exist.
 * Key advantage: avoids value construction when key already exists.
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
template <typename... Args>
std::pair<typename ordered_array<Key, Value, Length, Compare, SearchModeT>::iterator,
          bool>
ordered_array<Key, Value, Length, Compare, SearchModeT>::try_emplace(
    const Key& key, Args&&... args) {
  // Early check for better error message (helper also checks)
  if (size_ >= Length) {
    throw std::runtime_error("Cannot insert: array is full");
  }

  // Find the position where the key should be inserted
  const size_type idx = lower_bound_key(key) - keys_.begin();

  return insert_impl(
      idx, key,
      [this](size_type idx) {
        // Key exists - CRITICAL: don't construct value, just return
        return std::pair{iterator(this, idx), false};
      },
      [this, &args...](size_type idx) {
        // Construct value IN-PLACE using provided arguments
        // This is the key difference from insert() - no temporary value!
        std::construct_at(&values_[idx], std::forward<Args>(args)...);
      });
}

/**
 * insert_or_assign - inserts a new element or assigns to an existing one.
 * Key difference from insert(): overwrites existing values instead of leaving
 * them unchanged.
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
template <typename M>
std::pair<typename ordered_array<Key, Value, Length, Compare, SearchModeT>::iterator,
          bool>
ordered_array<Key, Value, Length, Compare, SearchModeT>::insert_or_assign(const Key& key, M&& value) {
  // Find the position where the key should be inserted
  const size_type idx = lower_bound_key(key) - keys_.begin();

  return insert_impl(
      idx, key,
      [this, &value](size_type idx) {
        // Key exists - ASSIGN the new value
        values_[idx] = std::forward<M>(value);
        return std::pair{iterator(this, idx), false};  // false = assignment
      },
      [this, &value](size_type idx) {
        // Insert new value
        values_[idx] = std::forward<M>(value);
      });
}

/**
 * Erase an element by iterator.
 * More efficient than key-based erase when you already have an iterator.
 *
 * @param pos Iterator to the element to erase
 * @return Iterator to the element following the erased element
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
typename ordered_array<Key, Value, Length, Compare, SearchModeT>::iterator
ordered_array<Key, Value, Length, Compare, SearchModeT>::erase(iterator pos) {
  assert(pos != end() && "Cannot erase end iterator");

  const size_type idx = pos.index();
  // Shift elements to the left to fill the gap in both arrays
  std::move(&keys_[idx + 1], &keys_[size_], &keys_[idx]);
  std::move(&values_[idx + 1], &values_[size_], &values_[idx]);
  --size_;

  // Return iterator to element following the erased element
  // (same index, since everything shifted left)
  return iterator(this, idx);
}

/**
 * Erase an element by key.
 * Implemented as find() + iterator erase().
 * If the element does not exist, does nothing.
 *
 * @param key The key to erase
 * @return The number of elements erased (0 or 1)
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
typename ordered_array<Key, Value, Length, Compare, SearchModeT>::size_type
ordered_array<Key, Value, Length, Compare, SearchModeT>::erase(
    const Key& key) {
  const auto it = find(key);
  if (it != end()) {
    erase(it);  // Call iterator-based erase
    return 1;
  }
  return 0;
}

/**
 * Access or insert an element using subscript operator.
 * If the key exists, returns a reference to its value.
 * If the key doesn't exist, inserts it with a default-constructed value.
 *
 * @param key The key to access or insert
 * @return Reference to the value associated with the key
 * @throws std::runtime_error if insertion is needed but array is full
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
Value& ordered_array<Key, Value, Length, Compare, SearchModeT>::operator[](
    const Key& key) {
  const auto it = find(key);
  if (it != end()) {
    return it.value_ref();
  }

  // Need to insert - check if there's space
  if (size_ >= Length) {
    throw std::runtime_error("Cannot insert: array is full");
  }

  // Find insertion position
  const auto pos = lower_bound_key(key);
  size_type idx = pos - keys_.begin();

  // Shift elements to make space in both arrays
  std::move_backward(&keys_[idx], &keys_[size_], &keys_[size_ + 1]);
  std::move_backward(&values_[idx], &values_[size_], &values_[size_ + 1]);

  // Insert with default-constructed value
  keys_[idx] = key;
  values_[idx] = Value{};
  ++size_;

  return values_[idx];
}

// ============================================================================
// Find and Bound Operations
// ============================================================================

/**
 * Find an element by key.
 *
 * @param key The key to search for
 * @return Iterator to the found element, or end() if not found
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
typename ordered_array<Key, Value, Length, Compare, SearchModeT>::iterator
ordered_array<Key, Value, Length, Compare, SearchModeT>::find(
    const Key& key) {
  const auto pos = lower_bound_key(key);
  const size_type idx = pos - keys_.begin();

  if (idx < size_ && keys_[idx] == key) {
    return iterator(this, idx);
  }
  return end();
}

/**
 * Find an element by key (const version).
 *
 * @param key The key to search for
 * @return Const iterator to the found element, or end() if not found
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
typename ordered_array<Key, Value, Length, Compare, SearchModeT>::const_iterator
ordered_array<Key, Value, Length, Compare, SearchModeT>::find(
    const Key& key) const {
  const auto pos = lower_bound_key(key);
  const size_type idx = pos - keys_.begin();

  if (idx < size_ && keys_[idx] == key) {
    return const_iterator(this, idx);
  }
  return end();
}

/**
 * Find the first element not less than the given key.
 *
 * @param key The key to search for
 * @return Iterator to the first element >= key, or end() if all elements < key
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
typename ordered_array<Key, Value, Length, Compare, SearchModeT>::iterator
ordered_array<Key, Value, Length, Compare, SearchModeT>::lower_bound(
    const Key& key) {
  const auto pos = lower_bound_key(key);
  const size_type idx = pos - keys_.begin();
  return iterator(this, idx);
}

/**
 * Find the first element not less than the given key (const version).
 *
 * @param key The key to search for
 * @return Const iterator to the first element >= key, or end() if all elements
 * < key
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
typename ordered_array<Key, Value, Length, Compare, SearchModeT>::const_iterator
ordered_array<Key, Value, Length, Compare, SearchModeT>::lower_bound(
    const Key& key) const {
  const auto pos = lower_bound_key(key);
  const size_type idx = pos - keys_.begin();
  return const_iterator(this, idx);
}

/**
 * Find the first element greater than the given key.
 *
 * @param key The key to search for
 * @return Iterator to the first element > key, or end() if all elements <= key
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
typename ordered_array<Key, Value, Length, Compare, SearchModeT>::iterator
ordered_array<Key, Value, Length, Compare, SearchModeT>::upper_bound(
    const Key& key) {
  auto it = lower_bound(key);
  if (it != end() && it->first == key) {
    ++it;
  }
  return it;
}

/**
 * Find the first element greater than the given key (const version).
 *
 * @param key The key to search for
 * @return Const iterator to the first element > key, or end() if all elements
 * <= key
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
typename ordered_array<Key, Value, Length, Compare, SearchModeT>::const_iterator
ordered_array<Key, Value, Length, Compare, SearchModeT>::upper_bound(
    const Key& key) const {
  auto it = lower_bound(key);
  if (it != end() && it->first == key) {
    ++it;
  }
  return it;
}

// ============================================================================
// Split and Transfer Operations
// ============================================================================

/**
 * Split this array at the given position, moving elements to output array.
 * Elements [begin(), pos) remain in this array.
 * Elements [pos, end()) are moved to the output array.
 *
 * Note: Invalidates all iterators to both arrays.
 *
 * @tparam OutputLength The capacity of the output array
 * @param pos Iterator to the first element to move to output
 * @param output Destination array (must be empty)
 * @throws std::runtime_error if output is not empty or has insufficient
 * capacity
 *
 * Complexity: O(n) where n is the number of elements moved
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
template <std::size_t OutputLength>
void ordered_array<Key, Value, Length, Compare, SearchModeT>::split_at(
    iterator pos,
    ordered_array<Key, Value, OutputLength, Compare, SearchModeT>& output) {
  // Debug assertion: iterator must belong to this array
  assert(pos.array_ == this && "Iterator does not belong to this array");

  // Runtime check: output must be empty
  if (!output.empty()) {
    throw std::runtime_error("Cannot split: output array is not empty");
  }

  // Calculate the split index
  const size_type split_idx = pos.index_;

  // Calculate how many elements to move
  const size_type num_to_move = size_ - split_idx;

  // Check if output has sufficient capacity
  if (num_to_move > output.capacity()) {
    throw std::runtime_error(
        "Cannot split: output array has insufficient capacity");
  }

  // Move elements from [split_idx, size_) to output
  if (num_to_move > 0) {
    std::move(&keys_[split_idx], &keys_[size_], output.keys_.data());
    std::move(&values_[split_idx], &values_[size_],
                      output.values_.data());
    output.size_ = num_to_move;
  }

  // Update this array's size
  size_ = split_idx;
}

/**
 * Transfer elements from the beginning of source array to the end of
 * this array (append behavior).
 *
 * Precondition (debug mode only): All keys currently in this array must be
 * less than all keys in the transferred prefix.
 *
 * Note: Invalidates all iterators to both arrays.
 *
 * @tparam SourceLength The capacity of the source array
 * @param source The array to transfer from (will have elements removed)
 * @param count Number of elements to transfer from the beginning of source
 * @throws std::runtime_error if this array has insufficient capacity or
 * count exceeds source size
 *
 * Complexity: O(m) where m is count
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
template <std::size_t SourceLength>
void ordered_array<Key, Value, Length, Compare, SearchModeT>::
    transfer_prefix_from(
        ordered_array<Key, Value, SourceLength, Compare, SearchModeT>& source,
        size_type count) {
  // Validate count
  if (count > source.size_) {
    throw std::runtime_error(
        "Cannot transfer prefix: count exceeds source size");
  }

  // Check capacity constraint
  if (size_ + count > Length) {
    throw std::runtime_error(
        "Cannot transfer prefix: insufficient capacity in destination");
  }

  // Debug assertion: ordering precondition
  // All keys in this array must be < all keys in source prefix
  assert((empty() || count == 0 || comp_(keys_[size_ - 1], source.keys_[0])) &&
         "Ordering precondition violated: all keys in this must be < all "
         "keys in source prefix");

  if (count == 0) {
    return;
  }

  // Copy prefix from source to end of this array (append)
  std::move(source.keys_.data(), source.keys_.data() + count,
                    &keys_[size_]);
  std::move(source.values_.data(), source.values_.data() + count,
                    &values_[size_]);

  // Shift remaining elements in source left to fill the gap
  if (count < source.size_) {
    std::move(&source.keys_[count], &source.keys_[source.size_],
                      source.keys_.data());
    std::move(&source.values_[count], &source.values_[source.size_],
                      source.values_.data());
  }

  // Update sizes
  size_ += count;
  source.size_ -= count;
}

/**
 * Transfer elements from the end of source array to the beginning of this
 * array (prepend behavior). Elements from this array are shifted right to
 * make space.
 *
 * Precondition (debug mode only): All keys in the transferred suffix must be
 * less than all keys currently in this array.
 *
 * Note: Invalidates all iterators to both arrays.
 *
 * @tparam SourceLength The capacity of the source array
 * @param source The array to transfer from (will have elements removed)
 * @param count Number of elements to transfer from the end of source
 * @throws std::runtime_error if this array has insufficient capacity or
 * count exceeds source size
 *
 * Complexity: O(n + m) where n is the current size of this array and m is
 * count
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
template <std::size_t SourceLength>
void ordered_array<Key, Value, Length, Compare, SearchModeT>::
    transfer_suffix_from(
        ordered_array<Key, Value, SourceLength, Compare, SearchModeT>& source,
        size_type count) {
  // Validate count
  if (count > source.size_) {
    throw std::runtime_error(
        "Cannot transfer suffix: count exceeds source size");
  }

  // Check capacity constraint
  if (size_ + count > Length) {
    throw std::runtime_error(
        "Cannot transfer suffix: insufficient capacity in destination");
  }

  // Debug assertion: ordering precondition
  // All keys in source suffix must be < all keys in this array
  assert((empty() || count == 0 || comp_(source.keys_[source.size_ - 1], keys_[0])) &&
         "Ordering precondition violated: all keys in source suffix must be "
         "< all keys in this");

  if (count == 0) {
    return;
  }

  // Shift current elements right to make space for incoming suffix
  if (size_ > 0) {
    std::move_backward(&keys_[0], &keys_[size_], &keys_[size_ + count]);
    std::move_backward(&values_[0], &values_[size_], &values_[size_ + count]);
  }

  // Calculate starting position of suffix in source
  const size_type suffix_start = source.size_ - count;

  // Copy suffix from source to beginning of this array (prepend)
  std::move(&source.keys_[suffix_start], &source.keys_[source.size_],
                    keys_.data());
  std::move(&source.values_[suffix_start],
                    &source.values_[source.size_], values_.data());

  // Update sizes (no need to shift source - we took from the end)
  size_ += count;
  source.size_ -= count;
}

// ============================================================================
// SIMD Search Implementations (AVX2)
// ============================================================================

#ifdef __AVX2__
#include "ordered_array_simd.ipp"
#endif

// ============================================================================
// Private Helper Methods
// ============================================================================

/**
 * Common implementation for all insert operations.
 * Handles key existence check, shifting, and insertion logic.
 *
 * @param idx Position where key should be inserted
 * @param key Key to insert
 * @param on_exists Callback for when key exists: (size_type) -> pair<iterator, bool>
 *                  Can modify the existing value (insert_or_assign) or just return
 * @param on_insert Callback to insert value: (size_type) -> void
 *                  Uses assignment (insert) or construct_at (try_emplace)
 * @return Pair of iterator to element and bool indicating insertion success
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
template <typename OnExists, typename OnInsert>
std::pair<typename ordered_array<Key, Value, Length, Compare, SearchModeT>::iterator,
          bool>
ordered_array<Key, Value, Length, Compare, SearchModeT>::insert_impl(
    size_type idx,
    const Key& key,
    OnExists&& on_exists,
    OnInsert&& on_insert) {
  // Check if key exists at this position
  if (idx < size_ && keys_[idx] == key) {
    return on_exists(idx);  // Let caller decide what to do (return or assign)
  }

  // Key doesn't exist - insert it
  if (size_ >= Length) {
    throw std::runtime_error("Cannot insert: array is full");
  }

  // Shift elements to the right to make space in both arrays
  std::move_backward(&keys_[idx], &keys_[size_], &keys_[size_ + 1]);
  std::move_backward(&values_[idx], &values_[size_], &values_[size_ + 1]);

  // Insert the key
  keys_[idx] = key;

  // Let caller insert the value (assignment vs construct_at)
  on_insert(idx);

  ++size_;

  return {iterator(this, idx), true};
}

/**
 * Find the insertion position for a key using the configured search mode.
 * Returns an iterator to the first element not less than the given key.
 *
 * @param key The key to search for
 * @return Iterator to the insertion position
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
auto ordered_array<Key, Value, Length, Compare, SearchModeT>::lower_bound_key(
    const Key& key) const {
  if constexpr (SearchModeT == SearchMode::Linear) {
    // Linear search: scan from beginning until we find key >= search key
    auto it = keys_.begin();
    auto end_it = keys_.begin() + size_;
    while (it != end_it && comp_(*it, key)) {
      ++it;
    }
    return it;
  } else if constexpr (SearchModeT == SearchMode::Binary) {
    // Binary search using standard library
    return std::lower_bound(keys_.begin(), keys_.begin() + size_, key, comp_);
  } else if constexpr (SearchModeT == SearchMode::SIMD) {
    // SIMD-accelerated linear search
#ifdef __AVX2__
    if constexpr (SIMDSearchable<Key>) {
      if constexpr (sizeof(Key) == 1) {
        return simd_lower_bound_1byte(key);
      } else if constexpr (sizeof(Key) == 2) {
        return simd_lower_bound_2byte(key);
      } else if constexpr (sizeof(Key) == 4) {
        return simd_lower_bound_4byte(key);
      } else if constexpr (sizeof(Key) == 8) {
        return simd_lower_bound_8byte(key);
      }
      // Note: SIMDSearchable only accepts primitive types (1, 2, 4, or 8 bytes)
      // so we never reach here
    } else {
      // Fallback to regular linear search if type doesn't support SIMD
      auto it = keys_.begin();
      auto end_it = keys_.begin() + size_;
      while (it != end_it && comp_(*it, key)) {
        ++it;
      }
      return it;
    }
#else
    // AVX2 not available, fall back to linear search
    auto it = keys_.begin();
    auto end_it = keys_.begin() + size_;
    while (it != end_it && comp_(*it, key)) {
      ++it;
    }
    return it;
#endif
  } else {
    static_assert(SearchModeT == SearchMode::Binary ||
                      SearchModeT == SearchMode::Linear ||
                      SearchModeT == SearchMode::SIMD,
                  "Invalid SearchMode");
  }
}

}  // namespace kressler::fast_containers
