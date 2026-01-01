// Copyright (c) 2025 Bryan Kressler
//
// SPDX-License-Identifier: BSD-3-Clause

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fast_containers/btree.hpp>
#include <map>
#include <string>

using namespace kressler::fast_containers;

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
                   "[btree][constructor]", BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Default constructor - int, int") {
    btree<int, int, 64, 64, std::less<int>, Mode> tree;
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }

  SECTION("Default constructor - int, std::string") {
    btree<int, std::string, 64, 64, std::less<int>, Mode> tree;
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }

  SECTION("Default constructor - std::string, int") {
    btree<std::string, int, 64, 64, std::less<std::string>, Mode> tree;
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }

  SECTION("Default constructor - different node sizes") {
    btree<int, int, 16, 64, std::less<int>, Mode> tree;
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }

  SECTION("Default constructor - small node sizes") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }
}

TEMPLATE_TEST_CASE("btree destructor deallocates resources",
                   "[btree][destructor]", BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Destructor on empty tree") {
    btree<int, int, 64, 64, std::less<int>, Mode> tree;
    // Destructor called automatically - should not crash
  }

  SECTION("Multiple constructions and destructions") {
    for (int i = 0; i < 10; ++i) {
      btree<int, std::string, 32, 32, std::less<int>, Mode> tree;
      REQUIRE(tree.empty());
    }
  }
}

TEMPLATE_TEST_CASE("btree size and empty methods", "[btree][size]",
                   BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Empty tree reports size 0") {
    btree<int, int, 64, 64, std::less<int>, Mode> tree;
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.empty() == true);
  }

  SECTION("Size and empty are consistent") {
    btree<int, std::string, 64, 64, std::less<int>, Mode> tree;
    REQUIRE(tree.empty() == (tree.size() == 0));
  }
}

TEMPLATE_TEST_CASE("btree works with different template parameters",
                   "[btree][template]", BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Small leaf nodes") {
    btree<int, int, 8, 64, std::less<int>, Mode> tree;
    REQUIRE(tree.empty());
  }

  SECTION("Small internal nodes") {
    btree<int, int, 64, 8, std::less<int>, Mode> tree;
    REQUIRE(tree.empty());
  }

  SECTION("Both small") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    REQUIRE(tree.empty());
  }

  SECTION("Large value type") {
    struct LargeValue {
      char data[1024];
    };
    btree<int, LargeValue, 16, 64, std::less<int>, Mode> tree;
    REQUIRE(tree.empty());
  }
}

TEST_CASE("btree works with different MoveMode", "[btree][movemode]") {
  SECTION("Standard MoveMode") {
    btree<int, int, 64, 64, std::less<int>, SearchMode::Binary> tree;
    REQUIRE(tree.empty());
  }

  SECTION("SIMD MoveMode") {
    btree<int, int, 64, 64, std::less<int>, SearchMode::Binary> tree;
    REQUIRE(tree.empty());
  }
}

// Helper function to populate tree with data using insert()
template <typename BTree>
void populate_tree(BTree& tree, std::vector<std::pair<int, int>> data) {
  for (const auto& [key, value] : data) {
    tree.insert(key, value);
  }
}

TEMPLATE_TEST_CASE("btree empty tree iterators", "[btree][iterator]",
                   BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Empty tree begin() == end()") {
    btree<int, int, 64, 64, std::less<int>, Mode> tree;
    REQUIRE(tree.begin() == tree.end());
  }

  SECTION("Empty tree find returns end()") {
    btree<int, int, 64, 64, std::less<int>, Mode> tree;
    REQUIRE(tree.find(42) == tree.end());
  }
}

TEMPLATE_TEST_CASE("btree single-leaf tree operations",
                   "[btree][find][iterator]", BinarySearchMode,
                   LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  using BTree = btree<int, int, 64, 64, std::less<int>, Mode>;

  SECTION("Find existing elements") {
    BTree tree;
    populate_tree(tree, {{1, 10}, {3, 30}, {5, 50}});

    auto it = tree.find(3);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 3);
    REQUIRE(it->second == 30);

    it = tree.find(1);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 1);
    REQUIRE(it->second == 10);

    it = tree.find(5);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 5);
    REQUIRE(it->second == 50);
  }

  SECTION("Find non-existing elements") {
    BTree tree;
    populate_tree(tree, {{1, 10}, {3, 30}, {5, 50}});

    REQUIRE(tree.find(0) == tree.end());
    REQUIRE(tree.find(2) == tree.end());
    REQUIRE(tree.find(4) == tree.end());
    REQUIRE(tree.find(10) == tree.end());
  }

  SECTION("Iterate through elements") {
    BTree tree;
    populate_tree(tree, {{1, 10}, {3, 30}, {5, 50}, {7, 70}});

    std::vector<std::pair<int, int>> collected;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
      collected.push_back({it->first, it->second});
    }

    REQUIRE(collected.size() == 4);
    REQUIRE(collected[0] == std::pair{1, 10});
    REQUIRE(collected[1] == std::pair{3, 30});
    REQUIRE(collected[2] == std::pair{5, 50});
    REQUIRE(collected[3] == std::pair{7, 70});
  }

  SECTION("Range-based for loop") {
    BTree tree;
    populate_tree(tree, {{2, 20}, {4, 40}, {6, 60}});

    int sum_keys = 0;
    int sum_values = 0;
    for (auto pair : tree) {
      sum_keys += pair.first;
      sum_values += pair.second;
    }

    REQUIRE(sum_keys == 2 + 4 + 6);
    REQUIRE(sum_values == 20 + 40 + 60);
  }
}

TEMPLATE_TEST_CASE("btree insert operations", "[btree][insert]",
                   BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  using BTree = btree<int, int, 64, 64, std::less<int>, Mode>;

  SECTION("Insert into empty tree") {
    BTree tree;
    auto [it, inserted] = tree.insert(5, 50);

    REQUIRE(inserted == true);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 5);
    REQUIRE(it->second == 50);
    REQUIRE(tree.size() == 1);
    REQUIRE(!tree.empty());
  }

  SECTION("Insert multiple elements in ascending order") {
    BTree tree;
    for (int i = 1; i <= 10; ++i) {
      auto [it, inserted] = tree.insert(i, i * 10);
      REQUIRE(inserted == true);
      REQUIRE(it->first == i);
      REQUIRE(it->second == i * 10);
    }

    REQUIRE(tree.size() == 10);

    // Verify all elements can be found
    for (int i = 1; i <= 10; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->first == i);
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("Insert multiple elements in descending order") {
    BTree tree;
    for (int i = 10; i >= 1; --i) {
      auto [it, inserted] = tree.insert(i, i * 10);
      REQUIRE(inserted == true);
      REQUIRE(it->first == i);
      REQUIRE(it->second == i * 10);
    }

    REQUIRE(tree.size() == 10);

    // Verify iteration is in sorted order
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  }

  SECTION("Insert elements in random order") {
    BTree tree;
    std::vector<int> insert_order = {5, 2, 8, 1, 9, 3, 7, 4, 6, 10};

    for (int key : insert_order) {
      auto [it, inserted] = tree.insert(key, key * 10);
      REQUIRE(inserted == true);
    }

    REQUIRE(tree.size() == 10);

    // Verify sorted iteration
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  }

  SECTION("Insert duplicate key returns false") {
    BTree tree;

    // First insert
    auto [it1, inserted1] = tree.insert(5, 50);
    REQUIRE(inserted1 == true);
    REQUIRE(it1->first == 5);
    REQUIRE(it1->second == 50);
    REQUIRE(tree.size() == 1);

    // Duplicate insert - should not modify tree
    auto [it2, inserted2] = tree.insert(5, 99);
    REQUIRE(inserted2 == false);
    REQUIRE(it2->first == 5);
    REQUIRE(it2->second == 50);  // Value unchanged
    REQUIRE(tree.size() == 1);   // Size unchanged
  }

  SECTION("Insert with std::pair (STL compatibility)") {
    BTree tree;

    // Insert using pair - matches std::map API
    auto [it1, inserted1] = tree.insert({5, 50});
    REQUIRE(inserted1 == true);
    REQUIRE(it1->first == 5);
    REQUIRE(it1->second == 50);
    REQUIRE(tree.size() == 1);

    // Insert multiple using pairs
    tree.insert({1, 10});
    tree.insert({3, 30});
    tree.insert({7, 70});
    REQUIRE(tree.size() == 4);

    // Verify all elements
    REQUIRE(tree.find(1)->second == 10);
    REQUIRE(tree.find(3)->second == 30);
    REQUIRE(tree.find(5)->second == 50);
    REQUIRE(tree.find(7)->second == 70);

    // Duplicate insert with pair returns false
    auto [it2, inserted2] = tree.insert({5, 99});
    REQUIRE(inserted2 == false);
    REQUIRE(it2->second == 50);  // Value unchanged
    REQUIRE(tree.size() == 4);   // Size unchanged
  }

  SECTION("Insert and find interleaved") {
    BTree tree;

    tree.insert(3, 30);
    tree.insert(1, 10);
    tree.insert(5, 50);

    auto it1 = tree.find(1);
    REQUIRE(it1 != tree.end());
    REQUIRE(it1->second == 10);

    tree.insert(2, 20);
    tree.insert(4, 40);

    auto it2 = tree.find(2);
    REQUIRE(it2 != tree.end());
    REQUIRE(it2->second == 20);

    // Verify all elements
    REQUIRE(tree.size() == 5);
    for (int i = 1; i <= 5; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("Insert fills node to capacity") {
    // Create a tree with small node size
    btree<int, int, 8, 8, std::less<int>, Mode> small_tree;

    // Insert up to capacity (8 elements)
    for (int i = 1; i <= 8; ++i) {
      auto [it, inserted] = small_tree.insert(i, i * 10);
      REQUIRE(inserted == true);
    }

    REQUIRE(small_tree.size() == 8);

    // Verify all elements
    std::vector<int> keys;
    for (auto pair : small_tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8});
  }

  SECTION("Insert with string keys") {
    btree<std::string, int, 64, 64, std::less<std::string>, Mode> tree;

    tree.insert("banana", 2);
    tree.insert("apple", 1);
    tree.insert("cherry", 3);

    REQUIRE(tree.size() == 3);

    auto it = tree.find("banana");
    REQUIRE(it != tree.end());
    REQUIRE(it->second == 2);

    // Verify sorted order
    std::vector<std::string> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys == std::vector<std::string>{"apple", "banana", "cherry"});
  }

  SECTION("Insert with string values") {
    btree<int, std::string, 64, 64, std::less<int>, Mode> tree;

    tree.insert(2, "two");
    tree.insert(1, "one");
    tree.insert(3, "three");

    auto it = tree.find(2);
    REQUIRE(it != tree.end());
    REQUIRE(it->second == "two");

    REQUIRE(tree.size() == 3);
  }
}

TEMPLATE_TEST_CASE("btree node splitting", "[btree][insert][split]",
                   BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Split leaf node when full") {
    // Create tree with small leaf size to force splitting
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert 9 elements (leaf size is 8, so should split)
    for (int i = 1; i <= 9; ++i) {
      auto [it, inserted] = tree.insert(i, i * 10);
      REQUIRE(inserted == true);
      REQUIRE(it->first == i);
      REQUIRE(it->second == i * 10);
    }

    REQUIRE(tree.size() == 9);

    // Verify all elements can be found
    for (int i = 1; i <= 9; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->first == i);
      REQUIRE(it->second == i * 10);
    }

    // Verify sorted iteration
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9});
  }

  SECTION("Multiple leaf splits") {
    // Create tree with small node size
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert 20 elements (will cause multiple splits)
    for (int i = 1; i <= 20; ++i) {
      auto [it, inserted] = tree.insert(i, i * 10);
      REQUIRE(inserted == true);
    }

    REQUIRE(tree.size() == 20);

    // Verify all elements
    for (int i = 1; i <= 20; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }

    // Verify sorted iteration
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys.size() == 20);
    for (size_t i = 0; i < 20; ++i) {
      REQUIRE(keys[i] == static_cast<int>(i + 1));
    }
  }

  SECTION("Split with descending insertion") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert in descending order (harder test case for splitting)
    for (int i = 18; i >= 1; --i) {
      auto [it, inserted] = tree.insert(i, i * 10);
      REQUIRE(inserted == true);
    }

    REQUIRE(tree.size() == 18);

    // Verify sorted iteration (should be ascending despite descending insert)
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys.size() == 18);
    for (int i = 0; i < 18; ++i) {
      REQUIRE(keys[i] == i + 1);
    }
  }

  SECTION("Split with random insertion") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    std::vector<int> insert_order = {9, 3,  15, 1,  17, 5,  13, 7,  11,
                                     2, 16, 4,  14, 6,  12, 8,  10, 18};
    for (int key : insert_order) {
      auto [it, inserted] = tree.insert(key, key * 10);
      REQUIRE(inserted == true);
    }

    REQUIRE(tree.size() == 18);

    // Verify sorted iteration
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys.size() == 18);
    for (int i = 0; i < 18; ++i) {
      REQUIRE(keys[i] == i + 1);
    }
  }

  SECTION("Internal node splitting") {
    // Small internal node size to force internal node splits
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert many elements to force multiple levels and internal splits
    // With leaf size 8, internal size 8: need 65+ elements for internal split
    for (int i = 1; i <= 70; ++i) {
      auto [it, inserted] = tree.insert(i, i * 10);
      REQUIRE(inserted == true);
    }

    REQUIRE(tree.size() == 70);

    // Verify all elements
    for (int i = 1; i <= 70; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }

    // Verify sorted iteration
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys.size() == 70);
    for (size_t i = 0; i < 70; ++i) {
      REQUIRE(keys[i] == static_cast<int>(i + 1));
    }
  }

  SECTION("Duplicate during split") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Fill tree to cause splits
    for (int i = 1; i <= 18; ++i) {
      tree.insert(i, i * 10);
    }

    // Try to insert duplicate (should return false even after splits)
    auto [it, inserted] = tree.insert(9, 999);
    REQUIRE(inserted == false);
    REQUIRE(it->first == 9);
    REQUIRE(it->second == 90);  // Original value unchanged
    REQUIRE(tree.size() == 18);
  }
}

