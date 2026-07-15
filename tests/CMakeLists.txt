add_subdirectory(unit)
add_subdirectory(integration)

if(PONDER_BUILD_RENDER AND PONDER_ENABLE_INSTALL_RULES)
    add_subdirectory(install)
endif()
