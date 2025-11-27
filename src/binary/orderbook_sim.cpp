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
#include "../hugepage_allocator.hpp"
#include "../hugepage_pool.hpp"
#include "../policy_based_hugepage_allocator.hpp"
#include "histogram.h"
#include "log_linear_bucketer.h"

using namespace kressler::fast_containers;
using namespace kressler::histograms;

struct TimingStats {
  Histogram<LogLinearBucketer<384, 4, 1>> add_histogram;
  Histogram<LogLinearBucketer<384, 4, 1>> modify_histogram;
  Histogram<LogLinearBucketer<384, 4, 1>> cancel_histogram;
};

struct BookKey {
  uint64_t price{0};
  uint64_t priority{0};

  friend bool operator<(const BookKey& lhs, const BookKey& rhs) {
    if (lhs.price != rhs.price)
      return lhs.price < rhs.price;
    return lhs.priority < rhs.priority;
  }

  friend bool operator>(const BookKey& lhs, const BookKey& rhs) {
    if (lhs.price != rhs.price)
      return lhs.price > rhs.price;
    return lhs.priority < rhs.priority;
  }

  friend bool operator==(const BookKey& lhs, const BookKey& rhs) {
    return lhs.price == rhs.price && lhs.priority == rhs.priority;
  }
};

template <size_t AnnotationSize = 0>
struct BookData {
  uint64_t size{0};
  uint64_t order_number{0};
  uint64_t timestamp{0};
  uint64_t flags;

  struct empty {};
  [[no_unique_address]] std::conditional_t<
      (AnnotationSize > 0), std::array<std::byte, AnnotationSize>, empty>
      annotation;
};
static_assert(sizeof(BookData<>) == 32);
static_assert(sizeof(BookData<256>) == 288);

template <>
struct std::hash<BookKey> {
  std::size_t operator()(const BookKey& k) const {
    using std::hash;
    return hash<uint64_t>()(k.price) ^ hash<uint64_t>()(k.priority);
  }
};

void run_benchmark(auto& tree, TimingStats& stats, uint64_t seed,
                   size_t book_size, size_t event_count) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<uint64_t> uniform_dist(
      0, std::numeric_limits<uint64_t>::max());
  std::geometric_distribution<int> geometric_dist(0.1);

  // Map of price -> [order_numbers]
  std::map<uint64_t, ankerl::unordered_dense::set<uint64_t>> levels;
  uint64_t priority = 0;

  auto get_price = [&]() -> uint64_t {
    return 3ul * geometric_dist(rng) + (uniform_dist(rng) % 1000);
  };

  auto get_new_order = [&]() -> BookKey {
    BookKey key;
    key.price = get_price();
    key.priority = ++priority;
    return key;
  };

  auto get_existing_order = [&]() -> BookKey {
    if (levels.empty())
      throw std::runtime_error("Empty order book");
    uint64_t tprice = get_price();
    auto it = levels.lower_bound(tprice);
    if (it == levels.end())
      it--;

    // it now points to a level. Need to select an order from it.
    const size_t sz = it->second.size();
    const size_t idx = uniform_dist(rng) % sz;
    const uint64_t priority = it->second.values()[idx];

    return {it->first, priority};
  };

  // Initialize orderbook
  for (size_t i = 0; i < book_size; ++i) {
    auto key = get_new_order();
    tree.insert({key, {}});
    levels[key.price].insert(key.priority);
  }

  unsigned int dummy;
  for (size_t i = 0; i < event_count; ++i) {
    uint64_t type = uniform_dist(rng) % 3;
    if (type == 0) {  // Add a new order to the book
      const auto key = get_new_order();
      const uint64_t start = __rdtscp(&dummy);
      tree.insert({key, {}});
      const uint64_t end = __rdtscp(&dummy);
      levels[key.price].insert(key.priority);
      stats.add_histogram.observe(end - start);
    } else if (type == 1) {  // Modify an existing order
      const auto key = get_existing_order();
      const uint64_t start = __rdtscp(&dummy);
      auto it = tree.find(key);
      if (it == tree.end())
        throw std::runtime_error("Order not found (modify)");
      it->second.size++;
      const uint64_t end = __rdtscp(&dummy);
      stats.modify_histogram.observe(end - start);
    } else {  // Remove an order
      const auto key = get_existing_order();
      const uint64_t start = __rdtscp(&dummy);
      auto erased = tree.erase(key);
      const uint64_t end = __rdtscp(&dummy);
      stats.cancel_histogram.observe(end - start);

      if (erased == 0)
        throw std::runtime_error("Order not found (erase)");
      auto it = levels.find(key.price);
      it->second.erase(key.priority);
      if (it->second.empty())
        levels.erase(it);
    }
  }
}

using LambdaType = std::function<void(TimingStats&)>;

