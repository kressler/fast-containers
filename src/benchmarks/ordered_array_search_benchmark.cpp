#include <benchmark/benchmark.h>

#include <random>
#include <string>

#include "../ordered_array.hpp"

using namespace fast_containers;

// Benchmark insert operations with binary search
template <std::size_t Size>
static void BM_OrderedArray_Insert_Binary(benchmark::State& state) {
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(1, 100000);

  for (auto _ : state) {
    ordered_array<int, int, Size, SearchMode::Binary> arr;
    for (std::size_t i = 0; i < Size; ++i) {
      arr.insert(dist(rng), i);
    }
    benchmark::DoNotOptimize(arr);
  }
  state.SetItemsProcessed(state.iterations() * Size);
}

// Benchmark insert operations with linear search
template <std::size_t Size>
static void BM_OrderedArray_Insert_Linear(benchmark::State& state) {
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(1, 100000);

  for (auto _ : state) {
    ordered_array<int, int, Size, SearchMode::Linear> arr;
    for (std::size_t i = 0; i < Size; ++i) {
      arr.insert(dist(rng), i);
    }
    benchmark::DoNotOptimize(arr);
  }
  state.SetItemsProcessed(state.iterations() * Size);
}

// Benchmark find operations with binary search
template <std::size_t Size>
static void BM_OrderedArray_Find_Binary(benchmark::State& state) {
  ordered_array<int, int, Size, SearchMode::Binary> arr;
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(1, 100000);

  // Pre-populate the array
  std::vector<int> keys;
  for (std::size_t i = 0; i < Size; ++i) {
    int key = dist(rng);
    arr.insert(key, i);
    keys.push_back(key);
  }

  std::size_t idx = 0;
  for (auto _ : state) {
    auto it = arr.find(keys[idx % Size]);
    benchmark::DoNotOptimize(it);
    ++idx;
  }
  state.SetItemsProcessed(state.iterations());
}

// Benchmark find operations with linear search
template <std::size_t Size>
static void BM_OrderedArray_Find_Linear(benchmark::State& state) {
  ordered_array<int, int, Size, SearchMode::Linear> arr;
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(1, 100000);

  // Pre-populate the array
  std::vector<int> keys;
  for (std::size_t i = 0; i < Size; ++i) {
    int key = dist(rng);
    arr.insert(key, i);
    keys.push_back(key);
  }

  std::size_t idx = 0;
  for (auto _ : state) {
    auto it = arr.find(keys[idx % Size]);
    benchmark::DoNotOptimize(it);
    ++idx;
  }
  state.SetItemsProcessed(state.iterations());
}

// Register benchmarks for small arrays (where linear might win)
BENCHMARK(BM_OrderedArray_Insert_Binary<8>);
BENCHMARK(BM_OrderedArray_Insert_Linear<8>);
BENCHMARK(BM_OrderedArray_Insert_Binary<16>);
BENCHMARK(BM_OrderedArray_Insert_Linear<16>);
BENCHMARK(BM_OrderedArray_Insert_Binary<32>);
BENCHMARK(BM_OrderedArray_Insert_Linear<32>);
BENCHMARK(BM_OrderedArray_Insert_Binary<64>);
BENCHMARK(BM_OrderedArray_Insert_Linear<64>);

BENCHMARK(BM_OrderedArray_Find_Binary<8>);
BENCHMARK(BM_OrderedArray_Find_Linear<8>);
BENCHMARK(BM_OrderedArray_Find_Binary<16>);
BENCHMARK(BM_OrderedArray_Find_Linear<16>);
BENCHMARK(BM_OrderedArray_Find_Binary<32>);
BENCHMARK(BM_OrderedArray_Find_Linear<32>);
BENCHMARK(BM_OrderedArray_Find_Binary<64>);
BENCHMARK(BM_OrderedArray_Find_Linear<64>);
BENCHMARK(BM_OrderedArray_Find_Binary<128>);
BENCHMARK(BM_OrderedArray_Find_Linear<128>);

BENCHMARK_MAIN();
