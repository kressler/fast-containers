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
  // Check if array is full
  if (size_ >= Length) {
    throw std::runtime_error("Cannot insert: array is full");
  }

  // Find the position where the key should be inserted
  const auto pos = lower_bound_key(key);

  // Calculate index
  const size_type idx = pos - keys_.begin();

  // Check if key already exists
  if (idx < size_ && keys_[idx] == key) {
    return {iterator(this, idx), false};
  }

  // Shift elements to the right to make space in both arrays
  std::move_backward(&keys_[idx], &keys_[size_], &keys_[size_ + 1]);
  std::move_backward(&values_[idx], &values_[size_], &values_[size_ + 1]);

  // Insert the new elements
  keys_[idx] = key;
  values_[idx] = value;
  ++size_;

  return {iterator(this, idx), true};
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
  // Check if array is full
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

  // Check if key already exists at the hint position
  if (idx < size_ && keys_[idx] == key) {
    return {iterator(this, idx), false};
  }

  // Shift elements to the right to make space in both arrays
  std::move_backward(&keys_[idx], &keys_[size_], &keys_[size_ + 1]);
  std::move_backward(&values_[idx], &values_[size_], &values_[size_ + 1]);

  // Insert the new elements
  keys_[idx] = key;
  values_[idx] = value;
  ++size_;

  return {iterator(this, idx), true};
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
  // Check if array is full
  if (size_ >= Length) {
    throw std::runtime_error("Cannot insert: array is full");
  }

  // Find the position where the key should be inserted
  const auto pos = lower_bound_key(key);

  // Calculate index
  const size_type idx = pos - keys_.begin();

  // Check if key already exists - CRITICAL: don't construct value!
  if (idx < size_ && keys_[idx] == key) {
    return {iterator(this, idx), false};
  }

  // Shift elements to the right to make space in both arrays
  std::move_backward(&keys_[idx], &keys_[size_], &keys_[size_ + 1]);
  std::move_backward(&values_[idx], &values_[size_], &values_[size_ + 1]);

  // Insert the key
  keys_[idx] = key;

  // Construct the value IN-PLACE using the provided arguments
  // This is the key difference from insert() - no temporary value created!
  std::construct_at(&values_[idx], std::forward<Args>(args)...);

  ++size_;

  return {iterator(this, idx), true};
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
  const auto pos = lower_bound_key(key);

  // Calculate index
  const size_type idx = pos - keys_.begin();

  // Check if key already exists - if so, ASSIGN the new value
  if (idx < size_ && keys_[idx] == key) {
    values_[idx] = std::forward<M>(value);
    return {iterator(this, idx), false};  // false = assignment, not insertion
  }

  // Key doesn't exist - insert new element
  // Check if array is full
  if (size_ >= Length) {
    throw std::runtime_error("Cannot insert: array is full");
  }

  // Shift elements to the right to make space in both arrays
  std::move_backward(&keys_[idx], &keys_[size_], &keys_[size_ + 1]);
  std::move_backward(&values_[idx], &values_[size_], &values_[size_ + 1]);

  // Insert the key and value
  keys_[idx] = key;
  values_[idx] = std::forward<M>(value);

  ++size_;

  return {iterator(this, idx), true};  // true = insertion
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
// Private Helper Methods - SIMD Search
// ============================================================================

