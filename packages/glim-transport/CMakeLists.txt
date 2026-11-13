add_library(glim-transport STATIC
    src/Chunk.cpp
    src/DirectTransport.cpp
    src/Endpoint.cpp
    src/GuestAbi.cpp
    src/Handshake.cpp
    src/Inbox.cpp
    src/Outbox.cpp
    src/Requests.cpp
    src/Ring.cpp
    src/RingTransport.cpp)
add_library(glim::transport ALIAS glim-transport)
target_include_directories(glim-transport PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(glim-transport PUBLIC glim::wire)
glim_target_defaults(glim-transport)

if(GLIM_HOST_TESTS)
    add_executable(glim-transport-test tests/transport_test.cpp)
    target_link_libraries(glim-transport-test PRIVATE glim::transport)
    glim_test_defaults(glim-transport-test)
    add_test(NAME glim-transport-test COMMAND glim-transport-test)
endif()

if(GLIM_FUZZ)
    glim_fuzz_target(glim-transport-fuzz tests/transport_fuzz.cpp)
    target_link_libraries(glim-transport-fuzz PRIVATE glim::transport)
endif()
