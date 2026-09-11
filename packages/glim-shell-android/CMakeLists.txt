set(_glim_ndk "${CMAKE_ANDROID_NDK}")
if(NOT _glim_ndk)
    set(_glim_ndk "$ENV{ANDROID_NDK_HOME}")
endif()
set(_glim_glue "${_glim_ndk}/sources/android/native_app_glue")
if(NOT EXISTS "${_glim_glue}/android_native_app_glue.c")
    message(FATAL_ERROR "android_native_app_glue not found in ${_glim_glue}")
endif()

add_library(glim-android-glue STATIC "${_glim_glue}/android_native_app_glue.c")
target_include_directories(glim-android-glue PUBLIC "${_glim_glue}")
target_link_libraries(glim-android-glue PUBLIC android log)
set_source_files_properties("${_glim_glue}/android_native_app_glue.c" PROPERTIES COMPILE_FLAGS "-w")

add_library(glim-shell STATIC
    src/Application.cpp
    src/Window.cpp
    src/Surface.cpp
    src/RunLoop.cpp
    src/WindowRegistry.cpp
    src/App.cpp)
add_library(glim::shell ALIAS glim-shell)
target_include_directories(glim-shell PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(glim-shell PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(glim-shell PUBLIC glim::core glim::gpu glim::shell-api glim-android-glue)
target_link_options(glim-shell PUBLIC
    "LINKER:-u,ANativeActivity_onCreate"
    "LINKER:-u,android_main"
    "LINKER:-u,glim_app_main")
glim_target_defaults(glim-shell)
