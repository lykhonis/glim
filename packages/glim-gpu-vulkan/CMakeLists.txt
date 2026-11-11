add_library(glim-volk STATIC "${CMAKE_SOURCE_DIR}/vendor/volk/volk.c")
add_library(glim::volk ALIAS glim-volk)
target_include_directories(glim-volk PUBLIC "${CMAKE_SOURCE_DIR}/vendor/volk")
target_link_libraries(glim-volk PUBLIC ${CMAKE_DL_LIBS})
set_source_files_properties("${CMAKE_SOURCE_DIR}/vendor/volk/volk.c" PROPERTIES COMPILE_FLAGS "-w")

if(ANDROID)
    target_compile_definitions(glim-volk PUBLIC VK_NO_PROTOTYPES VK_USE_PLATFORM_ANDROID_KHR)
    target_link_libraries(glim-volk PUBLIC android)
else()
    find_package(Vulkan REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
    if(TARGET Vulkan::Headers)
        target_link_libraries(glim-volk PUBLIC Vulkan::Headers)
    else()
        target_include_directories(glim-volk PUBLIC ${Vulkan_INCLUDE_DIRS})
    endif()
    target_compile_definitions(glim-volk PUBLIC VK_NO_PROTOTYPES VK_USE_PLATFORM_WAYLAND_KHR)
    target_include_directories(glim-volk PUBLIC ${WAYLAND_CLIENT_INCLUDE_DIRS})
endif()

find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)
include(GlimShaders)
set(_glim_glsl_dir "${GLIM_SHADER_SOURCE_DIR}")
set(_glim_spv_dir "${CMAKE_CURRENT_BINARY_DIR}/spirv")
set(_glim_spv_outputs)
foreach(_name ${GLIM_SHADERS})
  foreach(_stage ${GLIM_SHADER_STAGES})
    set(_shader "${_name}.${_stage}")
    set(_spv "${_glim_spv_dir}/${_shader}.spv")
    add_custom_command(
        OUTPUT "${_spv}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_glim_spv_dir}"
        COMMAND "${GLSLANG_VALIDATOR}" -V --target-env vulkan1.1 "${_glim_glsl_dir}/${_shader}" -o "${_spv}"
        DEPENDS "${_glim_glsl_dir}/${_shader}"
        COMMENT "SPIR-V ${_shader}"
        VERBATIM)
    list(APPEND _glim_spv_outputs "${_spv}")
  endforeach()
endforeach()

set(_glim_vk_header "${CMAKE_CURRENT_BINARY_DIR}/glim_vulkan_shaders.h")
add_custom_command(
    OUTPUT "${_glim_vk_header}"
    COMMAND "${CMAKE_COMMAND}"
        "-DGLIM_SPV_DIR=${_glim_spv_dir}"
        "-DGLIM_HEADER_OUT=${_glim_vk_header}"
        -P "${CMAKE_SOURCE_DIR}/cmake/GlimEmbedSpirv.cmake"
    DEPENDS ${_glim_spv_outputs}
        "${CMAKE_SOURCE_DIR}/cmake/GlimShaders.cmake"
        "${CMAKE_SOURCE_DIR}/cmake/GlimEmbedSpirv.cmake"
    COMMENT "Embedding Vulkan SPIR-V"
    VERBATIM)
add_custom_target(glim-vulkan-shaders DEPENDS "${_glim_vk_header}")
set(GLIM_VULKAN_SHADER_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)

add_library(glim-gpu-vulkan STATIC
    src/Device.cpp
    src/VulkanCreate.cpp
    src/VulkanObjects.cpp
    src/VulkanPass.cpp
    src/VulkanResources.cpp
    src/VulkanSwapchain.cpp)
add_library(glim::gpu-vulkan ALIAS glim-gpu-vulkan)
target_link_libraries(glim-gpu-vulkan PUBLIC glim::gpu glim::volk)
add_dependencies(glim-gpu-vulkan glim-vulkan-shaders)
glim_target_defaults(glim-gpu-vulkan)
