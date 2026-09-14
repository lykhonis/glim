function(glim_target_defaults target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "glim_target_defaults: ${target} is not a target")
    endif()
    target_compile_features(${target} PUBLIC cxx_std_17)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_compile_definitions(${target} PUBLIC
        $<$<CONFIG:Debug>:GLIM_DEBUG=1>)
endfunction()

if(APPLE)
    set(_glim_gpu_default "metal")
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(_glim_shell_default "ios")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "tvOS")
        set(_glim_shell_default "tvos")
    else()
        set(_glim_shell_default "macos")
    endif()
elseif(ANDROID)
    set(_glim_gpu_default "vulkan")
    set(_glim_shell_default "android")
elseif(WIN32)
    set(_glim_gpu_default "vulkan")
    set(_glim_shell_default "windows")
else()
    set(_glim_gpu_default "vulkan")
    set(_glim_shell_default "wayland")
endif()
set(GLIM_GPU "${_glim_gpu_default}" CACHE STRING "GPU backend: metal|vulkan|webgpu")
set(GLIM_SHELL "${_glim_shell_default}" CACHE STRING "Shell: macos|ios|tvos|wayland|android|windows|web|custom")

if(GLIM_GPU STREQUAL "metal" AND NOT APPLE)
    message(FATAL_ERROR "GLIM_GPU=metal requires Apple")
endif()
if(GLIM_GPU STREQUAL "vulkan" AND APPLE)
    message(FATAL_ERROR "GLIM_GPU=vulkan is not used on Apple (native Metal only)")
endif()
option(GLIM_SOFTWARE "CPU raster in Renderer (no Device)" OFF)
option(GLIM_EMBED "Packet submit API instead of draw(Scene)" OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "tvOS")
    set(GLIM_APPLE_MOBILE ON)
    # Ninja/iOS try_compile executables cannot codesign; build static libs instead.
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
else()
    set(GLIM_APPLE_MOBILE OFF)
endif()

# Host ctest binaries (math, merge, golden). iOS/tvOS/Android only build the app.
if(GLIM_APPLE_MOBILE OR ANDROID)
    set(GLIM_HOST_TESTS OFF)
else()
    set(GLIM_HOST_TESTS ON)
endif()

