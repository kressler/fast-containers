// Copyright (c) 2025 Bryan Kressler
//
// SPDX-License-Identifier: BSD-3-Clause

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fast_containers/dense_map.hpp>
#include <string>

using namespace kressler::fast_containers;

// Template types for parametrized testing of different search modes
template <SearchMode Mode>
struct SearchModeType {
  static constexpr SearchMode value = Mode;
};

using BinarySearchMode = SearchModeType<SearchMode::Binary>;
using LinearSearchMode = SearchModeType<SearchMode::Linear>;
using SIMDSearchMode = SearchModeType<SearchMode::SIMD>;

TEST_CASE("dense_map basic construction", "[dense_map]") {
  dense_map<int, std::string, 10> arr;

  REQUIRE(arr.size() == 0);
  REQUIRE(arr.empty());
  REQUIRE(!arr.full());
  REQUIRE(arr.capacity() == 10);
}

TEMPLATE_TEST_CASE("dense_map insert operations", "[dense_map]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  dense_map<int, std::string, 5, std::less<int>, Mode> arr;

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

  SECTION("Insert returns false when key already exists") {
    auto [it1, inserted1] = arr.insert(3, "three");
    REQUIRE(inserted1 == true);
    REQUIRE(it1->first == 3);
    REQUIRE(it1->second == "three");

    auto [it2, inserted2] = arr.insert(3, "tres");
    REQUIRE(inserted2 == false);
    REQUIRE(it2->first == 3);
    REQUIRE(it2->second == "three");  // Value should be unchanged
    REQUIRE(it1 == it2);              // Both iterators point to same element
  }
}

