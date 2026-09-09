add_executable(glim-hello src/main.cpp)
target_link_libraries(glim-hello PRIVATE glim::glim)
glim_target_defaults(glim-hello)

install(TARGETS glim-hello RUNTIME DESTINATION bin)
