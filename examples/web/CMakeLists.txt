if(GLIM_SHELL STREQUAL "web" OR GLIM_GPU STREQUAL "webgpu")
    add_executable(glim-web src/run_web.cpp src/main.cpp)
    target_include_directories(glim-web PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(glim-web PRIVATE glim::glim)
    glim_target_defaults(glim-web)
    if(EMSCRIPTEN)
        target_link_options(glim-web PRIVATE
            "-sUSE_WEBGPU=1"
            "-sWASM=1"
            "-sEXPORTED_FUNCTIONS=['_main','_glimWebResize','_glimWebFrame','_glimWebEvent']"
            "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']")
        set_target_properties(glim-web PROPERTIES OUTPUT_NAME "glim" SUFFIX ".js")
    endif()
    set(_glim_web_serve "${CMAKE_CURRENT_BINARY_DIR}/serve")
    add_custom_command(TARGET glim-web POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_glim_web_serve}/lib"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/main.js"
            "${_glim_web_serve}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/packages/glim-web/js/glim.js"
            "${_glim_web_serve}/lib/glim.js"
        VERBATIM)
    if(EMSCRIPTEN)
        add_custom_command(TARGET glim-web POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE_DIR:glim-web>/glim.js"
                "$<TARGET_FILE_DIR:glim-web>/glim.wasm"
                "${_glim_web_serve}/"
            VERBATIM)
    endif()
endif()
