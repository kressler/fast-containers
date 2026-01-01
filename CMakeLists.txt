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

# Only build tests if this is the main project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()
    add_subdirectory(tests)
endif()
