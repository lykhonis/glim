option(GLIM_GL "Build Glim with OpenGL" ON)

set(SOURCE_FILES)
set(LIBRARIES glm::glm)
set(DEFINITIONS)

if (${CMAKE_BUILD_TYPE} STREQUAL Debug)
    list(APPEND DEFINITIONS GLIM_DEBUG=1)
endif ()

if (GLIM_GL)
    list(APPEND SOURCE_FILES
            src/paint/gl/Context.cpp
            src/paint/gl/Shader.cpp
            src/paint/gl/Buffer.cpp
            src/paint/gl/VertexArray.cpp)
    list(APPEND DEFINITIONS GLIM_GL=1)
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
    list(APPEND SOURCE_FILES
            src/shell/macos/Application.mm
            src/shell/macos/Window.mm
            src/shell/macos/RunLoop.cpp
            src/shell/macos/Surface.mm)

    list(APPEND DEFINITIONS GLIM_MACOS=1)

    find_library(OPEN_GL OpenGL REQUIRED)
    find_library(APP_KIT AppKit REQUIRED)
    list(APPEND LIBRARIES ${OPEN_GL} ${APP_KIT})
endif ()

add_library(glim STATIC ${SOURCE_FILES})
add_library(glim::glim ALIAS glim)

target_include_directories(glim PUBLIC include)
target_link_libraries(glim ${LIBRARIES})
target_compile_definitions(glim PUBLIC ${DEFINITIONS})
