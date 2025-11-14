# Tests
add_executable(sample_test tests/sample_test.cpp)
target_link_libraries(sample_test PRIVATE Catch2::Catch2WithMain)
add_test(NAME sample_test COMMAND sample_test)
