// Copyright (c) 2025 Bryan Kressler
//
// SPDX-License-Identifier: BSD-3-Clause

#include <absl/container/btree_map.h>
#include <ankerl/unordered_dense.h>
#include <benchmark/benchmark.h>
#include <histograms/histogram.h>
#include <histograms/log_linear_bucketer.h>

#include <fast_containers/btree.hpp>
#include <fast_containers/hugepage_allocator.hpp>
#include <fast_containers/hugepage_pool.hpp>
#include <fast_containers/policy_based_hugepage_allocator.hpp>
#include <iostream>
#include <lyra/lyra.hpp>
#include <map>
#include <print>
#include <random>
#include <thread>
#include <unordered_set>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || \
    defined(_M_IX86)
#if defined(__GNUC__) || defined(__clang__)
#include <x86intrin.h>
#elif defined(_MSC_VER)
#include <intrin.h>
#endif
#endif

using namespace kressler::fast_containers;
using namespace kressler::histograms;

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

  Histogram<LogLinearBucketer<384, 4, 1>> insert_histogram;
  Histogram<LogLinearBucketer<384, 4, 1>> find_histogram;
  Histogram<LogLinearBucketer<384, 4, 1>> erase_histogram;
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

auto print_pool_stats = [](const HugePagePool& pool,
                           std::string_view name) -> void {
#ifdef ALLOCATOR_STATS
  std::cout << name << " pool stats:\n";
  std::println("a: {}, d: {}, g: {}, b: {}, p: {}", pool.get_allocations(),
               pool.get_deallocations(), pool.get_growth_events(),
               pool.get_bytes_allocated(), pool.get_peak_usage());
#endif
};

