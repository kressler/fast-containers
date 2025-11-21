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

add_executable(test_simd_encoding tests/test_simd_encoding.cpp)
target_link_libraries(test_simd_encoding PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_simd_encoding COMMAND test_simd_encoding)

# Benchmarks
add_executable(sample_benchmark benchmarks/sample_benchmark.cpp)
target_link_libraries(sample_benchmark PRIVATE benchmark::benchmark)

add_executable(ordered_array_search_benchmark
               benchmarks/ordered_array_search_benchmark.cpp)
target_link_libraries(ordered_array_search_benchmark
                      PRIVATE benchmark::benchmark)

add_executable(byte_array_simd_benchmark
               benchmarks/byte_array_simd_benchmark.cpp)
target_link_libraries(byte_array_simd_benchmark PRIVATE benchmark::benchmark)

add_subdirectory(binary)
