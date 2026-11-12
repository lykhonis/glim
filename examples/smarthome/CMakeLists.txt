glim_add_example(smarthome TITLE "Glim Home" SOFTWARE)
target_link_libraries(glim-smarthome PRIVATE glim::image)

if(GLIM_HOST_TESTS)
    add_executable(glim-smarthome-golden tests/smarthome_test.cpp src/main.cpp)
    target_include_directories(glim-smarthome-golden PRIVATE src)
    target_link_libraries(glim-smarthome-golden PRIVATE glim::toolkit-host glim::image)
    glim_test_defaults(glim-smarthome-golden)
    target_compile_definitions(glim-smarthome-golden PRIVATE GLIM_GOLDEN=1)
    add_test(NAME glim-smarthome-golden
             COMMAND glim-smarthome-golden)
    # Holds a wall-clock record gate (< 4 ms); parallel tests skew it.
    set_tests_properties(glim-smarthome-golden PROPERTIES RUN_SERIAL TRUE)
    add_test(NAME glim-smarthome-png-golden
             COMMAND glim-smarthome-golden --golden "${CMAKE_CURRENT_SOURCE_DIR}/golden/smarthome.png")
    # The app's own headless flags are the perf gate: soak fails on op
    # growth, e2e fails when a `~TEXT` check is missing.
    add_dependencies(glim-smarthome-golden glim-smarthome)
    add_test(NAME glim-smarthome-soak COMMAND glim-smarthome --soak 20 --stats)
    # Interaction scenarios, one ctest each: <name> <e2e steps, "|"-separated>.
    # Focus order: 0 Wi-Fi, 1 thermostat, 2 ruler, 3 ceiling, 4 dial, 5 Live, 6 Light.
    set(_scenarios
        launch      "~Glim Home|~Living Room|~All devices online"
        live-off    "tap:i:5|step:0.5|~Updates paused"
        live-toggle "tap:i:5|step:0.5|tap:i:5|step:0.5|~All devices online"
        wifi-off    "tap:i:0|step:0.5|~Disconnected"
        thermostat  "tap:i:1|step:0.5|~Thermostat off"
        keyboard    "key:Next|key:Activate|step:0.5|~Disconnected")
    list(LENGTH _scenarios _n)
    math(EXPR _last "${_n} - 1")
    foreach(_i RANGE 0 ${_last} 2)
        math(EXPR _j "${_i} + 1")
        list(GET _scenarios ${_i} _name)
        list(GET _scenarios ${_j} _steps)
        string(REPLACE "|" ";" _steps "${_steps}")
        add_test(NAME glim-smarthome-e2e-${_name} COMMAND glim-smarthome --e2e "${_steps}")
    endforeach()
endif()
