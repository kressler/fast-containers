#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace kressler::fast_containers {

// Enum to control search strategy
enum class SearchMode {
  Binary,  // Binary search using std::lower_bound (O(log n))
  Linear,  // Linear search for small arrays (better cache behavior)
  SIMD     // SIMD-accelerated linear search for small arrays
};

// Concept to enforce that a comparator is compatible with a key type
template <typename Key, typename Compare>
concept ComparatorCompatible = requires(Compare comp, Key a, Key b) {
  { comp(a, b) } -> std::convertible_to<bool>;
} && std::is_default_constructible_v<Compare>;

// Concept for primitive types that have well-defined SIMD comparison semantics
template <typename T>
concept SimdPrimitive =
    // 8-bit integer types
    std::is_same_v<T, int8_t> || std::is_same_v<T, uint8_t> ||
    // 16-bit integer types
    std::is_same_v<T, int16_t> || std::is_same_v<T, uint16_t> ||
    // 32-bit integer types
    std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> ||
    // 64-bit integer types
    std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t> ||
    // Floating point types
    std::is_same_v<T, float> || std::is_same_v<T, double> ||
    // Common aliases that may map to standard types
    (std::is_same_v<T, char> && sizeof(char) == 1) ||
    (std::is_same_v<T, signed char> && sizeof(signed char) == 1) ||
    (std::is_same_v<T, unsigned char> && sizeof(unsigned char) == 1) ||
    (std::is_same_v<T, short> && sizeof(short) == 2) ||
    (std::is_same_v<T, unsigned short> && sizeof(unsigned short) == 2) ||
    (std::is_same_v<T, int> && sizeof(int) == 4) ||
    (std::is_same_v<T, unsigned int> && sizeof(unsigned int) == 4) ||
    (std::is_same_v<T, long> && sizeof(long) == 8) ||
    (std::is_same_v<T, unsigned long> && sizeof(unsigned long) == 8);

// Concept for types that can use SIMD-accelerated search
// Only supports primitive types with well-defined SIMD comparison semantics
template <typename T>
concept SIMDSearchable = std::is_trivially_copyable_v<T> && SimdPrimitive<T>;

/**
 * A fixed-size ordered array that maintains key-value pairs in sorted order.
 * Keys and values are stored in separate arrays for better cache locality
 * and to enable SIMD optimizations.
 *
 * @tparam Key The key type
 * @tparam Value The value type
 * @tparam Compare The comparison function object type (must be
 * default-constructible)
 * @tparam Length The maximum number of elements
 * @tparam SearchModeT The search mode (Binary, Linear, or SIMD)
 */
template <typename Key, typename Value, std::size_t Length,
          typename Compare = std::less<Key>,
          SearchMode SearchModeT = SearchMode::Binary>
  requires ComparatorCompatible<Key, Compare>
class ordered_array {
 private:
  // Forward declare iterator classes
  template <bool IsConst>
  class ordered_array_iterator;

  // Allow different instantiations to access each other's private members
  template <typename K, typename V, std::size_t L, typename C, SearchMode SM>
    requires ComparatorCompatible<K, C>
  friend class ordered_array;

 public:
  // Type aliases for key-value pair
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<Key, Value>;
  using size_type = std::size_t;
  using key_compare = Compare;

  // Iterator types
  using iterator = ordered_array_iterator<false>;
  using const_iterator = ordered_array_iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // Static assertions for SIMD mode requirements
  static_assert(
      SearchModeT != SearchMode::SIMD ||
          (SIMDSearchable<Key> && (std::is_same_v<Compare, std::less<Key>> ||
                                   std::is_same_v<Compare, std::greater<Key>>)),
      "SearchMode::SIMD requires:\n"
      "  1. Key type must be a primitive type (int8_t, uint8_t, int16_t, "
      "uint16_t, int32_t, uint32_t, int64_t, uint64_t, float, double)\n"
      "  2. Compare must be std::less<Key> or std::greater<Key>\n"
      "For custom comparators, use SearchMode::Binary or SearchMode::Linear");

  /**
   * Default constructor - initializes an empty ordered array
   */
  ordered_array() : size_(0), comp_() {}

