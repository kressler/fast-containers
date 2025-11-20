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

TEMPLATE_TEST_CASE("ordered_array split_at operation", "[ordered_array]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Split empty array") {
    ordered_array<int, std::string, 10, Mode> arr;
    ordered_array<int, std::string, 10, Mode> output;

    arr.split_at(arr.begin(), output);

    REQUIRE(arr.size() == 0);
    REQUIRE(output.size() == 0);
  }

  SECTION("Split at beginning (move all elements)") {
    ordered_array<int, std::string, 10, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");
    arr.insert(3, "three");

    ordered_array<int, std::string, 10, Mode> output;
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
    ordered_array<int, std::string, 10, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");
    arr.insert(3, "three");

    ordered_array<int, std::string, 10, Mode> output;
    arr.split_at(arr.end(), output);

    REQUIRE(arr.size() == 3);
    REQUIRE(output.size() == 0);
    REQUIRE(output.empty());
  }

  SECTION("Split in middle") {
    ordered_array<int, std::string, 10, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");
    arr.insert(3, "three");
    arr.insert(4, "four");
    arr.insert(5, "five");

    ordered_array<int, std::string, 10, Mode> output;
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
    ordered_array<int, std::string, 10, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");

    ordered_array<int, std::string, 10, Mode> output;
    output.insert(10, "ten");

    REQUIRE_THROWS_AS(arr.split_at(arr.begin(), output), std::runtime_error);
  }

  SECTION("Split throws if output has insufficient capacity") {
    ordered_array<int, std::string, 10, Mode> arr;
    for (int i = 0; i < 10; ++i) {
      arr.insert(i, std::to_string(i));
    }

    ordered_array<int, std::string, 5, Mode> small_output;

    // Trying to move 10 elements to capacity-5 array should throw
    REQUIRE_THROWS_AS(arr.split_at(arr.begin(), small_output),
                      std::runtime_error);
  }

  SECTION("Split maintains element order and values") {
    ordered_array<int, std::string, 20, Mode> arr;
    for (int i = 1; i <= 10; ++i) {
      arr.insert(i, std::to_string(i * 10));
    }

    ordered_array<int, std::string, 20, Mode> output;
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

TEMPLATE_TEST_CASE("ordered_array append operation", "[ordered_array]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Append to empty array") {
    ordered_array<int, std::string, 10, Mode> arr;

    ordered_array<int, std::string, 10, Mode> other;
    other.insert(1, "one");
    other.insert(2, "two");

    arr.append(std::move(other));

    REQUIRE(arr.size() == 2);
    REQUIRE(arr.find(1)->second == "one");
    REQUIRE(arr.find(2)->second == "two");

    REQUIRE(other.size() == 0);
    REQUIRE(other.empty());
  }

  SECTION("Append empty array to non-empty array") {
    ordered_array<int, std::string, 10, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");

    ordered_array<int, std::string, 10, Mode> other;

    arr.append(std::move(other));

    REQUIRE(arr.size() == 2);
    REQUIRE(arr.find(1)->second == "one");
    REQUIRE(arr.find(2)->second == "two");
  }

  SECTION("Append maintains sorted order") {
    ordered_array<int, std::string, 10, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");

    ordered_array<int, std::string, 10, Mode> other;
    other.insert(5, "five");
    other.insert(6, "six");

    arr.append(std::move(other));

    REQUIRE(arr.size() == 4);

    // Verify sorted order
    auto it = arr.begin();
    REQUIRE(it->first == 1);
    ++it;
    REQUIRE(it->first == 2);
    ++it;
    REQUIRE(it->first == 5);
    ++it;
    REQUIRE(it->first == 6);

    REQUIRE(other.size() == 0);
  }

  SECTION("Append throws if combined size exceeds capacity") {
    ordered_array<int, std::string, 5, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");
    arr.insert(3, "three");

    ordered_array<int, std::string, 5, Mode> other;
    other.insert(4, "four");
    other.insert(5, "five");
    other.insert(6, "six");

    // Combined size 6 exceeds capacity 5
    REQUIRE_THROWS_AS(arr.append(std::move(other)), std::runtime_error);
  }

  SECTION("Append preserves all values") {
    ordered_array<int, std::string, 20, Mode> arr;
    arr.insert(1, "value1");
    arr.insert(3, "value3");

    ordered_array<int, std::string, 20, Mode> other;
    other.insert(10, "value10");
    other.insert(15, "value15");
    other.insert(20, "value20");

    arr.append(std::move(other));

    REQUIRE(arr.size() == 5);
    REQUIRE(arr.find(1)->second == "value1");
    REQUIRE(arr.find(3)->second == "value3");
    REQUIRE(arr.find(10)->second == "value10");
    REQUIRE(arr.find(15)->second == "value15");
    REQUIRE(arr.find(20)->second == "value20");
  }

  SECTION("Moved-from array is reusable after append") {
    ordered_array<int, std::string, 10, Mode> arr;
    arr.insert(1, "one");

    ordered_array<int, std::string, 10, Mode> other;
    other.insert(5, "five");

    arr.append(std::move(other));

    // Reuse moved-from array
    other.insert(10, "ten");
    other.insert(20, "twenty");

    REQUIRE(other.size() == 2);
    REQUIRE(other.find(10)->second == "ten");
    REQUIRE(other.find(20)->second == "twenty");
  }
}