TEMPLATE_TEST_CASE("dense_map find operations", "[dense_map]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  dense_map<int, std::string, 10, std::less<int>, Mode> arr;
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

TEMPLATE_TEST_CASE("dense_map remove operations", "[dense_map]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  dense_map<int, std::string, 10, std::less<int>, Mode> arr;
  arr.insert(10, "ten");
  arr.insert(20, "twenty");
  arr.insert(30, "thirty");
  arr.insert(40, "forty");
  arr.insert(50, "fifty");

  SECTION("Remove existing element") {
    REQUIRE(arr.size() == 5);

    arr.erase(30);
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
    arr.erase(10);
    REQUIRE(arr.size() == 4);
    REQUIRE(arr.find(10) == arr.end());
    REQUIRE(arr.begin()->first == 20);
  }

  SECTION("Remove last element") {
    arr.erase(50);
    REQUIRE(arr.size() == 4);
    REQUIRE(arr.find(50) == arr.end());
  }

  SECTION("Remove non-existing element does nothing") {
    arr.erase(100);
    REQUIRE(arr.size() == 5);

    arr.erase(25);
    REQUIRE(arr.size() == 5);
  }

  SECTION("Remove returns number of elements removed") {
    // Removing existing element returns 1
    auto removed = arr.erase(30);
    REQUIRE(removed == 1);
    REQUIRE(arr.size() == 4);

    // Removing non-existing element returns 0
    removed = arr.erase(100);
    REQUIRE(removed == 0);
    REQUIRE(arr.size() == 4);

    // Removing already removed element returns 0
    removed = arr.erase(30);
    REQUIRE(removed == 0);
    REQUIRE(arr.size() == 4);
  }
}

TEMPLATE_TEST_CASE("dense_map subscript operator", "[dense_map]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  dense_map<int, std::string, 10, std::less<int>, Mode> arr;

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

TEST_CASE("dense_map iterator support", "[dense_map]") {
  dense_map<int, std::string, 10> arr;
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

TEST_CASE("dense_map reverse iterator support", "[dense_map]") {
  dense_map<int, std::string, 10> arr;
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

TEST_CASE("dense_map with different types", "[dense_map]") {
  SECTION("String keys and int values") {
    dense_map<std::string, int, 5> arr;
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
    dense_map<double, double, 5> arr;
    arr.insert(3.14, 1.0);
    arr.insert(2.71, 2.0);
    arr.insert(1.41, 3.0);

    REQUIRE(arr.find(2.71)->second == 2.0);
    REQUIRE(arr.begin()->first == 1.41);
  }
}

TEST_CASE("dense_map clear operation", "[dense_map]") {
  dense_map<int, std::string, 10> arr;
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

TEST_CASE("dense_map concept enforcement", "[dense_map]") {
  // This test verifies that the Comparable concept works at compile time
  // If this compiles, the concept is working

  SECTION("Integer types are comparable") {
    dense_map<int, std::string, 5> arr1;
    dense_map<long, std::string, 5> arr2;
    REQUIRE(true);
  }

  SECTION("String types are comparable") {
    dense_map<std::string, int, 5> arr;
    REQUIRE(true);
  }

  SECTION("Floating point types are comparable") {
    dense_map<double, int, 5> arr;
    REQUIRE(true);
  }
}

TEST_CASE("dense_map search mode comparison",
          "[dense_map][search_mode]") {
  using BinaryArray =
      dense_map<int, std::string, 20, std::less<int>, SearchMode::Binary>;
  using LinearArray =
      dense_map<int, std::string, 20, std::less<int>, SearchMode::Linear>;

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
    binary_arr.erase(10);
    linear_arr.erase(10);

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

TEMPLATE_TEST_CASE("dense_map copy constructor", "[dense_map]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Copy empty array") {
    dense_map<int, std::string, 10, std::less<int>, Mode> original;
    dense_map<int, std::string, 10, std::less<int>, Mode> copy(original);

    REQUIRE(copy.size() == 0);
    REQUIRE(copy.empty());
    REQUIRE(copy.capacity() == 10);
  }

  SECTION("Copy array with elements") {
    dense_map<int, std::string, 10, std::less<int>, Mode> original;
    original.insert(3, "three");
    original.insert(1, "one");
    original.insert(5, "five");

    dense_map<int, std::string, 10, std::less<int>, Mode> copy(original);

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
    dense_map<int, std::string, 10, std::less<int>, Mode> original;
    original.insert(1, "one");
    original.insert(2, "two");

    dense_map<int, std::string, 10, std::less<int>, Mode> copy(original);

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

TEMPLATE_TEST_CASE("dense_map copy assignment", "[dense_map]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Copy assign to empty array") {
    dense_map<int, std::string, 10, std::less<int>, Mode> original;
    original.insert(1, "one");
    original.insert(2, "two");

    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dest = original;

    REQUIRE(dest.size() == 2);
    REQUIRE(dest.find(1)->second == "one");
    REQUIRE(dest.find(2)->second == "two");
  }

  SECTION("Copy assign overwrites existing elements") {
    dense_map<int, std::string, 10, std::less<int>, Mode> original;
    original.insert(5, "five");
    original.insert(10, "ten");

    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
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
    dense_map<int, std::string, 10, std::less<int>, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");

    arr = arr;

    REQUIRE(arr.size() == 2);
    REQUIRE(arr.find(1)->second == "one");
    REQUIRE(arr.find(2)->second == "two");
  }

  SECTION("Copy assignment creates independent copy") {
    dense_map<int, std::string, 10, std::less<int>, Mode> original;
    original.insert(1, "one");

    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dest = original;

    // Modify original
    original[1] = "uno";

    // Verify dest is unchanged
    REQUIRE(dest.find(1)->second == "one");
  }
}

TEMPLATE_TEST_CASE("dense_map move constructor", "[dense_map]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Move empty array") {
    dense_map<int, std::string, 10, std::less<int>, Mode> original;
    dense_map<int, std::string, 10, std::less<int>, Mode> moved(
        std::move(original));

    REQUIRE(moved.size() == 0);
    REQUIRE(moved.empty());
    REQUIRE(original.size() == 0);  // Original should be empty
    REQUIRE(original.empty());
  }

  SECTION("Move array with elements") {
    dense_map<int, std::string, 10, std::less<int>, Mode> original;
    original.insert(3, "three");
    original.insert(1, "one");
    original.insert(5, "five");

    dense_map<int, std::string, 10, std::less<int>, Mode> moved(
        std::move(original));

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
    dense_map<int, std::string, 10, std::less<int>, Mode> original;
    original.insert(1, "one");

    dense_map<int, std::string, 10, std::less<int>, Mode> moved(
        std::move(original));

    // Reuse moved-from array
    original.insert(2, "two");
    original.insert(3, "three");

    REQUIRE(original.size() == 2);
    REQUIRE(original.find(2)->second == "two");
    REQUIRE(original.find(3)->second == "three");
  }
}

TEMPLATE_TEST_CASE("dense_map move assignment", "[dense_map]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Move assign to empty array") {
    dense_map<int, std::string, 10, std::less<int>, Mode> original;
    original.insert(1, "one");
    original.insert(2, "two");

    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dest = std::move(original);

    REQUIRE(dest.size() == 2);
    REQUIRE(dest.find(1)->second == "one");
    REQUIRE(dest.find(2)->second == "two");

    REQUIRE(original.size() == 0);
    REQUIRE(original.empty());
  }

  SECTION("Move assign overwrites existing elements") {
    dense_map<int, std::string, 10, std::less<int>, Mode> original;
    original.insert(5, "five");
    original.insert(10, "ten");

    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
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
    dense_map<int, std::string, 10, std::less<int>, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");

    arr = std::move(arr);

    // After self-move, array should remain valid (possibly empty or unchanged)
    // The exact state is implementation-defined, but should be valid
    REQUIRE(arr.size() >= 0);
  }

  SECTION("Moved-from array is reusable") {
    dense_map<int, std::string, 10, std::less<int>, Mode> original;
    original.insert(1, "one");
    original.insert(2, "two");

    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dest = std::move(original);

    // Reuse moved-from array
    original.insert(10, "ten");
    original.insert(20, "twenty");

    REQUIRE(original.size() == 2);
    REQUIRE(original.find(10)->second == "ten");
    REQUIRE(original.find(20)->second == "twenty");
  }
}

TEMPLATE_TEST_CASE("dense_map copy/move with different types",
                   "[dense_map]", BinarySearchMode, LinearSearchMode,
                   SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Copy with int keys and int values") {
    dense_map<int, int, 10, std::less<int>, Mode> original;
    original.insert(5, 50);
    original.insert(3, 30);

    dense_map<int, int, 10, std::less<int>, Mode> copy(original);

    REQUIRE(copy.size() == 2);
    REQUIRE(copy.find(3)->second == 30);
    REQUIRE(copy.find(5)->second == 50);
  }

  SECTION("Move with double keys and string values") {
    dense_map<double, std::string, 10, std::less<double>, Mode> original;
    original.insert(3.14, "pi");
    original.insert(2.71, "e");

    dense_map<double, std::string, 10, std::less<double>, Mode> moved(
        std::move(original));

    REQUIRE(moved.size() == 2);
    REQUIRE(moved.find(2.71)->second == "e");
    REQUIRE(moved.find(3.14)->second == "pi");
    REQUIRE(original.size() == 0);
  }
}

TEMPLATE_TEST_CASE("dense_map split_at operation", "[dense_map]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Split empty array") {
    dense_map<int, std::string, 10, std::less<int>, Mode> arr;
    dense_map<int, std::string, 10, std::less<int>, Mode> output;

    arr.split_at(arr.begin(), output);

    REQUIRE(arr.size() == 0);
    REQUIRE(output.size() == 0);
  }

  SECTION("Split at beginning (move all elements)") {
    dense_map<int, std::string, 10, std::less<int>, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");
    arr.insert(3, "three");

    dense_map<int, std::string, 10, std::less<int>, Mode> output;
    arr.split_at(arr.begin(), output);

    REQUIRE(arr.size() == 0);
    REQUIRE(arr.empty());

    REQUIRE(output.size() == 3);
    auto it = output.begin();
    REQUIRE(it->first == 1);
    REQUIRE(it->second == "one");
    ++it;
    REQUIRE(it->first == 2);
    ++it;
    REQUIRE(it->first == 3);
  }

  SECTION("Split at end (move no elements)") {
    dense_map<int, std::string, 10, std::less<int>, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");
    arr.insert(3, "three");

    dense_map<int, std::string, 10, std::less<int>, Mode> output;
    arr.split_at(arr.end(), output);

    REQUIRE(arr.size() == 3);
    REQUIRE(output.size() == 0);
    REQUIRE(output.empty());
  }

  SECTION("Split in middle") {
    dense_map<int, std::string, 10, std::less<int>, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");
    arr.insert(3, "three");
    arr.insert(4, "four");
    arr.insert(5, "five");

    dense_map<int, std::string, 10, std::less<int>, Mode> output;
    auto it = arr.begin();
    ++it;
    ++it;  // Points to element with key 3
    arr.split_at(it, output);

    // Original should have [1, 2]
    REQUIRE(arr.size() == 2);
    auto arr_it = arr.begin();
    REQUIRE(arr_it->first == 1);
    ++arr_it;
    REQUIRE(arr_it->first == 2);

    // Output should have [3, 4, 5]
    REQUIRE(output.size() == 3);
    auto out_it = output.begin();
    REQUIRE(out_it->first == 3);
    REQUIRE(out_it->second == "three");
    ++out_it;
    REQUIRE(out_it->first == 4);
    ++out_it;
    REQUIRE(out_it->first == 5);
  }

  SECTION("Split throws if output is not empty") {
    dense_map<int, std::string, 10, std::less<int>, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");

    dense_map<int, std::string, 10, std::less<int>, Mode> output;
    output.insert(10, "ten");

    REQUIRE_THROWS_AS(arr.split_at(arr.begin(), output), std::runtime_error);
  }

  SECTION("Split throws if output has insufficient capacity") {
    dense_map<int, std::string, 10, std::less<int>, Mode> arr;
    for (int i = 0; i < 10; ++i) {
      arr.insert(i, std::to_string(i));
    }

    dense_map<int, std::string, 5, std::less<int>, Mode> small_output;

    // Trying to move 10 elements to capacity-5 array should throw
    REQUIRE_THROWS_AS(arr.split_at(arr.begin(), small_output),
                      std::runtime_error);
  }

  SECTION("Split maintains element order and values") {
    dense_map<int, std::string, 20, std::less<int>, Mode> arr;
    for (int i = 1; i <= 10; ++i) {
      arr.insert(i, std::to_string(i * 10));
    }

    dense_map<int, std::string, 20, std::less<int>, Mode> output;
    auto split_pos = arr.begin();
    std::advance(split_pos, 6);  // Split at key 7

    arr.split_at(split_pos, output);

    // Verify first array [1..6]
    REQUIRE(arr.size() == 6);
    for (int i = 1; i <= 6; ++i) {
      auto it = arr.find(i);
      REQUIRE(it != arr.end());
      REQUIRE(it->second == std::to_string(i * 10));
    }

    // Verify second array [7..10]
    REQUIRE(output.size() == 4);
    for (int i = 7; i <= 10; ++i) {
      auto it = output.find(i);
      REQUIRE(it != output.end());
      REQUIRE(it->second == std::to_string(i * 10));
    }
  }
}

TEMPLATE_TEST_CASE("dense_map transfer_prefix_from operation",
                   "[dense_map]", BinarySearchMode, LinearSearchMode,
                   SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Transfer prefix to empty array") {
    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dense_map<int, std::string, 10, std::less<int>, Mode> source;
    source.insert(1, "one");
    source.insert(2, "two");
    source.insert(3, "three");

    dest.transfer_prefix_from(source, 2);

    REQUIRE(dest.size() == 2);
    REQUIRE(dest.find(1)->second == "one");
    REQUIRE(dest.find(2)->second == "two");

    REQUIRE(source.size() == 1);
    REQUIRE(source.find(3)->second == "three");
  }

  SECTION("Transfer prefix to non-empty array") {
    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dest.insert(1, "one");
    dest.insert(2, "two");

    dense_map<int, std::string, 10, std::less<int>, Mode> source;
    source.insert(5, "five");
    source.insert(6, "six");
    source.insert(7, "seven");

    dest.transfer_prefix_from(source, 2);

    // Dest should have [1, 2, 5, 6] (appended [5,6] to end)
    REQUIRE(dest.size() == 4);
    auto it = dest.begin();
    REQUIRE(it->first == 1);
    REQUIRE(it->second == "one");
    ++it;
    REQUIRE(it->first == 2);
    ++it;
    REQUIRE(it->first == 5);
    REQUIRE(it->second == "five");
    ++it;
    REQUIRE(it->first == 6);
    REQUIRE(it->second == "six");

    // Source should have [7]
    REQUIRE(source.size() == 1);
    REQUIRE(source.find(7)->second == "seven");
  }

  SECTION("Transfer zero elements") {
    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dest.insert(5, "five");

    dense_map<int, std::string, 10, std::less<int>, Mode> source;
    source.insert(1, "one");

    dest.transfer_prefix_from(source, 0);

    REQUIRE(dest.size() == 1);
    REQUIRE(source.size() == 1);
  }

  SECTION("Transfer all elements from source") {
    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dest.insert(1, "one");

    dense_map<int, std::string, 10, std::less<int>, Mode> source;
    source.insert(10, "ten");
    source.insert(11, "eleven");

    dest.transfer_prefix_from(source, 2);

    REQUIRE(dest.size() == 3);
    REQUIRE(source.size() == 0);
    REQUIRE(source.empty());
  }

  SECTION("Transfer throws if count exceeds source size") {
    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dense_map<int, std::string, 10, std::less<int>, Mode> source;
    source.insert(1, "one");

    REQUIRE_THROWS_AS(dest.transfer_prefix_from(source, 2), std::runtime_error);
  }

  SECTION("Transfer throws if destination has insufficient capacity") {
    dense_map<int, std::string, 5, std::less<int>, Mode> dest;
    for (int i = 0; i < 4; ++i) {
      dest.insert(i + 10, std::to_string(i));
    }

    dense_map<int, std::string, 5, std::less<int>, Mode> source;
    source.insert(1, "one");
    source.insert(2, "two");

    // Dest has 4 elements, capacity 5, trying to add 2 more (total 6)
    REQUIRE_THROWS_AS(dest.transfer_prefix_from(source, 2), std::runtime_error);
  }

  SECTION("Transfer maintains element order and values") {
    dense_map<int, std::string, 20, std::less<int>, Mode> dest;
    for (int i = 1; i <= 6; ++i) {
      dest.insert(i, std::to_string(i * 10));
    }

    dense_map<int, std::string, 20, std::less<int>, Mode> source;
    for (int i = 10; i <= 14; ++i) {
      source.insert(i, std::to_string(i * 100));
    }

    dest.transfer_prefix_from(source, 3);

    // Verify dest has [1, 2, 3, 4, 5, 6, 10, 11, 12] (appended [10,11,12])
    REQUIRE(dest.size() == 9);
    REQUIRE(dest.find(1)->second == "10");
    REQUIRE(dest.find(6)->second == "60");
    REQUIRE(dest.find(10)->second == "1000");
    REQUIRE(dest.find(11)->second == "1100");
    REQUIRE(dest.find(12)->second == "1200");

    // Verify source has [13, 14]
    REQUIRE(source.size() == 2);
    REQUIRE(source.find(13)->second == "1300");
    REQUIRE(source.find(14)->second == "1400");
  }
}

TEMPLATE_TEST_CASE("dense_map transfer_suffix_from operation",
                   "[dense_map]", BinarySearchMode, LinearSearchMode,
                   SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Transfer suffix to empty array") {
    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dense_map<int, std::string, 10, std::less<int>, Mode> source;
    source.insert(1, "one");
    source.insert(2, "two");
    source.insert(3, "three");

    dest.transfer_suffix_from(source, 2);

    REQUIRE(dest.size() == 2);
    REQUIRE(dest.find(2)->second == "two");
    REQUIRE(dest.find(3)->second == "three");

    REQUIRE(source.size() == 1);
    REQUIRE(source.find(1)->second == "one");
  }

  SECTION("Transfer suffix to non-empty array") {
    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dest.insert(5, "five");
    dest.insert(6, "six");

    dense_map<int, std::string, 10, std::less<int>, Mode> source;
    source.insert(1, "one");
    source.insert(2, "two");
    source.insert(3, "three");

    dest.transfer_suffix_from(source, 2);

    // Dest should have [2, 3, 5, 6] (prepended [2,3] to beginning)
    REQUIRE(dest.size() == 4);
    auto it = dest.begin();
    REQUIRE(it->first == 2);
    REQUIRE(it->second == "two");
    ++it;
    REQUIRE(it->first == 3);
    REQUIRE(it->second == "three");
    ++it;
    REQUIRE(it->first == 5);
    ++it;
    REQUIRE(it->first == 6);

    // Source should have [1]
    REQUIRE(source.size() == 1);
    REQUIRE(source.find(1)->second == "one");
  }

  SECTION("Transfer zero elements") {
    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dest.insert(1, "one");

    dense_map<int, std::string, 10, std::less<int>, Mode> source;
    source.insert(5, "five");

    dest.transfer_suffix_from(source, 0);

    REQUIRE(dest.size() == 1);
    REQUIRE(source.size() == 1);
  }

  SECTION("Transfer all elements from source") {
    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dest.insert(10, "ten");

    dense_map<int, std::string, 10, std::less<int>, Mode> source;
    source.insert(1, "one");
    source.insert(2, "two");

    dest.transfer_suffix_from(source, 2);

    REQUIRE(dest.size() == 3);
    REQUIRE(source.size() == 0);
    REQUIRE(source.empty());
  }

  SECTION("Transfer throws if count exceeds source size") {
    dense_map<int, std::string, 10, std::less<int>, Mode> dest;
    dense_map<int, std::string, 10, std::less<int>, Mode> source;
    source.insert(5, "five");

    REQUIRE_THROWS_AS(dest.transfer_suffix_from(source, 2), std::runtime_error);
  }

  SECTION("Transfer throws if destination has insufficient capacity") {
    dense_map<int, std::string, 5, std::less<int>, Mode> dest;
    for (int i = 0; i < 4; ++i) {
      dest.insert(i, std::to_string(i));
    }

    dense_map<int, std::string, 5, std::less<int>, Mode> source;
    source.insert(10, "ten");
    source.insert(11, "eleven");

    // Dest has 4 elements, capacity 5, trying to add 2 more (total 6)
    REQUIRE_THROWS_AS(dest.transfer_suffix_from(source, 2), std::runtime_error);
  }

  SECTION("Transfer maintains element order and values") {
    dense_map<int, std::string, 20, std::less<int>, Mode> dest;
    for (int i = 10; i <= 14; ++i) {
      dest.insert(i, std::to_string(i * 10));
    }

    dense_map<int, std::string, 20, std::less<int>, Mode> source;
    for (int i = 1; i <= 6; ++i) {
      source.insert(i, std::to_string(i * 100));
    }

    dest.transfer_suffix_from(source, 3);

    // Verify dest has [4, 5, 6, 10, 11, 12, 13, 14] (prepended [4,5,6])
    REQUIRE(dest.size() == 8);
    REQUIRE(dest.find(4)->second == "400");
    REQUIRE(dest.find(5)->second == "500");
    REQUIRE(dest.find(6)->second == "600");
    REQUIRE(dest.find(10)->second == "100");
    REQUIRE(dest.find(14)->second == "140");

    // Verify source has [1, 2, 3]
    REQUIRE(source.size() == 3);
    REQUIRE(source.find(1)->second == "100");
    REQUIRE(source.find(3)->second == "300");
  }
}

