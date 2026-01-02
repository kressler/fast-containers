// Copyright (c) 2025 Bryan Kressler
//
// SPDX-License-Identifier: BSD-3-Clause

#include <benchmark/benchmark.h>

#include <vector>

// Sample benchmark to verify Google Benchmark integration
static void BM_VectorPushBack(benchmark::State& state) {
  for (auto _ : state) {
    std::vector<int> v;
    for (int i = 0; i < state.range(0); ++i) {
      v.push_back(i);
    }
    benchmark::DoNotOptimize(v.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_VectorPushBack)->Range(8, 8 << 10);

// Sample benchmark for vector reserve
static void BM_VectorReserve(benchmark::State& state) {
  for (auto _ : state) {
    std::vector<int> v;
    v.reserve(state.range(0));
    for (int i = 0; i < state.range(0); ++i) {
      v.push_back(i);
    }
    benchmark::DoNotOptimize(v.data());
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_VectorReserve)->Range(8, 8 << 10);

BENCHMARK_MAIN();
