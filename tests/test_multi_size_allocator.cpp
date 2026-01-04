// Copyright (c) 2025 Bryan Kressler
//
// SPDX-License-Identifier: BSD-3-Clause

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <fast_containers/multi_size_hugepage_allocator.hpp>
#include <fast_containers/multi_size_hugepage_pool.hpp>
#include <memory>
#include <vector>

using namespace kressler::fast_containers;

TEST_CASE("MultiSizeHugePagePool size class calculation", "[multi_size_pool]") {
  SECTION("Zero bytes") {
    REQUIRE(MultiSizeHugePagePool::get_size_class(0) == 0);
  }

  SECTION("Small sizes (0-512 bytes) round to 64-byte boundary") {
    REQUIRE(MultiSizeHugePagePool::get_size_class(1) == 64);
    REQUIRE(MultiSizeHugePagePool::get_size_class(63) == 64);
    REQUIRE(MultiSizeHugePagePool::get_size_class(64) == 64);
    REQUIRE(MultiSizeHugePagePool::get_size_class(65) == 128);
    REQUIRE(MultiSizeHugePagePool::get_size_class(128) == 128);
    REQUIRE(MultiSizeHugePagePool::get_size_class(129) == 192);
    REQUIRE(MultiSizeHugePagePool::get_size_class(256) == 256);
    REQUIRE(MultiSizeHugePagePool::get_size_class(320) == 320);
    REQUIRE(MultiSizeHugePagePool::get_size_class(512) == 512);
  }

  SECTION("Medium sizes (513-2048 bytes) round to 256-byte boundary") {
    REQUIRE(MultiSizeHugePagePool::get_size_class(513) == 768);
    REQUIRE(MultiSizeHugePagePool::get_size_class(768) == 768);
    REQUIRE(MultiSizeHugePagePool::get_size_class(769) == 1024);
    REQUIRE(MultiSizeHugePagePool::get_size_class(1024) == 1024);
    REQUIRE(MultiSizeHugePagePool::get_size_class(1025) == 1280);
    REQUIRE(MultiSizeHugePagePool::get_size_class(2048) == 2048);
  }

  SECTION("Large sizes (2049+ bytes) round to power of 2") {
    REQUIRE(MultiSizeHugePagePool::get_size_class(2049) == 4096);
    REQUIRE(MultiSizeHugePagePool::get_size_class(4096) == 4096);
    REQUIRE(MultiSizeHugePagePool::get_size_class(4097) == 8192);
    REQUIRE(MultiSizeHugePagePool::get_size_class(8192) == 8192);
    REQUIRE(MultiSizeHugePagePool::get_size_class(10000) == 16384);
  }
}

TEST_CASE("MultiSizeHugePagePool basic allocation", "[multi_size_pool]") {
  MultiSizeHugePagePool pool(1UL * 1024 * 1024,  // 1MB per pool
                             false);             // Don't require hugepages

  SECTION("Allocate and deallocate small sizes") {
    void* p1 = pool.allocate(100, 8);
    REQUIRE(p1 != nullptr);

    void* p2 = pool.allocate(200, 8);
    REQUIRE(p2 != nullptr);
    REQUIRE(p2 != p1);

    pool.deallocate(p1, 100);
    pool.deallocate(p2, 200);

    // Should have created pools for size classes 128 and 256
    REQUIRE(pool.active_size_classes() == 2);
  }

  SECTION("Allocate same size multiple times") {
    std::vector<void*> ptrs;
    for (int i = 0; i < 10; ++i) {
      void* p = pool.allocate(128, 8);
      REQUIRE(p != nullptr);
      ptrs.push_back(p);
    }

    // All should use same size class (128 bytes)
    REQUIRE(pool.active_size_classes() == 1);

    for (void* p : ptrs) {
      pool.deallocate(p, 128);
    }
  }

  SECTION("Allocate different sizes") {
    void* p64 = pool.allocate(50, 8);    // → 64-byte pool
    void* p128 = pool.allocate(100, 8);  // → 128-byte pool
    void* p256 = pool.allocate(200, 8);  // → 256-byte pool
    void* p512 = pool.allocate(400, 8);  // → 512-byte pool

    REQUIRE(p64 != nullptr);
    REQUIRE(p128 != nullptr);
    REQUIRE(p256 != nullptr);
    REQUIRE(p512 != nullptr);

    // Should have 4 different size class pools
    REQUIRE(pool.active_size_classes() == 4);

    pool.deallocate(p64, 50);
    pool.deallocate(p128, 100);
    pool.deallocate(p256, 200);
    pool.deallocate(p512, 400);
  }

  SECTION("Reuse freed blocks") {
    void* p1 = pool.allocate(128, 8);
    REQUIRE(p1 != nullptr);

    pool.deallocate(p1, 128);

    // Allocate same size - should reuse from free list
    void* p2 = pool.allocate(128, 8);
    REQUIRE(p2 == p1);  // Should get same block back

    pool.deallocate(p2, 128);
  }
}

