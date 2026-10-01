glim_add_example(glass TITLE "Glim Glass" SOFTWARE)

if(GLIM_HOST_TESTS)
    add_executable(glim-glass-golden tests/golden_test.cpp src/main.cpp)
    target_include_directories(glim-glass-golden PRIVATE src)
    target_link_libraries(glim-glass-golden PRIVATE glim::paint glim::image)
    glim_target_defaults(glim-glass-golden)
    target_compile_definitions(glim-glass-golden PRIVATE GLIM_GOLDEN=1)
    add_test(NAME glim-glass-golden
             COMMAND glim-glass-golden "${CMAKE_CURRENT_SOURCE_DIR}/golden/glass.png")
    if(GLIM_GPU STREQUAL "vulkan")
        add_executable(glim-glass-vulkan-golden tests/vulkan_golden.cpp src/main.cpp)
        target_include_directories(glim-glass-vulkan-golden PRIVATE src)
        target_link_libraries(glim-glass-vulkan-golden PRIVATE glim::glim glim::image)
        glim_target_defaults(glim-glass-vulkan-golden)
        target_compile_definitions(glim-glass-vulkan-golden PRIVATE GLIM_GOLDEN=1)
        add_test(NAME glim-glass-vulkan-golden
                 COMMAND glim-glass-vulkan-golden "${CMAKE_CURRENT_SOURCE_DIR}/golden/glass.png")
    endif()
endif()