TEMPLATE_TEST_CASE("dense_map transfer operations combined",
                   "[dense_map]", BinarySearchMode, LinearSearchMode,
                   SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Rebalancing simulation: transfer from overfull to underfull node") {
    // Simulate B+ tree rebalancing
    dense_map<int, int, 10, std::less<int>, Mode> left, right;

    // Left node is underfull
    left.insert(1, 10);
    left.insert(2, 20);

    // Right node is overfull
    for (int i = 5; i <= 9; ++i) {
      right.insert(i, i * 10);
    }

    // Transfer prefix from right to left (rebalance)
    left.transfer_prefix_from(right, 2);

    REQUIRE(left.size() == 4);   // [1, 2, 5, 6]
    REQUIRE(right.size() == 3);  // [7, 8, 9]

    // Verify left has correct elements
    REQUIRE(left.find(1)->second == 10);
    REQUIRE(left.find(2)->second == 20);
    REQUIRE(left.find(5)->second == 50);
    REQUIRE(left.find(6)->second == 60);

    // Verify right has correct elements
    REQUIRE(right.find(7)->second == 70);
    REQUIRE(right.find(8)->second == 80);
    REQUIRE(right.find(9)->second == 90);
  }

  SECTION("Multiple transfers in sequence") {
    dense_map<int, std::string, 15, std::less<int>, Mode> node1, node2,
        node3;

    // Setup initial nodes
    node1.insert(1, "a");
    node1.insert(2, "b");

    node2.insert(5, "e");
    node2.insert(6, "f");
    node2.insert(7, "g");
    node2.insert(8, "h");

    node3.insert(10, "j");
    node3.insert(11, "k");

    // Transfer prefix from node2 to node1 (append to node1)
    node1.transfer_prefix_from(node2, 1);
    REQUIRE(node1.size() == 3);  // [1, 2, 5]
    REQUIRE(node2.size() == 3);  // [6, 7, 8]

    // Transfer suffix from node2 to node3 (prepend to node3)
    node3.transfer_suffix_from(node2, 2);
    REQUIRE(node3.size() == 4);  // [7, 8, 10, 11]
    REQUIRE(node2.size() == 1);  // [6]

    // Verify final states
    REQUIRE(node1.find(5)->second == "e");
    REQUIRE(node2.find(6)->second == "f");
    REQUIRE(node3.find(7)->second == "g");
    REQUIRE(node3.find(8)->second == "h");
  }
}

