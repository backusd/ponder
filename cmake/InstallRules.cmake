include_guard(GLOBAL)

include(GNUInstallDirs)

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
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endfunction()

function(ponder_install_public_headers include_dir)
    if(NOT PONDER_ENABLE_INSTALL_RULES)
        return()
    endif()

    if(NOT IS_ABSOLUTE "${include_dir}")
        set(include_dir "${PROJECT_SOURCE_DIR}/${include_dir}")
    endif()

    install(DIRECTORY "${include_dir}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endfunction()

function(ponder_install_export)
    if(NOT PONDER_ENABLE_INSTALL_RULES)
        return()
    endif()

    install(EXPORT ponderTargets
        NAMESPACE ponder::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ponder")
endfunction()
