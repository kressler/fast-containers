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

TEMPLATE_TEST_CASE("btree erase operations - basic", "[btree][erase]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  using BTree = btree<int, int, 64, 64, Mode>;

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
                   LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Erase causes borrow from left sibling") {
    // Small node size to test underflow
    btree<int, int, 4, 8, Mode> tree;

    // Insert elements to create siblings
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    // Remove elements to cause underflow and borrowing
    tree.erase(9);
    tree.erase(10);

    // Verify tree integrity
    REQUIRE(tree.size() == 8);
    for (int i = 1; i <= 8; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("Erase causes borrow from right sibling") {
    btree<int, int, 4, 8, Mode> tree;

    // Insert elements
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    // Remove from left side to cause borrowing from right
    tree.erase(1);
    tree.erase(2);

    REQUIRE(tree.size() == 8);
    for (int i = 3; i <= 10; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("Erase causes merge with left sibling") {
    btree<int, int, 4, 8, Mode> tree;

    // Insert to create structure with mergeable nodes
    for (int i = 1; i <= 15; ++i) {
      tree.insert(i, i * 10);
    }

    // Remove many elements to force merging
    for (int i = 8; i <= 15; ++i) {
      tree.erase(i);
    }

    REQUIRE(tree.size() == 7);
    for (int i = 1; i <= 7; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }

    // Verify iteration still works
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys == std::vector<int>{1, 2, 3, 4, 5, 6, 7});
  }

  SECTION("Erase causes merge with right sibling") {
    btree<int, int, 4, 8, Mode> tree;

    for (int i = 1; i <= 15; ++i) {
      tree.insert(i, i * 10);
    }

    // Remove from left to force merge with right
    for (int i = 1; i <= 8; ++i) {
      tree.erase(i);
    }

    REQUIRE(tree.size() == 7);
    for (int i = 9; i <= 15; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("Erase causes cascading merges up the tree") {
    // Very small nodes to create deep tree
    btree<int, int, 3, 3, Mode> tree;

    // Insert many elements to create multi-level tree
    for (int i = 1; i <= 40; ++i) {
      tree.insert(i, i * 10);
    }

    // Verify all inserted
    REQUIRE(tree.size() == 40);

    // Remove most elements (causes cascading merges/rebalances)
    for (int i = 20; i <= 40; ++i) {
      tree.erase(i);
    }

    REQUIRE(tree.size() == 19);

    // Verify remaining elements
    for (int i = 1; i <= 19; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }

    // Verify iteration
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys.size() == 19);
    for (int i = 0; i < 19; ++i) {
      REQUIRE(keys[i] == i + 1);
    }
  }

  SECTION("Erase reduces tree height") {
    btree<int, int, 3, 3, Mode> tree;

    // Build multi-level tree
    for (int i = 1; i <= 30; ++i) {
      tree.insert(i, i * 10);
    }

    // Remove elements to collapse tree
    for (int i = 1; i <= 25; ++i) {
      tree.erase(i);
    }

    REQUIRE(tree.size() == 5);

    // Verify remaining elements
    for (int i = 26; i <= 30; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 10);
    }
  }
}

TEMPLATE_TEST_CASE("btree erase operations - edge cases", "[btree][erase]",
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("Erase minimum element updates leftmost") {
    btree<int, int, 4, 8, Mode> tree;

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
    btree<int, int, 4, 8, Mode> tree;

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
    btree<int, int, 4, 8, Mode> tree;

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
    btree<std::string, int, 4, 8, Mode> tree;

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
    btree<int, int, 4, 8, Mode> tree;

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
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("erase(iterator) - single element from middle") {
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 3, 3, Mode> tree;
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
    btree<int, int, 3, 3, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto next = tree.erase(tree.begin(), tree.end());
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.empty());
    REQUIRE(next == tree.end());
  }

  SECTION("erase(iterator, iterator) - empty range") {
    btree<int, int, 4, 4, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.find(5);
    auto next = tree.erase(it, it);  // Empty range

    REQUIRE(tree.size() == 10);  // Nothing erased
    REQUIRE(next == it);
  }

  SECTION("erase(iterator, iterator) - large range with underflow") {
    btree<int, int, 3, 3, Mode> tree;
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
    btree<std::string, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;

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
                   BinarySearchMode, LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("lower_bound - empty tree") {
    btree<int, int, 4, 4, Mode> tree;
    auto it = tree.lower_bound(5);
    REQUIRE(it == tree.end());
  }

  SECTION("upper_bound - empty tree") {
    btree<int, int, 4, 4, Mode> tree;
    auto it = tree.upper_bound(5);
    REQUIRE(it == tree.end());
  }

  SECTION("lower_bound - exact match") {
    btree<int, int, 4, 4, Mode> tree;
    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.lower_bound(10);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 10);
    REQUIRE(it->second == 100);
  }

  SECTION("upper_bound - exact match") {
    btree<int, int, 4, 4, Mode> tree;
    for (int i = 1; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.upper_bound(10);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 11);
    REQUIRE(it->second == 110);
  }

  SECTION("lower_bound - key between elements") {
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.lower_bound(1);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 1);
    REQUIRE(it->second == 10);
  }

  SECTION("upper_bound - key at beginning") {
    btree<int, int, 4, 4, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.upper_bound(1);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 2);
    REQUIRE(it->second == 20);
  }

  SECTION("lower_bound - key at end") {
    btree<int, int, 4, 4, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.lower_bound(10);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 10);
    REQUIRE(it->second == 100);
  }

  SECTION("upper_bound - key at end") {
    btree<int, int, 4, 4, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.upper_bound(10);
    REQUIRE(it == tree.end());
  }

  SECTION("lower_bound - key before all elements") {
    btree<int, int, 4, 4, Mode> tree;
    for (int i = 10; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.lower_bound(5);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 10);
    REQUIRE(it->second == 100);
  }

  SECTION("upper_bound - key before all elements") {
    btree<int, int, 4, 4, Mode> tree;
    for (int i = 10; i <= 20; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.upper_bound(5);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 10);
    REQUIRE(it->second == 100);
  }

  SECTION("lower_bound - key after all elements") {
    btree<int, int, 4, 4, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.lower_bound(20);
    REQUIRE(it == tree.end());
  }

  SECTION("upper_bound - key after all elements") {
    btree<int, int, 4, 4, Mode> tree;
    for (int i = 1; i <= 10; ++i) {
      tree.insert(i, i * 10);
    }

    auto it = tree.upper_bound(20);
    REQUIRE(it == tree.end());
  }

  SECTION("lower_bound and upper_bound - range query") {
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;

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
    btree<int, int, 4, 4, Mode> tree;

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
    btree<int, int, 4, 4, Mode> tree;

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
    btree<int, int, 4, 4, Mode> tree;

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
    btree<std::string, int, 4, 4, Mode> tree;

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
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;

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
    btree<int, int, 4, 4, Mode> tree;

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
TEMPLATE_TEST_CASE("btree clear", "[btree][clear]", BinarySearchMode,
                   LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("clear - empty tree") {
    btree<int, int, 4, 4, Mode> tree;
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);

    tree.clear();
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
  }

  SECTION("clear - single element") {
    btree<int, int, 4, 4, Mode> tree;
    tree.insert(5, 50);
    REQUIRE(tree.size() == 1);

    tree.clear();
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.find(5) == tree.end());
  }

  SECTION("clear - multiple elements, single leaf") {
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;

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
                   LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("count - empty tree") {
    const btree<int, int, 4, 4, Mode> tree;
    REQUIRE(tree.count(5) == 0);
    REQUIRE(tree.count(0) == 0);
    REQUIRE(tree.count(100) == 0);
  }

  SECTION("count - single element found") {
    btree<int, int, 4, 4, Mode> tree;
    tree.insert(5, 50);

    const auto& const_tree = tree;
    REQUIRE(const_tree.count(5) == 1);
  }

  SECTION("count - single element not found") {
    btree<int, int, 4, 4, Mode> tree;
    tree.insert(5, 50);

    const auto& const_tree = tree;
    REQUIRE(const_tree.count(3) == 0);
    REQUIRE(const_tree.count(7) == 0);
  }

  SECTION("count - multiple elements") {
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;
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
    btree<std::string, int, 4, 4, Mode> tree;

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
    btree<int, int, 4, 4, Mode> tree;
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
    btree<int, int, 4, 4, Mode> tree;

    tree.insert(5, 50);
    const auto& const_tree = tree;
    REQUIRE(const_tree.count(5) == 1);

    // Try to insert duplicate (should not be inserted)
    tree.insert(5, 999);
    REQUIRE(tree.size() == 1);
    REQUIRE(const_tree.count(5) == 1);  // Still 1, not 2
  }
}

