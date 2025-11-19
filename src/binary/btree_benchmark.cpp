#include <absl/container/btree_map.h>
#include <benchmark/benchmark.h>

#include <chrono>
#include <iostream>
#include <map>
#include <random>
#include <unordered_set>

#include "../btree.hpp"

int main(int argc, char** argv) {
  uint64_t seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::uniform_int_distribution<size_t> num_key_dist(100000, 2000000);
  for (size_t iter = 0; iter < 1000; ++iter) {
    uint64_t btree_time = 0;
    uint64_t absl_time = 0;
    uint64_t map_time = 0;
    uint dummy = 0;
    std::mt19937 rng(iter + seed);
    std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(),
                                            std::numeric_limits<int>::max());

    size_t num_keys = num_key_dist(rng);
    std::cout << "Iteration " << iter << " using " << num_keys << " keys, seed "
              << iter + seed << std::endl;

    std::map<int, std::array<char, 256>> ordered_map;
    absl::btree_map<int, std::array<char, 256>> absl_btree;
    fast_containers::btree<int, std::array<char, 256>, 4, 64,
                           fast_containers::SearchMode::SIMD>
        btree;
    std::unordered_set<int> seen;

    while (ordered_map.size() < num_keys) {
      int key = dist(rng);
      int val = dist(rng);
      if (seen.find(key) == seen.end()) {
        uint64_t start = __rdtscp(&dummy);
        ordered_map.insert({key, {}});
        uint64_t mid1 = __rdtscp(&dummy);
        absl_btree.insert({key, {}});
        uint64_t mid2 = __rdtscp(&dummy);
        btree.insert(key, {});
        uint64_t end = __rdtscp(&dummy);

        map_time += mid1 - start;
        absl_time += mid2 - mid1;
        btree_time += end - mid2;

        seen.insert(key);
      }
    }

    double ratio =
        static_cast<double>(btree_time) / static_cast<double>(map_time);
    double absl_ratio =
        static_cast<double>(btree_time) / static_cast<double>(absl_time);
    std::cout << "INSERT TIMES: Ordered map time: " << map_time
              << ", absl time: " << absl_time << ", btree time: " << btree_time
              << ", ratio(btree/map): " << ratio
              << ", ratio(btree/absl): " << absl_ratio << std::endl;

    map_time = 0;
    absl_time = 0;
    btree_time = 0;

    for (const auto& key : seen) {
      uint64_t start = __rdtscp(&dummy);
      benchmark::DoNotOptimize(ordered_map.find(key));
      uint64_t mid1 = __rdtscp(&dummy);
      benchmark::DoNotOptimize(absl_btree.find(key));
      uint64_t mid2 = __rdtscp(&dummy);
      benchmark::DoNotOptimize(btree.find(key));
      uint64_t end = __rdtscp(&dummy);

      map_time += mid1 - start;
      absl_time += mid2 - mid1;
      btree_time += end - mid2;
    }

    ratio = static_cast<double>(btree_time) / static_cast<double>(map_time);
    absl_ratio =
        static_cast<double>(btree_time) / static_cast<double>(absl_time);
    std::cout << "FIND TIMES: Ordered map time: " << map_time
              << ", absl time: " << absl_time << ", btree time: " << btree_time
              << ", ratio(btree/map): " << ratio
              << ", ratio(btree/absl): " << absl_ratio << std::endl;

    auto it = ordered_map.begin();
    auto btree_it = btree.begin();

    while (it != ordered_map.end() && btree_it != btree.end()) {
      if (it->first != btree_it->first) {
        std::cout << "Mismatch at key " << it->first
                  << " != " << btree_it->first << std::endl;
        return 1;
      }
      it++;
      btree_it++;
    }

    if (it != ordered_map.end()) {
      std::cout << "B-tree ended early!" << std::endl;
      return 1;
    }

    if (btree_it != btree.end()) {
      std::cout << "Ordered map ended early!" << std::endl;
      return 1;
    }
  }
  return 0;
}