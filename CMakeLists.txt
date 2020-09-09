cmake_minimum_required(VERSION 3.5)

project(glim)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(vendor/glm)
add_subdirectory(glim)
add_subdirectory(examples/hello)
