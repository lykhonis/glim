add_library(glim-gpu-webgpu STATIC src/Device.cpp)
add_library(glim::gpu-webgpu ALIAS glim-gpu-webgpu)
target_include_directories(glim-gpu-webgpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(glim-gpu-webgpu PUBLIC glim::gpu)
glim_target_defaults(glim-gpu-webgpu)

# WGSL sources are data, not build inputs yet (slice 2 compiles them via
# tint/naga or embeds as strings). Keep them visible to IDEs.
file(GLOB _glim_wgsl_shaders "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.wgsl")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_glim_wgsl_shaders})
