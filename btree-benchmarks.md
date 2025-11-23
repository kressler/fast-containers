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

Adding in std::map and std::unordered_map:

```
./cmake-build-release/src/binary/btree_benchmark -i 5 -t 1000000 --batch-size 500000 --batches 10 absl_16_256 absl_16_32 absl_32_256 absl_32_32 absl_8_256 absl_8_32 btree_16_256_16_128_linear_simd btree_16_32_128_128_linear_simd btree_16_32_16_128_linear_simd btree_32_256_16_128_linear_simd btree_32_32_128_128_linear_simd btree_32_32_16_128_linear_simd btree_8_256_16_128_linear_simd btree_8_256_16_128_simd_simd btree_8_256_64_64_simd_simd btree_8_32_16_128_linear_simd btree_8_32_16_128_simd_simd btree_8_32_64_64_linear_simd btree_8_32_64_64_simd_simd map_16_256 map_16_32 map_32_256 map_32_32 map_8_256 map_8_32 unordered_map_16_256 unordered_map_16_32 unordered_map_32_256 unordered_map_32_32 unordered_map_8_256 unordered_map_8_32
```

```
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
                             absl_16_256,     114571107912,     145597000480,      58153973184,       3064704172
                              absl_16_32,      57974943192,      98886829406,      37979872380,       2485753294
                             absl_32_256,     116333610486,     145850589208,      57891330358,       3024381974
                              absl_32_32,      62813691646,     110763603054,      43123359842,       2969809628
                              absl_8_256,      92022364896,     103096716512,      39556127612,       3024733704
                               absl_8_32,      41719089292,      75192411918,      26867544906,       2024429020
         btree_16_256_16_128_linear_simd,      60785610346,      62361517996,      21162689428,       1025408822
         btree_16_32_128_128_linear_simd,      45553933686,      53098247032,      17564404760,        184597152
          btree_16_32_16_128_linear_simd,      41251462302,      57211942970,      19026783690,        958966702
         btree_32_256_16_128_linear_simd,      65751015192,      71978648584,      25855660026,       1100732728
         btree_32_32_128_128_linear_simd,      53766750510,      62382479846,      21461469800,        187935510
          btree_32_32_16_128_linear_simd,      42838757232,      64422382130,      22363739666,        945363914
          btree_8_256_16_128_linear_simd,      54561601558,      52777902194,      17326597030,        998225380
            btree_8_256_16_128_simd_simd,      54333923550,      52850684226,      17232192392,        996037140
             btree_8_256_64_64_simd_simd,      74894594732,      49721085104,      15886743848,        300861376
           btree_8_32_16_128_linear_simd,      35006752954,      48365231268,      16131990336,        946641430
             btree_8_32_16_128_simd_simd,      34530411832,      48761186598,      15803308528,        929974664
            btree_8_32_64_64_linear_simd,      33576292102,      42480034860,      13140615902,        267032872
              btree_8_32_64_64_simd_simd,      34629436594,      44701957606,      14241085436,        268589528
                              map_16_256,      89769638660,     148853563184,      63004002362,      10778829124
                               map_16_32,      79413173516,     139932461216,      60545189376,      10246450970
                              map_32_256,      91745806618,     153359510288,      64266537112,      10742581860
                               map_32_32,      80471839472,     143892023798,      60730572166,      10421776220
                               map_8_256,      85073365676,     145100797408,      62324892244,      10599790734
                                map_8_32,      76503876374,     135710381438,      59339316340,      10082704386
                    unordered_map_16_256,      50213312590,      64311725118,      20146460572,       9508393828
                     unordered_map_16_32,      39751688098,      54618885098,      14888196318,       7903253352
                    unordered_map_32_256,      49932157410,      64672840886,      18717353498,       9577630986
                     unordered_map_32_32,      41656469112,      56822600538,      15646458062,       8521191622
                     unordered_map_8_256,      45078181546,      56435619236,      16076080532,       9452581298
                      unordered_map_8_32,      38060633056,      50489346758,      14241422818,       7167687118
```


## Some work on tuning

### Exploring 8 byte keys, 32 byte values

**Varying internal node size, holding leaf node size at 16 elements:**

