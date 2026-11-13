option(GLIM_WERROR "Treat warnings in Glim targets as errors" ON)
option(GLIM_COVERAGE "Instrument Glim targets for gcov line coverage" OFF)

include(CheckCXXCompilerFlag)
# Partial designated initializers are the props-form idiom (K53). Newer clang
# flags them under -Wextra (-Wmissing-designated-field-initializers); GCC has
# no narrower flag than -Wmissing-field-initializers. Only these are off.
check_cxx_compiler_flag(-Wmissing-designated-field-initializers GLIM_HAS_WMDFI)

function(glim_target_defaults target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "glim_target_defaults: ${target} is not a target")
    endif()
    target_compile_features(${target} PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(GLIM_HAS_WMDFI)
            target_compile_options(${target} PRIVATE -Wno-missing-designated-field-initializers)
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE -Wno-missing-field-initializers)
        endif()
    endif()
    if(GLIM_WERROR)
        target_compile_options(${target} PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/WX,-Werror>)
    endif()
    target_compile_definitions(${target} PUBLIC
        $<$<CONFIG:Debug>:GLIM_DEBUG=1>)
    if(GLIM_COVERAGE)
        target_compile_options(${target} PRIVATE --coverage -fprofile-update=atomic)
        target_link_options(${target} PUBLIC --coverage)
        target_compile_definitions(${target} PUBLIC GLIM_COVERAGE=1)
    endif()
endfunction()

# Test executables keep assert() live in every build type: under NDEBUG an
# assert-based test would compile to a no-op and pass while checking nothing.
function(glim_test_defaults target)
    glim_target_defaults(${target})
    target_compile_options(${target} PRIVATE -UNDEBUG)
endfunction()

if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
    set(GLIM_WASM_GUEST ON)
    set(_glim_gpu_default "none")
    set(_glim_shell_default "none")
elseif(EMSCRIPTEN)
    set(_glim_gpu_default "webgpu")
    set(_glim_shell_default "web")
    add_compile_options(--use-port=emdawnwebgpu)
    add_link_options(--use-port=emdawnwebgpu)
    add_compile_options(-sUSE_ZLIB=1)
    add_link_options(-sUSE_ZLIB=1)
elseif(APPLE)
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
if(GLIM_GPU STREQUAL "vulkan" AND APPLE AND NOT EMSCRIPTEN)
    message(FATAL_ERROR "GLIM_GPU=vulkan is not used on Apple (native Metal only)")
endif()
option(GLIM_SOFTWARE "CPU raster in Renderer (no Device)" OFF)
option(GLIM_EMBED "Packet submit API instead of draw(Scene)" OFF)
option(GLIM_GUEST "Wasm guest embedding layers (glim-wire and up, K76)" OFF)
option(GLIM_FUZZ "libFuzzer targets for guest-boundary validators (needs GLIM_GUEST, Clang)" OFF)
set(GLIM_WASI_SDK "" CACHE PATH "wasi-sdk root; builds the G13 smoke guest (needs GLIM_GUEST)")
if(GLIM_WASM_GUEST AND NOT GLIM_GUEST)
    message(FATAL_ERROR "a WASI build is a guest build; set GLIM_GUEST=ON")
endif()
if(GLIM_FUZZ AND NOT GLIM_GUEST)
    message(FATAL_ERROR "GLIM_FUZZ requires GLIM_GUEST=ON")
endif()
if(GLIM_FUZZ AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "GLIM_FUZZ needs Clang (libFuzzer)")
endif()

