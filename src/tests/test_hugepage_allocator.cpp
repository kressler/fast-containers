#include <catch2/catch_test_macros.hpp>

#include "../btree.hpp"
#include "../hugepage_allocator.hpp"

using namespace fast_containers;

TEST_CASE("HugePageAllocator - basic allocation", "[hugepage_allocator]") {
  HugePageAllocator<int64_t> alloc(1024 * 1024);  // 1MB pool

  SECTION("Allocate and deallocate single elements") {
    int64_t* p1 = alloc.allocate(1);
    REQUIRE(p1 != nullptr);
    *p1 = 42;
    REQUIRE(*p1 == 42);
    alloc.deallocate(p1, 1);

    int64_t* p2 = alloc.allocate(1);
    REQUIRE(p2 != nullptr);
    *p2 = 100;
    REQUIRE(*p2 == 100);
    alloc.deallocate(p2, 1);
  }

  SECTION("Allocate n>1 throws exception") {
    REQUIRE_THROWS_AS(alloc.allocate(100), std::invalid_argument);
  }

  SECTION("Check remaining bytes") {
    size_t initial = alloc.bytes_remaining();
    REQUIRE(initial > 0);

    int64_t* p = alloc.allocate(1);
    size_t after = alloc.bytes_remaining();
    REQUIRE(after < initial);

    alloc.deallocate(p, 1);
  }
}

TEST_CASE("HugePageAllocator - rebind", "[hugepage_allocator]") {
  using Int64Alloc = HugePageAllocator<int64_t>;
  using DoubleAlloc = typename Int64Alloc::rebind<double>::other;

  Int64Alloc int64_alloc(1024 * 1024);
  DoubleAlloc double_alloc(int64_alloc);

  SECTION("Rebind creates separate pool") {
    int64_t* ip = int64_alloc.allocate(1);
    double* dp = double_alloc.allocate(1);

    REQUIRE(ip != nullptr);
    REQUIRE(dp != nullptr);

    *ip = 42;
    *dp = 3.14;

    REQUIRE(*ip == 42);
    REQUIRE(*dp == 3.14);

    int64_alloc.deallocate(ip, 1);
    double_alloc.deallocate(dp, 1);
  }
}

TEST_CASE("HugePageAllocator - fallback to regular pages",
          "[hugepage_allocator]") {
  // Force fallback by requesting hugepages but allowing fallback
  HugePageAllocator<int64_t> alloc(1024 * 1024, true);

  SECTION("Allocator works regardless of hugepage availability") {
    int64_t* p = alloc.allocate(1);
    REQUIRE(p != nullptr);
    *p = 42;
    REQUIRE(*p == 42);
    alloc.deallocate(p, 1);
  }
}

TEST_CASE("HugePageAllocator - dynamic growth", "[hugepage_allocator]") {
  // Create small initial pool that will need to grow
  constexpr size_t initial_size = 1024;  // 1KB initial
  constexpr size_t growth_size = 2048;   // 2KB growth
  HugePageAllocator<int64_t> alloc(initial_size, false, growth_size);

  SECTION("Pool grows when exhausted") {
    std::vector<int64_t*> ptrs;

    // Allocate enough to exhaust initial pool and trigger growth
    // initial_size / sizeof(int64_t) = 1024 / 8 = 128 int64_t's
    // Allocate 200 to ensure we trigger at least one growth
    for (int i = 0; i < 200; ++i) {
      int64_t* p = alloc.allocate(1);
      REQUIRE(p != nullptr);
      *p = i;
      ptrs.push_back(p);
    }

    // Verify all allocations are valid
    for (size_t i = 0; i < ptrs.size(); ++i) {
      REQUIRE(*ptrs[i] == static_cast<int64_t>(i));
    }

    // Clean up
    for (int64_t* p : ptrs) {
      alloc.deallocate(p, 1);
    }
  }

  SECTION("Free list works across regions") {
    // Allocate, deallocate, then allocate again from free list
    int64_t* p1 = alloc.allocate(1);
    *p1 = 42;
    alloc.deallocate(p1, 1);

    int64_t* p2 = alloc.allocate(1);  // Should reuse p1 from free list
    REQUIRE(p2 == p1);                // Same pointer reused
    *p2 = 100;
    REQUIRE(*p2 == 100);
    alloc.deallocate(p2, 1);
  }
}

