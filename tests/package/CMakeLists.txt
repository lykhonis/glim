cmake_minimum_required(VERSION 3.22)
project(glim-package LANGUAGES CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

find_package(glim 0.1 REQUIRED COMPONENTS core paint paint-cpu toolkit toolkit-host)

add_executable(glim-package-cpu src/cpu.cpp)
target_link_libraries(glim-package-cpu PRIVATE glim::paint-cpu)

add_executable(glim-package-host src/host.cpp)
target_link_libraries(glim-package-host PRIVATE glim::toolkit-host)
