# Build-time: turn compiled .spv blobs into glim_vulkan_shaders.h
# Required -D:
#   GLIM_SPV_DIR, GLIM_HEADER_IN, GLIM_HEADER_OUT

if(NOT GLIM_SPV_DIR OR NOT GLIM_HEADER_IN OR NOT GLIM_HEADER_OUT)
    message(FATAL_ERROR "GlimEmbedSpirv: GLIM_SPV_DIR, GLIM_HEADER_IN, GLIM_HEADER_OUT required")
endif()

function(glim_spv_bytes file varname)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "GlimEmbedSpirv: missing ${file}")
    endif()
    file(READ "${file}" _hex HEX)
    if(NOT _hex)
        message(FATAL_ERROR "GlimEmbedSpirv: empty ${file}")
    endif()
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _bytes "${_hex}")
    set(${varname} "${_bytes}" PARENT_SCOPE)
endfunction()

glim_spv_bytes("${GLIM_SPV_DIR}/solid.vert.spv" GLIM_SOLID_VERT_SPV)
glim_spv_bytes("${GLIM_SPV_DIR}/solid.frag.spv" GLIM_SOLID_FRAG_SPV)
glim_spv_bytes("${GLIM_SPV_DIR}/blit.vert.spv" GLIM_BLIT_VERT_SPV)
glim_spv_bytes("${GLIM_SPV_DIR}/blit.frag.spv" GLIM_BLIT_FRAG_SPV)

configure_file("${GLIM_HEADER_IN}" "${GLIM_HEADER_OUT}" @ONLY)