TEMPLATE_TEST_CASE("btree copy constructor", "[btree][copy]", BinarySearchMode,
                   LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("copy constructor - empty tree") {
    btree<int, int, 4, 4, Mode> tree1;
    btree<int, int, 4, 4, Mode> tree2(tree1);

    REQUIRE(tree2.empty());
    REQUIRE(tree2.size() == 0);
  }

  SECTION("copy constructor - single element") {
    btree<int, int, 4, 4, Mode> tree1;
    tree1.insert(5, 50);

    btree<int, int, 4, 4, Mode> tree2(tree1);

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
    btree<int, int, 4, 4, Mode> tree1;
    for (int i = 1; i <= 20; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 4, 4, Mode> tree2(tree1);

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
    btree<int, int, 4, 4, Mode> tree1;
    for (int i = 1; i <= 1000; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 4, 4, Mode> tree2(tree1);

    REQUIRE(tree2.size() == 1000);
    for (int i = 1; i <= 1000; i += 100) {
      auto it = tree2.find(i);
      REQUIRE(it != tree2.end());
      REQUIRE(it->second == i * 10);
    }
  }

  SECTION("copy constructor - string keys") {
    btree<std::string, int, 4, 4, Mode> tree1;
    tree1.insert("apple", 1);
    tree1.insert("banana", 2);
    tree1.insert("cherry", 3);

    btree<std::string, int, 4, 4, Mode> tree2(tree1);

    REQUIRE(tree2.size() == 3);
    REQUIRE(tree2.find("apple")->second == 1);
    REQUIRE(tree2.find("banana")->second == 2);
    REQUIRE(tree2.find("cherry")->second == 3);
  }
}

TEMPLATE_TEST_CASE("btree copy assignment", "[btree][copy]", BinarySearchMode,
                   LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("copy assignment - empty to empty") {
    btree<int, int, 4, 4, Mode> tree1;
    btree<int, int, 4, 4, Mode> tree2;

    tree2 = tree1;
    REQUIRE(tree2.empty());
  }

  SECTION("copy assignment - non-empty to empty") {
    btree<int, int, 4, 4, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 4, 4, Mode> tree2;
    tree2 = tree1;

    REQUIRE(tree2.size() == 10);
    for (int i = 1; i <= 10; ++i) {
      REQUIRE(tree2.find(i) != tree2.end());
    }
  }

  SECTION("copy assignment - empty to non-empty") {
    btree<int, int, 4, 4, Mode> tree1;

    btree<int, int, 4, 4, Mode> tree2;
    for (int i = 1; i <= 10; ++i) {
      tree2.insert(i, i * 10);
    }

    tree2 = tree1;
    REQUIRE(tree2.empty());
    REQUIRE(tree2.size() == 0);
  }

  SECTION("copy assignment - non-empty to non-empty") {
    btree<int, int, 4, 4, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 4, 4, Mode> tree2;
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
    btree<int, int, 4, 4, Mode> tree1;
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
    btree<int, int, 4, 4, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 4, 4, Mode> tree2;
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
                   LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("move constructor - empty tree") {
    btree<int, int, 4, 4, Mode> tree1;
    btree<int, int, 4, 4, Mode> tree2(std::move(tree1));

    REQUIRE(tree2.empty());
    REQUIRE(tree1.empty());  // Moved-from tree is empty
  }

  SECTION("move constructor - single element") {
    btree<int, int, 4, 4, Mode> tree1;
    tree1.insert(5, 50);

    btree<int, int, 4, 4, Mode> tree2(std::move(tree1));

    REQUIRE(tree2.size() == 1);
    REQUIRE(tree2.find(5) != tree2.end());

    // tree1 is now empty
    REQUIRE(tree1.empty());
    REQUIRE(tree1.size() == 0);
  }

  SECTION("move constructor - multiple elements") {
    btree<int, int, 4, 4, Mode> tree1;
    for (int i = 1; i <= 20; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 4, 4, Mode> tree2(std::move(tree1));

    REQUIRE(tree2.size() == 20);
    for (int i = 1; i <= 20; ++i) {
      REQUIRE(tree2.find(i) != tree2.end());
    }

    // tree1 is now empty
    REQUIRE(tree1.empty());
  }

  SECTION("move constructor - moved-from tree is reusable") {
    btree<int, int, 4, 4, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 4, 4, Mode> tree2(std::move(tree1));

    // Reuse tree1
    tree1.insert(100, 1000);
    REQUIRE(tree1.size() == 1);
    REQUIRE(tree1.find(100) != tree1.end());

    // tree2 should still have the original data
    REQUIRE(tree2.size() == 10);
  }

  SECTION("move constructor - string keys") {
    btree<std::string, int, 4, 4, Mode> tree1;
    tree1.insert("apple", 1);
    tree1.insert("banana", 2);

    btree<std::string, int, 4, 4, Mode> tree2(std::move(tree1));

    REQUIRE(tree2.size() == 2);
    REQUIRE(tree2.find("apple") != tree2.end());
    REQUIRE(tree1.empty());
  }
}

TEMPLATE_TEST_CASE("btree move assignment", "[btree][move]", BinarySearchMode,
                   LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("move assignment - empty to empty") {
    btree<int, int, 4, 4, Mode> tree1;
    btree<int, int, 4, 4, Mode> tree2;

    tree2 = std::move(tree1);
    REQUIRE(tree2.empty());
    REQUIRE(tree1.empty());
  }

  SECTION("move assignment - non-empty to empty") {
    btree<int, int, 4, 4, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 4, 4, Mode> tree2;
    tree2 = std::move(tree1);

    REQUIRE(tree2.size() == 10);
    REQUIRE(tree1.empty());
  }

  SECTION("move assignment - empty to non-empty") {
    btree<int, int, 4, 4, Mode> tree1;

    btree<int, int, 4, 4, Mode> tree2;
    for (int i = 1; i <= 10; ++i) {
      tree2.insert(i, i * 10);
    }

    tree2 = std::move(tree1);
    REQUIRE(tree2.empty());
    REQUIRE(tree1.empty());
  }

  SECTION("move assignment - non-empty to non-empty") {
    btree<int, int, 4, 4, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 4, 4, Mode> tree2;
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
    btree<int, int, 4, 4, Mode> tree1;
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
    btree<int, int, 4, 4, Mode> tree1;
    for (int i = 1; i <= 10; ++i) {
      tree1.insert(i, i * 10);
    }

    btree<int, int, 4, 4, Mode> tree2;
    tree2 = std::move(tree1);

    // Reuse tree1
    tree1.insert(100, 1000);
    REQUIRE(tree1.size() == 1);
    REQUIRE(tree1.find(100) != tree1.end());

    // tree2 should have the original data
    REQUIRE(tree2.size() == 10);
  }
}

TEMPLATE_TEST_CASE("btree operator[]", "[btree][access]", BinarySearchMode,
                   LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;

  SECTION("operator[] - insert new element with default value") {
    btree<int, int, 4, 4, Mode> tree;

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
    btree<int, int, 4, 4, Mode> tree;
    tree.insert(5, 50);

    // Access existing key returns its value
    int& value = tree[5];
    REQUIRE(value == 50);
    REQUIRE(tree.size() == 1);  // Size unchanged
  }

  SECTION("operator[] - modify value through reference") {
    btree<int, int, 4, 4, Mode> tree;
    tree.insert(5, 50);

    // Modify value through operator[]
    tree[5] = 100;

    REQUIRE(tree[5] == 100);
    auto it = tree.find(5);
    REQUIRE(it != tree.end());
    REQUIRE(it->second == 100);
  }

  SECTION("operator[] - insert and modify in one operation") {
    btree<int, int, 4, 4, Mode> tree;

    // Insert and immediately modify
    tree[10] = 999;

    REQUIRE(tree.size() == 1);
    REQUIRE(tree[10] == 999);
  }

  SECTION("operator[] - multiple inserts and accesses") {
    btree<int, int, 4, 4, Mode> tree;

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
    btree<int, int, 4, 4, Mode> tree;

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
    btree<int, int, 4, 4, Mode> tree;

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
    btree<std::string, int, 4, 4, Mode> tree;

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
    btree<int, std::string, 4, 4, Mode> tree;

    // Access non-existent key creates it with default value (empty string)
    std::string& value = tree[5];
    REQUIRE(value.empty());
    REQUIRE(tree.size() == 1);

    // Modify the value
    tree[5] = "hello";
    REQUIRE(tree[5] == "hello");
  }

  SECTION("operator[] - increment pattern") {
    btree<int, int, 4, 4, Mode> tree;

    // Common pattern: increment counter
    for (int i = 0; i < 10; ++i) {
      tree[5]++;  // Increment value at key 5
    }

    REQUIRE(tree.size() == 1);
    REQUIRE(tree[5] == 10);
  }

  SECTION("operator[] - overwrite pattern") {
    btree<int, int, 4, 4, Mode> tree;

    // Insert initial value
    tree.insert(5, 50);
    REQUIRE(tree[5] == 50);

    // Overwrite with operator[]
    tree[5] = 100;
    REQUIRE(tree[5] == 100);
    REQUIRE(tree.size() == 1);
  }
}
