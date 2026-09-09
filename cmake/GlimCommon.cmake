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

set(GLIM_GPU "metal" CACHE STRING "GPU backend: metal|vulkan|webgpu")
set(GLIM_SHELL "macos" CACHE STRING "Shell: macos|ios|tvos|wayland|android|windows|web|custom")
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

# Host ctest binaries (math, merge, golden). iOS/tvOS only build app bundles.
if(GLIM_APPLE_MOBILE)
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
    if(CMAKE_SYSTEM_NAME STREQUAL "tvOS")
        set(_plist "${CMAKE_SOURCE_DIR}/examples/hello/apple/Info.tvos.plist.in")
        set(_family "3")
    else()
        set(_plist "${CMAKE_SOURCE_DIR}/examples/hello/apple/Info.ios.plist.in")
        set(_family "1,2")
    endif()
    set_target_properties(${target} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.lykhonis.glim.hello"
        MACOSX_BUNDLE_BUNDLE_NAME "Glim"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_INFO_PLIST "${_plist}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.lykhonis.glim.hello"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "${_family}"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
    glim_apple_app_icon(${target})
endfunction()
