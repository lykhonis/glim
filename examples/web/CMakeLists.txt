if(GLIM_SHELL STREQUAL "web" OR GLIM_GPU STREQUAL "webgpu")
    add_executable(glim-web
        src/run_web.cpp
        src/select.cpp
        "${CMAKE_SOURCE_DIR}/examples/hello/src/main.cpp"
        "${CMAKE_SOURCE_DIR}/examples/glass/src/main.cpp"
        "${CMAKE_SOURCE_DIR}/examples/gradients/src/main.cpp"
        "${CMAKE_SOURCE_DIR}/examples/foreign/src/main.cpp")
    target_include_directories(glim-web PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/examples/common")
    target_link_libraries(glim-web PRIVATE glim::glim)
    glim_target_defaults(glim-web)
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/examples/hello/src/main.cpp"
        "${CMAKE_SOURCE_DIR}/examples/glass/src/main.cpp"
        "${CMAKE_SOURCE_DIR}/examples/gradients/src/main.cpp"
        PROPERTIES COMPILE_DEFINITIONS GLIM_GOLDEN=1)
    set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/examples/foreign/src/main.cpp"
        PROPERTIES COMPILE_DEFINITIONS recordExample=recordForeignExample)
    if(EMSCRIPTEN)
        target_link_options(glim-web PRIVATE
            "-sWASM=1"
            "-sMODULARIZE=1"
            "-sEXPORT_ES6=1"
            "-sEXPORT_NAME=createGlimModule"
            "-sEXPORTED_FUNCTIONS=['_main','_glimWebSelect','_glimWebResize','_glimWebFrame','_glimWebEvent']"
            "-sEXPORTED_RUNTIME_METHODS=['ccall']")
        set_target_properties(glim-web PROPERTIES OUTPUT_NAME "glim-app" SUFFIX ".js")
    endif()
    set(_glim_web_serve "${CMAKE_CURRENT_BINARY_DIR}/serve")
    # Web assets refresh on every build; POST_BUILD alone only runs on relink.
    add_custom_target(glim-web-assets ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_glim_web_serve}/lib"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/glass.html"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/gradients.html"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/foreign.html"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/main.js"
            "${_glim_web_serve}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/packages/glim-web/js/glim.js"
            "${_glim_web_serve}/lib/glim.js"
        VERBATIM)
    add_dependencies(glim-web glim-web-assets)
    if(EMSCRIPTEN)
        add_custom_command(TARGET glim-web POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE_DIR:glim-web>/glim-app.js"
                "$<TARGET_FILE_DIR:glim-web>/glim-app.wasm"
                "${_glim_web_serve}/"
            VERBATIM)
    endif()
endif()
