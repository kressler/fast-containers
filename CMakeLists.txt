cmake_minimum_required(VERSION 3.30)
project(fast_containers)

set(CMAKE_CXX_STANDARD 23)

# Option to enable AVX2 optimizations
option(ENABLE_AVX2 "Enable AVX2 SIMD optimizations" OFF)

# Set aggressive optimization flags for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Aggressive optimizations
        add_compile_options(-O3)
        add_compile_options(-ffast-math)
        add_compile_options(-funroll-loops)
        add_compile_options(-fno-trapping-math)

        if(ENABLE_AVX2)
            message(STATUS "Building with AVX2 optimizations enabled")
            add_compile_options(-mavx2)
            add_compile_options(-mfma)
            add_compile_options(-march=haswell)  # Haswell or later for AVX2
            add_compile_options(-mtune=native)
        else()
            message(STATUS "Building without AVX2 (use -DENABLE_AVX2=ON to enable)")
            add_compile_options(-march=native)
            add_compile_options(-mtune=native)
        endif()
    elseif(MSVC)
        # MSVC optimizations
        add_compile_options(/O2 /fp:fast)

        if(ENABLE_AVX2)
            message(STATUS "Building with AVX2 optimizations enabled (MSVC)")
            add_compile_options(/arch:AVX2)
        endif()
    endif()
endif()

# Add Catch2 testing framework
add_subdirectory(third_party/catch2)

# Add Google Benchmark
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark tests" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "Disable gtest tests" FORCE)
add_subdirectory(third_party/benchmark)

# Enable testing
enable_testing()

# Clang-format target
find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.hpp
    )
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${ALL_SOURCE_FILES}
        COMMENT "Running clang-format on all C++ source files"
        VERBATIM
    )
else()
    message(WARNING "clang-format not found. 'format' target will not be available.")
endif()

add_subdirectory(src)
