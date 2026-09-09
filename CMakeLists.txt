cmake_minimum_required(VERSION 3.22)
project(glim LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GlimCommon)

add_subdirectory(packages/glim-core)
add_subdirectory(packages/glim-gpu)

if(APPLE AND GLIM_GPU STREQUAL "metal")
    add_subdirectory(packages/glim-gpu-metal)
endif()

add_subdirectory(packages/glim-paint)

if(APPLE AND GLIM_SHELL STREQUAL "macos")
    enable_language(OBJCXX)
    add_subdirectory(packages/glim-shell-macos)
endif()

add_subdirectory(examples/hello)

add_library(glim INTERFACE)
add_library(glim::glim ALIAS glim)
target_link_libraries(glim INTERFACE glim::core glim::gpu glim::paint)
if(TARGET glim::gpu-metal)
    target_link_libraries(glim INTERFACE glim::gpu-metal)
endif()
if(TARGET glim::shell)
    target_link_libraries(glim INTERFACE glim::shell)
endif()
