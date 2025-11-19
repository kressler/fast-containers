add_executable(btree_insert_stress btree_insert_stress.cpp)
target_link_libraries(btree_insert_stress PRIVATE absl::base benchmark::benchmark)

add_executable(lyra_example lyra_example.cpp)
target_link_libraries(lyra_example PRIVATE lyra)