template <typename T>
void run_benchmark(T& tree, uint64_t seed, size_t iterations, size_t tree_size,
                   size_t batches, size_t batch_size, bool record_rampup,
                   TimingStats& stats) {
  auto get_dist = [&]() -> auto {
    if constexpr (std::is_fundamental_v<typename T::key_type>) {
      if (tree_size > std::numeric_limits<typename T::key_type>::max() / 2) {
        throw std::runtime_error("Tree size too large for key data type");
      }
      return std::uniform_int_distribution<typename T::key_type>(
          std::numeric_limits<typename T::key_type>::min(),
          std::numeric_limits<typename T::key_type>::max());
    } else {
      if (tree_size > std::numeric_limits<int>::max() / 2) {
        throw std::runtime_error("Tree size too large for key data type");
      }
      return std::uniform_int_distribution<int>(
          std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    }
  };
  std::mt19937 rng(seed);
  auto dist = get_dist();
  unsigned int dummy;
  std::unordered_set<typename T::key_type> keys{};

  auto insert = [&](bool record_stats = true) -> void {
    bool inserted;
    typename T::key_type key;
    do {
      if constexpr (std::is_fundamental_v<typename T::key_type>) {
        key = dist(rng);
      } else {
        for (int i = 0; i < key.size(); ++i) {
          key[i] = dist(rng);
        }
      }
      auto [_, i] = keys.insert(key);
      inserted = i;
    } while (!inserted);
    uint64_t start = __rdtscp(&dummy);
    tree.insert({key, {}});
    uint64_t stop = __rdtscp(&dummy);
    if (record_stats) {
      stats.insert_time += stop - start;
      stats.insert_histogram.observe(stop - start);
    }
  };

  auto find = [&](const typename T::key_type& key) -> void {
    uint64_t start = __rdtscp(&dummy);
    benchmark::DoNotOptimize(tree.find(key));
    uint64_t stop = __rdtscp(&dummy);
    stats.find_time += stop - start;
    stats.find_histogram.observe(stop - start);
  };

  auto erase = [&]() -> void {
    auto key = *keys.begin();
    keys.erase(key);
    uint64_t start = __rdtscp(&dummy);
    benchmark::DoNotOptimize(tree.find(key));
    uint64_t stop = __rdtscp(&dummy);
    stats.erase_time += stop - start;
    stats.erase_histogram.observe(stop - start);
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
    insert(false);
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
}

using LambdaType = std::function<void(TimingStats&)>;

int main(int argc, char** argv) {
  bool show_help = false;
  bool json_output = false;
  uint64_t seed = 42;
  size_t target_iterations = 1000;
  size_t tree_size = 1000000;
  size_t batches = 100;
  size_t batch_size = 1000;
  std::vector<std::string> names;
  std::unordered_map<std::string, TimingStats> results;
  bool record_rampup = true;

  auto benchmarker =
      [&]<typename Tree,
          typename Allocator = std::allocator<typename Tree::value_type>>(
          Tree tree, TimingStats& stats) -> void {
    run_benchmark(tree, seed, target_iterations, tree_size, batches, batch_size,
                  record_rampup, stats);
  };

  // Define command line interface
  auto cli =
      lyra::cli() | lyra::help(show_help) |
      lyra::opt(seed, "seed")["-d"]["--seed"](
          "Random seed (defaults to time since epoch") |
      lyra::opt(target_iterations,
                "iterations")["-i"]["--iterations"]("Iterations to run") |
      lyra::opt(tree_size, "min_keys")["-t"]["--tree-size"](
          "Minimum keys to target in tree") |
      lyra::opt(batches, "batches")["-b"]["--batches"](
          "Number of erase/insert batches to run") |
      lyra::opt(batch_size, "batch_size")["-s"]["--batch-size"](
          "Size of an erase/insert batch") |
      lyra::opt(record_rampup, "record_rampup")["-r"]["--record-rampup"](
          "Record stats during rampup in tree size") |
      lyra::opt(json_output)["-j"]["--json"]("Output results in JSON format") |
      lyra::arg(names, "names")("Name of the benchmark to run");

  // Parse command line
  auto result = cli.parse({argc, argv});
  if (!result) {
    std::cerr << "Error in command line: " << result.message() << std::endl;
    exit(1);
  }

  std::map<std::string, LambdaType> benchmarkers{
      // 8 byte keys, 32 byte values
      {"btree_8_32_96_128_linear",
       [&](TimingStats& stats) -> void {
         benchmarker(btree<int64_t, std::array<std::byte, 32>, 96, 128,
                           std::less<int64_t>, SearchMode::Linear>{},
                     stats);
       }},
      {"btree_8_32_96_128_linear_hp",
       [&](TimingStats& stats) -> void {
         auto alloc =
             make_two_pool_allocator<int64_t, std::array<std::byte, 32>>(
                 64ul * 1024ul * 1024ul, 64ul * 1024ul * 1024ul);
         benchmarker(
             btree<int64_t, std::array<std::byte, 32>, 96, 128,
                   std::less<int64_t>, SearchMode::Linear, decltype(alloc)>{
                 alloc},
             stats);
         print_pool_stats(*alloc.get_policy().leaf_pool_, "Leaf");
         print_pool_stats(*alloc.get_policy().internal_pool_, "Internal");
       }},
      {"btree_8_32_96_128_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(btree<int64_t, std::array<std::byte, 32>, 96, 128,
                           std::less<int64_t>, SearchMode::SIMD>{},
                     stats);
       }},
      {"btree_8_32_96_128_simd_hp",
       [&](TimingStats& stats) -> void {
         auto alloc =
             make_two_pool_allocator<int64_t, std::array<std::byte, 32>>(
                 64ul * 1024ul * 1024ul, 64ul * 1024ul * 1024ul);
         benchmarker(
             btree<int64_t, std::array<std::byte, 32>, 96, 128,
                   std::less<int64_t>, SearchMode::SIMD, decltype(alloc)>{
                 alloc},
             stats);
         print_pool_stats(*alloc.get_policy().leaf_pool_, "Leaf");
         print_pool_stats(*alloc.get_policy().internal_pool_, "Internal");
       }},
      {"absl_8_32",
       [&](TimingStats& stats) -> void {
         benchmarker(absl::btree_map<std::int64_t, std::array<std::byte, 32>>{},
                     stats);
       }},
      {"map_8_32",
       [&](TimingStats& stats) -> void {
         benchmarker(std::map<std::int64_t, std::array<std::byte, 32>>{},
                     stats);
       }},
      {"unordered_map_8_32",
       [&](TimingStats& stats) -> void {
         benchmarker(
             std::unordered_map<std::int64_t, std::array<std::byte, 32>>{},
             stats);
       }},
      {"unordered_dense_8_32",
       [&](TimingStats& stats) -> void {
         benchmarker(ankerl::unordered_dense::map<std::int64_t,
                                                  std::array<std::byte, 32>>{},
                     stats);
       }},

      // 8 byte keys, 256 byte values
      {"btree_8_256_8_128_linear",
       [&](TimingStats& stats) -> void {
         benchmarker(btree<int64_t, std::array<std::byte, 256>, 8, 128,
                           std::less<int64_t>, SearchMode::Linear>{},
                     stats);
       }},
      {"btree_8_256_8_128_linear_hp",
       [&](TimingStats& stats) -> void {
         auto alloc =
             make_two_pool_allocator<int64_t, std::array<std::byte, 256>>(
                 64ul * 1024ul * 1024ul, 64ul * 1024ul * 1024ul);
         benchmarker(
             btree<int64_t, std::array<std::byte, 32>, 8, 128,
                   std::less<int64_t>, SearchMode::Linear, decltype(alloc)>{
                 alloc},
             stats);
         print_pool_stats(*alloc.get_policy().leaf_pool_, "Leaf");
         print_pool_stats(*alloc.get_policy().internal_pool_, "Internal");
       }},
      {"btree_8_256_8_128_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(btree<int64_t, std::array<std::byte, 256>, 8, 128,
                           std::less<int64_t>, SearchMode::SIMD>{},
                     stats);
       }},
      {"btree_8_256_8_128_simd_hp",
       [&](TimingStats& stats) -> void {
         auto alloc =
             make_two_pool_allocator<int64_t, std::array<std::byte, 256>>(
                 64ul * 1024ul * 1024ul, 64ul * 1024ul * 1024ul);
         benchmarker(
             btree<int64_t, std::array<std::byte, 32>, 8, 128,
                   std::less<int64_t>, SearchMode::SIMD, decltype(alloc)>{
                 alloc},
             stats);
         print_pool_stats(*alloc.get_policy().leaf_pool_, "Leaf");
         print_pool_stats(*alloc.get_policy().internal_pool_, "Internal");
       }},
      {"absl_8_256",
       [&](TimingStats& stats) -> void {
         benchmarker(
             absl::btree_map<std::int64_t, std::array<std::byte, 256>>{},
             stats);
       }},
      {"map_8_256",
       [&](TimingStats& stats) -> void {
         benchmarker(std::map<std::int64_t, std::array<std::byte, 256>>{},
                     stats);
       }},
      {"unordered_map_8_256",
       [&](TimingStats& stats) -> void {
         benchmarker(
             std::unordered_map<std::int64_t, std::array<std::byte, 256>>{},
             stats);
       }},
      {"unordered_dense_8_256",
       [&](TimingStats& stats) -> void {
         benchmarker(ankerl::unordered_dense::map<std::int64_t,
                                                  std::array<std::byte, 256>>{},
                     stats);
       }},
  };

  auto print_valid_benchmarks = [&]() {
    std::cout << "Valid benchmark names:" << std::endl;
    for (const auto& name : benchmarkers) {
      std::cout << "  " << name.first << std::endl;
    }
  };

  // Show help if requested
  if (show_help) {
    std::cout << cli << std::endl;
    print_valid_benchmarks();
    return 0;
  }

  if (batch_size > tree_size) {
    throw std::runtime_error("Batch size must be less than tree size");
  }

  for (const auto& name : names) {
    if (!benchmarkers.count(name)) {
      std::cerr << "Unknown benchmark: " << name << std::endl;
      print_valid_benchmarks();
      exit(1);
    }
  }

  // Run benchmarks with optional progress output
  for (size_t iter = 0; iter < target_iterations; ++iter) {
    if (!json_output) {
      std::cout << "Iteration " << iter << std::endl;
    }
    for (const auto& name : names) {
      benchmarkers.at(name)(results[name]);
    }
  }

  // Calibrate rdtsc to nanoseconds
  unsigned int dummy;
  auto start = std::chrono::high_resolution_clock::now();
  uint64_t rdtsc_start = __rdtsc();
  std::this_thread::sleep_for(std::chrono::milliseconds{1000});
  auto end = std::chrono::high_resolution_clock::now();
  uint64_t rdtsc_end = __rdtsc();
  const double cycles_per_nano =
      static_cast<double>(rdtsc_end - rdtsc_start) /
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

  // Define percentiles to calculate
  const std::vector<double> percentiles{0.0, 0.5, 0.95, 0.99, 0.999, 0.9999};

  if (json_output) {
    // JSON output format for interleaved_benchmark.py
    if (names.size() != 1) {
      std::cerr << "Error: JSON output mode requires exactly one benchmark name"
                << std::endl;
      exit(1);
    }

    const auto& stats = results.at(names[0]);

    // Get percentile values
    auto insert_percentiles = stats.insert_histogram.percentiles(percentiles);
    auto find_percentiles = stats.find_histogram.percentiles(percentiles);
    auto erase_percentiles = stats.erase_histogram.percentiles(percentiles);

    // Convert cycles to nanoseconds
    auto to_ns = [&](double cycles) { return cycles / cycles_per_nano; };

    // Output JSON
    std::cout << "{\n";
    std::cout << "  \"config\": \"" << names[0] << "\",\n";
    std::cout << "  \"iterations\": " << target_iterations << ",\n";
    std::cout << "  \"total_cycles\": {\n";
    std::cout << "    \"insert\": " << stats.insert_time << ",\n";
    std::cout << "    \"find\": " << stats.find_time << ",\n";
    std::cout << "    \"erase\": " << stats.erase_time << ",\n";
    std::cout << "    \"iterate\": " << stats.iterate_time << "\n";
    std::cout << "  },\n";
    std::cout << "  \"performance\": {\n";
    std::cout << "    \"insert\": {\n";
    std::cout << "      \"p0\": " << to_ns(insert_percentiles[0]) << ",\n";
    std::cout << "      \"p50\": " << to_ns(insert_percentiles[1]) << ",\n";
    std::cout << "      \"p95\": " << to_ns(insert_percentiles[2]) << ",\n";
    std::cout << "      \"p99\": " << to_ns(insert_percentiles[3]) << ",\n";
    std::cout << "      \"p99_9\": " << to_ns(insert_percentiles[4]) << ",\n";
    std::cout << "      \"p99_99\": " << to_ns(insert_percentiles[5]) << "\n";
    std::cout << "    },\n";
    std::cout << "    \"find\": {\n";
    std::cout << "      \"p0\": " << to_ns(find_percentiles[0]) << ",\n";
    std::cout << "      \"p50\": " << to_ns(find_percentiles[1]) << ",\n";
    std::cout << "      \"p95\": " << to_ns(find_percentiles[2]) << ",\n";
    std::cout << "      \"p99\": " << to_ns(find_percentiles[3]) << ",\n";
    std::cout << "      \"p99_9\": " << to_ns(find_percentiles[4]) << ",\n";
    std::cout << "      \"p99_99\": " << to_ns(find_percentiles[5]) << "\n";
    std::cout << "    },\n";
    std::cout << "    \"erase\": {\n";
    std::cout << "      \"p0\": " << to_ns(erase_percentiles[0]) << ",\n";
    std::cout << "      \"p50\": " << to_ns(erase_percentiles[1]) << ",\n";
    std::cout << "      \"p95\": " << to_ns(erase_percentiles[2]) << ",\n";
    std::cout << "      \"p99\": " << to_ns(erase_percentiles[3]) << ",\n";
    std::cout << "      \"p99_9\": " << to_ns(erase_percentiles[4]) << ",\n";
    std::cout << "      \"p99_99\": " << to_ns(erase_percentiles[5]) << "\n";
    std::cout << "    }\n";
    std::cout << "  }\n";
    std::cout << "}\n";
  } else {
    // Text output format (existing behavior)
    std::cout << "rdtsc calibration: " << cycles_per_nano << " cycles / ns"
              << std::endl;

    std::println("{:>40}, {:>16}, {:>16}, {:>16}, {:>16}", "Benchmark Name",
                 "Insert cycles", "Find cycles", "Erase cycles",
                 "Iterate cycles");
    for (const auto& name : names) {
      const auto& stats = results.at(name);
      std::println("{:>40}, {:>16}, {:>16}, {:>16}, {:>16}", name,
                   stats.insert_time, stats.find_time, stats.erase_time,
                   stats.iterate_time);
    }

    std::cout << std::endl << std::endl;

    std::cout << "    ";
    for (const auto& percentile : percentiles) {
      std::print("{:>16}, ", percentile);
    }
    std::cout << std::endl;

    auto print_percentiles = [&](const std::string& name,
                                 const auto& histogram) {
      auto values = histogram.percentiles(percentiles);
      std::cout << "  " << name << ":";
      for (const auto& value : values) {
        std::print("{:>16f}, ", value / cycles_per_nano);
      }
      std::cout << std::endl;
    };

    for (const auto& name : names) {
      std::cout << name << std::endl;
      const auto& stats = results.at(name);
      print_percentiles("i", stats.insert_histogram);
      print_percentiles("f", stats.find_histogram);
      print_percentiles("e", stats.erase_histogram);
    }
  }
}