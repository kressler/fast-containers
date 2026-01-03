cmake_minimum_required(VERSION 3.30)
project(fast_containers)

# Create header-only INTERFACE library
add_library(fast_containers INTERFACE)
add_library(kressler::fast_containers ALIAS fast_containers)

# Specify include directories
target_include_directories(fast_containers INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Require C++23
target_compile_features(fast_containers INTERFACE cxx_std_23)

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
        target_compile_options(fast_containers INTERFACE -mavx2 -mfma)
    elseif(MSVC)
        message(STATUS "Building with AVX2 optimizations enabled (MSVC)")
        target_compile_options(fast_containers INTERFACE /arch:AVX2)
    endif()
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        message(STATUS "Building without AVX2 (explicitly disabled)")
    else()
        message(STATUS "Building without AVX2 (default for Debug)")
    endif()
endif()

# Option to enable allocator statistics tracking
# Tracks allocation counts, growth events, peak usage, etc.
option(ENABLE_ALLOCATOR_STATS "Enable allocator statistics tracking" OFF)

if(ENABLE_ALLOCATOR_STATS)
    message(STATUS "Building with allocator statistics tracking enabled")
    target_compile_definitions(fast_containers INTERFACE ALLOCATOR_STATS)
endif()

# Only build tests and benchmarks if this is the main project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
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

    # Option to enable profiling symbols for callgrind/cachegrind/perf analysis
    # Adds debug symbols and frame pointers without affecting optimizations
    option(ENABLE_PROFILING "Enable debug symbols and frame pointers for profiling" OFF)

    if(ENABLE_PROFILING)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            message(STATUS "Building with profiling support enabled (debug symbols + frame pointers)")
            add_compile_options(-g)
            add_compile_options(-fno-omit-frame-pointer)
        elseif(MSVC)
            message(STATUS "Building with profiling support enabled (MSVC)")
            add_compile_options(/Zi)
        else()
            message(WARNING "Profiling flags not configured for this compiler")
        endif()
    endif()

    # Option to enable Link-Time Optimization (LTO)
    # May be disabled to work around toolchain incompatibilities
    option(ENABLE_LTO "Enable Link-Time Optimization" ON)

    # Set aggressive optimization flags for Release builds only
    # Applied BEFORE third-party libraries so our code gets optimized
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            # Aggressive optimizations
            add_compile_options(-O3)
            add_compile_options(-march=haswell)  # Haswell or later for AVX2
            add_compile_options(-ffast-math)
            add_compile_options(-funroll-loops)
            add_compile_options(-fno-trapping-math)
            add_compile_options(-mtune=native)

            # Link-Time Optimization (conditionally enabled)
            if(ENABLE_LTO)
                message(STATUS "Building with Link-Time Optimization (LTO) enabled")
                if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                    # Clang: use ThinLTO for faster builds and better scalability
                    add_compile_options(-flto=thin)
                    add_link_options(-flto=thin)
                else()
                    # GCC: use auto LTO for parallel linking
                    add_compile_options(-flto=auto)
                    add_link_options(-flto=auto)
                endif()
            else()
                message(STATUS "Building without LTO (disabled)")
            endif()
        elseif(MSVC)
            # MSVC optimizations
            add_compile_options(/O2 /fp:fast)
        endif()
    endif()

    # Build tests
    enable_testing()
    add_subdirectory(tests)

    # Option to build benchmarks
    option(BUILD_BENCHMARKS "Build benchmarks and stress tests" ON)

    if(BUILD_BENCHMARKS)
        message(STATUS "Building benchmarks and stress tests")

        # Add histograms library as a submodule
        add_subdirectory(third_party/histograms)

        # Add Google Benchmark
        set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark tests" FORCE)
        set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "Disable gtest tests" FORCE)
        # Disable -Werror in Google Benchmark to avoid issues with -ffast-math
        # (std::isnan/std::isinf trigger -Wnan-infinity-disabled with -ffast-math)
        set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "Disable -Werror for benchmark" FORCE)
        add_subdirectory(third_party/benchmark)

        # Add Abseil
        set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "Use C++23 for Abseil" FORCE)
        add_subdirectory(third_party/abseil-cpp)

        # Add Lyra (header-only)
        add_library(lyra INTERFACE)
        target_include_directories(lyra INTERFACE
          $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/lyra/include>
          $<INSTALL_INTERFACE:include>
        )
        target_compile_features(lyra INTERFACE cxx_std_23)
        add_library(lyra::lyra ALIAS lyra)

        # Add unordered_dense (header-only)
        add_library(unordered_dense INTERFACE)
        target_include_directories(unordered_dense INTERFACE
          $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/unordered_dense/include>
          $<INSTALL_INTERFACE:include>
        )
        target_compile_features(unordered_dense INTERFACE cxx_std_23)
        add_library(unordered_dense::unordered_dense ALIAS unordered_dense)

        # Build benchmarks and binaries
        add_subdirectory(src)
    else()
        message(STATUS "Skipping benchmarks (BUILD_BENCHMARKS=OFF)")
    endif()
endif()
