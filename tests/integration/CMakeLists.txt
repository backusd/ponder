add_subdirectory(platform)

if(PONDER_BUILD_RENDER_INTEGRATION_TESTS)
    add_subdirectory(render)
endif()
