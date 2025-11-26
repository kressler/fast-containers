#include <benchmark/benchmark.h>

#include <random>

#include "../btree.hpp"
#include "../hugepage_allocator.hpp"

using namespace fast_containers;

// Benchmark configuration
constexpr size_t TREE_SIZE = 100000;  // 100K elements
constexpr size_t NODE_SIZE = 64;      // Node size
constexpr size_t POOL_SIZE_MB = 256;  // 256MB pool for hugepage allocator

// Random number generator for benchmark data
std::mt19937_64 rng(12345);

// Benchmark: Insert with std::allocator
static void BM_BTree_StdAllocator_Insert(benchmark::State& state) {
  std::uniform_int_distribution<int64_t> dist(0, TREE_SIZE * 10);

  for (auto _ : state) {
    state.PauseTiming();
    btree<int64_t, int64_t, NODE_SIZE, NODE_SIZE> tree;
    state.ResumeTiming();

    for (size_t i = 0; i < TREE_SIZE; ++i) {
      int64_t key = dist(rng);
      tree.insert(key, key * 2);
    }

    benchmark::DoNotOptimize(tree);
  }

  state.SetItemsProcessed(state.iterations() * TREE_SIZE);
}
BENCHMARK(BM_BTree_StdAllocator_Insert);

// Benchmark: Insert with HugePageAllocator
static void BM_BTree_HugePageAllocator_Insert(benchmark::State& state) {
  std::uniform_int_distribution<int64_t> dist(0, TREE_SIZE * 10);

  for (auto _ : state) {
    state.PauseTiming();
    using TreeType =
        btree<int64_t, int64_t, NODE_SIZE, NODE_SIZE, std::less<int64_t>,
              SearchMode::Binary, MoveMode::SIMD,
              HugePageAllocator<std::pair<int64_t, int64_t>>>;
    HugePageAllocator<std::pair<int64_t, int64_t>> alloc(
        POOL_SIZE_MB * 1024 * 1024, false);
    TreeType tree(alloc);
    state.ResumeTiming();

    for (size_t i = 0; i < TREE_SIZE; ++i) {
      int64_t key = dist(rng);
      tree.insert(key, key * 2);
    }

    benchmark::DoNotOptimize(tree);
  }

  state.SetItemsProcessed(state.iterations() * TREE_SIZE);
}
BENCHMARK(BM_BTree_HugePageAllocator_Insert);

// Benchmark: Random lookup with std::allocator
static void BM_BTree_StdAllocator_Lookup(benchmark::State& state) {
  // Pre-populate tree
  btree<int64_t, int64_t, NODE_SIZE, NODE_SIZE> tree;
  std::uniform_int_distribution<int64_t> insert_dist(0, TREE_SIZE * 10);
  for (size_t i = 0; i < TREE_SIZE; ++i) {
    tree.insert(insert_dist(rng), i);
  }

  std::uniform_int_distribution<int64_t> lookup_dist(0, TREE_SIZE * 10);

  for (auto _ : state) {
    for (size_t i = 0; i < 10000; ++i) {
      int64_t key = lookup_dist(rng);
      auto it = tree.find(key);
      benchmark::DoNotOptimize(it);
    }
  }

  state.SetItemsProcessed(state.iterations() * 10000);
}
BENCHMARK(BM_BTree_StdAllocator_Lookup);

// Benchmark: Random lookup with HugePageAllocator
static void BM_BTree_HugePageAllocator_Lookup(benchmark::State& state) {
  // Pre-populate tree
  using TreeType = btree<int64_t, int64_t, NODE_SIZE, NODE_SIZE,
                         std::less<int64_t>, SearchMode::Binary, MoveMode::SIMD,
                         HugePageAllocator<std::pair<int64_t, int64_t>>>;
  HugePageAllocator<std::pair<int64_t, int64_t>> alloc(
      POOL_SIZE_MB * 1024 * 1024, false);
  TreeType tree(alloc);

  std::uniform_int_distribution<int64_t> insert_dist(0, TREE_SIZE * 10);
  for (size_t i = 0; i < TREE_SIZE; ++i) {
    tree.insert(insert_dist(rng), i);
  }

  std::uniform_int_distribution<int64_t> lookup_dist(0, TREE_SIZE * 10);

  for (auto _ : state) {
    for (size_t i = 0; i < 10000; ++i) {
      int64_t key = lookup_dist(rng);
      auto it = tree.find(key);
      benchmark::DoNotOptimize(it);
    }
  }

  state.SetItemsProcessed(state.iterations() * 10000);
}
BENCHMARK(BM_BTree_HugePageAllocator_Lookup);

