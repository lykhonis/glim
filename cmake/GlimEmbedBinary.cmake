# Build-time: turn compiled per-stage shader blobs (<dir>/<name>.<stage>.<ext>)
# into a header table.
# Required -D: GLIM_BLOB_DIR, GLIM_BLOB_EXT, GLIM_SHADER_NS, GLIM_HEADER_OUT
include("${CMAKE_CURRENT_LIST_DIR}/GlimShaders.cmake")

if(NOT GLIM_BLOB_DIR OR NOT GLIM_BLOB_EXT OR NOT GLIM_SHADER_NS OR NOT GLIM_HEADER_OUT)
    message(FATAL_ERROR "GlimEmbedBinary: GLIM_BLOB_DIR, GLIM_BLOB_EXT, GLIM_SHADER_NS and GLIM_HEADER_OUT required")
endif()

set(_entries "")
foreach(_name ${GLIM_SHADERS})
    foreach(_stage ${GLIM_SHADER_STAGES})
        set(_file "${GLIM_BLOB_DIR}/${_name}.${_stage}.${GLIM_BLOB_EXT}")
        file(READ "${_file}" _hex HEX)
        if(NOT _hex)
            message(FATAL_ERROR "GlimEmbedBinary: missing or empty ${_file}")
        endif()
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _bytes "${_hex}")
        string(APPEND _entries "inline constexpr unsigned char ${_name}_${_stage}[] = {${_bytes}};\n")
    endforeach()
endforeach()
glim_shader_table_header("${GLIM_HEADER_OUT}" "${GLIM_SHADER_NS}" "${_entries}" 0)
