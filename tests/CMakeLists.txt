# Option to enable AddressSanitizer (ASAN) for memory error detection
# Useful for debugging memory issues like buffer overflows, use-after-free, etc.
option(ENABLE_ASAN "Enable AddressSanitizer for memory error detection" OFF)

if(ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Building tests with AddressSanitizer (ASAN) enabled")
        add_compile_options(-fsanitize=address)
        add_compile_options(-fno-omit-frame-pointer)
        add_compile_options(-g)
        add_link_options(-fsanitize=address)

        # Disable optimizations that interfere with ASAN
        add_compile_options(-O1)
        add_compile_options(-fno-optimize-sibling-calls)
    elseif(MSVC)
        message(STATUS "Building tests with AddressSanitizer (ASAN) enabled (MSVC)")
        add_compile_options(/fsanitize=address)
        add_compile_options(/Zi)
    else()
        message(WARNING "AddressSanitizer not supported for this compiler")
    endif()
endif()

# Test executables
add_executable(sample_test sample_test.cpp)
target_link_libraries(sample_test PRIVATE
  Catch2::Catch2WithMain
  kressler::fast_containers
)
add_test(NAME sample_test COMMAND sample_test)

add_executable(test_ordered_array test_ordered_array.cpp)
target_link_libraries(test_ordered_array PRIVATE
  Catch2::Catch2WithMain
  kressler::fast_containers
)
add_test(NAME test_ordered_array COMMAND test_ordered_array)

add_executable(test_btree test_btree.cpp)
target_link_libraries(test_btree PRIVATE
  Catch2::Catch2WithMain
  kressler::fast_containers
)
add_test(NAME test_btree COMMAND test_btree)

add_executable(test_hugepage_allocator test_hugepage_allocator.cpp)
target_link_libraries(test_hugepage_allocator PRIVATE
  Catch2::Catch2WithMain
  kressler::fast_containers
)
add_test(NAME test_hugepage_allocator COMMAND test_hugepage_allocator)

add_executable(test_policy_based_allocator test_policy_based_allocator.cpp)
target_link_libraries(test_policy_based_allocator PRIVATE
  Catch2::Catch2WithMain
  kressler::fast_containers
)
add_test(NAME test_policy_based_allocator COMMAND test_policy_based_allocator)
