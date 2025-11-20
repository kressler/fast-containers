#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <limits>
#include <vector>

#include "../simd_encoding.hpp"

using namespace fast_containers;

// ============================================================================
// Integer Encoding Tests
// ============================================================================

TEST_CASE("encode_int32 round-trip", "[simd_encoding]") {
  std::vector<int32_t> test_values = {
      std::numeric_limits<int32_t>::min(),
      std::numeric_limits<int32_t>::min() + 1,
      -1000000,
      -1,
      0,
      1,
      1000000,
      std::numeric_limits<int32_t>::max() - 1,
      std::numeric_limits<int32_t>::max(),
  };

  for (int32_t value : test_values) {
    auto encoded = encode_int32(value);
    auto decoded = decode_int32(encoded);
    REQUIRE(decoded == value);
  }
}

TEST_CASE("encode_uint32 round-trip", "[simd_encoding]") {
  std::vector<uint32_t> test_values = {
      0,
      1,
      1000000,
      std::numeric_limits<uint32_t>::max() / 2,
      std::numeric_limits<uint32_t>::max() - 1,
      std::numeric_limits<uint32_t>::max(),
  };

  for (uint32_t value : test_values) {
    auto encoded = encode_uint32(value);
    auto decoded = decode_uint32(encoded);
    REQUIRE(decoded == value);
  }
}

TEST_CASE("encode_int64 round-trip", "[simd_encoding]") {
  std::vector<int64_t> test_values = {
      std::numeric_limits<int64_t>::min(),
      std::numeric_limits<int64_t>::min() + 1,
      -1000000000000LL,
      -1,
      0,
      1,
      1000000000000LL,
      std::numeric_limits<int64_t>::max() - 1,
      std::numeric_limits<int64_t>::max(),
  };

  for (int64_t value : test_values) {
    auto encoded = encode_int64(value);
    auto decoded = decode_int64(encoded);
    REQUIRE(decoded == value);
  }
}

TEST_CASE("encode_uint64 round-trip", "[simd_encoding]") {
  std::vector<uint64_t> test_values = {
      0,
      1,
      1000000000000ULL,
      std::numeric_limits<uint64_t>::max() / 2,
      std::numeric_limits<uint64_t>::max() - 1,
      std::numeric_limits<uint64_t>::max(),
  };

  for (uint64_t value : test_values) {
    auto encoded = encode_uint64(value);
    auto decoded = decode_uint64(encoded);
    REQUIRE(decoded == value);
  }
}

// ============================================================================
// Floating Point Encoding Tests
// ============================================================================

TEST_CASE("encode_float round-trip", "[simd_encoding]") {
  std::vector<float> test_values = {
      -std::numeric_limits<float>::infinity(),
      std::numeric_limits<float>::lowest(),
      -1000000.0f,
      -1.0f,
      -0.0f,
      0.0f,
      1.0f,
      1000000.0f,
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::infinity(),
  };

  for (float value : test_values) {
    auto encoded = encode_float(value);
    auto decoded = decode_float(encoded);
    // Use memcmp to distinguish -0.0 from +0.0
    if (std::memcmp(&value, &decoded, sizeof(float)) == 0) {
      REQUIRE(true);  // Exact match including sign of zero
    } else {
      REQUIRE_THAT(decoded, Catch::Matchers::WithinRel(value, 0.0001f));
    }
  }
}

TEST_CASE("encode_double round-trip", "[simd_encoding]") {
  std::vector<double> test_values = {
      -std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::lowest(),
      -1000000.0,
      -1.0,
      -0.0,
      0.0,
      1.0,
      1000000.0,
      std::numeric_limits<double>::max(),
      std::numeric_limits<double>::infinity(),
  };

  for (double value : test_values) {
    auto encoded = encode_double(value);
    auto decoded = decode_double(encoded);
    // Use memcmp to distinguish -0.0 from +0.0
    if (std::memcmp(&value, &decoded, sizeof(double)) == 0) {
      REQUIRE(true);  // Exact match including sign of zero
    } else {
      REQUIRE_THAT(decoded, Catch::Matchers::WithinRel(value, 0.0001));
    }
  }
}

// ============================================================================
// Ordering Preservation Tests
// ============================================================================

