add_library(glim-paint STATIC
    src/Context.cpp
    src/Merge.cpp
    src/Raster.cpp
    src/Renderer.cpp)
add_library(glim::paint ALIAS glim-paint)
target_include_directories(glim-paint PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(glim-paint PUBLIC glim::core glim::gpu z)
glim_target_defaults(glim-paint)
target_compile_definitions(glim-paint PRIVATE
    GLIM_METAL_SHADER_DIR="${CMAKE_SOURCE_DIR}/packages/glim-gpu-metal/shaders")

add_executable(glim-merge-test tests/merge_test.cpp)
target_link_libraries(glim-merge-test PRIVATE glim::paint)
glim_target_defaults(glim-merge-test)
add_test(NAME glim-merge-test COMMAND glim-merge-test)

add_executable(glim-golden-test tests/golden_test.cpp)
target_link_libraries(glim-golden-test PRIVATE glim::paint)
glim_target_defaults(glim-golden-test)
add_test(NAME glim-golden-test
         COMMAND glim-golden-test "${CMAKE_SOURCE_DIR}/tests/golden/hello.png")
