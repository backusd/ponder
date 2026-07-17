ponder_add_project_test(ponder_ui_live_integration_tests
    SOURCES
        ../LiveRenderTestSupport.hpp
        UiRectangleLiveIntegrationTests.cpp
    PRIVATE_DEPS
        ponder::platform
        ponder::render
        ponder::ui_experimental
    RESOURCE_LOCK
        ponder_platform_sdl
    TIMEOUT
        300
    LABELS
        ui
        render
        integration
        live
        vulkan
        platform
        rectangle
        ui_live_vulkan)

target_include_directories(ponder_ui_live_integration_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/libs/render/src")

target_compile_definitions(ponder_ui_live_integration_tests PRIVATE
    PONDER_RENDER_REQUIRE_LIVE_TESTS=$<BOOL:${PONDER_RENDER_REQUIRE_LIVE_TESTS}>
    PONDER_RENDER_LIVE_VALIDATION_MODE="${PONDER_RENDER_LIVE_VALIDATION_MODE}")
