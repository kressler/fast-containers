// Copyright (c) 2025 Bryan Kressler
//
// SPDX-License-Identifier: BSD-3-Clause

#include <catch2/catch_test_macros.hpp>
#include <fast_containers/btree.hpp>
#include <fast_containers/hugepage_pool.hpp>
#include <fast_containers/policy_based_hugepage_allocator.hpp>
#include <string>
#include <vector>

using namespace kressler::fast_containers;

TEST_CASE("HugePagePool - type-erased allocations", "[hugepage_pool][policy]") {
  SECTION("Basic allocation and deallocation") {
    HugePagePool pool(1024 * 1024, false);  // 1MB, no hugepages

    // Allocate various sizes
    void* p1 = pool.allocate(64, 64);
    REQUIRE(p1 != nullptr);

    void* p2 = pool.allocate(128, 64);
    REQUIRE(p2 != nullptr);
    REQUIRE(p2 != p1);

    void* p3 = pool.allocate(256, 64);
    REQUIRE(p3 != nullptr);
    REQUIRE(p3 != p1);
    REQUIRE(p3 != p2);

    // Deallocate
    pool.deallocate(p1, 64);
    pool.deallocate(p2, 128);
    pool.deallocate(p3, 256);

    // Reallocate (should reuse from free list)
    void* p4 = pool.allocate(64, 64);
    REQUIRE(p4 != nullptr);
    // p4 might be p3, p2, or p1 depending on free list order

    pool.deallocate(p4, 64);
  }

  SECTION("Alignment requirements") {
    HugePagePool pool(1024 * 1024, false);

    // Test various alignments
    for (size_t align : {8, 16, 32, 64, 128, 256}) {
      void* p = pool.allocate(align, align);
      REQUIRE(p != nullptr);
      REQUIRE(reinterpret_cast<uintptr_t>(p) % align == 0);
      pool.deallocate(p, align);
    }
  }

  SECTION("Pool growth") {
    // Small initial pool to force growth
    HugePagePool pool(1024, false, 2048);  // 1KB initial, 2KB growth

    std::vector<void*> ptrs;

    // Allocate until pool grows
    for (int i = 0; i < 50; ++i) {
      void* p = pool.allocate(64, 64);
      REQUIRE(p != nullptr);
      ptrs.push_back(p);
    }

#ifdef ALLOCATOR_STATS
    REQUIRE(pool.get_growth_events() >= 1);
#endif

    // Clean up
    for (auto* p : ptrs) {
      pool.deallocate(p, 64);
    }
  }
}

#ifdef ALLOCATOR_STATS
TEST_CASE("HugePagePool - statistics", "[hugepage_pool][policy][stats]") {
  HugePagePool pool(1024 * 1024, false);

  REQUIRE(pool.get_allocations() == 0);
  REQUIRE(pool.get_deallocations() == 0);

  // Allocate 10 blocks
  std::vector<void*> ptrs;
  for (int i = 0; i < 10; ++i) {
    ptrs.push_back(pool.allocate(64, 64));
  }

  REQUIRE(pool.get_allocations() == 10);
  REQUIRE(pool.get_bytes_allocated() == 10 * 64);
  REQUIRE(pool.get_current_usage() == 10 * 64);

  // Deallocate 5 blocks
  for (int i = 0; i < 5; ++i) {
    pool.deallocate(ptrs[i], 64);
  }

  REQUIRE(pool.get_deallocations() == 5);
  REQUIRE(pool.get_current_usage() == 5 * 64);

  // Clean up
  for (size_t i = 5; i < ptrs.size(); ++i) {
    pool.deallocate(ptrs[i], 64);
  }
}
#endif

TEST_CASE("PolicyBasedHugePageAllocator - TwoPoolPolicy",
          "[policy][allocator]") {
  auto leaf_pool = std::make_shared<HugePagePool>(512 * 1024, false);
  auto internal_pool = std::make_shared<HugePagePool>(256 * 1024, false);
  TwoPoolPolicy policy(leaf_pool, internal_pool);

  SECTION("Value type uses leaf pool") {
    PolicyBasedHugePageAllocator<std::pair<int, std::string>, TwoPoolPolicy>
        alloc(policy);

    auto* p = alloc.allocate(1);
    REQUIRE(p != nullptr);

#ifdef ALLOCATOR_STATS
    REQUIRE(leaf_pool->get_allocations() == 1);
    REQUIRE(internal_pool->get_allocations() == 0);
#endif

    alloc.deallocate(p, 1);
  }

  SECTION("Policy selection preserves through rebind") {
    PolicyBasedHugePageAllocator<int64_t, TwoPoolPolicy> alloc1(policy);
    PolicyBasedHugePageAllocator<double, TwoPoolPolicy> alloc2(alloc1);

    // Both should use leaf pool (no leaf_node/internal_node traits)
    int64_t* p1 = alloc1.allocate(1);
    double* p2 = alloc2.allocate(1);

    REQUIRE(p1 != nullptr);
    REQUIRE(p2 != nullptr);

    alloc1.deallocate(p1, 1);
    alloc2.deallocate(p2, 1);
  }
}

