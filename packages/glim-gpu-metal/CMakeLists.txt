add_library(glim-gpu-metal STATIC src/Device.mm)
add_library(glim::gpu-metal ALIAS glim-gpu-metal)
set_source_files_properties(src/Device.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
target_link_libraries(glim-gpu-metal PUBLIC glim::gpu)
glim_target_defaults(glim-gpu-metal)

find_library(GLIM_METAL Metal REQUIRED)
find_library(GLIM_QUARTZ QuartzCore REQUIRED)
find_library(GLIM_FOUNDATION Foundation REQUIRED)
find_library(GLIM_IOSURFACE IOSurface REQUIRED)
target_link_libraries(glim-gpu-metal PUBLIC ${GLIM_METAL} ${GLIM_QUARTZ} ${GLIM_FOUNDATION}
                      ${GLIM_IOSURFACE})

string(TOLOWER "${CMAKE_OSX_SYSROOT}" _glim_sysroot)
if(_glim_sysroot MATCHES "iphonesimulator")
    set(_glim_metal_sdk iphonesimulator)
    set(_glim_metal_min -mios-simulator-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET})
elseif(_glim_sysroot MATCHES "iphoneos")
    set(_glim_metal_sdk iphoneos)
    set(_glim_metal_min -mios-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET})
elseif(_glim_sysroot MATCHES "appletvsimulator")
    set(_glim_metal_sdk appletvsimulator)
    set(_glim_metal_min -mtvos-simulator-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET})
elseif(_glim_sysroot MATCHES "appletvos")
    set(_glim_metal_sdk appletvos)
    set(_glim_metal_min -mtvos-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET})
else()
    set(_glim_metal_sdk macosx)
    set(_glim_metal_min -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET})
endif()
execute_process(COMMAND xcrun -sdk ${_glim_metal_sdk} -f metal
    RESULT_VARIABLE _glim_metal_found OUTPUT_QUIET ERROR_QUIET)
if(NOT _glim_metal_found EQUAL 0)
    message(FATAL_ERROR "Metal compiler missing for ${_glim_metal_sdk}: xcodebuild -downloadComponent MetalToolchain")
endif()

include(GlimShaders)
set(_glim_msl_dir "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(_glim_metallib_dir "${CMAKE_CURRENT_BINARY_DIR}/metallib")
set(_glim_metallib_outputs)
foreach(_name ${GLIM_SHADERS})
    foreach(_stage ${GLIM_SHADER_STAGES})
        set(_shader "${_name}.${_stage}")
        set(_lib "${_glim_metallib_dir}/${_shader}.metallib")
        add_custom_command(
            OUTPUT "${_lib}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${_glim_metallib_dir}"
            COMMAND xcrun -sdk ${_glim_metal_sdk} metal ${_glim_metal_min} -Werror
                "${_glim_msl_dir}/${_shader}.metal" -o "${_lib}"
            DEPENDS "${_glim_msl_dir}/${_shader}.metal"
            COMMENT "metallib ${_shader}"
            VERBATIM)
        list(APPEND _glim_metallib_outputs "${_lib}")
    endforeach()
endforeach()

set(_glim_metal_header "${CMAKE_CURRENT_BINARY_DIR}/glim_metal_shaders.h")
add_custom_command(
    OUTPUT "${_glim_metal_header}"
    COMMAND "${CMAKE_COMMAND}"
        "-DGLIM_BLOB_DIR=${_glim_metallib_dir}"
        -DGLIM_BLOB_EXT=metallib
        -DGLIM_SHADER_NS=metal_shaders
        "-DGLIM_HEADER_OUT=${_glim_metal_header}"
        -P "${CMAKE_SOURCE_DIR}/cmake/GlimEmbedBinary.cmake"
    DEPENDS ${_glim_metallib_outputs}
        "${CMAKE_SOURCE_DIR}/cmake/GlimShaders.cmake"
        "${CMAKE_SOURCE_DIR}/cmake/GlimEmbedBinary.cmake"
    COMMENT "Embedding Metal libraries"
    VERBATIM)
add_custom_target(glim-metal-shaders DEPENDS "${_glim_metal_header}")
set(GLIM_METAL_SHADER_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)
