#pragma once

#include <algorithm>
#include <array>
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
 * @tparam Mode The search mode (Binary, Linear, or SIMD)
 */
template <Comparable Key, typename Value, std::size_t Length,
          SearchMode Mode = SearchMode::Binary>
class ordered_array {
 private:
  // Forward declare iterator classes
  template <bool IsConst>
  class ordered_array_iterator;

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
    std::move_backward(keys_.begin() + idx, keys_.begin() + size_,
                       keys_.begin() + size_ + 1);
    std::move_backward(values_.begin() + idx, values_.begin() + size_,
                       values_.begin() + size_ + 1);

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
      std::move(keys_.begin() + idx + 1, keys_.begin() + size_,
                keys_.begin() + idx);
      std::move(values_.begin() + idx + 1, values_.begin() + size_,
                values_.begin() + idx);
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
    std::move_backward(keys_.begin() + idx, keys_.begin() + size_,
                       keys_.begin() + size_ + 1);
    std::move_backward(values_.begin() + idx, values_.begin() + size_,
                       values_.begin() + size_ + 1);

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
    __m256i search_vec = _mm256_set1_epi32(key_bits);

    size_type i = 0;
    // Process 8 keys at a time
    for (; i + 8 <= size_; i += 8) {
      // Load 8 keys from array
      __m256i keys_vec =
          _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&keys_[i]));

      // Compare: keys_vec < search_vec (returns 0xFFFFFFFF where true)
      __m256i cmp_lt = _mm256_cmpgt_epi32(search_vec, keys_vec);

      // Check if any key is >= search_key
      int mask = _mm256_movemask_epi8(cmp_lt);
      if (mask != static_cast<int>(0xFFFFFFFF)) {
        // Found a position where key >= search_key, finish with scalar search
        break;
      }
    }

    // Handle remaining keys with scalar loop
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
    __m256i search_vec = _mm256_set1_epi64x(key_bits);

    size_type i = 0;
    // Process 4 keys at a time
    for (; i + 4 <= size_; i += 4) {
      // Load 4 keys from array
      __m256i keys_vec =
          _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&keys_[i]));

      // Compare: keys_vec < search_vec (returns 0xFFFFFFFFFFFFFFFF where true)
      __m256i cmp_lt = _mm256_cmpgt_epi64(search_vec, keys_vec);

      // Check if any key is >= search_key
      int mask = _mm256_movemask_epi8(cmp_lt);
      if (mask != static_cast<int>(0xFFFFFFFF)) {
        // Found a position where key >= search_key, finish with scalar search
        break;
      }
    }

    // Handle remaining keys with scalar loop
    while (i < size_ && keys_[i] < key) {
      ++i;
    }

    return keys_.begin() + i;
  }
#endif

  /**
   * Find the insertion position for a key using the configured search mode.
   * Returns an iterator to the first element not less than the given key.
   *
   * @param key The key to search for
   * @return Iterator to the insertion position
   */
  auto lower_bound_key(const Key& key) const {
    if constexpr (Mode == SearchMode::Linear) {
      // Linear search: scan from beginning until we find key >= search key
      auto it = keys_.begin();
      auto end_it = keys_.begin() + size_;
      while (it != end_it && *it < key) {
        ++it;
      }
      return it;
    } else if constexpr (Mode == SearchMode::Binary) {
      // Binary search using standard library
      return std::lower_bound(keys_.begin(), keys_.begin() + size_, key);
    } else if constexpr (Mode == SearchMode::SIMD) {
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
      static_assert(Mode == SearchMode::Binary || Mode == SearchMode::Linear ||
                        Mode == SearchMode::SIMD,
                    "Invalid SearchMode");
    }
  }

  // Separate arrays for keys and values
  std::array<Key, Length> keys_;
  std::array<Value, Length> values_;
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
template <Comparable Key, typename Value, std::size_t Length, SearchMode Mode,
          bool IsConst>
typename ordered_array<Key, Value, Length,
                       Mode>::template ordered_array_iterator<IsConst>
operator+(typename ordered_array<Key, Value, Length, Mode>::
              template ordered_array_iterator<IsConst>::difference_type n,
          const typename ordered_array<Key, Value, Length, Mode>::
              template ordered_array_iterator<IsConst>& it) {
  return it + n;
}

}  // namespace fast_containers
