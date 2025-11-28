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

## Histograms

``` 
./cmake-build-release/src/binary/btree_benchmark -i 50 -t 1000000 --batch-size 500000 --batches 10 absl_16_32 map_16_32 unordered_map_16_32 unordered_dense_16_32 btree_16_32_64_64_linear_simd
```

``` 
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
                              absl_16_32,     580430599038,    1047469332326,     401498629684,      25553394586
                               map_16_32,     842650551194,    1475567598102,     631527735780,     105484441244
                     unordered_map_16_32,     424121052604,     558360402928,     156483759986,      81529077734
                   unordered_dense_16_32,     244777527326,     398591364960,     105210994476,        203537770
           btree_16_32_64_64_linear_simd,     410062075486,     542759112846,     177324720572,       2931973850


rdtsc calibration: 3.39989 cycles / ns
                 0.5,              0.9,             0.95,             0.99,            0.999,           0.9999,
absl_16_32
  i:      529.356049,       922.730137,      1068.603181,      1370.548086,      1801.220248,      4876.797057,
  f:      520.402166,       846.248821,       990.941759,      1174.877355,      1536.060057,      4713.192141,
  e:      432.611780,       708.167867,       865.502305,      1062.495727,      1417.509692,      4433.837488,
map_16_32
  i:      793.765024,      1270.091313,      1415.859923,      1673.899251,      2446.883396,      5622.395525,
  f:      748.909975,      1175.907651,      1334.774358,      1485.241133,      2201.416861,      5427.360425,
  e:      702.574938,      1113.808476,      1225.546434,      1461.108571,      2025.229169,      5304.898202,
unordered_map_16_32
  i:      126.707560,       560.141713,       731.088605,      1006.917899,      1327.652124,      2091.674411,
  f:      270.324903,       532.923536,       713.474472,       889.665943,      1068.250145,      1751.699862,
  e:      167.548937,       296.866667,       495.120661,       738.910460,       897.669731,      1173.176743,
unordered_dense_16_32
  i:      106.644314,       130.535687,       149.551638,       574.173862,       734.289638,       795.445730,
  f:      197.056576,       289.389321,       564.872648,       746.125035,       889.667607,      1134.810264,
  e:      103.987907,       183.175955,       222.854173,       628.339552,       741.788169,       785.575831,
btree_16_32_64_64_linear_simd
  i:      311.577467,       444.159601,       670.579769,       880.476127,      1191.133591,      4650.818015,
  f:      272.253916,       392.635164,       629.622907,       836.211390,       966.066644,      1496.684708,
  e:      198.887804,       267.491506,       407.799539,       707.659354,       867.099242,      1183.922975,
```



### 16 byte keys, 256 byte values, core isolation
``` 
taskset -c 15 ./cmake-build-release/src/binary/btree_benchmark -i 25 -t 1000000 --batch-size 500000 --batches 10 absl_16_256 map_16_256 unordered_map_16_256 unordered_dense_16_256 btree_16_256_24_64_linear_simd btree_16_256_24_64_linear_std btree_16_256_16_64_linear_simd btree_16_256_16_64_linear_std btree_16_256_8_64_linear_simd btree_16_256_8_64_linear_std btree_16_256_4_64_linear_simd btree_16_256_4_64_linear_std
```

