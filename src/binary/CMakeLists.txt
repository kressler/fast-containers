add_executable(btree_stress btree_stress.cpp)
target_link_libraries(btree_stress PRIVATE absl::base benchmark::benchmark lyra)

add_executable(btree_benchmark btree_benchmark.cpp)
target_link_libraries(btree_benchmark PRIVATE absl::base benchmark::benchmark lyra unordered_dense)

add_executable(lyra_example lyra_example.cpp)
target_link_libraries(lyra_example PRIVATE lyra)