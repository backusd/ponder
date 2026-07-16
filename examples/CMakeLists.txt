add_subdirectory(platform)

if(PONDER_BUILD_RENDER)
    add_subdirectory(render)
endif()

if(PONDER_BUILD_UI_RENDER_INTEGRATION)
    add_subdirectory(ui)
endif()
