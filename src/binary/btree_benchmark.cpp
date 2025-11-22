#include <absl/container/btree_map.h>
#include <benchmark/benchmark.h>

#include <chrono>
#include <iostream>
#include <lyra/lyra.hpp>
#include <map>
#include <print>
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

template <size_t N>
struct std::hash<std::array<int64_t, N>> {
  std::size_t operator()(const std::array<int64_t, N>& k) const {
    using std::hash;
    using std::size_t;
    size_t h = 0;
    for (size_t i = 0; i < N; ++i) {
      h ^= hash<int64_t>()(k[i]);
    }
    return h;
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
  std::unordered_set<typename T::key_type> keys{};

  auto insert = [&]() -> void {
    typename T::key_type key;
    if constexpr (std::is_same_v<typename T::key_type, int64_t>) {
      key = dist(rng);
    } else {
      for (int i = 0; i < key.size(); ++i) {
        key[i] = dist(rng);
      }
    }
    keys.insert(key);
    uint64_t start = __rdtscp(&dummy);
    tree.insert({key, {}});
    uint64_t stop = __rdtscp(&dummy);
    stats.insert_time += stop - start;
  };

  auto find = [&](const typename T::key_type& key) -> void {
    uint64_t start = __rdtscp(&dummy);
    benchmark::DoNotOptimize(tree.find(key));
    uint64_t stop = __rdtscp(&dummy);
    stats.find_time += stop - start;
  };

  auto erase = [&]() -> void {
    auto key = *keys.begin();
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

using LambdaType = std::function<TimingStats()>;

int main(int argc, char** argv) {
  bool show_help = false;
  uint64_t seed = 42;
  size_t target_iterations = 1000;
  size_t tree_size = 1000000;
  size_t batches = 100;
  size_t batch_size = 1000;
  std::vector<std::string> names;
  std::unordered_map<std::string, TimingStats> results;

  auto benchmarker = [&](auto tree) -> TimingStats {
    return run_benchmark(tree, seed, target_iterations, tree_size, batches,
                         batch_size);
  };

  std::map<std::string, LambdaType> benchmarkers{
      /* 256 byte values */
      {"btree_8_256_64_64_simd_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 64, 64,
                                    fast_containers::SearchMode::SIMD>{});
       }},
      {"btree_8_256_16_128_simd_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 16,
                                    128, fast_containers::SearchMode::SIMD>{});
       }},
      {"btree_8_256_16_128_binary_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 16,
                                    128,
                                    fast_containers::SearchMode::Binary>{});
       }},
      {"btree_8_256_16_128_linear_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 16,
                                    128,
                                    fast_containers::SearchMode::Linear>{});
       }},
      {"absl_8_256",
       [&]() -> TimingStats {
         return benchmarker(
             absl::btree_map<std::int64_t, std::array<std::byte, 256>>{});
       }},
      {"map_8_256",
       [&]() -> TimingStats {
         return benchmarker(
             std::map<std::int64_t, std::array<std::byte, 256>>{});
       }},
      {"unordered_map_8_256",
       [&]() -> TimingStats {
         return benchmarker(
             std::unordered_map<std::int64_t, std::array<std::byte, 256>>{});
       }},

      {"btree_16_256_16_128_linear_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<std::array<std::int64_t, 2>,
                                    std::array<std::byte, 256>, 16, 128,
                                    fast_containers::SearchMode::Linear>{});
       }},
      {"btree_16_256_16_128_binary_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 256>, 16, 128,
                                    fast_containers::SearchMode::Binary>{});
       }},
      {"absl_16_256",
       [&]() -> TimingStats {
         return benchmarker(absl::btree_map<std::array<std::int64_t, 2>,
                                            std::array<std::byte, 256>>{});
       }},
      {"map_16_256",
       [&]() -> TimingStats {
         return benchmarker(std::map<std::array<std::int64_t, 2>,
                                     std::array<std::byte, 256>>{});
       }},
      {"unordered_map_16_256",
       [&]() -> TimingStats {
         return benchmarker(std::unordered_map<std::array<std::int64_t, 2>,
                                               std::array<std::byte, 256>>{});
       }},

      {"btree_32_256_16_128_linear_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 256>, 16, 128,
                                    fast_containers::SearchMode::Linear>{});
       }},
      {"btree_32_256_16_128_binary_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 256>, 16, 128,
                                    fast_containers::SearchMode::Binary>{});
       }},
      {"absl_32_256",
       [&]() -> TimingStats {
         return benchmarker(absl::btree_map<std::array<std::int64_t, 4>,
                                            std::array<std::byte, 256>>{});
       }},
      {"map_32_256",
       [&]() -> TimingStats {
         return benchmarker(std::map<std::array<std::int64_t, 4>,
                                     std::array<std::byte, 256>>{});
       }},
      {"unordered_map_32_256",
       [&]() -> TimingStats {
         return benchmarker(std::unordered_map<std::array<std::int64_t, 4>,
                                               std::array<std::byte, 256>>{});
       }},

      /* 32 byte values */
      {"btree_16_32_16_128_linear_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 16, 128,
                                    fast_containers::SearchMode::Linear>{});
       }},
      {"btree_16_32_16_128_binary_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 16, 128,
                                    fast_containers::SearchMode::Binary>{});
       }},
      {"absl_16_32",
       [&]() -> TimingStats {
         return benchmarker(absl::btree_map<std::array<std::int64_t, 2>,
                                            std::array<std::byte, 32>>{});
       }},
      {"map_16_32",
       [&]() -> TimingStats {
         return benchmarker(std::map<std::array<std::int64_t, 2>,
                                     std::array<std::byte, 32>>{});
       }},
      {"unordered_map_16_32",
       [&]() -> TimingStats {
         return benchmarker(std::unordered_map<std::array<std::int64_t, 2>,
                                               std::array<std::byte, 32>>{});
       }},

      {"btree_16_32_128_128_linear_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 128, 128,
                                    fast_containers::SearchMode::Linear>{});
       }},
      {"btree_16_32_128_128_binary_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 128, 128,
                                    fast_containers::SearchMode::Binary>{});
       }},

      {"btree_32_32_16_128_linear_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 16, 128,
                                    fast_containers::SearchMode::Linear>{});
       }},
      {"btree_32_32_16_128_binary_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 16, 128,
                                    fast_containers::SearchMode::Binary>{});
       }},
      {"absl_32_32",
       [&]() -> TimingStats {
         return benchmarker(absl::btree_map<std::array<std::int64_t, 4>,
                                            std::array<std::byte, 32>>{});
       }},
      {"map_32_32",
       [&]() -> TimingStats {
         return benchmarker(std::map<std::array<std::int64_t, 4>,
                                     std::array<std::byte, 32>>{});
       }},
      {"unordered_map_32_32",
       [&]() -> TimingStats {
         return benchmarker(std::unordered_map<std::array<std::int64_t, 4>,
                                               std::array<std::byte, 32>>{});
       }},

      {"btree_32_32_128_128_linear_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 128, 128,
                                    fast_containers::SearchMode::Linear>{});
       }},
      {"btree_32_32_128_128_binary_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 128, 128,
                                    fast_containers::SearchMode::Binary>{});
       }},

      {"btree_8_32_64_64_simd_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 64, 64,
                                    fast_containers::SearchMode::SIMD>{});
       }},
      {"btree_8_32_64_64_binary_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 64, 64,
                                    fast_containers::SearchMode::Binary>{});
       }},
      {"btree_8_32_64_64_linear_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 64, 64,
                                    fast_containers::SearchMode::Linear>{});
       }},
      {"btree_8_32_16_128_simd_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 128,
                                    fast_containers::SearchMode::SIMD>{});
       }},
      {"btree_8_32_16_128_binary_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 128,
                                    fast_containers::SearchMode::Binary>{});
       }},
      {"btree_8_32_16_128_linear_simd",
       [&]() -> TimingStats {
         return benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 128,
                                    fast_containers::SearchMode::Linear>{});
       }},
      {"absl_8_32",
       [&]() -> TimingStats {
         return benchmarker(
             absl::btree_map<std::int64_t, std::array<std::byte, 32>>{});
       }},
      {"map_8_32",
       [&]() -> TimingStats {
         return benchmarker(
             std::map<std::int64_t, std::array<std::byte, 32>>{});
       }},
      {"unordered_map_8_32",
       [&]() -> TimingStats {
         return benchmarker(
             std::unordered_map<std::int64_t, std::array<std::byte, 32>>{});
       }},
  };

  auto print_valid_benchmarks = [&]() {
    std::cout << "Valid benchmark names:" << std::endl;
    for (const auto& name : benchmarkers) {
      std::cout << "  " << name.first << std::endl;
    }
  };

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
    print_valid_benchmarks();
    return 0;
  }

  for (const auto& name : names) {
    if (!benchmarkers.count(name)) {
      std::cerr << "Unknown benchmark: " << name << std::endl;
      print_valid_benchmarks();
      exit(1);
    }
  }

  for (size_t iter = 0; iter < target_iterations; ++iter) {
    std::cout << "Iteration " << iter << std::endl;
    for (const auto& name : names) {
      results[name] += benchmarkers.at(name)();
    }
  }

  std::println("{:>40}, {:>16}, {:>16}, {:>16}, {:>16}", "Benchmark Name",
               "Insert cycles", "Find cycles", "Erase cycles",
               "Iterate cycles");
  for (const auto& name : names) {
    const auto& stats = results.at(name);
#if 0
    std::cout << name << ": Insert: " << stats.insert_time
              << ", Find: " << stats.find_time
              << ", Erase: " << stats.erase_time
              << ", Iterate: " << stats.iterate_time << std::endl;
#endif
    std::println("{:>40}, {:>16}, {:>16}, {:>16}, {:>16}", name,
                 stats.insert_time, stats.find_time, stats.erase_time,
                 stats.iterate_time);
  }
}