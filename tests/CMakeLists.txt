# Test executables
add_executable(sample_test sample_test.cpp)
target_link_libraries(sample_test PRIVATE
  Catch2::Catch2WithMain
  fast_containers::fast_containers
)
add_test(NAME sample_test COMMAND sample_test)

add_executable(test_ordered_array test_ordered_array.cpp)
target_link_libraries(test_ordered_array PRIVATE
  Catch2::Catch2WithMain
  fast_containers::fast_containers
)
add_test(NAME test_ordered_array COMMAND test_ordered_array)

add_executable(test_btree test_btree.cpp)
target_link_libraries(test_btree PRIVATE
  Catch2::Catch2WithMain
  fast_containers::fast_containers
)
add_test(NAME test_btree COMMAND test_btree)

add_executable(test_hugepage_allocator test_hugepage_allocator.cpp)
target_link_libraries(test_hugepage_allocator PRIVATE
  Catch2::Catch2WithMain
  fast_containers::fast_containers
)
add_test(NAME test_hugepage_allocator COMMAND test_hugepage_allocator)

add_executable(test_policy_based_allocator test_policy_based_allocator.cpp)
target_link_libraries(test_policy_based_allocator PRIVATE
  Catch2::Catch2WithMain
  fast_containers::fast_containers
)
add_test(NAME test_policy_based_allocator COMMAND test_policy_based_allocator)
