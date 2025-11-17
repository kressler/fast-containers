#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace fast_containers {

// Enum to control search strategy
enum class SearchMode {
  Binary,  // Binary search using std::lower_bound (O(log n))
  Linear,  // Linear search for small arrays (better cache behavior)
  SIMD     // SIMD-accelerated linear search for small arrays
};

// Enum to control data movement strategy
enum class MoveMode {
  Standard,  // Use std::move/std::move_backward
  SIMD       // Use SIMD-accelerated moves (default)
};

// Concept to enforce that Key type supports comparison operations
template <typename T>
concept Comparable = requires(T a, T b) {
  { a < b } -> std::convertible_to<bool>;
  { a == b } -> std::convertible_to<bool>;
};

// Concept for types that can use SIMD-accelerated search
template <typename T>
concept SIMDSearchable = Comparable<T> && std::is_trivially_copyable_v<T> &&
                         (sizeof(T) == 4 || sizeof(T) == 8);

/**
 * A fixed-size ordered array that maintains key-value pairs in sorted order.
 * Keys and values are stored in separate arrays for better cache locality
 * and to enable SIMD optimizations.
 *
 * @tparam Key The key type (must be Comparable)
 * @tparam Value The value type
 * @tparam Length The maximum number of elements
 * @tparam SearchModeT The search mode (Binary, Linear, or SIMD)
 * @tparam MoveModeT The data movement mode (Standard or SIMD, defaults to SIMD)
 */
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT = SearchMode::Binary,
          MoveMode MoveModeT = MoveMode::SIMD>
class ordered_array {
 private:
  // Forward declare iterator classes
  template <bool IsConst>
  class ordered_array_iterator;

  // Allow different instantiations to access each other's private members
  template <Comparable, typename, std::size_t, SearchMode, MoveMode>
  friend class ordered_array;

 public:
  // Type aliases for key-value pair
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<Key, Value>;
  using size_type = std::size_t;

  // Iterator types
  using iterator = ordered_array_iterator<false>;
  using const_iterator = ordered_array_iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  /**
   * Default constructor - initializes an empty ordered array
   */
  ordered_array() : size_(0) {}