# libFuzzer target; with GLIM_FUZZ the whole tree builds with ASan/UBSan so
# linked Glim libraries are instrumented too.
if(GLIM_FUZZ)
    add_compile_options(-fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
endif()
function(glim_fuzz_target target)
    add_executable(${target} ${ARGN})
    target_link_options(${target} PRIVATE -fsanitize=fuzzer)
    glim_test_defaults(${target})
endfunction()

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
if(GLIM_APPLE_MOBILE OR ANDROID OR GLIM_WASM_GUEST)
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
    set(_assets "${CMAKE_SOURCE_DIR}/assets/apple")
    if(_platform MATCHES "^appletv")
        # Letterboxed 1280x768 / 2560x1536 icons are committed assets.
        set(_icon_commands
            COMMAND ${CMAKE_COMMAND} -E copy
                "${_assets}/tvos/AppIcon.png" "${_assets}/tvos/AppIcon@2x.png" "${_out}")
        set(_icon_depends "${_assets}/tvos/AppIcon.png" "${_assets}/tvos/AppIcon@2x.png")
    else()
        set(_catalog "${CMAKE_CURRENT_BINARY_DIR}/${target}-AppIcon.xcassets")
        set(_icon_commands
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${_assets}/AppIcon.xcassets" "${_catalog}"
            COMMAND ${CMAKE_COMMAND} -E copy "${_icon}" "${_catalog}/AppIcon.appiconset/glim-icon.png"
            COMMAND xcrun actool
                --output-format human-readable-text --notices --warnings
                --platform "${_platform}" --minimum-deployment-target "${_min}"
                --app-icon AppIcon --compile "${_out}"
                --output-partial-info-plist "${_out}/icon-partial.plist"
                "${_catalog}")
        set(_icon_depends
            "${_icon}"
            "${_assets}/AppIcon.xcassets/Contents.json"
            "${_assets}/AppIcon.xcassets/AppIcon.appiconset/Contents.json")
    endif()
    add_custom_command(
        OUTPUT "${_out}/.stamp"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${_out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_out}"
        ${_icon_commands}
        COMMAND ${CMAKE_COMMAND} -E touch "${_out}/.stamp"
        DEPENDS ${_icon_depends}
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
    if(NOT ARG_BUNDLE_ID OR NOT ARG_NAME)
        message(FATAL_ERROR "glim_apple_mobile_bundle: ${target} needs BUNDLE_ID and NAME")
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
    if(ANDROID)
        add_library(${_target} SHARED ${ARG_SOURCES})
        target_link_libraries(${_target} PRIVATE glim::toolkit-host android log)
        glim_target_defaults(${_target})
        set_target_properties(${_target} PROPERTIES OUTPUT_NAME ${_target})
        install(TARGETS ${_target} LIBRARY DESTINATION lib/${ANDROID_ABI} COMPONENT ${_target})
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
        set(_glim_apk_script "${CMAKE_SOURCE_DIR}/tools/package-android-apk.mjs")
        find_program(GLIM_NODE node)
        set(_manifest "${CMAKE_CURRENT_BINARY_DIR}/AndroidManifest.xml")
        set(GLIM_EXAMPLE_PACKAGE "com.glim.${name}")
        set(GLIM_EXAMPLE_LABEL "${ARG_TITLE}")
        set(GLIM_EXAMPLE_LIB "${_target}")
        configure_file("${CMAKE_SOURCE_DIR}/examples/common/AndroidManifest.xml.in" "${_manifest}" @ONLY)
        if(DEFINED ENV{CI} AND NOT (_glim_sdk AND GLIM_NODE))
            message(FATAL_ERROR "APK packaging needs an Android SDK and node in CI")
        endif()
        if(_glim_sdk AND GLIM_NODE)
            set(_glim_apk "${CMAKE_CURRENT_BINARY_DIR}/${_target}.apk")
            add_custom_command(TARGET ${_target} POST_BUILD
                COMMAND "${GLIM_NODE}" "${_glim_apk_script}"
                    "$<TARGET_FILE:${_target}>"
                    "${_manifest}"
                    "${CMAKE_SOURCE_DIR}/assets/android/res"
                    "${_glim_apk}"
                    "${_glim_sdk}"
                    "${ANDROID_ABI}"
                COMMENT "Packaging ${_target}.apk"
                VERBATIM)
        endif()
    else()
        add_executable(${_target} ${ARG_SOURCES})
        target_link_libraries(${_target} PRIVATE glim::toolkit-host)
        glim_target_defaults(${_target})
        if(GLIM_APPLE_MOBILE)
            glim_apple_mobile_bundle(${_target} BUNDLE_ID "com.glim.${name}" NAME "${ARG_TITLE}")
            install(TARGETS ${_target} BUNDLE DESTINATION . COMPONENT ${_target})
        else()
            install(TARGETS ${_target} RUNTIME DESTINATION bin COMPONENT ${_target})
        endif()
    endif()
    target_include_directories(${_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

    if(ARG_SOFTWARE AND GLIM_HOST_TESTS AND NOT GLIM_SOFTWARE)
        set(_soft "glim-${name}-software")
        add_executable(${_soft} ${ARG_SOURCES})
        target_include_directories(${_soft} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_link_libraries(${_soft} PRIVATE glim::toolkit-host-cpu)
        glim_target_defaults(${_soft})
    endif()
endfunction()
