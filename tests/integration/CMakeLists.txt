add_subdirectory(platform)

if(PONDER_BUILD_RENDER_INTEGRATION_TESTS)
    add_subdirectory(render)

    if(WIN32 AND PONDER_BUILD_UI_RENDER_INTEGRATION)
        add_subdirectory(ui)
    endif()
endif()