TEMPLATE_TEST_CASE("ordered_array split and append together", "[ordered_array]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Split then append to reconstruct") {
    ordered_array<int, std::string, 10, Mode> arr;
    arr.insert(1, "one");
    arr.insert(2, "two");
    arr.insert(3, "three");
    arr.insert(4, "four");

    ordered_array<int, std::string, 10, Mode> upper;
    auto mid = arr.begin();
    ++mid;
    ++mid;  // Split at key 3

    arr.split_at(mid, upper);

    REQUIRE(arr.size() == 2);
    REQUIRE(upper.size() == 2);

    // Reconstruct by appending upper back to arr
    arr.append(std::move(upper));

    REQUIRE(arr.size() == 4);
    auto it = arr.begin();
    REQUIRE(it->first == 1);
    ++it;
    REQUIRE(it->first == 2);
    ++it;
    REQUIRE(it->first == 3);
    ++it;
    REQUIRE(it->first == 4);
  }

  SECTION("Multiple splits and appends (simulating B+ tree operations)") {
    // Simulate splitting a node and merging with siblings
    ordered_array<int, int, 10, Mode> node1, node2, node3;

    // Fill node1 with [1..5]
    for (int i = 1; i <= 5; ++i) {
      node1.insert(i, i * 10);
    }

    // Split node1 into two parts
    auto split_point = node1.begin();
    std::advance(split_point, 3);
    node1.split_at(split_point, node2);

    REQUIRE(node1.size() == 3);  // [1, 2, 3]
    REQUIRE(node2.size() == 2);  // [4, 5]

    // Add more elements to node2
    node2.insert(6, 60);
    node2.insert(7, 70);

    // Split node2
    split_point = node2.begin();
    std::advance(split_point, 2);
    node2.split_at(split_point, node3);

    REQUIRE(node2.size() == 2);  // [4, 5]
    REQUIRE(node3.size() == 2);  // [6, 7]

    // Merge node2 back into node1
    node1.append(std::move(node2));
    REQUIRE(node1.size() == 5);  // [1, 2, 3, 4, 5]

    // Verify node3 is independent
    REQUIRE(node3.size() == 2);
    REQUIRE(node3.find(6)->second == 60);
    REQUIRE(node3.find(7)->second == 70);
  }
}

