cmake_minimum_required(VERSION 3.22)
project(glim-lint LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(glim-lint-core src/Tokenizer.cpp src/Factories.cpp)
target_include_directories(glim-lint-core PUBLIC src)

add_executable(glim-lint src/main.cpp)
target_link_libraries(glim-lint PRIVATE glim-lint-core)

enable_testing()
add_test(NAME glim-lint-accepts-good
    COMMAND glim-lint ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/good.h)
add_test(NAME glim-lint-rejects-bad
    COMMAND glim-lint ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/bad.h)
set_tests_properties(glim-lint-rejects-bad PROPERTIES
    PASS_REGULAR_EXPRESSION "pinned takes 3 fixed params.*gone takes As&&\\.\\.\\. without forwarding.*lost takes As&&\\.\\.\\. without finish.*drop takes 2 fixed params.*GlassTag built by hand.*RadiusTag built by hand.*needs a trailing comma")