TEMPLATE_TEST_CASE("btree erase operations - basic", "[btree][erase]",
                   BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  using BTree = btree<int, int, 64, 64, std::less<int>, Mode>;

  SECTION("Erase from empty tree") {
    BTree tree;
    size_t removed = tree.erase(5);
    REQUIRE(removed == 0);
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.empty());
  }

  SECTION("Erase non-existing element") {
    BTree tree;
    populate_tree(tree, {{1, 10}, {3, 30}, {5, 50}});

    size_t removed = tree.erase(2);
    REQUIRE(removed == 0);
    REQUIRE(tree.size() == 3);

    // Verify all elements still exist
    REQUIRE(tree.find(1) != tree.end());
    REQUIRE(tree.find(3) != tree.end());
    REQUIRE(tree.find(5) != tree.end());
  }

  SECTION("Erase single element from single-leaf tree") {
    BTree tree;
    tree.insert(5, 50);

    size_t removed = tree.erase(5);
    REQUIRE(removed == 1);
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.empty());
    REQUIRE(tree.begin() == tree.end());
  }

  SECTION("Erase elements from single-leaf tree") {
    BTree tree;
    populate_tree(tree, {{1, 10}, {3, 30}, {5, 50}, {7, 70}});

    // Erase middle element
    size_t removed = tree.erase(3);
    REQUIRE(removed == 1);
    REQUIRE(tree.size() == 3);
    REQUIRE(tree.find(3) == tree.end());

    // Verify remaining elements
    REQUIRE(tree.find(1) != tree.end());
    REQUIRE(tree.find(5) != tree.end());
    REQUIRE(tree.find(7) != tree.end());

    // Erase first element
    removed = tree.erase(1);
    REQUIRE(removed == 1);
    REQUIRE(tree.size() == 2);
    REQUIRE(tree.find(1) == tree.end());

    // Erase last element
    removed = tree.erase(7);
    REQUIRE(removed == 1);
    REQUIRE(tree.size() == 1);
    REQUIRE(tree.find(7) == tree.end());

    // Erase final element
    removed = tree.erase(5);
    REQUIRE(removed == 1);
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.empty());
  }

  SECTION("Erase all elements one by one") {
    BTree tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    for (int i = 1; i <= 10; ++i) {
      size_t removed = tree.erase(i);
      REQUIRE(removed == 1);
      REQUIRE(tree.size() == static_cast<size_t>(10 - i));
      REQUIRE(tree.find(i) == tree.end());
    }

    REQUIRE(tree.empty());
    REQUIRE(tree.begin() == tree.end());
  }

  SECTION("Erase elements in descending order") {
    BTree tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    for (int i = 10; i >= 1; --i) {
      size_t removed = tree.erase(i);
      REQUIRE(removed == 1);
      REQUIRE(tree.size() == static_cast<size_t>(i - 1));
      REQUIRE(tree.find(i) == tree.end());
    }

    REQUIRE(tree.empty());
  }

  SECTION("Erase duplicate key returns 0 after first erase") {
    BTree tree;
    tree.insert(5, 50);

    size_t removed1 = tree.erase(5);
    REQUIRE(removed1 == 1);

    size_t removed2 = tree.erase(5);
    REQUIRE(removed2 == 0);
  }

  SECTION("Insert and erase interleaved") {
    BTree tree;

    tree.insert(1, 10);
    tree.insert(2, 20);
    tree.insert(3, 30);

    tree.erase(2);
    REQUIRE(tree.size() == 2);

    tree.insert(4, 40);
    REQUIRE(tree.size() == 3);

    tree.erase(1);
    tree.erase(3);
    REQUIRE(tree.size() == 1);

    auto it = tree.find(4);
    REQUIRE(it != tree.end());
    REQUIRE(it->second == 40);
  }
}

TEMPLATE_TEST_CASE("btree erase operations - underflow handling",
                   "[btree][erase][underflow]", BinarySearchMode,
                   LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Erase causes borrow from left sibling") {
    // Small node size to test underflow
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert elements to create multiple leaves (need 18+ for 3 leaves)
    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    // Remove elements from rightmost leaf to cause underflow and borrowing
    tree.erase(20);
    tree.erase(19);
    tree.erase(18);
    tree.erase(17);
    tree.erase(16);
    tree.erase(15);

    // Verify tree integrity
    REQUIRE(tree.size() == 14);
    for (int i = 1; i <= 14; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("Erase causes borrow from right sibling") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert elements to create multiple leaves
    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    // Remove from left side to cause underflow and borrowing from right
    tree.erase(1);
    tree.erase(2);
    tree.erase(3);
    tree.erase(4);
    tree.erase(5);
    tree.erase(6);

    REQUIRE(tree.size() == 14);
    for (int i = 7; i <= 20; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("Erase causes merge with left sibling") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert to create structure with multiple leaves
    for (int i = 1; i <= 30; ++i) {
      tree.insert(i, i * 10);
    }

    // Remove many elements from right half to force merging
    for (int i = 16; i <= 30; ++i) {
      tree.erase(i);
    }

    REQUIRE(tree.size() == 15);
    for (int i = 1; i <= 15; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }

    // Verify iteration still works
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys.size() == 15);
    for (int i = 0; i < 15; ++i) {
      REQUIRE(keys[i] == i + 1);
    }
  }

  SECTION("Erase causes merge with right sibling") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    for (int i = 1; i <= 30; ++i) {
      tree.insert(i, i * 10);
    }

    // Remove from left to force merge with right
    for (int i = 1; i <= 15; ++i) {
      tree.erase(i);
    }

    REQUIRE(tree.size() == 15);
    for (int i = 16; i <= 30; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("Erase causes cascading merges up the tree") {
    // Small nodes to create deep tree
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert many elements to create multi-level tree
    // With leaf size 8, need 70+ for internal split (multiple levels)
    for (int i = 1; i <= 80; ++i) {
      tree.insert(i, i * 10);
    }

    // Verify all inserted
    REQUIRE(tree.size() == 80);

    // Remove most elements (causes cascading merges/rebalances)
    for (int i = 41; i <= 80; ++i) {
      tree.erase(i);
    }

    REQUIRE(tree.size() == 40);

    // Verify remaining elements
    for (int i = 1; i <= 40; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }

    // Verify iteration
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys.size() == 40);
    for (int i = 0; i < 40; ++i) {
      REQUIRE(keys[i] == i + 1);
    }
  }

  SECTION("Erase reduces tree height") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Build multi-level tree (need 70+ for internal split)
    for (int i = 1; i <= 70; ++i) {
      tree.insert(i, i * 10);
    }

    // Remove elements to collapse tree back to single level
    for (int i = 1; i <= 62; ++i) {
      tree.erase(i);
    }

    REQUIRE(tree.size() == 8);

    // Verify remaining elements
    for (int i = 63; i <= 70; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }
}

TEMPLATE_TEST_CASE("btree erase operations - edge cases", "[btree][erase]",
                   BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Erase minimum element updates leftmost") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    // Erase minimum
    tree.erase(1);

    // Verify iteration starts from new minimum
    auto it = tree.begin();
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 2);
  }

  SECTION("Erase maximum element updates rightmost") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    tree.erase(10);

    // Verify iteration
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys.back() == 9);
  }

  SECTION("Erase all but one element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    for (int i = 1; i <= 9; ++i) {
      tree.erase(i);
    }

    REQUIRE(tree.size() == 1);
    REQUIRE(!tree.empty());

    auto it = tree.find(10);
    REQUIRE(it != tree.end());
    REQUIRE(it->second == 100);

    // Verify iteration
    int count = 0;
    for (auto pair : tree) {
      REQUIRE(pair.first == 10);
      REQUIRE(pair.second == 100);
      count++;
    }
    REQUIRE(count == 1);
  }

  SECTION("Erase with string keys") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree;

    tree.insert("apple", 1);
    tree.insert("banana", 2);
    tree.insert("cherry", 3);
    tree.insert("date", 4);

    tree.erase("banana");

    REQUIRE(tree.size() == 3);
    REQUIRE(tree.find("banana") == tree.end());
    REQUIRE(tree.find("apple") != tree.end());
    REQUIRE(tree.find("cherry") != tree.end());
    REQUIRE(tree.find("date") != tree.end());
  }

  SECTION("Stress test - many inserts and erases") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert 100 elements
    for (int i = 1; i <= 100; ++i) {
      tree.insert(i, i * 10);
    }
    REQUIRE(tree.size() == 100);

    // Erase every other element
    for (int i = 2; i <= 100; i += 2) {
      tree.erase(i);
    }
    REQUIRE(tree.size() == 50);

    // Verify only odd elements remain
    for (int i = 1; i <= 100; ++i) {
      auto it = tree.find(i);
      if (i % 2 == 1) {
        REQUIRE(it != tree.end());
        REQUIRE(it->second == i * 10);
      } else {
        REQUIRE(it == tree.end());
      }
    }

    // Insert the even elements back
    for (int i = 2; i <= 100; i += 2) {
      tree.insert(i, i * 10);
    }
    REQUIRE(tree.size() == 100);

    // Verify all elements
    for (int i = 1; i <= 100; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }
}

// Phase 6: Iterator-based erase operations
TEMPLATE_TEST_CASE("btree iterator-based erase", "[btree][erase][iterator]",
                   BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("erase(iterator) - single element from middle") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.find(5);
    REQUIRE(it != tree.end());

    auto next = tree.erase(it);
    REQUIRE(tree.size() == 9);
    REQUIRE(next != tree.end());
    REQUIRE(next->first == 6);
    REQUIRE(next->second == 60);

    // Verify 5 is gone
    REQUIRE(tree.find(5) == tree.end());

    // Verify others still exist
    for (int i = 1; i <= 10; ++i) {
      if (i != 5) {
        auto found = tree.find(i);
        REQUIRE(found != tree.end());
        REQUIRE(found->second == i * 10);
      }
    }
  }

  SECTION("erase(iterator) - first element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.begin();
    REQUIRE(it->first == 1);

    auto next = tree.erase(it);
    REQUIRE(tree.size() == 9);
    REQUIRE(next == tree.begin());
    REQUIRE(next->first == 2);
  }

  SECTION("erase(iterator) - last element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.find(10);
    REQUIRE(it != tree.end());

    auto next = tree.erase(it);
    REQUIRE(tree.size() == 9);
    REQUIRE(next == tree.end());
  }

  SECTION("erase(iterator) - all elements one by one forward") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    while (!tree.empty()) {
      auto it = tree.begin();
      tree.erase(it);
    }

    REQUIRE(tree.size() == 0);
    REQUIRE(tree.begin() == tree.end());
  }

  SECTION("erase(iterator) - triggers underflow and borrow") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    // Erase middle elements to trigger underflow
    auto it = tree.find(5);
    tree.erase(it);
    REQUIRE(tree.size() == 9);

    // Verify tree is still valid
    int count = 0;
    for (auto pair : tree) {
      (void)pair;
      count++;
    }
    REQUIRE(count == 9);
  }

  SECTION("erase(iterator) - triggers underflow and merge") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    // Erase multiple elements to trigger merges
    for (int i = 1; i <= 10; ++i) {
      auto it = tree.find(i);
      if (it != tree.end()) {
        tree.erase(it);
      }
    }

    REQUIRE(tree.size() == 10);

    // Verify remaining elements
    for (int i = 11; i <= 20; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("erase(iterator, iterator) - range erase middle") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto first = tree.find(4);
    auto last = tree.find(8);
    REQUIRE(first != tree.end());
    REQUIRE(last != tree.end());

    auto next = tree.erase(first, last);
    REQUIRE(tree.size() == 6);  // Erased 4, 5, 6, 7
    REQUIRE(next == tree.find(8));

    // Verify erased elements are gone
    for (int i = 4; i <= 7; ++i) {
      REQUIRE(tree.find(i) == tree.end());
    }

    // Verify remaining elements
    for (int i : {1, 2, 3, 8, 9, 10}) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("erase(iterator, iterator) - range erase from begin") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto last = tree.find(6);
    auto next = tree.erase(tree.begin(), last);

    REQUIRE(tree.size() == 5);  // Erased 1-5
    REQUIRE(next == tree.begin());
    REQUIRE(next->first == 6);
  }

  SECTION("erase(iterator, iterator) - range erase to end") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto first = tree.find(6);
    auto next = tree.erase(first, tree.end());

    REQUIRE(tree.size() == 5);  // Erased 6-10
    REQUIRE(next == tree.end());

    // Verify remaining elements
    for (int i = 1; i <= 5; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("erase(iterator, iterator) - erase everything") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto next = tree.erase(tree.begin(), tree.end());
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.empty());
    REQUIRE(next == tree.end());
  }

  SECTION("erase(iterator, iterator) - empty range") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.find(5);
    auto next = tree.erase(it, it);  // Empty range

    REQUIRE(tree.size() == 10);  // Nothing erased
    REQUIRE(next == it);
  }

  SECTION("erase(iterator, iterator) - large range with underflow") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 50; ++i) {
      tree.insert(i, i * 10);
    }

    auto first = tree.find(10);
    auto last = tree.find(40);
    tree.erase(first, last);

    REQUIRE(tree.size() == 20);  // Erased 10-39

    // Verify tree integrity
    int count = 0;
    for (auto pair : tree) {
      (void)pair;
      count++;
    }
    REQUIRE(count == 20);

    // Verify specific elements
    for (int i = 1; i <= 9; ++i) {
      REQUIRE(tree.find(i) != tree.end());
    }
    for (int i = 40; i <= 50; ++i) {
      REQUIRE(tree.find(i) != tree.end());
    }
  }

  SECTION("erase(iterator) - with string keys") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree;
    tree.insert("apple", 1);
    tree.insert("banana", 2);
    tree.insert("cherry", 3);
    tree.insert("date", 4);
    tree.insert("elderberry", 5);

    auto it = tree.find("cherry");
    REQUIRE(it != tree.end());

    auto next = tree.erase(it);
    REQUIRE(tree.size() == 4);
    REQUIRE(next != tree.end());
    REQUIRE(next->first == "date");

    REQUIRE(tree.find("cherry") == tree.end());
  }

  SECTION("erase(iterator, iterator) - sequential operations") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert 1-20
    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    // Erase 5-10
    tree.erase(tree.find(5), tree.find(11));
    REQUIRE(tree.size() == 14);

    // Verify elements 5-10 are gone
    for (int i = 5; i <= 10; ++i) {
      REQUIRE(tree.find(i) == tree.end());
    }

    // Verify remaining elements
    for (int i = 1; i <= 4; ++i) {
      REQUIRE(tree.find(i) != tree.end());
    }
    for (int i = 11; i <= 20; ++i) {
      REQUIRE(tree.find(i) != tree.end());
    }
  }
}

