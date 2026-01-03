#!/usr/bin/env python3
"""
Interleaved A/B Benchmark Runner for btree_benchmark

Runs multiple btree configurations in an interleaved forward/reverse pattern
to eliminate sequential bias and reduce variance.

Pattern:
  Pass 1,3,5,7,9:  A → B → C → D (forward)
  Pass 2,4,6,8,10: D → C → B → A (reverse)

This reduces variance from ~6% (sequential) to ~1% (interleaved).
"""

import argparse
import subprocess
import json
import sys
from pathlib import Path
from typing import List, Dict, Any
from collections import defaultdict
from statistics import median, stdev


def run_benchmark(binary: Path, config: str, iterations: int = 1,
                  tree_size: int = 1000000, batches: int = 100,
                  batch_size: int = 1000, record_rampup: bool = False,
                  seed: int = 42, taskset_core: int = -1) -> Dict[str, Any]:
    """Run a single benchmark and return parsed JSON results."""

    cmd = []

    # Add taskset if core pinning requested
    if taskset_core >= 0:
        cmd.extend(["taskset", "-c", str(taskset_core)])

    cmd.extend([
        str(binary),
        "-j",  # JSON output
        "-i", str(iterations),
        "-t", str(tree_size),
        "-b", str(batches),
        "-s", str(batch_size),
        "-d", str(seed),
        config  # benchmark name
    ])

    if not record_rampup:
        cmd.extend(["-r", "false"])

    print(f"  Running: {config}...", end="", flush=True)

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=True
        )

        data = json.loads(result.stdout)
        print(" ✓")
        return data

    except subprocess.CalledProcessError as e:
        print(f" ✗ (exit code {e.returncode})")
        print(f"Error output: {e.stderr}")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f" ✗ (invalid JSON)")
        print(f"Output: {result.stdout[:200]}")
        sys.exit(1)


def interleaved_benchmark(
    binary: Path,
    configs: List[str],
    num_passes: int = 10,
    iterations: int = 1,
    tree_size: int = 1000000,
    batches: int = 100,
    batch_size: int = 1000,
    record_rampup: bool = False,
    seed: int = 42,
    taskset_core: int = -1
) -> Dict[str, List[Dict[str, Any]]]:
    """
    Run interleaved forward/reverse passes for all configs.

    Returns:
        Dict mapping config name to list of results (one per pass)
    """

    results = defaultdict(list)

    for pass_num in range(1, num_passes + 1):
        # Odd passes: forward (A → B → C)
        # Even passes: reverse (C → B → A)
        if pass_num % 2 == 1:
            print(f"\nPass {pass_num}/{num_passes} (forward: {' → '.join(configs)})")
            order = configs
        else:
            print(f"\nPass {pass_num}/{num_passes} (reverse: {' → '.join(reversed(configs))})")
            order = list(reversed(configs))

        for config in order:
            result = run_benchmark(
                binary, config, iterations, tree_size, batches,
                batch_size, record_rampup, seed, taskset_core
            )
            results[config].append(result)

    return results


