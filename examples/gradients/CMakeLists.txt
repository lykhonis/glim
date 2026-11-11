glim_add_example(gradients TITLE "Glim Gradients" SOFTWARE)

if(GLIM_HOST_TESTS)
    add_executable(glim-gradients-golden tests/golden_test.cpp src/main.cpp)
    target_include_directories(glim-gradients-golden PRIVATE src)
    target_link_libraries(glim-gradients-golden PRIVATE glim::paint glim::image)
    glim_target_defaults(glim-gradients-golden)
    target_compile_definitions(glim-gradients-golden PRIVATE GLIM_GOLDEN=1)
    add_test(NAME glim-gradients-golden
             COMMAND glim-gradients-golden "${CMAKE_CURRENT_SOURCE_DIR}/golden/gradients.png")
    if(GLIM_GPU STREQUAL "vulkan")
        glim_add_gpu_golden(gradients
            GOLDEN "${CMAKE_CURRENT_SOURCE_DIR}/golden/gradients.png"
            SOURCES tests/gpu_golden.cpp src/main.cpp)
    endif()
endif()
