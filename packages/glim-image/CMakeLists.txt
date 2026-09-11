add_library(glim-image STATIC
    src/Image.cpp
    src/Png.cpp)
add_library(glim::image ALIAS glim-image)
target_include_directories(glim-image PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(glim-image PRIVATE z)
glim_target_defaults(glim-image)

if(GLIM_HOST_TESTS)
    add_executable(glim-image-test tests/image_test.cpp)
    target_link_libraries(glim-image-test PRIVATE glim::image)
    glim_target_defaults(glim-image-test)
    add_test(NAME glim-image-test COMMAND glim-image-test)
endif()
