set(SOURCE_FILES src/main.cpp)

add_executable(glim-hello ${SOURCE_FILES})

target_link_libraries(glim-hello glim::glim)
