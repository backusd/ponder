ponder_add_project_test(ponder_platform_integration_tests
    SOURCES
        PlatformRuntimeIntegrationTests.cpp
    PRIVATE_DEPS
        ponder::platform
        ponder::SDL3
    RESOURCE_LOCK
        ponder_platform_sdl
    LABELS
        platform
        integration
        live)

add_dependencies(ponder_platform_integration_tests ponder_platform_process_helper)
target_compile_definitions(ponder_platform_integration_tests PRIVATE
    PONDER_PLATFORM_PROCESS_HELPER_PATH="$<TARGET_FILE:ponder_platform_process_helper>")