TEST_CASE("HugePageAllocator - cache-line alignment", "[hugepage_allocator]") {
  HugePageAllocator<int64_t> alloc(1024 * 1024, false);

  SECTION("All allocations are 64-byte aligned") {
    constexpr size_t cache_line_size = 64;
    std::vector<int64_t*> ptrs;

    // Allocate multiple blocks and verify alignment
    for (int i = 0; i < 100; ++i) {
      int64_t* p = alloc.allocate(1);
      REQUIRE(p != nullptr);

      // Check that pointer is 64-byte aligned
      uintptr_t addr = reinterpret_cast<uintptr_t>(p);
      REQUIRE(addr % cache_line_size == 0);

      *p = i;
      ptrs.push_back(p);
    }

    // Verify all values
    for (size_t i = 0; i < ptrs.size(); ++i) {
      REQUIRE(*ptrs[i] == static_cast<int64_t>(i));
    }

    // Clean up
    for (int64_t* p : ptrs) {
      alloc.deallocate(p, 1);
    }
  }

  SECTION("Free list preserves alignment") {
    constexpr size_t cache_line_size = 64;

    // Allocate and deallocate
    int64_t* p1 = alloc.allocate(1);
    uintptr_t addr1 = reinterpret_cast<uintptr_t>(p1);
    REQUIRE(addr1 % cache_line_size == 0);
    alloc.deallocate(p1, 1);

    // Allocate again from free list
    int64_t* p2 = alloc.allocate(1);
    uintptr_t addr2 = reinterpret_cast<uintptr_t>(p2);
    REQUIRE(addr2 % cache_line_size == 0);
    REQUIRE(p2 == p1);  // Should be same pointer

    alloc.deallocate(p2, 1);
  }
}

TEST_CASE("btree with HugePageAllocator - basic operations",
          "[btree][hugepage_allocator]") {
  using TreeType =
      btree<int, std::string, 16, 16, std::less<int>, SearchMode::Binary,
            MoveMode::SIMD, HugePageAllocator<std::pair<int, std::string>>>;

  // Create allocator with 10MB pool (enough for testing)
  HugePageAllocator<std::pair<int, std::string>> alloc(10 * 1024 * 1024, false);

  SECTION("Construct tree with custom allocator") {
    TreeType tree(alloc);
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.empty());
  }

  SECTION("Insert elements") {
    TreeType tree(alloc);

    for (int i = 0; i < 100; ++i) {
      auto [it, inserted] = tree.insert(i, "value" + std::to_string(i));
      REQUIRE(inserted);
      REQUIRE(it->first == i);
      REQUIRE(it->second == "value" + std::to_string(i));
    }

    REQUIRE(tree.size() == 100);

    // Verify all elements
    for (int i = 0; i < 100; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->first == i);
      REQUIRE(it->second == "value" + std::to_string(i));
    }
  }

  SECTION("Erase elements") {
    TreeType tree(alloc);

    // Insert 50 elements
    for (int i = 0; i < 50; ++i) {
      tree.insert(i, "value" + std::to_string(i));
    }
    REQUIRE(tree.size() == 50);

    // Erase every other element
    for (int i = 0; i < 50; i += 2) {
      size_t erased = tree.erase(i);
      REQUIRE(erased == 1);
    }
    REQUIRE(tree.size() == 25);

    // Verify remaining elements
    for (int i = 1; i < 50; i += 2) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->first == i);
    }

    // Verify erased elements are gone
    for (int i = 0; i < 50; i += 2) {
      auto it = tree.find(i);
      REQUIRE(it == tree.end());
    }
  }

  SECTION("Copy constructor") {
    TreeType tree1(alloc);

    for (int i = 0; i < 20; ++i) {
      tree1.insert(i, "value" + std::to_string(i));
    }

    TreeType tree2(tree1);
    REQUIRE(tree2.size() == 20);

    // Verify all elements copied
    for (int i = 0; i < 20; ++i) {
      auto it = tree2.find(i);
      REQUIRE(it != tree2.end());
      REQUIRE(it->second == "value" + std::to_string(i));
    }

    // Modify original
    tree1.insert(100, "new_value");
    REQUIRE(tree1.size() == 21);
    REQUIRE(tree2.size() == 20);
  }

  SECTION("Move constructor") {
    TreeType tree1(alloc);

    for (int i = 0; i < 20; ++i) {
      tree1.insert(i, "value" + std::to_string(i));
    }

    TreeType tree2(std::move(tree1));
    REQUIRE(tree2.size() == 20);
    REQUIRE(tree1.empty());  // tree1 should be empty after move

    // Verify all elements moved
    for (int i = 0; i < 20; ++i) {
      auto it = tree2.find(i);
      REQUIRE(it != tree2.end());
      REQUIRE(it->second == "value" + std::to_string(i));
    }
  }
}

TEST_CASE("btree with HugePageAllocator - large dataset",
          "[btree][hugepage_allocator]") {
  using TreeType =
      btree<int64_t, int64_t, 64, 64, std::less<int64_t>, SearchMode::SIMD,
            MoveMode::SIMD, HugePageAllocator<std::pair<int64_t, int64_t>>>;

  // Create allocator with 100MB pool
  HugePageAllocator<std::pair<int64_t, int64_t>> alloc(100 * 1024 * 1024,
                                                       false);
  TreeType tree(alloc);

  SECTION("Insert 10K elements") {
    for (int64_t i = 0; i < 10000; ++i) {
      auto [it, inserted] = tree.insert(i, i * 2);
      REQUIRE(inserted);
    }

    REQUIRE(tree.size() == 10000);

    // Verify random subset
    for (int64_t i = 0; i < 10000; i += 137) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == i * 2);
    }
  }

  SECTION("Sequential iteration") {
    for (int64_t i = 0; i < 1000; ++i) {
      tree.insert(i, i * 3);
    }

    int64_t expected = 0;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
      REQUIRE(it->first == expected);
      REQUIRE(it->second == expected * 3);
      ++expected;
    }
    REQUIRE(expected == 1000);
  }
}

