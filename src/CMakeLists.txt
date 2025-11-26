# Tests
add_executable(sample_test tests/sample_test.cpp)
target_link_libraries(sample_test PRIVATE Catch2::Catch2WithMain)
add_test(NAME sample_test COMMAND sample_test)

add_executable(test_ordered_array tests/test_ordered_array.cpp)
target_link_libraries(test_ordered_array PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_ordered_array COMMAND test_ordered_array)

add_executable(test_btree tests/test_btree.cpp)
target_link_libraries(test_btree PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_btree COMMAND test_btree)

add_executable(test_hugepage_allocator tests/test_hugepage_allocator.cpp)
target_link_libraries(test_hugepage_allocator PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_hugepage_allocator COMMAND test_hugepage_allocator)

add_executable(test_policy_based_allocator tests/test_policy_based_allocator.cpp)
target_link_libraries(test_policy_based_allocator PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_policy_based_allocator COMMAND test_policy_based_allocator)

# Benchmarks
add_executable(sample_benchmark benchmarks/sample_benchmark.cpp)
target_link_libraries(sample_benchmark PRIVATE benchmark::benchmark)

add_executable(ordered_array_search_benchmark
               benchmarks/ordered_array_search_benchmark.cpp)
target_link_libraries(ordered_array_search_benchmark
                      PRIVATE benchmark::benchmark)

add_executable(hugepage_allocator_benchmark
               benchmarks/hugepage_allocator_benchmark.cpp)
target_link_libraries(hugepage_allocator_benchmark
                      PRIVATE benchmark::benchmark)

add_subdirectory(binary)