int main(int argc, char** argv) {
  bool show_help = false;
  uint64_t seed = 42;
  size_t book_size = 250000;
  size_t event_count = 1000000;
  size_t iterations = 1;
  std::vector<std::string> names;
  std::unordered_map<std::string, TimingStats> results;

  // Define command line interface
  auto cli = lyra::cli() | lyra::help(show_help) |
             lyra::opt(seed, "seed")["-d"]["--seed"](
                 "Random seed (defaults to time since epoch") |
             lyra::opt(book_size, "book_size")["-s"]["--book-size"](
                 "Target size of the book to maintain") |
             lyra::opt(event_count, "event_count")["-e"]["--event-count"](
                 "Number of events to run (after at target size)") |
             lyra::opt(iterations, "iterations")["-i"]["--iterations"](
                 "Iterations to run") |
             lyra::arg(names, "names")("Name of the benchmark to run");

  // Parse command line
  auto result = cli.parse({argc, argv});
  if (!result) {
    std::cerr << "Error in command line: " << result.message() << std::endl;
    exit(1);
  }

  auto benchmarker =
      [&]<typename Tree,
          typename Allocator = std::allocator<typename Tree::value_type>>(
          Tree tree, TimingStats& stats) -> void {
    run_benchmark(tree, stats, seed, book_size, event_count);
  };

  std::map<std::string, LambdaType> benchmarkers{
      {"btree_0_64_64_linear_std",
       [&](TimingStats& stats) -> void {
         benchmarker(btree<BookKey, BookData<0>, 64, 64, std::less<>,
                           SearchMode::Linear, MoveMode::Standard>{},
                     stats);
       }},
      {"btree_0_64_64_linear_std_hp",
       [&](TimingStats& stats) -> void {
         auto alloc = make_two_pool_allocator<std::array<int64_t, 2>,
                                              std::array<std::byte, 256>>(
             64ul * 1024ul * 1024ul, 64ul * 1024ul * 1024ul);
         benchmarker(
             btree<BookKey, BookData<0>, 64, 64, std::less<>,
                   SearchMode::Linear, MoveMode::Standard, decltype(alloc)>{
                 alloc},
             stats);
       }},
      {"absl_0",
       [&](TimingStats& stats) -> void {
         benchmarker(absl::btree_map<BookKey, BookData<0>>{}, stats);
       }},
      {"map_0",
       [&](TimingStats& stats) -> void {
         benchmarker(std::map<BookKey, BookData<0>>{}, stats);
       }},
      {"unordered_map_0",
       [&](TimingStats& stats) -> void {
         benchmarker(std::unordered_map<BookKey, BookData<0>>{}, stats);
       }},
      {"unordered_dense_0",
       [&](TimingStats& stats) -> void {
         benchmarker(ankerl::unordered_dense::map<BookKey, BookData<0>>{},
                     stats);
       }},
      {"btree_256_8_64_linear_std",
       [&](TimingStats& stats) -> void {
         benchmarker(btree<BookKey, BookData<256>, 8, 64, std::less<>,
                           SearchMode::Linear, MoveMode::Standard>{},
                     stats);
       }},
      {"btree_256_8_64_linear_std_hp",
       [&](TimingStats& stats) -> void {
         auto alloc = make_two_pool_allocator<std::array<int64_t, 2>,
                                              std::array<std::byte, 256>>(
             64ul * 1024ul * 1024ul, 64ul * 1024ul * 1024ul);
         benchmarker(
             btree<BookKey, BookData<256>, 8, 64, std::less<>,
                   SearchMode::Linear, MoveMode::Standard, decltype(alloc)>{
                 alloc},
             stats);
       }},
      {"absl_256",
       [&](TimingStats& stats) -> void {
         benchmarker(absl::btree_map<BookKey, BookData<256>>{}, stats);
       }},
      {"map_256",
       [&](TimingStats& stats) -> void {
         benchmarker(std::map<BookKey, BookData<256>>{}, stats);
       }},
      {"unordered_map_256",
       [&](TimingStats& stats) -> void {
         benchmarker(std::unordered_map<BookKey, BookData<256>>{}, stats);
       }},
      {"unordered_dense_256",
       [&](TimingStats& stats) -> void {
         benchmarker(ankerl::unordered_dense::map<BookKey, BookData<256>>{},
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

  for (const auto& name : names) {
    if (!benchmarkers.count(name)) {
      std::cerr << "Unknown benchmark: " << name << std::endl;
      print_valid_benchmarks();
      exit(1);
    }
  }

  for (size_t i = 0; i < iterations; ++i) {
    for (const auto& name : names) {
      benchmarkers.at(name)(results[name]);
    }
  }

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
    print_percentiles("a", stats.add_histogram);
    print_percentiles("m", stats.modify_histogram);
    print_percentiles("c", stats.cancel_histogram);
  }

  return 0;
}