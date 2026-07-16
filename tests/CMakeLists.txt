add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(build)

if(PONDER_ENABLE_INSTALL_RULES)
    add_subdirectory(install)
endif()
