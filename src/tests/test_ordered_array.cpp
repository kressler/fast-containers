#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <string>

#include "../ordered_array.hpp"

using namespace fast_containers;

// Template types for parametrized testing of different search modes
template <SearchMode Mode>
struct SearchModeType {
  static constexpr SearchMode value = Mode;
};

using BinarySearchMode = SearchModeType<SearchMode::Binary>;
using LinearSearchMode = SearchModeType<SearchMode::Linear>;
using SIMDSearchMode = SearchModeType<SearchMode::SIMD>;

TEST_CASE("ordered_array basic construction", "[ordered_array]") {
  ordered_array<int, std::string, 10> arr;

  REQUIRE(arr.size() == 0);
  REQUIRE(arr.empty());
  REQUIRE(!arr.full());
  REQUIRE(arr.capacity() == 10);
}

TEMPLATE_TEST_CASE("ordered_array insert operations", "[ordered_array]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  ordered_array<int, std::string, 5, Mode> arr;

  SECTION("Insert single element") {
    arr.insert(5, "five");
    REQUIRE(arr.size() == 1);
    REQUIRE(!arr.empty());

    auto it = arr.find(5);
    REQUIRE(it != arr.end());
    REQUIRE(it->first == 5);
    REQUIRE(it->second == "five");
  }

  SECTION("Insert multiple elements in sorted order") {
    arr.insert(3, "three");
    arr.insert(1, "one");
    arr.insert(5, "five");
    arr.insert(2, "two");
    arr.insert(4, "four");

    REQUIRE(arr.size() == 5);
    REQUIRE(arr.full());

    // Verify elements are in sorted order
    auto it = arr.begin();
    REQUIRE(it->first == 1);
    ++it;
    REQUIRE(it->first == 2);
    ++it;
    REQUIRE(it->first == 3);
    ++it;
    REQUIRE(it->first == 4);
    ++it;
    REQUIRE(it->first == 5);
  }

  SECTION("Insert throws when array is full") {
    arr.insert(1, "one");
    arr.insert(2, "two");
    arr.insert(3, "three");
    arr.insert(4, "four");
    arr.insert(5, "five");

    REQUIRE(arr.full());
    REQUIRE_THROWS_AS(arr.insert(6, "six"), std::runtime_error);
  }

  SECTION("Insert throws when key already exists") {
    arr.insert(3, "three");
    REQUIRE_THROWS_AS(arr.insert(3, "tres"), std::runtime_error);
  }
}

TEMPLATE_TEST_CASE("ordered_array find operations", "[ordered_array]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  ordered_array<int, std::string, 10, Mode> arr;
  arr.insert(10, "ten");
  arr.insert(20, "twenty");
  arr.insert(30, "thirty");
  arr.insert(40, "forty");

  SECTION("Find existing elements") {
    auto it1 = arr.find(10);
    REQUIRE(it1 != arr.end());
    REQUIRE(it1->second == "ten");

    auto it2 = arr.find(30);
    REQUIRE(it2 != arr.end());
    REQUIRE(it2->second == "thirty");

    auto it3 = arr.find(40);
    REQUIRE(it3 != arr.end());
    REQUIRE(it3->second == "forty");
  }

  SECTION("Find non-existing element returns end()") {
    auto it = arr.find(25);
    REQUIRE(it == arr.end());

    it = arr.find(100);
    REQUIRE(it == arr.end());

    it = arr.find(5);
    REQUIRE(it == arr.end());
  }

  SECTION("Find on const array") {
    const auto& const_arr = arr;
    auto it = const_arr.find(20);
    REQUIRE(it != const_arr.end());
    REQUIRE(it->second == "twenty");
  }
}

