#include <x86intrin.h>

#include <iostream>
#include <lyra/lyra.hpp>
#include <print>

#include "histogram.h"
#include "log_linear_bucketer.h"

int main(int argc, char** argv) {
  bool show_help = false;
  size_t run_cycles = 4000000000;

  // Define command line interface
  auto cli =
      lyra::cli() | lyra::help(show_help) |
      lyra::arg(run_cycles, "run-cycles")("Number of clock cycles to run for");

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

  histograms::Histogram<histograms::LogLinearBucketer<384, 4, 1>> histogram;

  unsigned int dummy;
  const uint64_t end_cycles = __rdtscp(&dummy) + run_cycles;
  uint64_t prev_cycles = __rdtscp(&dummy);
  while (prev_cycles < end_cycles) {
    const uint64_t curr_cycles = __rdtscp(&dummy);
    histogram.observe(curr_cycles - prev_cycles);
    prev_cycles = curr_cycles;
  }

  std::vector percentiles{0.5, 0.9, 0.95, 0.99, 0.999, 0.9999, 0.99999, 1.0};
  auto percentile_values = histogram.percentiles(percentiles);

  std::cout << "Percentiles:" << std::endl;
  for (size_t i = 0; i < percentiles.size(); ++i) {
    std::println("  {:>8}: {:>8} cycles", percentiles[i], percentile_values[i]);
  }

  auto data = histogram.data();
  std::cout << "Bucket data:" << std::endl;
  for (const auto& bucket : data) {
    std::println("  {:>8}: {:>10}", bucket.first, bucket.second);
  }

  return 0;
}