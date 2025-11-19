#include <benchmark/benchmark.h>

#include <chrono>
#include <iostream>
#include <lyra/lyra.hpp>
#include <map>
#include <random>
#include <unordered_set>

#include "../btree.hpp"

int main(int argc, char** argv) {
  bool show_help = false;
  uint64_t seed = __rdtsc();
  size_t target_iterations = 1000;
  size_t min_keys = 100000;
  size_t max_keys = 2000000;
  size_t batches = 100;
  size_t batch_size = 1000;

  // Define command line interface
  auto cli =
      lyra::cli() | lyra::help(show_help) |
      lyra::opt(seed, "seed")["-d"]["--seed"](
          "Random seed (defaults to time since epoch") |
      lyra::opt(target_iterations,
                "iterations")["-i"]["--iterations"]("Iterations to run") |
      lyra::opt(min_keys,
                "min_keys")["--min-keys"]("Minimum keys to target in tree") |
      lyra::opt(max_keys,
                "max_keys")["--max-keys"]("Maximum keys to target in tree") |
      lyra::opt(batches, "batches")["-b"]["--batches"](
          "Number of erase/insert batches to run") |
      lyra::opt(batch_size, "batch_size")["-s"]["--batch-size"](
          "Size of an erase/insert batch");

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

  std::uniform_int_distribution<size_t> num_key_dist(min_keys, max_keys);
  for (size_t iter = 0; iter < target_iterations; ++iter) {
    std::mt19937 rng(iter + seed);
    std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(),
                                            std::numeric_limits<int>::max());

    size_t num_keys = num_key_dist(rng);
    std::cout << "Iteration " << iter << " using " << num_keys << " keys, seed "
              << iter + seed << std::endl;

    std::map<int, int> ordered_map;
    fast_containers::btree<int, int, 32, 128, fast_containers::SearchMode::SIMD>
        btree;
    std::unordered_set<int> seen;

    auto insert = [&]() -> void {
      int key = dist(rng);
      int val = dist(rng);
      ordered_map.insert({key, val});
      btree.insert(key, val);
      seen.insert(key);
    };

    auto remove = [&]() -> void {
      auto key = *seen.begin();
      seen.erase(key);
      ordered_map.erase(key);
      btree.erase(key);
    };

    auto validate = [&]() -> void {
      auto it = ordered_map.begin();
      auto btree_it = btree.begin();

      while (it != ordered_map.end() && btree_it != btree.end()) {
        if (it->first != btree_it->first) {
          std::cout << "Mismatch at key " << it->first
                    << " != " << btree_it->first << std::endl;
          exit(1);
        }
        ++it;
        ++btree_it;
      }

      if (it != ordered_map.end()) {
        std::cout << "B-tree ended early!" << std::endl;
        exit(1);
      }

      if (btree_it != btree.end()) {
        std::cout << "Ordered map ended early!" << std::endl;
        exit(1);
      }
    };

    // Build up the initial trees/maps
    while (ordered_map.size() < num_keys) {
      insert();
    }
    validate();

    // Run erase/insert batches
    for (size_t batch = 0; batch < batches; ++batch) {
      for (size_t i = 0; i < batch_size; ++i) {
        remove();
      }
      for (size_t i = 0; i < batch_size; ++i) {
        insert();
      }
      validate();
    }

    // Empty out the tree/map
    while (!seen.empty()) {
      remove();
    }
    validate();
  }
  return 0;
}