TEMPLATE_TEST_CASE("btree lower_bound and upper_bound", "[btree][bounds]",
                   BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("lower_bound - empty tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    auto it = tree.lower_bound(5);
    REQUIRE(it == tree.end());
  }

  SECTION("upper_bound - empty tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    auto it = tree.upper_bound(5);
    REQUIRE(it == tree.end());
  }

  SECTION("lower_bound - exact match") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.lower_bound(10);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 10);
    REQUIRE(it->second == 100);
  }

  SECTION("upper_bound - exact match") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.upper_bound(10);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 11);
    REQUIRE(it->second == 110);
  }

  SECTION("lower_bound - key between elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    // Insert: 1, 3, 5, 7, 9
    for (int i = 1; i <= 9; i += 2) {
      tree.insert(i, i * 10);
    }

    // lower_bound(4) should return iterator to 5
    auto it = tree.lower_bound(4);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 5);
    REQUIRE(it->second == 50);
  }

  SECTION("upper_bound - key between elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    // Insert: 1, 3, 5, 7, 9
    for (int i = 1; i <= 9; i += 2) {
      tree.insert(i, i * 10);
    }

    // upper_bound(4) should return iterator to 5
    auto it = tree.upper_bound(4);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 5);
    REQUIRE(it->second == 50);
  }

  SECTION("lower_bound - key at beginning") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.lower_bound(1);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 1);
    REQUIRE(it->second == 10);
  }

  SECTION("upper_bound - key at beginning") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.upper_bound(1);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 2);
    REQUIRE(it->second == 20);
  }

  SECTION("lower_bound - key at end") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.lower_bound(10);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 10);
    REQUIRE(it->second == 100);
  }

  SECTION("upper_bound - key at end") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.upper_bound(10);
    REQUIRE(it == tree.end());
  }

  SECTION("lower_bound - key before all elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 10; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.lower_bound(5);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 10);
    REQUIRE(it->second == 100);
  }

  SECTION("upper_bound - key before all elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 10; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.upper_bound(5);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 10);
    REQUIRE(it->second == 100);
  }

  SECTION("lower_bound - key after all elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.lower_bound(20);
    REQUIRE(it == tree.end());
  }

  SECTION("upper_bound - key after all elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.upper_bound(20);
    REQUIRE(it == tree.end());
  }

  SECTION("lower_bound and upper_bound - range query") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    // Find all elements in range [5, 15)
    auto first = tree.lower_bound(5);
    auto last = tree.upper_bound(14);

    REQUIRE(first != tree.end());
    REQUIRE(first->first == 5);

    // Count elements in range
    int count = 0;
    for (auto it = first; it != last; ++it) {
      count++;
      REQUIRE(it->first >= 5);
      REQUIRE(it->first <= 14);
    }
    REQUIRE(count == 10);  // 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
  }

  SECTION("lower_bound - across leaf boundaries") {
    // Create a tree that will have multiple leaves
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert enough elements to create multiple leaves (more than leaf size)
    for (int i = 1; i <= 50; ++i) {
      tree.insert(i, i * 10);
    }

    // Test lower_bound at various positions
    auto it25 = tree.lower_bound(25);
    REQUIRE(it25 != tree.end());
    REQUIRE(it25->first == 25);
    REQUIRE(it25->second == 250);

    auto it30 = tree.lower_bound(30);
    REQUIRE(it30 != tree.end());
    REQUIRE(it30->first == 30);
    REQUIRE(it30->second == 300);
  }

  SECTION("upper_bound - across leaf boundaries") {
    // Create a tree that will have multiple leaves
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert enough elements to create multiple leaves
    for (int i = 1; i <= 50; ++i) {
      tree.insert(i, i * 10);
    }

    // Test upper_bound at various positions
    auto it25 = tree.upper_bound(25);
    REQUIRE(it25 != tree.end());
    REQUIRE(it25->first == 26);
    REQUIRE(it25->second == 260);

    auto it30 = tree.upper_bound(30);
    REQUIRE(it30 != tree.end());
    REQUIRE(it30->first == 31);
    REQUIRE(it30->second == 310);
  }

  SECTION("lower_bound - with gaps in data") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert: 1, 5, 10, 15, 20, 25, 30
    tree.insert(1, 10);
    for (int i = 5; i <= 30; i += 5) {
      tree.insert(i, i * 10);
    }

    // Test lower_bound for values in gaps
    auto it3 = tree.lower_bound(3);
    REQUIRE(it3 != tree.end());
    REQUIRE(it3->first == 5);

    auto it12 = tree.lower_bound(12);
    REQUIRE(it12 != tree.end());
    REQUIRE(it12->first == 15);

    auto it27 = tree.lower_bound(27);
    REQUIRE(it27 != tree.end());
    REQUIRE(it27->first == 30);
  }

  SECTION("upper_bound - with gaps in data") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert: 1, 5, 10, 15, 20, 25, 30
    tree.insert(1, 10);
    for (int i = 5; i <= 30; i += 5) {
      tree.insert(i, i * 10);
    }

    // Test upper_bound for values in gaps
    auto it3 = tree.upper_bound(3);
    REQUIRE(it3 != tree.end());
    REQUIRE(it3->first == 5);

    auto it12 = tree.upper_bound(12);
    REQUIRE(it12 != tree.end());
    REQUIRE(it12->first == 15);

    auto it27 = tree.upper_bound(27);
    REQUIRE(it27 != tree.end());
    REQUIRE(it27->first == 30);
  }

  SECTION("lower_bound and upper_bound - string keys") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree;

    tree.insert("apple", 1);
    tree.insert("banana", 2);
    tree.insert("cherry", 3);
    tree.insert("date", 4);
    tree.insert("elderberry", 5);

    // lower_bound("cherry") -> "cherry"
    auto lb_cherry = tree.lower_bound("cherry");
    REQUIRE(lb_cherry != tree.end());
    REQUIRE(lb_cherry->first == "cherry");
    REQUIRE(lb_cherry->second == 3);

    // upper_bound("cherry") -> "date"
    auto ub_cherry = tree.upper_bound("cherry");
    REQUIRE(ub_cherry != tree.end());
    REQUIRE(ub_cherry->first == "date");
    REQUIRE(ub_cherry->second == 4);

    // lower_bound("cake") -> "cherry"
    auto lb_cake = tree.lower_bound("cake");
    REQUIRE(lb_cake != tree.end());
    REQUIRE(lb_cake->first == "cherry");

    // upper_bound("cake") -> "cherry"
    auto ub_cake = tree.upper_bound("cake");
    REQUIRE(ub_cake != tree.end());
    REQUIRE(ub_cake->first == "cherry");

    // lower_bound("zebra") -> end()
    auto lb_zebra = tree.lower_bound("zebra");
    REQUIRE(lb_zebra == tree.end());

    // upper_bound("zebra") -> end()
    auto ub_zebra = tree.upper_bound("zebra");
    REQUIRE(ub_zebra == tree.end());
  }

  SECTION("lower_bound and upper_bound - single element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);

    // lower_bound(5) -> 5
    auto lb_eq = tree.lower_bound(5);
    REQUIRE(lb_eq != tree.end());
    REQUIRE(lb_eq->first == 5);

    // upper_bound(5) -> end()
    auto ub_eq = tree.upper_bound(5);
    REQUIRE(ub_eq == tree.end());

    // lower_bound(3) -> 5
    auto lb_less = tree.lower_bound(3);
    REQUIRE(lb_less != tree.end());
    REQUIRE(lb_less->first == 5);

    // upper_bound(3) -> 5
    auto ub_less = tree.upper_bound(3);
    REQUIRE(ub_less != tree.end());
    REQUIRE(ub_less->first == 5);

    // lower_bound(7) -> end()
    auto lb_greater = tree.lower_bound(7);
    REQUIRE(lb_greater == tree.end());

    // upper_bound(7) -> end()
    auto ub_greater = tree.upper_bound(7);
    REQUIRE(ub_greater == tree.end());
  }

  SECTION("lower_bound and upper_bound - duplicates not allowed") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert 1-10
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    // Try to insert duplicate (should not be inserted)
    auto result = tree.insert(5, 999);
    REQUIRE(result.second == false);  // Not inserted
    REQUIRE(tree.size() == 10);

    // lower_bound and upper_bound should work correctly
    auto lb = tree.lower_bound(5);
    auto ub = tree.upper_bound(5);

    REQUIRE(lb != tree.end());
    REQUIRE(lb->first == 5);
    REQUIRE(lb->second == 50);  // Original value, not 999

    REQUIRE(ub != tree.end());
    REQUIRE(ub->first == 6);
  }

  SECTION("lower_bound and upper_bound - large tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert 1000 elements
    for (int i = 1; i <= 1000; ++i) {
      tree.insert(i, i * 10);
    }

    // Test lower_bound at various positions
    auto lb_100 = tree.lower_bound(100);
    REQUIRE(lb_100 != tree.end());
    REQUIRE(lb_100->first == 100);

    auto lb_500 = tree.lower_bound(500);
    REQUIRE(lb_500 != tree.end());
    REQUIRE(lb_500->first == 500);

    auto lb_999 = tree.lower_bound(999);
    REQUIRE(lb_999 != tree.end());
    REQUIRE(lb_999->first == 999);

    // Test upper_bound at various positions
    auto ub_100 = tree.upper_bound(100);
    REQUIRE(ub_100 != tree.end());
    REQUIRE(ub_100->first == 101);

    auto ub_500 = tree.upper_bound(500);
    REQUIRE(ub_500 != tree.end());
    REQUIRE(ub_500->first == 501);

    auto ub_999 = tree.upper_bound(999);
    REQUIRE(ub_999 != tree.end());
    REQUIRE(ub_999->first == 1000);

    auto ub_1000 = tree.upper_bound(1000);
    REQUIRE(ub_1000 == tree.end());
  }
}

TEMPLATE_TEST_CASE("btree equal_range", "[btree][equal_range]",
                   BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("equal_range - empty tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    auto [first, last] = tree.equal_range(5);
    REQUIRE(first == tree.end());
    REQUIRE(last == tree.end());
  }

  SECTION("equal_range - exact match") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);
    tree.insert(10, 100);
    tree.insert(15, 150);

    auto [first, last] = tree.equal_range(10);
    REQUIRE(first != tree.end());
    REQUIRE(first->first == 10);
    REQUIRE(first->second == 100);

    // For unique keys, range should contain exactly one element
    REQUIRE(last != tree.end());
    REQUIRE(last->first == 15);  // Points to next element
  }

  SECTION("equal_range - key not found (between elements)") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);
    tree.insert(15, 150);

    auto [first, last] = tree.equal_range(10);
    // Both should point to the next greater element
    REQUIRE(first == last);
    REQUIRE(first != tree.end());
    REQUIRE(first->first == 15);
  }

  SECTION("equal_range - key at beginning") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);
    tree.insert(10, 100);
    tree.insert(15, 150);

    auto [first, last] = tree.equal_range(5);
    REQUIRE(first != tree.end());
    REQUIRE(first->first == 5);
    REQUIRE(last != tree.end());
    REQUIRE(last->first == 10);
  }

  SECTION("equal_range - key at end") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);
    tree.insert(10, 100);
    tree.insert(15, 150);

    auto [first, last] = tree.equal_range(15);
    REQUIRE(first != tree.end());
    REQUIRE(first->first == 15);
    REQUIRE(last == tree.end());  // No element after
  }

  SECTION("equal_range - key before all elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(10, 100);
    tree.insert(20, 200);

    auto [first, last] = tree.equal_range(5);
    REQUIRE(first == last);
    REQUIRE(first != tree.end());
    REQUIRE(first->first == 10);
  }

  SECTION("equal_range - key after all elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(10, 100);
    tree.insert(20, 200);

    auto [first, last] = tree.equal_range(30);
    REQUIRE(first == last);
    REQUIRE(first == tree.end());
    REQUIRE(last == tree.end());
  }

  SECTION("equal_range - single element tree (found)") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(10, 100);

    auto [first, last] = tree.equal_range(10);
    REQUIRE(first != tree.end());
    REQUIRE(first->first == 10);
    REQUIRE(last == tree.end());
  }

  SECTION("equal_range - single element tree (not found, before)") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(10, 100);

    auto [first, last] = tree.equal_range(5);
    REQUIRE(first == last);
    REQUIRE(first != tree.end());
    REQUIRE(first->first == 10);
  }

  SECTION("equal_range - single element tree (not found, after)") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(10, 100);

    auto [first, last] = tree.equal_range(15);
    REQUIRE(first == last);
    REQUIRE(first == tree.end());
  }

  SECTION("equal_range - across leaf boundaries") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert enough elements to span multiple leaves
    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    // Test key in middle of tree
    auto [first, last] = tree.equal_range(10);
    REQUIRE(first != tree.end());
    REQUIRE(first->first == 10);
    REQUIRE(first->second == 100);
    REQUIRE(last != tree.end());
    REQUIRE(last->first == 11);

    // Verify we can iterate from first to last
    int count = 0;
    for (auto it = first; it != last; ++it) {
      ++count;
    }
    REQUIRE(count == 1);  // Unique keys, so exactly one element
  }

  SECTION("equal_range - string keys") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree;
    tree.insert("apple", 1);
    tree.insert("banana", 2);
    tree.insert("cherry", 3);
    tree.insert("date", 4);

    // Found
    auto [first1, last1] = tree.equal_range("banana");
    REQUIRE(first1 != tree.end());
    REQUIRE(first1->first == "banana");
    REQUIRE(first1->second == 2);
    REQUIRE(last1 != tree.end());
    REQUIRE(last1->first == "cherry");

    // Not found (between)
    auto [first2, last2] = tree.equal_range("blueberry");
    REQUIRE(first2 == last2);
    REQUIRE(first2 != tree.end());
    REQUIRE(first2->first == "cherry");

    // Not found (before)
    auto [first3, last3] = tree.equal_range("aardvark");
    REQUIRE(first3 == last3);
    REQUIRE(first3 != tree.end());
    REQUIRE(first3->first == "apple");

    // Not found (after)
    auto [first4, last4] = tree.equal_range("zebra");
    REQUIRE(first4 == last4);
    REQUIRE(first4 == tree.end());
  }

  SECTION("equal_range - large tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert 1000 elements
    for (int i = 1; i <= 1000; ++i) {
      tree.insert(i, i * 10);
    }

    // Test various positions
    auto [first1, last1] = tree.equal_range(100);
    REQUIRE(first1 != tree.end());
    REQUIRE(first1->first == 100);
    REQUIRE(last1 != tree.end());
    REQUIRE(last1->first == 101);

    auto [first2, last2] = tree.equal_range(500);
    REQUIRE(first2 != tree.end());
    REQUIRE(first2->first == 500);
    REQUIRE(last2 != tree.end());
    REQUIRE(last2->first == 501);

    auto [first3, last3] = tree.equal_range(1000);
    REQUIRE(first3 != tree.end());
    REQUIRE(first3->first == 1000);
    REQUIRE(last3 == tree.end());

    // Not found (gaps)
    auto [first4, last4] = tree.equal_range(1500);
    REQUIRE(first4 == last4);
    REQUIRE(first4 == tree.end());
  }

  SECTION("equal_range - consistency with lower_bound and upper_bound") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    for (int i = 1; i <= 50; ++i) {
      tree.insert(i * 2, i * 20);  // Insert only even numbers
    }

    // Test several keys
    for (int key : {10, 25, 50, 75, 100, 150}) {
      auto [er_first, er_last] = tree.equal_range(key);
      auto lb = tree.lower_bound(key);
      auto ub = tree.upper_bound(key);

      // equal_range should return {lower_bound, upper_bound}
      REQUIRE(er_first == lb);
      REQUIRE(er_last == ub);
    }
  }

  SECTION("equal_range - range iteration") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    // Find element
    auto [first, last] = tree.equal_range(10);
    REQUIRE(first != tree.end());

    // Iterate over the range (should be exactly one element for unique keys)
    std::vector<int> keys;
    for (auto it = first; it != last; ++it) {
      keys.push_back(it->first);
    }

    REQUIRE(keys.size() == 1);
    REQUIRE(keys[0] == 10);
  }
}

TEMPLATE_TEST_CASE("btree clear", "[btree][clear]", BinarySearchMode,
                   LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("clear - empty tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);

    tree.clear();
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }

  SECTION("clear - single element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);
    REQUIRE(tree.size() == 1);

    tree.clear();
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.find(5) == tree.end());
  }

  SECTION("clear - multiple elements, single leaf") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 3; ++i) {
      tree.insert(i, i * 10);
    }
    REQUIRE(tree.size() == 3);

    tree.clear();
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);

    for (int i = 1; i <= 3; ++i) {
      REQUIRE(tree.find(i) == tree.end());
    }
  }

  SECTION("clear - multiple leaves") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    // Insert enough to create multiple leaves
    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }
    REQUIRE(tree.size() == 20);

    tree.clear();
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);

    for (int i = 1; i <= 20; ++i) {
      REQUIRE(tree.find(i) == tree.end());
    }
  }

  SECTION("clear - large tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    // Insert 1000 elements
    for (int i = 1; i <= 1000; ++i) {
      tree.insert(i, i * 10);
    }
    REQUIRE(tree.size() == 1000);

    tree.clear();
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }

  SECTION("clear - reuse after clear") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert elements
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }
    REQUIRE(tree.size() == 10);

    // Clear
    tree.clear();
    REQUIRE(tree.empty());

    // Reuse - insert new elements
    for (int i = 100; i <= 110; ++i) {
      tree.insert(i, i * 10);
    }
    REQUIRE(tree.size() == 11);

    // Verify old elements are gone
    for (int i = 1; i <= 10; ++i) {
      REQUIRE(tree.find(i) == tree.end());
    }

    // Verify new elements exist
    for (int i = 100; i <= 110; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }
}