```
./cmake-build-release/src/binary/btree_benchmark -i 5 -t 1000000 --batch-size 500000 --batches 10 btree_8_32_16_128_binary_simd btree_8_32_16_128_linear_simd btree_8_32_16_128_simd_simd btree_8_32_16_160_binary_simd btree_8_32_16_160_linear_simd btree_8_32_16_160_simd_simd btree_8_32_16_256_binary_simd btree_8_32_16_256_linear_simd btree_8_32_16_256_simd_simd btree_8_32_16_64_binary_simd btree_8_32_16_64_linear_simd btree_8_32_16_64_simd_simd btree_8_32_16_96_binary_simd btree_8_32_16_96_linear_simd btree_8_32_16_96_simd_simd
```

```
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
           btree_8_32_16_128_binary_simd,      45478404046,      67161009336,      24066543510,        964550352
           btree_8_32_16_128_linear_simd,      37053161138,      50553665408,      16727803616,        980837746
             btree_8_32_16_128_simd_simd,      36084635880,      50926677266,      16592552330,        979435620
           btree_8_32_16_160_binary_simd,      45276363840,      67537931092,      24501093158,        959849104
           btree_8_32_16_160_linear_simd,      37164904926,      51371948168,      16834634812,        984089880
             btree_8_32_16_160_simd_simd,      36285268418,      51729146456,      17056771092,        978820866
           btree_8_32_16_256_binary_simd,      44967977788,      66895940542,      24000113868,        954451366
           btree_8_32_16_256_linear_simd,      38322988854,      53147632508,      17788802346,        981989020
             btree_8_32_16_256_simd_simd,      36886069124,      52650678376,      17775435688,        978108090
            btree_8_32_16_64_binary_simd,      45909657428,      68415864098,      24689685582,        967109668
            btree_8_32_16_64_linear_simd,      37065701426,      51262592710,      16854521072,        978662086
              btree_8_32_16_64_simd_simd,      37334468434,      52439961744,      17496553680,        985627360
            btree_8_32_16_96_binary_simd,      45628529482,      67739555376,      24219419512,        964146058
            btree_8_32_16_96_linear_simd,      37461737370,      50810241070,      16701792358,        976284228
              btree_8_32_16_96_simd_simd,      36803040512,      51548547942,      16904510490,        977487386
```

It looks like linear simd is the fastest, and that internal node size 128 is fastest for finds, fastest for inserts, and one of the fastest for erases.

** Varying leaf node size, and locking internal node size at 128 elements:**

```
./cmake-build-release/src/binary/btree_benchmark -i 5 -t 1000000 --batch-size 500000 --batches 10 btree_8_32_128_128_binary_simd btree_8_32_128_128_linear_simd btree_8_32_128_128_simd_simd btree_8_32_16_128_binary_simd btree_8_32_16_128_linear_simd btree_8_32_16_128_simd_simd btree_8_32_24_128_binary_simd btree_8_32_24_128_linear_simd btree_8_32_24_128_simd_simd btree_8_32_32_128_binary_simd btree_8_32_32_128_linear_simd btree_8_32_32_128_simd_simd btree_8_32_48_128_binary_simd btree_8_32_48_128_linear_simd btree_8_32_48_128_simd_simd btree_8_32_64_128_binary_simd btree_8_32_64_128_linear_simd btree_8_32_64_128_simd_simd btree_8_32_96_128_binary_simd btree_8_32_96_128_linear_simd btree_8_32_96_128_simd_simd
```

```
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
          btree_8_32_128_128_binary_simd,      43252029816,      53675082872,      18148383898,        164467418
          btree_8_32_128_128_linear_simd,      39711096212,      43944703996,      13583234294,        155936682
            btree_8_32_128_128_simd_simd,      39160124954,      47123245914,      14451998486,        159569208
           btree_8_32_16_128_binary_simd,      45343246430,      67451158774,      24170756944,        962191330
           btree_8_32_16_128_linear_simd,      36954515442,      50608244078,      16595440392,        983236548
             btree_8_32_16_128_simd_simd,      35868905914,      50711074902,      16516740082,        979366600
           btree_8_32_24_128_binary_simd,      42579567854,      62215453520,      22219445060,        667851936
           btree_8_32_24_128_linear_simd,      37397884078,      49460235268,      16751736896,        681557982
             btree_8_32_24_128_simd_simd,      37612792298,      49556175856,      16093833666,        662253768
           btree_8_32_32_128_binary_simd,      41893361222,      61394520732,      22017513586,        513357636
           btree_8_32_32_128_linear_simd,      36502298952,      49136195752,      15909901010,        529045270
             btree_8_32_32_128_simd_simd,      36601530026,      47986105224,      15964483114,        503650534
           btree_8_32_48_128_binary_simd,      40531948114,      59807526260,      21038047536,        361668200
           btree_8_32_48_128_linear_simd,      35778736384,      47215167940,      14967222586,        376613036
             btree_8_32_48_128_simd_simd,      36140668934,      47302968214,      15008318148,        352800558
           btree_8_32_64_128_binary_simd,      40531659998,      57106742992,      19786420088,        268481986
           btree_8_32_64_128_linear_simd,      36138691222,      45610094206,      14357430410,        286729514
             btree_8_32_64_128_simd_simd,      36290095942,      47348100324,      14776114298,        286145836
           btree_8_32_96_128_binary_simd,      41355593966,      55332691196,      19188619610,        196321576
           btree_8_32_96_128_linear_simd,      37312876632,      45083514208,      13964307280,        195732730
             btree_8_32_96_128_simd_simd,      37197160930,      46948779708,      14786708324,        200716348
```