#ifdef __AVX2__
/**
 * SIMD-accelerated linear search for 8-bit keys
 * Supports: int8_t (signed), uint8_t (unsigned), char, signed char, unsigned
 * char Compares 32 keys at a time using AVX2
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
template <typename K>
  requires(sizeof(K) == 1)
auto ordered_array<Key, Value, Length, Compare,
                   SearchModeT>::simd_lower_bound_1byte(const K& key) const {
  static_assert(SimdPrimitive<K>,
                "1-byte SIMD search requires primitive type (int8_t, uint8_t, "
                "char, signed char, unsigned char)");

  // Convert key to int8_t for comparison
  int8_t key_bits;
  std::memcpy(&key_bits, &key, sizeof(K));

  // Check if type is unsigned
  constexpr bool is_unsigned = std::is_unsigned_v<K>;
  constexpr bool is_ascending = std::is_same_v<Compare, std::less<Key>>;

  if constexpr (is_unsigned) {
    // Flip sign bit: converts unsigned comparison to signed comparison
    key_bits ^= static_cast<int8_t>(0x80);
  }

  __m256i search_vec_256 = _mm256_set1_epi8(key_bits);

  size_type i = 0;
  // Process 32 keys at a time with full AVX2
  for (; i + 32 <= size_; i += 32) {
    __m256i keys_vec =
        _mm256_load_si256(reinterpret_cast<const __m256i*>(&keys_[i]));

    // For unsigned, flip sign bits before comparison
    if constexpr (is_unsigned) {
      __m256i flip_mask = _mm256_set1_epi8(static_cast<int8_t>(0x80));
      keys_vec = _mm256_xor_si256(keys_vec, flip_mask);
    }

    // For ascending (std::less): find first position where key >= search_key
    //   Check: keys_vec < search_vec, i.e., search_vec > keys_vec
    // For descending (std::greater): find first position where key <= search_key
    //   Check: keys_vec > search_vec
    __m256i cmp;
    if constexpr (is_ascending) {
      cmp = _mm256_cmpgt_epi8(search_vec_256, keys_vec);
    } else {
      cmp = _mm256_cmpgt_epi8(keys_vec, search_vec_256);
    }

    // Check if any key satisfies the termination condition
    int mask = _mm256_movemask_epi8(cmp);
    if (mask != static_cast<int>(0xFFFFFFFF)) {
      // Each 1-byte element contributes 1 bit to the mask
      size_type offset = std::countr_one(static_cast<unsigned int>(mask));
      return keys_.begin() + i + offset;
    }
  }

  // Process 16 keys at a time with half AVX2 (128-bit SSE)
  if (i + 16 <= size_) {
    __m128i search_vec_128 = _mm_set1_epi8(key_bits);
    __m128i keys_vec =
        _mm_load_si128(reinterpret_cast<const __m128i*>(&keys_[i]));

    if constexpr (is_unsigned) {
      __m128i flip_mask = _mm_set1_epi8(static_cast<int8_t>(0x80));
      keys_vec = _mm_xor_si128(keys_vec, flip_mask);
    }

    __m128i cmp;
    if constexpr (is_ascending) {
      cmp = _mm_cmpgt_epi8(search_vec_128, keys_vec);
    } else {
      cmp = _mm_cmpgt_epi8(keys_vec, search_vec_128);
    }

    int mask = _mm_movemask_epi8(cmp);
    if (mask != static_cast<int>(0xFFFF)) {
      size_type offset = std::countr_one(static_cast<unsigned int>(mask));
      return keys_.begin() + i + offset;
    }
    i += 16;
  }

  // Handle remaining 0-15 keys with scalar
  while (i < size_ && comp_(keys_[i], key)) {
    ++i;
  }

  return keys_.begin() + i;
}

/**
 * SIMD-accelerated linear search for 16-bit keys
 * Supports: int16_t (signed), uint16_t (unsigned), short, unsigned short
 * Compares 16 keys at a time using AVX2
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
template <typename K>
  requires(sizeof(K) == 2)
auto ordered_array<Key, Value, Length, Compare,
                   SearchModeT>::simd_lower_bound_2byte(const K& key) const {
  static_assert(SimdPrimitive<K>,
                "2-byte SIMD search requires primitive type (int16_t, "
                "uint16_t, short, unsigned short)");

  // Convert key to int16_t for comparison
  int16_t key_bits;
  std::memcpy(&key_bits, &key, sizeof(K));

  // Check if type is unsigned
  constexpr bool is_unsigned = std::is_unsigned_v<K>;
  constexpr bool is_ascending = std::is_same_v<Compare, std::less<Key>>;

  if constexpr (is_unsigned) {
    // Flip sign bit: converts unsigned comparison to signed comparison
    key_bits ^= static_cast<int16_t>(0x8000);
  }

  __m256i search_vec_256 = _mm256_set1_epi16(key_bits);

  size_type i = 0;
  // Process 16 keys at a time with full AVX2
  for (; i + 16 <= size_; i += 16) {
    __m256i keys_vec =
        _mm256_load_si256(reinterpret_cast<const __m256i*>(&keys_[i]));

    // For unsigned, flip sign bits before comparison
    if constexpr (is_unsigned) {
      __m256i flip_mask = _mm256_set1_epi16(static_cast<int16_t>(0x8000));
      keys_vec = _mm256_xor_si256(keys_vec, flip_mask);
    }

    // For ascending (std::less): find first position where key >= search_key
    //   Check: keys_vec < search_vec, i.e., search_vec > keys_vec
    // For descending (std::greater): find first position where key <= search_key
    //   Check: keys_vec > search_vec
    __m256i cmp;
    if constexpr (is_ascending) {
      cmp = _mm256_cmpgt_epi16(search_vec_256, keys_vec);
    } else {
      cmp = _mm256_cmpgt_epi16(keys_vec, search_vec_256);
    }

    // Check if any key satisfies the termination condition
    int mask = _mm256_movemask_epi8(cmp);
    if (mask != static_cast<int>(0xFFFFFFFF)) {
      // Each 2-byte element contributes 2 bits to the mask
      size_type offset = std::countr_one(static_cast<unsigned int>(mask)) / 2;
      return keys_.begin() + i + offset;
    }
  }

  // Process 8 keys at a time with half AVX2 (128-bit SSE)
  if (i + 8 <= size_) {
    __m128i search_vec_128 = _mm_set1_epi16(key_bits);
    __m128i keys_vec =
        _mm_load_si128(reinterpret_cast<const __m128i*>(&keys_[i]));

    if constexpr (is_unsigned) {
      __m128i flip_mask = _mm_set1_epi16(static_cast<int16_t>(0x8000));
      keys_vec = _mm_xor_si128(keys_vec, flip_mask);
    }

    __m128i cmp;
    if constexpr (is_ascending) {
      cmp = _mm_cmpgt_epi16(search_vec_128, keys_vec);
    } else {
      cmp = _mm_cmpgt_epi16(keys_vec, search_vec_128);
    }

    int mask = _mm_movemask_epi8(cmp);
    if (mask != static_cast<int>(0xFFFF)) {
      size_type offset = std::countr_one(static_cast<unsigned int>(mask)) / 2;
      return keys_.begin() + i + offset;
    }
    i += 8;
  }

  // Process 4 keys at a time with 64-bit SIMD
  if (i + 4 <= size_) {
    __m128i search_vec_128 = _mm_set1_epi16(key_bits);
    __m128i keys_vec = _mm_castpd_si128(
        _mm_load_sd(reinterpret_cast<const double*>(&keys_[i])));

    if constexpr (is_unsigned) {
      __m128i flip_mask = _mm_set1_epi16(static_cast<int16_t>(0x8000));
      keys_vec = _mm_xor_si128(keys_vec, flip_mask);
    }

    __m128i cmp;
    if constexpr (is_ascending) {
      cmp = _mm_cmpgt_epi16(search_vec_128, keys_vec);
    } else {
      cmp = _mm_cmpgt_epi16(keys_vec, search_vec_128);
    }

    int mask = _mm_movemask_epi8(cmp);
    if ((mask & 0xFF) != 0xFF) {
      size_type offset = std::countr_one(static_cast<unsigned int>(mask)) / 2;
      return keys_.begin() + i + offset;
    }
    i += 4;
  }

  // Handle remaining 0-3 keys with scalar
  while (i < size_ && comp_(keys_[i], key)) {
    ++i;
  }

  return keys_.begin() + i;
}

/**
 * SIMD-accelerated linear search for 32-bit keys
 * Supports: int32_t (signed), uint32_t (unsigned), float
 * Compares 8 keys at a time using AVX2
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
template <typename K>
  requires(sizeof(K) == 4)
auto ordered_array<Key, Value, Length, Compare,
                   SearchModeT>::simd_lower_bound_4byte(const K& key) const {
  static_assert(SimdPrimitive<K>,
                "4-byte SIMD search requires primitive type (int32_t, "
                "uint32_t, int, unsigned int, float)");

  if constexpr (std::is_same_v<K, float>) {
    // Use floating-point comparison instructions
    __m256 search_vec_256 = _mm256_set1_ps(key);

    // Detect comparator type at compile-time
    constexpr bool is_ascending = std::is_same_v<Compare, std::less<Key>>;

    size_type i = 0;
    // Process 8 floats at a time with AVX2
    for (; i + 8 <= size_; i += 8) {
      __m256 keys_vec =
          _mm256_load_ps(reinterpret_cast<const float*>(&keys_[i]));

      // For ascending (std::less): find first position where key >= search_key
      //   Check: keys_vec < search_vec, find first false
      // For descending (std::greater): find first position where key <= search_key
      //   Check: keys_vec > search_vec, find first false
      __m256 cmp;
      if constexpr (is_ascending) {
        // _CMP_LT_OQ: less-than, ordered, quiet (matches C++ operator<)
        cmp = _mm256_cmp_ps(keys_vec, search_vec_256, _CMP_LT_OQ);
      } else {
        // _CMP_GT_OQ: greater-than, ordered, quiet (matches C++ operator>)
        cmp = _mm256_cmp_ps(keys_vec, search_vec_256, _CMP_GT_OQ);
      }

      // movemask_ps returns 8 bits (one per float)
      int mask = _mm256_movemask_ps(cmp);
      if (mask != 0xFF) {
        // Found first position where comparison is false
        size_type offset = std::countr_one(static_cast<unsigned int>(mask));
        return keys_.begin() + i + offset;
      }
    }

    // Process 4 floats at a time with SSE
    if (i + 4 <= size_) {
      __m128 search_vec_128 = _mm_set1_ps(key);
      __m128 keys_vec = _mm_load_ps(reinterpret_cast<const float*>(&keys_[i]));

      __m128 cmp;
      if constexpr (is_ascending) {
        cmp = _mm_cmp_ps(keys_vec, search_vec_128, _CMP_LT_OQ);
      } else {
        cmp = _mm_cmp_ps(keys_vec, search_vec_128, _CMP_GT_OQ);
      }

      int mask = _mm_movemask_ps(cmp);
      if (mask != 0x0F) {
        size_type offset = std::countr_one(static_cast<unsigned int>(mask));
        return keys_.begin() + i + offset;
      }
      i += 4;
    }

    // Handle remaining 0-3 keys with scalar
    while (i < size_ && comp_(keys_[i], key)) {
      ++i;
    }
    return keys_.begin() + i;

  } else {
    // Integer types use integer comparison
    int32_t key_bits;
    std::memcpy(&key_bits, &key, sizeof(K));

    // Check if type is unsigned (uint32_t or unsigned int)
    constexpr bool is_unsigned = std::is_unsigned_v<K>;
    constexpr bool is_ascending = std::is_same_v<Compare, std::less<Key>>;

    if constexpr (is_unsigned) {
      // Flip sign bit: converts unsigned comparison to signed comparison
      key_bits ^= static_cast<int32_t>(0x80000000u);
    }

    __m256i search_vec_256 = _mm256_set1_epi32(key_bits);

    size_type i = 0;
    // Process 8 keys at a time with full AVX2
    for (; i + 8 <= size_; i += 8) {
      __m256i keys_vec =
          _mm256_load_si256(reinterpret_cast<const __m256i*>(&keys_[i]));

      // For unsigned, flip sign bits before comparison
      if constexpr (is_unsigned) {
        __m256i flip_mask =
            _mm256_set1_epi32(static_cast<int32_t>(0x80000000u));
        keys_vec = _mm256_xor_si256(keys_vec, flip_mask);
      }

      // For ascending (std::less): find first position where key >= search_key
      //   Check: keys_vec < search_vec, i.e., search_vec > keys_vec
      // For descending (std::greater): find first position where key <= search_key
      //   Check: keys_vec > search_vec
      __m256i cmp;
      if constexpr (is_ascending) {
        cmp = _mm256_cmpgt_epi32(search_vec_256, keys_vec);
      } else {
        cmp = _mm256_cmpgt_epi32(keys_vec, search_vec_256);
      }

      // Check if any key satisfies the termination condition
      int mask = _mm256_movemask_epi8(cmp);
      if (mask != static_cast<int>(0xFFFFFFFF)) {
        // Each 4-byte element contributes 4 bits to the mask
        size_type offset = std::countr_one(static_cast<unsigned int>(mask)) / 4;
        return keys_.begin() + i + offset;
      }
    }

    // Process 4 keys at a time with half AVX2 (128-bit SSE)
    if (i + 4 <= size_) {
      __m128i search_vec_128 = _mm_set1_epi32(key_bits);
      __m128i keys_vec =
          _mm_load_si128(reinterpret_cast<const __m128i*>(&keys_[i]));

      if constexpr (is_unsigned) {
        __m128i flip_mask = _mm_set1_epi32(static_cast<int32_t>(0x80000000u));
        keys_vec = _mm_xor_si128(keys_vec, flip_mask);
      }

      __m128i cmp;
      if constexpr (is_ascending) {
        cmp = _mm_cmpgt_epi32(search_vec_128, keys_vec);
      } else {
        cmp = _mm_cmpgt_epi32(keys_vec, search_vec_128);
      }

      int mask = _mm_movemask_epi8(cmp);
      if (mask != static_cast<int>(0xFFFF)) {
        size_type offset = std::countr_one(static_cast<unsigned int>(mask)) / 4;
        return keys_.begin() + i + offset;
      }
      i += 4;
    }

    // Process 2 keys at a time with 64-bit SIMD
    if (i + 2 <= size_) {
      __m128i search_vec_128 = _mm_set1_epi32(key_bits);
      __m128i keys_vec = _mm_castpd_si128(
          _mm_load_sd(reinterpret_cast<const double*>(&keys_[i])));

      if constexpr (is_unsigned) {
        __m128i flip_mask = _mm_set1_epi32(static_cast<int32_t>(0x80000000u));
        keys_vec = _mm_xor_si128(keys_vec, flip_mask);
      }

      __m128i cmp;
      if constexpr (is_ascending) {
        cmp = _mm_cmpgt_epi32(search_vec_128, keys_vec);
      } else {
        cmp = _mm_cmpgt_epi32(keys_vec, search_vec_128);
      }

      int mask = _mm_movemask_epi8(cmp);
      if ((mask & 0xFF) != 0xFF) {
        size_type offset = std::countr_one(static_cast<unsigned int>(mask)) / 4;
        return keys_.begin() + i + offset;
      }
      i += 2;
    }

    // Handle remaining 0-1 keys with scalar
    while (i < size_ && comp_(keys_[i], key)) {
      ++i;
    }

    return keys_.begin() + i;
  }
}

/**
 * SIMD-accelerated linear search for 64-bit keys
 * Supports: int64_t (signed), uint64_t (unsigned), double
 * Compares 4 keys at a time using AVX2
 */
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT>
  requires ComparatorCompatible<Key, Compare>
