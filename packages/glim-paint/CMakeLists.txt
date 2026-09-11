add_library(glim-paint STATIC
    src/Context.cpp
    src/Merge.cpp
    src/Encode.cpp
    src/Software.cpp)
add_library(glim::paint ALIAS glim-paint)
target_include_directories(glim-paint PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(glim-paint PUBLIC glim::core)
glim_target_defaults(glim-paint)

set(_glim_shader_dir "${CMAKE_SOURCE_DIR}/packages/glim-gpu-metal/shaders")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${_glim_shader_dir}/solid.metal"
    "${_glim_shader_dir}/blit.metal")
file(READ "${_glim_shader_dir}/solid.metal" GLIM_SOLID_METAL)
file(READ "${_glim_shader_dir}/blit.metal" GLIM_BLIT_METAL)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/glim_metal_shaders.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/glim_metal_shaders.h"
    @ONLY)

function(glim_add_renderer name)
    add_library(${name} STATIC src/Renderer.cpp)
    target_include_directories(${name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${name} PUBLIC glim::paint ${ARGN})
    glim_target_defaults(${name})
    target_compile_definitions(${name} PRIVATE
        GLIM_METAL_SHADER_DIR="${CMAKE_SOURCE_DIR}/packages/glim-gpu-metal/shaders")
endfunction()

glim_add_renderer(glim-paint-gpu glim::gpu)
target_compile_definitions(glim-paint-gpu PUBLIC GLIM_SOFTWARE=0 GLIM_EMBED=0)
if(GLIM_GPU STREQUAL "vulkan")
    target_compile_definitions(glim-paint-gpu PRIVATE GLIM_GPU_VULKAN=1)
    target_include_directories(glim-paint-gpu PRIVATE "${GLIM_VULKAN_SHADER_INCLUDE}")
    add_dependencies(glim-paint-gpu glim-vulkan-shaders)
endif()
add_library(glim::paint-gpu ALIAS glim-paint-gpu)

glim_add_renderer(glim-paint-cpu)
target_compile_definitions(glim-paint-cpu PUBLIC GLIM_SOFTWARE=1 GLIM_EMBED=0)
add_library(glim::paint-cpu ALIAS glim-paint-cpu)

glim_add_renderer(glim-paint-embed-gpu glim::gpu)
target_compile_definitions(glim-paint-embed-gpu PUBLIC GLIM_SOFTWARE=0 GLIM_EMBED=1)
if(GLIM_GPU STREQUAL "vulkan")
    target_compile_definitions(glim-paint-embed-gpu PRIVATE GLIM_GPU_VULKAN=1)
    target_include_directories(glim-paint-embed-gpu PRIVATE "${GLIM_VULKAN_SHADER_INCLUDE}")
    add_dependencies(glim-paint-embed-gpu glim-vulkan-shaders)
endif()
add_library(glim::paint-embed-gpu ALIAS glim-paint-embed-gpu)

if(GLIM_SOFTWARE)
    if(GLIM_EMBED)
        set(_glim_renderer glim::paint-embed-gpu)
        message(FATAL_ERROR "packet-cpu renderer is not built as a named alias yet; use GLIM_SOFTWARE without GLIM_EMBED for cpu draw")
    endif()
    set(_glim_renderer glim::paint-cpu)
elseif(GLIM_EMBED)
    set(_glim_renderer glim::paint-embed-gpu)
else()
    set(_glim_renderer glim::paint-gpu)
endif()

if(GLIM_HOST_TESTS)
    add_executable(glim-merge-test tests/merge_test.cpp)
    target_link_libraries(glim-merge-test PRIVATE glim::paint)
    glim_target_defaults(glim-merge-test)
    add_test(NAME glim-merge-test COMMAND glim-merge-test)

    add_executable(glim-encode-test tests/encode_test.cpp)
    target_link_libraries(glim-encode-test PRIVATE glim::paint)
    glim_target_defaults(glim-encode-test)
    add_test(NAME glim-encode-test COMMAND glim-encode-test)

    add_executable(glim-cpu-test tests/cpu_test.cpp)
    target_link_libraries(glim-cpu-test PRIVATE glim::paint-cpu)
    glim_target_defaults(glim-cpu-test)
    add_test(NAME glim-cpu-test COMMAND glim-cpu-test)

    add_custom_target(glim-paint-tests DEPENDS glim-merge-test glim-encode-test glim-cpu-test)
endif()