It looks like linear simd is fastest. A leaf node size of 64 or 96 seems to offer a reasonable compromise among inserts, finds, and erases.

### Exploring 8 byte keys, 256 byte values

Now, let's assume that for key size 8, internal node size stays at 128, and see what leaf size should be for 256 byte values.

** Varying leaf size for 256 byte values:**

```
./cmake-build-release/src/binary/btree_benchmark -i 5 -t 1000000 --batch-size 500000 --batches 10 btree_8_256_16_128_binary_simd btree_8_256_16_128_linear_simd btree_8_256_16_128_simd_simd btree_8_256_24_128_binary_simd btree_8_256_24_128_linear_simd btree_8_256_24_128_simd_simd btree_8_256_32_128_binary_simd btree_8_256_32_128_linear_simd btree_8_256_32_128_simd_simd btree_8_256_48_128_binary_simd btree_8_256_48_128_linear_simd btree_8_256_48_128_simd_simd btree_8_256_64_128_binary_simd btree_8_256_64_128_linear_simd btree_8_256_64_128_simd_simd btree_8_256_96_128_binary_simd btree_8_256_96_128_linear_simd btree_8_256_96_128_simd_simd
```

```
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
          btree_8_256_16_128_binary_simd,      69315775622,      71375260206,      25925146846,        987251234
          btree_8_256_16_128_linear_simd,      61890641014,      54264434894,      17978441324,       1028142796
            btree_8_256_16_128_simd_simd,      61638995182,      54366748000,      17584474824,       1020143344
          btree_8_256_24_128_binary_simd,      69926299512,      67696694296,      24342563296,        683272432
          btree_8_256_24_128_linear_simd,      64702394332,      52885719084,      17359743392,        709959440
            btree_8_256_24_128_simd_simd,      64568645166,      53307803992,      17238805596,        696242548
          btree_8_256_32_128_binary_simd,      73128465548,      65915424266,      23734277320,        524345416
          btree_8_256_32_128_linear_simd,      67761236122,      52867890198,      17196713902,        550213840
            btree_8_256_32_128_simd_simd,      68289584562,      52381273290,      16766296886,        533305402
          btree_8_256_48_128_binary_simd,      81202187896,      63922678316,      22692935452,        361361724
          btree_8_256_48_128_linear_simd,      76227018488,      51357549542,      16507435710,        384112722
            btree_8_256_48_128_simd_simd,      76594143586,      50775131242,      16164306724,        392682320
          btree_8_256_64_128_binary_simd,      90243622970,      62002151018,      21764639912,        289287572
          btree_8_256_64_128_linear_simd,      84746679872,      50914711918,      16531792324,        304404380
            btree_8_256_64_128_simd_simd,      85251011358,      51466656596,      16457028670,        302134744
          btree_8_256_96_128_binary_simd,     108504446228,      61600577596,      21278079206,        210341034
          btree_8_256_96_128_linear_simd,     102609575698,      49269195320,      15710331312,        219162878
            btree_8_256_96_128_simd_simd,     101629657108,      52100929928,      16451985110,        262961950
```

As before, linear simd is fastest.

There isn't an obvious winner here in terms of leaf node size, with a pretty strong trade-off between inserts and finds. The erase behavior is also puzzling as it keeps getting
better with larger leaf node sizes (possibly because there are fewer merges/rebalances). Between 16 and 32 entries per leaf node is probably a reasonable range, as above that
insert performance degrades pretty quickly.

