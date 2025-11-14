#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace fast_containers {

// Concept to enforce that Key type supports comparison operations
template <typename T>
concept Comparable = requires(T a, T b) {
  { a < b } -> std::convertible_to<bool>;
  { a == b } -> std::convertible_to<bool>;
};

/**
 * A fixed-size ordered array that maintains key-value pairs in sorted order.
 * Keys and values are stored in separate arrays for better cache locality
 * and to enable SIMD optimizations.
 *
 * @tparam Key The key type (must be Comparable)
 * @tparam Value The value type
 * @tparam Length The maximum number of elements
 */
template <Comparable Key, typename Value, std::size_t Length>
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

    // Find the position where the key should be inserted using binary search
    auto pos = std::lower_bound(keys_.begin(), keys_.begin() + size_, key);

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
    auto pos = std::lower_bound(keys_.begin(), keys_.begin() + size_, key);
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
    auto pos = std::lower_bound(keys_.begin(), keys_.begin() + size_, key);
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
    auto pos = std::lower_bound(keys_.begin(), keys_.begin() + size_, key);
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
  // Separate arrays for keys and values
  std::array<Key, Length> keys_;
  std::array<Value, Length> values_;
  size_type size_;

  // Proxy class to represent a key-value pair reference
  template <bool IsConst>
  class pair_proxy {
   public:
    using key_ref_type = std::conditional_t<IsConst, const Key&, Key&>;
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
template <Comparable Key, typename Value, std::size_t Length, bool IsConst>
typename ordered_array<Key, Value,
                       Length>::template ordered_array_iterator<IsConst>
operator+(
    typename ordered_array<Key, Value, Length>::template ordered_array_iterator<
        IsConst>::difference_type n,
    const typename ordered_array<
        Key, Value, Length>::template ordered_array_iterator<IsConst>& it) {
  return it + n;
}

}  // namespace fast_containers
