add_library(glim-toolkit STATIC
    src/Toolkit.cpp
    src/A11y.cpp
    src/Elements.cpp
    src/Geometry.cpp
    src/text/Text.cpp
    src/Flow.cpp
    src/Hit.cpp
    src/Events.cpp
    src/Focus.cpp
    src/Screen.cpp
    src/TextEdit.cpp
    src/text/Segment.cpp
    src/Slider.cpp
    src/TextField.cpp
    src/EditMenu.cpp
)
add_library(glim::toolkit ALIAS glim-toolkit)
target_include_directories(glim-toolkit PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
# Plane maps (hit testing under perspective) share glim-core's Mat3.
target_link_libraries(glim-toolkit PUBLIC glim::core)
glim_target_defaults(glim-toolkit)

if(GLIM_HOST_TESTS)
    add_executable(glim-toolkit-test
        tests/test_main.cpp
        tests/test_basics.cpp
        tests/test_layout.cpp
        tests/test_scroll.cpp
        tests/test_hit.cpp
        tests/test_motion.cpp
        tests/test_pump.cpp
        tests/test_chrome.cpp
        tests/test_state.cpp
        tests/test_text.cpp
        tests/test_tags.cpp
        tests/test_slider.cpp
        tests/test_scoped.cpp
        tests/test_theme.cpp
        tests/test_text_edit.cpp
        tests/test_edit_menu.cpp
        tests/test_text_commands.cpp
    )
    target_link_libraries(glim-toolkit-test PRIVATE glim::toolkit)
    glim_test_defaults(glim-toolkit-test)
    add_test(NAME glim-toolkit-test COMMAND glim-toolkit-test)

    add_executable(glim-text-perf-test tests/text_perf_test.cpp)
    target_link_libraries(glim-text-perf-test PRIVATE glim::toolkit)
    glim_test_defaults(glim-text-perf-test)
    add_test(NAME glim-text-perf-test COMMAND glim-text-perf-test)
endif()
