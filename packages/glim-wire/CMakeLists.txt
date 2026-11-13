add_library(glim-wire STATIC
    src/Record.cpp
    src/Validate.cpp
    src/Writer.cpp)
add_library(glim::wire ALIAS glim-wire)
target_include_directories(glim-wire PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
glim_target_defaults(glim-wire)

if(GLIM_HOST_TESTS)
    add_executable(glim-wire-test tests/wire_test.cpp)
    target_link_libraries(glim-wire-test PRIVATE glim::wire)
    glim_test_defaults(glim-wire-test)
    add_test(NAME glim-wire-test COMMAND glim-wire-test)
endif()

if(GLIM_FUZZ)
    glim_fuzz_target(glim-wire-fuzz tests/wire_fuzz.cpp src/Record.cpp src/Validate.cpp src/Writer.cpp)
    target_include_directories(glim-wire-fuzz PRIVATE include)
endif()
