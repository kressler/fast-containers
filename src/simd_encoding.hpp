#ifndef FAST_CONTAINERS_SIMD_ENCODING_HPP
#define FAST_CONTAINERS_SIMD_ENCODING_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fast_containers {

/**
 * @brief Helper functions for encoding primitive types to byte arrays for SIMD
 * search with lexicographic comparison.
 *
 * These functions transform values into big-endian byte arrays where
 * lexicographic byte comparison produces the same ordering as the original
 * type's comparison operator.
 *
 * Use cases:
 * - Enable SIMD search for types that don't directly support SIMD
 * - Build composite keys from multiple fields
 * - Store keys in a format optimized for byte-level comparison
 *
 * Example:
 * @code
 * ordered_array<std::array<std::byte, 4>, Value, 64, SearchMode::SIMD> arr;
 * arr.insert(encode_float(3.14f), value);  // SIMD search with float key
 * @endcode
 */

// ============================================================================
// Integer Encoding
// ============================================================================

/**
 * @brief Encode signed 32-bit integer to sortable byte array.
 *
 * Transforms int32_t to big-endian byte array with sign bit flipped,
 * ensuring lexicographic byte comparison matches numeric ordering.
 *
 * @param value The integer to encode
 * @return Big-endian byte array representation
 */
inline std::array<std::byte, 4> encode_int32(int32_t value) {
  // Flip sign bit: maps negative values to 0x00000000-0x7FFFFFFF,
  // positive values to 0x80000000-0xFFFFFFFF
  uint32_t bits = static_cast<uint32_t>(value) ^ 0x80000000u;

  // Convert to big-endian byte array using bswap
  bits = __builtin_bswap32(bits);
  std::array<std::byte, 4> result;
  *reinterpret_cast<uint32_t*>(result.data()) = bits;
  return result;
}

/**
 * @brief Encode unsigned 32-bit integer to sortable byte array.
 *
 * @param value The unsigned integer to encode
 * @return Big-endian byte array representation
 */
inline std::array<std::byte, 4> encode_uint32(uint32_t value) {
  // Unsigned already has correct ordering, just convert to big-endian
  value = __builtin_bswap32(value);
  std::array<std::byte, 4> result;
  *reinterpret_cast<uint32_t*>(result.data()) = value;
  return result;
}

/**
 * @brief Encode signed 64-bit integer to sortable byte array.
 *
 * @param value The integer to encode
 * @return Big-endian byte array representation
 */
inline std::array<std::byte, 8> encode_int64(int64_t value) {
  // Flip sign bit
  uint64_t bits = static_cast<uint64_t>(value) ^ 0x8000000000000000ull;

  // Convert to big-endian byte array using bswap
  bits = __builtin_bswap64(bits);
  std::array<std::byte, 8> result;
  *reinterpret_cast<uint64_t*>(result.data()) = bits;
  return result;
}

/**
 * @brief Encode unsigned 64-bit integer to sortable byte array.
 *
 * @param value The unsigned integer to encode
 * @return Big-endian byte array representation
 */
inline std::array<std::byte, 8> encode_uint64(uint64_t value) {
  // Unsigned already has correct ordering, just convert to big-endian
  value = __builtin_bswap64(value);
  std::array<std::byte, 8> result;
  *reinterpret_cast<uint64_t*>(result.data()) = value;
  return result;
}

// ============================================================================
// Floating Point Encoding
// ============================================================================

/**
 * @brief Encode float to sortable byte array.
 *
 * Uses bit manipulation to transform IEEE 754 float representation into
 * a format where lexicographic byte comparison matches numeric ordering.
 *
 * Algorithm:
 * - Positive values: XOR with 0x80000000 (flip sign bit)
 * - Negative values: XOR with 0xFFFFFFFF (flip all bits)
 *
 * This ensures:
 * - Negative values sort before positive values
 * - Within negative values, more negative sorts first
 * - Within positive values, smaller sorts first
 * - Special values (±0, ±Inf, NaN) sort correctly
 *
 * @param value The float to encode
 * @return Big-endian byte array representation
 */
