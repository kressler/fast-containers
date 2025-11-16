#include <benchmark/benchmark.h>

#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include "../ordered_array.hpp"

using namespace fast_containers;

// Generate unique random keys for benchmarking
template <std::size_t Size>
std::vector<int> GenerateUniqueKeys() {
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(1, 1000000);
  std::unordered_set<int> unique_keys;

  // Keep generating until we have enough unique keys
  while (unique_keys.size() < Size) {
    unique_keys.insert(dist(rng));
  }

  return std::vector<int>(unique_keys.begin(), unique_keys.end());
}

// Benchmark insert operations with binary search
template <std::size_t Size>
static void BM_OrderedArray_Insert_Binary(benchmark::State& state) {
  // Pre-generate unique keys outside the benchmark loop
  auto keys = GenerateUniqueKeys<Size>();

  for (auto _ : state) {
    ordered_array<int, int, Size, SearchMode::Binary> arr;
    for (std::size_t i = 0; i < Size; ++i) {
      arr.insert(keys[i], i);
    }
    benchmark::DoNotOptimize(arr);
  }
  state.SetItemsProcessed(state.iterations() * Size);
}

// Benchmark insert operations with linear search
template <std::size_t Size>
static void BM_OrderedArray_Insert_Linear(benchmark::State& state) {
  // Pre-generate unique keys outside the benchmark loop
  auto keys = GenerateUniqueKeys<Size>();

  for (auto _ : state) {
    ordered_array<int, int, Size, SearchMode::Linear> arr;
    for (std::size_t i = 0; i < Size; ++i) {
      arr.insert(keys[i], i);
    }
    benchmark::DoNotOptimize(arr);
  }
  state.SetItemsProcessed(state.iterations() * Size);
}

// Benchmark insert operations with SIMD search
template <std::size_t Size>
static void BM_OrderedArray_Insert_SIMD(benchmark::State& state) {
  // Pre-generate unique keys outside the benchmark loop
  auto keys = GenerateUniqueKeys<Size>();

  for (auto _ : state) {
    ordered_array<int, int, Size, SearchMode::SIMD> arr;
    for (std::size_t i = 0; i < Size; ++i) {
      arr.insert(keys[i], i);
    }
    benchmark::DoNotOptimize(arr);
  }
  state.SetItemsProcessed(state.iterations() * Size);
}

