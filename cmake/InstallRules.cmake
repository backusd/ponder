include_guard(GLOBAL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

function(ponder_install_target target_name)
    if(NOT PONDER_ENABLE_INSTALL_RULES)
        return()
    endif()

    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Cannot install missing target: ${target_name}")
    endif()

    install(TARGETS "${target_name}"
        EXPORT ponderTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endfunction()

function(ponder_install_export)
    if(NOT PONDER_ENABLE_INSTALL_RULES)
        return()
    endif()

    install(EXPORT ponderTargets
        NAMESPACE ponder::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ponder")
endfunction()

function(ponder_install_package_config)
    if(NOT PONDER_ENABLE_INSTALL_RULES)
        return()
    endif()

    set(config_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/ponder")
    set(config_input "${PROJECT_SOURCE_DIR}/cmake/ponderConfig.cmake.in")
    set(config_output "${PROJECT_BINARY_DIR}/cmake/ponderConfig.cmake")
    set(version_output "${PROJECT_BINARY_DIR}/cmake/ponderConfigVersion.cmake")

    configure_package_config_file(
        "${config_input}"
        "${config_output}"
        INSTALL_DESTINATION "${config_install_dir}")

    write_basic_package_version_file(
        "${version_output}"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion)

    install(FILES
        "${config_output}"
        "${version_output}"
        DESTINATION "${config_install_dir}")
endfunction()
