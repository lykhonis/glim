# Shaders are whatever packages/glim-gpu/shaders/<name>.vert (+ .frag) exist:
# GLSL, hand-edited. tools/shaders.mjs generates the committed
# <name>.<stage>.metal and .wgsl beside each backend from the same directory.
set(GLIM_SHADER_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../packages/glim-gpu/shaders")
if(CMAKE_SCRIPT_MODE_FILE)  # GlimEmbedBinary runs under cmake -P
    file(GLOB _glim_vert_sources "${GLIM_SHADER_SOURCE_DIR}/*.vert")
else()
    file(GLOB _glim_vert_sources CONFIGURE_DEPENDS "${GLIM_SHADER_SOURCE_DIR}/*.vert")
endif()
set(GLIM_SHADERS "")
foreach(_vert ${_glim_vert_sources})
    get_filename_component(_name "${_vert}" NAME_WE)
    list(APPEND GLIM_SHADERS "${_name}")
endforeach()
list(SORT GLIM_SHADERS)
set(GLIM_SHADER_STAGES vert frag)

# Header body shared by every backend: per-stage arrays plus
#   glim::<ns>::kSources[] = {{name, {vert, size}, {frag, size}}, ...}
# <entries> declares <name>_<stage> arrays; <nul> is 1 when each array ends in a
# NUL that the size must exclude (text), 0 for binary.
function(glim_shader_table_header out ns entries nul)
    set(_body "#pragma once\n\n#include <cstddef>\n\nnamespace glim::${ns} {\n\n")
    string(APPEND _body "${entries}\n")
    string(APPEND _body "struct Stage {\n  const void* data;\n  std::size_t size;\n};\n\n")
    string(APPEND _body "struct Source {\n  const char* name;\n  Stage vert;\n  Stage frag;\n};\n\n")
    string(APPEND _body "inline constexpr Source kSources[] = {\n")
    foreach(_name ${GLIM_SHADERS})
        string(APPEND _body "    {\"${_name}\", {${_name}_vert, sizeof(${_name}_vert) - ${nul}}, {${_name}_frag, sizeof(${_name}_frag) - ${nul}}},\n")
    endforeach()
    string(APPEND _body "};\n\n}  // namespace glim::${ns}\n")
    # Verbatim (WGSL uses @ attributes) and only on change, so builds stay incremental.
    if(EXISTS "${out}")
        file(READ "${out}" _old)
        if(_old STREQUAL _body)
            return()
        endif()
    endif()
    file(WRITE "${out}" "${_body}")
endfunction()

# Configure-time: embed the committed text shaders (<dir>/<name>.<stage>.<ext>).
function(glim_embed_text_shaders out ns dir ext)
    set(_entries "")
    foreach(_name ${GLIM_SHADERS})
        foreach(_stage ${GLIM_SHADER_STAGES})
            set(_file "${dir}/${_name}.${_stage}.${ext}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_file}")
            file(READ "${_file}" _text)
            string(APPEND _entries
                "inline constexpr char ${_name}_${_stage}[] = R\"GLIMSHADER(${_text})GLIMSHADER\";\n")
        endforeach()
    endforeach()
    glim_shader_table_header("${out}" "${ns}" "${_entries}" 1)
endfunction()
