glim_add_example(showcase TITLE "Glim Dashboard" SOFTWARE NO_RUN SOFTWARE_LIBS glim::toolkit-host-cpu)
target_link_libraries(glim-showcase PRIVATE glim::toolkit-host)

if(GLIM_HOST_TESTS)
    add_executable(glim-showcase-golden tests/showcase_test.cpp src/main.cpp)
    target_include_directories(glim-showcase-golden PRIVATE src)
    target_link_libraries(glim-showcase-golden PRIVATE glim::toolkit-host glim::image)
    glim_target_defaults(glim-showcase-golden)
    target_compile_definitions(glim-showcase-golden PRIVATE GLIM_GOLDEN=1)
    add_test(NAME glim-showcase-golden
             COMMAND glim-showcase-golden)
endif()
