set(HELLO_SCENE src/HelloScene.cpp)

if(ANDROID)
    add_library(glim-hello SHARED src/main.cpp ${HELLO_SCENE})
    target_include_directories(glim-hello PRIVATE src)
    target_link_libraries(glim-hello PRIVATE glim::glim android log)
    glim_target_defaults(glim-hello)
    set_target_properties(glim-hello PROPERTIES OUTPUT_NAME glim-hello)
    install(TARGETS glim-hello LIBRARY DESTINATION lib/${ANDROID_ABI})

    set(_glim_sdk "")
    if(DEFINED ENV{ANDROID_HOME} AND EXISTS "$ENV{ANDROID_HOME}")
        set(_glim_sdk "$ENV{ANDROID_HOME}")
    elseif(DEFINED ENV{ANDROID_SDK_ROOT} AND EXISTS "$ENV{ANDROID_SDK_ROOT}")
        set(_glim_sdk "$ENV{ANDROID_SDK_ROOT}")
    elseif(EXISTS "$ENV{HOME}/Library/Android/sdk")
        set(_glim_sdk "$ENV{HOME}/Library/Android/sdk")
    elseif(EXISTS "$ENV{HOME}/Android/Sdk")
        set(_glim_sdk "$ENV{HOME}/Android/Sdk")
    endif()
    set(_glim_apk_script "${CMAKE_SOURCE_DIR}/tools/scripts/package-android-apk.sh")
    if(_glim_sdk AND EXISTS "${_glim_apk_script}")
        set(_glim_apk "${CMAKE_CURRENT_BINARY_DIR}/glim-hello.apk")
        add_custom_command(TARGET glim-hello POST_BUILD
            COMMAND /bin/bash "${_glim_apk_script}"
                "$<TARGET_FILE:glim-hello>"
                "${CMAKE_SOURCE_DIR}/examples/hello/android/AndroidManifest.xml"
                "${CMAKE_SOURCE_DIR}/assets/glim-icon.png"
                "${_glim_apk}"
                "${_glim_sdk}"
                "${ANDROID_ABI}"
            COMMENT "Packaging glim-hello.apk"
            VERBATIM)
    endif()
else()
    add_executable(glim-hello src/main.cpp ${HELLO_SCENE})
    target_include_directories(glim-hello PRIVATE src)
    target_link_libraries(glim-hello PRIVATE glim::glim)
    glim_target_defaults(glim-hello)
    if(GLIM_APPLE_MOBILE)
        glim_apple_mobile_bundle(glim-hello)
        install(TARGETS glim-hello BUNDLE DESTINATION .)
    else()
        install(TARGETS glim-hello RUNTIME DESTINATION bin)
    endif()
endif()

if(GLIM_HOST_TESTS)
    add_executable(glim-hello-software src/main.cpp ${HELLO_SCENE})
    target_include_directories(glim-hello-software PRIVATE src)
    target_link_libraries(glim-hello-software PRIVATE glim::core glim::paint glim::paint-cpu)
    if(TARGET glim::shell)
        target_link_libraries(glim-hello-software PRIVATE glim::shell)
    endif()
    glim_target_defaults(glim-hello-software)

    add_executable(glim-hello-golden tests/golden_test.cpp ${HELLO_SCENE})
    target_include_directories(glim-hello-golden PRIVATE src)
    target_link_libraries(glim-hello-golden PRIVATE glim::paint glim::image)
    glim_target_defaults(glim-hello-golden)
    add_test(NAME glim-hello-golden
             COMMAND glim-hello-golden "${CMAKE_CURRENT_SOURCE_DIR}/golden/hello.png")
endif()
