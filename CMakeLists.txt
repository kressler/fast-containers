cmake_minimum_required(VERSION 4.0)
project(fast_containers)

set(CMAKE_CXX_STANDARD 23)

# Add Catch2 testing framework
add_subdirectory(third_party/catch2)

add_subdirectory(src)