``` 
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
                             absl_16_256,     660065281772,     843715592660,     338494750912,      17553151848
                              map_16_256,     517001831310,     868873061938,     368580639768,      60329511498
                    unordered_map_16_256,     268001687040,     362740742682,     111072166790,      53049447920
                  unordered_dense_16_256,     146326404044,     252280993718,      79227266242,        150004226
          btree_16_256_24_64_linear_simd,     360746321178,     359361919378,     122843453772,       4109199268
           btree_16_256_24_64_linear_std,     359997269728,     360134001442,     122291720438,       4100219834
          btree_16_256_16_64_linear_simd,     352700751578,     372934329194,     125766542062,       5766563710
           btree_16_256_16_64_linear_std,     351187982404,     374026564166,     128659509582,       5790704832
           btree_16_256_8_64_linear_simd,     364511857728,     420853977406,     146934196952,      11248521342
            btree_16_256_8_64_linear_std,     364318785974,     406254649440,     140782901822,      11313726270
           btree_16_256_4_64_linear_simd,     401627997100,     450938375124,     161536144788,      21728175762
            btree_16_256_4_64_linear_std,     402368301064,     450240677036,     159674829884,      21748305020


rdtsc calibration: 3.39989 cycles / ns
                 0.5,              0.9,             0.95,             0.99,            0.999,           0.9999,
absl_16_256
  i:     1102.206819,      1703.226684,      1908.449438,      3390.043220,      5036.505016,      7895.454448,
  f:      872.441840,      1248.467094,      1382.075704,      1571.230131,      1756.826589,      2530.849826,
  e:      765.128709,      1088.927778,      1247.190546,      1449.147859,      1637.796328,      2265.674949,
map_16_256
  i:      966.976771,      1438.280647,      1574.045379,      1856.482086,      3405.996396,      6466.827659,
  f:      901.311283,      1309.786507,      1425.583596,      1603.011628,      1760.891086,      2708.605895,
  e:      845.383865,      1214.755608,      1352.225730,      1519.987953,      1673.026296,      2554.440974,
unordered_map_16_256
  i:      168.130418,       671.847373,       864.976147,      1153.154803,      1882.783775,      3029.539766,
  f:      368.058019,       671.702435,       825.274928,       971.936381,      1148.404645,      1352.454650,
  e:      233.778174,       427.629500,       644.468184,       831.130469,       967.675663,      1139.965080,
unordered_dense_16_256
  i:      140.664420,       177.326700,       236.852184,       623.436312,       726.152736,    101794.225989,
  f:      239.922952,       380.622278,       643.173265,       791.860605,       861.392680,       945.489143,
  e:      187.080329,       224.521608,       454.136223,       695.330341,       752.914630,       838.573650,
btree_16_256_24_64_linear_simd
  i:      510.627181,       870.815182,      1052.146755,      3132.766840,      6287.578870,      9498.744326,
  f:      365.084437,       509.849306,       724.875252,       893.427873,      1006.609035,      1293.017150,
  e:      270.334736,       360.387339,       487.313260,       761.712923,       889.634485,      1027.903487,
btree_16_256_24_64_linear_std
  i:      509.140131,       866.772537,      1053.193955,      3139.779909,      6280.532327,      9391.599249,
  f:      365.928182,       512.503888,       726.098799,       894.882329,      1007.867736,      1244.693879,
  e:      269.662785,       355.940127,       486.514222,       759.312782,       882.729119,      1014.674887,
btree_16_256_16_64_linear_simd
  i:      469.209037,       870.187693,      1031.931043,      2780.480354,      5738.346886,      8579.781671,
  f:      374.687873,       539.439560,       742.520993,       907.573862,      1030.988721,      1249.721471,
  e:      274.992530,       366.496540,       500.064924,       769.337072,       894.829737,      1012.045571,
btree_16_256_16_64_linear_std
  i:      466.119553,       862.519279,      1029.875459,      2789.402045,      5736.591848,      8606.194754,
  f:      375.916178,       540.651109,       743.934653,       909.401222,      1033.473513,      1289.684991,
  e:      280.622180,       376.279424,       515.088408,       780.391269,       904.103101,      1038.252302,
btree_16_256_8_64_linear_simd
  i:      453.925709,       867.677268,      1088.514613,      2737.933896,      4999.905954,      7701.039428,
  f:      410.921569,       643.252712,       819.149927,       991.417425,      1129.411565,      1350.371602,
  e:      319.669700,       455.277833,       618.048921,       855.889947,       996.833669,      1133.432327,
btree_16_256_8_64_linear_std
  i:      453.026377,       867.493607,      1092.561519,      2737.043019,      4951.971662,      7643.062604,
  f:      395.250261,       621.262402,       799.356668,       969.776361,      1101.966492,      1311.654038,
  e:      296.390540,       434.436952,       602.065040,       839.596720,       970.810082,      1091.586937,
btree_16_256_4_64_linear_simd
  i:      533.810711,       980.154480,      1212.689055,      2857.169530,      5939.775577,      7972.915631,
  f:      438.489670,       711.810159,       870.624898,      1041.046750,      1172.975576,      1383.489791,
  e:      348.137710,       534.968481,       708.957305,       918.263582,      1058.354124,      1189.401463,
btree_16_256_4_64_linear_std
  i:      531.015336,       984.527860,      1223.608068,      2854.304217,      5923.119270,      7939.482059,
  f:      437.017693,       712.739436,       870.779723,      1042.394019,      1177.996920,      1396.430790,
  e:      344.516137,       526.201354,       703.008790,       909.632171,      1049.424405,      1182.589765,
```


