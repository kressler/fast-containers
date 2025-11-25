cmake_minimum_required(VERSION 3.30)
project(fast_containers)

set(CMAKE_CXX_STANDARD 23)

# Option to enable AVX2 optimizations
# Default to ON for Release builds, OFF for Debug (most modern CPUs support AVX2)
# Users can override with -DENABLE_AVX2=ON/OFF as needed
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    option(ENABLE_AVX2 "Enable AVX2 SIMD optimizations" ON)
else()
    option(ENABLE_AVX2 "Enable AVX2 SIMD optimizations" OFF)
endif()

# Apply AVX2 flags if enabled (for both Debug and Release)
if(ENABLE_AVX2)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            message(STATUS "Building with AVX2 optimizations enabled (default for Release)")
        else()
            message(STATUS "Building with AVX2 optimizations enabled (explicitly enabled for Debug)")
        endif()
        add_compile_options(-mavx2)
        add_compile_options(-mfma)
        add_compile_options(-march=haswell)  # Haswell or later for AVX2
        add_compile_options(-mtune=native)
    elseif(MSVC)
        message(STATUS "Building with AVX2 optimizations enabled (MSVC)")
        add_compile_options(/arch:AVX2)
    endif()
else()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            message(STATUS "Building without AVX2 (explicitly disabled)")
        else()
            message(STATUS "Building without AVX2 (default for Debug)")
        endif()
        add_compile_options(-march=native)
        add_compile_options(-mtune=native)
    endif()
endif()

# Option to enable AddressSanitizer (ASAN) for memory error detection
# Useful for debugging memory issues like buffer overflows, use-after-free, etc.
option(ENABLE_ASAN "Enable AddressSanitizer for memory error detection" OFF)

if(ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Building with AddressSanitizer (ASAN) enabled")
        add_compile_options(-fsanitize=address)
        add_compile_options(-fno-omit-frame-pointer)
        add_compile_options(-g)
        add_link_options(-fsanitize=address)

        # Disable optimizations that interfere with ASAN
        add_compile_options(-O1)
        add_compile_options(-fno-optimize-sibling-calls)
    elseif(MSVC)
        message(STATUS "Building with AddressSanitizer (ASAN) enabled (MSVC)")
        add_compile_options(/fsanitize=address)
        add_compile_options(/Zi)
    else()
        message(WARNING "AddressSanitizer not supported for this compiler")
    endif()
endif()

# Set aggressive optimization flags for Release builds only
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        # Aggressive optimizations
        add_compile_options(-O3)
        add_compile_options(-ffast-math)
        add_compile_options(-funroll-loops)
        add_compile_options(-fno-trapping-math)
        add_compile_options(-flto)
    elseif(MSVC)
        # MSVC optimizations
        add_compile_options(/O2 /fp:fast)
    endif()
endif()

# Add Catch2 testing framework
add_subdirectory(third_party/catch2)

# Add Google Benchmark
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark tests" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "Disable gtest tests" FORCE)
# Disable -Werror in Google Benchmark when ASAN is enabled to avoid false positives
if(ENABLE_ASAN)
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "Disable -Werror for ASAN builds" FORCE)
endif()
add_subdirectory(third_party/benchmark)

# Add Abseil library
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "Use C++23 for Abseil" FORCE)
add_subdirectory(third_party/abseil-cpp)

# Add Lyra commandline parser (header-only)
add_library(lyra INTERFACE)
target_include_directories(lyra INTERFACE third_party/lyra/include)

# Add ankerl::unordered_dense (header-only)
add_library(unordered_dense INTERFACE)
target_include_directories(unordered_dense INTERFACE third_party/unordered_dense/include)

add_library(histograms INTERFACE)
target_include_directories(histograms INTERFACE third_party/histograms/src)

# Enable testing
enable_testing()

# Clang-format target
find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.hpp
        ${CMAKE_SOURCE_DIR}/src/*.ipp
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