  /**
   * Copy constructor - creates a deep copy of another ordered array
   * Complexity: O(n) where n is the size of the other array
   *
   * @param other The ordered array to copy from
   */
  ordered_array(const ordered_array& other) : size_(other.size_) {
    // Copy only the active elements
    if (size_ > 0) {
      std::copy(other.keys_.begin(), other.keys_.begin() + size_,
                keys_.begin());
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
  ordered_array(ordered_array&& other) noexcept : size_(other.size_) {
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
  ordered_array& operator=(const ordered_array& other) {
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
  ordered_array& operator=(ordered_array&& other) noexcept {
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

  /**
   * Insert a new key-value pair into the array in sorted order.
   *
   * @param key The key to insert
   * @param value The value to insert
   * @throws std::runtime_error if the array is full
   * @throws std::runtime_error if the key already exists
   */
  void insert(const Key& key, const Value& value) {
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
      throw std::runtime_error("Cannot insert: key already exists");
    }

    // Shift elements to the right to make space in both arrays
    simd_move_backward(&keys_[idx], &keys_[size_], &keys_[size_ + 1]);
    simd_move_backward(&values_[idx], &values_[size_], &values_[size_ + 1]);

    // Insert the new elements
    keys_[idx] = key;
    values_[idx] = value;
    ++size_;
  }

  /**
   * Find an element by key.
   *
   * @param key The key to search for
   * @return Iterator to the found element, or end() if not found
   */
  iterator find(const Key& key) {
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
  const_iterator find(const Key& key) const {
    auto pos = lower_bound_key(key);
    size_type idx = pos - keys_.begin();

    if (idx < size_ && keys_[idx] == key) {
      return const_iterator(this, idx);
    }
    return end();
  }

  /**
   * Remove an element by key.
   * If the element does not exist, does nothing.
   *
   * @param key The key to remove
   */
  void remove(const Key& key) {
    auto it = find(key);
    if (it != end()) {
      size_type idx = it.index();
      // Shift elements to the left to fill the gap in both arrays
      simd_move_forward(&keys_[idx + 1], &keys_[size_], &keys_[idx]);
      simd_move_forward(&values_[idx + 1], &values_[size_], &values_[idx]);
      --size_;
    }
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
  Value& operator[](const Key& key) {
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

  // Iterator methods
  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, size_); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size_); }
  const_iterator cbegin() const { return const_iterator(this, 0); }
  const_iterator cend() const { return const_iterator(this, size_); }

  // Reverse iterator methods
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }
  const_reverse_iterator crbegin() const {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator crend() const {
    return const_reverse_iterator(begin());
  }

  // Utility methods
  size_type size() const { return size_; }
  constexpr size_type capacity() const { return Length; }
  bool empty() const { return size_ == 0; }
  bool full() const { return size_ == Length; }

  void clear() { size_ = 0; }

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
  template <std::size_t OutputLength>
  void split_at(
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
  template <std::size_t OtherLength>
  void append(
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
      simd_move_forward(other.values_.data(),
                        other.values_.data() + other.size_, &values_[size_]);
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
  template <std::size_t SourceLength>
  void transfer_prefix_from(
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
  template <std::size_t SourceLength>
  void transfer_suffix_from(
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
    assert(
        (empty() || count == 0 || source.keys_[source.size_ - 1] < keys_[0]) &&
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

 private:
#ifdef __AVX2__
  /**
   * SIMD-accelerated linear search for 32-bit keys (int32_t, uint32_t, float)
   * Compares 8 keys at a time using AVX2
   */
  template <typename K>
    requires(sizeof(K) == 4)
  auto simd_lower_bound_4byte(const K& key) const {
    // Prepare search key in SIMD register (broadcast to all 8 lanes)
    int32_t key_bits;
    std::memcpy(&key_bits, &key, sizeof(K));
    __m256i search_vec_256 = _mm256_set1_epi32(key_bits);

    size_type i = 0;
    // Process 8 keys at a time with full AVX2
    for (; i + 8 <= size_; i += 8) {
      // Load 8 keys from array (aligned - array is 64-byte aligned, i is
      // multiple of 8)
      __m256i keys_vec =
          _mm256_load_si256(reinterpret_cast<const __m256i*>(&keys_[i]));

      // Compare: keys_vec < search_vec (returns 0xFFFFFFFF where true)
      __m256i cmp_lt = _mm256_cmpgt_epi32(search_vec_256, keys_vec);

      // Check if any key is >= search_key
      int mask = _mm256_movemask_epi8(cmp_lt);
      if (mask != static_cast<int>(0xFFFFFFFF)) {
        // Found a position where key >= search_key
        // Use bit manipulation to find the exact index within this block
        // Each 4-byte element contributes 4 bits to the mask
        // Count trailing ones and divide by 4 to get element offset
        size_type offset = std::countr_one(static_cast<unsigned int>(mask)) / 4;
        return keys_.begin() + i + offset;
      }
    }

    // Process 4 keys at a time with half AVX2 (128-bit SSE)
    if (i + 4 <= size_) {
      __m128i search_vec_128 = _mm_set1_epi32(key_bits);
      // Aligned load (i is multiple of 8 from previous loop)
      __m128i keys_vec =
          _mm_load_si128(reinterpret_cast<const __m128i*>(&keys_[i]));
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

  /**
   * SIMD-accelerated linear search for 64-bit keys (int64_t, uint64_t, double)
   * Compares 4 keys at a time using AVX2
   */
  template <typename K>
    requires(sizeof(K) == 8)
  auto simd_lower_bound_8byte(const K& key) const {
    // Prepare search key in SIMD register (broadcast to all 4 lanes)
    int64_t key_bits;
    std::memcpy(&key_bits, &key, sizeof(K));
    __m256i search_vec_256 = _mm256_set1_epi64x(key_bits);

    size_type i = 0;
    // Process 4 keys at a time with full AVX2
    for (; i + 4 <= size_; i += 4) {
      // Load 4 keys from array (aligned - array is 64-byte aligned, i is
      // multiple of 4)
      __m256i keys_vec =
          _mm256_load_si256(reinterpret_cast<const __m256i*>(&keys_[i]));

      // Compare: keys_vec < search_vec (returns 0xFFFFFFFFFFFFFFFF where true)
      __m256i cmp_lt = _mm256_cmpgt_epi64(search_vec_256, keys_vec);

      // Check if any key is >= search_key
      int mask = _mm256_movemask_epi8(cmp_lt);
      if (mask != static_cast<int>(0xFFFFFFFF)) {
        // Found a position where key >= search_key
        // Use bit manipulation to find the exact index within this block
        // Each 8-byte element contributes 8 bits to the mask
        // Count trailing ones and divide by 8 to get element offset
        size_type offset = std::countr_one(static_cast<unsigned int>(mask)) / 8;
        return keys_.begin() + i + offset;
      }
    }

    // Process 2 keys at a time with half AVX2 (128-bit SSE)
    if (i + 2 <= size_) {
      __m128i search_vec_128 = _mm_set1_epi64x(key_bits);
      // Aligned load (i is multiple of 4 from previous loop)
      __m128i keys_vec =
          _mm_load_si128(reinterpret_cast<const __m128i*>(&keys_[i]));
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
#endif

  /**
   * Move elements backward (towards higher indices) using SIMD when possible.
   * Used for insert operations to shift elements right and make space.
   *
   * @tparam T The element type
   * @param first Start of the source range
   * @param last End of the source range
   * @param dest_last End of the destination range
   */
  template <typename T>
  void simd_move_backward(T* first, T* last, T* dest_last) {
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
        __m256i data =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
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
   * Used for remove operations to shift elements left and fill gaps.
   *
   * @tparam T The element type
   * @param first Start of the source range
   * @param last End of the source range
   * @param dest_first Start of the destination range
   */
  template <typename T>
  void simd_move_forward(T* first, T* last, T* dest_first) {
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

      // Work forwards (safe since dest < src for remove operations)
      char* src = reinterpret_cast<char*>(first);
      char* dst = reinterpret_cast<char*>(dest_first);

      // Move 32-byte blocks (256-bit AVX2)
      while (num_bytes >= 32) {
        __m256i data =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
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
  auto lower_bound_key(const Key& key) const {
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

  // Separate arrays for keys and values
  // Align to 64-byte cache lines for optimal SIMD performance
  alignas(64) std::array<Key, Length> keys_;
  alignas(64) std::array<Value, Length> values_;
  size_type size_;

  // Proxy class to represent a key-value pair reference
  template <bool IsConst>
  class pair_proxy {
   public:
    // Key is always const to prevent breaking sorted order invariant
    using key_ref_type = const Key&;
    using value_ref_type = std::conditional_t<IsConst, const Value&, Value&>;

    pair_proxy(key_ref_type k, value_ref_type v) : first(k), second(v) {}

    // Allow conversion to std::pair for compatibility
    operator std::pair<Key, Value>() const { return {first, second}; }

    key_ref_type first;
    value_ref_type second;
  };

  // Custom iterator implementation
  template <bool IsConst>
  class ordered_array_iterator {
   public:
    // Arrow operator helper (forward declare)
    struct arrow_proxy {
      pair_proxy<IsConst> ref;
      arrow_proxy(pair_proxy<IsConst> r) : ref(r) {}
      pair_proxy<IsConst>* operator->() { return &ref; }
    };

    using iterator_category = std::random_access_iterator_tag;
    using value_type = std::pair<Key, Value>;
    using difference_type = std::ptrdiff_t;
    using pointer = arrow_proxy;  // This is what operator-> actually returns
    using reference = pair_proxy<IsConst>;

    using array_ptr_type =
        std::conditional_t<IsConst, const ordered_array*, ordered_array*>;

    ordered_array_iterator(array_ptr_type arr, size_type idx)
        : array_(arr), index_(idx) {}

    // Allow conversion from non-const to const iterator
    template <bool WasConst = IsConst, typename = std::enable_if_t<WasConst>>
    ordered_array_iterator(const ordered_array_iterator<false>& other)
        : array_(other.array_), index_(other.index_) {}

    reference operator*() const {
      return reference(array_->keys_[index_], array_->values_[index_]);
    }

    arrow_proxy operator->() const { return arrow_proxy(operator*()); }

    ordered_array_iterator& operator++() {
      ++index_;
      return *this;
    }

    ordered_array_iterator operator++(int) {
      auto tmp = *this;
      ++index_;
      return tmp;
    }

    ordered_array_iterator& operator--() {
      --index_;
      return *this;
    }

    ordered_array_iterator operator--(int) {
      auto tmp = *this;
      --index_;
      return tmp;
    }

    ordered_array_iterator& operator+=(difference_type n) {
      index_ += n;
      return *this;
    }

    ordered_array_iterator& operator-=(difference_type n) {
      index_ -= n;
      return *this;
    }

    ordered_array_iterator operator+(difference_type n) const {
      return ordered_array_iterator(array_, index_ + n);
    }

    ordered_array_iterator operator-(difference_type n) const {
      return ordered_array_iterator(array_, index_ - n);
    }

    difference_type operator-(const ordered_array_iterator& other) const {
      return index_ - other.index_;
    }

    reference operator[](difference_type n) const {
      return reference(array_->keys_[index_ + n], array_->values_[index_ + n]);
    }

    bool operator==(const ordered_array_iterator& other) const {
      return array_ == other.array_ && index_ == other.index_;
    }

    bool operator!=(const ordered_array_iterator& other) const {
      return !(*this == other);
    }

    bool operator<(const ordered_array_iterator& other) const {
      return index_ < other.index_;
    }

    bool operator>(const ordered_array_iterator& other) const {
      return index_ > other.index_;
    }

    bool operator<=(const ordered_array_iterator& other) const {
      return index_ <= other.index_;
    }

    bool operator>=(const ordered_array_iterator& other) const {
      return index_ >= other.index_;
    }

    // Helper method to get the index
    size_type index() const { return index_; }

    // Helper method to get a reference to the value
    std::conditional_t<IsConst, const Value&, Value&> value_ref() const {
      return array_->values_[index_];
    }

   private:
    array_ptr_type array_;
    size_type index_;

    friend class ordered_array;
    template <bool>
    friend class ordered_array_iterator;
  };
};

// Non-member operator+ for iterator + difference
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode SearchModeT, MoveMode MoveModeT, bool IsConst>
typename ordered_array<Key, Value, Length, SearchModeT,
                       MoveModeT>::template ordered_array_iterator<IsConst>
operator+(typename ordered_array<Key, Value, Length, SearchModeT, MoveModeT>::
              template ordered_array_iterator<IsConst>::difference_type n,
          const typename ordered_array<
              Key, Value, Length, SearchModeT,
              MoveModeT>::template ordered_array_iterator<IsConst>& it) {
  return it + n;
}

}  // namespace fast_containers