### 8 byte keys, 32 byte values, large internal nodes

``` 
taskset -c 15 ./cmake-build-release/src/binary/btree_benchmark -i 25 -t 1000000 --batch-size 500000 --batches 10 absl_8_32 map_8_32 unordered_map_8_32 unordered_dense_8_32 btree_8_32_96_128_binary_simd btree_8_32_96_128_linear_simd btree_8_32_96_128_linear_std btree_8_32_96_256_binary_simd btree_8_32_96_256_linear_simd btree_8_32_96_512_binary_simd btree_8_32_96_512_linear_simd btree_8_32_96_1024_binary_simd btree_8_32_96_1024_linear_simd
```

``` 
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
                               absl_8_32,     269275597476,     477937566602,     173992192416,      12056766010
                                map_8_32,     473004197338,     818154877500,     354206769848,      59037380972
                      unordered_map_8_32,     236193778594,     292391539236,      82332691194,      41918922558
                    unordered_dense_8_32,     132966085650,     198343668166,      55765060186,        300003556
           btree_8_32_96_128_binary_simd,     260246672362,     341088268328,     121562768394,       1166869392
           btree_8_32_96_128_linear_simd,     228848499990,     279825917666,      91023156134,       1163961202
            btree_8_32_96_128_linear_std,     223380336056,     277691112644,      91132137354,       1320877220
           btree_8_32_96_256_binary_simd,     256224592892,     336824380654,     117962695088,       1023960830
           btree_8_32_96_256_linear_simd,     234928913484,     287610615942,      95492411084,       1088246126
           btree_8_32_96_512_binary_simd,     254028954084,     333015862732,     115616563950,        949224002
           btree_8_32_96_512_linear_simd,     237959090418,     291648400522,      97716434996,       1088641750
          btree_8_32_96_1024_binary_simd,     254386151182,     329638181012,     114533974914,       1023915576
          btree_8_32_96_1024_linear_simd,     251081952438,     320874744796,     112494670996,       1081342460


rdtsc calibration: 3.39989 cycles / ns
                 0.5,              0.9,             0.95,             0.99,            0.999,           0.9999,
absl_8_32
  i:      468.204832,       853.139998,      1006.410663,      1278.362358,      1647.303422,      3717.033393,
  f:      490.233271,       743.989896,       901.313389,      1046.886067,      1160.537862,      1652.804009,
  e:      388.852547,       547.903627,       747.262165,       923.744538,      1039.587650,      1397.389366,
map_8_32
  i:      900.254988,      1364.555416,      1496.794752,      1710.031855,      2103.081569,      3742.834584,
  f:      850.387409,      1256.291298,      1375.230656,      1550.753340,      1715.530976,      2773.071708,
  e:      809.689474,      1184.651765,      1319.676093,      1485.167354,      1651.165891,      2669.646939,
unordered_map_8_32
  i:      147.595013,       557.036639,       737.085534,       988.920123,      1179.566209,      1394.143204,
  f:      287.990261,       543.318448,       726.332702,       911.378414,      1071.380218,      1269.424708,
  e:      166.641405,       328.866847,       498.535729,       747.788407,       895.630833,      1051.783122,
unordered_dense_8_32
  i:      127.671437,       163.386336,       183.793691,       587.662861,       700.137689,      1811.358472,
  f:      201.099608,       293.280925,       544.358647,       747.676989,       827.934639,       871.321446,
  e:      112.118789,       202.101442,       226.747995,       626.575551,       733.268637,       765.398015,
btree_8_32_96_128_binary_simd
  i:      419.692843,       645.989973,       818.153727,      1061.311324,      1423.132515,      1947.559103,
  f:      336.518342,       531.582160,       663.771249,       930.572372,      1103.576718,      1350.491168,
  e:      254.969827,       409.720385,       474.097425,       806.635044,      1001.000404,      1172.536099,
btree_8_32_96_128_linear_simd
  i:      370.148419,       508.045805,       737.608625,       932.474862,      1273.251751,      1803.269502,
  f:      276.452844,       387.183567,       590.876953,       818.026886,       903.540238,      1105.117309,
  e:      207.099991,       269.367237,       326.060288,       698.255038,       790.511860,       904.751368,
btree_8_32_96_128_linear_std
  i:      363.966705,       494.342716,       721.964102,       919.557154,      1274.267875,      1805.308147,
  f:      273.945230,       383.605840,       587.798989,       815.330947,       902.387681,      1125.023709,
  e:      206.861436,       269.791003,       326.741854,       698.181253,       792.253026,       902.230785,
btree_8_32_96_256_binary_simd
  i:      411.763884,       632.799038,       806.490093,      1050.614266,      1415.404350,      2074.908850,
  f:      331.457247,       528.580504,       659.200280,       926.425880,      1098.242059,      1321.472711,
  e:      247.415099,       404.685880,       465.367173,       793.465135,       994.320050,      1155.625315,
btree_8_32_96_256_linear_simd
  i:      381.113867,       529.005089,       753.101790,       958.905562,      1315.536517,      1970.903021,
  f:      285.285496,       404.668852,       599.896591,       830.468301,       927.082632,      1134.164986,
  e:      214.586998,       291.447132,       352.082105,       710.789523,       823.725415,       936.101248,
btree_8_32_96_512_binary_simd
  i:      407.304174,       627.043684,       803.335022,      1049.128431,      1405.222496,      1926.788496,
  f:      326.438358,       523.795127,       654.150583,       921.071798,      1089.818737,      1309.886368,
  e:      240.550097,       397.845844,       458.558892,       787.966541,       987.220927,      1154.422649,
btree_8_32_96_512_linear_simd
  i:      384.048870,       535.380121,       761.175753,       981.530117,      1384.247823,      1802.270025,
  f:      289.515230,       411.278230,       599.668424,       834.823215,       932.397416,      1154.013935,
  e:      217.655226,       298.537661,       356.702689,       714.231790,       826.255702,       937.871334,
btree_8_32_96_1024_binary_simd
  i:      406.697195,       624.484589,       812.638220,      1067.233044,      1486.176593,      1880.242988,
  f:      322.898466,       518.418287,       647.604653,       914.465963,      1083.837875,      1260.374206,
  e:      237.745797,       391.154005,       451.293731,       780.486232,       978.520063,      1109.165776,
btree_8_32_96_1024_linear_simd
  i:      402.599425,       568.436488,       788.578498,      1043.160554,      1503.823347,      1884.536723,
  f:      329.262456,       458.941872,       639.515204,       864.178711,       984.310375,      1229.943258,
  e:      248.838107,       362.232041,       422.496589,       743.836002,       883.076882,      1019.513975,
```