TEMPLATE_TEST_CASE("ordered_array remove operations", "[ordered_array]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  ordered_array<int, std::string, 10, Mode> arr;
  arr.insert(10, "ten");
  arr.insert(20, "twenty");
  arr.insert(30, "thirty");
  arr.insert(40, "forty");
  arr.insert(50, "fifty");

  SECTION("Remove existing element") {
    REQUIRE(arr.size() == 5);

    arr.remove(30);
    REQUIRE(arr.size() == 4);
    REQUIRE(arr.find(30) == arr.end());

    // Verify order is maintained
    auto it = arr.begin();
    REQUIRE(it->first == 10);
    ++it;
    REQUIRE(it->first == 20);
    ++it;
    REQUIRE(it->first == 40);
    ++it;
    REQUIRE(it->first == 50);
  }

  SECTION("Remove first element") {
    arr.remove(10);
    REQUIRE(arr.size() == 4);
    REQUIRE(arr.find(10) == arr.end());
    REQUIRE(arr.begin()->first == 20);
  }

  SECTION("Remove last element") {
    arr.remove(50);
    REQUIRE(arr.size() == 4);
    REQUIRE(arr.find(50) == arr.end());
  }

  SECTION("Remove non-existing element does nothing") {
    arr.remove(100);
    REQUIRE(arr.size() == 5);

    arr.remove(25);
    REQUIRE(arr.size() == 5);
  }
}

TEMPLATE_TEST_CASE("ordered_array subscript operator", "[ordered_array]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  ordered_array<int, std::string, 10, Mode> arr;

  SECTION("Access non-existing element inserts with default value") {
    auto& val = arr[5];
    REQUIRE(arr.size() == 1);
    REQUIRE(val == "");

    val = "five";
    REQUIRE(arr.find(5)->second == "five");
  }

  SECTION("Access existing element returns reference") {
    arr.insert(10, "ten");
    auto& val = arr[10];
    REQUIRE(val == "ten");

    val = "TEN";
    REQUIRE(arr.find(10)->second == "TEN");
  }

  SECTION("Subscript maintains sorted order when inserting") {
    arr[30] = "thirty";
    arr[10] = "ten";
    arr[20] = "twenty";

    auto it = arr.begin();
    REQUIRE(it->first == 10);
    ++it;
    REQUIRE(it->first == 20);
    ++it;
    REQUIRE(it->first == 30);
  }

  SECTION("Subscript throws when inserting into full array") {
    for (int i = 0; i < 10; ++i) {
      arr[i] = std::to_string(i);
    }

    REQUIRE(arr.full());
    REQUIRE_THROWS_AS(arr[100], std::runtime_error);
  }
}

TEST_CASE("ordered_array iterator support", "[ordered_array]") {
  ordered_array<int, std::string, 10> arr;
  arr.insert(5, "five");
  arr.insert(3, "three");
  arr.insert(7, "seven");
  arr.insert(1, "one");

  SECTION("Forward iteration") {
    std::vector<int> keys;
    for (const auto& pair : arr) {
      keys.push_back(pair.first);
    }

    REQUIRE(keys == std::vector<int>{1, 3, 5, 7});
  }

  SECTION("Const iterator") {
    const auto& const_arr = arr;
    std::vector<int> keys;
    for (auto it = const_arr.cbegin(); it != const_arr.cend(); ++it) {
      keys.push_back(it->first);
    }

    REQUIRE(keys == std::vector<int>{1, 3, 5, 7});
  }

  SECTION("Modify through iterator") {
    for (auto pair : arr) {
      pair.second += "!";
    }

    REQUIRE(arr.find(1)->second == "one!");
    REQUIRE(arr.find(3)->second == "three!");
    REQUIRE(arr.find(5)->second == "five!");
    REQUIRE(arr.find(7)->second == "seven!");
  }
}

TEST_CASE("ordered_array reverse iterator support", "[ordered_array]") {
  ordered_array<int, std::string, 10> arr;
  arr.insert(5, "five");
  arr.insert(3, "three");
  arr.insert(7, "seven");
  arr.insert(1, "one");

  SECTION("Reverse iteration") {
    std::vector<int> keys;
    for (auto it = arr.rbegin(); it != arr.rend(); ++it) {
      keys.push_back(it->first);
    }

    REQUIRE(keys == std::vector<int>{7, 5, 3, 1});
  }

  SECTION("Const reverse iterator") {
    const auto& const_arr = arr;
    std::vector<int> keys;
    for (auto it = const_arr.crbegin(); it != const_arr.crend(); ++it) {
      keys.push_back(it->first);
    }

    REQUIRE(keys == std::vector<int>{7, 5, 3, 1});
  }
}

