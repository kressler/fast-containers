add_executable(hugepage_allocator_benchmark hugepage_allocator_benchmark.cpp)
target_link_libraries(hugepage_allocator_benchmark PRIVATE
  benchmark::benchmark
  kressler::fast_containers
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(hugepage_allocator_benchmark PRIVATE -Werror)
elseif(MSVC)
  target_compile_options(hugepage_allocator_benchmark PRIVATE /WX)
endif()

add_executable(sample_benchmark sample_benchmark.cpp)
target_link_libraries(sample_benchmark PRIVATE benchmark::benchmark)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(sample_benchmark PRIVATE -Werror)
elseif(MSVC)
  target_compile_options(sample_benchmark PRIVATE /WX)
endif()

add_executable(dense_map_search_benchmark dense_map_search_benchmark.cpp)
target_link_libraries(dense_map_search_benchmark PRIVATE
  benchmark::benchmark
  kressler::fast_containers
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dense_map_search_benchmark PRIVATE -Werror)
elseif(MSVC)
  target_compile_options(dense_map_search_benchmark PRIVATE /WX)
endif()
