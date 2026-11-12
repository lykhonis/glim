add_library(glim-shell STATIC
    src/Application.mm
    src/Window.mm
    src/Surface.mm
    src/RunLoop.cpp
    src/WindowRegistry.cpp)
add_library(glim::shell ALIAS glim-shell)
target_include_directories(glim-shell PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(glim-shell PUBLIC glim::core glim::gpu glim::shell-api)
glim_target_defaults(glim-shell)

set_source_files_properties(
    src/Application.mm
    src/Window.mm
    src/Surface.mm
    PROPERTIES COMPILE_FLAGS "-fobjc-arc")

target_link_libraries(glim-shell PUBLIC
    "-framework UIKit" "-framework Foundation" "-framework Metal" "-framework QuartzCore"
    "-framework CoreGraphics" "-framework CoreFoundation")
# ObjC classes (GlimAppDelegate) live in this static lib; pull them into the app.
target_link_options(glim-shell PUBLIC "LINKER:-ObjC")