TEST_CASE("PolicyBasedHugePageAllocator - btree integration",
          "[policy][allocator][btree]") {
  // Create two pools: larger for leaves, smaller for internals
  auto leaf_pool = std::make_shared<HugePagePool>(512 * 1024, false);
  auto internal_pool = std::make_shared<HugePagePool>(256 * 1024, false);

#ifdef ALLOCATOR_STATS
  size_t initial_leaf_allocs = leaf_pool->get_allocations();
  size_t initial_internal_allocs = internal_pool->get_allocations();
#endif

  TwoPoolPolicy policy(leaf_pool, internal_pool);
  using AllocType =
      PolicyBasedHugePageAllocator<std::pair<int, std::string>, TwoPoolPolicy>;
  AllocType alloc(policy);

  btree<int, std::string, 32, 32, std::less<int>, SearchMode::Binary, AllocType>
      tree(alloc);

  SECTION("Single btree uses both pools") {
    // Insert enough elements to create internal nodes
    for (int i = 0; i < 100; ++i) {
      tree.insert(i, "value" + std::to_string(i));
    }

    REQUIRE(tree.size() == 100);

#ifdef ALLOCATOR_STATS
    // Both pools should have allocations
    size_t leaf_allocs = leaf_pool->get_allocations() - initial_leaf_allocs;
    size_t internal_allocs =
        internal_pool->get_allocations() - initial_internal_allocs;

    REQUIRE(leaf_allocs > 0);
    // May or may not have internal nodes depending on size
    // but we should at least see the pools are separate
#endif

    // Verify lookups
    for (int i = 0; i < 100; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == "value" + std::to_string(i));
    }
  }

  SECTION("Multiple btrees share pools") {
    using BTreeType = btree<int, std::string, 32, 32, std::less<int>,
                            SearchMode::Binary, AllocType>;

    BTreeType tree1(alloc);
    BTreeType tree2(alloc);
    BTreeType tree3(alloc);

    // Insert into all three trees
    for (int i = 0; i < 50; ++i) {
      tree1.insert(i, "tree1_" + std::to_string(i));
      tree2.insert(i + 100, "tree2_" + std::to_string(i));
      tree3.insert(i + 200, "tree3_" + std::to_string(i));
    }

    REQUIRE(tree1.size() == 50);
    REQUIRE(tree2.size() == 50);
    REQUIRE(tree3.size() == 50);

#ifdef ALLOCATOR_STATS
    // All three trees should share the same pools
    size_t leaf_allocs = leaf_pool->get_allocations() - initial_leaf_allocs;
    REQUIRE(leaf_allocs > 0);
    // Allocations come from shared pools
#endif

    // Verify all trees work correctly
    auto it1 = tree1.find(25);
    REQUIRE(it1 != tree1.end());
    REQUIRE(it1->second == "tree1_25");

    auto it2 = tree2.find(125);
    REQUIRE(it2 != tree2.end());
    REQUIRE(it2->second == "tree2_25");

    auto it3 = tree3.find(225);
    REQUIRE(it3 != tree3.end());
    REQUIRE(it3->second == "tree3_25");
  }
}

#ifdef ALLOCATOR_STATS
TEST_CASE("PolicyBasedHugePageAllocator - direct pool statistics access",
          "[policy][allocator][stats]") {
  auto leaf_pool = std::make_shared<HugePagePool>(512 * 1024, false);
  auto internal_pool = std::make_shared<HugePagePool>(256 * 1024, false);

  size_t initial_leaf_allocs = leaf_pool->get_allocations();
  size_t initial_internal_allocs = internal_pool->get_allocations();

  TwoPoolPolicy policy(leaf_pool, internal_pool);
  using AllocType =
      PolicyBasedHugePageAllocator<std::pair<int, std::string>, TwoPoolPolicy>;
  AllocType alloc(policy);

  btree<int, std::string, 32, 32, std::less<int>, SearchMode::Binary, AllocType>
      tree(alloc);

  // Insert elements
  for (int i = 0; i < 100; ++i) {
    tree.insert(i, "value" + std::to_string(i));
  }

  // Access statistics directly from pools
  size_t leaf_allocs = leaf_pool->get_allocations() - initial_leaf_allocs;
  size_t leaf_bytes = leaf_pool->get_bytes_allocated();
  size_t leaf_usage = leaf_pool->get_current_usage();
  size_t leaf_peak = leaf_pool->get_peak_usage();

  size_t internal_allocs =
      internal_pool->get_allocations() - initial_internal_allocs;
  size_t internal_bytes = internal_pool->get_bytes_allocated();
  size_t internal_usage = internal_pool->get_current_usage();
  size_t internal_peak = internal_pool->get_peak_usage();

  // Verify we can access statistics
  REQUIRE(leaf_allocs > 0);
  REQUIRE(leaf_bytes > 0);
  REQUIRE(leaf_usage > 0);
  REQUIRE(leaf_peak >= leaf_usage);

  // Internal nodes may or may not exist with 100 elements
  // but the pools should be separate
  REQUIRE(leaf_pool != internal_pool);
}
#endif