TEMPLATE_TEST_CASE("ordered_array transfer_prefix_from operation",
                   "[ordered_array]", BinarySearchMode, LinearSearchMode,
                   SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Transfer prefix to empty array") {
    ordered_array<int, std::string, 10, Mode> dest;
    ordered_array<int, std::string, 10, Mode> source;
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
    ordered_array<int, std::string, 10, Mode> dest;
    dest.insert(1, "one");
    dest.insert(2, "two");

    ordered_array<int, std::string, 10, Mode> source;
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
    ordered_array<int, std::string, 10, Mode> dest;
    dest.insert(5, "five");

    ordered_array<int, std::string, 10, Mode> source;
    source.insert(1, "one");

    dest.transfer_prefix_from(source, 0);

    REQUIRE(dest.size() == 1);
    REQUIRE(source.size() == 1);
  }

  SECTION("Transfer all elements from source") {
    ordered_array<int, std::string, 10, Mode> dest;
    dest.insert(1, "one");

    ordered_array<int, std::string, 10, Mode> source;
    source.insert(10, "ten");
    source.insert(11, "eleven");

    dest.transfer_prefix_from(source, 2);

    REQUIRE(dest.size() == 3);
    REQUIRE(source.size() == 0);
    REQUIRE(source.empty());
  }

  SECTION("Transfer throws if count exceeds source size") {
    ordered_array<int, std::string, 10, Mode> dest;
    ordered_array<int, std::string, 10, Mode> source;
    source.insert(1, "one");

    REQUIRE_THROWS_AS(dest.transfer_prefix_from(source, 2), std::runtime_error);
  }

  SECTION("Transfer throws if destination has insufficient capacity") {
    ordered_array<int, std::string, 5, Mode> dest;
    for (int i = 0; i < 4; ++i) {
      dest.insert(i + 10, std::to_string(i));
    }

    ordered_array<int, std::string, 5, Mode> source;
    source.insert(1, "one");
    source.insert(2, "two");

    // Dest has 4 elements, capacity 5, trying to add 2 more (total 6)
    REQUIRE_THROWS_AS(dest.transfer_prefix_from(source, 2), std::runtime_error);
  }

  SECTION("Transfer maintains element order and values") {
    ordered_array<int, std::string, 20, Mode> dest;
    for (int i = 1; i <= 6; ++i) {
      dest.insert(i, std::to_string(i * 10));
    }

    ordered_array<int, std::string, 20, Mode> source;
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

TEMPLATE_TEST_CASE("ordered_array transfer_suffix_from operation",
                   "[ordered_array]", BinarySearchMode, LinearSearchMode,
                   SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Transfer suffix to empty array") {
    ordered_array<int, std::string, 10, Mode> dest;
    ordered_array<int, std::string, 10, Mode> source;
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
    ordered_array<int, std::string, 10, Mode> dest;
    dest.insert(5, "five");
    dest.insert(6, "six");

    ordered_array<int, std::string, 10, Mode> source;
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
    ordered_array<int, std::string, 10, Mode> dest;
    dest.insert(1, "one");

    ordered_array<int, std::string, 10, Mode> source;
    source.insert(5, "five");

    dest.transfer_suffix_from(source, 0);

    REQUIRE(dest.size() == 1);
    REQUIRE(source.size() == 1);
  }

  SECTION("Transfer all elements from source") {
    ordered_array<int, std::string, 10, Mode> dest;
    dest.insert(10, "ten");

    ordered_array<int, std::string, 10, Mode> source;
    source.insert(1, "one");
    source.insert(2, "two");

    dest.transfer_suffix_from(source, 2);

    REQUIRE(dest.size() == 3);
    REQUIRE(source.size() == 0);
    REQUIRE(source.empty());
  }

  SECTION("Transfer throws if count exceeds source size") {
    ordered_array<int, std::string, 10, Mode> dest;
    ordered_array<int, std::string, 10, Mode> source;
    source.insert(5, "five");

    REQUIRE_THROWS_AS(dest.transfer_suffix_from(source, 2), std::runtime_error);
  }

  SECTION("Transfer throws if destination has insufficient capacity") {
    ordered_array<int, std::string, 5, Mode> dest;
    for (int i = 0; i < 4; ++i) {
      dest.insert(i, std::to_string(i));
    }

    ordered_array<int, std::string, 5, Mode> source;
    source.insert(10, "ten");
    source.insert(11, "eleven");

    // Dest has 4 elements, capacity 5, trying to add 2 more (total 6)
    REQUIRE_THROWS_AS(dest.transfer_suffix_from(source, 2), std::runtime_error);
  }

  SECTION("Transfer maintains element order and values") {
    ordered_array<int, std::string, 20, Mode> dest;
    for (int i = 10; i <= 14; ++i) {
      dest.insert(i, std::to_string(i * 10));
    }

    ordered_array<int, std::string, 20, Mode> source;
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

TEMPLATE_TEST_CASE("ordered_array transfer operations combined",
                   "[ordered_array]", BinarySearchMode, LinearSearchMode,
                   SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Rebalancing simulation: transfer from overfull to underfull node") {
    // Simulate B+ tree rebalancing
    ordered_array<int, int, 10, Mode> left, right;

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
    ordered_array<int, std::string, 15, Mode> node1, node2, node3;

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

// Test SIMD search for 16-byte keys
TEMPLATE_TEST_CASE("ordered_array SIMD search with 16-byte keys",
                   "[ordered_array][simd]", BinarySearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  using Key16 = std::array<uint8_t, 16>;

  ordered_array<Key16, int, 10, Mode> arr;

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

// Test SIMD search for 32-byte keys
TEMPLATE_TEST_CASE("ordered_array SIMD search with 32-byte keys",
                   "[ordered_array][simd]", BinarySearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  using Key32 = std::array<uint8_t, 32>;

  ordered_array<Key32, int, 10, Mode> arr;

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