// Test search for 16-byte keys (byte arrays don't support SIMD mode)
TEMPLATE_TEST_CASE("dense_map search with 16-byte keys",
                   "[dense_map][simd]", BinarySearchMode) {
  constexpr SearchMode Mode = TestType::value;
  using Key16 = std::array<uint8_t, 16>;

  dense_map<Key16, int, 10, std::less<Key16>, Mode> arr;

  // Create test keys with distinct values
  Key16 key1 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  Key16 key2 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2};
  Key16 key3 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3};
  Key16 key4 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4};
  Key16 key5 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5};

  SECTION("Insert and find 16-byte keys") {
    arr.insert(key3, 300);
    arr.insert(key1, 100);
    arr.insert(key5, 500);
    arr.insert(key2, 200);
    arr.insert(key4, 400);

    REQUIRE(arr.size() == 5);

    // Verify all keys can be found
    auto it1 = arr.find(key1);
    REQUIRE(it1 != arr.end());
    REQUIRE(it1->second == 100);

    auto it2 = arr.find(key2);
    REQUIRE(it2 != arr.end());
    REQUIRE(it2->second == 200);

    auto it3 = arr.find(key3);
    REQUIRE(it3 != arr.end());
    REQUIRE(it3->second == 300);

    auto it4 = arr.find(key4);
    REQUIRE(it4 != arr.end());
    REQUIRE(it4->second == 400);

    auto it5 = arr.find(key5);
    REQUIRE(it5 != arr.end());
    REQUIRE(it5->second == 500);

    // Verify keys are in sorted order
    auto it = arr.begin();
    REQUIRE(it->first == key1);
    ++it;
    REQUIRE(it->first == key2);
    ++it;
    REQUIRE(it->first == key3);
    ++it;
    REQUIRE(it->first == key4);
    ++it;
    REQUIRE(it->first == key5);
  }

  SECTION("Find non-existent 16-byte key") {
    arr.insert(key1, 100);
    arr.insert(key3, 300);
    arr.insert(key5, 500);

    Key16 key_not_found = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10};
    auto it = arr.find(key_not_found);
    REQUIRE(it == arr.end());
  }

  SECTION("Lower bound with 16-byte keys") {
    arr.insert(key1, 100);
    arr.insert(key3, 300);
    arr.insert(key5, 500);

    // Lower bound of key2 should point to key3
    auto it = arr.lower_bound(key2);
    REQUIRE(it != arr.end());
    REQUIRE(it->first == key3);

    // Lower bound of key4 should point to key5
    auto it2 = arr.lower_bound(key4);
    REQUIRE(it2 != arr.end());
    REQUIRE(it2->first == key5);

    // Lower bound of key beyond all should be end()
    Key16 key_large = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    auto it3 = arr.lower_bound(key_large);
    REQUIRE(it3 == arr.end());
  }
}

