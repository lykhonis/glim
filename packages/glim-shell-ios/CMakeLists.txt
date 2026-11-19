add_library(glim-shell STATIC
    src/Application.mm
    src/Window.mm
    src/Present.mm
    src/Clipboard.mm
    src/A11y.mm
    src/Surface.mm
    src/RunLoop.cpp
    src/WindowRegistry.cpp)
add_library(glim::shell ALIAS glim-shell)
target_include_directories(glim-shell PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(glim-shell PUBLIC glim::core glim::gpu glim::shell-api)
target_include_directories(glim-shell PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../glim-shell/src/apple)
glim_target_defaults(glim-shell)

set_source_files_properties(
    src/Application.mm
    src/Window.mm
    src/Present.mm
    src/Clipboard.mm
    src/A11y.mm
    src/Surface.mm
    PROPERTIES COMPILE_FLAGS "-fobjc-arc")

target_link_libraries(glim-shell PUBLIC
    "-framework UIKit" "-framework Foundation" "-framework Metal" "-framework QuartzCore"
    "-framework CoreGraphics" "-framework CoreFoundation")
# ObjC classes (GlimAppDelegate) live in this static lib; pull them into the app.
target_link_options(glim-shell PUBLIC "LINKER:-ObjC")

# Runs on the simulator, not under ctest: xcrun simctl spawn <device> <binary>.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(glim-ios-a11y-bridge-test tests/a11y_bridge_test.mm)
    set_source_files_properties(tests/a11y_bridge_test.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(glim-ios-a11y-bridge-test PRIVATE glim::shell)
    glim_test_defaults(glim-ios-a11y-bridge-test)
endif()