  /**
   * Copy constructor - creates a deep copy of another ordered array
   * Complexity: O(n) where n is the size of the other array
   *
   * @param other The ordered array to copy from
   */
  ordered_array(const ordered_array& other);

  /**
   * Move constructor - transfers ownership from another ordered array
   * Note: Due to inline storage, this is still O(n) as data must be copied.
   * The move only provides benefit for non-POD Value types.
   *
   * @param other The ordered array to move from (will be left empty)
   */
  ordered_array(ordered_array&& other) noexcept;

  /**
   * Copy assignment operator - replaces contents with a copy of another array
   * Complexity: O(n) where n is the size of the other array
   *
   * @param other The ordered array to copy from
   * @return Reference to this array
   */
  ordered_array& operator=(const ordered_array& other);

  /**
   * Move assignment operator - replaces contents by moving from another array
   * Note: Due to inline storage, this is still O(n) as data must be copied.
   * The move only provides benefit for non-POD Value types.
   *
   * @param other The ordered array to move from (will be left empty)
   * @return Reference to this array
   */
  ordered_array& operator=(ordered_array&& other) noexcept;

  /**
   * Insert a new key-value pair into the array in sorted order.
   *
   * @param key The key to insert
   * @param value The value to insert
   * @return A pair consisting of an iterator to the inserted element (or to the
   *         element that prevented insertion) and a bool indicating whether
   *         insertion took place (true if inserted, false if key already
   * exists)
   * @throws std::runtime_error if the array is full
   */
  std::pair<iterator, bool> insert(const Key& key, const Value& value);

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
  std::pair<iterator, bool> insert_hint(iterator hint, const Key& key,
                                        const Value& value);

  /**
   * Inserts a new element if the key does not exist.
   * Constructs the value in-place using the provided arguments.
   * If the key already exists, does nothing (does not construct the value).
   *
   * @param key The key to insert
   * @param args Arguments to forward to the Value constructor
   * @return Pair of iterator to inserted/existing element and bool indicating
   * success
   * @throws std::runtime_error if the array is full
   */
  template <typename... Args>
  std::pair<iterator, bool> try_emplace(const Key& key, Args&&... args);

  /**
   * Inserts a new element or assigns to an existing one.
   * If the key exists, assigns the new value to the existing element.
   * If the key doesn't exist, inserts a new element with the value.
   *
   * @param key The key to insert or assign
   * @param value The value to insert or assign
   * @return Pair of iterator to inserted/updated element and bool indicating
   * insertion (true) vs assignment (false)
   * @throws std::runtime_error if the array is full and key doesn't exist
   */
  template <typename M>
  std::pair<iterator, bool> insert_or_assign(const Key& key, M&& value);

  /**
   * Find an element by key.
   *
   * @param key The key to search for
   * @return Iterator to the found element, or end() if not found
   */
  iterator find(const Key& key);

  /**
   * Find an element by key (const version).
   *
   * @param key The key to search for
   * @return Const iterator to the found element, or end() if not found
   */
  const_iterator find(const Key& key) const;

  /**
   * Find the first element not less than the given key.
   *
   * @param key The key to search for
   * @return Iterator to the first element >= key, or end() if all elements <
   * key
   */
  iterator lower_bound(const Key& key);

  /**
   * Find the first element not less than the given key (const version).
   *
   * @param key The key to search for
   * @return Const iterator to the first element >= key, or end() if all
   * elements < key
   */
  const_iterator lower_bound(const Key& key) const;

  /**
   * Find the first element greater than the given key.
   *
   * @param key The key to search for
   * @return Iterator to the first element > key, or end() if all elements <=
   * key
   */
  iterator upper_bound(const Key& key);

  /**
   * Find the first element greater than the given key (const version).
   *
   * @param key The key to search for
   * @return Const iterator to the first element > key, or end() if all
   * elements <= key
   */
  const_iterator upper_bound(const Key& key) const;

  /**
   * Erase an element by iterator.
   * More efficient than key-based erase when you already have an iterator.
   *
   * @param pos Iterator to the element to erase
   * @return Iterator to the element following the erased element
   */
  iterator erase(iterator pos);

