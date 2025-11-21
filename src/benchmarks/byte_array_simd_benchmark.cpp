#include <benchmark/benchmark.h>

#include <random>

#include "../benchmark_encoding.hpp"
#include "../btree.hpp"

using namespace fast_containers;

// Benchmark parameters
constexpr size_t TREE_SIZE = 100000;
constexpr size_t BATCH_SIZE = 10000;
constexpr size_t BATCHES = 10;

/**
 * Benchmark helper that performs a realistic btree workload:
 * 1. Pre-populate tree to TREE_SIZE
 * 2. Run BATCHES of: erase BATCH_SIZE, insert BATCH_SIZE, find all keys
 * 3. Measure total time
 */
template <typename Tree>
static void RunBTreeBenchmark(benchmark::State& state, Tree& tree,
                              const auto& keys) {
  std::mt19937 rng(42);

  // Pre-populate
  for (size_t i = 0; i < TREE_SIZE; ++i) {
    tree.insert({keys[i], i});
  }

  // Benchmark: Remove-Insert-Find cycles
  for (auto _ : state) {
    for (size_t batch = 0; batch < BATCHES; ++batch) {
      // Remove batch
      for (size_t i = 0; i < BATCH_SIZE; ++i) {
        tree.erase(keys[i]);
      }

      // Re-insert batch
      for (size_t i = 0; i < BATCH_SIZE; ++i) {
        tree.insert({keys[i], i});
      }

      // Find all keys (exercises internal node traversal!)
      for (size_t i = 0; i < TREE_SIZE; ++i) {
        benchmark::DoNotOptimize(tree.find(keys[i]));
      }
    }
  }

  state.SetItemsProcessed(state.iterations() * BATCHES *
                          (2 * BATCH_SIZE + TREE_SIZE));
}

// ============================================================================
// Native int64_t Benchmarks
// ============================================================================

static void BM_Native_Int64_Binary(benchmark::State& state) {
  btree<int64_t, size_t, 64, 64, SearchMode::Binary> tree;
  std::vector<int64_t> keys(TREE_SIZE);
  std::mt19937 rng(42);
  std::uniform_int_distribution<int64_t> dist;
  for (auto& k : keys)
    k = dist(rng);
  RunBTreeBenchmark(state, tree, keys);
}
BENCHMARK(BM_Native_Int64_Binary);

static void BM_Native_Int64_Linear(benchmark::State& state) {
  btree<int64_t, size_t, 64, 64, SearchMode::Linear> tree;
  std::vector<int64_t> keys(TREE_SIZE);
  std::mt19937 rng(42);
  std::uniform_int_distribution<int64_t> dist;
  for (auto& k : keys)
    k = dist(rng);
  RunBTreeBenchmark(state, tree, keys);
}
BENCHMARK(BM_Native_Int64_Linear);

static void BM_Native_Int64_SIMD(benchmark::State& state) {
  btree<int64_t, size_t, 64, 64, SearchMode::SIMD> tree;
  std::vector<int64_t> keys(TREE_SIZE);
  std::mt19937 rng(42);
  std::uniform_int_distribution<int64_t> dist;
  for (auto& k : keys)
    k = dist(rng);
  RunBTreeBenchmark(state, tree, keys);
}
BENCHMARK(BM_Native_Int64_SIMD);

// ============================================================================
// 16-Byte Encoded Byte Array Benchmarks
// ============================================================================

static void BM_ByteArray16_Binary(benchmark::State& state) {
  btree<std::array<std::byte, 16>, size_t, 64, 64, SearchMode::Binary> tree;
  std::vector<std::array<std::byte, 16>> keys(TREE_SIZE);
  std::mt19937 rng(42);
  std::uniform_int_distribution<int64_t> dist;
  for (auto& k : keys)
    k = encode_int64_pair(dist(rng), dist(rng));
  RunBTreeBenchmark(state, tree, keys);
}
BENCHMARK(BM_ByteArray16_Binary);

static void BM_ByteArray16_Linear(benchmark::State& state) {
  btree<std::array<std::byte, 16>, size_t, 64, 64, SearchMode::Linear> tree;
  std::vector<std::array<std::byte, 16>> keys(TREE_SIZE);
  std::mt19937 rng(42);
  std::uniform_int_distribution<int64_t> dist;
  for (auto& k : keys)
    k = encode_int64_pair(dist(rng), dist(rng));
  RunBTreeBenchmark(state, tree, keys);
}
BENCHMARK(BM_ByteArray16_Linear);

static void BM_ByteArray16_SIMD(benchmark::State& state) {
  btree<std::array<std::byte, 16>, size_t, 64, 64, SearchMode::SIMD> tree;
  std::vector<std::array<std::byte, 16>> keys(TREE_SIZE);
  std::mt19937 rng(42);
  std::uniform_int_distribution<int64_t> dist;
  for (auto& k : keys)
    k = encode_int64_pair(dist(rng), dist(rng));
  RunBTreeBenchmark(state, tree, keys);
}
BENCHMARK(BM_ByteArray16_SIMD);

// ============================================================================
// 32-Byte Encoded Byte Array Benchmarks
// ============================================================================

static void BM_ByteArray32_Binary(benchmark::State& state) {
  btree<std::array<std::byte, 32>, size_t, 64, 64, SearchMode::Binary> tree;
  std::vector<std::array<std::byte, 32>> keys(TREE_SIZE);
  std::mt19937 rng(42);
  std::uniform_int_distribution<int64_t> dist;
  for (auto& k : keys)
    k = encode_int64_quad(dist(rng), dist(rng), dist(rng), dist(rng));
  RunBTreeBenchmark(state, tree, keys);
}
BENCHMARK(BM_ByteArray32_Binary);

static void BM_ByteArray32_Linear(benchmark::State& state) {
  btree<std::array<std::byte, 32>, size_t, 64, 64, SearchMode::Linear> tree;
  std::vector<std::array<std::byte, 32>> keys(TREE_SIZE);
  std::mt19937 rng(42);
  std::uniform_int_distribution<int64_t> dist;
  for (auto& k : keys)
    k = encode_int64_quad(dist(rng), dist(rng), dist(rng), dist(rng));
  RunBTreeBenchmark(state, tree, keys);
}
BENCHMARK(BM_ByteArray32_Linear);

static void BM_ByteArray32_SIMD(benchmark::State& state) {
  btree<std::array<std::byte, 32>, size_t, 64, 64, SearchMode::SIMD> tree;
  std::vector<std::array<std::byte, 32>> keys(TREE_SIZE);
  std::mt19937 rng(42);
  std::uniform_int_distribution<int64_t> dist;
  for (auto& k : keys)
    k = encode_int64_quad(dist(rng), dist(rng), dist(rng), dist(rng));
  RunBTreeBenchmark(state, tree, keys);
}
BENCHMARK(BM_ByteArray32_SIMD);

BENCHMARK_MAIN();
