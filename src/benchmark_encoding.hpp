#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fast_containers {

/**
 * Simple encoding functions for benchmarking byte array SIMD performance.
 * These convert integers to big-endian byte arrays for lexicographic ordering.
 */

inline std::array<std::byte, 8> encode_int64(int64_t value) {
  // Flip sign bit for correct ordering, then convert to big-endian
  uint64_t unsigned_value =
      static_cast<uint64_t>(value) ^ 0x8000000000000000ULL;
  unsigned_value = __builtin_bswap64(unsigned_value);

  std::array<std::byte, 8> result;
  std::memcpy(result.data(), &unsigned_value, 8);
  return result;
}

inline std::array<std::byte, 16> encode_int64_pair(int64_t a, int64_t b) {
  auto enc_a = encode_int64(a);
  auto enc_b = encode_int64(b);

  std::array<std::byte, 16> result;
  std::memcpy(result.data(), enc_a.data(), 8);
  std::memcpy(result.data() + 8, enc_b.data(), 8);
  return result;
}

inline std::array<std::byte, 32> encode_int64_quad(int64_t a, int64_t b,
                                                   int64_t c, int64_t d) {
  auto enc_a = encode_int64(a);
  auto enc_b = encode_int64(b);
  auto enc_c = encode_int64(c);
  auto enc_d = encode_int64(d);

  std::array<std::byte, 32> result;
  std::memcpy(result.data(), enc_a.data(), 8);
  std::memcpy(result.data() + 8, enc_b.data(), 8);
  std::memcpy(result.data() + 16, enc_c.data(), 8);
  std::memcpy(result.data() + 24, enc_d.data(), 8);
  return result;
}

}  // namespace fast_containers