inline std::array<std::byte, 4> encode_float(float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(float));

  // Transform for sortable comparison:
  // If negative (sign bit = 1): flip all bits
  // If positive (sign bit = 0): flip only sign bit
  uint32_t mask = -static_cast<int32_t>(bits >> 31) | 0x80000000u;
  uint32_t sortable = bits ^ mask;

  // Convert to big-endian byte array using bswap
  sortable = __builtin_bswap32(sortable);
  std::array<std::byte, 4> result;
  *reinterpret_cast<uint32_t*>(result.data()) = sortable;
  return result;
}

/**
 * @brief Encode double to sortable byte array.
 *
 * Same algorithm as encode_float, but for 64-bit doubles.
 *
 * @param value The double to encode
 * @return Big-endian byte array representation
 */
inline std::array<std::byte, 8> encode_double(double value) {
  uint64_t bits;
  std::memcpy(&bits, &value, sizeof(double));

  // Transform for sortable comparison
  uint64_t mask = -static_cast<int64_t>(bits >> 63) | 0x8000000000000000ull;
  uint64_t sortable = bits ^ mask;

  // Convert to big-endian byte array using bswap
  sortable = __builtin_bswap64(sortable);
  std::array<std::byte, 8> result;
  *reinterpret_cast<uint64_t*>(result.data()) = sortable;
  return result;
}

// ============================================================================
// Decoding Functions
// ============================================================================

/**
 * @brief Decode byte array back to int32_t.
 *
 * @param encoded Big-endian byte array from encode_int32
 * @return Original int32_t value
 */
inline int32_t decode_int32(const std::array<std::byte, 4>& encoded) {
  uint32_t bits = *reinterpret_cast<const uint32_t*>(encoded.data());
  bits = __builtin_bswap32(bits);
  // Undo sign bit flip
  return static_cast<int32_t>(bits ^ 0x80000000u);
}

/**
 * @brief Decode byte array back to uint32_t.
 */
inline uint32_t decode_uint32(const std::array<std::byte, 4>& encoded) {
  uint32_t bits = *reinterpret_cast<const uint32_t*>(encoded.data());
  return __builtin_bswap32(bits);
}

/**
 * @brief Decode byte array back to int64_t.
 */
inline int64_t decode_int64(const std::array<std::byte, 8>& encoded) {
  uint64_t bits = *reinterpret_cast<const uint64_t*>(encoded.data());
  bits = __builtin_bswap64(bits);
  // Undo sign bit flip
  return static_cast<int64_t>(bits ^ 0x8000000000000000ull);
}

/**
 * @brief Decode byte array back to uint64_t.
 */
inline uint64_t decode_uint64(const std::array<std::byte, 8>& encoded) {
  uint64_t bits = *reinterpret_cast<const uint64_t*>(encoded.data());
  return __builtin_bswap64(bits);
}

/**
 * @brief Decode byte array back to float.
 */
inline float decode_float(const std::array<std::byte, 4>& encoded) {
  uint32_t sortable = *reinterpret_cast<const uint32_t*>(encoded.data());
  sortable = __builtin_bswap32(sortable);

  // Undo the transformation
  uint32_t mask = ((sortable >> 31) - 1) | 0x80000000u;
  uint32_t bits = sortable ^ mask;

  float result;
  std::memcpy(&result, &bits, sizeof(float));
  return result;
}

/**
 * @brief Decode byte array back to double.
 */
inline double decode_double(const std::array<std::byte, 8>& encoded) {
  uint64_t sortable = *reinterpret_cast<const uint64_t*>(encoded.data());
  sortable = __builtin_bswap64(sortable);

  // Undo the transformation
  uint64_t mask = ((sortable >> 63) - 1) | 0x8000000000000000ull;
  uint64_t bits = sortable ^ mask;

  double result;
  std::memcpy(&result, &bits, sizeof(double));
  return result;
}

}  // namespace fast_containers

#endif  // FAST_CONTAINERS_SIMD_ENCODING_HPP
