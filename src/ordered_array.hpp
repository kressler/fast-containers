#pragma once

#include <array>
#include <algorithm>
#include <stdexcept>
#include <concepts>
#include <utility>

namespace fast_containers {

// Concept to enforce that Key type supports comparison operations
template<typename T>
concept Comparable = requires(T a, T b) {
    { a < b } -> std::convertible_to<bool>;
    { a == b } -> std::convertible_to<bool>;
};

/**
 * A fixed-size ordered array that maintains key-value pairs in sorted order.
 *
 * @tparam Key The key type (must be Comparable)
 * @tparam Value The value type
 * @tparam Length The maximum number of elements
 */
template<Comparable Key, typename Value, std::size_t Length>
class ordered_array {
public:
    // Type aliases for key-value pair
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<Key, Value>;
    using size_type = std::size_t;

    // Iterator types
    using iterator = typename std::array<value_type, Length>::iterator;
    using const_iterator = typename std::array<value_type, Length>::const_iterator;
    using reverse_iterator = typename std::array<value_type, Length>::reverse_iterator;
    using const_reverse_iterator = typename std::array<value_type, Length>::const_reverse_iterator;

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
        auto pos = std::lower_bound(
            data_.begin(),
            data_.begin() + size_,
            key,
            [](const value_type& pair, const Key& k) {
                return pair.first < k;
            }
        );

        // Check if key already exists
        if (pos != data_.begin() + size_ && pos->first == key) {
            throw std::runtime_error("Cannot insert: key already exists");
        }

        // Shift elements to the right to make space
        std::move_backward(pos, data_.begin() + size_, data_.begin() + size_ + 1);

        // Insert the new element
        *pos = std::make_pair(key, value);
        ++size_;
    }

    /**
     * Find an element by key.
     *
     * @param key The key to search for
     * @return Iterator to the found element, or end() if not found
     */
    iterator find(const Key& key) {
        auto pos = std::lower_bound(
            data_.begin(),
            data_.begin() + size_,
            key,
            [](const value_type& pair, const Key& k) {
                return pair.first < k;
            }
        );

        if (pos != data_.begin() + size_ && pos->first == key) {
            return pos;
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
        auto pos = std::lower_bound(
            data_.begin(),
            data_.begin() + size_,
            key,
            [](const value_type& pair, const Key& k) {
                return pair.first < k;
            }
        );

        if (pos != data_.begin() + size_ && pos->first == key) {
            return pos;
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
        auto pos = find(key);
        if (pos != end()) {
            // Shift elements to the left to fill the gap
            std::move(pos + 1, data_.begin() + size_, pos);
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
        auto pos = find(key);
        if (pos != end()) {
            return pos->second;
        }

        // Need to insert - check if there's space
        if (size_ >= Length) {
            throw std::runtime_error("Cannot insert: array is full");
        }

        // Find insertion position
        auto insert_pos = std::lower_bound(
            data_.begin(),
            data_.begin() + size_,
            key,
            [](const value_type& pair, const Key& k) {
                return pair.first < k;
            }
        );

        // Shift elements to make space
        std::move_backward(insert_pos, data_.begin() + size_, data_.begin() + size_ + 1);

        // Insert with default-constructed value
        *insert_pos = std::make_pair(key, Value{});
        ++size_;

        return insert_pos->second;
    }

    // Iterator methods
    iterator begin() { return data_.begin(); }
    iterator end() { return data_.begin() + size_; }
    const_iterator begin() const { return data_.begin(); }
    const_iterator end() const { return data_.begin() + size_; }
    const_iterator cbegin() const { return data_.begin(); }
    const_iterator cend() const { return data_.begin() + size_; }

    // Reverse iterator methods
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator crend() const { return const_reverse_iterator(begin()); }

    // Utility methods
    size_type size() const { return size_; }
    constexpr size_type capacity() const { return Length; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == Length; }

    void clear() { size_ = 0; }

private:
    std::array<value_type, Length> data_;
    size_type size_;
};

} // namespace fast_containers