### Exploring 16 byte keys, 32 byte values

```
./cmake-build-release/src/binary/btree_benchmark -i 5 -t 1000000 --batch-size 500000 --batches 10 btree_16_32_16_128_binary_simd btree_16_32_16_128_linear_simd btree_16_32_16_32_binary_simd btree_16_32_16_32_linear_simd btree_16_32_16_48_binary_simd btree_16_32_16_48_linear_simd btree_16_32_16_64_binary_simd btree_16_32_16_64_linear_simd btree_16_32_16_96_binary_simd btree_16_32_16_96_linear_simd
```

```
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
          btree_16_32_16_128_binary_simd,      51234015676,      75004143854,      27010595690,        974676368
          btree_16_32_16_128_linear_simd,      43516035804,      60644389306,      19977063324,        982950982
           btree_16_32_16_32_binary_simd,      53841695044,      79583279188,      29201792262,        979324100
           btree_16_32_16_32_linear_simd,      44072719742,      60671336584,      19798585492,        985163192
           btree_16_32_16_48_binary_simd,      53278883456,      78655609180,      28800360246,        981259516
           btree_16_32_16_48_linear_simd,      42766205720,      59786287582,      19503323168,        976098418
           btree_16_32_16_64_binary_simd,      52225029384,      76848753538,      27942191950,        975570160
           btree_16_32_16_64_linear_simd,      43018127122,      59800684916,      19361633812,        968265226
           btree_16_32_16_96_binary_simd,      51614513450,      76248700526,      27406498252,        979267966
           btree_16_32_16_96_linear_simd,      43169665802,      60668266656,      19512660486,        987122510
```

As elsewhere, linear simd is fastest.

Here, it looks like an internal leaf size of 48 or 64 is a good value for both find and insert. This is roughly half the
optimal for 8 byte keys, which might suggest that targeting a roughly constant internal node byte size is best. This would
seem to be 1024 bytes of keys.

### Exploring 32 byte keys, 32 byte values

``` 
./cmake-build-release/src/binary/btree_benchmark -i 5 -t 1000000 --batch-size 500000 --batches 10 btree_32_32_16_16_binary_simd btree_32_32_16_16_linear_simd btree_32_32_16_24_binary_simd btree_32_32_16_24_linear_simd btree_32_32_16_32_binary_simd btree_32_32_16_32_linear_simd btree_32_32_16_48_binary_simd btree_32_32_16_48_linear_simd btree_32_32_16_64_binary_simd btree_32_32_16_64_linear_simd
```

``` 
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
           btree_32_32_16_16_binary_simd,      61490910528,      95320002286,      36072264980,        973493032
           btree_32_32_16_16_linear_simd,      46307721778,      70288710340,      24865312242,        978087758
           btree_32_32_16_24_binary_simd,      58436896772,      90852273426,      34233546000,        976223912
           btree_32_32_16_24_linear_simd,      44412000200,      67048821780,      23204439782,        981024338
           btree_32_32_16_32_binary_simd,      57740811950,      89041619304,      33503664566,        977140484
           btree_32_32_16_32_linear_simd,      43206558930,      65336984918,      22129587616,        982429558
           btree_32_32_16_48_binary_simd,      56266388860,      86847859914,      32236408712,        981653372
           btree_32_32_16_48_linear_simd,      43007841646,      64760274776,      21904085336,        987215432
           btree_32_32_16_64_binary_simd,      55242294886,      85040931546,      31550998530,        979272046
           btree_32_32_16_64_linear_simd,      43024613506,      65771188378,      22447008318,        976274436
```

As always, linear simd is fastest.

Here, an internal node size of 48 is the winner for both find and insert. This seems to break with the hypothesis that a roughly
fixed key size is best.

### Pulling a bunch of this work together

And running against absl::btree and some other ordered and unordered maps:

```
./cmake-build-release/src/binary/btree_benchmark -i 50 -t 1000000 --batch-size 500000 --batches 10 absl_8_32 map_8_32 unordered_map_8_32 unordered_dense_8_32 btree_8_32_96_128_linear_simd absl_8_256 map_8_256 unordered_map_8_256 unordered_dense_8_256 btree_8_256_16_128_linear_simd btree_8_256_24_128_linear_simd btree_8_256_32_128_linear_simd absl_16_32 map_16_32 unordered_map_16_32 unordered_dense_16_32 btree_16_32_64_64_linear_simd absl_16_256 map_16_256 unordered_map_16_256 unordered_dense_16_256 btree_16_256_16_64_linear_simd btree_16_256_24_64_linear_simd btree_16_256_32_64_linear_simd absl_32_32 map_32_32 unordered_map_32_32 unordered_dense_32_32 btree_32_32_48_48_linear_simd absl_32_256 map_32_256 unordered_map_32_256 unordered_dense_32_256 btree_32_256_16_48_linear_simd btree_32_256_24_48_linear_simd btree_32_256_32_48_linear_simd
```

``` 
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
                               absl_8_32,     408495001988,     727519229634,     259250606560,      19989221180
                                map_8_32,     728733722554,    1290686406430,     556894022164,      98355093444
                      unordered_map_8_32,     388244231320,     493617292236,     138504507966,      69895815554
                    unordered_dense_8_32,     137818031748,     352863515664,      98522123204,        406677366
           btree_8_32_96_128_linear_simd,     347165374608,     410529554780,     127095167848,       2037090824
                              absl_8_256,     877027342492,     982090396984,     373652163340,      29674824980
                               map_8_256,     816290315460,    1388345089400,     592470467048,     102431464182
                     unordered_map_8_256,     437108565044,     542529106450,     152743909314,      91533053658
                   unordered_dense_8_256,     194367636562,     405642593204,     127231927034,        406912850
          btree_8_256_16_128_linear_simd,     514190564464,     496282287274,     161134449918,       9637734350
          btree_8_256_24_128_linear_simd,     531408468866,     483519859832,     157362474788,       6584799584
          btree_8_256_32_128_linear_simd,     554985756864,     479945250316,     154434270382,       5113730288
                              absl_16_32,     527691923528,     938719706046,     358894700592,      23970362950
                               map_16_32,     759891957844,    1339304436266,     575655836068,      99364134972
                     unordered_map_16_32,     399973719688,     529826762448,     146046483098,      75844015492
                   unordered_dense_16_32,     146045389658,     383275660496,     103015755440,        405679772
           btree_16_32_64_64_linear_simd,     379408445144,     492777255936,     160484660482,       2797865644
                             absl_16_256,    1085732215980,    1376846271346,     548585307048,      29565837674
                              map_16_256,     840195234054,    1421056698330,     605552252716,     102699992748
                    unordered_map_16_256,     454170063258,     623917179010,     194607134330,      92150364254
                  unordered_dense_16_256,     199583951514,     429722464918,     132304658626,        202926620
          btree_16_256_16_64_linear_simd,     579354318396,     593200749648,     192766789266,      10029685466
          btree_16_256_24_64_linear_simd,     580395692122,     561260051448,     184626206700,       7047527140
          btree_16_256_32_64_linear_simd,     600756028844,     552528357292,     182340157540,       5344418180
                              absl_32_32,     594481566232,    1056944325710,     412192640164,      29041894580
                               map_32_32,     776084874140,    1364151466266,     578054660746,     100214332478
                     unordered_map_32_32,     419780434308,     553677448322,     151492881864,      82524474382
                   unordered_dense_32_32,     143832627770,     410376623426,     107142989158,        205114928
           btree_32_32_48_48_linear_simd,     406834955388,     554052604390,     184958946136,       3464618428
                             absl_32_256,    1108402106674,    1371203324802,     543566546240,      29706282902
                              map_32_256,     860221015774,    1422084896274,     588917335944,     103354042350
                    unordered_map_32_256,     501063216280,     633724575400,     185631729398,      91756984798
                  unordered_dense_32_256,     213480128744,     464520718856,     144544623708,        408419628
          btree_32_256_16_48_linear_simd,     614517112650,     664011327586,     230982674482,      10520328450
          btree_32_256_24_48_linear_simd,     634333014716,     649929908258,     224352850126,       7305632496
          btree_32_256_32_48_linear_simd,     654998356054,     635379924186,     218358109560,       5499999290
```

As expected, we're handily beating absl::btree in all cases, and std::map even more so. Surprisingly, we are often
beating std::unordered_map. As one would expect, ankerl::unorderd_dense is faster than we are.

It also looks like 24 is probablyu leaf size for 256 byte values, offering a reasonable tradeoff between inserts, finds,
and erases.