TEMPLATE_TEST_CASE("btree count", "[btree][count]", BinarySearchMode,
                   LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("count - empty tree") {
    const btree<int, int, 8, 8, std::less<int>, Mode> tree;
    REQUIRE(tree.count(5) == 0);
    REQUIRE(tree.count(0) == 0);
    REQUIRE(tree.count(100) == 0);
  }

  SECTION("count - single element found") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);

    const auto& const_tree = tree;
    REQUIRE(const_tree.count(5) == 1);
  }

  SECTION("count - single element not found") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);

    const auto& const_tree = tree;
    REQUIRE(const_tree.count(3) == 0);
    REQUIRE(const_tree.count(7) == 0);
  }

  SECTION("count - multiple elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    const auto& const_tree = tree;

    // Elements that exist
    for (int i = 1; i <= 20; ++i) {
      REQUIRE(const_tree.count(i) == 1);
    }

    // Elements that don't exist
    REQUIRE(const_tree.count(0) == 0);
    REQUIRE(const_tree.count(21) == 0);
    REQUIRE(const_tree.count(100) == 0);
  }

  SECTION("count - after erase") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    const auto& const_tree = tree;
    REQUIRE(const_tree.count(5) == 1);

    tree.erase(5);
    REQUIRE(const_tree.count(5) == 0);
    REQUIRE(const_tree.count(4) == 1);
    REQUIRE(const_tree.count(6) == 1);
  }

  SECTION("count - with gaps in data") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    // Insert: 1, 5, 10, 15, 20, 25, 30
    tree.insert(1, 10);
    for (int i = 5; i <= 30; i += 5) {
      tree.insert(i, i * 10);
    }

    const auto& const_tree = tree;

    // Elements that exist
    REQUIRE(const_tree.count(1) == 1);
    REQUIRE(const_tree.count(5) == 1);
    REQUIRE(const_tree.count(10) == 1);
    REQUIRE(const_tree.count(30) == 1);

    // Elements in gaps
    REQUIRE(const_tree.count(2) == 0);
    REQUIRE(const_tree.count(3) == 0);
    REQUIRE(const_tree.count(7) == 0);
    REQUIRE(const_tree.count(12) == 0);
    REQUIRE(const_tree.count(27) == 0);
  }

  SECTION("count - string keys") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree;

    tree.insert("apple", 1);
    tree.insert("banana", 2);
    tree.insert("cherry", 3);

    const auto& const_tree = tree;

    REQUIRE(const_tree.count("apple") == 1);
    REQUIRE(const_tree.count("banana") == 1);
    REQUIRE(const_tree.count("cherry") == 1);
    REQUIRE(const_tree.count("date") == 0);
    REQUIRE(const_tree.count("zebra") == 0);
  }

  SECTION("count - large tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 1000; ++i) {
      tree.insert(i, i * 10);
    }

    const auto& const_tree = tree;

    // Check various elements
    REQUIRE(const_tree.count(1) == 1);
    REQUIRE(const_tree.count(500) == 1);
    REQUIRE(const_tree.count(1000) == 1);
    REQUIRE(const_tree.count(0) == 0);
    REQUIRE(const_tree.count(1001) == 0);
    REQUIRE(const_tree.count(5000) == 0);
  }

  SECTION("count - no duplicates allowed") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    tree.insert(5, 50);
    const auto& const_tree = tree;
    REQUIRE(const_tree.count(5) == 1);

    // Try to insert duplicate (should not be inserted)
    tree.insert(5, 999);
    REQUIRE(tree.size() == 1);
    REQUIRE(const_tree.count(5) == 1);  // Still 1, not 2
  }
}

TEMPLATE_TEST_CASE("btree key_comp and value_comp", "[btree][comparators]",
                   BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("key_comp - basic functionality") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    auto comp = tree.key_comp();

    // Test basic comparisons
    REQUIRE(comp(1, 2) == true);
    REQUIRE(comp(2, 1) == false);
    REQUIRE(comp(5, 5) == false);
    REQUIRE(comp(10, 20) == true);
  }

  SECTION("key_comp - string keys") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree;
    auto comp = tree.key_comp();

    REQUIRE(comp("apple", "banana") == true);
    REQUIRE(comp("banana", "apple") == false);
    REQUIRE(comp("hello", "hello") == false);
    REQUIRE(comp("zebra", "aardvark") == false);
  }

  SECTION("key_comp - ordering verification") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    auto comp = tree.key_comp();

    // Insert elements
    for (int i = 10; i >= 1; --i) {
      tree.insert(i, i * 10);
    }

    // Verify tree maintains sorted order using key_comp
    auto it = tree.begin();
    auto next = it;
    ++next;

    while (next != tree.end()) {
      // Current key should be less than next key
      REQUIRE(comp(it->first, next->first) == true);
      // Next key should not be less than current
      REQUIRE(comp(next->first, it->first) == false);

      ++it;
      ++next;
    }
  }

  SECTION("value_comp - basic functionality") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    auto comp = tree.value_comp();

    // value_comp compares pairs by their keys
    std::pair<int, int> p1{1, 100};
    std::pair<int, int> p2{2, 200};
    std::pair<int, int> p3{1, 999};  // Same key as p1, different value

    REQUIRE(comp(p1, p2) == true);
    REQUIRE(comp(p2, p1) == false);
    REQUIRE(comp(p1, p3) == false);  // Same key
    REQUIRE(comp(p3, p1) == false);  // Same key
  }

  SECTION("value_comp - string keys") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree;
    auto comp = tree.value_comp();

    std::pair<std::string, int> p1{"apple", 1};
    std::pair<std::string, int> p2{"banana", 2};
    std::pair<std::string, int> p3{"apple", 999};

    REQUIRE(comp(p1, p2) == true);
    REQUIRE(comp(p2, p1) == false);
    REQUIRE(comp(p1, p3) == false);  // Same key
  }

  SECTION("value_comp - ordering verification") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    auto comp = tree.value_comp();

    // Insert elements
    for (int i = 10; i >= 1; --i) {
      tree.insert(i, i * 10);
    }

    // Verify tree maintains sorted order using value_comp
    auto it = tree.begin();
    auto next = it;
    ++next;

    while (next != tree.end()) {
      // Current pair should be less than next pair (by key)
      std::pair<int, int> current{it->first, it->second};
      std::pair<int, int> next_pair{next->first, next->second};

      REQUIRE(comp(current, next_pair) == true);
      REQUIRE(comp(next_pair, current) == false);

      ++it;
      ++next;
    }
  }

  SECTION("key_comp and value_comp - consistency") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    auto key_cmp = tree.key_comp();
    auto val_cmp = tree.value_comp();

    // Insert some elements
    tree.insert(5, 50);
    tree.insert(10, 100);
    tree.insert(15, 150);

    // For any two elements, key_comp and value_comp should agree
    auto it1 = tree.find(5);
    auto it2 = tree.find(10);
    auto it3 = tree.find(15);

    std::pair<int, int> p1{it1->first, it1->second};
    std::pair<int, int> p2{it2->first, it2->second};
    std::pair<int, int> p3{it3->first, it3->second};

    // key_comp(k1, k2) == value_comp(p1, p2)
    REQUIRE(key_cmp(it1->first, it2->first) == val_cmp(p1, p2));
    REQUIRE(key_cmp(it2->first, it3->first) == val_cmp(p2, p3));
    REQUIRE(key_cmp(it1->first, it3->first) == val_cmp(p1, p3));

    // Reverse comparisons
    REQUIRE(key_cmp(it2->first, it1->first) == val_cmp(p2, p1));
    REQUIRE(key_cmp(it3->first, it2->first) == val_cmp(p3, p2));
    REQUIRE(key_cmp(it3->first, it1->first) == val_cmp(p3, p1));
  }

  SECTION("comparators - empty tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Should be able to get comparators even from empty tree
    auto key_cmp = tree.key_comp();
    auto val_cmp = tree.value_comp();

    REQUIRE(key_cmp(1, 2) == true);
    REQUIRE(val_cmp(std::pair{1, 10}, std::pair{2, 20}) == true);
  }

  SECTION("comparators - const tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);
    tree.insert(10, 100);

    const auto& const_tree = tree;

    // Should work with const tree
    auto key_cmp = const_tree.key_comp();
    auto val_cmp = const_tree.value_comp();

    REQUIRE(key_cmp(5, 10) == true);
    REQUIRE(val_cmp(std::pair{5, 50}, std::pair{10, 100}) == true);
  }

  SECTION("comparators - sorting with std algorithms") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    auto key_cmp = tree.key_comp();

    // Use key_comp with std::sort
    std::vector<int> keys{5, 2, 8, 1, 9, 3};
    std::sort(keys.begin(), keys.end(), key_cmp);

    REQUIRE(keys == std::vector<int>{1, 2, 3, 5, 8, 9});
  }

  SECTION("comparators - value pairs with std algorithms") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    auto val_cmp = tree.value_comp();

    // Use value_comp with std::sort
    std::vector<std::pair<int, int>> pairs{
        {5, 50}, {2, 20}, {8, 80}, {1, 10}, {9, 90}};
    std::sort(pairs.begin(), pairs.end(), val_cmp);

    REQUIRE(pairs[0].first == 1);
    REQUIRE(pairs[1].first == 2);
    REQUIRE(pairs[2].first == 5);
    REQUIRE(pairs[3].first == 8);
    REQUIRE(pairs[4].first == 9);
  }
}

TEMPLATE_TEST_CASE("btree get_allocator", "[btree][allocator]",
                   BinarySearchMode, LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("get_allocator - basic functionality") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    auto alloc = tree.get_allocator();

    // Verify we got an allocator
    // Should be std::allocator<std::pair<int, int>>
    static_assert(
        std::is_same_v<decltype(alloc), std::allocator<std::pair<int, int>>>);
  }

  SECTION("get_allocator - type alias") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Verify allocator_type alias matches get_allocator return type
    static_assert(std::is_same_v<typename decltype(tree)::allocator_type,
                                 std::allocator<std::pair<int, int>>>);

    auto alloc = tree.get_allocator();
    static_assert(std::is_same_v<decltype(alloc),
                                 typename decltype(tree)::allocator_type>);
  }

  SECTION("get_allocator - const tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);

    const auto& const_tree = tree;
    auto alloc = const_tree.get_allocator();

    // Should work with const tree
    static_assert(
        std::is_same_v<decltype(alloc), std::allocator<std::pair<int, int>>>);
  }

  SECTION("get_allocator - empty tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    auto alloc = tree.get_allocator();

    // Should work even on empty tree
    REQUIRE(tree.empty());
    static_assert(
        std::is_same_v<decltype(alloc), std::allocator<std::pair<int, int>>>);
  }

  SECTION("get_allocator - non-empty tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto alloc = tree.get_allocator();
    REQUIRE(tree.size() == 10);

    // Should work on non-empty tree
    static_assert(
        std::is_same_v<decltype(alloc), std::allocator<std::pair<int, int>>>);
  }

  SECTION("get_allocator - string key type") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree;
    auto alloc = tree.get_allocator();

    // Allocator should be for std::pair<std::string, int>
    static_assert(std::is_same_v<decltype(alloc),
                                 std::allocator<std::pair<std::string, int>>>);
  }

  SECTION("get_allocator - different value types") {
    btree<int, std::string, 8, 8, std::less<int>, Mode> tree;
    auto alloc = tree.get_allocator();

    // Allocator should be for std::pair<int, std::string>
    static_assert(std::is_same_v<decltype(alloc),
                                 std::allocator<std::pair<int, std::string>>>);
  }

  SECTION("get_allocator - allocator can allocate") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    auto alloc = tree.get_allocator();

    // Test that the allocator can actually allocate/deallocate
    auto ptr = alloc.allocate(1);
    REQUIRE(ptr != nullptr);

    // Construct a pair
    std::allocator_traits<decltype(alloc)>::construct(alloc, ptr, 5, 50);
    REQUIRE(ptr->first == 5);
    REQUIRE(ptr->second == 50);

    // Destroy and deallocate
    std::allocator_traits<decltype(alloc)>::destroy(alloc, ptr);
    alloc.deallocate(ptr, 1);
  }

  SECTION("get_allocator - multiple calls return equivalent allocators") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    auto alloc1 = tree.get_allocator();
    auto alloc2 = tree.get_allocator();

    // Standard allocators are stateless, so they should be equal
    REQUIRE(alloc1 == alloc2);
  }

  SECTION("get_allocator - allocator type properties") {
    using TreeType = btree<int, int, 8, 8, std::less<int>, Mode>;
    using AllocType = typename TreeType::allocator_type;

    // Verify allocator type properties
    static_assert(
        std::is_same_v<typename AllocType::value_type, std::pair<int, int>>);

    // std::allocator is copy constructible
    static_assert(std::is_copy_constructible_v<AllocType>);

    // std::allocator is move constructible
    static_assert(std::is_move_constructible_v<AllocType>);
  }
}

TEMPLATE_TEST_CASE("btree copy constructor", "[btree][copy]", BinarySearchMode,
                   LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("copy constructor - empty tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    btree<int, int, 8, 8, std::less<int>, Mode> tree2(tree1);

    REQUIRE(tree2.empty());
    REQUIRE(tree2.size() == 0);
  }

  SECTION("copy constructor - single element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    tree1.insert(5, 50);

    btree<int, int, 8, 8, std::less<int>, Mode> tree2(tree1);

    REQUIRE(tree2.size() == 1);
    auto it = tree2.find(5);
    REQUIRE(it != tree2.end());
    REQUIRE(it->second == 50);

    // Verify independence - modify tree1
    tree1.insert(10, 100);
    REQUIRE(tree1.size() == 2);
    REQUIRE(tree2.size() == 1);  // tree2 unaffected
  }

  SECTION("copy constructor - multiple elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    for (int i = 1; i <= 20; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 8, 8, std::less<int>, Mode> tree2(tree1);

    REQUIRE(tree2.size() == 20);
    for (int i = 1; i <= 20; ++i) {
      auto it = tree2.find(i);
      REQUIRE(it != tree2.end());
      REQUIRE(it->second == i * 10);
    }

    // Verify independence
    tree1.erase(10);
    REQUIRE(tree1.size() == 19);
    REQUIRE(tree2.size() == 20);
    REQUIRE(tree2.find(10) != tree2.end());
  }

  SECTION("copy constructor - large tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    for (int i = 1; i <= 1000; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 8, 8, std::less<int>, Mode> tree2(tree1);

    REQUIRE(tree2.size() == 1000);
    for (int i = 1; i <= 1000; i += 100) {
      auto it = tree2.find(i);
      REQUIRE(it != tree2.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("copy constructor - string keys") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree1;
    tree1.insert("apple", 1);
    tree1.insert("banana", 2);
    tree1.insert("cherry", 3);

    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree2(tree1);

    REQUIRE(tree2.size() == 3);
    REQUIRE(tree2.find("apple")->second == 1);
    REQUIRE(tree2.find("banana")->second == 2);
    REQUIRE(tree2.find("cherry")->second == 3);
  }
}

TEMPLATE_TEST_CASE("btree copy assignment", "[btree][copy]", BinarySearchMode,
                   LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("copy assignment - empty to empty") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    btree<int, int, 8, 8, std::less<int>, Mode> tree2;

    tree2 = tree1;
    REQUIRE(tree2.empty());
  }

  SECTION("copy assignment - non-empty to empty") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 8, 8, std::less<int>, Mode> tree2;
    tree2 = tree1;

    REQUIRE(tree2.size() == 10);
    for (int i = 1; i <= 10; ++i) {
      REQUIRE(tree2.find(i) != tree2.end());
    }
  }

  SECTION("copy assignment - empty to non-empty") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;

    btree<int, int, 8, 8, std::less<int>, Mode> tree2;
    for (int i = 1; i <= 10; ++i) {
      tree2.insert(i, i * 10);
    }

    tree2 = tree1;
    REQUIRE(tree2.empty());
    REQUIRE(tree2.size() == 0);
  }

  SECTION("copy assignment - non-empty to non-empty") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 8, 8, std::less<int>, Mode> tree2;
    for (int i = 100; i <= 110; ++i) {
      tree2.insert(i, i * 10);
    }

    tree2 = tree1;

    REQUIRE(tree2.size() == 10);
    for (int i = 1; i <= 10; ++i) {
      REQUIRE(tree2.find(i) != tree2.end());
    }
    // Old elements should be gone
    REQUIRE(tree2.find(100) == tree2.end());
  }

  SECTION("copy assignment - self assignment") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    tree1 = tree1;  // Self assignment

    REQUIRE(tree1.size() == 10);
    for (int i = 1; i <= 10; ++i) {
      REQUIRE(tree1.find(i) != tree1.end());
    }
  }

  SECTION("copy assignment - independence") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 8, 8, std::less<int>, Mode> tree2;
    tree2 = tree1;

    // Modify tree1
    tree1.insert(100, 1000);
    tree1.erase(5);

    // tree2 should be unaffected
    REQUIRE(tree2.size() == 10);
    REQUIRE(tree2.find(5) != tree2.end());
    REQUIRE(tree2.find(100) == tree2.end());
  }
}

