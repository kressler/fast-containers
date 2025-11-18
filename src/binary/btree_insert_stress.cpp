#include <chrono>
#include <iostream>
#include <map>
#include <random>
#include <unordered_set>

#include "../btree.hpp"

int main(int argc, char** argv) {
  uint64_t seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::uniform_int_distribution<size_t> num_key_dist(100000, 2000000);
  uint64_t btree_time = 0;
  uint64_t map_time = 0;
  uint dummy = 0;
  for (size_t iter = 0; iter < 1000; ++iter) {
    std::mt19937 rng(iter + seed);
    std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(),
                                            std::numeric_limits<int>::max());

    size_t num_keys = num_key_dist(rng);
    std::cout << "Iteration " << iter << " using " << num_keys << " keys, seed "
              << iter + seed << std::endl;

    std::map<int, int> ordered_map;
    fast_containers::btree<int, int, 32, 64, fast_containers::SearchMode::SIMD>
        btree;
    std::unordered_set<int> seen;

    while (ordered_map.size() < num_keys) {
      int key = dist(rng);
      int val = dist(rng);
      if (seen.find(key) == seen.end()) {
        uint64_t start = __rdtscp(&dummy);
        ordered_map.insert({key, val});
        uint64_t mid = __rdtscp(&dummy);
        btree.insert(key, val);
        uint64_t end = __rdtscp(&dummy);

        map_time += mid - start;
        btree_time += end - mid;

        seen.insert(key);
      }
    }

    double ratio =
        static_cast<double>(btree_time) / static_cast<double>(map_time);
    std::cout << "Ordered map time: " << map_time
              << ", btree time: " << btree_time << ", ratio: " << ratio
              << std::endl;

    auto it = ordered_map.begin();
    auto btree_it = btree.begin();

    while (it != ordered_map.end() && btree_it != btree.end()) {
      if (it->first != btree_it->first && it->second != btree_it->second) {
        std::cout << "Mismatch at key " << it->first << " != " << it->second
                  << " or val " << it->second << " != " << btree_it->second
                  << std::endl;
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