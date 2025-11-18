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

// Helper function to populate tree with data using insert()
template <typename BTree>
void populate_tree(BTree& tree, std::vector<std::pair<int, int>> data) {
  for (const auto& [key, value] : data) {
    tree.insert(key, value);
  }
}

TEMPLATE_TEST_CASE("btree empty tree iterators", "[btree][iterator]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Empty tree begin() == end()") {
    btree<int, int, 64, 64, Mode> tree;
    REQUIRE(tree.begin() == tree.end());
  }

  SECTION("Empty tree find returns end()") {
    btree<int, int, 64, 64, Mode> tree;
    REQUIRE(tree.find(42) == tree.end());
  }
}

TEMPLATE_TEST_CASE("btree single-leaf tree operations",
                   "[btree][find][iterator]", BinarySearchMode,
                   LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  using BTree = btree<int, int, 64, 64, Mode>;

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
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  using BTree = btree<int, int, 64, 64, Mode>;

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
    btree<int, int, 8, 8, Mode> small_tree;

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
    btree<std::string, int, 64, 64, Mode> tree;

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
    btree<int, std::string, 64, 64, Mode> tree;

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
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Split leaf node when full") {
    // Create tree with small leaf size to force splitting
    btree<int, int, 4, 8, Mode> tree;

    // Insert 5 elements (leaf size is 4, so should split)
    for (int i = 1; i <= 5; ++i) {
      auto [it, inserted] = tree.insert(i, i * 10);
      REQUIRE(inserted == true);
      REQUIRE(it->first == i);
      REQUIRE(it->second == i * 10);
    }

    REQUIRE(tree.size() == 5);

    // Verify all elements can be found
    for (int i = 1; i <= 5; ++i) {
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
    REQUIRE(keys == std::vector<int>{1, 2, 3, 4, 5});
  }

  SECTION("Multiple leaf splits") {
    // Create tree with small node size
    btree<int, int, 4, 8, Mode> tree;

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
    btree<int, int, 4, 8, Mode> tree;

    // Insert in descending order (harder test case for splitting)
    for (int i = 10; i >= 1; --i) {
      auto [it, inserted] = tree.insert(i, i * 10);
      REQUIRE(inserted == true);
    }

    REQUIRE(tree.size() == 10);

    // Verify sorted iteration (should be ascending despite descending insert)
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
  }

  SECTION("Split with random insertion") {
    btree<int, int, 4, 8, Mode> tree;

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

  SECTION("Internal node splitting") {
    // Small internal node size to force internal node splits
    btree<int, int, 3, 3, Mode> tree;

    // Insert many elements to force multiple levels and internal splits
    for (int i = 1; i <= 30; ++i) {
      auto [it, inserted] = tree.insert(i, i * 10);
      REQUIRE(inserted == true);
    }

    REQUIRE(tree.size() == 30);

    // Verify all elements
    for (int i = 1; i <= 30; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }

    // Verify sorted iteration
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys.size() == 30);
    for (size_t i = 0; i < 30; ++i) {
      REQUIRE(keys[i] == static_cast<int>(i + 1));
    }
  }

  SECTION("Duplicate during split") {
    btree<int, int, 4, 8, Mode> tree;

    // Fill tree
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    // Try to insert duplicate (should return false even after splits)
    auto [it, inserted] = tree.insert(5, 999);
    REQUIRE(inserted == false);
    REQUIRE(it->first == 5);
    REQUIRE(it->second == 50);  // Original value unchanged
    REQUIRE(tree.size() == 10);
  }
}