TEMPLATE_TEST_CASE("btree move constructor", "[btree][move]", BinarySearchMode,
                   LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("move constructor - empty tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    btree<int, int, 8, 8, std::less<int>, Mode> tree2(std::move(tree1));

    REQUIRE(tree2.empty());
    REQUIRE(tree1.empty());  // Moved-from tree is empty
  }

  SECTION("move constructor - single element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    tree1.insert(5, 50);

    btree<int, int, 8, 8, std::less<int>, Mode> tree2(std::move(tree1));

    REQUIRE(tree2.size() == 1);
    REQUIRE(tree2.find(5) != tree2.end());

    // tree1 is now empty
    REQUIRE(tree1.empty());
    REQUIRE(tree1.size() == 0);
  }

  SECTION("move constructor - multiple elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    for (int i = 1; i <= 20; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 8, 8, std::less<int>, Mode> tree2(std::move(tree1));

    REQUIRE(tree2.size() == 20);
    for (int i = 1; i <= 20; ++i) {
      REQUIRE(tree2.find(i) != tree2.end());
    }

    // tree1 is now empty
    REQUIRE(tree1.empty());
  }

  SECTION("move constructor - moved-from tree is reusable") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 8, 8, std::less<int>, Mode> tree2(std::move(tree1));

    // Reuse tree1
    tree1.insert(100, 1000);
    REQUIRE(tree1.size() == 1);
    REQUIRE(tree1.find(100) != tree1.end());

    // tree2 should still have the original data
    REQUIRE(tree2.size() == 10);
  }

  SECTION("move constructor - string keys") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree1;
    tree1.insert("apple", 1);
    tree1.insert("banana", 2);

    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree2(
        std::move(tree1));

    REQUIRE(tree2.size() == 2);
    REQUIRE(tree2.find("apple") != tree2.end());
    REQUIRE(tree1.empty());
  }
}

TEMPLATE_TEST_CASE("btree move assignment", "[btree][move]", BinarySearchMode,
                   LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("move assignment - empty to empty") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    btree<int, int, 8, 8, std::less<int>, Mode> tree2;

    tree2 = std::move(tree1);
    REQUIRE(tree2.empty());
    REQUIRE(tree1.empty());
  }

  SECTION("move assignment - non-empty to empty") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 8, 8, std::less<int>, Mode> tree2;
    tree2 = std::move(tree1);

    REQUIRE(tree2.size() == 10);
    REQUIRE(tree1.empty());
  }

  SECTION("move assignment - empty to non-empty") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;

    btree<int, int, 8, 8, std::less<int>, Mode> tree2;
    for (int i = 1; i <= 10; ++i) {
      tree2.insert(i, i * 10);
    }

    tree2 = std::move(tree1);
    REQUIRE(tree2.empty());
    REQUIRE(tree1.empty());
  }

  SECTION("move assignment - non-empty to non-empty") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 8, 8, std::less<int>, Mode> tree2;
    for (int i = 100; i <= 110; ++i) {
      tree2.insert(i, i * 10);
    }

    tree2 = std::move(tree1);

    REQUIRE(tree2.size() == 10);
    for (int i = 1; i <= 10; ++i) {
      REQUIRE(tree2.find(i) != tree2.end());
    }
    // Old elements should be gone
    REQUIRE(tree2.find(100) == tree2.end());

    // tree1 is empty
    REQUIRE(tree1.empty());
  }

  SECTION("move assignment - self assignment") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    tree1 = std::move(tree1);  // Self move assignment

    // Should still be valid
    REQUIRE(tree1.size() == 10);
    for (int i = 1; i <= 10; ++i) {
      REQUIRE(tree1.find(i) != tree1.end());
    }
  }

  SECTION("move assignment - moved-from tree is reusable") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 8, 8, std::less<int>, Mode> tree2;
    tree2 = std::move(tree1);

    // Reuse tree1
    tree1.insert(100, 1000);
    REQUIRE(tree1.size() == 1);
    REQUIRE(tree1.find(100) != tree1.end());

    // tree2 should have the original data
    REQUIRE(tree2.size() == 10);
  }
}

TEMPLATE_TEST_CASE("btree emplace", "[btree][emplace]", BinarySearchMode,
                   LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("emplace - insert new element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Emplace with separate key and value
    auto [it, inserted] = tree.emplace(5, 50);

    REQUIRE(inserted);
    REQUIRE(tree.size() == 1);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 5);
    REQUIRE(it->second == 50);
  }

  SECTION("emplace - existing element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);

    // Try to emplace duplicate
    auto [it, inserted] = tree.emplace(5, 999);

    REQUIRE(!inserted);
    REQUIRE(tree.size() == 1);
    REQUIRE(it->second == 50);  // Original value unchanged
  }

  SECTION("emplace - multiple elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    auto [it1, ins1] = tree.emplace(1, 10);
    auto [it2, ins2] = tree.emplace(2, 20);
    auto [it3, ins3] = tree.emplace(3, 30);

    REQUIRE(ins1);
    REQUIRE(ins2);
    REQUIRE(ins3);
    REQUIRE(tree.size() == 3);

    REQUIRE(tree.find(1)->second == 10);
    REQUIRE(tree.find(2)->second == 20);
    REQUIRE(tree.find(3)->second == 30);
  }

  SECTION("emplace - with std::pair") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Emplace using std::pair
    auto [it, inserted] = tree.emplace(std::make_pair(5, 50));

    REQUIRE(inserted);
    REQUIRE(tree.size() == 1);
    REQUIRE(it->first == 5);
    REQUIRE(it->second == 50);
  }

  SECTION("emplace - string keys") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree;

    auto [it1, ins1] = tree.emplace("apple", 1);
    auto [it2, ins2] = tree.emplace("banana", 2);
    auto [it3, ins3] = tree.emplace("cherry", 3);

    REQUIRE(ins1);
    REQUIRE(ins2);
    REQUIRE(ins3);
    REQUIRE(tree.size() == 3);

    REQUIRE(tree.find("apple")->second == 1);
    REQUIRE(tree.find("banana")->second == 2);
    REQUIRE(tree.find("cherry")->second == 3);
  }

  SECTION("emplace - large tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    for (int i = 1; i <= 100; ++i) {
      auto [it, inserted] = tree.emplace(i, i * 10);
      REQUIRE(inserted);
      REQUIRE(it->first == i);
      REQUIRE(it->second == i * 10);
    }

    REQUIRE(tree.size() == 100);

    // Verify all elements
    for (int i = 1; i <= 100; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("emplace - return value on duplicate") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    auto [it1, ins1] = tree.emplace(5, 50);
    REQUIRE(ins1);
    REQUIRE(it1->second == 50);

    auto [it2, ins2] = tree.emplace(5, 999);
    REQUIRE(!ins2);
    REQUIRE(it2->second == 50);  // Returns iterator to existing element
  }
}

TEMPLATE_TEST_CASE("btree emplace_hint", "[btree][emplace]", BinarySearchMode,
                   LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("emplace_hint - insert new element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Emplace with hint (hint is ignored for now)
    auto it = tree.emplace_hint(tree.end(), 5, 50);

    REQUIRE(tree.size() == 1);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 5);
    REQUIRE(it->second == 50);
  }

  SECTION("emplace_hint - existing element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);

    // Try to emplace duplicate with hint
    auto it = tree.emplace_hint(tree.begin(), 5, 999);

    REQUIRE(tree.size() == 1);
    REQUIRE(it->second == 50);  // Original value unchanged
  }

  SECTION("emplace_hint - multiple elements with hints") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    auto it1 = tree.emplace_hint(tree.end(), 1, 10);
    auto it2 = tree.emplace_hint(it1, 2, 20);
    auto it3 = tree.emplace_hint(it2, 3, 30);

    REQUIRE(tree.size() == 3);
    REQUIRE(tree.find(1)->second == 10);
    REQUIRE(tree.find(2)->second == 20);
    REQUIRE(tree.find(3)->second == 30);
  }

  SECTION("emplace_hint - with std::pair") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Emplace using std::pair with hint
    auto it = tree.emplace_hint(tree.end(), std::make_pair(5, 50));

    REQUIRE(tree.size() == 1);
    REQUIRE(it->first == 5);
    REQUIRE(it->second == 50);
  }

  SECTION("emplace_hint - string keys") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree;

    auto it1 = tree.emplace_hint(tree.end(), "apple", 1);
    auto it2 = tree.emplace_hint(tree.end(), "banana", 2);
    auto it3 = tree.emplace_hint(tree.end(), "cherry", 3);

    REQUIRE(tree.size() == 3);
    REQUIRE(tree.find("apple")->second == 1);
    REQUIRE(tree.find("banana")->second == 2);
    REQUIRE(tree.find("cherry")->second == 3);
  }

  SECTION("emplace_hint - large tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    auto hint = tree.end();
    for (int i = 1; i <= 100; ++i) {
      hint = tree.emplace_hint(hint, i, i * 10);
      REQUIRE(hint->first == i);
      REQUIRE(hint->second == i * 10);
    }

    REQUIRE(tree.size() == 100);

    // Verify all elements
    for (int i = 1; i <= 100; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("emplace_hint - return value on duplicate") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    auto it1 = tree.emplace_hint(tree.end(), 5, 50);
    REQUIRE(it1->second == 50);

    auto it2 = tree.emplace_hint(it1, 5, 999);
    REQUIRE(it2->second == 50);  // Returns iterator to existing element
  }

  SECTION("emplace_hint - wrong hint doesn't break correctness") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert some elements
    tree.insert(1, 10);
    tree.insert(5, 50);
    tree.insert(10, 100);

    // Emplace with "wrong" hint (doesn't matter since we ignore it)
    auto it = tree.emplace_hint(tree.begin(), 7, 70);

    REQUIRE(tree.size() == 4);
    REQUIRE(it->first == 7);
    REQUIRE(it->second == 70);
    REQUIRE(tree.find(7) != tree.end());
  }
}
TEMPLATE_TEST_CASE("btree operator[]", "[btree][access]", BinarySearchMode,
                   LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("operator[] - insert new element with default value") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Access non-existent key creates it with default value (0 for int)
    int& value = tree[5];
    REQUIRE(value == 0);
    REQUIRE(tree.size() == 1);

    // Verify the element exists
    auto it = tree.find(5);
    REQUIRE(it != tree.end());
    REQUIRE(it->second == 0);
  }

  SECTION("operator[] - access existing element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);

    // Access existing key returns its value
    int& value = tree[5];
    REQUIRE(value == 50);
    REQUIRE(tree.size() == 1);  // Size unchanged
  }

  SECTION("operator[] - modify value through reference") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    tree.insert(5, 50);

    // Modify value through operator[]
    tree[5] = 100;

    REQUIRE(tree[5] == 100);
    auto it = tree.find(5);
    REQUIRE(it != tree.end());
    REQUIRE(it->second == 100);
  }

  SECTION("operator[] - insert and modify in one operation") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert and immediately modify
    tree[10] = 999;

    REQUIRE(tree.size() == 1);
    REQUIRE(tree[10] == 999);
  }

  SECTION("operator[] - multiple inserts and accesses") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert multiple elements
    tree[1] = 10;
    tree[2] = 20;
    tree[3] = 30;

    REQUIRE(tree.size() == 3);
    REQUIRE(tree[1] == 10);
    REQUIRE(tree[2] == 20);
    REQUIRE(tree[3] == 30);

    // Modify existing
    tree[2] = 200;
    REQUIRE(tree[2] == 200);
    REQUIRE(tree.size() == 3);  // Size unchanged
  }

  SECTION("operator[] - mixed insert and access") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Pre-populate some elements
    for (int i = 1; i <= 10; i += 2) {
      tree.insert(i, i * 10);
    }
    REQUIRE(tree.size() == 5);  // 1, 3, 5, 7, 9

    // Access existing (odd numbers)
    REQUIRE(tree[1] == 10);
    REQUIRE(tree[5] == 50);
    REQUIRE(tree.size() == 5);

    // Insert new (even numbers)
    tree[2] = 20;
    tree[4] = 40;
    REQUIRE(tree.size() == 7);

    // Verify all elements
    for (int i = 1; i <= 5; ++i) {
      REQUIRE(tree[i] == i * 10);
    }
  }

  SECTION("operator[] - large tree") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert many elements
    for (int i = 1; i <= 100; ++i) {
      tree[i] = i * 10;
    }

    REQUIRE(tree.size() == 100);

    // Verify all elements
    for (int i = 1; i <= 100; ++i) {
      REQUIRE(tree[i] == i * 10);
    }

    // Modify some elements
    for (int i = 1; i <= 100; i += 10) {
      tree[i] = 999;
    }

    // Verify modifications
    for (int i = 1; i <= 100; i += 10) {
      REQUIRE(tree[i] == 999);
    }

    REQUIRE(tree.size() == 100);  // Size unchanged
  }

  SECTION("operator[] - string keys") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree;

    // Insert with operator[]
    tree["apple"] = 1;
    tree["banana"] = 2;
    tree["cherry"] = 3;

    REQUIRE(tree.size() == 3);
    REQUIRE(tree["apple"] == 1);
    REQUIRE(tree["banana"] == 2);
    REQUIRE(tree["cherry"] == 3);

    // Modify existing
    tree["banana"] = 20;
    REQUIRE(tree["banana"] == 20);
    REQUIRE(tree.size() == 3);

    // Insert new
    tree["date"] = 4;
    REQUIRE(tree.size() == 4);
    REQUIRE(tree["date"] == 4);
  }

  SECTION("operator[] - default value for custom types") {
    btree<int, std::string, 8, 8, std::less<int>, Mode> tree;

    // Access non-existent key creates it with default value (empty string)
    std::string& value = tree[5];
    REQUIRE(value.empty());
    REQUIRE(tree.size() == 1);

    // Modify the value
    tree[5] = "hello";
    REQUIRE(tree[5] == "hello");
  }

  SECTION("operator[] - increment pattern") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Common pattern: increment counter
    for (int i = 0; i < 10; ++i) {
      tree[5]++;  // Increment value at key 5
    }

    REQUIRE(tree.size() == 1);
    REQUIRE(tree[5] == 10);
  }

  SECTION("operator[] - overwrite pattern") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert initial value
    tree.insert(5, 50);
    REQUIRE(tree[5] == 50);

    // Overwrite with operator[]
    tree[5] = 100;
    REQUIRE(tree[5] == 100);
    REQUIRE(tree.size() == 1);
  }
}
TEMPLATE_TEST_CASE("btree swap", "[btree][swap]", BinarySearchMode,
                   LinearSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("swap - two empty trees") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    btree<int, int, 8, 8, std::less<int>, Mode> tree2;

    tree1.swap(tree2);

    REQUIRE(tree1.empty());
    REQUIRE(tree2.empty());
  }

  SECTION("swap - empty with non-empty") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    btree<int, int, 8, 8, std::less<int>, Mode> tree2;

    for (int i = 1; i <= 10; ++i) {
      tree2.insert(i, i * 10);
    }

    tree1.swap(tree2);

    REQUIRE(tree1.size() == 10);
    REQUIRE(tree2.empty());

    for (int i = 1; i <= 10; ++i) {
      REQUIRE(tree1.find(i) != tree1.end());
      REQUIRE(tree1.find(i)->second == i * 10);
    }
  }

  SECTION("swap - non-empty with empty") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    btree<int, int, 8, 8, std::less<int>, Mode> tree2;

    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    tree1.swap(tree2);

    REQUIRE(tree1.empty());
    REQUIRE(tree2.size() == 10);

    for (int i = 1; i <= 10; ++i) {
      REQUIRE(tree2.find(i) != tree2.end());
      REQUIRE(tree2.find(i)->second == i * 10);
    }
  }

  SECTION("swap - two non-empty trees") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    btree<int, int, 8, 8, std::less<int>, Mode> tree2;

    // Fill tree1 with 1-10
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    // Fill tree2 with 100-110
    for (int i = 100; i <= 110; ++i) {
      tree2.insert(i, i * 10);
    }

    tree1.swap(tree2);

    // tree1 should have tree2's old data
    REQUIRE(tree1.size() == 11);
    for (int i = 100; i <= 110; ++i) {
      REQUIRE(tree1.find(i) != tree1.end());
      REQUIRE(tree1.find(i)->second == i * 10);
    }
    REQUIRE(tree1.find(5) == tree1.end());

    // tree2 should have tree1's old data
    REQUIRE(tree2.size() == 10);
    for (int i = 1; i <= 10; ++i) {
      REQUIRE(tree2.find(i) != tree2.end());
      REQUIRE(tree2.find(i)->second == i * 10);
    }
    REQUIRE(tree2.find(100) == tree2.end());
  }

  SECTION("swap - self swap") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    tree.swap(tree);  // Self swap

    // Should be unchanged
    REQUIRE(tree.size() == 10);
    for (int i = 1; i <= 10; ++i) {
      REQUIRE(tree.find(i) != tree.end());
      REQUIRE(tree.find(i)->second == i * 10);
    }
  }

  SECTION("swap - large trees") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    btree<int, int, 8, 8, std::less<int>, Mode> tree2;

    // Fill tree1 with 1-1000
    for (int i = 1; i <= 1000; ++i) {
      tree1.insert(i, i * 10);
    }

    // Fill tree2 with 2000-3000
    for (int i = 2000; i <= 3000; ++i) {
      tree2.insert(i, i * 10);
    }

    tree1.swap(tree2);

    // Verify tree1 has tree2's old data
    REQUIRE(tree1.size() == 1001);
    REQUIRE(tree1.find(2500) != tree1.end());
    REQUIRE(tree1.find(2500)->second == 25000);
    REQUIRE(tree1.find(500) == tree1.end());

    // Verify tree2 has tree1's old data
    REQUIRE(tree2.size() == 1000);
    REQUIRE(tree2.find(500) != tree2.end());
    REQUIRE(tree2.find(500)->second == 5000);
    REQUIRE(tree2.find(2500) == tree2.end());
  }

  SECTION("swap - string keys") {
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree1;
    btree<std::string, int, 8, 8, std::less<std::string>, Mode> tree2;

    tree1.insert("apple", 1);
    tree1.insert("banana", 2);

    tree2.insert("cherry", 3);
    tree2.insert("date", 4);
    tree2.insert("elderberry", 5);

    tree1.swap(tree2);

    REQUIRE(tree1.size() == 3);
    REQUIRE(tree1.find("cherry") != tree1.end());
    REQUIRE(tree1.find("apple") == tree1.end());

    REQUIRE(tree2.size() == 2);
    REQUIRE(tree2.find("apple") != tree2.end());
    REQUIRE(tree2.find("cherry") == tree2.end());
  }

  SECTION("swap - iterators remain valid") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    btree<int, int, 8, 8, std::less<int>, Mode> tree2;

    tree1.insert(1, 10);
    tree1.insert(2, 20);

    tree2.insert(100, 1000);
    tree2.insert(200, 2000);

    auto it1 = tree1.find(1);
    auto it2 = tree2.find(100);

    tree1.swap(tree2);

    // Iterators are now invalidated after swap in our implementation
    // (we don't guarantee iterator validity across swap)
    // But the elements should have moved correctly

    REQUIRE(tree1.find(100) != tree1.end());
    REQUIRE(tree1.find(100)->second == 1000);
    REQUIRE(tree2.find(1) != tree2.end());
    REQUIRE(tree2.find(1)->second == 10);
  }

  SECTION("swap - followed by modifications") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree1;
    btree<int, int, 8, 8, std::less<int>, Mode> tree2;

    tree1.insert(1, 10);
    tree2.insert(100, 1000);

    tree1.swap(tree2);

    // Modify after swap
    tree1.insert(101, 1010);
    tree2.insert(2, 20);

    REQUIRE(tree1.size() == 2);
    REQUIRE(tree1.find(100) != tree1.end());
    REQUIRE(tree1.find(101) != tree1.end());

    REQUIRE(tree2.size() == 2);
    REQUIRE(tree2.find(1) != tree2.end());
    REQUIRE(tree2.find(2) != tree2.end());
  }
}
// SIMD-specific tests with int keys (SIMD-compatible types)
TEST_CASE("btree with SIMD search mode and int keys", "[btree][simd]") {
  constexpr SearchMode Mode = SearchMode::SIMD;

  SECTION("Basic insert and find with SIMD mode") {
    btree<int, int, 64, 64, std::less<int>, Mode> tree;

    // Insert some values
    for (int i = 0; i < 100; ++i) {
      tree.insert(i, i * 10);
    }

    REQUIRE(tree.size() == 100);

    // Find values
    for (int i = 0; i < 100; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("SIMD mode with int32_t keys") {
    btree<int32_t, std::string, 32, 32, std::less<int32_t>, Mode> tree;

    tree.insert(42, "answer");
    tree.insert(-100, "negative");
    tree.insert(0, "zero");

    REQUIRE(tree.size() == 3);
    REQUIRE(tree.find(42) != tree.end());
    REQUIRE(tree.find(42)->second == "answer");
  }

  SECTION("SIMD mode with int64_t keys") {
    btree<int64_t, int, 16, 16, std::less<int64_t>, Mode> tree;

    tree.insert(1000000000LL, 1);
    tree.insert(-1000000000LL, 2);
    tree.insert(0LL, 3);

    REQUIRE(tree.size() == 3);
    REQUIRE(tree.find(1000000000LL) != tree.end());
    REQUIRE(tree.find(1000000000LL)->second == 1);
  }

  SECTION("SIMD mode with uint32_t keys") {
    btree<uint32_t, int, 16, 16, std::less<uint32_t>, Mode> tree;

    tree.insert(100u, 1);
    tree.insert(200u, 2);
    tree.insert(0u, 3);

    REQUIRE(tree.size() == 3);
    REQUIRE(tree.find(100u) != tree.end());
  }

  SECTION("SIMD mode erase operations") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    for (int i = 0; i < 50; ++i) {
      tree.insert(i, i * 2);
    }

    REQUIRE(tree.size() == 50);

    // Erase some elements
    tree.erase(10);
    tree.erase(20);
    tree.erase(30);

    REQUIRE(tree.size() == 47);
    REQUIRE(tree.find(10) == tree.end());
    REQUIRE(tree.find(20) == tree.end());
    REQUIRE(tree.find(30) == tree.end());
  }

  SECTION("SIMD mode iteration") {
    btree<int, int, 16, 16, std::less<int>, Mode> tree;

    for (int i = 0; i < 20; ++i) {
      tree.insert(i, i * 3);
    }

    int count = 0;
    int prev_key = -1;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
      REQUIRE(it->first > prev_key);  // Check sorted order
      prev_key = it->first;
      count++;
    }

    REQUIRE(count == 20);
  }
}

