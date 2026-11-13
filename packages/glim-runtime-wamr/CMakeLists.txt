add_subdirectory(vm)

add_library(glim-runtime-wamr STATIC src/Wamr.cpp)
add_library(glim::runtime-wamr ALIAS glim-runtime-wamr)
target_include_directories(glim-runtime-wamr PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(glim-runtime-wamr PUBLIC glim::guest-host PRIVATE glim-wamr-vm)
glim_target_defaults(glim-runtime-wamr)

if(GLIM_HOST_TESTS AND NOT GLIM_WASI_SDK)
    message(STATUS "glim-runtime-wamr: set GLIM_WASI_SDK to build the G13 smoke guest")
elseif(GLIM_HOST_TESTS)
    include(ExternalProject)
    set(_guest_dir "${CMAKE_CURRENT_BINARY_DIR}/guest")
    set(_guest_wasm "${_guest_dir}/packages/glim-runtime-wamr/tests/guest/glim-smoke-guest.wasm")
    ExternalProject_Add(glim-smoke-guest-build
        SOURCE_DIR "${CMAKE_SOURCE_DIR}"
        BINARY_DIR "${_guest_dir}"
        CMAKE_ARGS
            "-DCMAKE_TOOLCHAIN_FILE=${GLIM_WASI_SDK}/share/cmake/wasi-sdk-p1.cmake"
            "-DWASI_SDK_PREFIX=${GLIM_WASI_SDK}"
            -DCMAKE_BUILD_TYPE=Release
            -DGLIM_GUEST=ON
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target glim-smoke-guest
        INSTALL_COMMAND ""
        BUILD_ALWAYS ON
        BUILD_BYPRODUCTS "${_guest_wasm}")

    add_executable(glim-runtime-wamr-smoke tests/smoke_test.cpp tests/SmokeScene.cpp)
    target_link_libraries(glim-runtime-wamr-smoke PRIVATE glim::runtime-wamr glim::guest glim::paint-cpu)
    add_dependencies(glim-runtime-wamr-smoke glim-smoke-guest-build)
    glim_test_defaults(glim-runtime-wamr-smoke)
    add_test(NAME glim-runtime-wamr-smoke COMMAND glim-runtime-wamr-smoke "${_guest_wasm}")
endif()
