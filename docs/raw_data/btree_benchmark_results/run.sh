#!/bin/bash

./scripts/interleaved_btree_benchmark.py -b cmake-build-clang-release/src/binary/btree_benchmark -p 11 -t 10000 --batch-size 5000 --batches 100 -i 25 --taskset 15 -c absl_8_32 absl_8_32_hp map_8_32 btree_8_32_96_128_linear btree_8_32_96_128_linear_hp btree_8_32_96_128_simd btree_8_32_96_128_simd_hp | tee clang_8_32_output_small.txt

./scripts/interleaved_btree_benchmark.py -b cmake-build-clang-release/src/binary/btree_benchmark -p 11 -t 10000 --batch-size 5000 --batches 100 -i 25 --taskset 15 -c absl_8_256 absl_8_256_hp btree_8_256_8_128_linear btree_8_256_8_128_linear_hp btree_8_256_8_128_simd btree_8_256_8_128_simd_hp map_8_256 | tee clang_8_256_output_small.txt

./scripts/interleaved_btree_benchmark.py -b cmake-build-clang-release/src/binary/btree_benchmark -p 5 -t 10000000 --batch-size 50000 --batches 25 --taskset 15 -c absl_8_32 absl_8_32_hp map_8_32 btree_8_32_96_128_linear btree_8_32_96_128_linear_hp btree_8_32_96_128_simd btree_8_32_96_128_simd_hp | tee clang_8_32_output_large.txt

./scripts/interleaved_btree_benchmark.py -b cmake-build-clang-release/src/binary/btree_benchmark -p 5 -t 10000000 --batch-size 50000 --batches 25 --taskset 15 -c absl_8_256 absl_8_256_hp btree_8_256_8_128_linear btree_8_256_8_128_linear_hp btree_8_256_8_128_simd btree_8_256_8_128_simd_hp map_8_256 | tee clang_8_256_output_large.txt

