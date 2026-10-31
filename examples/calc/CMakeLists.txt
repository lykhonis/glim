add_executable(glim-calc src/main.cpp)
target_link_libraries(glim-calc PRIVATE glim::toolkit glim::paint-cpu glim::image)
target_include_directories(glim-calc PRIVATE src)
glim_target_defaults(glim-calc)

if(GLIM_HOST_TESTS)
    add_executable(glim-calc-golden tests/dump_test.cpp src/main.cpp)
    target_include_directories(glim-calc-golden PRIVATE src)
    target_link_libraries(glim-calc-golden PRIVATE glim::toolkit glim::paint)
    glim_target_defaults(glim-calc-golden)
    target_compile_definitions(glim-calc-golden PRIVATE GLIM_GOLDEN=1)
    add_test(NAME glim-calc-golden
             COMMAND glim-calc-golden "${CMAKE_CURRENT_SOURCE_DIR}/golden")
endif()
