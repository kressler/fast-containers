#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <string>

#include "../btree.hpp"

using namespace fast_containers;

// Helper types for testing different SearchModes
struct BinarySearchMode {
  static constexpr SearchMode value = SearchMode::Binary;
};
struct LinearSearchMode {
  static constexpr SearchMode value = SearchMode::Linear;
};
struct SIMDSearchMode {
  static constexpr SearchMode value = SearchMode::SIMD;
};

TEMPLATE_TEST_CASE("btree default constructor creates empty tree",
                   "[btree][constructor]", BinarySearchMode, LinearSearchMode,
                   SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Default constructor - int, int") {
    btree<int, int, 64, 64, Mode> tree;
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }

  SECTION("Default constructor - int, std::string") {
    btree<int, std::string, 64, 64, Mode> tree;
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }

  SECTION("Default constructor - std::string, int") {
    btree<std::string, int, 64, 64, Mode> tree;
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }

  SECTION("Default constructor - different node sizes") {
    btree<int, int, 16, 64, Mode> tree;
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }

  SECTION("Default constructor - small node sizes") {
    btree<int, int, 4, 8, Mode> tree;
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }
}

TEMPLATE_TEST_CASE("btree destructor deallocates resources",
                   "[btree][destructor]", BinarySearchMode, LinearSearchMode,
                   SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Destructor on empty tree") {
    btree<int, int, 64, 64, Mode> tree;
    // Destructor called automatically - should not crash
  }

  SECTION("Multiple constructions and destructions") {
    for (int i = 0; i < 10; ++i) {
      btree<int, std::string, 32, 32, Mode> tree;
      REQUIRE(tree.empty());
    }
  }
}

TEMPLATE_TEST_CASE("btree size and empty methods", "[btree][size]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Empty tree reports size 0") {
    btree<int, int, 64, 64, Mode> tree;
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.empty() == true);
  }

  SECTION("Size and empty are consistent") {
    btree<int, std::string, 64, 64, Mode> tree;
    REQUIRE(tree.empty() == (tree.size() == 0));
  }
}

TEMPLATE_TEST_CASE("btree works with different template parameters",
                   "[btree][template]", BinarySearchMode, LinearSearchMode,
                   SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Small leaf nodes") {
    btree<int, int, 8, 64, Mode> tree;
    REQUIRE(tree.empty());
  }

  SECTION("Small internal nodes") {
    btree<int, int, 64, 8, Mode> tree;
    REQUIRE(tree.empty());
  }

  SECTION("Both small") {
    btree<int, int, 4, 4, Mode> tree;
    REQUIRE(tree.empty());
  }

  SECTION("Large value type") {
    struct LargeValue {
      char data[1024];
    };
    btree<int, LargeValue, 16, 64, Mode> tree;
    REQUIRE(tree.empty());
  }
}

TEST_CASE("btree works with different MoveMode", "[btree][movemode]") {
  SECTION("Standard MoveMode") {
    btree<int, int, 64, 64, SearchMode::Binary, MoveMode::Standard> tree;
    REQUIRE(tree.empty());
  }

  SECTION("SIMD MoveMode") {
    btree<int, int, 64, 64, SearchMode::Binary, MoveMode::SIMD> tree;
    REQUIRE(tree.empty());
  }
}