// Test search for 32-byte keys (byte arrays don't support SIMD mode)
TEMPLATE_TEST_CASE("dense_map search with 32-byte keys",
                   "[dense_map][simd]", BinarySearchMode) {
  constexpr SearchMode Mode = TestType::value;
  using Key32 = std::array<uint8_t, 32>;

  dense_map<Key32, int, 10, std::less<Key32>, Mode> arr;

  // Create test keys with distinct values
  Key32 key1 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  Key32 key2 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2};
  Key32 key3 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3};
  Key32 key4 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4};
  Key32 key5 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5};

  SECTION("Insert and find 32-byte keys") {
    arr.insert(key3, 300);
    arr.insert(key1, 100);
    arr.insert(key5, 500);
    arr.insert(key2, 200);
    arr.insert(key4, 400);

    REQUIRE(arr.size() == 5);

    // Verify all keys can be found
    auto it1 = arr.find(key1);
    REQUIRE(it1 != arr.end());
    REQUIRE(it1->second == 100);

    auto it2 = arr.find(key2);
    REQUIRE(it2 != arr.end());
    REQUIRE(it2->second == 200);

    auto it3 = arr.find(key3);
    REQUIRE(it3 != arr.end());
    REQUIRE(it3->second == 300);

    auto it4 = arr.find(key4);
    REQUIRE(it4 != arr.end());
    REQUIRE(it4->second == 400);

    auto it5 = arr.find(key5);
    REQUIRE(it5 != arr.end());
    REQUIRE(it5->second == 500);

    // Verify keys are in sorted order
    auto it = arr.begin();
    REQUIRE(it->first == key1);
    ++it;
    REQUIRE(it->first == key2);
    ++it;
    REQUIRE(it->first == key3);
    ++it;
    REQUIRE(it->first == key4);
    ++it;
    REQUIRE(it->first == key5);
  }

  SECTION("Find non-existent 32-byte key") {
    arr.insert(key1, 100);
    arr.insert(key3, 300);
    arr.insert(key5, 500);

    Key32 key_not_found = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10};
    auto it = arr.find(key_not_found);
    REQUIRE(it == arr.end());
  }

  SECTION("Lower bound with 32-byte keys") {
    arr.insert(key1, 100);
    arr.insert(key3, 300);
    arr.insert(key5, 500);

    // Lower bound of key2 should point to key3
    auto it = arr.lower_bound(key2);
    REQUIRE(it != arr.end());
    REQUIRE(it->first == key3);

    // Lower bound of key4 should point to key5
    auto it2 = arr.lower_bound(key4);
    REQUIRE(it2 != arr.end());
    REQUIRE(it2->first == key5);

    // Lower bound of key beyond all should be end()
    Key32 key_large = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    auto it3 = arr.lower_bound(key_large);
    REQUIRE(it3 == arr.end());
  }

  SECTION("Test lexicographic comparison with 32-byte keys") {
    // Test keys that differ in different byte positions
    Key32 key_a = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    Key32 key_b = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    Key32 key_c = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    arr.insert(key_b, 2);
    arr.insert(key_a, 1);
    arr.insert(key_c, 3);

    REQUIRE(arr.size() == 3);

    // Verify sorted order: key_a < key_b < key_c
    auto it = arr.begin();
    REQUIRE(it->first == key_a);
    REQUIRE(it->second == 1);
    ++it;
    REQUIRE(it->first == key_b);
    REQUIRE(it->second == 2);
    ++it;
    REQUIRE(it->first == key_c);
    REQUIRE(it->second == 3);
  }
}

