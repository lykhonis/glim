find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
if(NOT WAYLAND_PROTOCOLS_DIR)
    message(FATAL_ERROR "wayland-protocols pkgdatadir not found")
endif()

set(_glim_wl_gen "${CMAKE_CURRENT_BINARY_DIR}/protocol")
file(MAKE_DIRECTORY "${_glim_wl_gen}")

function(glim_wayland_protocol xml)
    get_filename_component(_name "${xml}" NAME_WE)
    set(_h "${_glim_wl_gen}/${_name}-client-protocol.h")
    set(_c "${_glim_wl_gen}/${_name}-protocol.c")
    add_custom_command(
        OUTPUT "${_h}" "${_c}"
        COMMAND "${WAYLAND_SCANNER}" client-header "${xml}" "${_h}"
        COMMAND "${WAYLAND_SCANNER}" private-code "${xml}" "${_c}"
        DEPENDS "${xml}"
        VERBATIM)
    set(GLIM_WL_PROTOCOL_HEADERS ${GLIM_WL_PROTOCOL_HEADERS} "${_h}" PARENT_SCOPE)
    set(GLIM_WL_PROTOCOL_SOURCES ${GLIM_WL_PROTOCOL_SOURCES} "${_c}" PARENT_SCOPE)
endfunction()

glim_wayland_protocol("${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
glim_wayland_protocol("${WAYLAND_PROTOCOLS_DIR}/stable/viewporter/viewporter.xml")

set(_fractional "${WAYLAND_PROTOCOLS_DIR}/staging/fractional-scale/fractional-scale-v1.xml")
if(EXISTS "${_fractional}")
    glim_wayland_protocol("${_fractional}")
    set(GLIM_HAS_FRACTIONAL_SCALE ON)
endif()

add_library(glim-shell STATIC
    src/Application.cpp
    src/Window.cpp
    src/Surface.cpp
    src/RunLoop.cpp
    src/WindowRegistry.cpp
    src/Display.cpp
    ${GLIM_WL_PROTOCOL_SOURCES}
    ${GLIM_WL_PROTOCOL_HEADERS})
add_library(glim::shell ALIAS glim-shell)
target_include_directories(glim-shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(glim-shell PRIVATE ${_glim_wl_gen} ${WAYLAND_CLIENT_INCLUDE_DIRS})
target_link_libraries(glim-shell PUBLIC glim::core glim::gpu glim::shell-api)
target_link_libraries(glim-shell PUBLIC ${WAYLAND_CLIENT_LIBRARIES})
if(GLIM_HAS_FRACTIONAL_SCALE)
    target_compile_definitions(glim-shell PRIVATE GLIM_HAS_FRACTIONAL_SCALE=1)
endif()
set_source_files_properties(src/Window.cpp PROPERTIES COMPILE_DEFINITIONS GLIM_SOFTWARE=1)
set_source_files_properties(${GLIM_WL_PROTOCOL_SOURCES} PROPERTIES COMPILE_FLAGS "-w")
glim_target_defaults(glim-shell)