TEST_CASE("btree with std::greater (descending order)", "[btree][comparator]") {
  SECTION("Binary search mode") {
    btree<int, std::string, 64, 64, std::greater<int>, SearchMode::Binary> tree;

    // Insert elements (they should be stored in descending order)
    tree.insert(5, "five");
    tree.insert(10, "ten");
    tree.insert(3, "three");
    tree.insert(7, "seven");
    tree.insert(1, "one");

    REQUIRE(tree.size() == 5);

    // Verify find works
    auto found = tree.find(7);
    REQUIRE(found != tree.end());
    REQUIRE(found->second == "seven");

    // Verify iteration is in descending order
    auto it = tree.begin();
    REQUIRE(it->first == 10);
    ++it;
    REQUIRE(it->first == 7);
    ++it;
    REQUIRE(it->first == 5);
    ++it;
    REQUIRE(it->first == 3);
    ++it;
    REQUIRE(it->first == 1);

    // Verify erase works
    tree.erase(7);
    REQUIRE(tree.size() == 4);
    REQUIRE(tree.find(7) == tree.end());

    // Verify order is maintained after erase
    it = tree.begin();
    REQUIRE(it->first == 10);
    ++it;
    REQUIRE(it->first == 5);
    ++it;
    REQUIRE(it->first == 3);
    ++it;
    REQUIRE(it->first == 1);
  }

  SECTION("Linear search mode") {
    btree<int, std::string, 64, 64, std::greater<int>, SearchMode::Linear> tree;

    tree.insert(15, "fifteen");
    tree.insert(25, "twenty-five");
    tree.insert(5, "five");
    tree.insert(20, "twenty");

    REQUIRE(tree.size() == 4);

    // Verify descending order
    auto it = tree.begin();
    REQUIRE(it->first == 25);
    ++it;
    REQUIRE(it->first == 20);
    ++it;
    REQUIRE(it->first == 15);
    ++it;
    REQUIRE(it->first == 5);

    // Verify find works
    REQUIRE(tree.find(20) != tree.end());
    REQUIRE(tree.find(20)->second == "twenty");
    REQUIRE(tree.find(100) == tree.end());
  }

  SECTION("Large tree with many elements") {
    btree<int, int, 16, 16, std::greater<int>, SearchMode::Binary> tree;

    // Insert 100 elements
    for (int i = 0; i < 100; ++i) {
      tree.insert(i, i * 2);
    }

    REQUIRE(tree.size() == 100);

    // Verify descending order by checking first and last elements
    auto it = tree.begin();
    REQUIRE(it->first == 99);  // Largest key first in descending order

    // Verify find works
    for (int i = 0; i < 100; ++i) {
      auto found = tree.find(i);
      REQUIRE(found != tree.end());
      REQUIRE(found->second == i * 2);
    }

    // Erase some elements
    for (int i = 40; i < 60; ++i) {
      tree.erase(i);
    }

    REQUIRE(tree.size() == 80);

    // Verify erased elements are gone
    for (int i = 40; i < 60; ++i) {
      REQUIRE(tree.find(i) == tree.end());
    }

    // Verify remaining elements are still there and in descending order
    int prev_key = 100;
    for (auto pair : tree) {
      REQUIRE(pair.first < prev_key);
      prev_key = pair.first;
    }
  }
}

TEMPLATE_TEST_CASE("btree cbegin/cend const iterators", "[btree][iterators]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  btree<int, std::string, 32, 32, std::less<int>, Mode> tree;

  SECTION("cbegin/cend on empty tree") {
    REQUIRE(tree.cbegin() == tree.cend());
    REQUIRE(tree.cbegin() == tree.end());
  }

  SECTION("cbegin/cend with elements") {
    tree.insert(5, "five");
    tree.insert(3, "three");
    tree.insert(7, "seven");
    tree.insert(1, "one");
    tree.insert(9, "nine");

    REQUIRE(tree.size() == 5);

    // Verify cbegin points to smallest element
    auto it = tree.cbegin();
    REQUIRE(it != tree.cend());
    REQUIRE(it->first == 1);
    REQUIRE(it->second == "one");

    // Iterate through all elements using cbegin/cend
    std::vector<int> keys;
    for (auto cit = tree.cbegin(); cit != tree.cend(); ++cit) {
      keys.push_back(cit->first);
    }

    REQUIRE(keys == std::vector<int>{1, 3, 5, 7, 9});
  }

  SECTION("cbegin/cend compatibility with begin/end") {
    tree.insert(10, "ten");
    tree.insert(20, "twenty");

    // cbegin should match begin for const access
    REQUIRE(tree.cbegin()->first == tree.begin()->first);

    // Verify iteration produces same results
    std::vector<int> keys_regular;
    std::vector<int> keys_const;

    for (auto it = tree.begin(); it != tree.end(); ++it) {
      keys_regular.push_back(it->first);
    }

    for (auto cit = tree.cbegin(); cit != tree.cend(); ++cit) {
      keys_const.push_back(cit->first);
    }

    REQUIRE(keys_regular == keys_const);
  }
}

TEMPLATE_TEST_CASE("btree reverse iterators", "[btree][iterators][reverse]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  btree<int, std::string, 32, 32, std::less<int>, Mode> tree;

  SECTION("rbegin/rend on empty tree") {
    REQUIRE(tree.rbegin() == tree.rend());
  }

  SECTION("rbegin/rend with single element") {
    tree.insert(42, "forty-two");

    auto rit = tree.rbegin();
    REQUIRE(rit != tree.rend());
    REQUIRE(rit->first == 42);
    REQUIRE(rit->second == "forty-two");

    ++rit;
    REQUIRE(rit == tree.rend());
  }

  SECTION("rbegin/rend iterate in reverse order") {
    tree.insert(5, "five");
    tree.insert(3, "three");
    tree.insert(7, "seven");
    tree.insert(1, "one");
    tree.insert(9, "nine");

    REQUIRE(tree.size() == 5);

    // Verify rbegin points to largest element
    auto rit = tree.rbegin();
    REQUIRE(rit != tree.rend());
    REQUIRE(rit->first == 9);
    REQUIRE(rit->second == "nine");

    // Collect all keys in reverse order
    std::vector<int> keys;
    for (auto it = tree.rbegin(); it != tree.rend(); ++it) {
      keys.push_back(it->first);
    }

    REQUIRE(keys == std::vector<int>{9, 7, 5, 3, 1});
  }

  SECTION("rbegin/rend with many elements across multiple leaves") {
    // Insert enough elements to span multiple leaves
    for (int i = 0; i < 100; ++i) {
      tree.insert(i, "value" + std::to_string(i));
    }

    REQUIRE(tree.size() == 100);

    // Verify reverse iteration produces descending order
    std::vector<int> keys;
    for (auto rit = tree.rbegin(); rit != tree.rend(); ++rit) {
      keys.push_back(rit->first);
    }

    REQUIRE(keys.size() == 100);
    for (size_t i = 0; i < keys.size(); ++i) {
      REQUIRE(keys[i] == 99 - static_cast<int>(i));
    }
  }

  SECTION("crbegin/crend const reverse iterators") {
    tree.insert(10, "ten");
    tree.insert(20, "twenty");
    tree.insert(30, "thirty");

    // Use const reverse iterators
    std::vector<int> keys;
    for (auto crit = tree.crbegin(); crit != tree.crend(); ++crit) {
      keys.push_back(crit->first);
    }

    REQUIRE(keys == std::vector<int>{30, 20, 10});
  }

  SECTION("reverse iteration matches forward iteration reversed") {
    for (int i = 1; i <= 50; ++i) {
      tree.insert(i, std::to_string(i));
    }

    // Collect keys in forward order
    std::vector<int> forward_keys;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
      forward_keys.push_back(it->first);
    }

    // Collect keys in reverse order
    std::vector<int> reverse_keys;
    for (auto rit = tree.rbegin(); rit != tree.rend(); ++rit) {
      reverse_keys.push_back(rit->first);
    }

    // Reverse the reverse_keys and compare
    std::reverse(reverse_keys.begin(), reverse_keys.end());
    REQUIRE(forward_keys == reverse_keys);
  }
}

TEMPLATE_TEST_CASE("btree reverse iterators with std::greater",
                   "[btree][iterators][reverse][comparator]", BinarySearchMode,
                   LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  // Tree with descending order (std::greater)
  btree<int, std::string, 32, 32, std::greater<int>, Mode> tree;

  SECTION("rbegin/rend iterate from smallest to largest") {
    tree.insert(5, "five");
    tree.insert(3, "three");
    tree.insert(7, "seven");
    tree.insert(1, "one");
    tree.insert(9, "nine");

    // Forward iteration: 9, 7, 5, 3, 1 (descending)
    std::vector<int> forward_keys;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
      forward_keys.push_back(it->first);
    }
    REQUIRE(forward_keys == std::vector<int>{9, 7, 5, 3, 1});

    // Reverse iteration: 1, 3, 5, 7, 9 (ascending - reverse of descending)
    std::vector<int> reverse_keys;
    for (auto rit = tree.rbegin(); rit != tree.rend(); ++rit) {
      reverse_keys.push_back(rit->first);
    }
    REQUIRE(reverse_keys == std::vector<int>{1, 3, 5, 7, 9});
  }
}