// ============================================================================
// SIMD Tests for 8-bit and 16-bit primitive types
// ============================================================================

TEMPLATE_TEST_CASE("dense_map SIMD search with 8-bit signed integers",
                   "[dense_map][simd]", BinarySearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("int8_t basic operations") {
    dense_map<int8_t, int, 64, std::less<int8_t>, Mode> arr;

    // Test with positive values
    arr.insert(10, 100);
    arr.insert(5, 50);
    arr.insert(20, 200);
    arr.insert(-5, -50);
    arr.insert(0, 0);

    REQUIRE(arr.size() == 5);

    // Verify find works
    auto it = arr.find(10);
    REQUIRE(it != arr.end());
    REQUIRE(it->second == 100);

    // Verify negative values
    it = arr.find(-5);
    REQUIRE(it != arr.end());
    REQUIRE(it->second == -50);

    // Verify sorted order
    auto begin = arr.begin();
    REQUIRE(begin->first == -5);
    REQUIRE((begin + 1)->first == 0);
    REQUIRE((begin + 2)->first == 5);
    REQUIRE((begin + 3)->first == 10);
    REQUIRE((begin + 4)->first == 20);
  }

  SECTION("int8_t edge cases") {
    dense_map<int8_t, int, 64, std::less<int8_t>, Mode> arr;

    // Test minimum and maximum values
    arr.insert(127, 1);   // INT8_MAX
    arr.insert(-128, 2);  // INT8_MIN
    arr.insert(0, 3);

    REQUIRE(arr.size() == 3);

    auto it = arr.find(127);
    REQUIRE(it != arr.end());
    REQUIRE(it->second == 1);

    it = arr.find(-128);
    REQUIRE(it != arr.end());
    REQUIRE(it->second == 2);

    // Verify sorted order
    auto begin = arr.begin();
    REQUIRE(begin->first == -128);
    REQUIRE((begin + 1)->first == 0);
    REQUIRE((begin + 2)->first == 127);
  }

  SECTION("int8_t with many elements (32+)") {
    dense_map<int8_t, int, 64, std::less<int8_t>, Mode> arr;

    // Insert 40 elements to test full AVX2 path (32 at a time)
    for (int8_t i = 0; i < 40; ++i) {
      arr.insert(i, i * 10);
    }

    REQUIRE(arr.size() == 40);

    // Verify all can be found
    for (int8_t i = 0; i < 40; ++i) {
      auto it = arr.find(i);
      REQUIRE(it != arr.end());
      REQUIRE(it->second == i * 10);
    }
  }
}

TEMPLATE_TEST_CASE("dense_map SIMD search with 8-bit unsigned integers",
                   "[dense_map][simd]", BinarySearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("uint8_t basic operations") {
    dense_map<uint8_t, int, 64, std::less<uint8_t>, Mode> arr;

    arr.insert(10, 100);
    arr.insert(5, 50);
    arr.insert(200, 2000);
    arr.insert(255, 2550);
    arr.insert(0, 0);

    REQUIRE(arr.size() == 5);

    // Verify find works
    auto it = arr.find(200);
    REQUIRE(it != arr.end());
    REQUIRE(it->second == 2000);

    // Verify max value
    it = arr.find(255);
    REQUIRE(it != arr.end());
    REQUIRE(it->second == 2550);

    // Verify sorted order
    auto begin = arr.begin();
    REQUIRE(begin->first == 0);
    REQUIRE((begin + 1)->first == 5);
    REQUIRE((begin + 2)->first == 10);
    REQUIRE((begin + 3)->first == 200);
    REQUIRE((begin + 4)->first == 255);
  }

  SECTION("uint8_t high values (tests sign bit handling)") {
    dense_map<uint8_t, int, 64, std::less<uint8_t>, Mode> arr;

    // Values > 127 have sign bit set in two's complement
    arr.insert(128, 1);
    arr.insert(200, 2);
    arr.insert(255, 3);
    arr.insert(127, 4);
    arr.insert(100, 5);

    REQUIRE(arr.size() == 5);

    // Verify sorted order (must be unsigned comparison)
    auto begin = arr.begin();
    REQUIRE(begin->first == 100);
    REQUIRE((begin + 1)->first == 127);
    REQUIRE((begin + 2)->first == 128);
    REQUIRE((begin + 3)->first == 200);
    REQUIRE((begin + 4)->first == 255);
  }
}