  /**
   * Erase an element by key.
   * Implemented as find() + iterator erase().
   * If the element does not exist, does nothing.
   *
   * @param key The key to erase
   * @return The number of elements erased (0 or 1)
   */
  size_type erase(const Key& key);

  /**
   * Updates the key at the given position without maintaining sorted order.
   * UNSAFE: Caller MUST guarantee that the new key maintains sorted order:
   *   - key at (position - 1) < new_key, if it exists (strict inequality)
   *   - new_key < key at (position + 1), if it exists (strict inequality)
   *
   * No duplicate keys are allowed (strict ordering enforced).
   *
   * This is an optimization to avoid array shifts when the caller knows
   * the new key will remain at the same position in sorted order.
   *
   * Debug builds assert that sorted order is maintained.
   *
   * @param it Iterator to the element whose key should be updated
   * @param new_key The new key value
   */
  void unsafe_update_key(iterator it, const Key& new_key) {
    assert(it.index() < size_ && "Iterator out of bounds");

    const size_type index = it.index();

    // Debug assertions: verify sorted order is maintained (no duplicates)
    // If not first element: comp_(previous key, new_key) (strict inequality)
    assert(index == 0 || comp_(keys_[index - 1], new_key) &&
                             "New key violates sorted order (not greater than "
                             "previous key)");

    // If not last element: comp_(new_key, next key) (strict inequality)
    assert(index == size_ - 1 || comp_(new_key, keys_[index + 1]) &&
                                     "New key violates sorted order (not less "
                                     "than next key)");

    // Update key in place
    keys_[index] = new_key;
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
  Value& operator[](const Key& key);

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
      ordered_array<Key, Value, OutputLength, Compare, SearchModeT>& output);

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
      ordered_array<Key, Value, SourceLength, Compare, SearchModeT>& source,
      size_type count);

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
      ordered_array<Key, Value, SourceLength, Compare, SearchModeT>& source,
      size_type count);

 private:
#ifdef __AVX2__
  /**
   * SIMD-accelerated linear search for 8-bit keys (int8_t, uint8_t)
   * Compares 32 keys at a time using AVX2
   */
  template <typename K>
    requires(sizeof(K) == 1)
  auto simd_lower_bound_1byte(const K& key) const;

  /**
   * SIMD-accelerated linear search for 16-bit keys (int16_t, uint16_t)
   * Compares 16 keys at a time using AVX2
   */
  template <typename K>
    requires(sizeof(K) == 2)
  auto simd_lower_bound_2byte(const K& key) const;

  /**
   * SIMD-accelerated linear search for 32-bit keys (int32_t, uint32_t, float)
   * Compares 8 keys at a time using AVX2
   */
  template <typename K>
    requires(sizeof(K) == 4)
  auto simd_lower_bound_4byte(const K& key) const;

  /**
   * SIMD-accelerated linear search for 64-bit keys (int64_t, uint64_t, double)
   * Compares 4 keys at a time using AVX2
   */
  template <typename K>
    requires(sizeof(K) == 8)
  auto simd_lower_bound_8byte(const K& key) const;
#endif

  /**
   * Find the insertion position for a key using the configured search mode.
   * Returns an iterator to the first element not less than the given key.
   *
   * @param key The key to search for
   * @return Iterator to the insertion position
   */
  auto lower_bound_key(const Key& key) const;

  // Separate arrays for keys and values
  // Align to 64-byte cache lines for optimal SIMD performance
  alignas(64) std::array<Key, Length> keys_;
  alignas(64) std::array<Value, Length> values_;
  size_type size_;
  [[no_unique_address]] Compare comp_;

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
template <typename Key, typename Value, std::size_t Length, typename Compare,
          SearchMode SearchModeT, bool IsConst>
typename ordered_array<Key, Value, Length, Compare,
                       SearchModeT>::template ordered_array_iterator<IsConst>
operator+(typename ordered_array<Key, Value, Length, Compare, SearchModeT>::
              template ordered_array_iterator<IsConst>::difference_type n,
          const typename ordered_array<
              Key, Value, Length, Compare,
              SearchModeT>::template ordered_array_iterator<IsConst>& it) {
  return it + n;
}

}  // namespace kressler::fast_containers

// Include implementation
#include "ordered_array.ipp"
