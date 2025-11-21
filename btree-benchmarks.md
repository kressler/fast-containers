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

## Large number of benchmarks

100,000 item trees, inserting and removing batches of 99,990 elements (so measurements are mixed across large and small trees, and we should see lots of splits and merges).

```
./cmake-build-release/src/binary/btree_benchmark -i 1000 -t 100000 --batch-size 99990 --batches 10 absl_16_256 absl_16_32 absl_32_256 absl_32_32 absl_8_256 btree_16_256_16_128_binary_simd btree_16_256_16_128_linear_simd btree_16_32_128_128_binary_simd btree_16_32_128_128_linear_simd btree_16_32_16_128_binary_simd btree_16_32_16_128_linear_simd btree_32_256_16_128_binary_simd btree_32_256_16_128_linear_simd btree_32_32_128_128_binary_simd btree_32_32_128_128_linear_simd btree_32_32_16_128_binary_simd btree_32_32_16_128_linear_simd btree_8_256_16_128_binary_simd btree_8_256_16_128_linear_simd btree_8_256_16_128_simd_simd btree_8_256_64_64_simd_simd
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
                             absl_16_256,    2597546473698,     752312894492,     607883870036,      94032230356
                              absl_16_32,    1080893206474,     422698944090,     361987980506,      63751449828
                             absl_32_256,    2766477869134,     783000050092,     636897434014,      94738084398
                              absl_32_32,    1286250396340,     510992508280,     435555534634,      86210549600
                              absl_8_256,    2188642288714,     536990444366,     448096662672,      92404038084
         btree_16_256_16_128_binary_simd,    1648504926374,     476090768814,     343262338386,      30373517874
         btree_16_256_16_128_linear_simd,    1593652985182,     366027303994,     302987766022,      31767528822
         btree_16_32_128_128_binary_simd,     851286108854,     248712784668,     212093948302,       4585200696
         btree_16_32_128_128_linear_simd,     849655095194,     227384472366,     201357475154,       5178853854
          btree_16_32_16_128_binary_simd,     909489863580,     395298451088,     315896366808,      26142484264
          btree_16_32_16_128_linear_simd,     859993288772,     326007162164,     281607951812,      28078918632
         btree_32_256_16_128_binary_simd,    1802160307654,     550049826108,     441523102060,      30449327810
         btree_32_256_16_128_linear_simd,    1731931771772,     407517231212,     341975485816,      32331516470
         btree_32_32_128_128_binary_simd,    1089726921540,     320570345550,     261589663046,       4026440276
         btree_32_32_128_128_linear_simd,    1057452502766,     252406272328,     226670767044,       4585505642
          btree_32_32_16_128_binary_simd,    1046912193026,     495155828650,     417211293362,      28341809454
          btree_32_32_16_128_linear_simd,     936648921426,     374891831322,     329125440272,      29759173268
          btree_8_256_16_128_binary_simd,    1539149068772,     357852856170,     233567765678,      29845289624
          btree_8_256_16_128_linear_simd,    1525594738456,     301318194220,     216450392918,      28411312186
            btree_8_256_16_128_simd_simd,    1520896512070,     285606406858,     213803440864,      27576095386
             btree_8_256_64_64_simd_simd,    1964747058426,     181649212838,     139741834606,       8744373674
```

100,000 item trees, inserting and removing batches of 1,000 elements (so measurements are more against full trees, and we have less merging and splitting).

```
./cmake-build-release/src/binary/btree_benchmark -i 10 -t 100000 --batch-size 1000 --batches 1000 absl_16_256 absl_16_32 absl_32_256 absl_32_32 absl_8_256 btree_16_256_16_128_binary_simd btree_16_256_16_128_linear_simd btree_16_32_128_128_binary_simd btree_16_32_128_128_linear_simd btree_16_32_16_128_binary_simd btree_16_32_16_128_linear_simd btree_32_256_16_128_binary_simd btree_32_256_16_128_linear_simd btree_32_32_128_128_binary_simd btree_32_32_128_128_linear_simd btree_32_32_16_128_binary_simd btree_32_32_16_128_linear_simd btree_8_256_16_128_binary_simd btree_8_256_16_128_linear_simd btree_8_256_16_128_simd_simd btree_8_256_64_64_simd_simd
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
                             absl_16_256,      29063559584,     235672898288,       7563077504,       1052184502
                              absl_16_32,      14545839846,     197090280470,       5270182750,        764393406
                             absl_32_256,      29587391294,     242814810482,       8383338860,       1021212848
                              absl_32_32,      17337941300,     222426864672,       6755125882,        976641976
                              absl_8_256,      23304034928,     158366537390,       5184944682,        998590064
         btree_16_256_16_128_binary_simd,      15880138124,     206597006632,       3862980448,        331264686
         btree_16_256_16_128_linear_simd,      15293099054,     198284871184,       3611236458,        339486464
         btree_16_32_128_128_binary_simd,      11017071408,     208107827776,       3401776534,         56036012
         btree_16_32_128_128_linear_simd,      10904786306,     224407209326,       3148790490,         59706414
          btree_16_32_16_128_binary_simd,      10657979660,     208219988540,       3759885988,        318578946
          btree_16_32_16_128_linear_simd,       9993979006,     196454492404,       3514304770,        332557468
         btree_32_256_16_128_binary_simd,      17750253962,     241983839568,       4408862552,        353014316
         btree_32_256_16_128_linear_simd,      17224578296,     218792803266,       4061676210,        355370210
         btree_32_32_128_128_binary_simd,      14148964952,     243872587474,       3833617538,         45559014
         btree_32_32_128_128_linear_simd,      13584477368,     234243886856,       3618255282,         49209424
          btree_32_32_16_128_binary_simd,      12421727722,     240987870394,       4355663840,        338897788
          btree_32_32_16_128_linear_simd,      11092320038,     215873410232,       4023124732,        340189210
          btree_8_256_16_128_binary_simd,      14423332168,     146706524112,       3396090102,        318924556
          btree_8_256_16_128_linear_simd,      13761423058,     148859084070,       3166632466,        318835646
            btree_8_256_16_128_simd_simd,      13515346154,     141117342006,       2926482204,        319182242
             btree_8_256_64_64_simd_simd,      20221584490,     105507688574,       2440052908,         98868464
```
