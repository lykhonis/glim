find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)

add_library(glim-volk STATIC "${CMAKE_SOURCE_DIR}/vendor/volk/volk.c")
add_library(glim::volk ALIAS glim-volk)
target_include_directories(glim-volk PUBLIC "${CMAKE_SOURCE_DIR}/vendor/volk")
if(TARGET Vulkan::Headers)
    target_link_libraries(glim-volk PUBLIC Vulkan::Headers)
else()
    target_include_directories(glim-volk PUBLIC ${Vulkan_INCLUDE_DIRS})
endif()
target_compile_definitions(glim-volk PUBLIC VK_NO_PROTOTYPES)
target_link_libraries(glim-volk PUBLIC ${CMAKE_DL_LIBS})
set_source_files_properties("${CMAKE_SOURCE_DIR}/vendor/volk/volk.c" PROPERTIES COMPILE_FLAGS "-w")

find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)
set(_glim_glsl_dir "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(_glim_spv_dir "${CMAKE_CURRENT_BINARY_DIR}/spirv")
set(_glim_spirv_script "${CMAKE_SOURCE_DIR}/tools/scripts/compile-spirv.sh")
set(_glim_spv_outputs)
foreach(_shader solid.vert solid.frag blit.vert blit.frag)
    set(_spv "${_glim_spv_dir}/${_shader}.spv")
    add_custom_command(
        OUTPUT "${_spv}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_glim_spv_dir}"
        COMMAND /bin/bash "${_glim_spirv_script}" "${_glim_glsl_dir}/${_shader}" "${_spv}"
        DEPENDS "${_glim_glsl_dir}/${_shader}" "${_glim_spirv_script}"
        COMMENT "SPIR-V ${_shader}"
        VERBATIM)
    list(APPEND _glim_spv_outputs "${_spv}")
endforeach()

set(_glim_vk_header "${CMAKE_CURRENT_BINARY_DIR}/glim_vulkan_shaders.h")
add_custom_command(
    OUTPUT "${_glim_vk_header}"
    COMMAND "${CMAKE_COMMAND}"
        "-DGLIM_SPV_DIR=${_glim_spv_dir}"
        "-DGLIM_HEADER_IN=${CMAKE_CURRENT_SOURCE_DIR}/glim_vulkan_shaders.h.in"
        "-DGLIM_HEADER_OUT=${_glim_vk_header}"
        -P "${CMAKE_SOURCE_DIR}/cmake/GlimEmbedSpirv.cmake"
    DEPENDS ${_glim_spv_outputs}
        "${CMAKE_CURRENT_SOURCE_DIR}/glim_vulkan_shaders.h.in"
        "${CMAKE_SOURCE_DIR}/cmake/GlimEmbedSpirv.cmake"
    COMMENT "Embedding Vulkan SPIR-V"
    VERBATIM)
add_custom_target(glim-vulkan-shaders DEPENDS "${_glim_vk_header}")
set(GLIM_VULKAN_SHADER_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)

add_library(glim-gpu-vulkan STATIC src/Device.cpp)
add_library(glim::gpu-vulkan ALIAS glim-gpu-vulkan)
target_link_libraries(glim-gpu-vulkan PUBLIC glim::gpu glim::volk)
target_include_directories(glim-gpu-vulkan PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
target_compile_definitions(glim-gpu-vulkan PRIVATE VK_USE_PLATFORM_WAYLAND_KHR)
add_dependencies(glim-gpu-vulkan glim-vulkan-shaders)
glim_target_defaults(glim-gpu-vulkan)