## Experiments with huge page pool allocator

```
taskset -c 15 ./cmake-build-release/src/binary/btree_benchmark -i 25 -t 1000000 --batch-size 500000 --batches 10 absl_16_256 map_16_256 unordered_map_16_256 unordered_dense_16_256 btree_16_256_16_64_linear_std btree_16_256_16_64_linear_std_hp btree_16_256_8_64_linear_std btree_16_256_8_64_linear_std_hp
```

```
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
                             absl_16_256,     673446495920,     872539365646,     350595639198,      17668184218
                              map_16_256,     541877193714,     925459742494,     392787391152,      62097484774
                    unordered_map_16_256,     272418289386,     371678963072,     115052415822,      54329906008
                  unordered_dense_16_256,     121551086946,     254962512586,      76446824102,        300003862
           btree_16_256_16_64_linear_std,     364908779134,     390923093020,     133219275082,       5908103772
        btree_16_256_16_64_linear_std_hp,     241615703208,     333541614694,     109705428534,       5041515376
            btree_16_256_8_64_linear_std,     380293095292,     427369810098,     147693814616,      11478576120
         btree_16_256_8_64_linear_std_hp,     232806526248,     350362747698,     116307237060,       9805296414


rdtsc calibration: 3.39989 cycles / ns
                 0.5,              0.9,             0.95,             0.99,            0.999,           0.9999,
absl_16_256
  i:     1134.248897,      1731.472509,      1941.135322,      3289.943456,      4827.853025,      7926.799130,
  f:      899.287321,      1288.879022,      1418.425054,      1619.582466,      1868.903508,      2963.066928,
  e:      789.602886,      1136.674160,      1289.961820,      1497.465004,      1720.831475,      2680.182509,
map_16_256
  i:     1021.203223,      1516.143632,      1652.812124,      1955.526202,      3219.710792,      5784.368019,
  f:      953.785907,      1387.995115,      1515.651291,      1735.338444,      2517.382057,      3499.661377,
  e:      888.538728,      1308.868560,      1429.325550,      1656.324341,      2362.801910,      3215.913263,
unordered_map_16_256
  i:      170.829381,       683.657114,       878.788278,      1159.853355,      1480.409806,      1847.747303,
  f:      380.148327,       686.264834,       837.566670,       988.522141,      1180.435275,      1503.423737,
  e:      240.605893,       443.790037,       658.775944,       842.670454,       998.014899,      1239.535978,
unordered_dense_16_256
  i:      146.979619,       179.151309,       242.034284,       623.645984,       712.936253,    102074.827284,
  f:      241.067763,       380.879804,       646.138338,       793.616259,       860.902924,      1035.313861,
  e:      147.505570,       223.785497,       425.305237,       687.739512,       752.085268,       825.764596,
btree_16_256_16_64_linear_std
  i:      495.540602,       893.673429,      1056.829904,      2748.197447,      5147.629963,      8659.641503,
  f:      385.295108,       599.366824,       779.464320,       934.817698,      1087.760549,      1501.530861,
  e:      284.370648,       402.870523,       582.411395,       805.944684,       937.148154,      1263.872074,
btree_16_256_16_64_linear_std_hp
  i:      408.724816,       685.330805,       803.475313,      1038.297787,      1384.100509,      1736.118808,
  f:      336.668550,       451.632242,       695.510252,       856.041817,       939.521584,      1325.375797,
  e:      240.393082,       286.336832,       447.213074,       727.748490,       807.891061,      1015.517484,
btree_16_256_8_64_linear_std
  i:      478.341723,       909.578512,      1139.629137,      2711.757260,      4674.786291,      7843.223911,
  f:      409.222052,       685.861333,       843.846903,      1011.405665,      1239.674425,      1615.382514,
  e:      307.443399,       479.409030,       663.818782,       871.821831,      1062.194798,      1398.318802,
btree_16_256_8_64_linear_std_hp
  i:      386.889989,       643.880740,       800.604945,      1025.694393,      1282.383526,      1568.970453,
  f:      347.071518,       506.618406,       725.370431,       878.703869,       986.564750,      1313.806417,
  e:      248.263717,       339.435076,       505.106941,       743.205492,       861.178409,      1035.892280,
```