// Benchmark: Sequential iteration with std::allocator
static void BM_BTree_StdAllocator_Iteration(benchmark::State& state) {
  // Pre-populate tree with sequential keys
  btree<int64_t, int64_t, NODE_SIZE, NODE_SIZE> tree;
  for (size_t i = 0; i < TREE_SIZE; ++i) {
    tree.insert(i, i * 2);
  }

  for (auto _ : state) {
    int64_t sum = 0;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
      sum += it->second;
    }
    benchmark::DoNotOptimize(sum);
  }

  state.SetItemsProcessed(state.iterations() * TREE_SIZE);
}
BENCHMARK(BM_BTree_StdAllocator_Iteration);

// Benchmark: Sequential iteration with HugePageAllocator
static void BM_BTree_HugePageAllocator_Iteration(benchmark::State& state) {
  // Pre-populate tree with sequential keys
  using TreeType = btree<int64_t, int64_t, NODE_SIZE, NODE_SIZE,
                         std::less<int64_t>, SearchMode::Binary, MoveMode::SIMD,
                         HugePageAllocator<std::pair<int64_t, int64_t>>>;
  HugePageAllocator<std::pair<int64_t, int64_t>> alloc(
      POOL_SIZE_MB * 1024 * 1024, false);
  TreeType tree(alloc);

  for (size_t i = 0; i < TREE_SIZE; ++i) {
    tree.insert(i, i * 2);
  }

  for (auto _ : state) {
    int64_t sum = 0;
    for (auto it = tree.begin(); it != tree.end(); ++it) {
      sum += it->second;
    }
    benchmark::DoNotOptimize(sum);
  }

  state.SetItemsProcessed(state.iterations() * TREE_SIZE);
}
BENCHMARK(BM_BTree_HugePageAllocator_Iteration);

// Benchmark: Mixed workload (insert + lookup + erase) with std::allocator
static void BM_BTree_StdAllocator_Mixed(benchmark::State& state) {
  std::uniform_int_distribution<int64_t> dist(0, TREE_SIZE * 2);
  std::uniform_int_distribution<int> op_dist(0,
                                             2);  // 0=insert, 1=lookup, 2=erase

  for (auto _ : state) {
    state.PauseTiming();
    btree<int64_t, int64_t, NODE_SIZE, NODE_SIZE> tree;
    // Pre-populate with half the elements
    for (size_t i = 0; i < TREE_SIZE / 2; ++i) {
      tree.insert(i, i);
    }
    state.ResumeTiming();

    size_t ops = 0;
    for (size_t i = 0; i < 50000; ++i) {
      int op = op_dist(rng);
      int64_t key = dist(rng);

      if (op == 0) {
        tree.insert(key, key * 2);
      } else if (op == 1) {
        auto it = tree.find(key);
        benchmark::DoNotOptimize(it);
      } else {
        tree.erase(key);
      }
      ++ops;
    }

    benchmark::DoNotOptimize(tree);
    state.SetItemsProcessed(ops);
  }
}
BENCHMARK(BM_BTree_StdAllocator_Mixed);

// Benchmark: Mixed workload with HugePageAllocator
static void BM_BTree_HugePageAllocator_Mixed(benchmark::State& state) {
  std::uniform_int_distribution<int64_t> dist(0, TREE_SIZE * 2);
  std::uniform_int_distribution<int> op_dist(0, 2);

  for (auto _ : state) {
    state.PauseTiming();
    using TreeType =
        btree<int64_t, int64_t, NODE_SIZE, NODE_SIZE, std::less<int64_t>,
              SearchMode::Binary, MoveMode::SIMD,
              HugePageAllocator<std::pair<int64_t, int64_t>>>;
    HugePageAllocator<std::pair<int64_t, int64_t>> alloc(
        POOL_SIZE_MB * 1024 * 1024, false);
    TreeType tree(alloc);

    // Pre-populate with half the elements
    for (size_t i = 0; i < TREE_SIZE / 2; ++i) {
      tree.insert(i, i);
    }
    state.ResumeTiming();

    size_t ops = 0;
    for (size_t i = 0; i < 50000; ++i) {
      int op = op_dist(rng);
      int64_t key = dist(rng);

      if (op == 0) {
        tree.insert(key, key * 2);
      } else if (op == 1) {
        auto it = tree.find(key);
        benchmark::DoNotOptimize(it);
      } else {
        tree.erase(key);
      }
      ++ops;
    }

    benchmark::DoNotOptimize(tree);
    state.SetItemsProcessed(ops);
  }
}
BENCHMARK(BM_BTree_HugePageAllocator_Mixed);

BENCHMARK_MAIN();
