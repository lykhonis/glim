add_library(glim-shell STATIC
    src/Application.cpp
    src/Window.cpp
    src/Surface.cpp
    src/RunLoop.cpp)
add_library(glim::shell ALIAS glim-shell)
target_include_directories(glim-shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(glim-shell PUBLIC glim::core glim::gpu glim::shell-api)
glim_target_defaults(glim-shell)

set_source_files_properties(src/Window.cpp PROPERTIES COMPILE_DEFINITIONS GLIM_SOFTWARE=1)