```
taskset -c 15 ./cmake-build-release/src/binary/btree_benchmark -i 25 -t 1000000 --batch-size 500000 --batches 10 absl_8_32 map_8_32 unordered_map_8_32 unordered_dense_8_32 btree_8_32_96_128_linear_std btree_8_32_96_128_linear_std_hp
```

```
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
                               absl_8_32,     236702958480,     416368465628,     152290877130,      11085266024
                                map_8_32,     414181652030,     721104777734,     311597684180,      54226313346
                      unordered_map_8_32,     211698374758,     266467479574,      75423807860,      37742522732
                    unordered_dense_8_32,     125109925018,     189666022688,      53553231598,        150002900
            btree_8_32_96_128_linear_std,     204808690918,     249871743518,      82527857620,       1075199306
         btree_8_32_96_128_linear_std_hp,     168310808362,     257924761158,      80282782778,        963176446


rdtsc calibration: 3.39989 cycles / ns
                 0.5,              0.9,             0.95,             0.99,            0.999,           0.9999,
absl_8_32
  i:      409.502064,       762.831407,       917.232535,      1176.402981,      1515.391982,      3454.936863,
  f:      413.825652,       675.874364,       838.858670,       985.217010,      1089.512906,      1395.532917,
  e:      337.747885,       494.112966,       693.392347,       882.677475,       994.686184,      1198.239054,
map_8_32
  i:      775.155518,      1233.550628,      1365.220745,      1579.389056,      2016.455897,      3709.081937,
  f:      737.834164,      1155.152159,      1274.291684,      1438.747923,      1617.716640,      2228.707822,
  e:      704.822170,      1090.065169,      1210.206132,      1383.517609,      1554.323759,      2160.991445,
unordered_map_8_32
  i:      139.564834,       517.687283,       697.968218,       942.322210,      1125.555968,      1316.830285,
  f:      261.784030,       506.399138,       695.584510,       875.640878,      1022.070396,      1177.601960,
  e:      139.740213,       294.303922,       459.366100,       728.942948,       863.633967,      1006.838732,
unordered_dense_8_32
  i:      121.034618,       149.910556,       177.677139,       582.449599,       692.379205,       768.609767,
  f:      192.802136,       282.315033,       533.980177,       739.964254,       819.608488,       862.643391,
  e:      110.237978,       196.451543,       219.015693,       621.022856,       728.892666,       752.497056,
btree_8_32_96_128_linear_std
  i:      340.427106,       441.139712,       666.987176,       885.703897,      1212.654421,      1730.534565,
  f:      246.483125,       351.330286,       542.012974,       776.576090,       868.807634,       986.741247,
  e:      190.558399,       248.211378,       284.741499,       672.946215,       777.446002,       859.354911,
btree_8_32_96_128_linear_std_hp
  i:      328.450549,       404.695433,       593.830343,       848.849883,      1004.097545,      1325.953866,
  f:      265.590406,       337.622258,       572.439843,       780.721123,       860.056072,       971.096181,
  e:      188.839281,       235.078902,       264.518819,       661.404747,       761.876612,       847.874529,
```


