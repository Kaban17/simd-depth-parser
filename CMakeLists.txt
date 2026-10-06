cmake_minimum_required(VERSION 3.20)
project(simd_depth_parser CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(depth_parser INTERFACE)
target_include_directories(depth_parser INTERFACE include)
target_compile_options(depth_parser INTERFACE -march=native)

add_executable(depth_bench bench/bench.cpp)
target_link_libraries(depth_bench PRIVATE depth_parser)
target_compile_options(depth_bench PRIVATE -O3 -Wall -Wextra -Wpedantic)
