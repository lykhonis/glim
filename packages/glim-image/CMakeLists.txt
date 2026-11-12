add_library(glim-image STATIC
    src/Analyze.cpp
    src/Image.cpp
    src/Load.cpp
    src/Png.cpp)
add_library(glim::image ALIAS glim-image)
target_include_directories(glim-image PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_include_directories(glim-image PRIVATE ${CMAKE_SOURCE_DIR}/vendor/stb)
target_link_libraries(glim-image PRIVATE z)
# Vendored decoder: keep project warnings off third-party code.
set_source_files_properties(src/Load.cpp PROPERTIES COMPILE_OPTIONS "-w")
glim_target_defaults(glim-image)

if(GLIM_HOST_TESTS)
    add_executable(glim-image-test tests/image_test.cpp)
    target_link_libraries(glim-image-test PRIVATE glim::image)
    glim_test_defaults(glim-image-test)
    add_test(NAME glim-image-test COMMAND glim-image-test)
endif()