def aggregate_results(results: Dict[str, List[Dict[str, Any]]]) -> Dict[str, Dict[str, Any]]:
    """
    Aggregate results across all passes for each config.

    Returns:
        Dict mapping config name to aggregated stats
    """

    aggregated = {}

    for config, pass_results in results.items():
        if not pass_results:
            print(f"WARNING: No results collected for config '{config}'")
            continue

        try:
            # Extract P99.9 values from each pass for each operation
            insert_p99_9_values = [r["performance"]["insert"]["p99_9"] for r in pass_results]
            find_p99_9_values = [r["performance"]["find"]["p99_9"] for r in pass_results]
            erase_p99_9_values = [r["performance"]["erase"]["p99_9"] for r in pass_results]

            # Extract P50 values from each pass for each operation
            insert_p50_values = [r["performance"]["insert"]["p50"] for r in pass_results]
            find_p50_values = [r["performance"]["find"]["p50"] for r in pass_results]
            erase_p50_values = [r["performance"]["erase"]["p50"] for r in pass_results]
        except KeyError as e:
            print(f"ERROR: Missing key {e} in performance data for config '{config}'")
            print(f"Sample result keys: {pass_results[0].keys() if pass_results else 'N/A'}")
            sys.exit(1)

        aggregated[config] = {
            "passes": len(pass_results),
            # Insert P99.9
            "insert_p99_9_median": median(insert_p99_9_values),
            "insert_p99_9_min": min(insert_p99_9_values),
            "insert_p99_9_max": max(insert_p99_9_values),
            "insert_p99_9_stdev": stdev(insert_p99_9_values) if len(insert_p99_9_values) > 1 else 0,
            "insert_p99_9_values": insert_p99_9_values,
            # Insert P50
            "insert_p50_median": median(insert_p50_values),
            "insert_p50_min": min(insert_p50_values),
            "insert_p50_max": max(insert_p50_values),
            "insert_p50_stdev": stdev(insert_p50_values) if len(insert_p50_values) > 1 else 0,
            "insert_p50_values": insert_p50_values,
            # Find P99.9
            "find_p99_9_median": median(find_p99_9_values),
            "find_p99_9_min": min(find_p99_9_values),
            "find_p99_9_max": max(find_p99_9_values),
            "find_p99_9_stdev": stdev(find_p99_9_values) if len(find_p99_9_values) > 1 else 0,
            "find_p99_9_values": find_p99_9_values,
            # Find P50
            "find_p50_median": median(find_p50_values),
            "find_p50_min": min(find_p50_values),
            "find_p50_max": max(find_p50_values),
            "find_p50_stdev": stdev(find_p50_values) if len(find_p50_values) > 1 else 0,
            "find_p50_values": find_p50_values,
            # Erase P99.9
            "erase_p99_9_median": median(erase_p99_9_values),
            "erase_p99_9_min": min(erase_p99_9_values),
            "erase_p99_9_max": max(erase_p99_9_values),
            "erase_p99_9_stdev": stdev(erase_p99_9_values) if len(erase_p99_9_values) > 1 else 0,
            "erase_p99_9_values": erase_p99_9_values,
            # Erase P50
            "erase_p50_median": median(erase_p50_values),
            "erase_p50_min": min(erase_p50_values),
            "erase_p50_max": max(erase_p50_values),
            "erase_p50_stdev": stdev(erase_p50_values) if len(erase_p50_values) > 1 else 0,
            "erase_p50_values": erase_p50_values,
            # Store full performance data from median run
            "median_run": pass_results[len(pass_results) // 2]["performance"]
        }

    return aggregated


def print_comparison_table(aggregated: Dict[str, Dict[str, Any]],
                          title: str,
                          metric_key: str,
                          metric_label: str,
                          show_variance: bool = True):
    """
    Generic comparison table printer.

    Args:
        aggregated: Aggregated results dictionary
        title: Table title
        metric_key: Key prefix for the metric (e.g., 'insert_p99_9', 'find_p50')
        metric_label: Display label for the metric column
        show_variance: Whether to show min/max/passes columns and variance analysis
    """

    print("\n" + "=" * 100)
    print(title)
    print("=" * 100)

    # Sort by metric median (best to worst)
    median_key = f"{metric_key}_median"
    sorted_configs = sorted(aggregated.items(), key=lambda x: x[1][median_key])

    # Find baseline (first config)
    if not sorted_configs:
        print(f"ERROR: No configs to compare for {title}")
        return

    baseline = sorted_configs[0][1][median_key]

    # Debug: Check for zero baseline
    if baseline == 0:
        print(f"ERROR: Baseline is zero for metric '{metric_key}'")
        print(f"  Winner config: {sorted_configs[0][0]}")
        print(f"  Winner stats: {sorted_configs[0][1]}")
        sys.exit(1)

    # Header
    if show_variance:
        print(f"\n{'Config':<30} {'Passes':<8} {metric_label:<14} {'Min':<10} {'Max':<10} {'StdDev':<10} {'vs Best':<10}")
    else:
        print(f"\n{'Config':<30} {metric_label:<14} {'StdDev':<10} {'vs Best':<10}")
    print("-" * 100)

    for config, stats in sorted_configs:
        median_val = stats[median_key]
        stdev_val = stats[f"{metric_key}_stdev"]

        # Calculate percentage difference from baseline
        if baseline != 0:
            diff_pct = ((median_val - baseline) / baseline) * 100
        else:
            diff_pct = 0.0 if median_val == 0 else float('inf')

        # Mark winner
        marker = "⭐" if config == sorted_configs[0][0] else "  "
        if config == sorted_configs[0][0]:
            diff_str = "baseline"
        elif baseline == 0:
            diff_str = "N/A"
        else:
            diff_str = f"{diff_pct:+.2f}%"

        if show_variance:
            min_val = stats[f"{metric_key}_min"]
            max_val = stats[f"{metric_key}_max"]
            passes = stats["passes"]
            print(f"{marker} {config:<28} {passes:<8} {median_val:<14.1f} {min_val:<10.1f} {max_val:<10.1f} "
                  f"{stdev_val:<10.2f} {diff_str:<10}")
        else:
            print(f"{marker} {config:<28} {median_val:<14.1f} {stdev_val:<10.2f} {diff_str:<10}")

    print("=" * 100)

    # Variance analysis for winner (if requested)
    if show_variance:
        winner_config, winner_stats = sorted_configs[0]
        print(f"\n🏆 Winner: {winner_config}")
        print(f"\nVariance analysis:")
        min_val = winner_stats[f"{metric_key}_min"]
        max_val = winner_stats[f"{metric_key}_max"]
        median_val = winner_stats[median_key]
        stdev_val = winner_stats[f"{metric_key}_stdev"]
        print(f"  Range: {max_val - min_val:.1f} ns "
              f"({((max_val - min_val) / median_val * 100):.2f}%)")
        print(f"  StdDev: {stdev_val:.2f} ns "
              f"({(stdev_val / median_val * 100):.2f}%)")


def print_detailed_percentiles(aggregated: Dict[str, Dict[str, Any]], operation: str):
    """Print detailed percentile breakdown for the P99.9 winner of a specific operation."""

    # Find winner by operation P99.9
    metric_key = f"{operation}_p99_9_median"
    sorted_configs = sorted(aggregated.items(), key=lambda x: x[1][metric_key])
    winner_config, winner_stats = sorted_configs[0]

    print("\n" + "=" * 100)
    print(f"DETAILED PERCENTILES - {winner_config} (P99.9 Winner for {operation.upper()})")
    print("=" * 100)

    median_run = winner_stats["median_run"][operation]
    print(f"\n{operation.capitalize()} latencies (from median run):")
    print(f"  P0:     {median_run['p0']:>10.2f} ns")
    print(f"  P50:    {median_run['p50']:>10.2f} ns")
    print(f"  P95:    {median_run['p95']:>10.2f} ns")
    print(f"  P99:    {median_run['p99']:>10.2f} ns")
    print(f"  P99.9:  {median_run['p99_9']:>10.2f} ns")
    print(f"  P99.99: {median_run['p99_99']:>10.2f} ns")
    print("=" * 100)


def export_csv(aggregated: Dict[str, Dict[str, Any]], output_file: Path):
    """Export results to CSV for further analysis."""

    import csv

    with output_file.open('w', newline='') as f:
        writer = csv.writer(f)

        # Header
        writer.writerow([
            "config", "passes",
            "insert_p99_9_median", "insert_p99_9_min", "insert_p99_9_max", "insert_p99_9_stdev",
            "find_p99_9_median", "find_p99_9_min", "find_p99_9_max", "find_p99_9_stdev",
            "erase_p99_9_median", "erase_p99_9_min", "erase_p99_9_max", "erase_p99_9_stdev"
        ])

        # Data rows
        for config, stats in sorted(aggregated.items(), key=lambda x: x[1]["insert_p99_9_median"]):
            writer.writerow([
                config,
                stats["passes"],
                stats["insert_p99_9_median"],
                stats["insert_p99_9_min"],
                stats["insert_p99_9_max"],
                stats["insert_p99_9_stdev"],
                stats["find_p99_9_median"],
                stats["find_p99_9_min"],
                stats["find_p99_9_max"],
                stats["find_p99_9_stdev"],
                stats["erase_p99_9_median"],
                stats["erase_p99_9_min"],
                stats["erase_p99_9_max"],
                stats["erase_p99_9_stdev"]
            ])

    print(f"\n✓ Exported CSV to {output_file}")


def export_json(results: Dict[str, List[Dict[str, Any]]], output_file: Path):
    """Export raw results to JSON for archival."""

    with output_file.open('w') as f:
        json.dump(results, f, indent=2)

    print(f"✓ Exported raw JSON to {output_file}")


def main():
    parser = argparse.ArgumentParser(
        description="Interleaved A/B benchmark runner for btree_benchmark",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Compare 3 configs with 10 interleaved passes
  %(prog)s -c btree_8_32_96_128_linear btree_8_32_96_128_simd btree_8_32_96_128_linear_hp -p 10

  # Quick test with fewer iterations
  %(prog)s -c btree_8_32_96_128_linear btree_8_32_96_128_simd -p 3 -i 100 -t 10000

  # Pin to isolated CPU core for reduced variance
  %(prog)s -c btree_8_32_96_128_linear btree_8_32_96_128_simd -p 10 --taskset 7

  # Export results to CSV
  %(prog)s -c btree_8_32_96_128_linear btree_8_32_96_128_simd -p 10 --csv results.csv
        """
    )

    parser.add_argument(
        "-b", "--binary",
        type=Path,
        default=Path("./cmake-build-release/src/binary/btree_benchmark"),
        help="Path to btree_benchmark binary (default: ./cmake-build-release/src/binary/btree_benchmark)"
    )

    parser.add_argument(
        "-c", "--configs",
        nargs="+",
        required=True,
        help="List of configurations to benchmark (e.g., btree_8_32_96_128_linear btree_8_32_96_128_simd)"
    )

    parser.add_argument(
        "-p", "--passes",
        type=int,
        default=10,
        help="Number of interleaved passes (default: 10)"
    )

    parser.add_argument(
        "-i", "--iterations",
        type=int,
        default=1,
        help="Number of iterations per run (default: 1)"
    )

    parser.add_argument(
        "-t", "--tree-size",
        type=int,
        default=1000000,
        help="Minimum keys to target in tree (default: 1000000)"
    )

    parser.add_argument(
        "--batches",
        type=int,
        default=100,
        help="Number of erase/insert batches to run (default: 100)"
    )

    parser.add_argument(
        "--batch-size",
        type=int,
        default=1000,
        help="Size of an erase/insert batch (default: 1000)"
    )

    parser.add_argument(
        "--no-record-rampup",
        action="store_true",
        help="Don't record stats during rampup in tree size"
    )

    parser.add_argument(
        "-d", "--seed",
        type=int,
        default=42,
        help="Random seed (default: 42)"
    )

    parser.add_argument(
        "--taskset",
        type=int,
        default=-1,
        metavar="CORE",
        help="Pin benchmark to specific CPU core using taskset (default: -1 = no pinning)"
    )

    parser.add_argument(
        "--csv",
        type=Path,
        help="Export results to CSV file"
    )

    parser.add_argument(
        "--json",
        type=Path,
        help="Export raw results to JSON file"
    )

    args = parser.parse_args()

    # Validate inputs
    if not args.binary.exists():
        print(f"Error: Binary not found: {args.binary}")
        sys.exit(1)

    if args.passes < 2:
        print("Error: Need at least 2 passes for interleaved testing")
        sys.exit(1)

    # Print configuration
    print("=" * 100)
    print("INTERLEAVED BENCHMARK CONFIGURATION")
    print("=" * 100)
    print(f"Binary:       {args.binary}")
    print(f"Configs:      {', '.join(args.configs)}")
    print(f"Passes:       {args.passes}")
    print(f"Iterations:   {args.iterations}")
    print(f"Tree size:    {args.tree_size:,}")
    print(f"Batches:      {args.batches}")
    print(f"Batch size:   {args.batch_size}")
    print(f"Seed:         {args.seed}")
    if args.taskset >= 0:
        print(f"CPU core:     {args.taskset} (pinned with taskset)")
    print("=" * 100)

    # Run interleaved benchmarks
    results = interleaved_benchmark(
        args.binary,
        args.configs,
        args.passes,
        args.iterations,
        args.tree_size,
        args.batches,
        args.batch_size,
        not args.no_record_rampup,
        args.seed,
        args.taskset
    )

    # Aggregate results
    aggregated = aggregate_results(results)

    # Print comparison tables
    print("\n" + "=" * 100)
    print("INTERLEAVED BENCHMARK RESULTS")
    print("=" * 100)

    # Insert latency comparisons
    print_comparison_table(aggregated, "INSERT P99.9 LATENCY COMPARISON",
                          "insert_p99_9", "P99.9 Median (ns)", show_variance=True)
    print_comparison_table(aggregated, "INSERT P50 (MEDIAN) LATENCY COMPARISON",
                          "insert_p50", "P50 Median (ns)", show_variance=True)

    # Find latency comparisons
    print_comparison_table(aggregated, "FIND P99.9 LATENCY COMPARISON",
                          "find_p99_9", "P99.9 Median (ns)", show_variance=True)
    print_comparison_table(aggregated, "FIND P50 (MEDIAN) LATENCY COMPARISON",
                          "find_p50", "P50 Median (ns)", show_variance=True)

    # Erase latency comparisons
    print_comparison_table(aggregated, "ERASE P99.9 LATENCY COMPARISON",
                          "erase_p99_9", "P99.9 Median (ns)", show_variance=True)
    print_comparison_table(aggregated, "ERASE P50 (MEDIAN) LATENCY COMPARISON",
                          "erase_p50", "P50 Median (ns)", show_variance=True)

    # Detailed percentiles for winners
    print_detailed_percentiles(aggregated, "insert")
    print_detailed_percentiles(aggregated, "find")
    print_detailed_percentiles(aggregated, "erase")

    # Export if requested
    if args.csv:
        export_csv(aggregated, args.csv)

    if args.json:
        export_json(results, args.json)

    print("\n✓ Benchmark complete!")


if __name__ == "__main__":
    main()