TEST_CASE("btree with std::allocator - default allocator still works",
          "[btree][allocator]") {
  // Ensure default std::allocator still works
  btree<int, std::string> tree;

  SECTION("Basic operations with default allocator") {
    for (int i = 0; i < 100; ++i) {
      tree.insert(i, "value" + std::to_string(i));
    }

    REQUIRE(tree.size() == 100);

    for (int i = 0; i < 100; ++i) {
      auto it = tree.find(i);
      REQUIRE(it != tree.end());
      REQUIRE(it->second == "value" + std::to_string(i));
    }
  }
}

#ifdef ALLOCATOR_STATS
TEST_CASE("HugePageAllocator - statistics tracking",
          "[hugepage_allocator][stats]") {
  // Create small pool to force growth
  constexpr size_t initial_size = 1024;  // 1KB initial
  constexpr size_t growth_size = 2048;   // 2KB growth
  HugePageAllocator<int64_t> alloc(initial_size, false, growth_size);

  SECTION("Track allocations and deallocations") {
    std::vector<int64_t*> ptrs;

    // Allocate 10 blocks
    for (int i = 0; i < 10; ++i) {
      int64_t* p = alloc.allocate(1);
      *p = i;
      ptrs.push_back(p);
    }

    REQUIRE(alloc.get_allocations() == 10);
    REQUIRE(alloc.get_deallocations() == 0);
    REQUIRE(alloc.get_bytes_allocated() == 10 * sizeof(int64_t));
    REQUIRE(alloc.get_current_usage() == 10 * sizeof(int64_t));
    REQUIRE(alloc.get_peak_usage() == 10 * sizeof(int64_t));

    // Deallocate 5 blocks
    for (int i = 0; i < 5; ++i) {
      alloc.deallocate(ptrs[i], 1);
    }

    REQUIRE(alloc.get_allocations() == 10);
    REQUIRE(alloc.get_deallocations() == 5);
    REQUIRE(alloc.get_current_usage() == 5 * sizeof(int64_t));
    REQUIRE(alloc.get_peak_usage() == 10 * sizeof(int64_t));  // Peak unchanged

    // Allocate 3 more blocks (reusing from free list)
    for (int i = 0; i < 3; ++i) {
      int64_t* p = alloc.allocate(1);
      *p = i + 100;
      ptrs.push_back(p);
    }

    REQUIRE(alloc.get_allocations() == 13);
    REQUIRE(alloc.get_deallocations() == 5);
    REQUIRE(alloc.get_current_usage() == 8 * sizeof(int64_t));
    REQUIRE(alloc.get_peak_usage() == 10 * sizeof(int64_t));

    // Clean up
    for (size_t i = 5; i < ptrs.size(); ++i) {
      alloc.deallocate(ptrs[i], 1);
    }
  }

  SECTION("Track pool growth") {
    REQUIRE(alloc.get_growth_events() == 0);

    std::vector<int64_t*> ptrs;

    // Allocate enough to trigger growth
    // initial_size / sizeof(int64_t) = 1024 / 8 = 128 int64_t's
    // Allocate 200 to ensure growth
    for (int i = 0; i < 200; ++i) {
      int64_t* p = alloc.allocate(1);
      *p = i;
      ptrs.push_back(p);
    }

    // Should have grown at least once
    REQUIRE(alloc.get_growth_events() >= 1);
    REQUIRE(alloc.get_allocations() == 200);
    REQUIRE(alloc.get_bytes_allocated() == 200 * sizeof(int64_t));

    // Clean up
    for (int64_t* p : ptrs) {
      alloc.deallocate(p, 1);
    }

    REQUIRE(alloc.get_deallocations() == 200);
    REQUIRE(alloc.get_current_usage() == 0);
  }

  SECTION("Peak usage tracking") {
    std::vector<int64_t*> ptrs;

    // Allocate 20 blocks
    for (int i = 0; i < 20; ++i) {
      ptrs.push_back(alloc.allocate(1));
    }
    size_t peak1 = alloc.get_peak_usage();
    REQUIRE(peak1 == 20 * sizeof(int64_t));

    // Deallocate 10 blocks
    for (int i = 0; i < 10; ++i) {
      alloc.deallocate(ptrs[i], 1);
    }
    size_t usage_after_dealloc = alloc.get_current_usage();
    REQUIRE(usage_after_dealloc == 10 * sizeof(int64_t));
    REQUIRE(alloc.get_peak_usage() == peak1);  // Peak unchanged

    // Allocate 15 more blocks (net usage = 25)
    for (int i = 0; i < 15; ++i) {
      ptrs.push_back(alloc.allocate(1));
    }
    size_t peak2 = alloc.get_peak_usage();
    REQUIRE(peak2 == 25 * sizeof(int64_t));
    REQUIRE(peak2 > peak1);

    // Clean up
    for (size_t i = 10; i < ptrs.size(); ++i) {
      alloc.deallocate(ptrs[i], 1);
    }
  }
}
#endif