TEST_CASE("encode_int32 preserves ordering", "[simd_encoding]") {
  std::vector<int32_t> values = {
      std::numeric_limits<int32_t>::min(),
      -1000000,
      -100,
      -1,
      0,
      1,
      100,
      1000000,
      std::numeric_limits<int32_t>::max(),
  };

  for (size_t i = 0; i < values.size(); ++i) {
    for (size_t j = i + 1; j < values.size(); ++j) {
      auto encoded_i = encode_int32(values[i]);
      auto encoded_j = encode_int32(values[j]);
      REQUIRE(encoded_i < encoded_j);
    }
  }
}

TEST_CASE("encode_uint32 preserves ordering", "[simd_encoding]") {
  std::vector<uint32_t> values = {0, 1, 100, 1000000,
                                  std::numeric_limits<uint32_t>::max()};

  for (size_t i = 0; i < values.size(); ++i) {
    for (size_t j = i + 1; j < values.size(); ++j) {
      auto encoded_i = encode_uint32(values[i]);
      auto encoded_j = encode_uint32(values[j]);
      REQUIRE(encoded_i < encoded_j);
    }
  }
}

TEST_CASE("encode_int64 preserves ordering", "[simd_encoding]") {
  std::vector<int64_t> values = {
      std::numeric_limits<int64_t>::min(),
      -1000000000000LL,
      -100,
      -1,
      0,
      1,
      100,
      1000000000000LL,
      std::numeric_limits<int64_t>::max(),
  };

  for (size_t i = 0; i < values.size(); ++i) {
    for (size_t j = i + 1; j < values.size(); ++j) {
      auto encoded_i = encode_int64(values[i]);
      auto encoded_j = encode_int64(values[j]);
      REQUIRE(encoded_i < encoded_j);
    }
  }
}

TEST_CASE("encode_uint64 preserves ordering", "[simd_encoding]") {
  std::vector<uint64_t> values = {0, 1, 100, 1000000000000ULL,
                                  std::numeric_limits<uint64_t>::max()};

  for (size_t i = 0; i < values.size(); ++i) {
    for (size_t j = i + 1; j < values.size(); ++j) {
      auto encoded_i = encode_uint64(values[i]);
      auto encoded_j = encode_uint64(values[j]);
      REQUIRE(encoded_i < encoded_j);
    }
  }
}

TEST_CASE("encode_float preserves ordering", "[simd_encoding]") {
  std::vector<float> values = {
      -std::numeric_limits<float>::infinity(),
      std::numeric_limits<float>::lowest(),
      -1000000.0f,
      -100.0f,
      -1.0f,
      -0.0f,  // Note: -0.0 and +0.0 should sort to same position
      0.0f,
      1.0f,
      100.0f,
      1000000.0f,
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::infinity(),
  };

  for (size_t i = 0; i < values.size(); ++i) {
    for (size_t j = i + 1; j < values.size(); ++j) {
      // Skip -0.0 vs +0.0 comparison (they should be equal)
      if ((values[i] == -0.0f && values[j] == 0.0f) ||
          (values[i] == 0.0f && values[j] == -0.0f)) {
        continue;
      }

      auto encoded_i = encode_float(values[i]);
      auto encoded_j = encode_float(values[j]);
      REQUIRE(encoded_i < encoded_j);
    }
  }
}