TEMPLATE_TEST_CASE("dense_map SIMD search with 16-bit signed integers",
                   "[dense_map][simd]", BinarySearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("int16_t basic operations") {
    dense_map<int16_t, int, 64, std::less<int16_t>, Mode> arr;

    arr.insert(1000, 100);
    arr.insert(500, 50);
    arr.insert(2000, 200);
    arr.insert(-500, -50);
    arr.insert(0, 0);

    REQUIRE(arr.size() == 5);

    // Verify find works
    auto it = arr.find(1000);
    REQUIRE(it != arr.end());
    REQUIRE(it->second == 100);

    // Verify negative values
    it = arr.find(-500);
    REQUIRE(it != arr.end());
    REQUIRE(it->second == -50);

    // Verify sorted order
    auto begin = arr.begin();
    REQUIRE(begin->first == -500);
    REQUIRE((begin + 1)->first == 0);
    REQUIRE((begin + 2)->first == 500);
    REQUIRE((begin + 3)->first == 1000);
    REQUIRE((begin + 4)->first == 2000);
  }

  SECTION("int16_t edge cases") {
    dense_map<int16_t, int, 64, std::less<int16_t>, Mode> arr;

    // Test minimum and maximum values
    arr.insert(32767, 1);   // INT16_MAX
    arr.insert(-32768, 2);  // INT16_MIN
    arr.insert(0, 3);

    REQUIRE(arr.size() == 3);

    auto it = arr.find(32767);
    REQUIRE(it != arr.end());
    REQUIRE(it->second == 1);

    it = arr.find(-32768);
    REQUIRE(it != arr.end());
    REQUIRE(it->second == 2);

    // Verify sorted order
    auto begin = arr.begin();
    REQUIRE(begin->first == -32768);
    REQUIRE((begin + 1)->first == 0);
    REQUIRE((begin + 2)->first == 32767);
  }

  SECTION("int16_t with many elements (16+)") {
    dense_map<int16_t, int, 64, std::less<int16_t>, Mode> arr;

    // Insert 20 elements to test full AVX2 path (16 at a time)
    for (int16_t i = 0; i < 20; ++i) {
      arr.insert(i * 100, i);
    }

    REQUIRE(arr.size() == 20);

    // Verify all can be found
    for (int16_t i = 0; i < 20; ++i) {
      auto it = arr.find(i * 100);
      REQUIRE(it != arr.end());
      REQUIRE(it->second == i);
    }
  }
}

TEMPLATE_TEST_CASE("dense_map SIMD search with 16-bit unsigned integers",
                   "[dense_map][simd]", BinarySearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("uint16_t basic operations") {
    dense_map<uint16_t, int, 64, std::less<uint16_t>, Mode> arr;

    arr.insert(1000, 100);
    arr.insert(500, 50);
    arr.insert(40000, 400);
    arr.insert(65535, 655);
    arr.insert(0, 0);

    REQUIRE(arr.size() == 5);

    // Verify find works
    auto it = arr.find(40000);
    REQUIRE(it != arr.end());
    REQUIRE(it->second == 400);

    // Verify max value
    it = arr.find(65535);
    REQUIRE(it != arr.end());
    REQUIRE(it->second == 655);

    // Verify sorted order
    auto begin = arr.begin();
    REQUIRE(begin->first == 0);
    REQUIRE((begin + 1)->first == 500);
    REQUIRE((begin + 2)->first == 1000);
    REQUIRE((begin + 3)->first == 40000);
    REQUIRE((begin + 4)->first == 65535);
  }

  SECTION("uint16_t high values (tests sign bit handling)") {
    dense_map<uint16_t, int, 64, std::less<uint16_t>, Mode> arr;

    // Values > 32767 have sign bit set in two's complement
    arr.insert(32768, 1);
    arr.insert(40000, 2);
    arr.insert(65535, 3);
    arr.insert(32767, 4);
    arr.insert(10000, 5);

    REQUIRE(arr.size() == 5);

    // Verify sorted order (must be unsigned comparison)
    auto begin = arr.begin();
    REQUIRE(begin->first == 10000);
    REQUIRE((begin + 1)->first == 32767);
    REQUIRE((begin + 2)->first == 32768);
    REQUIRE((begin + 3)->first == 40000);
    REQUIRE((begin + 4)->first == 65535);
  }
}

// Test with char types (may alias to int8_t or be separate type)
TEMPLATE_TEST_CASE("dense_map SIMD search with char types",
                   "[dense_map][simd]", BinarySearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("unsigned char basic operations") {
    dense_map<unsigned char, int, 64, std::less<unsigned char>, Mode> arr;

    arr.insert('A', 65);
    arr.insert('Z', 90);
    arr.insert('a', 97);
    arr.insert('z', 122);
    arr.insert('0', 48);

    REQUIRE(arr.size() == 5);

    // Verify find works
    auto it = arr.find('A');
    REQUIRE(it != arr.end());
    REQUIRE(it->second == 65);

    // Verify sorted order (ASCII)
    auto begin = arr.begin();
    REQUIRE(begin->first == '0');
    REQUIRE((begin + 1)->first == 'A');
    REQUIRE((begin + 2)->first == 'Z');
    REQUIRE((begin + 3)->first == 'a');
    REQUIRE((begin + 4)->first == 'z');
  }
}

// Test with short types (may alias to int16_t or be separate type)
TEMPLATE_TEST_CASE("dense_map SIMD search with short types",
                   "[dense_map][simd]", BinarySearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("unsigned short basic operations") {
    dense_map<unsigned short, int, 64, std::less<unsigned short>, Mode> arr;

    arr.insert(1000, 100);
    arr.insert(500, 50);
    arr.insert(40000, 400);
    arr.insert(0, 0);

    REQUIRE(arr.size() == 4);

    // Verify find works
    auto it = arr.find(40000);
    REQUIRE(it != arr.end());
    REQUIRE(it->second == 400);

    // Verify sorted order
    auto begin = arr.begin();
    REQUIRE(begin->first == 0);
    REQUIRE((begin + 1)->first == 500);
    REQUIRE((begin + 2)->first == 1000);
    REQUIRE((begin + 3)->first == 40000);
  }
}

