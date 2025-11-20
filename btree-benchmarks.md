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
$ ./cmake-build-release/src/binary/btree_benchmark -i 25 -t 100000 --batch-size 99990 --batches 10 absl_16_256 btree_16_256_16_128_binary_simd btree_16_256_16_128_linear_simd absl_32_256 btree_32_256_16_128_binary_simd btree_32_256_16_128_linear_simd

absl_16_256: Insert: 76026299420, Find: 19083935684, Erase: 15560795470, Iterate: 2452092546
btree_16_256_16_128_binary_simd: Insert: 46755839194, Find: 12143901992, Erase: 9295895936, Iterate: 818731424
btree_16_256_16_128_linear_simd: Insert: 46927121300, Find: 9505361946, Erase: 7999602552, Iterate: 812603502
absl_32_256: Insert: 78986135794, Find: 20181945964, Erase: 16598025174, Iterate: 2490640692
btree_32_256_16_128_binary_simd: Insert: 50159313008, Find: 13887257534, Erase: 10870326558, Iterate: 799617712
btree_32_256_16_128_linear_simd: Insert: 50768113336, Find: 10517524402, Erase: 8827891444, Iterate: 834153688
```
