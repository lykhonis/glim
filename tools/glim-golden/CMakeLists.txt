cmake_minimum_required(VERSION 3.22)
project(glim-golden LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Host-only CLI over glim-image, built without the GPU/shell stack so CI
# jobs that only compare screenshots need no platform toolchain.
set(_image ${CMAKE_CURRENT_SOURCE_DIR}/../../packages/glim-image)
find_package(ZLIB REQUIRED)

add_executable(glim-golden
    src/main.cpp
    ${_image}/src/Analyze.cpp
    ${_image}/src/Image.cpp
    ${_image}/src/Load.cpp
    ${_image}/src/Png.cpp)
target_include_directories(glim-golden PRIVATE
    ${_image}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/stb)
target_link_libraries(glim-golden PRIVATE ZLIB::ZLIB)
set_source_files_properties(${_image}/src/Load.cpp PROPERTIES COMPILE_OPTIONS "-w")