function(glim_apple_app_icon target)
    if(NOT APPLE)
        return()
    endif()
    set(_icon "${CMAKE_SOURCE_DIR}/assets/glim-icon.png")
    if(NOT EXISTS "${_icon}")
        message(WARNING "glim_apple_app_icon: ${_icon} is missing")
        return()
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "tvOS")
        if(CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
            set(_platform appletvsimulator)
        else()
            set(_platform appletvos)
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        if(CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
            set(_platform iphonesimulator)
        else()
            set(_platform iphoneos)
        endif()
    else()
        return()
    endif()
    set(_min "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    if(NOT _min)
        set(_min 16.0)
    endif()
    set(_out "${CMAKE_CURRENT_BINARY_DIR}/${target}-appicon")
    set(_script "${CMAKE_SOURCE_DIR}/tools/scripts/compile-app-icon.sh")
    add_custom_command(
        OUTPUT "${_out}/.stamp"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_out}"
        COMMAND /bin/bash "${_script}" "${_icon}" "${_platform}" "${_min}" "${_out}"
        COMMAND ${CMAKE_COMMAND} -E touch "${_out}/.stamp"
        DEPENDS "${_icon}" "${_script}"
        COMMENT "Compiling AppIcon for ${target} (${_platform})"
        VERBATIM)
    add_custom_target(${target}-appicon DEPENDS "${_out}/.stamp")
    add_dependencies(${target} ${target}-appicon)
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${_out}/.stamp")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${_out}" "$<TARGET_BUNDLE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E remove -f
            "$<TARGET_BUNDLE_DIR:${target}>/.stamp"
            "$<TARGET_BUNDLE_DIR:${target}>/icon-partial.plist"
        COMMENT "Copying AppIcon into ${target}.app")
endfunction()

function(glim_apple_mobile_bundle target)
    if(NOT GLIM_APPLE_MOBILE)
        return()
    endif()
    cmake_parse_arguments(ARG "" "BUNDLE_ID;NAME" "" ${ARGN})
    if(NOT ARG_BUNDLE_ID)
        set(ARG_BUNDLE_ID "com.glim.hello")
    endif()
    if(NOT ARG_NAME)
        set(ARG_NAME "Glim Hello")
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "tvOS")
        set(_plist "${CMAKE_SOURCE_DIR}/examples/common/apple/Info.tvos.plist.in")
        set(_family "3")
    else()
        set(_plist "${CMAKE_SOURCE_DIR}/examples/common/apple/Info.ios.plist.in")
        set(_family "1,2")
    endif()
    set_target_properties(${target} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "${ARG_BUNDLE_ID}"
        MACOSX_BUNDLE_BUNDLE_NAME "${ARG_NAME}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_INFO_PLIST "${_plist}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${ARG_BUNDLE_ID}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "${_family}"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
    glim_apple_app_icon(${target})
endfunction()

function(glim_add_example name)
    cmake_parse_arguments(ARG "SOFTWARE" "TITLE" "SOURCES" ${ARGN})
    if(NOT ARG_TITLE)
        set(ARG_TITLE "Glim ${name}")
    endif()
    if(NOT ARG_SOURCES)
        set(ARG_SOURCES src/main.cpp)
    endif()
    set(_target "glim-${name}")
    set(_run "${CMAKE_SOURCE_DIR}/examples/common/run.cpp")
    if(ANDROID)
        add_library(${_target} SHARED ${_run} ${ARG_SOURCES})
        target_link_libraries(${_target} PRIVATE glim::glim glim-example-run android log)
        glim_target_defaults(${_target})
        set_target_properties(${_target} PROPERTIES OUTPUT_NAME ${_target})
        install(TARGETS ${_target} LIBRARY DESTINATION lib/${ANDROID_ABI})
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
        set(_manifest "${CMAKE_CURRENT_BINARY_DIR}/AndroidManifest.xml")
        set(GLIM_EXAMPLE_PACKAGE "com.glim.${name}")
        set(GLIM_EXAMPLE_LABEL "${ARG_TITLE}")
        set(GLIM_EXAMPLE_LIB "${_target}")
        configure_file("${CMAKE_SOURCE_DIR}/examples/common/AndroidManifest.xml.in" "${_manifest}" @ONLY)
        if(_glim_sdk AND EXISTS "${_glim_apk_script}")
            set(_glim_apk "${CMAKE_CURRENT_BINARY_DIR}/${_target}.apk")
            add_custom_command(TARGET ${_target} POST_BUILD
                COMMAND /bin/bash "${_glim_apk_script}"
                    "$<TARGET_FILE:${_target}>"
                    "${_manifest}"
                    "${CMAKE_SOURCE_DIR}/assets/glim-icon.png"
                    "${_glim_apk}"
                    "${_glim_sdk}"
                    "${ANDROID_ABI}"
                COMMENT "Packaging ${_target}.apk"
                VERBATIM)
        endif()
    else()
        add_executable(${_target} ${_run} ${ARG_SOURCES})
        target_link_libraries(${_target} PRIVATE glim::glim glim-example-run)
        glim_target_defaults(${_target})
        if(GLIM_APPLE_MOBILE)
            glim_apple_mobile_bundle(${_target} BUNDLE_ID "com.glim.${name}" NAME "${ARG_TITLE}")
            install(TARGETS ${_target} BUNDLE DESTINATION .)
        else()
            install(TARGETS ${_target} RUNTIME DESTINATION bin)
        endif()
    endif()
    target_include_directories(${_target} PRIVATE
        "${CMAKE_SOURCE_DIR}/examples/common"
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(${_target} PRIVATE GLIM_EXAMPLE_TITLE="${ARG_TITLE}")

    if(ARG_SOFTWARE AND GLIM_HOST_TESTS AND NOT GLIM_SOFTWARE)
        set(_soft "glim-${name}-software")
        add_executable(${_soft} ${_run} ${ARG_SOURCES})
        target_include_directories(${_soft} PRIVATE
            "${CMAKE_SOURCE_DIR}/examples/common"
            "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_link_libraries(${_soft} PRIVATE glim::core glim::paint glim::paint-cpu)
        if(TARGET glim::shell)
            target_link_libraries(${_soft} PRIVATE glim::shell)
        endif()
        glim_target_defaults(${_soft})
        target_compile_definitions(${_soft} PRIVATE GLIM_EXAMPLE_TITLE="${ARG_TITLE}")
    endif()
endfunction()
