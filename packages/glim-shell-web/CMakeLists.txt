add_library(glim-shell STATIC
    src/Application.cpp
    src/Window.cpp
    src/Surface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../glim-shell/src/ClipboardUnsupported.cpp
    src/RunLoop.cpp
    src/Input.cpp
    src/TextInput.cpp)
add_library(glim::shell ALIAS glim-shell)
target_include_directories(glim-shell PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(glim-shell PUBLIC glim::core glim::gpu glim::shell-api)
glim_target_defaults(glim-shell)

set_source_files_properties(src/Window.cpp PROPERTIES COMPILE_DEFINITIONS GLIM_SOFTWARE=1)

# The text bridge's browser half.
if(EMSCRIPTEN)
    set(_glim_text_lib "${CMAKE_CURRENT_SOURCE_DIR}/src/text_input.lib.js")
    target_link_options(glim-shell PUBLIC
        "$<BUILD_INTERFACE:--js-library=${_glim_text_lib}>"
        "$<INSTALL_INTERFACE:--js-library=$<INSTALL_PREFIX>/share/glim/text_input.lib.js>")
    install(FILES "${_glim_text_lib}" DESTINATION share/glim COMPONENT glim)
    set_property(TARGET glim-shell APPEND PROPERTY INTERFACE_LINK_DEPENDS
        "$<BUILD_INTERFACE:${_glim_text_lib}>")
endif()
