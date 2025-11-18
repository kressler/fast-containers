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

// Helper class to build test trees manually (friend of btree)
template <typename BTree>
class BTreeTestHelper {
 public:
  using LeafNode = typename BTree::LeafNode;
  using InternalNode = typename BTree::InternalNode;

  // Create a single-leaf tree with given key-value pairs
  static void create_single_leaf_tree(BTree& tree,
                                      std::vector<std::pair<int, int>> data) {
    if (data.empty())
      return;

    auto* leaf = tree.allocate_leaf_node();
    for (const auto& [key, value] : data) {
      leaf->data.insert(key, value);
    }

    tree.root_is_leaf_ = true;
    tree.leaf_root_ = leaf;
    tree.size_ = data.size();
  }

  // Create a two-level tree with given structure
  // leaves_data: vector of vectors, each inner vector is one leaf's data
  static void create_two_level_tree(
      BTree& tree, std::vector<std::vector<std::pair<int, int>>> leaves_data) {
    if (leaves_data.empty())
      return;

    std::vector<LeafNode*> leaves;
    LeafNode* prev = nullptr;

    // Create all leaf nodes
    for (const auto& leaf_data : leaves_data) {
      auto* leaf = tree.allocate_leaf_node();
      for (const auto& [key, value] : leaf_data) {
        leaf->data.insert(key, value);
      }

      // Link leaves together
      if (prev) {
        prev->next_leaf = leaf;
        leaf->prev_leaf = prev;
      }
      leaves.push_back(leaf);
      prev = leaf;
    }

    // Create internal root node
    auto* root = tree.allocate_internal_node(true);
    for (auto* leaf : leaves) {
      // Use the first key of each leaf as the key in the internal node
      int first_key = leaf->data.begin()->first;
      root->leaf_children.insert(first_key, leaf);
      leaf->parent = root;
    }

    tree.root_is_leaf_ = false;
    tree.internal_root_ = root;

    // Count total size
    std::size_t total_size = 0;
    for (const auto& leaf_data : leaves_data) {
      total_size += leaf_data.size();
    }
    tree.size_ = total_size;
  }
};

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
  using Helper = BTreeTestHelper<BTree>;

  SECTION("Find existing elements") {
    BTree tree;
    Helper::create_single_leaf_tree(tree, {{1, 10}, {3, 30}, {5, 50}});

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
    Helper::create_single_leaf_tree(tree, {{1, 10}, {3, 30}, {5, 50}});

    REQUIRE(tree.find(0) == tree.end());
    REQUIRE(tree.find(2) == tree.end());
    REQUIRE(tree.find(4) == tree.end());
    REQUIRE(tree.find(10) == tree.end());
  }

  SECTION("Iterate through elements") {
    BTree tree;
    Helper::create_single_leaf_tree(tree, {{1, 10}, {3, 30}, {5, 50}, {7, 70}});

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
    Helper::create_single_leaf_tree(tree, {{2, 20}, {4, 40}, {6, 60}});

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

TEMPLATE_TEST_CASE("btree multi-level tree operations",
                   "[btree][find][iterator][multilevel]", BinarySearchMode,
                   LinearSearchMode, SIMDSearchMode) {
  constexpr SearchMode Mode = TestType::value;
  using BTree = btree<int, int, 64, 64, Mode>;
  using Helper = BTreeTestHelper<BTree>;

  SECTION("Two-level tree find operations") {
    BTree tree;
    // Create a tree with 3 leaves:
    // Leaf 1: {1,10}, {2,20}
    // Leaf 2: {5,50}, {6,60}, {7,70}
    // Leaf 3: {10,100}, {15,150}
    Helper::create_two_level_tree(tree, {{{1, 10}, {2, 20}},
                                         {{5, 50}, {6, 60}, {7, 70}},
                                         {{10, 100}, {15, 150}}});

    REQUIRE(tree.size() == 7);

    // Find in first leaf
    auto it = tree.find(1);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 1);
    REQUIRE(it->second == 10);

    // Find in middle leaf
    it = tree.find(6);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 6);
    REQUIRE(it->second == 60);

    // Find in last leaf
    it = tree.find(15);
    REQUIRE(it != tree.end());
    REQUIRE(it->first == 15);
    REQUIRE(it->second == 150);

    // Find non-existing
    REQUIRE(tree.find(3) == tree.end());
    REQUIRE(tree.find(8) == tree.end());
    REQUIRE(tree.find(20) == tree.end());
  }

  SECTION("Two-level tree iteration") {
    BTree tree;
    Helper::create_two_level_tree(
        tree, {{{1, 10}, {3, 30}}, {{5, 50}, {7, 70}}, {{9, 90}}});

    std::vector<int> keys;
    std::vector<int> values;
    for (auto pair : tree) {
      keys.push_back(pair.first);
      values.push_back(pair.second);
    }

    REQUIRE(keys == std::vector<int>{1, 3, 5, 7, 9});
    REQUIRE(values == std::vector<int>{10, 30, 50, 70, 90});
  }

  SECTION("Two-level tree with single-element leaves") {
    BTree tree;
    Helper::create_two_level_tree(tree,
                                  {{{2, 20}}, {{4, 40}}, {{6, 60}}, {{8, 80}}});

    REQUIRE(tree.size() == 4);

    // Verify all elements can be found
    for (int key : {2, 4, 6, 8}) {
      auto it = tree.find(key);
      REQUIRE(it != tree.end());
      REQUIRE(it->first == key);
      REQUIRE(it->second == key * 10);
    }

    // Verify iteration order
    std::vector<int> keys;
    for (auto pair : tree) {
      keys.push_back(pair.first);
    }
    REQUIRE(keys == std::vector<int>{2, 4, 6, 8});
  }
}
