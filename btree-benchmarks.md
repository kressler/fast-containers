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

1,000,000 item trees

```
./cmake-build-release/src/binary/btree_benchmark -i 100 -t 1000000 --batch-size 500000 --batches 10 absl_16_256 absl_16_32 absl_32_256 absl_32_32 absl_8_256 absl_8_32 btree_16_256_16_128_binary_simd btree_16_256_16_128_linear_simd btree_16_32_128_128_binary_simd btree_16_32_128_128_linear_simd btree_16_32_16_128_binary_simd btree_16_32_16_128_linear_simd btree_32_256_16_128_binary_simd btree_32_256_16_128_linear_simd btree_32_32_128_128_binary_simd btree_32_32_128_128_linear_simd btree_32_32_16_128_binary_simd btree_32_32_16_128_linear_simd btree_8_256_16_128_binary_simd btree_8_256_16_128_linear_simd btree_8_256_16_128_simd_simd btree_8_256_64_64_simd_simd btree_8_32_16_128_binary_simd btree_8_32_16_128_linear_simd btree_8_32_16_128_simd_simd btree_8_32_64_64_binary_simd btree_8_32_64_64_linear_simd btree_8_32_64_64_simd_simd
```

```
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
                             absl_16_256,    2410133498036,     622001356496,     387518787702,      60615692626
                              absl_16_32,    1103217604196,     423990989854,     251267336180,      48265400400
                             absl_32_256,    2476806683872,     642482266228,     399169110436,      61092461876
                              absl_32_32,    1241037425738,     473012992322,     278097408428,      59333122730
                              absl_8_256,    1971294130624,     479342032142,     299832700152,      60575282742
                               absl_8_32,     855970563914,     302616763054,     181671303998,      39887982062
         btree_16_256_16_128_binary_simd,    1480833525208,     350058623172,     212356737668,      20268423910
         btree_16_256_16_128_linear_simd,    1365000347054,     348789081396,     187807086492,      20303429970
         btree_16_32_128_128_binary_simd,    1011479276558,     222510025306,     121442554288,       3833556746
         btree_16_32_128_128_linear_simd,     935584405702,     247210261972,     113182638012,       3910157590
          btree_16_32_16_128_binary_simd,     987098873778,     327163759000,     202748844370,      18238463770
          btree_16_32_16_128_linear_simd,     834511833052,     323516580530,     176916901792,      18259773576
         btree_32_256_16_128_binary_simd,    1614128576872,     382721891004,     228949019086,      20419401386
         btree_32_256_16_128_linear_simd,    1503291431274,     370225490544,     195669446650,      20585521170
         btree_32_32_128_128_binary_simd,    1214104775286,     252371200376,     138712894646,       3312196496
         btree_32_32_128_128_linear_simd,    1103443600190,     267507741058,     128447723820,       3385193714
          btree_32_32_16_128_binary_simd,    1104017519034,     354696424282,     215681473024,      18420791796
          btree_32_32_16_128_linear_simd,     918063717734,     331655038878,     177555729084,      18386094422
          btree_8_256_16_128_binary_simd,    1396230453686,     308735690422,     187153978758,      19786429268
          btree_8_256_16_128_linear_simd,    1178091507616,     270639204876,     153920770762,      19732548992
            btree_8_256_16_128_simd_simd,    1164448694646,     269733084680,     153681866906,      19594151074
             btree_8_256_64_64_simd_simd,    1512185174156,     200342070322,     106048531198,       6073431968
           btree_8_32_16_128_binary_simd,     956378540298,     296064150250,     177724807208,      18168010534
           btree_8_32_16_128_linear_simd,     746050086502,     260546726986,     145941442342,      18219063710
             btree_8_32_16_128_simd_simd,     733437313562,     260504472296,     146887957938,      18248556160
            btree_8_32_64_64_binary_simd,     833901788528,     213647915718,     112886814382,       5334582150
            btree_8_32_64_64_linear_simd,     723355006228,     177645178800,      90626830612,       5722241072
              btree_8_32_64_64_simd_simd,     737940629168,     191643955314,     100248790560,       5644447848
```
