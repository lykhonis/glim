glim_add_example(hello TITLE "Glim Hello" SOFTWARE)

if(GLIM_HOST_TESTS)
    add_executable(glim-hello-golden tests/golden_test.cpp src/main.cpp)
    target_include_directories(glim-hello-golden PRIVATE src)
    target_link_libraries(glim-hello-golden PRIVATE glim::paint glim::image)
    glim_target_defaults(glim-hello-golden)
    target_compile_definitions(glim-hello-golden PRIVATE GLIM_GOLDEN=1)
    add_test(NAME glim-hello-golden
             COMMAND glim-hello-golden "${CMAKE_CURRENT_SOURCE_DIR}/golden/hello.png")
endif()