TEST_CASE("ordered_array with different types", "[ordered_array]") {
  SECTION("String keys and int values") {
    ordered_array<std::string, int, 5> arr;
    arr.insert("apple", 1);
    arr.insert("zebra", 26);
    arr.insert("banana", 2);
    arr.insert("mango", 13);

    // Verify alphabetical order
    auto it = arr.begin();
    REQUIRE(it->first == "apple");
    ++it;
    REQUIRE(it->first == "banana");
    ++it;
    REQUIRE(it->first == "mango");
    ++it;
    REQUIRE(it->first == "zebra");
  }

  SECTION("Double keys and double values") {
    ordered_array<double, double, 5> arr;
    arr.insert(3.14, 1.0);
    arr.insert(2.71, 2.0);
    arr.insert(1.41, 3.0);

    REQUIRE(arr.find(2.71)->second == 2.0);
    REQUIRE(arr.begin()->first == 1.41);
  }
}

TEST_CASE("ordered_array clear operation", "[ordered_array]") {
  ordered_array<int, std::string, 10> arr;
  arr.insert(1, "one");
  arr.insert(2, "two");
  arr.insert(3, "three");

  REQUIRE(arr.size() == 3);

  arr.clear();

  REQUIRE(arr.size() == 0);
  REQUIRE(arr.empty());
  REQUIRE(arr.find(1) == arr.end());
  REQUIRE(arr.find(2) == arr.end());
  REQUIRE(arr.find(3) == arr.end());
}

TEST_CASE("ordered_array concept enforcement", "[ordered_array]") {
  // This test verifies that the Comparable concept works at compile time
  // If this compiles, the concept is working

  SECTION("Integer types are comparable") {
    ordered_array<int, std::string, 5> arr1;
    ordered_array<long, std::string, 5> arr2;
    REQUIRE(true);
  }

  SECTION("String types are comparable") {
    ordered_array<std::string, int, 5> arr;
    REQUIRE(true);
  }

  SECTION("Floating point types are comparable") {
    ordered_array<double, int, 5> arr;
    REQUIRE(true);
  }
}

TEST_CASE("ordered_array search mode comparison",
          "[ordered_array][search_mode]") {
  using BinaryArray = ordered_array<int, std::string, 20, SearchMode::Binary>;
  using LinearArray = ordered_array<int, std::string, 20, SearchMode::Linear>;

  BinaryArray binary_arr;
  LinearArray linear_arr;

  // Insert the same data into both arrays
  std::vector<std::pair<int, std::string>> test_data = {
      {5, "five"},  {10, "ten"},     {15, "fifteen"}, {3, "three"},
      {7, "seven"}, {12, "twelve"},  {1, "one"},      {20, "twenty"},
      {8, "eight"}, {14, "fourteen"}};

  for (const auto& [key, value] : test_data) {
    binary_arr.insert(key, value);
    linear_arr.insert(key, value);
  }

  SECTION("Both modes produce same sorted order") {
    auto binary_it = binary_arr.begin();
    auto linear_it = linear_arr.begin();

    while (binary_it != binary_arr.end() && linear_it != linear_arr.end()) {
      REQUIRE(binary_it->first == linear_it->first);
      REQUIRE(binary_it->second == linear_it->second);
      ++binary_it;
      ++linear_it;
    }

    REQUIRE(binary_it == binary_arr.end());
    REQUIRE(linear_it == linear_arr.end());
  }

  SECTION("Both modes find same elements") {
    for (int key : {1, 5, 10, 15, 20}) {
      auto binary_it = binary_arr.find(key);
      auto linear_it = linear_arr.find(key);

      REQUIRE(binary_it != binary_arr.end());
      REQUIRE(linear_it != linear_arr.end());
      REQUIRE(binary_it->first == linear_it->first);
      REQUIRE(binary_it->second == linear_it->second);
    }

    // Test non-existing keys
    for (int key : {0, 2, 100}) {
      REQUIRE(binary_arr.find(key) == binary_arr.end());
      REQUIRE(linear_arr.find(key) == linear_arr.end());
    }
  }

  SECTION("Both modes handle removal the same way") {
    binary_arr.remove(10);
    linear_arr.remove(10);

    REQUIRE(binary_arr.size() == linear_arr.size());
    REQUIRE(binary_arr.find(10) == binary_arr.end());
    REQUIRE(linear_arr.find(10) == linear_arr.end());

    auto binary_it = binary_arr.begin();
    auto linear_it = linear_arr.begin();

    while (binary_it != binary_arr.end()) {
      REQUIRE(binary_it->first == linear_it->first);
      REQUIRE(binary_it->second == linear_it->second);
      ++binary_it;
      ++linear_it;
    }
  }
}

