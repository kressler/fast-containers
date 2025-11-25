#include <absl/container/btree_map.h>
#include <ankerl/unordered_dense.h>
#include <benchmark/benchmark.h>

#include <iostream>
#include <lyra/lyra.hpp>
#include <map>
#include <print>
#include <random>
#include <thread>
#include <unordered_set>

#include "../btree.hpp"
#include "histogram.h"
#include "log_linear_bucketer.h"

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

  histograms::Histogram<histograms::LogLinearBucketer<130, 2, 1>>
      insert_histogram;
  histograms::Histogram<histograms::LogLinearBucketer<130, 2, 1>>
      find_histogram;
  histograms::Histogram<histograms::LogLinearBucketer<130, 2, 1>>
      erase_histogram;
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
void run_benchmark(T& tree, uint64_t seed, size_t iterations, size_t tree_size,
                   size_t batches, size_t batch_size, TimingStats& stats) {
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

  auto insert = [&]() -> void {
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
    stats.insert_time += stop - start;
    stats.insert_histogram.observe(stop - start);
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
}

using LambdaType = std::function<void(TimingStats&)>;

int main(int argc, char** argv) {
  bool show_help = false;
  uint64_t seed = 42;
  size_t target_iterations = 1000;
  size_t tree_size = 1000000;
  size_t batches = 100;
  size_t batch_size = 1000;
  std::vector<std::string> names;
  std::unordered_map<std::string, TimingStats> results;

  auto benchmarker = [&](auto tree, TimingStats& stats) -> void {
    run_benchmark(tree, seed, target_iterations, tree_size, batches, batch_size,
                  stats);
  };

  std::map<std::string, LambdaType> benchmarkers{
      /* 256 byte values */
      {"btree_8_256_16_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 16,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_256_16_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 16,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_256_16_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 16,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_256_24_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 24,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_256_24_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 24,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_256_24_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 24,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_256_24_128_linear_std",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 24,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Linear,
                                    fast_containers::MoveMode::Standard>{},
             stats);
       }},
      {"btree_8_256_32_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 32,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_256_32_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 32,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_256_32_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 32,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_256_48_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 48,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_256_48_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 48,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_256_48_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 48,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_256_64_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 64,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_256_64_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 64,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_256_64_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 64,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_256_96_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 96,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_256_96_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 96,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_256_96_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 256>, 96,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
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

      {"btree_16_256_16_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<std::int64_t, 2>,
                                    std::array<std::byte, 256>, 16, 128,
                                    std::less<std::array<std::int64_t, 2>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_16_256_16_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 256>, 16, 128,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_16_256_16_64_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<std::int64_t, 2>,
                                    std::array<std::byte, 256>, 16, 64,
                                    std::less<std::array<std::int64_t, 2>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_16_256_24_64_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<std::int64_t, 2>,
                                    std::array<std::byte, 256>, 24, 64,
                                    std::less<std::array<std::int64_t, 2>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_16_256_24_64_linear_std",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<std::int64_t, 2>,
                                    std::array<std::byte, 256>, 24, 64,
                                    std::less<std::array<std::int64_t, 2>>,
                                    fast_containers::SearchMode::Linear,
                                    fast_containers::MoveMode::Standard>{},
             stats);
       }},
      {"btree_16_256_32_64_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<std::int64_t, 2>,
                                    std::array<std::byte, 256>, 32, 64,
                                    std::less<std::array<std::int64_t, 2>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"absl_16_256",
       [&](TimingStats& stats) -> void {
         benchmarker(absl::btree_map<std::array<std::int64_t, 2>,
                                     std::array<std::byte, 256>>{},
                     stats);
       }},
      {"map_16_256",
       [&](TimingStats& stats) -> void {
         benchmarker(std::map<std::array<std::int64_t, 2>,
                              std::array<std::byte, 256>>{},
                     stats);
       }},
      {"unordered_map_16_256",
       [&](TimingStats& stats) -> void {
         benchmarker(std::unordered_map<std::array<std::int64_t, 2>,
                                        std::array<std::byte, 256>>{},
                     stats);
       }},
      {"unordered_dense_16_256",
       [&](TimingStats& stats) -> void {
         benchmarker(ankerl::unordered_dense::map<std::array<std::int64_t, 2>,
                                                  std::array<std::byte, 256>>{},
                     stats);
       }},

      {"btree_32_256_16_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 256>, 16, 128,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_32_256_16_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 256>, 16, 128,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_32_256_16_48_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 256>, 16, 128,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_32_256_24_48_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 256>, 24, 128,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_32_256_24_48_linear_std",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 256>, 24, 128,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Linear,
                                    fast_containers::MoveMode::Standard>{},
             stats);
       }},
      {"btree_32_256_32_48_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 256>, 32, 128,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"absl_32_256",
       [&](TimingStats& stats) -> void {
         benchmarker(absl::btree_map<std::array<std::int64_t, 4>,
                                     std::array<std::byte, 256>>{},
                     stats);
       }},
      {"map_32_256",
       [&](TimingStats& stats) -> void {
         benchmarker(std::map<std::array<std::int64_t, 4>,
                              std::array<std::byte, 256>>{},
                     stats);
       }},
      {"unordered_map_32_256",
       [&](TimingStats& stats) -> void {
         benchmarker(std::unordered_map<std::array<std::int64_t, 4>,
                                        std::array<std::byte, 256>>{},
                     stats);
       }},
      {"unordered_dense_32_256",
       [&](TimingStats& stats) -> void {
         benchmarker(ankerl::unordered_dense::map<std::array<std::int64_t, 4>,
                                                  std::array<std::byte, 256>>{},
                     stats);
       }},

      /* 32 byte values */
      {"btree_16_32_16_32_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 16, 32,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_16_32_16_32_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 16, 32,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_16_32_16_48_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 16, 48,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_16_32_16_48_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 16, 48,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_16_32_16_64_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 16, 64,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_16_32_16_64_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 16, 64,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_16_32_16_96_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 16, 96,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_16_32_16_96_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 16, 96,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_16_32_16_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 16, 128,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_16_32_16_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 16, 128,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_16_32_64_64_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 64, 64,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_16_32_64_64_linear_std",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 64, 64,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Linear,
                                    fast_containers::MoveMode::Standard>{},
             stats);
       }},

      {"absl_16_32",
       [&](TimingStats& stats) -> void {
         benchmarker(absl::btree_map<std::array<std::int64_t, 2>,
                                     std::array<std::byte, 32>>{},
                     stats);
       }},
      {"map_16_32",
       [&](TimingStats& stats) -> void {
         benchmarker(
             std::map<std::array<std::int64_t, 2>, std::array<std::byte, 32>>{},
             stats);
       }},
      {"unordered_map_16_32",
       [&](TimingStats& stats) -> void {
         benchmarker(std::unordered_map<std::array<std::int64_t, 2>,
                                        std::array<std::byte, 32>>{},
                     stats);
       }},
      {"unordered_dense_16_32",
       [&](TimingStats& stats) -> void {
         benchmarker(ankerl::unordered_dense::map<std::array<std::int64_t, 2>,
                                                  std::array<std::byte, 32>>{},
                     stats);
       }},
      {"btree_16_32_128_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 128, 128,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_16_32_128_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 2>,
                                    std::array<std::byte, 32>, 128, 128,
                                    std::less<std::array<int64_t, 2>>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},

      {"btree_32_32_16_16_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 16, 16,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_32_32_16_16_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 16, 16,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_32_32_16_24_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 16, 24,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_32_32_16_24_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 16, 24,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_32_32_16_32_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 16, 32,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_32_32_16_32_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 16, 32,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_32_32_16_48_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 16, 48,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_32_32_16_48_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 16, 48,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_32_32_16_64_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 16, 64,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_32_32_16_64_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 16, 64,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_32_32_48_48_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 48, 48,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_32_32_48_48_linear_std",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<std::array<int64_t, 4>,
                                    std::array<std::byte, 32>, 48, 48,
                                    std::less<std::array<int64_t, 4>>,
                                    fast_containers::SearchMode::Linear,
                                    fast_containers::MoveMode::Standard>{},
             stats);
       }},
      {"absl_32_32",
       [&](TimingStats& stats) -> void {
         benchmarker(absl::btree_map<std::array<std::int64_t, 4>,
                                     std::array<std::byte, 32>>{},
                     stats);
       }},
      {"map_32_32",
       [&](TimingStats& stats) -> void {
         benchmarker(
             std::map<std::array<std::int64_t, 4>, std::array<std::byte, 32>>{},
             stats);
       }},
      {"unordered_map_32_32",
       [&](TimingStats& stats) -> void {
         benchmarker(std::unordered_map<std::array<std::int64_t, 4>,
                                        std::array<std::byte, 32>>{},
                     stats);
       }},
      {"unordered_dense_32_32",
       [&](TimingStats& stats) -> void {
         benchmarker(ankerl::unordered_dense::map<std::array<std::int64_t, 4>,
                                                  std::array<std::byte, 32>>{},
                     stats);
       }},

      {"btree_8_32_64_64_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 64, 64,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_32_64_64_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 64, 64,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_64_64_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 64, 64,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_16_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_32_16_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_16_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_16_64_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 64,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_32_16_64_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 64,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_16_64_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 64,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_16_96_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 96,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_32_16_96_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 96,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_16_96_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 96,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_16_160_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 160,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_32_16_160_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 160,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_16_160_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 160,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_16_256_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 256,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_32_16_256_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 256,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_16_256_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 16, 256,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_24_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 24, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_32_24_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 24, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_24_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 24, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_32_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 32, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_32_32_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 32, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_32_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 32, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_48_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 48, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_32_48_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 48, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_48_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 48, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_64_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 64, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_32_64_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 64, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_64_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 64, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_96_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 96, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_32_96_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 96, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_96_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 96, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_96_128_linear_std",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 96, 128,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear,
                                    fast_containers::MoveMode::Standard>{},
             stats);
       }},
      {"btree_8_32_96_256_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 96, 256,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_96_256_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 96, 256,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_96_512_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 96, 512,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_96_512_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 96, 512,
                                    std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_96_1024_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 96,
                                    1024, std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_8_32_96_1024_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 96,
                                    1024, std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_128_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 128,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_8_32_128_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 128,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_8_32_128_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int64_t, std::array<std::byte, 32>, 128,
                                    128, std::less<int64_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
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

      {"btree_4_32_64_64_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int32_t, std::array<std::byte, 32>, 64, 64,
                                    std::less<int32_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_4_32_64_64_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int32_t, std::array<std::byte, 32>, 64, 64,
                                    std::less<int32_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_4_32_64_64_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int32_t, std::array<std::byte, 32>, 64, 64,
                                    std::less<int32_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_4_32_16_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int32_t, std::array<std::byte, 32>, 16, 128,
                                    std::less<int32_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_4_32_16_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int32_t, std::array<std::byte, 32>, 16, 128,
                                    std::less<int32_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_4_32_16_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int32_t, std::array<std::byte, 32>, 16, 128,
                                    std::less<int32_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"absl_4_32",
       [&](TimingStats& stats) -> void {
         benchmarker(absl::btree_map<std::int32_t, std::array<std::byte, 32>>{},
                     stats);
       }},
      {"map_4_32",
       [&](TimingStats& stats) -> void {
         benchmarker(std::map<std::int32_t, std::array<std::byte, 32>>{},
                     stats);
       }},
      {"unordered_map_4_32",
       [&](TimingStats& stats) -> void {
         benchmarker(
             std::unordered_map<std::int32_t, std::array<std::byte, 32>>{},
             stats);
       }},
      {"unordered_dense_4_32",
       [&](TimingStats& stats) -> void {
         benchmarker(ankerl::unordered_dense::map<std::int32_t,
                                                  std::array<std::byte, 32>>{},
                     stats);
       }},

      {"btree_2_32_64_64_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int16_t, std::array<std::byte, 32>, 64, 64,
                                    std::less<int16_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_2_32_64_64_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int16_t, std::array<std::byte, 32>, 64, 64,
                                    std::less<int16_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_2_32_64_64_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int16_t, std::array<std::byte, 32>, 64, 64,
                                    std::less<int16_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"btree_2_32_16_128_simd_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int16_t, std::array<std::byte, 32>, 16, 128,
                                    std::less<int16_t>,
                                    fast_containers::SearchMode::SIMD>{},
             stats);
       }},
      {"btree_2_32_16_128_binary_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int16_t, std::array<std::byte, 32>, 16, 128,
                                    std::less<int16_t>,
                                    fast_containers::SearchMode::Binary>{},
             stats);
       }},
      {"btree_2_32_16_128_linear_simd",
       [&](TimingStats& stats) -> void {
         benchmarker(
             fast_containers::btree<int16_t, std::array<std::byte, 32>, 16, 128,
                                    std::less<int16_t>,
                                    fast_containers::SearchMode::Linear>{},
             stats);
       }},
      {"absl_2_32",
       [&](TimingStats& stats) -> void {
         benchmarker(absl::btree_map<std::int16_t, std::array<std::byte, 32>>{},
                     stats);
       }},
      {"map_2_32",
       [&](TimingStats& stats) -> void {
         benchmarker(std::map<std::int16_t, std::array<std::byte, 32>>{},
                     stats);
       }},
      {"unordered_map_2_32",
       [&](TimingStats& stats) -> void {
         benchmarker(
             std::unordered_map<std::int16_t, std::array<std::byte, 32>>{},
             stats);
       }},
      {"unordered_dense_2_32",
       [&](TimingStats& stats) -> void {
         benchmarker(ankerl::unordered_dense::map<std::int16_t,
                                                  std::array<std::byte, 32>>{},
                     stats);
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

  for (size_t iter = 0; iter < target_iterations; ++iter) {
    std::cout << "Iteration " << iter << std::endl;
    for (const auto& name : names) {
      benchmarkers.at(name)(results[name]);
    }
  }

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

  unsigned int dummy;
  auto start = std::chrono::high_resolution_clock::now();
  uint64_t rdtsc_start = __rdtsc();
  std::this_thread::sleep_for(std::chrono::milliseconds{1000});
  auto end = std::chrono::high_resolution_clock::now();
  uint64_t rdtsc_end = __rdtsc();
  const double cycles_per_nano =
      static_cast<double>(rdtsc_end - rdtsc_start) /
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  std::cout << "rdtsc calibration: " << cycles_per_nano << " cycles / ns"
            << std::endl;

  const std::vector<double> percentiles{0.5, 0.9, 0.95, 0.99, 0.999, 0.9999};
  std::cout << "    ";
  for (const auto& percentile : percentiles) {
    std::print("{:>16}, ", percentile);
  }
  std::cout << std::endl;

  auto print_percentiles = [&](const std::string& name, const auto& histogram) {
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