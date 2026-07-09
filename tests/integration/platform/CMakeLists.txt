ponder_add_project_test(ponder_platform_integration_tests
    SOURCES
        PlatformRuntimeIntegrationTests.cpp
    PRIVATE_DEPS
        ponder::platform
        ponder::SDL3
    RESOURCE_LOCK
        ponder_platform_sdl)
