add_library(glim-paint STATIC
    src/Context.cpp
    src/Merge.cpp
    src/HitTest.cpp
    src/Glass.cpp
    src/Encode.cpp
    src/RasterFill.cpp
    src/RasterGlass.cpp
    src/RasterPlate.cpp
    src/Software.cpp
    src/ImageStore.cpp
    src/Strip.cpp
    src/StripCoverage.cpp
    src/StripFlatten.cpp
    src/PolylineCache.cpp
    src/StripGeometry.cpp
    src/Reuse.cpp
    src/Font.cpp)
add_library(glim::paint ALIAS glim-paint)
target_include_directories(glim-paint PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_include_directories(glim-paint PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    "${CMAKE_SOURCE_DIR}/vendor/stb")
find_package(Threads REQUIRED)
target_link_libraries(glim-paint PUBLIC glim::core PRIVATE Threads::Threads)
glim_target_defaults(glim-paint)

file(READ "${CMAKE_SOURCE_DIR}/vendor/inter/Inter-Latin.ttf" _glim_inter_hex HEX)
if(NOT _glim_inter_hex)
    message(FATAL_ERROR "missing vendor/inter/Inter-Latin.ttf")
endif()
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," GLIM_INTER_LATIN_TTF "${_glim_inter_hex}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/inter_latin.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/inter_latin.h"
    @ONLY)

include(GlimShaders)
glim_embed_text_shaders("${CMAKE_CURRENT_BINARY_DIR}/glim_webgpu_shaders.h" webgpu_shaders
    "${CMAKE_SOURCE_DIR}/packages/glim-gpu-webgpu/shaders" wgsl)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/vendor/inter/Inter-Latin.ttf")

