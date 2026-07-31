add_subdirectory(platform)
add_subdirectory(application)

if(PONDER_BUILD_RENDER)
    add_subdirectory(render)
endif()

if(PONDER_BUILD_UI_RENDER_INTEGRATION)
    add_subdirectory(ui)
endif()
