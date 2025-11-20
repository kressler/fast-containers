# B+Tree Benchmarking Results

## Naming conventions
`absl` --> Abseil B Tree implementation
`btree` --> Custom B Tree implementation

### Parameters
absl:
```
absl_K_V --> absl::btree_map<std::array<int64_t, K/8>, std::array<std::byte, V>>
```

btree:
````
btree_K_V_L_I_S_M --> btree::btree_map<std::array<int64_t, K/8>, std::array<std::byte, V>, L, I, S, M>
````

## 16 and 32 byte keys, 32 byte values

```
./cmake-build-release/src/binary/btree_benchmark -i 25 -t 100000 --batch-size 99990 --batches 10 absl_16_32 btree_16_32_16_128_binary_simd btree_16_32_16_128_linear_simd btree_16_32_16_128_simd_simd absl_32_32 btree_32_32_16_128_binary_simd btree_32_32_16_128_linear_simd btree_32_32_16_128_simd_simd
absl_16_32: Insert: 31652663902, Find: 11675265848, Erase: 9970921702, Iterate: 1756619078
btree_16_32_16_128_binary_simd: Insert: 24067689888, Find: 10151067506, Erase: 8095617634, Iterate: 730247308
btree_16_32_16_128_linear_simd: Insert: 23595979090, Find: 8517859384, Erase: 7244591854, Iterate: 810650678
btree_16_32_16_128_simd_simd: Insert: 28902533306, Find: 15923929512, Erase: 13148176856, Iterate: 466678458
absl_32_32: Insert: 37839225770, Find: 14165502926, Erase: 11950825988, Iterate: 2321378458
btree_32_32_16_128_binary_simd: Insert: 28278379904, Find: 12528030082, Erase: 10436633642, Iterate: 780486524
btree_32_32_16_128_linear_simd: Insert: 26766678840, Find: 9927642660, Erase: 8654050430, Iterate: 813429702
btree_32_32_16_128_simd_simd: Insert: 46870897710, Find: 29279189488, Erase: 26019907090, Iterate: 834288294
```