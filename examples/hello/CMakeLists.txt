set(HELLO_SCENE src/HelloScene.cpp)

add_executable(glim-hello src/main.cpp ${HELLO_SCENE})
target_include_directories(glim-hello PRIVATE src)
target_link_libraries(glim-hello PRIVATE glim::glim)
glim_target_defaults(glim-hello)
install(TARGETS glim-hello RUNTIME DESTINATION bin)

add_executable(glim-hello-software src/main.cpp ${HELLO_SCENE})
target_include_directories(glim-hello-software PRIVATE src)
target_link_libraries(glim-hello-software PRIVATE glim::core glim::paint glim::paint-cpu)
if(TARGET glim::shell)
    target_link_libraries(glim-hello-software PRIVATE glim::shell)
endif()
glim_target_defaults(glim-hello-software)

add_executable(glim-hello-golden tests/golden_test.cpp ${HELLO_SCENE})
target_include_directories(glim-hello-golden PRIVATE src)
target_link_libraries(glim-hello-golden PRIVATE glim::paint-ref)
glim_target_defaults(glim-hello-golden)
add_test(NAME glim-hello-golden
         COMMAND glim-hello-golden "${CMAKE_CURRENT_SOURCE_DIR}/golden/hello.png")
