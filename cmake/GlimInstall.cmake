include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(GLIM_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/glim")

# Component name (glim::<name> for consumers) per build target. Only the
# targets this configuration built are installed; the GPU and shell
# backends are fixed at configure time.
set(_glim_components
    glim-core core
    glim-image image
    glim-gpu gpu
    glim-shell-api shell-api
    glim-gpu-metal gpu-metal
    glim-gpu-vulkan gpu-vulkan
    glim-gpu-webgpu gpu-webgpu
    glim-volk volk
    glim-paint paint
    glim-paint-cpu paint-cpu
    glim-paint-embed-cpu paint-embed-cpu
    glim-paint-gpu paint-gpu
    glim-paint-embed-gpu paint-embed-gpu
    glim-embed embed
    glim-toolkit toolkit
    glim-toolkit-host toolkit-host
    glim-toolkit-host-cpu toolkit-host-cpu
    glim-android-glue android-glue
    glim-shell shell
    glim glim)

add_custom_target(glim-package)
set(GLIM_INSTALLED_COMPONENTS)
set(_glim_header_dirs)
while(_glim_components)
    list(POP_FRONT _glim_components _target _name)
    if(NOT TARGET ${_target})
        continue()
    endif()
    set_target_properties(${_target} PROPERTIES EXPORT_NAME ${_name})
    install(TARGETS ${_target} EXPORT glimTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT glim
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    list(APPEND GLIM_INSTALLED_COMPONENTS ${_name})
    get_target_property(_type ${_target} TYPE)
    if(NOT _type STREQUAL "INTERFACE_LIBRARY")
        add_dependencies(glim-package ${_target})
    endif()
    get_target_property(_dir ${_target} SOURCE_DIR)
    if(IS_DIRECTORY "${_dir}/include")
        list(APPEND _glim_header_dirs "${_dir}/include")
    endif()
endwhile()
list(REMOVE_DUPLICATES _glim_header_dirs)
foreach(_dir ${_glim_header_dirs})
    install(DIRECTORY "${_dir}/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT glim)
endforeach()

install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT glim)

install(EXPORT glimTargets NAMESPACE glim:: DESTINATION ${GLIM_INSTALL_CMAKEDIR} COMPONENT glim)
configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/glimConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/glimConfig.cmake"
    INSTALL_DESTINATION ${GLIM_INSTALL_CMAKEDIR})
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/glimConfigVersion.cmake"
    COMPATIBILITY SameMinorVersion)
install(FILES
    "${CMAKE_BINARY_DIR}/glimConfig.cmake"
    "${CMAKE_BINARY_DIR}/glimConfigVersion.cmake"
    DESTINATION ${GLIM_INSTALL_CMAKEDIR} COMPONENT glim)
