# Build-time: turn compiled .spv blobs into glim_vulkan_shaders.h.
# Required -D: GLIM_SPV_DIR, GLIM_HEADER_OUT
include("${CMAKE_CURRENT_LIST_DIR}/GlimShaders.cmake")

if(NOT GLIM_SPV_DIR OR NOT GLIM_HEADER_OUT)
    message(FATAL_ERROR "GlimEmbedSpirv: GLIM_SPV_DIR and GLIM_HEADER_OUT required")
endif()

set(_entries "")
foreach(_name ${GLIM_SHADERS})
    foreach(_stage ${GLIM_SHADER_STAGES})
        set(_file "${GLIM_SPV_DIR}/${_name}.${_stage}.spv")
        file(READ "${_file}" _hex HEX)
        if(NOT _hex)
            message(FATAL_ERROR "GlimEmbedSpirv: missing or empty ${_file}")
        endif()
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _bytes "${_hex}")
        string(APPEND _entries "inline constexpr unsigned char ${_name}_${_stage}[] = {${_bytes}};\n")
    endforeach()
endforeach()
glim_shader_table_header("${GLIM_HEADER_OUT}" vulkan_shaders "${_entries}" 0)