TEST_CASE("dense_map with std::greater (descending order)",
          "[dense_map][comparator]") {
  SECTION("Binary search mode") {
    dense_map<int, std::string, 10, std::greater<int>, SearchMode::Binary>
        arr;

    // Insert elements (they should be stored in descending order)
    arr.insert(5, "five");
    arr.insert(10, "ten");
    arr.insert(3, "three");
    arr.insert(7, "seven");
    arr.insert(1, "one");

    REQUIRE(arr.size() == 5);

    // Verify descending order
    auto it = arr.begin();
    REQUIRE(it->first == 10);
    ++it;
    REQUIRE(it->first == 7);
    ++it;
    REQUIRE(it->first == 5);
    ++it;
    REQUIRE(it->first == 3);
    ++it;
    REQUIRE(it->first == 1);

    // Verify find works
    auto found = arr.find(7);
    REQUIRE(found != arr.end());
    REQUIRE(found->second == "seven");

    // Verify lower_bound works (finds first element >= key in descending order)
    auto lb = arr.lower_bound(6);
    REQUIRE(lb != arr.end());
    REQUIRE(lb->first == 5);  // 5 is the first element >= 6 in descending order

    // Verify erase works
    arr.erase(7);
    REQUIRE(arr.size() == 4);
    REQUIRE(arr.find(7) == arr.end());

    // Verify order is maintained after erase
    it = arr.begin();
    REQUIRE(it->first == 10);
    ++it;
    REQUIRE(it->first == 5);
    ++it;
    REQUIRE(it->first == 3);
    ++it;
    REQUIRE(it->first == 1);
  }

  SECTION("Linear search mode") {
    dense_map<int, std::string, 10, std::greater<int>, SearchMode::Linear>
        arr;

    arr.insert(15, "fifteen");
    arr.insert(25, "twenty-five");
    arr.insert(5, "five");
    arr.insert(20, "twenty");

    REQUIRE(arr.size() == 4);

    // Verify descending order
    auto it = arr.begin();
    REQUIRE(it->first == 25);
    ++it;
    REQUIRE(it->first == 20);
    ++it;
    REQUIRE(it->first == 15);
    ++it;
    REQUIRE(it->first == 5);

    // Verify find works
    REQUIRE(arr.find(20) != arr.end());
    REQUIRE(arr.find(20)->second == "twenty");
    REQUIRE(arr.find(100) == arr.end());
  }

  SECTION("SIMD search mode with 4-byte keys (int32_t)") {
    dense_map<int32_t, std::string, 64, std::greater<int32_t>,
                  SearchMode::SIMD>
        arr;

    // Insert many elements to test SIMD paths
    for (int32_t i = 0; i < 50; ++i) {
      arr.insert(i, "value" + std::to_string(i));
    }

    REQUIRE(arr.size() == 50);

    // Verify descending order
    auto it = arr.begin();
    for (int32_t i = 49; i >= 0; --i) {
      REQUIRE(it->first == i);
      ++it;
    }

    // Verify find works for various values
    REQUIRE(arr.find(25) != arr.end());
    REQUIRE(arr.find(25)->second == "value25");
    REQUIRE(arr.find(0) != arr.end());
    REQUIRE(arr.find(49) != arr.end());
    REQUIRE(arr.find(100) == arr.end());

    // Verify lower_bound finds correct position
    auto lb = arr.lower_bound(26);
    REQUIRE(lb != arr.end());
    REQUIRE(lb->first == 26);

    lb = arr.lower_bound(100);
    REQUIRE(lb == arr.begin());  // 49 is first element >= 100

    lb = arr.lower_bound(-10);
    REQUIRE(lb == arr.end());  // No element >= -10 in descending order
  }

  SECTION("SIMD search mode with 8-byte keys (int64_t)") {
    dense_map<int64_t, int, 64, std::greater<int64_t>, SearchMode::SIMD>
        arr;

    // Insert values to test 8-byte SIMD implementation
    for (int64_t i = 0; i < 40; ++i) {
      arr.insert(i * 10, static_cast<int>(i));
    }

    REQUIRE(arr.size() == 40);

    // Verify descending order
    auto it = arr.begin();
    REQUIRE(it->first == 390);
    ++it;
    REQUIRE(it->first == 380);

    // Verify find
    REQUIRE(arr.find(200) != arr.end());
    REQUIRE(arr.find(200)->second == 20);
    REQUIRE(arr.find(205) == arr.end());
  }

  SECTION("SIMD search mode with 4-byte keys (float)") {
    dense_map<float, int, 64, std::greater<float>, SearchMode::SIMD> arr;

    // Insert float values
    arr.insert(3.14f, 1);
    arr.insert(2.71f, 2);
    arr.insert(1.41f, 3);
    arr.insert(9.99f, 4);
    arr.insert(5.55f, 5);

    REQUIRE(arr.size() == 5);

    // Verify descending order
    auto it = arr.begin();
    REQUIRE(it->first == 9.99f);
    ++it;
    REQUIRE(it->first == 5.55f);
    ++it;
    REQUIRE(it->first == 3.14f);

    // Verify find
    REQUIRE(arr.find(3.14f) != arr.end());
    REQUIRE(arr.find(3.14f)->second == 1);
  }

  SECTION("SIMD search mode with 8-byte keys (double)") {
    dense_map<double, int, 64, std::greater<double>, SearchMode::SIMD> arr;

    // Insert double values
    for (int i = 0; i < 30; ++i) {
      arr.insert(i * 1.5, i);
    }

    REQUIRE(arr.size() == 30);

    // Verify descending order
    auto it = arr.begin();
    REQUIRE(it->first == 29 * 1.5);

    // Verify find
    REQUIRE(arr.find(15.0) != arr.end());
    REQUIRE(arr.find(15.0)->second == 10);
  }

  SECTION("SIMD search mode with 2-byte keys (int16_t)") {
    dense_map<int16_t, int, 64, std::greater<int16_t>, SearchMode::SIMD>
        arr;

    // Insert int16_t values
    for (int16_t i = 0; i < 50; ++i) {
      arr.insert(i, static_cast<int>(i * 2));
    }

    REQUIRE(arr.size() == 50);

    // Verify descending order
    auto it = arr.begin();
    REQUIRE(it->first == 49);
    ++it;
    REQUIRE(it->first == 48);

    // Verify find
    REQUIRE(arr.find(25) != arr.end());
    REQUIRE(arr.find(25)->second == 50);
  }

  SECTION("SIMD search mode with 1-byte keys (int8_t)") {
    dense_map<int8_t, int, 64, std::greater<int8_t>, SearchMode::SIMD> arr;

    // Insert int8_t values
    for (int8_t i = 0; i < 50; ++i) {
      arr.insert(i, static_cast<int>(i));
    }

    REQUIRE(arr.size() == 50);

    // Verify descending order
    auto it = arr.begin();
    REQUIRE(it->first == 49);
    ++it;
    REQUIRE(it->first == 48);

    // Verify find
    REQUIRE(arr.find(25) != arr.end());
    REQUIRE(arr.find(25)->second == 25);
    REQUIRE(arr.find(100) == arr.end());
  }
}