function(glim_add_renderer name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_library(${name} STATIC src/Renderer.cpp ${ARG_SOURCES})
    target_include_directories(${name} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(${name} PUBLIC glim::paint ${ARG_LIBS})
    glim_target_defaults(${name})
endfunction()

function(glim_gpu_shaders target)
    if(GLIM_GPU STREQUAL "vulkan")
        target_compile_definitions(${target} PRIVATE GLIM_GPU_VULKAN=1)
        target_include_directories(${target} PRIVATE "${GLIM_VULKAN_SHADER_INCLUDE}")
        add_dependencies(${target} glim-vulkan-shaders)
    elseif(GLIM_GPU STREQUAL "webgpu")
        target_compile_definitions(${target} PRIVATE GLIM_GPU_WEBGPU=1)
    elseif(GLIM_GPU STREQUAL "metal")
        target_include_directories(${target} PRIVATE "${GLIM_METAL_SHADER_INCLUDE}")
        add_dependencies(${target} glim-metal-shaders)
    endif()
endfunction()

glim_add_renderer(glim-paint-cpu)
target_compile_definitions(glim-paint-cpu PUBLIC GLIM_SOFTWARE=1 GLIM_EMBED=0)
add_library(glim::paint-cpu ALIAS glim-paint-cpu)

glim_add_renderer(glim-paint-embed-cpu)
target_compile_definitions(glim-paint-embed-cpu PUBLIC GLIM_SOFTWARE=1 GLIM_EMBED=1)
add_library(glim::paint-embed-cpu ALIAS glim-paint-embed-cpu)

if(NOT GLIM_GPU STREQUAL "none")
    set(_glim_gpu_common src/gpu/Pipelines.cpp src/gpu/Batch.cpp src/gpu/Isolates.cpp)

    glim_add_renderer(glim-paint-gpu SOURCES ${_glim_gpu_common} src/gpu/SceneEncode.cpp LIBS glim::gpu)
    target_compile_definitions(glim-paint-gpu PUBLIC GLIM_SOFTWARE=0 GLIM_EMBED=0)
    glim_gpu_shaders(glim-paint-gpu)
    add_library(glim::paint-gpu ALIAS glim-paint-gpu)

    glim_add_renderer(glim-paint-embed-gpu SOURCES ${_glim_gpu_common} src/gpu/PacketSubmit.cpp LIBS glim::gpu)
    target_compile_definitions(glim-paint-embed-gpu PUBLIC GLIM_SOFTWARE=0 GLIM_EMBED=1)
    glim_gpu_shaders(glim-paint-embed-gpu)
    add_library(glim::paint-embed-gpu ALIAS glim-paint-embed-gpu)
endif()

if(GLIM_SOFTWARE)
    if(GLIM_EMBED)
        set(_glim_renderer glim::paint-embed-cpu)
    else()
        set(_glim_renderer glim::paint-cpu)
    endif()
elseif(GLIM_EMBED)
    set(_glim_renderer glim::paint-embed-gpu)
else()
    set(_glim_renderer glim::paint-gpu)
endif()

if(GLIM_HOST_TESTS)
    add_executable(glim-merge-test tests/merge_test.cpp)
    target_link_libraries(glim-merge-test PRIVATE glim::paint)
    glim_test_defaults(glim-merge-test)
    add_test(NAME glim-merge-test COMMAND glim-merge-test)

    add_executable(glim-strip-test tests/strip_test.cpp)
    target_link_libraries(glim-strip-test PRIVATE glim::paint)
    target_include_directories(glim-strip-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    glim_test_defaults(glim-strip-test)
    add_test(NAME glim-strip-test COMMAND glim-strip-test)

    add_executable(glim-encode-test tests/encode_test.cpp)
    target_link_libraries(glim-encode-test PRIVATE glim::paint)
    glim_test_defaults(glim-encode-test)
    add_test(NAME glim-encode-test COMMAND glim-encode-test)

    add_executable(glim-cpu-test tests/cpu_test.cpp)
    target_link_libraries(glim-cpu-test PRIVATE glim::paint-cpu)
    glim_test_defaults(glim-cpu-test)
    add_test(NAME glim-cpu-test COMMAND glim-cpu-test)

    add_executable(glim-glyph-test tests/glyph_test.cpp)
    target_link_libraries(glim-glyph-test PRIVATE glim::paint-cpu)
    glim_test_defaults(glim-glyph-test)
    add_test(NAME glim-glyph-test COMMAND glim-glyph-test)

    add_executable(glim-slot-test tests/slot_test.cpp)
    target_link_libraries(glim-slot-test PRIVATE glim::paint-cpu)
    glim_test_defaults(glim-slot-test)
    add_test(NAME glim-slot-test COMMAND glim-slot-test)

    add_executable(glim-context-test tests/context_test.cpp)
    target_link_libraries(glim-context-test PRIVATE glim::paint)
    glim_test_defaults(glim-context-test)
    add_test(NAME glim-context-test COMMAND glim-context-test)

    add_executable(glim-foreign-test tests/foreign_test.cpp)
    target_link_libraries(glim-foreign-test PRIVATE glim::paint-cpu)
    glim_test_defaults(glim-foreign-test)
    add_test(NAME glim-foreign-test COMMAND glim-foreign-test)

    add_executable(glim-glass-test tests/glass_test.cpp)
    target_link_libraries(glim-glass-test PRIVATE glim::paint-cpu)
    target_include_directories(glim-glass-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    glim_test_defaults(glim-glass-test)
    add_test(NAME glim-glass-test COMMAND glim-glass-test)

    add_executable(glim-lensmath-test tests/lensmath_test.cpp)
    target_link_libraries(glim-lensmath-test PRIVATE glim::paint-cpu)
    target_include_directories(glim-lensmath-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    glim_test_defaults(glim-lensmath-test)
    add_test(NAME glim-lensmath-test COMMAND glim-lensmath-test)

    add_executable(glim-hit-test tests/hit_test.cpp)
    target_link_libraries(glim-hit-test PRIVATE glim::paint)
    glim_test_defaults(glim-hit-test)
    add_test(NAME glim-hit-test COMMAND glim-hit-test)

    add_executable(glim-perf-test tests/perf_test.cpp)
    target_link_libraries(glim-perf-test PRIVATE glim::paint-cpu)
    glim_test_defaults(glim-perf-test)
    add_test(NAME glim-perf-test COMMAND glim-perf-test)

    add_executable(glim-reuse-test tests/reuse_test.cpp)
    target_link_libraries(glim-reuse-test PRIVATE glim::paint)
    glim_test_defaults(glim-reuse-test)
    add_test(NAME glim-reuse-test COMMAND glim-reuse-test)

    add_executable(glim-gradient-test tests/gradient_test.cpp)
    target_link_libraries(glim-gradient-test PRIVATE glim::paint-cpu)
    glim_test_defaults(glim-gradient-test)
    add_test(NAME glim-gradient-test COMMAND glim-gradient-test)

    add_executable(glim-embed-cpu-test tests/embed_cpu_test.cpp)
    target_link_libraries(glim-embed-cpu-test PRIVATE glim::paint-embed-cpu)
    glim_test_defaults(glim-embed-cpu-test)
    add_test(NAME glim-embed-cpu-test COMMAND glim-embed-cpu-test)

    if(EMSCRIPTEN AND GLIM_SOFTWARE AND GLIM_EMBED)
        add_executable(glim-wasm-paint-smoke tests/wasm_paint_smoke.cpp)
        target_link_libraries(glim-wasm-paint-smoke PRIVATE glim::paint-embed-cpu)
        glim_test_defaults(glim-wasm-paint-smoke)
        target_link_options(glim-wasm-paint-smoke PRIVATE "-sWASM=1" "-sENVIRONMENT=node")
    endif()

    add_custom_target(glim-paint-tests DEPENDS glim-merge-test glim-encode-test glim-cpu-test
                      glim-glyph-test glim-slot-test glim-foreign-test
                      glim-glass-test glim-hit-test glim-perf-test glim-reuse-test glim-gradient-test
                      glim-embed-cpu-test glim-lensmath-test)
endif()
