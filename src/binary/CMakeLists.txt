add_executable(btree_stress btree_stress.cpp)
target_link_libraries(btree_stress PRIVATE
  absl::base
  benchmark::benchmark
  lyra::lyra
  kressler::fast_containers
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(btree_stress PRIVATE -Werror)
elseif(MSVC)
  target_compile_options(btree_stress PRIVATE /WX)
endif()

add_executable(btree_benchmark btree_benchmark.cpp)
target_link_libraries(btree_benchmark PRIVATE
  absl::base
  benchmark::benchmark
  histograms::histograms
  lyra::lyra
  unordered_dense::unordered_dense
  kressler::fast_containers
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(btree_benchmark PRIVATE -Werror)
elseif(MSVC)
  target_compile_options(btree_benchmark PRIVATE /WX)
endif()
