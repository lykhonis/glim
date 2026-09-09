add_library(glim-shell STATIC
    src/Application.mm
    src/Window.mm
    src/Surface.mm
    src/RunLoop.cpp
    src/WindowRegistry.cpp)
add_library(glim::shell ALIAS glim-shell)
target_include_directories(glim-shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(glim-shell PUBLIC glim::core glim::gpu glim::shell-api)
glim_target_defaults(glim-shell)

set_source_files_properties(
    src/Application.mm
    src/Window.mm
    src/Surface.mm
    PROPERTIES COMPILE_FLAGS "-fobjc-arc")

find_library(GLIM_UIKIT UIKit REQUIRED)
find_library(GLIM_FOUNDATION Foundation REQUIRED)
find_library(GLIM_METAL Metal REQUIRED)
find_library(GLIM_QUARTZ QuartzCore REQUIRED)
find_library(GLIM_COREGRAPHICS CoreGraphics REQUIRED)
find_library(GLIM_COREFOUNDATION CoreFoundation REQUIRED)
target_link_libraries(glim-shell PUBLIC
    ${GLIM_UIKIT} ${GLIM_FOUNDATION} ${GLIM_METAL} ${GLIM_QUARTZ}
    ${GLIM_COREGRAPHICS} ${GLIM_COREFOUNDATION})
# ObjC classes (GlimAppDelegate) live in this static lib; pull them into the app.
target_link_options(glim-shell PUBLIC "LINKER:-ObjC")
