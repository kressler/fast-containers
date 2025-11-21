// ordered_array.ipp - Implementation details for ordered_array
// This file is included at the end of ordered_array.hpp
// DO NOT include this file directly

namespace fast_containers {

// ============================================================================
// Constructors and Assignment Operators
// ============================================================================

/**
 * Copy constructor - creates a deep copy of another ordered array
 * Complexity: O(n) where n is the size of the other array
 *
 * @param other The ordered array to copy from
 */
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::ordered_array(
    const ordered_array& other)
    : size_(other.size_) {
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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::ordered_array(
    ordered_array&& other) noexcept
    : size_(other.size_) {
  // Move the active elements
  if (size_ > 0) {
    simd_move_forward(other.keys_.data(), other.keys_.data() + size_,
                      keys_.data());
    simd_move_forward(other.values_.data(), other.values_.data() + size_,
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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>&
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::operator=(
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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>&
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::operator=(
    ordered_array&& other) noexcept {
  if (this != &other) {
    size_ = other.size_;
    // Move the active elements
    if (size_ > 0) {
      simd_move_forward(other.keys_.data(), other.keys_.data() + size_,
                        keys_.data());
      simd_move_forward(other.values_.data(), other.values_.data() + size_,
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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
std::pair<typename ordered_array<Key, Value, Length, SearchModeT,
                                 MoveModeT>::iterator,
          bool>
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::insert(
    const Key& key, const Value& value) {
  // Check if array is full
  if (size_ >= Length) {
    throw std::runtime_error("Cannot insert: array is full");
  }

  // Find the position where the key should be inserted
  auto pos = lower_bound_key(key);

  // Calculate index
  size_type idx = pos - keys_.begin();

  // Check if key already exists
  if (idx < size_ && keys_[idx] == key) {
    return {iterator(this, idx), false};
  }

  // Shift elements to the right to make space in both arrays
  simd_move_backward(&keys_[idx], &keys_[size_], &keys_[size_ + 1]);
  simd_move_backward(&values_[idx], &values_[size_], &values_[size_ + 1]);

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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
std::pair<typename ordered_array<Key, Value, Length, SearchModeT,
                                 MoveModeT>::iterator,
          bool>
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::insert_hint(
    iterator hint, const Key& key, const Value& value) {
  // Check if array is full
  if (size_ >= Length) {
    throw std::runtime_error("Cannot insert: array is full");
  }

  // Get the index from the hint
  size_type idx = hint.index_;

  // Verify the hint is valid (optional debug check)
  // In release builds, we trust the hint for performance
  assert(idx <= size_);
  assert(idx == size_ || keys_[idx] >= key);
  assert(idx == 0 || keys_[idx - 1] < key);

  // Check if key already exists at the hint position
  if (idx < size_ && keys_[idx] == key) {
    return {iterator(this, idx), false};
  }

  // Shift elements to the right to make space in both arrays
  simd_move_backward(&keys_[idx], &keys_[size_], &keys_[size_ + 1]);
  simd_move_backward(&values_[idx], &values_[size_], &values_[size_ + 1]);

  // Insert the new elements
  keys_[idx] = key;
  values_[idx] = value;
  ++size_;

  return {iterator(this, idx), true};
}

/**
 * Erase an element by key.
 * If the element does not exist, does nothing.
 *
 * @param key The key to erase
 * @return The number of elements erased (0 or 1)
 */
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
typename ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::size_type
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::erase(
    const Key& key) {
  auto it = find(key);
  if (it != end()) {
    size_type idx = it.index();
    // Shift elements to the left to fill the gap in both arrays
    simd_move_forward(&keys_[idx + 1], &keys_[size_], &keys_[idx]);
    simd_move_forward(&values_[idx + 1], &values_[size_], &values_[idx]);
    --size_;
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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
Value& ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::operator[](
    const Key& key) {
  auto it = find(key);
  if (it != end()) {
    return it.value_ref();
  }

  // Need to insert - check if there's space
  if (size_ >= Length) {
    throw std::runtime_error("Cannot insert: array is full");
  }

  // Find insertion position
  auto pos = lower_bound_key(key);
  size_type idx = pos - keys_.begin();

  // Shift elements to make space in both arrays
  simd_move_backward(&keys_[idx], &keys_[size_], &keys_[size_ + 1]);
  simd_move_backward(&values_[idx], &values_[size_], &values_[size_ + 1]);

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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
typename ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::iterator
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::find(
    const Key& key) {
  auto pos = lower_bound_key(key);
  size_type idx = pos - keys_.begin();

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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
typename ordered_array<Key, Value, Length, SearchModeT,
                       MoveModeT>::const_iterator
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::find(
    const Key& key) const {
  auto pos = lower_bound_key(key);
  size_type idx = pos - keys_.begin();

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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
typename ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::iterator
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::lower_bound(
    const Key& key) {
  auto pos = lower_bound_key(key);
  size_type idx = pos - keys_.begin();
  return iterator(this, idx);
}

/**
 * Find the first element not less than the given key (const version).
 *
 * @param key The key to search for
 * @return Const iterator to the first element >= key, or end() if all elements
 * < key
 */
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
typename ordered_array<Key, Value, Length, SearchModeT,
                       MoveModeT>::const_iterator
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::lower_bound(
    const Key& key) const {
  auto pos = lower_bound_key(key);
  size_type idx = pos - keys_.begin();
  return const_iterator(this, idx);
}

/**
 * Find the first element greater than the given key.
 *
 * @param key The key to search for
 * @return Iterator to the first element > key, or end() if all elements <= key
 */
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
typename ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::iterator
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::upper_bound(
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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
typename ordered_array<Key, Value, Length, SearchModeT,
                       MoveModeT>::const_iterator
ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::upper_bound(
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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
template <std::size_t OutputLength>
void ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::split_at(
    iterator pos,
    ordered_array<Key, Value, OutputLength, SearchModeT, MoveModeT>& output) {
  // Debug assertion: iterator must belong to this array
  assert(pos.array_ == this && "Iterator does not belong to this array");

  // Runtime check: output must be empty
  if (!output.empty()) {
    throw std::runtime_error("Cannot split: output array is not empty");
  }

  // Calculate the split index
  size_type split_idx = pos.index_;

  // Calculate how many elements to move
  size_type num_to_move = size_ - split_idx;

  // Check if output has sufficient capacity
  if (num_to_move > output.capacity()) {
    throw std::runtime_error(
        "Cannot split: output array has insufficient capacity");
  }

  // Move elements from [split_idx, size_) to output
  if (num_to_move > 0) {
    simd_move_forward(&keys_[split_idx], &keys_[size_], output.keys_.data());
    simd_move_forward(&values_[split_idx], &values_[size_],
                      output.values_.data());
    output.size_ = num_to_move;
  }

  // Update this array's size
  size_ = split_idx;
}

/**
 * Append all elements from another array to the end of this array.
 * The other array is left empty after the operation.
 *
 * Precondition (debug mode only): All keys in other must be greater than
 * all keys in this array.
 *
 * Note: Invalidates all iterators to both arrays.
 *
 * @tparam OtherLength The capacity of the other array
 * @param other The array to append (rvalue reference, will be emptied)
 * @throws std::runtime_error if combined size exceeds capacity
 *
 * Complexity: O(n) where n is the size of other
 */
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
template <std::size_t OtherLength>
void ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::append(
    ordered_array<Key, Value, OtherLength, SearchModeT, MoveModeT>&& other) {
  // Check capacity constraint
  if (size_ + other.size_ > Length) {
    throw std::runtime_error("Cannot append: combined size exceeds capacity");
  }

  // Debug assertion: ordering precondition
  // All keys in other must be > all keys in this
  assert((empty() || other.empty() || keys_[size_ - 1] < other.keys_[0]) &&
         "Ordering precondition violated: all keys in other must be > all "
         "keys in this");

  // Append other's elements to the end of this array
  if (other.size_ > 0) {
    simd_move_forward(other.keys_.data(), other.keys_.data() + other.size_,
                      &keys_[size_]);
    simd_move_forward(other.values_.data(), other.values_.data() + other.size_,
                      &values_[size_]);
    size_ += other.size_;

    // Leave other in empty state
    other.size_ = 0;
  }
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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
template <std::size_t SourceLength>
void ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::
    transfer_prefix_from(
        ordered_array<Key, Value, SourceLength, SearchModeT, MoveModeT>& source,
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
  assert((empty() || count == 0 || keys_[size_ - 1] < source.keys_[0]) &&
         "Ordering precondition violated: all keys in this must be < all "
         "keys in source prefix");

  if (count == 0) {
    return;
  }

  // Copy prefix from source to end of this array (append)
  simd_move_forward(source.keys_.data(), source.keys_.data() + count,
                    &keys_[size_]);
  simd_move_forward(source.values_.data(), source.values_.data() + count,
                    &values_[size_]);

  // Shift remaining elements in source left to fill the gap
  if (count < source.size_) {
    simd_move_forward(&source.keys_[count], &source.keys_[source.size_],
                      source.keys_.data());
    simd_move_forward(&source.values_[count], &source.values_[source.size_],
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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
template <std::size_t SourceLength>
void ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::
    transfer_suffix_from(
        ordered_array<Key, Value, SourceLength, SearchModeT, MoveModeT>& source,
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
  assert((empty() || count == 0 || source.keys_[source.size_ - 1] < keys_[0]) &&
         "Ordering precondition violated: all keys in source suffix must be "
         "< all keys in this");

  if (count == 0) {
    return;
  }

  // Shift current elements right to make space for incoming suffix
  if (size_ > 0) {
    simd_move_backward(&keys_[0], &keys_[size_], &keys_[size_ + count]);
    simd_move_backward(&values_[0], &values_[size_], &values_[size_ + count]);
  }

  // Calculate starting position of suffix in source
  size_type suffix_start = source.size_ - count;

  // Copy suffix from source to beginning of this array (prepend)
  simd_move_forward(&source.keys_[suffix_start], &source.keys_[source.size_],
                    keys_.data());
  simd_move_forward(&source.values_[suffix_start],
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
 * SIMD-accelerated linear search for 32-bit keys
 * Supports: int32_t (signed), uint32_t (unsigned), float
 * Compares 8 keys at a time using AVX2
 */
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
template <typename K>
  requires(sizeof(K) == 4)
auto ordered_array<Key, Value, Length, SearchModeT,
                   MoveModeT>::simd_lower_bound_4byte(const K& key) const {
  static_assert(SimdPrimitive<K>,
                "4-byte SIMD search requires primitive type (int32_t, "
                "uint32_t, int, unsigned int, float)");

  if constexpr (std::is_same_v<K, float>) {
    // Use floating-point comparison instructions
    __m256 search_vec_256 = _mm256_set1_ps(key);

    size_type i = 0;
    // Process 8 floats at a time with AVX2
    for (; i + 8 <= size_; i += 8) {
      __m256 keys_vec =
          _mm256_load_ps(reinterpret_cast<const float*>(&keys_[i]));

      // Compare: keys_vec < search_vec (returns 0xFFFFFFFF where true)
      // _CMP_LT_OQ: less-than, ordered, quiet (matches C++ operator<)
      __m256 cmp_lt = _mm256_cmp_ps(keys_vec, search_vec_256, _CMP_LT_OQ);

      // movemask_ps returns 8 bits (one per float)
      int mask = _mm256_movemask_ps(cmp_lt);
      if (mask != 0xFF) {
        // Found first position where key >= search_key
        size_type offset = std::countr_one(static_cast<unsigned int>(mask));
        return keys_.begin() + i + offset;
      }
    }

    // Process 4 floats at a time with SSE
    if (i + 4 <= size_) {
      __m128 search_vec_128 = _mm_set1_ps(key);
      __m128 keys_vec = _mm_load_ps(reinterpret_cast<const float*>(&keys_[i]));
      __m128 cmp_lt = _mm_cmp_ps(keys_vec, search_vec_128, _CMP_LT_OQ);

      int mask = _mm_movemask_ps(cmp_lt);
      if (mask != 0x0F) {
        size_type offset = std::countr_one(static_cast<unsigned int>(mask));
        return keys_.begin() + i + offset;
      }
      i += 4;
    }

    // Handle remaining 0-3 keys with scalar
    while (i < size_ && keys_[i] < key) {
      ++i;
    }
    return keys_.begin() + i;

  } else {
    // Integer types use integer comparison
    int32_t key_bits;
    std::memcpy(&key_bits, &key, sizeof(K));

    // Check if type is unsigned (uint32_t or unsigned int)
    constexpr bool is_unsigned = std::is_unsigned_v<K>;

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

      // Compare: keys_vec < search_vec (returns 0xFFFFFFFF where true)
      __m256i cmp_lt = _mm256_cmpgt_epi32(search_vec_256, keys_vec);

      // Check if any key is >= search_key
      int mask = _mm256_movemask_epi8(cmp_lt);
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

      __m128i cmp_lt = _mm_cmpgt_epi32(search_vec_128, keys_vec);

      int mask = _mm_movemask_epi8(cmp_lt);
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

      __m128i cmp_lt = _mm_cmpgt_epi32(search_vec_128, keys_vec);

      int mask = _mm_movemask_epi8(cmp_lt);
      if ((mask & 0xFF) != 0xFF) {
        size_type offset = std::countr_one(static_cast<unsigned int>(mask)) / 4;
        return keys_.begin() + i + offset;
      }
      i += 2;
    }

    // Handle remaining 0-1 keys with scalar
    while (i < size_ && keys_[i] < key) {
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
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
template <typename K>
  requires(sizeof(K) == 8)
auto ordered_array<Key, Value, Length, SearchModeT,
                   MoveModeT>::simd_lower_bound_8byte(const K& key) const {
  static_assert(SimdPrimitive<K>,
                "8-byte SIMD search requires primitive type (int64_t, "
                "uint64_t, long, unsigned long, double)");

  if constexpr (std::is_same_v<K, double>) {
    // Use floating-point comparison instructions
    __m256d search_vec_256 = _mm256_set1_pd(key);

    size_type i = 0;
    // Process 4 doubles at a time with AVX2
    for (; i + 4 <= size_; i += 4) {
      __m256d keys_vec =
          _mm256_load_pd(reinterpret_cast<const double*>(&keys_[i]));

      // Compare: keys_vec < search_vec (returns 0xFFFFFFFFFFFFFFFF where true)
      // _CMP_LT_OQ: less-than, ordered, quiet (matches C++ operator<)
      __m256d cmp_lt = _mm256_cmp_pd(keys_vec, search_vec_256, _CMP_LT_OQ);

      // movemask_pd returns 4 bits (one per double)
      int mask = _mm256_movemask_pd(cmp_lt);
      if (mask != 0x0F) {
        // Found first position where key >= search_key
        size_type offset = std::countr_one(static_cast<unsigned int>(mask));
        return keys_.begin() + i + offset;
      }
    }

    // Process 2 doubles at a time with SSE
    if (i + 2 <= size_) {
      __m128d search_vec_128 = _mm_set1_pd(key);
      __m128d keys_vec =
          _mm_load_pd(reinterpret_cast<const double*>(&keys_[i]));
      __m128d cmp_lt = _mm_cmp_pd(keys_vec, search_vec_128, _CMP_LT_OQ);

      int mask = _mm_movemask_pd(cmp_lt);
      if (mask != 0x03) {
        size_type offset = std::countr_one(static_cast<unsigned int>(mask));
        return keys_.begin() + i + offset;
      }
      i += 2;
    }

    // Handle remaining 0-1 keys with scalar
    while (i < size_ && keys_[i] < key) {
      ++i;
    }
    return keys_.begin() + i;

  } else {
    // Integer types use integer comparison
    int64_t key_bits;
    std::memcpy(&key_bits, &key, sizeof(K));

    // Check if type is unsigned (uint64_t or unsigned long)
    constexpr bool is_unsigned = std::is_unsigned_v<K>;

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

      // Compare: keys_vec < search_vec (returns 0xFFFFFFFFFFFFFFFF where true)
      __m256i cmp_lt = _mm256_cmpgt_epi64(search_vec_256, keys_vec);

      // Check if any key is >= search_key
      int mask = _mm256_movemask_epi8(cmp_lt);
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

      __m128i cmp_lt = _mm_cmpgt_epi64(search_vec_128, keys_vec);

      int mask = _mm_movemask_epi8(cmp_lt);
      if (mask != static_cast<int>(0xFFFF)) {
        size_type offset = std::countr_one(static_cast<unsigned int>(mask)) / 8;
        return keys_.begin() + i + offset;
      }
      i += 2;
    }

    // Handle remaining 0-1 keys with scalar
    while (i < size_ && keys_[i] < key) {
      ++i;
    }

    return keys_.begin() + i;
  }
}
#endif

// ============================================================================
// Private Helper Methods - SIMD Data Movement
// ============================================================================

/**
 * Move elements backward (towards higher indices) using SIMD when possible.
 * Used for insert operations to shift elements right and make space.
 *
 * @tparam T The element type
 * @param first Start of the source range
 * @param last End of the source range
 * @param dest_last End of the destination range
 */
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
template <typename T>
void ordered_array<Key, Value, Length, SearchModeT,
                   MoveModeT>::simd_move_backward(T* first, T* last,
                                                  T* dest_last) {
  if constexpr (MoveModeT == MoveMode::Standard) {
    // Use standard library move
    std::move_backward(first, last, dest_last);
    return;
  }

#ifdef __AVX2__
  if constexpr (MoveModeT == MoveMode::SIMD &&
                std::is_trivially_copyable_v<T>) {
    // Work with bytes - cast to char* for simplicity
    size_type num_bytes = (last - first) * sizeof(T);
    if (num_bytes == 0)
      return;

    // Work backwards to avoid overwriting source data
    char* src = reinterpret_cast<char*>(last);
    char* dst = reinterpret_cast<char*>(dest_last);

    // Move 32-byte blocks (256-bit AVX2)
    while (num_bytes >= 32) {
      src -= 32;
      dst -= 32;
      __m256i data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst), data);
      num_bytes -= 32;
    }

    // Move 16-byte blocks (128-bit SSE)
    if (num_bytes >= 16) {
      src -= 16;
      dst -= 16;
      __m128i data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
      _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), data);
      num_bytes -= 16;
    }

    // Move remaining bytes one at a time
    while (num_bytes > 0) {
      --src;
      --dst;
      *dst = *src;
      --num_bytes;
    }
  } else {
    // Non-trivially copyable: use std::move_backward
    std::move_backward(first, last, dest_last);
  }
#else
  // No AVX2: fall back to std::move_backward
  std::move_backward(first, last, dest_last);
#endif
}

/**
 * Move elements forward (towards lower indices) using SIMD when possible.
 * Used for erase operations to shift elements left and fill gaps.
 *
 * @tparam T The element type
 * @param first Start of the source range
 * @param last End of the source range
 * @param dest_first Start of the destination range
 */
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
template <typename T>
void ordered_array<Key, Value, Length, SearchModeT,
                   MoveModeT>::simd_move_forward(T* first, T* last,
                                                 T* dest_first) {
  if constexpr (MoveModeT == MoveMode::Standard) {
    // Use standard library move
    std::move(first, last, dest_first);
    return;
  }

#ifdef __AVX2__
  if constexpr (MoveModeT == MoveMode::SIMD &&
                std::is_trivially_copyable_v<T>) {
    // Work with bytes - cast to char* for simplicity
    size_type num_bytes = (last - first) * sizeof(T);
    if (num_bytes == 0)
      return;

    // Work forwards (safe since dest < src for erase operations)
    char* src = reinterpret_cast<char*>(first);
    char* dst = reinterpret_cast<char*>(dest_first);

    // Move 32-byte blocks (256-bit AVX2)
    while (num_bytes >= 32) {
      __m256i data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst), data);
      src += 32;
      dst += 32;
      num_bytes -= 32;
    }

    // Move 16-byte blocks (128-bit SSE)
    if (num_bytes >= 16) {
      __m128i data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
      _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), data);
      src += 16;
      dst += 16;
      num_bytes -= 16;
    }

    // Move remaining bytes one at a time
    while (num_bytes > 0) {
      *dst = *src;
      ++src;
      ++dst;
      --num_bytes;
    }
  } else {
    // Non-trivially copyable: use std::move
    std::move(first, last, dest_first);
  }
#else
  // No AVX2: fall back to std::move
  std::move(first, last, dest_first);
#endif
}

/**
 * Find the insertion position for a key using the configured search mode.
 * Returns an iterator to the first element not less than the given key.
 *
 * @param key The key to search for
 * @return Iterator to the insertion position
 */
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT>
auto ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::lower_bound_key(
    const Key& key) const {
  if constexpr (SearchModeT == SearchMode::Linear) {
    // Linear search: scan from beginning until we find key >= search key
    auto it = keys_.begin();
    auto end_it = keys_.begin() + size_;
    while (it != end_it && *it < key) {
      ++it;
    }
    return it;
  } else if constexpr (SearchModeT == SearchMode::Binary) {
    // Binary search using standard library
    return std::lower_bound(keys_.begin(), keys_.begin() + size_, key);
  } else if constexpr (SearchModeT == SearchMode::SIMD) {
    // SIMD-accelerated linear search
#ifdef __AVX2__
    if constexpr (SIMDSearchable<Key>) {
      if constexpr (sizeof(Key) == 4) {
        return simd_lower_bound_4byte(key);
      } else if constexpr (sizeof(Key) == 8) {
        return simd_lower_bound_8byte(key);
      }
      // Note: SIMDSearchable only accepts primitive types (4 or 8 bytes)
      // so we never reach here
    } else {
      // Fallback to regular linear search if type doesn't support SIMD
      auto it = keys_.begin();
      auto end_it = keys_.begin() + size_;
      while (it != end_it && *it < key) {
        ++it;
      }
      return it;
    }
#else
    // AVX2 not available, fall back to linear search
    auto it = keys_.begin();
    auto end_it = keys_.begin() + size_;
    while (it != end_it && *it < key) {
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

}  // namespace fast_containers
