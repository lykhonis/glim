add_library(glim-core STATIC src/math.cpp)
add_library(glim::core ALIAS glim-core)
target_include_directories(glim-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
glim_target_defaults(glim-core)

add_executable(glim-math-test tests/math_test.cpp)
target_link_libraries(glim-math-test PRIVATE glim::core)
glim_target_defaults(glim-math-test)
add_test(NAME glim-math-test COMMAND glim-math-test)
