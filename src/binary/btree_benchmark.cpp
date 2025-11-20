#include <absl/container/btree_map.h>
#include <benchmark/benchmark.h>

#include <chrono>
#include <iostream>
#include <lyra/lyra.hpp>
#include <map>
#include <random>
#include <unordered_set>

#include "../btree.hpp"

struct TimingStats {
  uint64_t insert_time{0};
  uint64_t find_time{0};
  uint64_t erase_time{0};
  uint64_t iterate_time{0};

  TimingStats& operator+=(const TimingStats& rhs) {
    insert_time += rhs.insert_time;
    find_time += rhs.find_time;
    erase_time += rhs.erase_time;
    iterate_time += rhs.iterate_time;
    return *this;  // return the result by reference
  }
};

template <typename T>
TimingStats run_benchmark(T& tree, uint64_t seed, size_t iterations,
                          size_t tree_size, size_t batches, size_t batch_size) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(),
                                          std::numeric_limits<int>::max());
  TimingStats stats;
  unsigned int dummy;
  std::unordered_set<int> keys;

  auto insert = [&]() -> void {
    int val = dist(rng);
    keys.insert(val);
    uint64_t start = __rdtscp(&dummy);
    tree.insert({val, {}});
    uint64_t stop = __rdtscp(&dummy);
    stats.insert_time += stop - start;
  };

  auto find = [&](const int key) -> void {
    uint64_t start = __rdtscp(&dummy);
    benchmark::DoNotOptimize(tree.find(key));
    uint64_t stop = __rdtscp(&dummy);
    stats.find_time += stop - start;
  };

  auto erase = [&]() -> void {
    int key = *keys.begin();
    keys.erase(key);
    uint64_t start = __rdtscp(&dummy);
    benchmark::DoNotOptimize(tree.find(key));
    uint64_t stop = __rdtscp(&dummy);
    stats.erase_time += stop - start;
  };

  auto iterate = [&]() -> void {
    uint64_t start = __rdtscp(&dummy);
    auto it = tree.begin();
    while (it != tree.end()) {
      benchmark::DoNotOptimize(it++);
    }
    uint64_t stop = __rdtscp(&dummy);
    stats.iterate_time += stop - start;
  };

  while (tree.size() < tree_size) {
    insert();
  }

  for (const auto& key : keys) {
    find(key);
  }

  for (size_t batch = 0; batch < batches; ++batch) {
    for (size_t i = 0; i < batch_size; ++i) {
      erase();
    }
    for (size_t i = 0; i < batch_size; ++i) {
      insert();
    }
    for (const auto& key : keys) {
      find(key);
    }
  }

  iterate();

  return stats;
}

int main(int argc, char** argv) {
  bool show_help = false;
  uint64_t seed = 42;
  size_t target_iterations = 1000;
  size_t tree_size = 1000000;
  size_t batches = 100;
  size_t batch_size = 1000;
  std::vector<std::string> names;
  std::unordered_map<std::string, TimingStats> results;

  // Define command line interface
  auto cli = lyra::cli() | lyra::help(show_help) |
             lyra::opt(seed, "seed")["-d"]["--seed"](
                 "Random seed (defaults to time since epoch") |
             lyra::opt(target_iterations, "iterations")["-i"]["--iterations"](
                 "Iterations to run") |
             lyra::opt(tree_size, "min_keys")["-t"]["--tree-size"](
                 "Minimum keys to target in tree") |
             lyra::opt(batches, "batches")["-b"]["--batches"](
                 "Number of erase/insert batches to run") |
             lyra::opt(batch_size, "batch_size")["-s"]["--batch-size"](
                 "Size of an erase/insert batch") |
             lyra::arg(names, "names")("Name of the benchmark to run");

  // Parse command line
  auto result = cli.parse({argc, argv});
  if (!result) {
    std::cerr << "Error in command line: " << result.message() << std::endl;
    exit(1);
  }

  // Show help if requested
  if (show_help) {
    std::cout << cli << std::endl;
    return 0;
  }

  for (size_t iter = 0; iter < target_iterations; ++iter) {
    if (iter % 100 == 0)
      std::cout << "Iteration " << iter << std::endl;
    for (const auto& name : names) {
      /* Large payloads */
      if (name == "btree_64_64_256_simd_simd") {
        fast_containers::btree<int, std::array<std::byte, 256>, 64, 64,
                               fast_containers::SearchMode::SIMD>
            tree;
        results[name] += run_benchmark(tree, seed, target_iterations, tree_size,
                                       batches, batch_size);
      } else if (name == "btree_16_128_256_simd_simd") {
        fast_containers::btree<int, std::array<std::byte, 256>, 16, 128,
                               fast_containers::SearchMode::SIMD>
            tree;
        results[name] += run_benchmark(tree, seed, target_iterations, tree_size,
                                       batches, batch_size);
      } else if (name == "btree_16_128_256_bin_simd") {
        fast_containers::btree<int, std::array<std::byte, 256>, 16, 128,
                               fast_containers::SearchMode::Binary>
            tree;
        results[name] += run_benchmark(tree, seed, target_iterations, tree_size,
                                       batches, batch_size);
      } else if (name == "absl_256") {
        absl::btree_map<int, std::array<std::byte, 256>> tree;
        results[name] += run_benchmark(tree, seed, target_iterations, tree_size,
                                       batches, batch_size);
        /* Small payloads */
      } else if (name == "btree_64_64_8_simd_simd") {
        fast_containers::btree<int, std::array<std::byte, 8>, 64, 64,
                               fast_containers::SearchMode::SIMD>
            tree;
        results[name] += run_benchmark(tree, seed, target_iterations, tree_size,
                                       batches, batch_size);
      } else if (name == "btree_64_128_8_simd_simd") {
        fast_containers::btree<int, std::array<std::byte, 8>, 64, 128,
                               fast_containers::SearchMode::SIMD>
            tree;
        results[name] += run_benchmark(tree, seed, target_iterations, tree_size,
                                       batches, batch_size);
      } else if (name == "btree_64_128_8_bin_simd") {
        fast_containers::btree<int, std::array<std::byte, 256>, 64, 128,
                               fast_containers::SearchMode::Binary>
            tree;
        results[name] += run_benchmark(tree, seed, target_iterations, tree_size,
                                       batches, batch_size);
      } else if (name == "absl_8") {
        absl::btree_map<int, std::array<std::byte, 8>> tree;
        results[name] += run_benchmark(tree, seed, target_iterations, tree_size,
                                       batches, batch_size);
      } else {
        std::cerr << "Unknown benchmark: " << name << std::endl;
      }
    }
  }

  for (const auto& name : names) {
    const auto& stats = results.at(name);
    std::cout << name << ": Insert: " << stats.insert_time
              << ", Find: " << stats.find_time
              << ", Erase: " << stats.erase_time
              << ", Iterate: " << stats.iterate_time << std::endl;
  }
}