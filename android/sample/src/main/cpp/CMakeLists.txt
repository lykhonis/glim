cmake_minimum_required(VERSION 3.22)
project(glim-sample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
find_package(glim REQUIRED CONFIG)

add_library(glim-sample SHARED ../../../../../tests/package/src/host.cpp)
target_link_libraries(glim-sample PRIVATE glim::glim)
