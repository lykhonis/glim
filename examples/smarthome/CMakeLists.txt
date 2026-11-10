glim_add_example(smarthome TITLE "Glim Home" SOFTWARE NO_RUN SOFTWARE_LIBS glim::toolkit-host-cpu)
target_link_libraries(glim-smarthome PRIVATE glim::toolkit-host glim::image)

if(GLIM_HOST_TESTS)
    add_executable(glim-smarthome-golden tests/smarthome_test.cpp src/main.cpp)
    target_include_directories(glim-smarthome-golden PRIVATE src)
    target_link_libraries(glim-smarthome-golden PRIVATE glim::toolkit-host glim::image)
    glim_target_defaults(glim-smarthome-golden)
    target_compile_definitions(glim-smarthome-golden PRIVATE GLIM_GOLDEN=1)
    add_test(NAME glim-smarthome-golden
             COMMAND glim-smarthome-golden)
    # The app's own headless flags are the perf gate: soak fails on op
    # growth, e2e fails when a `~TEXT` check is missing.
    add_dependencies(glim-smarthome-golden glim-smarthome)
    add_test(NAME glim-smarthome-soak COMMAND glim-smarthome --soak 20 --stats)
    add_test(NAME glim-smarthome-e2e
             COMMAND glim-smarthome --e2e "~Glim Home;~Living Room;tap:i:5;step:0.5;~Updates paused")
endif()
