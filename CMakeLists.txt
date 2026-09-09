cmake_minimum_required(VERSION 3.22)
project(glim LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GlimCommon)

add_subdirectory(packages/glim-core)
add_subdirectory(packages/glim-gpu)

# Legacy OpenGL/GLM tree (unused by new packages; removed in a later PR).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm/CMakeLists.txt")
    add_subdirectory(vendor/glm)
    add_subdirectory(glim)
    add_subdirectory(examples/hello)
endif()
