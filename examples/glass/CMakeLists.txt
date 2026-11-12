glim_add_example(glass TITLE "Glim Glass" SOFTWARE SOURCES src/main.cpp src/Glass.cpp)

if(GLIM_HOST_TESTS)
    add_executable(glim-glass-golden tests/golden_test.cpp src/Glass.cpp)
    target_include_directories(glim-glass-golden PRIVATE src)
    target_link_libraries(glim-glass-golden PRIVATE glim::paint glim::image)
    glim_test_defaults(glim-glass-golden)
    add_test(NAME glim-glass-golden
             COMMAND glim-glass-golden "${CMAKE_CURRENT_SOURCE_DIR}/golden/glass.png")
    glim_add_gpu_golden(glass
        GOLDEN "${CMAKE_CURRENT_SOURCE_DIR}/golden/glass.png"
        SOURCES tests/gpu_golden.cpp src/Glass.cpp)
endif()

