add_library(glim-paint STATIC
    src/Context.cpp
    src/Merge.cpp
    src/Encode.cpp)
add_library(glim::paint ALIAS glim-paint)
target_include_directories(glim-paint PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(glim-paint PUBLIC glim::core)
glim_target_defaults(glim-paint)

add_library(glim-paint-ref STATIC src/Raster.cpp)
add_library(glim::paint-ref ALIAS glim-paint-ref)
target_include_directories(glim-paint-ref PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(glim-paint-ref PUBLIC glim::paint z)
glim_target_defaults(glim-paint-ref)

add_library(glim-paint-gpu STATIC src/Renderer.cpp)
add_library(glim::paint-gpu ALIAS glim-paint-gpu)
target_include_directories(glim-paint-gpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(glim-paint-gpu PUBLIC glim::paint glim::gpu)
glim_target_defaults(glim-paint-gpu)
target_compile_definitions(glim-paint-gpu PRIVATE
    GLIM_METAL_SHADER_DIR="${CMAKE_SOURCE_DIR}/packages/glim-gpu-metal/shaders")

add_executable(glim-merge-test tests/merge_test.cpp)
target_link_libraries(glim-merge-test PRIVATE glim::paint)
glim_target_defaults(glim-merge-test)
add_test(NAME glim-merge-test COMMAND glim-merge-test)

add_executable(glim-encode-test tests/encode_test.cpp)
target_link_libraries(glim-encode-test PRIVATE glim::paint)
glim_target_defaults(glim-encode-test)
add_test(NAME glim-encode-test COMMAND glim-encode-test)

add_custom_target(glim-paint-tests DEPENDS glim-merge-test glim-encode-test)
