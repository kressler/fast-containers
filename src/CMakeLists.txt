# Tests
add_executable(sample_test tests/sample_test.cpp)
target_link_libraries(sample_test PRIVATE Catch2::Catch2WithMain)
add_test(NAME sample_test COMMAND sample_test)

add_executable(test_ordered_array tests/test_ordered_array.cpp)
target_link_libraries(test_ordered_array PRIVATE Catch2::Catch2WithMain)
add_test(NAME test_ordered_array COMMAND test_ordered_array)

# Benchmarks
add_executable(sample_benchmark benchmarks/sample_benchmark.cpp)
target_link_libraries(sample_benchmark PRIVATE benchmark::benchmark)