// Benchmark find operations with binary search
template <std::size_t Size>
static void BM_OrderedArray_Find_Binary(benchmark::State& state) {
  // Pre-generate unique keys
  auto keys = GenerateUniqueKeys<Size>();

  // Pre-populate the array
  ordered_array<int, int, Size, SearchMode::Binary> arr;
  for (std::size_t i = 0; i < Size; ++i) {
    arr.insert(keys[i], i);
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
  // Pre-generate unique keys
  auto keys = GenerateUniqueKeys<Size>();

  // Pre-populate the array
  ordered_array<int, int, Size, SearchMode::Linear> arr;
  for (std::size_t i = 0; i < Size; ++i) {
    arr.insert(keys[i], i);
  }

  std::size_t idx = 0;
  for (auto _ : state) {
    auto it = arr.find(keys[idx % Size]);
    benchmark::DoNotOptimize(it);
    ++idx;
  }
  state.SetItemsProcessed(state.iterations());
}

// Benchmark find operations with SIMD search
template <std::size_t Size>
static void BM_OrderedArray_Find_SIMD(benchmark::State& state) {
  // Pre-generate unique keys
  auto keys = GenerateUniqueKeys<Size>();

  // Pre-populate the array
  ordered_array<int, int, Size, SearchMode::SIMD> arr;
  for (std::size_t i = 0; i < Size; ++i) {
    arr.insert(keys[i], i);
  }

  std::size_t idx = 0;
  for (auto _ : state) {
    auto it = arr.find(keys[idx % Size]);
    benchmark::DoNotOptimize(it);
    ++idx;
  }
  state.SetItemsProcessed(state.iterations());
}

// Register benchmarks for small arrays (where linear/SIMD might win)
BENCHMARK(BM_OrderedArray_Insert_Binary<8>);
BENCHMARK(BM_OrderedArray_Insert_Linear<8>);
BENCHMARK(BM_OrderedArray_Insert_SIMD<8>);
BENCHMARK(BM_OrderedArray_Insert_Binary<16>);
BENCHMARK(BM_OrderedArray_Insert_Linear<16>);
BENCHMARK(BM_OrderedArray_Insert_SIMD<16>);
BENCHMARK(BM_OrderedArray_Insert_Binary<32>);
BENCHMARK(BM_OrderedArray_Insert_Linear<32>);
BENCHMARK(BM_OrderedArray_Insert_SIMD<32>);
BENCHMARK(BM_OrderedArray_Insert_Binary<64>);
BENCHMARK(BM_OrderedArray_Insert_Linear<64>);
BENCHMARK(BM_OrderedArray_Insert_SIMD<64>);

BENCHMARK(BM_OrderedArray_Find_Binary<8>);
BENCHMARK(BM_OrderedArray_Find_Linear<8>);
BENCHMARK(BM_OrderedArray_Find_SIMD<8>);
BENCHMARK(BM_OrderedArray_Find_Binary<16>);
BENCHMARK(BM_OrderedArray_Find_Linear<16>);
BENCHMARK(BM_OrderedArray_Find_SIMD<16>);
BENCHMARK(BM_OrderedArray_Find_Binary<32>);
BENCHMARK(BM_OrderedArray_Find_Linear<32>);
BENCHMARK(BM_OrderedArray_Find_SIMD<32>);
BENCHMARK(BM_OrderedArray_Find_Binary<64>);
BENCHMARK(BM_OrderedArray_Find_Linear<64>);
BENCHMARK(BM_OrderedArray_Find_SIMD<64>);
BENCHMARK(BM_OrderedArray_Find_Binary<128>);
BENCHMARK(BM_OrderedArray_Find_Linear<128>);
BENCHMARK(BM_OrderedArray_Find_SIMD<128>);
BENCHMARK(BM_OrderedArray_Find_Binary<256>);
BENCHMARK(BM_OrderedArray_Find_Linear<256>);
BENCHMARK(BM_OrderedArray_Find_SIMD<256>);

// Non-power-of-2 benchmarks to exercise progressive SIMD fallback
// These sizes (2^n - 1) test all fallback paths: 8→4→2→1 for 32-bit keys
BENCHMARK(BM_OrderedArray_Find_Binary<7>);
BENCHMARK(BM_OrderedArray_Find_Linear<7>);
BENCHMARK(BM_OrderedArray_Find_SIMD<7>);
BENCHMARK(BM_OrderedArray_Find_Binary<15>);
BENCHMARK(BM_OrderedArray_Find_Linear<15>);
BENCHMARK(BM_OrderedArray_Find_SIMD<15>);
BENCHMARK(BM_OrderedArray_Find_Binary<31>);
BENCHMARK(BM_OrderedArray_Find_Linear<31>);
BENCHMARK(BM_OrderedArray_Find_SIMD<31>);
BENCHMARK(BM_OrderedArray_Find_Binary<63>);
BENCHMARK(BM_OrderedArray_Find_Linear<63>);
BENCHMARK(BM_OrderedArray_Find_SIMD<63>);
BENCHMARK(BM_OrderedArray_Find_Binary<127>);
BENCHMARK(BM_OrderedArray_Find_Linear<127>);
BENCHMARK(BM_OrderedArray_Find_SIMD<127>);
BENCHMARK(BM_OrderedArray_Find_Binary<255>);
BENCHMARK(BM_OrderedArray_Find_Linear<255>);
BENCHMARK(BM_OrderedArray_Find_SIMD<255>);

// MoveMode comparison benchmarks: Standard vs SIMD data movement
// These benchmarks compare insert performance with different move modes
template <std::size_t Size>
static void BM_OrderedArray_Insert_Binary_StandardMove(
    benchmark::State& state) {
  auto keys = GenerateUniqueKeys<Size>();

  for (auto _ : state) {
    ordered_array<int, int, Size, SearchMode::Binary, MoveMode::Standard> arr;
    for (std::size_t i = 0; i < Size; ++i) {
      arr.insert(keys[i], i);
    }
    benchmark::DoNotOptimize(arr);
  }
  state.SetItemsProcessed(state.iterations() * Size);
}

template <std::size_t Size>
static void BM_OrderedArray_Insert_Binary_SIMDMove(benchmark::State& state) {
  auto keys = GenerateUniqueKeys<Size>();

  for (auto _ : state) {
    ordered_array<int, int, Size, SearchMode::Binary, MoveMode::SIMD> arr;
    for (std::size_t i = 0; i < Size; ++i) {
      arr.insert(keys[i], i);
    }
    benchmark::DoNotOptimize(arr);
  }
  state.SetItemsProcessed(state.iterations() * Size);
}

// Register MoveMode comparison benchmarks
BENCHMARK(BM_OrderedArray_Insert_Binary_StandardMove<8>);
BENCHMARK(BM_OrderedArray_Insert_Binary_SIMDMove<8>);
BENCHMARK(BM_OrderedArray_Insert_Binary_StandardMove<16>);
BENCHMARK(BM_OrderedArray_Insert_Binary_SIMDMove<16>);
BENCHMARK(BM_OrderedArray_Insert_Binary_StandardMove<32>);
BENCHMARK(BM_OrderedArray_Insert_Binary_SIMDMove<32>);
BENCHMARK(BM_OrderedArray_Insert_Binary_StandardMove<64>);
BENCHMARK(BM_OrderedArray_Insert_Binary_SIMDMove<64>);
BENCHMARK(BM_OrderedArray_Insert_Binary_StandardMove<128>);
BENCHMARK(BM_OrderedArray_Insert_Binary_SIMDMove<128>);

BENCHMARK_MAIN();