```
taskset -c 15 ./cmake-build-release/src/binary/btree_benchmark -i 25 -t 1000000 --batch-size 500000 --batches 10 absl_32_32 map_32_32 unordered_map_32_32 unordered_dense_32_32 btree_32_32_48_48_linear_std btree_32_32_48_48_linear_std_hp
```

```
                          Benchmark Name,    Insert cycles,      Find cycles,     Erase cycles,   Iterate cycles
                              absl_32_32,     393796868866,     695266352784,     273473170836,      17584756480
                               map_32_32,     499641930192,     867954232814,     368763047252,      60537964002
                     unordered_map_32_32,     253270679148,     330140307190,      91150952138,      49023931288
                   unordered_dense_32_32,     142187638240,     240546437854,      61682197418,        150003240
            btree_32_32_48_48_linear_std,     260350548856,     370866744552,     128630382292,       2157614670
         btree_32_32_48_48_linear_std_hp,     189561226868,     336908084268,     109178062184,       1903075064


rdtsc calibration: 3.39989 cycles / ns
                 0.5,              0.9,             0.95,             0.99,            0.999,           0.9999,
absl_32_32
  i:      727.565088,      1156.185178,      1303.983725,      1558.888419,      2311.983140,      3908.632457,
  f:      704.608654,      1053.806111,      1183.520098,      1345.085893,      1501.646144,      2219.453955,
  e:      603.661302,       907.281746,      1060.292421,      1240.707654,      1404.775317,      1939.902016,
map_32_32
  i:      953.215939,      1419.520146,      1549.025117,      1755.953209,      2041.966947,      3542.743192,
  f:      900.161629,      1309.268367,      1421.257335,      1588.126391,      1742.464276,      2750.303696,
  e:      840.889262,      1217.191451,      1349.193071,      1512.053658,      1673.515825,      2575.203568,
unordered_map_32_32
  i:      165.059118,       660.533628,       846.053127,      1128.421083,      1429.979202,      2105.323280,
  f:      327.732615,       612.352114,       775.005531,       957.312126,      1141.818700,      1351.878906,
  e:      209.310985,       352.870851,       541.762944,       775.747078,       935.806824,      1103.615288,
unordered_dense_32_32
  i:      127.778721,       150.362645,       181.969531,       588.670761,       696.384863,       779.564964,
  f:      232.345497,       338.699124,       618.422377,       785.726566,       859.337316,       931.041620,
  e:      128.244593,       214.001972,       244.244943,       643.112985,       742.163893,       784.068317,
btree_32_32_48_48_linear_std
  i:      396.874885,       578.946368,       775.817465,       977.888584,      1352.988679,      2378.115384,
  f:      373.944703,       533.808436,       744.139569,       896.735747,       995.634491,      1333.259204,
  e:      276.659958,       381.347043,       561.765887,       775.716576,       901.198602,      1075.813643,
btree_32_32_48_48_linear_std_hp
  i:      356.252729,       445.026293,       647.691488,       881.119229,      1091.283328,      1380.890723,
  f:      337.918135,       457.108224,       699.608664,       858.392091,       940.259789,      1192.113979,
  e:      237.072606,       299.287999,       455.376529,       727.765434,       822.128635,       910.382719,
```