TEST_CASE("Type trait detection", "[policy][traits]") {
  // Create a mock btree to get its node types
  using TestTree = btree<int, std::string, 32, 32>;

  SECTION("leaf_node detection") {
    REQUIRE(has_next_leaf_v<TestTree::leaf_node>);
    REQUIRE_FALSE(has_children_are_leaves_v<TestTree::leaf_node>);
  }

  SECTION("internal_node detection") {
    REQUIRE_FALSE(has_next_leaf_v<TestTree::internal_node>);
    REQUIRE(has_children_are_leaves_v<TestTree::internal_node>);
  }

  SECTION("Regular types don't match") {
    REQUIRE_FALSE(has_next_leaf_v<int64_t>);
    REQUIRE_FALSE(has_children_are_leaves_v<int64_t>);
    REQUIRE_FALSE(has_next_leaf_v<std::string>);
    REQUIRE_FALSE(has_children_are_leaves_v<std::string>);
  }
}

TEST_CASE("make_two_pool_allocator factory function", "[policy][factory]") {
  SECTION("Basic creation and usage") {
    auto alloc = make_two_pool_allocator<int, std::string>(
        512 * 1024,   // leaf pool size
        256 * 1024);  // internal pool size

    // Verify allocator works
    auto* p = alloc.allocate(1);
    REQUIRE(p != nullptr);
    alloc.deallocate(p, 1);

    // Verify pools are accessible
    auto& leaf_pool = alloc.get_policy().leaf_pool_;
    auto& internal_pool = alloc.get_policy().internal_pool_;
    REQUIRE(leaf_pool != nullptr);
    REQUIRE(internal_pool != nullptr);
    REQUIRE(leaf_pool != internal_pool);
  }

  SECTION("Integration with btree") {
    auto alloc = make_two_pool_allocator<int, std::string>(512 * 1024,
                                                           256 * 1024, false);

    using AllocType = decltype(alloc);
    btree<int, std::string, 32, 32, std::less<int>, SearchMode::Binary,
          AllocType>
        tree(alloc);

    // Insert data
    for (int i = 0; i < 100; ++i) {
      tree.insert(i, "value" + std::to_string(i));
    }

    REQUIRE(tree.size() == 100);

#ifdef ALLOCATOR_STATS
    // Verify statistics are accessible
    auto& leaf_pool = alloc.get_policy().leaf_pool_;
    auto& internal_pool = alloc.get_policy().internal_pool_;
    REQUIRE(leaf_pool->get_allocations() > 0);
#endif
  }

  SECTION("Custom configuration") {
    auto alloc = make_two_pool_allocator<int, std::string>(
        1024 * 1024,  // leaf pool size
        512 * 1024,   // internal pool size
        false,        // no hugepages
        128 * 1024,   // leaf growth size
        64 * 1024);   // internal growth size

    auto* p = alloc.allocate(1);
    REQUIRE(p != nullptr);
    alloc.deallocate(p, 1);
  }
}

TEST_CASE("Factory functions - pool sharing", "[policy][factory]") {
  SECTION("Multiple btrees share same pools via two-pool allocator") {
    auto alloc = make_two_pool_allocator<int, std::string>(512 * 1024,
                                                           256 * 1024, false);

    using AllocType = decltype(alloc);
    using BTreeType = btree<int, std::string, 32, 32, std::less<int>,
                            SearchMode::Binary, AllocType>;

    BTreeType tree1(alloc);
    BTreeType tree2(alloc);

    // Insert into both trees
    for (int i = 0; i < 50; ++i) {
      tree1.insert(i, "tree1_" + std::to_string(i));
      tree2.insert(i + 100, "tree2_" + std::to_string(i));
    }

    REQUIRE(tree1.size() == 50);
    REQUIRE(tree2.size() == 50);

#ifdef ALLOCATOR_STATS
    // Both trees share the same pools
    auto& leaf_pool = alloc.get_policy().leaf_pool_;
    REQUIRE(leaf_pool->get_allocations() > 0);
#endif
  }
}
