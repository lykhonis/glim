glim_add_example(infotainment TITLE "Glim Infotainment" SOFTWARE NO_RUN SOFTWARE_LIBS glim::toolkit-host-cpu)
target_link_libraries(glim-infotainment PRIVATE glim::toolkit-host glim::image)

if(GLIM_HOST_TESTS)
    add_executable(glim-infotainment-golden tests/infotainment_test.cpp src/main.cpp)
    target_include_directories(glim-infotainment-golden PRIVATE src)
    target_link_libraries(glim-infotainment-golden PRIVATE glim::toolkit-host glim::image)
    glim_test_defaults(glim-infotainment-golden)
    target_compile_definitions(glim-infotainment-golden PRIVATE GLIM_GOLDEN=1)
    add_test(NAME glim-infotainment-golden
             COMMAND glim-infotainment-golden)
endif()