TEMPLATE_TEST_CASE("btree bidirectional forward iterator",
                   "[btree][iterators][bidirectional]", BinarySearchMode,
                   LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  btree<int, std::string, 8, 8, std::less<int>, Mode> tree;

  SECTION("bidirectional_iterator_tag is set") {
    using iterator = decltype(tree.begin());
    REQUIRE(std::is_same_v<typename iterator::iterator_category,
                           std::bidirectional_iterator_tag>);
  }

  SECTION("decrement from end() to last element") {
    tree.insert(1, "one");
    tree.insert(2, "two");
    tree.insert(3, "three");

    auto it = tree.end();
    --it;
    REQUIRE(it->first == 3);
    REQUIRE(it->second == "three");
  }

  SECTION("decrement through single leaf") {
    tree.insert(1, "one");
    tree.insert(2, "two");
    tree.insert(3, "three");

    auto it = tree.end();
    --it;
    REQUIRE(it->first == 3);
    --it;
    REQUIRE(it->first == 2);
    --it;
    REQUIRE(it->first == 1);
    REQUIRE(it == tree.begin());
  }

  SECTION("decrement through multiple leaves") {
    // Insert enough elements to span multiple leaves (leaf size is 4)
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, "value" + std::to_string(i));
    }

    // Start from end and decrement through all elements
    std::vector<int> keys;
    for (auto it = tree.end(); it != tree.begin();) {
      --it;
      keys.push_back(it->first);
    }

    // Should get keys in reverse: 10, 9, 8, ..., 2, 1
    std::vector<int> expected = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
    REQUIRE(keys == expected);
  }

  SECTION("mixed increment and decrement") {
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, "value" + std::to_string(i));
    }

    auto it = tree.begin();
    ++it;  // Now at 2
    ++it;  // Now at 3
    REQUIRE(it->first == 3);

    --it;  // Back to 2
    REQUIRE(it->first == 2);

    --it;  // Back to 1
    REQUIRE(it->first == 1);
    REQUIRE(it == tree.begin());

    ++it;  // Forward to 2
    REQUIRE(it->first == 2);
  }

  SECTION("postfix decrement operator") {
    tree.insert(1, "one");
    tree.insert(2, "two");
    tree.insert(3, "three");

    auto it = tree.end();
    auto prev = it--;
    REQUIRE(prev == tree.end());
    REQUIRE(it->first == 3);

    prev = it--;
    REQUIRE(prev->first == 3);
    REQUIRE(it->first == 2);
  }

  SECTION("reverse traverse gives correct sequence") {
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, "value" + std::to_string(i));
    }

    // Reverse traversal using decrement
    std::vector<int> keys;
    for (auto it = tree.end(); it != tree.begin();) {
      --it;
      keys.push_back(it->first);
    }

    // Should get keys in reverse: 10, 9, 8, ..., 2, 1
    std::vector<int> expected = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
    REQUIRE(keys == expected);
  }
}

TEMPLATE_TEST_CASE("btree bidirectional reverse iterator",
                   "[btree][iterators][bidirectional][reverse]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  btree<int, std::string, 8, 8, std::less<int>, Mode> tree;

  SECTION("bidirectional_iterator_tag is set") {
    using reverse_iterator = decltype(tree.rbegin());
    REQUIRE(std::is_same_v<typename reverse_iterator::iterator_category,
                           std::bidirectional_iterator_tag>);
  }

  SECTION("decrement from rend() behaves correctly") {
    tree.insert(1, "one");
    tree.insert(2, "two");
    tree.insert(3, "three");

    // rbegin points to largest (3), rend is past smallest (1)
    // Decrementing rend() should give us smallest element (1)
    auto rit = tree.rbegin();
    ++rit;  // Now at 2
    ++rit;  // Now at 1
    ++rit;  // Now at rend()

    --rit;  // Back to 1
    REQUIRE(rit->first == 1);
  }

  SECTION("decrement through single leaf") {
    tree.insert(1, "one");
    tree.insert(2, "two");
    tree.insert(3, "three");

    auto rit = tree.rbegin();
    REQUIRE(rit->first == 3);

    ++rit;  // Forward in reverse order -> 2
    REQUIRE(rit->first == 2);

    --rit;  // Back to 3
    REQUIRE(rit->first == 3);
    REQUIRE(rit == tree.rbegin());
  }

  SECTION("decrement through multiple leaves") {
    // Insert enough elements to span multiple leaves
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, "value" + std::to_string(i));
    }

    // Go forward in reverse order (10 -> 1), then back using decrement
    auto rit = tree.rbegin();
    REQUIRE(rit->first == 10);

    for (int i = 0; i < 5; ++i) {
      ++rit;
    }
    REQUIRE(rit->first == 5);

    // Now decrement back
    for (int i = 0; i < 5; ++i) {
      --rit;
    }
    REQUIRE(rit->first == 10);
    REQUIRE(rit == tree.rbegin());
  }

  SECTION("mixed increment and decrement") {
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, "value" + std::to_string(i));
    }

    auto rit = tree.rbegin();  // At 10
    ++rit;                     // At 9
    ++rit;                     // At 8
    REQUIRE(rit->first == 8);

    --rit;  // Back to 9
    REQUIRE(rit->first == 9);

    --rit;  // Back to 10
    REQUIRE(rit->first == 10);
    REQUIRE(rit == tree.rbegin());

    ++rit;  // Forward to 9
    REQUIRE(rit->first == 9);
  }

  SECTION("postfix decrement operator") {
    tree.insert(1, "one");
    tree.insert(2, "two");
    tree.insert(3, "three");

    auto rit = tree.rbegin();  // At 3
    ++rit;                     // At 2

    auto prev = rit--;
    REQUIRE(prev->first == 2);
    REQUIRE(rit->first == 3);
    REQUIRE(rit == tree.rbegin());
  }
}

TEMPLATE_TEST_CASE("btree bidirectional iterator with std::greater",
                   "[btree][iterators][bidirectional][comparator]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  btree<int, std::string, 8, 8, std::greater<int>, Mode> tree;

  SECTION("forward iterator decrement with descending order") {
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, "value" + std::to_string(i));
    }

    // Tree order: 10, 9, 8, ..., 2, 1
    auto it = tree.end();
    --it;
    REQUIRE(it->first == 1);  // Last in descending order

    --it;
    REQUIRE(it->first == 2);

    --it;
    REQUIRE(it->first == 3);
  }

  SECTION("reverse iterator decrement with descending order") {
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, "value" + std::to_string(i));
    }

    // rbegin() points to smallest value (1)
    // Incrementing goes to larger values: 1 -> 2 -> 3...
    auto rit = tree.rbegin();
    REQUIRE(rit->first == 1);

    ++rit;
    REQUIRE(rit->first == 2);

    --rit;  // Back to 1
    REQUIRE(rit->first == 1);
    REQUIRE(rit == tree.rbegin());
  }

  SECTION("full bidirectional traversal with std::greater") {
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, "value" + std::to_string(i));
    }

    // Forward: 10, 9, 8, ..., 2, 1 (descending)
    std::vector<int> forward_keys;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
      forward_keys.push_back(it->first);
    }
    REQUIRE(forward_keys == std::vector<int>{10, 9, 8, 7, 6, 5, 4, 3, 2, 1});

    // Backward using decrement: 1, 2, 3, ..., 10
    std::vector<int> backward_keys;
    for (auto it = tree.end(); it != tree.begin();) {
      --it;
      backward_keys.push_back(it->first);
    }
    REQUIRE(backward_keys == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  }
}

TEMPLATE_TEST_CASE("btree contains() method", "[btree][contains]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  btree<int, std::string, 32, 32, std::less<int>, Mode> tree;

  SECTION("empty tree") {
    REQUIRE_FALSE(tree.contains(1));
    REQUIRE_FALSE(tree.contains(42));
  }

  SECTION("contains existing keys") {
    tree.insert(1, "one");
    tree.insert(3, "three");
    tree.insert(5, "five");

    REQUIRE(tree.contains(1));
    REQUIRE(tree.contains(3));
    REQUIRE(tree.contains(5));
  }

  SECTION("does not contain missing keys") {
    tree.insert(1, "one");
    tree.insert(3, "three");
    tree.insert(5, "five");

    REQUIRE_FALSE(tree.contains(0));
    REQUIRE_FALSE(tree.contains(2));
    REQUIRE_FALSE(tree.contains(4));
    REQUIRE_FALSE(tree.contains(6));
  }

  SECTION("after erase") {
    tree.insert(1, "one");
    tree.insert(2, "two");
    tree.insert(3, "three");

    REQUIRE(tree.contains(2));
    tree.erase(2);
    REQUIRE_FALSE(tree.contains(2));
    REQUIRE(tree.contains(1));
    REQUIRE(tree.contains(3));
  }

  SECTION("large tree") {
    for (int i = 0; i < 1000; i += 2) {
      tree.insert(i, "value");
    }

    for (int i = 0; i < 1000; i += 2) {
      REQUIRE(tree.contains(i));
      REQUIRE_FALSE(tree.contains(i + 1));
    }
  }
}

TEMPLATE_TEST_CASE("btree at() method", "[btree][at]", BinarySearchMode,
                   LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  btree<int, std::string, 32, 32, std::less<int>, Mode> tree;

  SECTION("throws on empty tree") {
    REQUIRE_THROWS_AS(tree.at(1), std::out_of_range);
    REQUIRE_THROWS_AS(tree.at(42), std::out_of_range);
  }

  SECTION("returns value for existing key") {
    tree.insert(1, "one");
    tree.insert(3, "three");
    tree.insert(5, "five");

    REQUIRE(tree.at(1) == "one");
    REQUIRE(tree.at(3) == "three");
    REQUIRE(tree.at(5) == "five");
  }

  SECTION("throws for missing key") {
    tree.insert(1, "one");
    tree.insert(3, "three");
    tree.insert(5, "five");

    REQUIRE_THROWS_AS(tree.at(0), std::out_of_range);
    REQUIRE_THROWS_AS(tree.at(2), std::out_of_range);
    REQUIRE_THROWS_AS(tree.at(4), std::out_of_range);
    REQUIRE_THROWS_AS(tree.at(6), std::out_of_range);
  }

  SECTION("modifiable through non-const at()") {
    tree.insert(1, "one");
    tree.at(1) = "ONE";
    REQUIRE(tree.at(1) == "ONE");
  }

  SECTION("const at() works") {
    tree.insert(1, "one");
    tree.insert(2, "two");

    const auto& const_tree = tree;
    REQUIRE(const_tree.at(1) == "one");
    REQUIRE(const_tree.at(2) == "two");
    REQUIRE_THROWS_AS(const_tree.at(3), std::out_of_range);
  }

  SECTION("exception message contains context") {
    try {
      tree.at(42);
      FAIL("Expected std::out_of_range to be thrown");
    } catch (const std::out_of_range& e) {
      std::string msg = e.what();
      REQUIRE(msg.find("btree::at") != std::string::npos);
    }
  }
}

TEMPLATE_TEST_CASE("btree initializer_list constructor", "[btree][constructor]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("empty initializer list") {
    btree<int, std::string, 32, 32, std::less<int>, Mode> tree(
        std::initializer_list<std::pair<int, std::string>>{});
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }

  SECTION("single element") {
    btree<int, std::string, 32, 32, std::less<int>, Mode> tree = {{1, "one"}};
    REQUIRE(tree.size() == 1);
    REQUIRE(tree.contains(1));
    REQUIRE(tree.at(1) == "one");
  }

  SECTION("multiple elements") {
    btree<int, std::string, 32, 32, std::less<int>, Mode> tree = {
        {1, "one"}, {2, "two"}, {3, "three"}, {5, "five"}, {7, "seven"}};

    REQUIRE(tree.size() == 5);
    REQUIRE(tree.at(1) == "one");
    REQUIRE(tree.at(2) == "two");
    REQUIRE(tree.at(3) == "three");
    REQUIRE(tree.at(5) == "five");
    REQUIRE(tree.at(7) == "seven");
  }

  SECTION("duplicate keys - last value wins") {
    btree<int, std::string, 32, 32, std::less<int>, Mode> tree = {
        {1, "first"}, {2, "two"}, {1, "second"}};

    REQUIRE(tree.size() == 2);
    // First {1, "first"} inserted, then {1, "second"} doesn't insert (key
    // exists)
    REQUIRE(tree.at(1) == "first");
    REQUIRE(tree.at(2) == "two");
  }

  SECTION("unordered elements get sorted") {
    btree<int, std::string, 32, 32, std::less<int>, Mode> tree = {
        {5, "five"}, {1, "one"}, {3, "three"}, {2, "two"}, {4, "four"}};

    std::vector<int> keys;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
      keys.push_back(it->first);
    }
    REQUIRE(keys == std::vector<int>{1, 2, 3, 4, 5});
  }

  SECTION("large initializer list") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;
    std::initializer_list<std::pair<int, int>> init;
    std::vector<std::pair<int, int>> data;
    for (int i = 0; i < 100; ++i) {
      data.push_back({i, i * 10});
    }
    tree =
        btree<int, int, 8, 8, std::less<int>, Mode>(data.begin(), data.end());

    REQUIRE(tree.size() == 100);
    for (int i = 0; i < 100; ++i) {
      REQUIRE(tree.at(i) == i * 10);
    }
  }
}

TEMPLATE_TEST_CASE("btree range constructor", "[btree][constructor]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("from empty vector") {
    std::vector<std::pair<int, std::string>> vec;
    btree<int, std::string, 32, 32, std::less<int>, Mode> tree(vec.begin(),
                                                               vec.end());
    REQUIRE(tree.empty());
  }

  SECTION("from vector") {
    std::vector<std::pair<int, std::string>> vec = {
        {1, "one"}, {2, "two"}, {3, "three"}};
    btree<int, std::string, 32, 32, std::less<int>, Mode> tree(vec.begin(),
                                                               vec.end());

    REQUIRE(tree.size() == 3);
    REQUIRE(tree.at(1) == "one");
    REQUIRE(tree.at(2) == "two");
    REQUIRE(tree.at(3) == "three");
  }

  SECTION("from std::map") {
    std::map<int, std::string> map = {
        {5, "five"}, {1, "one"}, {3, "three"}, {2, "two"}};
    btree<int, std::string, 32, 32, std::less<int>, Mode> tree(map.begin(),
                                                               map.end());

    REQUIRE(tree.size() == 4);
    REQUIRE(tree.at(1) == "one");
    REQUIRE(tree.at(2) == "two");
    REQUIRE(tree.at(3) == "three");
    REQUIRE(tree.at(5) == "five");
  }

  SECTION("from another btree") {
    btree<int, std::string, 32, 32, std::less<int>, Mode> source;
    source.insert(1, "one");
    source.insert(2, "two");
    source.insert(3, "three");

    btree<int, std::string, 32, 32, std::less<int>, Mode> tree(source.begin(),
                                                               source.end());

    REQUIRE(tree.size() == 3);
    REQUIRE(tree.at(1) == "one");
    REQUIRE(tree.at(2) == "two");
    REQUIRE(tree.at(3) == "three");
  }

  SECTION("partial range from vector") {
    std::vector<std::pair<int, std::string>> vec = {
        {1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}, {5, "five"}};

    btree<int, std::string, 32, 32, std::less<int>, Mode> tree(vec.begin() + 1,
                                                               vec.begin() + 4);

    REQUIRE(tree.size() == 3);
    REQUIRE(tree.at(2) == "two");
    REQUIRE(tree.at(3) == "three");
    REQUIRE(tree.at(4) == "four");
    REQUIRE_FALSE(tree.contains(1));
    REQUIRE_FALSE(tree.contains(5));
  }

  SECTION("large range") {
    std::vector<std::pair<int, int>> vec;
    for (int i = 0; i < 1000; ++i) {
      vec.push_back({i, i * 2});
    }

    btree<int, int, 8, 8, std::less<int>, Mode> tree(vec.begin(), vec.end());

    REQUIRE(tree.size() == 1000);
    for (int i = 0; i < 1000; ++i) {
      REQUIRE(tree.at(i) == i * 2);
    }
  }
}