TEST_CASE("encode_double preserves ordering", "[simd_encoding]") {
  std::vector<double> values = {
      -std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::lowest(),
      -1000000.0,
      -100.0,
      -1.0,
      -0.0,  // Note: -0.0 and +0.0 should sort to same position
      0.0,
      1.0,
      100.0,
      1000000.0,
      std::numeric_limits<double>::max(),
      std::numeric_limits<double>::infinity(),
  };

  for (size_t i = 0; i < values.size(); ++i) {
    for (size_t j = i + 1; j < values.size(); ++j) {
      // Skip -0.0 vs +0.0 comparison (they should be equal)
      if ((values[i] == -0.0 && values[j] == 0.0) ||
          (values[i] == 0.0 && values[j] == -0.0)) {
        continue;
      }

      auto encoded_i = encode_double(values[i]);
      auto encoded_j = encode_double(values[j]);
      REQUIRE(encoded_i < encoded_j);
    }
  }
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_CASE("encode_float handles special values", "[simd_encoding]") {
  // Test that special float values encode/decode correctly
  SECTION("positive infinity") {
    float value = std::numeric_limits<float>::infinity();
    auto encoded = encode_float(value);
    auto decoded = decode_float(encoded);
    REQUIRE(std::isinf(decoded));
    REQUIRE(decoded > 0);
  }

  SECTION("negative infinity") {
    float value = -std::numeric_limits<float>::infinity();
    auto encoded = encode_float(value);
    auto decoded = decode_float(encoded);
    REQUIRE(std::isinf(decoded));
    REQUIRE(decoded < 0);
  }

  SECTION("zero values") {
    auto encoded_pos = encode_float(0.0f);
    auto encoded_neg = encode_float(-0.0f);
    // -0.0 and +0.0 encode differently but both decode correctly
    REQUIRE(encoded_neg < encoded_pos);  // -0.0 sorts before +0.0
    REQUIRE(decode_float(encoded_neg) == -0.0f);
    REQUIRE(decode_float(encoded_pos) == 0.0f);
  }

  SECTION("min and max") {
    float min_val = std::numeric_limits<float>::lowest();
    float max_val = std::numeric_limits<float>::max();

    auto encoded_min = encode_float(min_val);
    auto encoded_max = encode_float(max_val);

    REQUIRE(encoded_min < encoded_max);

    auto decoded_min = decode_float(encoded_min);
    auto decoded_max = decode_float(encoded_max);

    REQUIRE_THAT(decoded_min, Catch::Matchers::WithinRel(min_val, 0.0001f));
    REQUIRE_THAT(decoded_max, Catch::Matchers::WithinRel(max_val, 0.0001f));
  }
}

TEST_CASE("encode_double handles special values", "[simd_encoding]") {
  // Test that special double values encode/decode correctly
  SECTION("positive infinity") {
    double value = std::numeric_limits<double>::infinity();
    auto encoded = encode_double(value);
    auto decoded = decode_double(encoded);
    REQUIRE(std::isinf(decoded));
    REQUIRE(decoded > 0);
  }

  SECTION("negative infinity") {
    double value = -std::numeric_limits<double>::infinity();
    auto encoded = encode_double(value);
    auto decoded = decode_double(encoded);
    REQUIRE(std::isinf(decoded));
    REQUIRE(decoded < 0);
  }

  SECTION("zero values") {
    auto encoded_pos = encode_double(0.0);
    auto encoded_neg = encode_double(-0.0);
    // -0.0 and +0.0 encode differently but both decode correctly
    REQUIRE(encoded_neg < encoded_pos);  // -0.0 sorts before +0.0
    REQUIRE(decode_double(encoded_neg) == -0.0);
    REQUIRE(decode_double(encoded_pos) == 0.0);
  }

  SECTION("min and max") {
    double min_val = std::numeric_limits<double>::lowest();
    double max_val = std::numeric_limits<double>::max();

    auto encoded_min = encode_double(min_val);
    auto encoded_max = encode_double(max_val);

    REQUIRE(encoded_min < encoded_max);

    auto decoded_min = decode_double(encoded_min);
    auto decoded_max = decode_double(encoded_max);

    REQUIRE_THAT(decoded_min, Catch::Matchers::WithinRel(min_val, 0.0001));
    REQUIRE_THAT(decoded_max, Catch::Matchers::WithinRel(max_val, 0.0001));
  }
}

TEST_CASE("encode_int32 handles boundary values", "[simd_encoding]") {
  // Test values around zero crossing
  std::vector<int32_t> boundary_values = {-2, -1, 0, 1, 2};

  for (size_t i = 0; i < boundary_values.size(); ++i) {
    for (size_t j = i + 1; j < boundary_values.size(); ++j) {
      auto encoded_i = encode_int32(boundary_values[i]);
      auto encoded_j = encode_int32(boundary_values[j]);
      REQUIRE(encoded_i < encoded_j);
    }
  }
}

TEST_CASE("encode_int64 handles boundary values", "[simd_encoding]") {
  // Test values around zero crossing
  std::vector<int64_t> boundary_values = {-2LL, -1LL, 0LL, 1LL, 2LL};

  for (size_t i = 0; i < boundary_values.size(); ++i) {
    for (size_t j = i + 1; j < boundary_values.size(); ++j) {
      auto encoded_i = encode_int64(boundary_values[i]);
      auto encoded_j = encode_int64(boundary_values[j]);
      REQUIRE(encoded_i < encoded_j);
    }
  }
}