### Orderbook type benchmarks

```
taskset -c 15 ./cmake-build-release/src/binary/orderbook_sim -e 10000000 absl_0 map_0 btree_0_64_64_linear_std btree_0_64_64_linear_std_hp btree_0_128_128_linear_std btree_0_128_128_linear_std_hp btree_0_256_256_linear_std btree_0_256_256_linear_std_hp absl_256 map_256 btree_256_8_64_linear_std btree_256_8_64_linear_std_hp btree_256_8_128_linear_std btree_256_8_128_linear_std_hp btree_256_8_256_linear_std btree_256_8_256_linear_std_hp -i 100
```

```
                 0.5,              0.9,             0.95,             0.99,            0.999,           0.9999,
absl_0
  a:      242.473464,       326.756399,       358.433555,       444.437882,       650.208955,      1165.814045,
  m:      233.425596,       282.732285,       303.975380,       367.840060,       737.867040,       989.798025,
  c:      262.007509,       349.149431,       382.926064,       480.069536,       846.908762,      1110.691800,
map_0
  a:      312.010369,       434.167169,       465.401408,       530.590952,       669.222489,      1408.099643,
  m:      248.576374,       369.897803,       409.259736,       550.127403,       916.164762,      1145.354696,
  c:      297.935089,       440.517499,       501.359840,       746.145269,      1028.770990,      1303.842980,
btree_0_64_64_linear_std
  a:      206.099665,       244.009074,       259.842570,       726.129683,      1059.888423,      1377.619730,
  m:      182.747830,       225.095502,       243.078391,       294.233549,       696.926827,       851.351878,
  c:      212.034178,       288.929470,       420.185214,       802.203989,      1192.425510,      1440.326770,
btree_0_64_64_linear_std_hp
  a:      205.173440,       241.390635,       254.106637,       464.842062,       601.855912,      1023.236264,
  m:      176.035363,       210.097807,       222.136685,       251.786952,       491.151112,       785.107833,
  c:      202.468130,       259.903360,       380.099771,       618.276556,       797.539248,      1098.737193,
btree_0_128_128_linear_std
  a:      223.191327,       263.962770,       276.875780,       319.209383,      1127.296530,      1542.190184,
  m:      183.457710,       224.473212,       237.688955,       266.750179,       454.773415,       798.210857,
  c:      221.527991,       271.276493,       298.650650,       636.197332,      1055.923006,      1438.748951,
btree_0_128_128_linear_std_hp
  a:      217.072358,       253.050739,       263.970529,       294.288494,       708.830227,      1106.253727,
  m:      176.935981,       215.240189,       226.967758,       251.139385,       294.123795,       751.254296,
  c:      212.920594,       256.087676,       276.951383,       581.247593,       852.629014,      1131.610213,
btree_0_256_256_linear_std
  a:      282.345908,       354.810526,       370.523570,       398.330211,       467.779253,      1617.981913,
  m:      211.551270,       271.844347,       289.594163,       333.807451,       384.110511,       846.713350,
  c:      273.630295,       338.702476,       357.136311,       468.152022,       863.329711,      1336.749228,
btree_0_256_256_linear_std_hp
  a:      277.255085,       349.107484,       363.172226,       390.248498,       441.121028,      1250.440230,
  m:      212.291633,       270.784415,       287.649191,       329.924827,       370.559139,       802.087589,
  c:      271.870060,       335.323708,       352.439300,       438.065650,       835.620967,      1228.795931,
absl_256
  a:      401.452635,       630.639823,       755.384833,      1074.744699,      2740.827485,      4832.630758,
  m:      418.417857,       626.852350,       759.840951,      1029.834930,      1204.193231,      1510.820543,
  c:      530.100454,       902.356870,      1060.936523,      1351.491051,      1710.649958,      2069.413193,
map_256
  a:      404.251641,       544.299624,       586.299934,       725.967816,      1068.790942,      1707.832036,
  m:      382.323916,       571.810865,       689.463999,       975.551925,      1157.164713,      1464.668837,
  c:      501.425044,       782.413660,       937.936692,      1160.454059,      1356.139754,      1724.974131,
btree_256_8_64_linear_std
  a:      250.729728,       560.562896,       628.362812,      1061.758436,      3103.025125,      6240.013906,
  m:      334.409323,       401.718378,       590.122305,       856.788018,       928.039979,      1038.605435,
  c:      399.391221,      1063.688659,      1253.667565,      1506.320140,      1715.510697,      2065.049422,
btree_256_8_64_linear_std_hp
  a:      257.529021,       407.859860,       456.996180,       786.936799,       940.608405,      1186.339509,
  m:      299.443162,       362.994949,       504.180405,       825.477517,       894.165309,       968.894394,
  c:      370.541465,       766.360234,       894.174644,      1162.405506,      1373.162769,      1564.912037,
btree_256_8_128_linear_std
  a:      280.607797,       607.005392,       677.618042,      1093.438725,      3158.100322,      6291.986098,
  m:      338.839893,       415.873379,       591.866340,       863.316332,       938.215253,      1066.409207,
  c:      416.132245,      1186.889599,      1364.776962,      1642.899316,      1850.374992,      2225.696760,
btree_256_8_128_linear_std_hp
  a:      283.051377,       462.640338,       530.036011,       833.082597,      1009.606008,      1287.590230,
  m:      297.836809,       373.457608,       501.311990,       822.872488,       899.345720,       976.248231,
  c:      384.250352,       841.819613,       968.789676,      1242.353900,      1452.638734,      1681.454039,
btree_256_8_256_linear_std
  a:      353.319492,       718.641449,       791.396467,      1215.379273,      3336.007768,      6530.075397,
  m:      370.497320,       469.557148,       635.738298,       900.402994,      1004.555721,      1172.241047,
  c:      472.653227,      1390.887710,      1571.531450,      1855.776416,      2075.606053,      2495.817303,
btree_256_8_256_linear_std_hp
  a:      351.678878,       569.998753,       640.912127,       903.884954,      1118.462792,      1453.426861,
  m:      337.657160,       424.148371,       532.129413,       857.304926,       951.961728,      1046.745827,
  c:      433.049766,      1025.392597,      1159.298050,      1428.611691,      1669.503279,      1934.624660,
```
