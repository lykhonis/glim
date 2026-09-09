add_library(glim-paint STATIC src/Context.cpp src/Renderer.cpp)
add_library(glim::paint ALIAS glim-paint)
target_include_directories(glim-paint PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(glim-paint PUBLIC glim::core glim::gpu)
glim_target_defaults(glim-paint)
target_compile_definitions(glim-paint PRIVATE
    GLIM_METAL_SHADER_DIR="${CMAKE_SOURCE_DIR}/packages/glim-gpu-metal/shaders")
