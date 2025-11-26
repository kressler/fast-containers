#include <benchmark/benchmark.h>

#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include "../ordered_array.hpp"

using namespace kressler::fast_containers;

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

// Benchmark remove + insert cycle on nearly-full array
// Pre-populates to Size-1 elements, then repeatedly removes and re-inserts
// the last element. Measures combined remove+insert cost without the ~300ns
// overhead of PauseTiming/ResumeTiming on every iteration.
template <std::size_t Size, SearchMode search_mode,
          MoveMode move_mode = MoveMode::SIMD>
static void BM_OrderedArray_RemoveInsert(benchmark::State& state) {
  auto keys = GenerateUniqueKeys<Size>();

  // Pre-populate once outside the timing loop
  ordered_array<int, int, Size, std::less<int>, search_mode, move_mode> arr;
  for (std::size_t i = 0; i < Size - 1; ++i) {
    arr.insert(keys[i], i);
  }

  for (auto _ : state) {
    // Remove the element we're about to insert (if it exists)
    if (arr.size() == Size) {
      arr.erase(keys[Size - 1]);
    }

    // Measure insert into array of size (Size-1)
    arr.insert(keys[Size - 1], Size - 1);
    benchmark::DoNotOptimize(arr);
  }
  state.SetItemsProcessed(state.iterations());
}

// Consolidated benchmark for find operations
template <std::size_t Size, SearchMode search_mode,
          MoveMode move_mode = MoveMode::SIMD>
static void BM_OrderedArray_Find(benchmark::State& state) {
  // Pre-generate unique keys
  auto keys = GenerateUniqueKeys<Size>();

  // Pre-populate the array
  ordered_array<int, int, Size, std::less<int>, search_mode, move_mode> arr;
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

// Register remove+insert benchmarks (cycle on nearly-full arrays)
BENCHMARK(BM_OrderedArray_RemoveInsert<1, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_RemoveInsert<1, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_RemoveInsert<1, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_RemoveInsert<2, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_RemoveInsert<2, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_RemoveInsert<2, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_RemoveInsert<4, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_RemoveInsert<4, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_RemoveInsert<4, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_RemoveInsert<8, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_RemoveInsert<8, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_RemoveInsert<8, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_RemoveInsert<16, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_RemoveInsert<16, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_RemoveInsert<16, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_RemoveInsert<32, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_RemoveInsert<32, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_RemoveInsert<32, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_RemoveInsert<64, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_RemoveInsert<64, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_RemoveInsert<64, SearchMode::SIMD>);

// Register find benchmarks for various sizes
BENCHMARK(BM_OrderedArray_Find<8, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Find<8, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_Find<8, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_Find<16, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Find<16, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_Find<16, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_Find<32, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Find<32, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_Find<32, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_Find<64, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Find<64, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_Find<64, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_Find<128, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Find<128, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_Find<128, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_Find<256, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Find<256, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_Find<256, SearchMode::SIMD>);

// Non-power-of-2 benchmarks to exercise progressive SIMD fallback
// These sizes (2^n - 1) test all fallback paths: 8→4→2→1 for 32-bit keys
BENCHMARK(BM_OrderedArray_Find<7, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Find<7, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_Find<7, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_Find<15, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Find<15, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_Find<15, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_Find<31, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Find<31, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_Find<31, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_Find<63, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Find<63, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_Find<63, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_Find<127, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Find<127, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_Find<127, SearchMode::SIMD>);
BENCHMARK(BM_OrderedArray_Find<255, SearchMode::Binary>);
BENCHMARK(BM_OrderedArray_Find<255, SearchMode::Linear>);
BENCHMARK(BM_OrderedArray_Find<255, SearchMode::SIMD>);

// MoveMode comparison benchmarks: Standard vs SIMD data movement
// These benchmarks compare remove+insert performance with different move modes
BENCHMARK(
    BM_OrderedArray_RemoveInsert<1, SearchMode::Binary, MoveMode::Standard>);
BENCHMARK(BM_OrderedArray_RemoveInsert<1, SearchMode::Binary, MoveMode::SIMD>);
BENCHMARK(
    BM_OrderedArray_RemoveInsert<2, SearchMode::Binary, MoveMode::Standard>);
BENCHMARK(BM_OrderedArray_RemoveInsert<2, SearchMode::Binary, MoveMode::SIMD>);
BENCHMARK(
    BM_OrderedArray_RemoveInsert<4, SearchMode::Binary, MoveMode::Standard>);
BENCHMARK(BM_OrderedArray_RemoveInsert<4, SearchMode::Binary, MoveMode::SIMD>);
BENCHMARK(
    BM_OrderedArray_RemoveInsert<8, SearchMode::Binary, MoveMode::Standard>);
BENCHMARK(BM_OrderedArray_RemoveInsert<8, SearchMode::Binary, MoveMode::SIMD>);
BENCHMARK(
    BM_OrderedArray_RemoveInsert<16, SearchMode::Binary, MoveMode::Standard>);
BENCHMARK(BM_OrderedArray_RemoveInsert<16, SearchMode::Binary, MoveMode::SIMD>);
BENCHMARK(
    BM_OrderedArray_RemoveInsert<32, SearchMode::Binary, MoveMode::Standard>);
BENCHMARK(BM_OrderedArray_RemoveInsert<32, SearchMode::Binary, MoveMode::SIMD>);
BENCHMARK(
    BM_OrderedArray_RemoveInsert<64, SearchMode::Binary, MoveMode::Standard>);
BENCHMARK(BM_OrderedArray_RemoveInsert<64, SearchMode::Binary, MoveMode::SIMD>);
BENCHMARK(
    BM_OrderedArray_RemoveInsert<128, SearchMode::Binary, MoveMode::Standard>);
BENCHMARK(
    BM_OrderedArray_RemoveInsert<128, SearchMode::Binary, MoveMode::SIMD>);

BENCHMARK_MAIN();