TEST_CASE("MultiSizeHugePageAllocator basic usage", "[multi_size_allocator]") {
  using Alloc = MultiSizeHugePageAllocator<int>;

  auto pool = std::make_shared<MultiSizeHugePagePool>(
      1UL * 1024 * 1024,  // 1MB per pool
      false);             // Don't require hugepages

  Alloc alloc(pool);

  SECTION("Allocate and deallocate") {
    int* p = alloc.allocate(10);
    REQUIRE(p != nullptr);

    // Construct objects
    for (int i = 0; i < 10; ++i) {
      new (p + i) int(i);
    }

    // Verify values
    for (int i = 0; i < 10; ++i) {
      REQUIRE(p[i] == i);
    }

    // Note: No need to explicitly destroy trivial types like int

    alloc.deallocate(p, 10);
  }

  SECTION("Allocator equality") {
    Alloc alloc2(pool);
    REQUIRE(alloc == alloc2);  // Same pool

    auto pool2 = std::make_shared<MultiSizeHugePagePool>();
    Alloc alloc3(pool2);
    REQUIRE(alloc != alloc3);  // Different pool
  }

  SECTION("Rebind allocator") {
    using AllocDouble = MultiSizeHugePageAllocator<double>;
    AllocDouble alloc_double(alloc);

    REQUIRE(alloc.get_pool() == alloc_double.get_pool());
  }
}

TEST_CASE("MultiSizeHugePageAllocator with std::vector",
          "[multi_size_allocator]") {
  auto pool = std::make_shared<MultiSizeHugePagePool>(
      1UL * 1024 * 1024,  // 1MB per pool
      false);             // Don't require hugepages

  using Alloc = MultiSizeHugePageAllocator<int>;
  using Vec = std::vector<int, Alloc>;

  SECTION("Basic vector operations") {
    Vec vec{Alloc(pool)};

    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    REQUIRE(vec.size() == 3);
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 3);
  }

  SECTION("Vector resize") {
    Vec vec{Alloc(pool)};

    vec.resize(100);
    REQUIRE(vec.size() == 100);

    for (size_t i = 0; i < vec.size(); ++i) {
      vec[i] = static_cast<int>(i);
    }

    for (size_t i = 0; i < vec.size(); ++i) {
      REQUIRE(vec[i] == static_cast<int>(i));
    }
  }
}

TEST_CASE("MultiSizeHugePageAllocator with large allocations",
          "[multi_size_allocator]") {
  auto pool = std::make_shared<MultiSizeHugePagePool>(
      10UL * 1024 * 1024,  // 10MB per pool
      false);              // Don't require hugepages

  using Alloc = MultiSizeHugePageAllocator<std::byte>;

  Alloc alloc(pool);

  SECTION("Multiple large allocations use different size classes") {
    // These should map to different size classes
    auto* p1 = alloc.allocate(600);    // → 768-byte pool
    auto* p2 = alloc.allocate(1000);   // → 1024-byte pool
    auto* p3 = alloc.allocate(3000);   // → 4096-byte pool
    auto* p4 = alloc.allocate(10000);  // → 16384-byte pool

    REQUIRE(p1 != nullptr);
    REQUIRE(p2 != nullptr);
    REQUIRE(p3 != nullptr);
    REQUIRE(p4 != nullptr);

    // Should have 4 active size classes
    REQUIRE(pool->active_size_classes() == 4);

    alloc.deallocate(p1, 600);
    alloc.deallocate(p2, 1000);
    alloc.deallocate(p3, 3000);
    alloc.deallocate(p4, 10000);
  }
}

TEST_CASE("make_multi_size_hugepage_allocator helper",
          "[multi_size_allocator]") {
  SECTION("Default parameters") {
    auto alloc =
        make_multi_size_hugepage_allocator<int>(1UL * 1024 * 1024,  // 1MB
                                                false);  // No hugepages

    int* p = alloc.allocate(10);
    REQUIRE(p != nullptr);

    alloc.deallocate(p, 10);
  }

  SECTION("Custom parameters") {
    auto alloc = make_multi_size_hugepage_allocator<double>(
        2UL * 1024 * 1024,  // 2MB per pool
        false,              // No hugepages
        1UL * 1024 * 1024   // 1MB growth
    );

    double* p = alloc.allocate(100);
    REQUIRE(p != nullptr);

    alloc.deallocate(p, 100);
  }
}
