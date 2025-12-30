cmake_minimum_required(VERSION 3.30)
project(fast_containers)

set(CMAKE_CXX_STANDARD 23)

# Create header-only INTERFACE library
add_library(fast_containers INTERFACE)
add_library(fast_containers::fast_containers ALIAS fast_containers)

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
        # Add to INTERFACE library
        target_compile_options(fast_containers INTERFACE -mavx2 -mfma)
        # Also add to tests/binaries
        add_compile_options(-mavx2)
        add_compile_options(-mfma)
        add_compile_options(-march=haswell)  # Haswell or later for AVX2
        add_compile_options(-mtune=native)
    elseif(MSVC)
        message(STATUS "Building with AVX2 optimizations enabled (MSVC)")
        target_compile_options(fast_containers INTERFACE /arch:AVX2)
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

# Option to enable allocator statistics tracking
# Tracks allocation counts, growth events, peak usage, etc.
option(ENABLE_ALLOCATOR_STATS "Enable allocator statistics tracking" OFF)

# Option to enable Link-Time Optimization (LTO)
# May be disabled to work around toolchain incompatibilities
option(ENABLE_LTO "Enable Link-Time Optimization" ON)

if(ENABLE_ALLOCATOR_STATS)
    message(STATUS "Building with allocator statistics tracking enabled")
    target_compile_definitions(fast_containers INTERFACE ALLOCATOR_STATS)
    add_compile_definitions(ALLOCATOR_STATS)
endif()

# Option to enable NUMA (Non-Uniform Memory Access) awareness
# Requires libnuma library (libnuma-dev on Debian/Ubuntu)
# Useful for multi-socket systems to allocate memory on local NUMA nodes
option(ENABLE_NUMA "Enable NUMA awareness (requires libnuma)" OFF)

if(ENABLE_NUMA)
    find_library(NUMA_LIBRARY NAMES numa)
    find_path(NUMA_INCLUDE_DIR NAMES numa.h)

    if(NUMA_LIBRARY AND NUMA_INCLUDE_DIR)
        message(STATUS "Building with NUMA awareness enabled")
        message(STATUS "  libnuma library: ${NUMA_LIBRARY}")
        message(STATUS "  libnuma include: ${NUMA_INCLUDE_DIR}")
        # Add to INTERFACE library
        target_compile_definitions(fast_containers INTERFACE HAVE_NUMA)
        target_include_directories(fast_containers INTERFACE ${NUMA_INCLUDE_DIR})
        target_link_libraries(fast_containers INTERFACE ${NUMA_LIBRARY})
        # Also add to tests/binaries
        add_compile_definitions(HAVE_NUMA)
        include_directories(${NUMA_INCLUDE_DIR})
        link_libraries(${NUMA_LIBRARY})
    else()
        message(WARNING "NUMA support requested but libnuma not found. Install libnuma-dev and try again.")
        message(WARNING "  NUMA_LIBRARY: ${NUMA_LIBRARY}")
        message(WARNING "  NUMA_INCLUDE_DIR: ${NUMA_INCLUDE_DIR}")
    endif()
endif()

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

        # Link-Time Optimization (conditionally enabled)
        if(ENABLE_LTO)
            message(STATUS "Building with Link-Time Optimization (LTO) enabled")
            add_compile_options(-flto)
            add_link_options(-flto)
        else()
            message(STATUS "Building without LTO (disabled)")
        endif()
    elseif(MSVC)
        # MSVC optimizations
        add_compile_options(/O2 /fp:fast)
    endif()
endif()

# Add Catch2 testing framework
add_subdirectory(third_party/catch2)

# Enable testing
enable_testing()

# Clang-format target
find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/include/*.hpp
        ${CMAKE_SOURCE_DIR}/include/*.ipp
        ${CMAKE_SOURCE_DIR}/tests/*.cpp
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

# Only build tests if this is the main project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_subdirectory(tests)
endif()
