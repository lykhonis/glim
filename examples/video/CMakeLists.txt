set(_gen "${CMAKE_CURRENT_BINARY_DIR}/gen")
if(APPLE)
    set(_source src/VideoSourceMetal.mm)
    set_source_files_properties(${_source} PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    # Metal compiles the shared scene at runtime from this string.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS src/clip.scene)
    file(READ src/clip.scene GLIM_CLIP_SCENE)
    file(CONFIGURE OUTPUT "${_gen}/ClipScene.h" CONTENT
        "#pragma once\nnamespace glim::toolkit {\ninline constexpr const char* kClipScene = R\"glim(@GLIM_CLIP_SCENE@)glim\";\n}\n"
        @ONLY)
elseif(ANDROID)
    set(_source src/VideoSourceVulkan.cpp "${_gen}/clip.vert.inc" "${_gen}/clip.frag.inc")
    # minSdk 24; AHardwareBuffer is API 26, guarded at runtime.
    set_source_files_properties(src/VideoSourceVulkan.cpp PROPERTIES
        COMPILE_DEFINITIONS __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__)
    file(GLOB _glslc "${ANDROID_NDK}/shader-tools/*/glslc")
    if(NOT _glslc)
        message(FATAL_ERROR "glslc not found under ${ANDROID_NDK}/shader-tools")
    endif()
    foreach(_stage vert frag)
        add_custom_command(OUTPUT "${_gen}/clip.${_stage}.inc"
            COMMAND ${_glslc} -I "${CMAKE_CURRENT_SOURCE_DIR}/src" -mfmt=num
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders/clip.${_stage}" -o "${_gen}/clip.${_stage}.inc"
            DEPENDS shaders/clip.${_stage} src/clip.scene
            VERBATIM)
    endforeach()
else()
    set(_source src/VideoSourceNone.cpp)
endif()
glim_add_example(video TITLE "Glim Video" SOURCES src/main.cpp ${_source})
target_include_directories(glim-video PRIVATE "${_gen}")
if(APPLE)
    target_link_libraries(glim-video PRIVATE "-framework Metal" "-framework IOSurface")
elseif(ANDROID)
    target_link_libraries(glim-video PRIVATE android dl)
endif()

if(GLIM_HOST_TESTS AND APPLE AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # N3 gate: GPU video frames under glass, tap to pause.
    add_executable(glim-video-test tests/video_test.mm ${_source})
    set_source_files_properties(tests/video_test.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_include_directories(glim-video-test PRIVATE src "${_gen}")
    target_link_libraries(glim-video-test PRIVATE glim::toolkit-host glim::image
        "-framework Metal" "-framework IOSurface")
    glim_test_defaults(glim-video-test)
    add_test(NAME glim-video-test COMMAND glim-video-test)

    # N6 gate: the Share button presents the system sheet and returns its result.
    add_executable(glim-share-test tests/share_test.mm)
    set_source_files_properties(tests/share_test.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_include_directories(glim-share-test PRIVATE src)
    target_link_libraries(glim-share-test PRIVATE glim::toolkit-host "-framework AppKit")
    glim_test_defaults(glim-share-test)
    add_test(NAME glim-share-test COMMAND glim-share-test)

    # N5 gate: the clip's own a11y subtree reaches AppKit under the plate.
    add_executable(glim-video-a11y-test tests/a11y_test.mm)
    set_source_files_properties(tests/a11y_test.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_include_directories(glim-video-a11y-test PRIVATE src)
    target_link_libraries(glim-video-a11y-test PRIVATE glim::toolkit-host "-framework AppKit")
    glim_test_defaults(glim-video-a11y-test)
    add_test(NAME glim-video-a11y-test COMMAND glim-video-a11y-test)
endif()
