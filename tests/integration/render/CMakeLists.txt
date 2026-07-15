ponder_add_project_test(ponder_render_integration_tests
    SOURCES
        LiveRenderTestSupport.hpp
        OwnerThreadSurfaceIntegrationTests.cpp
    PRIVATE_DEPS
        ponder::platform
        ponder::render
    RESOURCE_LOCK
        ponder_platform_sdl
    TIMEOUT
        180
    LABELS
        render
        integration
        live
        vulkan
        platform)

target_compile_definitions(ponder_render_integration_tests PRIVATE
    PONDER_RENDER_REQUIRE_LIVE_TESTS=$<BOOL:${PONDER_RENDER_REQUIRE_LIVE_TESTS}>
    PONDER_RENDER_LIVE_VALIDATION_MODE="${PONDER_RENDER_LIVE_VALIDATION_MODE}")