// Helper class to track construction calls
struct ConstructionTracker {
  static inline int default_constructions = 0;
  static inline int value_constructions = 0;
  static inline int copy_constructions = 0;
  static inline int move_constructions = 0;

  int value;

  static void reset() {
    default_constructions = 0;
    value_constructions = 0;
    copy_constructions = 0;
    move_constructions = 0;
  }

  ConstructionTracker() : value(0) { ++default_constructions; }

  explicit ConstructionTracker(int v) : value(v) { ++value_constructions; }

  ConstructionTracker(const ConstructionTracker& other) : value(other.value) {
    ++copy_constructions;
  }

  ConstructionTracker(ConstructionTracker&& other) noexcept
      : value(other.value) {
    ++move_constructions;
  }

  ConstructionTracker& operator=(const ConstructionTracker&) = default;
  ConstructionTracker& operator=(ConstructionTracker&&) = default;
};

TEMPLATE_TEST_CASE("btree try_emplace", "[btree][try_emplace]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("try_emplace - insert new element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    auto [it, inserted] = tree.try_emplace(5, 50);

    REQUIRE(inserted);
    REQUIRE(tree.size() == 1);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 5);
    REQUIRE(it->second == 50);
  }

  SECTION("try_emplace - existing element does not construct value") {
    btree<int, ConstructionTracker, 8, 8, std::less<int>, Mode> tree;

    // Insert initial element
    ConstructionTracker::reset();
    auto [it1, ins1] = tree.try_emplace(5, 50);
    REQUIRE(ins1);
    REQUIRE(ConstructionTracker::value_constructions == 1);

    // Try to insert duplicate - should NOT construct new value
    ConstructionTracker::reset();
    auto [it2, ins2] = tree.try_emplace(5, 999);

    REQUIRE(!ins2);
    REQUIRE(tree.size() == 1);
    REQUIRE(it2->first == 5);
    REQUIRE(it2->second.value == 50);  // Original value unchanged
    REQUIRE(ConstructionTracker::value_constructions ==
            0);  // Key benefit: no construction!
  }

  SECTION("try_emplace - multiple arguments forwarding") {
    // Use a type that takes multiple constructor arguments
    struct MultiArg {
      int a;
      int b;
      std::string c;

      MultiArg() : a(0), b(0), c() {}
      MultiArg(int x, int y, std::string z) : a(x), b(y), c(z) {}
    };

    btree<int, MultiArg, 8, 8, std::less<int>, Mode> tree;

    auto [it, inserted] = tree.try_emplace(5, 10, 20, "test");

    REQUIRE(inserted);
    REQUIRE(tree.size() == 1);
    REQUIRE(it->first == 5);
    REQUIRE(it->second.a == 10);
    REQUIRE(it->second.b == 20);
    REQUIRE(it->second.c == "test");
  }

  SECTION("try_emplace - zero arguments (default construction)") {
    btree<int, ConstructionTracker, 8, 8, std::less<int>, Mode> tree;

    ConstructionTracker::reset();
    auto [it, inserted] = tree.try_emplace(5);

    REQUIRE(inserted);
    REQUIRE(tree.size() == 1);
    REQUIRE(ConstructionTracker::default_constructions == 1);
    REQUIRE(it->second.value == 0);
  }

  SECTION("try_emplace - multiple elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    auto [it1, ins1] = tree.try_emplace(1, 10);
    auto [it2, ins2] = tree.try_emplace(2, 20);
    auto [it3, ins3] = tree.try_emplace(3, 30);

    REQUIRE(ins1);
    REQUIRE(ins2);
    REQUIRE(ins3);
    REQUIRE(tree.size() == 3);

    REQUIRE(tree.find(1)->second == 10);
    REQUIRE(tree.find(2)->second == 20);
    REQUIRE(tree.find(3)->second == 30);
  }

  SECTION("try_emplace - string values with in-place construction") {
    btree<int, std::string, 8, 8, std::less<int>, Mode> tree;

    // Construct string in-place from const char* (avoiding temporary string)
    auto [it, inserted] = tree.try_emplace(5, "hello world");

    REQUIRE(inserted);
    REQUIRE(tree.size() == 1);
    REQUIRE(it->second == "hello world");
  }

  SECTION("try_emplace - large tree with splits") {
    btree<int, ConstructionTracker, 8, 8, std::less<int>, Mode> tree;

    // Insert enough elements to force multiple splits
    ConstructionTracker::reset();
    for (int i = 1; i <= 100; ++i) {
      auto [it, inserted] = tree.try_emplace(i, i * 10);
      REQUIRE(inserted);
      REQUIRE(it->first == i);
      REQUIRE(it->second.value == i * 10);
    }

    REQUIRE(tree.size() == 100);
    // Each successful try_emplace should construct exactly one value
    REQUIRE(ConstructionTracker::value_constructions == 100);

    // Verify all elements
    for (int i = 1; i <= 100; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second.value == i * 10);
    }

    // Try to insert duplicates - should not construct any values
    ConstructionTracker::reset();
    for (int i = 1; i <= 100; ++i) {
      auto [it, inserted] = tree.try_emplace(i, 999);
      REQUIRE(!inserted);
      REQUIRE(it->second.value == i * 10);  // Original value unchanged
    }
    REQUIRE(ConstructionTracker::value_constructions ==
            0);  // No constructions for duplicates
  }

  SECTION("try_emplace - comparison with emplace behavior") {
    // Demonstrate that emplace ALWAYS constructs, try_emplace does not

    // Test emplace - constructs even when key exists
    {
      btree<int, ConstructionTracker, 8, 8, std::less<int>, Mode> tree;
      tree.insert(5, ConstructionTracker(50));

      ConstructionTracker::reset();
      auto [it, inserted] = tree.emplace(5, 999);

      REQUIRE(!inserted);
      REQUIRE(it->second.value == 50);
      // emplace constructs the pair first, then discards it
      // (Note: current implementation might optimize this)
    }

    // Test try_emplace - does NOT construct when key exists
    {
      btree<int, ConstructionTracker, 8, 8, std::less<int>, Mode> tree;
      tree.insert(5, ConstructionTracker(50));

      ConstructionTracker::reset();
      auto [it, inserted] = tree.try_emplace(5, 999);

      REQUIRE(!inserted);
      REQUIRE(it->second.value == 50);
      REQUIRE(ConstructionTracker::value_constructions ==
              0);  // try_emplace avoids construction
    }
  }

  SECTION("try_emplace - return value on duplicate") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    auto [it1, ins1] = tree.try_emplace(5, 50);
    REQUIRE(ins1);
    REQUIRE(it1->second == 50);

    auto [it2, ins2] = tree.try_emplace(5, 999);
    REQUIRE(!ins2);
    REQUIRE(it2->first == 5);
    REQUIRE(it2->second == 50);  // Returns iterator to existing element
    REQUIRE(it1 == it2);         // Same element
  }

  SECTION("try_emplace - string keys") {
    // Note: SIMD mode doesn't support std::string keys, so use Binary mode
    btree<std::string, int, 8, 8, std::less<std::string>, SearchMode::Binary>
        tree;

    auto [it1, ins1] = tree.try_emplace("apple", 1);
    auto [it2, ins2] = tree.try_emplace("banana", 2);
    auto [it3, ins3] = tree.try_emplace("cherry", 3);

    REQUIRE(ins1);
    REQUIRE(ins2);
    REQUIRE(ins3);
    REQUIRE(tree.size() == 3);

    REQUIRE(tree.find("apple")->second == 1);
    REQUIRE(tree.find("banana")->second == 2);
    REQUIRE(tree.find("cherry")->second == 3);

    // Try duplicate
    auto [it4, ins4] = tree.try_emplace("apple", 999);
    REQUIRE(!ins4);
    REQUIRE(it4->second == 1);
  }

  SECTION("try_emplace - with complex value types") {
    struct ComplexValue {
      std::string name;
      std::vector<int> data;

      ComplexValue() : name(), data() {}
      ComplexValue(std::string n, std::vector<int> d)
          : name(std::move(n)), data(std::move(d)) {}
    };

    btree<int, ComplexValue, 8, 8, std::less<int>, Mode> tree;

    std::vector<int> vec{1, 2, 3, 4, 5};
    auto [it, inserted] = tree.try_emplace(5, "test", vec);

    REQUIRE(inserted);
    REQUIRE(it->second.name == "test");
    REQUIRE(it->second.data == vec);
  }
}

TEMPLATE_TEST_CASE("btree insert_or_assign", "[btree][insert_or_assign]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("insert_or_assign - insert new element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    auto [it, inserted] = tree.insert_or_assign(5, 50);

    REQUIRE(inserted);
    REQUIRE(tree.size() == 1);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 5);
    REQUIRE(it->second == 50);
  }

  SECTION("insert_or_assign - assign to existing element") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert initial element
    auto [it1, ins1] = tree.insert_or_assign(5, 50);
    REQUIRE(ins1);
    REQUIRE(it1->second == 50);

    // Assign new value to existing key
    auto [it2, ins2] = tree.insert_or_assign(5, 999);

    REQUIRE(!ins2);  // false = assignment, not insertion
    REQUIRE(tree.size() == 1);
    REQUIRE(it2->first == 5);
    REQUIRE(it2->second == 999);  // Value was updated!
    REQUIRE(it1 == it2);          // Same element
  }

  SECTION("insert_or_assign - multiple elements") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    auto [it1, ins1] = tree.insert_or_assign(1, 10);
    auto [it2, ins2] = tree.insert_or_assign(2, 20);
    auto [it3, ins3] = tree.insert_or_assign(3, 30);

    REQUIRE(ins1);
    REQUIRE(ins2);
    REQUIRE(ins3);
    REQUIRE(tree.size() == 3);

    REQUIRE(tree.find(1)->second == 10);
    REQUIRE(tree.find(2)->second == 20);
    REQUIRE(tree.find(3)->second == 30);
  }

  SECTION("insert_or_assign - update all values") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert initial values
    for (int i = 1; i <= 10; ++i) {
      auto [it, inserted] = tree.insert_or_assign(i, i * 10);
      REQUIRE(inserted);
    }
    REQUIRE(tree.size() == 10);

    // Update all values with insert_or_assign
    for (int i = 1; i <= 10; ++i) {
      auto [it, inserted] = tree.insert_or_assign(i, i * 100);
      REQUIRE(!inserted);  // Assignment, not insertion
      REQUIRE(it->second == i * 100);
    }
    REQUIRE(tree.size() == 10);  // Size unchanged

    // Verify all values were updated
    for (int i = 1; i <= 10; ++i) {
      REQUIRE(tree.find(i)->second == i * 100);
    }
  }

  SECTION("insert_or_assign - mixed insert and assign") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert some elements
    tree.insert_or_assign(2, 20);
    tree.insert_or_assign(4, 40);
    tree.insert_or_assign(6, 60);

    REQUIRE(tree.size() == 3);

    // Mix of inserts (odd numbers) and assigns (even numbers)
    auto [it1, ins1] = tree.insert_or_assign(1, 10);   // insert
    auto [it2, ins2] = tree.insert_or_assign(2, 200);  // assign
    auto [it3, ins3] = tree.insert_or_assign(3, 30);   // insert
    auto [it4, ins4] = tree.insert_or_assign(4, 400);  // assign
    auto [it5, ins5] = tree.insert_or_assign(5, 50);   // insert
    auto [it6, ins6] = tree.insert_or_assign(6, 600);  // assign

    REQUIRE(ins1);   // inserted
    REQUIRE(!ins2);  // assigned
    REQUIRE(ins3);   // inserted
    REQUIRE(!ins4);  // assigned
    REQUIRE(ins5);   // inserted
    REQUIRE(!ins6);  // assigned

    REQUIRE(tree.size() == 6);

    // Verify all values
    REQUIRE(tree.find(1)->second == 10);
    REQUIRE(tree.find(2)->second == 200);
    REQUIRE(tree.find(3)->second == 30);
    REQUIRE(tree.find(4)->second == 400);
    REQUIRE(tree.find(5)->second == 50);
    REQUIRE(tree.find(6)->second == 600);
  }

  SECTION("insert_or_assign - large tree with splits") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert enough elements to force multiple splits
    for (int i = 1; i <= 100; ++i) {
      auto [it, inserted] = tree.insert_or_assign(i, i * 10);
      REQUIRE(inserted);
      REQUIRE(it->first == i);
      REQUIRE(it->second == i * 10);
    }

    REQUIRE(tree.size() == 100);

    // Update all values using insert_or_assign
    for (int i = 1; i <= 100; ++i) {
      auto [it, inserted] = tree.insert_or_assign(i, i * 1000);
      REQUIRE(!inserted);  // All should be assignments
      REQUIRE(it->second == i * 1000);
    }

    REQUIRE(tree.size() == 100);  // Size unchanged

    // Verify all elements have updated values
    for (int i = 1; i <= 100; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 1000);
    }
  }

  SECTION("insert_or_assign - comparison with insert behavior") {
    // Demonstrate difference between insert() and insert_or_assign()

    // Test insert() - leaves existing values unchanged
    {
      btree<int, int, 8, 8, std::less<int>, Mode> tree;
      tree.insert(5, 50);

      auto [it, inserted] = tree.insert(5, 999);
      REQUIRE(!inserted);
      REQUIRE(it->second == 50);  // insert() leaves value unchanged
    }

    // Test insert_or_assign() - updates existing values
    {
      btree<int, int, 8, 8, std::less<int>, Mode> tree;
      tree.insert(5, 50);

      auto [it, inserted] = tree.insert_or_assign(5, 999);
      REQUIRE(!inserted);
      REQUIRE(it->second == 999);  // insert_or_assign() updates value
    }
  }

  SECTION("insert_or_assign - string values") {
    btree<int, std::string, 8, 8, std::less<int>, Mode> tree;

    // Insert initial value
    auto [it1, ins1] = tree.insert_or_assign(5, "hello");
    REQUIRE(ins1);
    REQUIRE(it1->second == "hello");

    // Assign new value
    auto [it2, ins2] = tree.insert_or_assign(5, "world");
    REQUIRE(!ins2);
    REQUIRE(it2->second == "world");

    // Verify assignment worked
    REQUIRE(tree.find(5)->second == "world");
    REQUIRE(tree.size() == 1);
  }

  SECTION("insert_or_assign - string keys") {
    // Note: SIMD mode doesn't support std::string keys, so use Binary mode
    btree<std::string, int, 8, 8, std::less<std::string>, SearchMode::Binary>
        tree;

    auto [it1, ins1] = tree.insert_or_assign("apple", 1);
    auto [it2, ins2] = tree.insert_or_assign("banana", 2);
    auto [it3, ins3] = tree.insert_or_assign("cherry", 3);

    REQUIRE(ins1);
    REQUIRE(ins2);
    REQUIRE(ins3);
    REQUIRE(tree.size() == 3);

    // Assign new values
    auto [it4, ins4] = tree.insert_or_assign("apple", 100);
    auto [it5, ins5] = tree.insert_or_assign("banana", 200);

    REQUIRE(!ins4);
    REQUIRE(!ins5);
    REQUIRE(tree.size() == 3);

    REQUIRE(tree.find("apple")->second == 100);
    REQUIRE(tree.find("banana")->second == 200);
    REQUIRE(tree.find("cherry")->second == 3);
  }

  SECTION("insert_or_assign - return value consistency") {
    btree<int, int, 8, 8, std::less<int>, Mode> tree;

    // Insert: should return (iterator, true)
    auto [it1, ins1] = tree.insert_or_assign(5, 50);
    REQUIRE(ins1 == true);
    REQUIRE(it1->first == 5);
    REQUIRE(it1->second == 50);

    // Assign: should return (iterator, false)
    auto [it2, ins2] = tree.insert_or_assign(5, 999);
    REQUIRE(ins2 == false);
    REQUIRE(it2->first == 5);
    REQUIRE(it2->second == 999);
    REQUIRE(it1 == it2);  // Same element
  }
}
