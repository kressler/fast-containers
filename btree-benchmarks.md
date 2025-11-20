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

## 16 and 32 byte keys, 256 byte values

Build trees with 100,000 values, then run batches of 99,990 removes followed by 99,990 inserts. Finds and inserts run on full sized trees.

```
$  ./cmake-build-release/src/binary/btree_benchmark -i 25 -t 100000 --batch-size 99990 --batches 10 absl_16_256 btree_16_256_16_128_binary_simd btree_16_256_16_128_linear_simd  btree_16_256_16_128_simd_simd absl_32_256 btree_32_256_16_128_binary_simd btree_32_256_16_128_linear_simd btree_32_256_16_128_simd_simd
absl_16_256: Insert: 78091619730, Find: 19511726540, Erase: 16025873888, Iterate: 2527129050
btree_16_256_16_128_binary_simd: Insert: 47548089956, Find: 12410790296, Erase: 9093463574, Iterate: 804665250
btree_16_256_16_128_linear_simd: Insert: 47194102356, Find: 9641649586, Erase: 7959959572, Iterate: 795893488
btree_16_256_16_128_simd_simd: Insert: 47244747736, Find: 9653775788, Erase: 8177831062, Iterate: 843055976
absl_32_256: Insert: 80034290344, Find: 20349390762, Erase: 16833290146, Iterate: 2480644046
btree_32_256_16_128_binary_simd: Insert: 51080827350, Find: 14125602702, Erase: 11336285202, Iterate: 798221944
btree_32_256_16_128_linear_simd: Insert: 51687647314, Find: 10784933212, Erase: 8948799456, Iterate: 854953562
btree_32_256_16_128_simd_simd: Insert: 50787068676, Find: 10697185026, Erase: 8844211410, Iterate: 853905070
```