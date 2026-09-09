add_library(glim-shell-api INTERFACE)
add_library(glim::shell-api ALIAS glim-shell-api)
target_include_directories(glim-shell-api INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(glim-shell-api INTERFACE cxx_std_17)
