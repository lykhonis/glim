add_library(glim-toolkit STATIC
    src/Toolkit.cpp
    src/Elements.cpp
    src/Geometry.cpp
    src/text/Text.cpp
    src/Flow.cpp
    src/Hit.cpp
    src/Events.cpp
    src/Focus.cpp
    src/Screen.cpp
)
add_library(glim::toolkit ALIAS glim-toolkit)
target_include_directories(glim-toolkit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
glim_target_defaults(glim-toolkit)

if(GLIM_HOST_TESTS)
    add_executable(glim-toolkit-test tests/toolkit_test.cpp)
    target_link_libraries(glim-toolkit-test PRIVATE glim::toolkit)
    glim_target_defaults(glim-toolkit-test)
    add_test(NAME glim-toolkit-test COMMAND glim-toolkit-test)
endif()
