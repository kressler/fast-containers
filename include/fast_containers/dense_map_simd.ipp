// Copyright (c) 2025 Bryan Kressler
//
// SPDX-License-Identifier: BSD-3-Clause

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
    // For descending (std::greater): find first position where key <=
    // search_key
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
    // For descending (std::greater): find first position where key <=
    // search_key
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
      // For descending (std::greater): find first position where key <=
      // search_key
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
      // For descending (std::greater): find first position where key <=
      // search_key
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
      // For descending (std::greater): find first position where key <=
      // search_key
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
      // For descending (std::greater): find first position where key <=
      // search_key
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