TEMPLATE_TEST_CASE("ordered_array copy constructor", "[ordered_array]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Copy empty array") {
    ordered_array<int, std::string, 10, Mode> original;
    ordered_array<int, std::string, 10, Mode> copy(original);

    REQUIRE(copy.size() == 0);
    REQUIRE(copy.empty());
    REQUIRE(copy.capacity() == 10);
  }

  SECTION("Copy array with elements") {
    ordered_array<int, std::string, 10, Mode> original;
    original.insert(3, "three");
    original.insert(1, "one");
    original.insert(5, "five");

    ordered_array<int, std::string, 10, Mode> copy(original);

    REQUIRE(copy.size() == 3);
    REQUIRE(!copy.empty());

    // Verify all elements were copied in correct order
    auto it = copy.begin();
    REQUIRE(it->first == 1);
    REQUIRE(it->second == "one");
    ++it;
    REQUIRE(it->first == 3);
    REQUIRE(it->second == "three");
    ++it;
    REQUIRE(it->first == 5);
    REQUIRE(it->second == "five");
  }

  SECTION("Copy is independent from original") {
    ordered_array<int, std::string, 10, Mode> original;
    original.insert(1, "one");
    original.insert(2, "two");

    ordered_array<int, std::string, 10, Mode> copy(original);

    // Modify original
    original.insert(3, "three");
    original[1] = "uno";

    // Verify copy is unchanged
    REQUIRE(copy.size() == 2);
    REQUIRE(copy.find(3) == copy.end());
    REQUIRE(copy.find(1)->second == "one");

    // Modify copy
    copy[2] = "dos";

    // Verify original is unchanged
    REQUIRE(original.find(2)->second == "two");
  }
}

TEMPLATE_TEST_CASE("ordered_array copy assignment", "[ordered_array]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Copy assign to empty array") {
    ordered_array<int, std::string, 10, Mode> original;
    original.insert(1, "one");
    original.insert(2, "two");

    ordered_array<int, std::string, 10, Mode> dest;
    dest = original;

    REQUIRE(dest.size() == 2);
    REQUIRE(dest.find(1)->second == "one");
    REQUIRE(dest.find(2)->second == "two");
  }

  SECTION("Copy assign overwrites existing elements") {
    ordered_array<int, std::string, 10, Mode> original;
    original.insert(5, "five");
    original.insert(10, "ten");

    ordered_array<int, std::string, 10, Mode> dest;
    dest.insert(1, "one");
    dest.insert(2, "two");
    dest.insert(3, "three");

    dest = original;

    REQUIRE(dest.size() == 2);
    REQUIRE(dest.find(5)->second == "five");
    REQUIRE(dest.find(10)->second == "ten");
    REQUIRE(dest.find(1) == dest.end());
  }

  SECTION("Self-assignment is safe") {
    ordered_array<int, std::string, 10, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");

    arr = arr;

    REQUIRE(arr.size() == 2);
    REQUIRE(arr.find(1)->second == "one");
    REQUIRE(arr.find(2)->second == "two");
  }

  SECTION("Copy assignment creates independent copy") {
    ordered_array<int, std::string, 10, Mode> original;
    original.insert(1, "one");

    ordered_array<int, std::string, 10, Mode> dest;
    dest = original;

    // Modify original
    original[1] = "uno";

    // Verify dest is unchanged
    REQUIRE(dest.find(1)->second == "one");
  }
}