template <typename K>
  requires(sizeof(K) == 8)
auto ordered_array<Key, Value, Length, Compare,
                   SearchModeT>::simd_lower_bound_8byte(const K& key) const {
  static_assert(SimdPrimitive<K>,
                "8-byte SIMD search requires primitive type (int64_t, "
                "uint64_t, long, unsigned long, double)");

  if constexpr (std::is_same_v<K, double>) {
    // Use floating-point comparison instructions
    __m256d search_vec_256 = _mm256_set1_pd(key);

    // Detect comparator type at compile-time
    constexpr bool is_ascending = std::is_same_v<Compare, std::less<Key>>;

    size_type i = 0;
    // Process 4 doubles at a time with AVX2
    for (; i + 4 <= size_; i += 4) {
      __m256d keys_vec =
          _mm256_load_pd(reinterpret_cast<const double*>(&keys_[i]));

      // For ascending (std::less): find first position where key >= search_key
      //   Check: keys_vec < search_vec, find first false
      // For descending (std::greater): find first position where key <= search_key
      //   Check: keys_vec > search_vec, find first false
      __m256d cmp;
      if constexpr (is_ascending) {
        // _CMP_LT_OQ: less-than, ordered, quiet (matches C++ operator<)
        cmp = _mm256_cmp_pd(keys_vec, search_vec_256, _CMP_LT_OQ);
      } else {
        // _CMP_GT_OQ: greater-than, ordered, quiet (matches C++ operator>)
        cmp = _mm256_cmp_pd(keys_vec, search_vec_256, _CMP_GT_OQ);
      }

      // movemask_pd returns 4 bits (one per double)
      int mask = _mm256_movemask_pd(cmp);
      if (mask != 0x0F) {
        // Found first position where comparison is false
        size_type offset = std::countr_one(static_cast<unsigned int>(mask));
        return keys_.begin() + i + offset;
      }
    }

    // Process 2 doubles at a time with SSE
    if (i + 2 <= size_) {
      __m128d search_vec_128 = _mm_set1_pd(key);
      __m128d keys_vec =
          _mm_load_pd(reinterpret_cast<const double*>(&keys_[i]));

      __m128d cmp;
      if constexpr (is_ascending) {
        cmp = _mm_cmp_pd(keys_vec, search_vec_128, _CMP_LT_OQ);
      } else {
        cmp = _mm_cmp_pd(keys_vec, search_vec_128, _CMP_GT_OQ);
      }

      int mask = _mm_movemask_pd(cmp);
      if (mask != 0x03) {
        size_type offset = std::countr_one(static_cast<unsigned int>(mask));
        return keys_.begin() + i + offset;
      }
      i += 2;
    }

    // Handle remaining 0-1 keys with scalar
    while (i < size_ && comp_(keys_[i], key)) {
      ++i;
    }
    return keys_.begin() + i;

  } else {
    // Integer types use integer comparison
    int64_t key_bits;
    std::memcpy(&key_bits, &key, sizeof(K));

    // Check if type is unsigned (uint64_t or unsigned long)
    constexpr bool is_unsigned = std::is_unsigned_v<K>;
    constexpr bool is_ascending = std::is_same_v<Compare, std::less<Key>>;

    if constexpr (is_unsigned) {
      // Flip sign bit: converts unsigned comparison to signed comparison
      key_bits ^= static_cast<int64_t>(0x8000000000000000ULL);
    }

    __m256i search_vec_256 = _mm256_set1_epi64x(key_bits);

    size_type i = 0;
    // Process 4 keys at a time with full AVX2
    for (; i + 4 <= size_; i += 4) {
      __m256i keys_vec =
          _mm256_load_si256(reinterpret_cast<const __m256i*>(&keys_[i]));

      // For unsigned, flip sign bits before comparison
      if constexpr (is_unsigned) {
        __m256i flip_mask =
            _mm256_set1_epi64x(static_cast<int64_t>(0x8000000000000000ULL));
        keys_vec = _mm256_xor_si256(keys_vec, flip_mask);
      }

      // For ascending (std::less): find first position where key >= search_key
      //   Check: keys_vec < search_vec, i.e., search_vec > keys_vec
      // For descending (std::greater): find first position where key <= search_key
      //   Check: keys_vec > search_vec
      __m256i cmp;
      if constexpr (is_ascending) {
        cmp = _mm256_cmpgt_epi64(search_vec_256, keys_vec);
      } else {
        cmp = _mm256_cmpgt_epi64(keys_vec, search_vec_256);
      }

      // Check if any key satisfies the termination condition
      int mask = _mm256_movemask_epi8(cmp);
      if (mask != static_cast<int>(0xFFFFFFFF)) {
        // Each 8-byte element contributes 8 bits to the mask
        size_type offset = std::countr_one(static_cast<unsigned int>(mask)) / 8;
        return keys_.begin() + i + offset;
      }
    }

    // Process 2 keys at a time with half AVX2 (128-bit SSE)
    if (i + 2 <= size_) {
      __m128i search_vec_128 = _mm_set1_epi64x(key_bits);
      __m128i keys_vec =
          _mm_load_si128(reinterpret_cast<const __m128i*>(&keys_[i]));

      if constexpr (is_unsigned) {
        __m128i flip_mask =
            _mm_set1_epi64x(static_cast<int64_t>(0x8000000000000000ULL));
        keys_vec = _mm_xor_si128(keys_vec, flip_mask);
      }

      __m128i cmp;
      if constexpr (is_ascending) {
        cmp = _mm_cmpgt_epi64(search_vec_128, keys_vec);
      } else {
        cmp = _mm_cmpgt_epi64(keys_vec, search_vec_128);
      }

      int mask = _mm_movemask_epi8(cmp);
      if (mask != static_cast<int>(0xFFFF)) {
        size_type offset = std::countr_one(static_cast<unsigned int>(mask)) / 8;
        return keys_.begin() + i + offset;
      }
      i += 2;
    }

    // Handle remaining 0-1 keys with scalar
    while (i < size_ && comp_(keys_[i], key)) {
      ++i;
    }

    return keys_.begin() + i;
  }
}
#endif

// ============================================================================
// Private Helper Methods
// ============================================================================

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