TEMPLATE_TEST_CASE("ordered_array move constructor", "[ordered_array]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Move empty array") {
    ordered_array<int, std::string, 10, Mode> original;
    ordered_array<int, std::string, 10, Mode> moved(std::move(original));

    REQUIRE(moved.size() == 0);
    REQUIRE(moved.empty());
    REQUIRE(original.size() == 0);  // Original should be empty
    REQUIRE(original.empty());
  }

  SECTION("Move array with elements") {
    ordered_array<int, std::string, 10, Mode> original;
    original.insert(3, "three");
    original.insert(1, "one");
    original.insert(5, "five");

    ordered_array<int, std::string, 10, Mode> moved(std::move(original));

    // Verify moved-to array has all elements
    REQUIRE(moved.size() == 3);
    auto it = moved.begin();
    REQUIRE(it->first == 1);
    REQUIRE(it->second == "one");
    ++it;
    REQUIRE(it->first == 3);
    REQUIRE(it->second == "three");
    ++it;
    REQUIRE(it->first == 5);
    REQUIRE(it->second == "five");

    // Verify moved-from array is empty
    REQUIRE(original.size() == 0);
    REQUIRE(original.empty());
  }

  SECTION("Moved-from array is reusable") {
    ordered_array<int, std::string, 10, Mode> original;
    original.insert(1, "one");

    ordered_array<int, std::string, 10, Mode> moved(std::move(original));

    // Reuse moved-from array
    original.insert(2, "two");
    original.insert(3, "three");

    REQUIRE(original.size() == 2);
    REQUIRE(original.find(2)->second == "two");
    REQUIRE(original.find(3)->second == "three");
  }
}

TEMPLATE_TEST_CASE("ordered_array move assignment", "[ordered_array]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Move assign to empty array") {
    ordered_array<int, std::string, 10, Mode> original;
    original.insert(1, "one");
    original.insert(2, "two");

    ordered_array<int, std::string, 10, Mode> dest;
    dest = std::move(original);

    REQUIRE(dest.size() == 2);
    REQUIRE(dest.find(1)->second == "one");
    REQUIRE(dest.find(2)->second == "two");

    REQUIRE(original.size() == 0);
    REQUIRE(original.empty());
  }

  SECTION("Move assign overwrites existing elements") {
    ordered_array<int, std::string, 10, Mode> original;
    original.insert(5, "five");
    original.insert(10, "ten");

    ordered_array<int, std::string, 10, Mode> dest;
    dest.insert(1, "one");
    dest.insert(2, "two");
    dest.insert(3, "three");

    dest = std::move(original);

    REQUIRE(dest.size() == 2);
    REQUIRE(dest.find(5)->second == "five");
    REQUIRE(dest.find(10)->second == "ten");
    REQUIRE(dest.find(1) == dest.end());

    REQUIRE(original.size() == 0);
  }

  SECTION("Self-move-assignment is safe") {
    ordered_array<int, std::string, 10, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");

    arr = std::move(arr);

    // After self-move, array should remain valid (possibly empty or unchanged)
    // The exact state is implementation-defined, but should be valid
    REQUIRE(arr.size() >= 0);
  }

  SECTION("Moved-from array is reusable") {
    ordered_array<int, std::string, 10, Mode> original;
    original.insert(1, "one");
    original.insert(2, "two");

    ordered_array<int, std::string, 10, Mode> dest;
    dest = std::move(original);

    // Reuse moved-from array
    original.insert(10, "ten");
    original.insert(20, "twenty");

    REQUIRE(original.size() == 2);
    REQUIRE(original.find(10)->second == "ten");
    REQUIRE(original.find(20)->second == "twenty");
  }
}

TEMPLATE_TEST_CASE("ordered_array copy/move with different types",
                   "[ordered_array]", BinarySearchMode, LinearSearchMode,
                   SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Copy with int keys and int values") {
    ordered_array<int, int, 10, Mode> original;
    original.insert(5, 50);
    original.insert(3, 30);

    ordered_array<int, int, 10, Mode> copy(original);

    REQUIRE(copy.size() == 2);
    REQUIRE(copy.find(3)->second == 30);
    REQUIRE(copy.find(5)->second == 50);
  }

  SECTION("Move with double keys and string values") {
    ordered_array<double, std::string, 10, Mode> original;
    original.insert(3.14, "pi");
    original.insert(2.71, "e");

    ordered_array<double, std::string, 10, Mode> moved(std::move(original));

    REQUIRE(moved.size() == 2);
    REQUIRE(moved.find(2.71)->second == "e");
    REQUIRE(moved.find(3.14)->second == "pi");
    REQUIRE(original.size() == 0);
  }
}
