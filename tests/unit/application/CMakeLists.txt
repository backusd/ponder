# Compile the Application behavioral tests against the platform's test-only
# concrete mock runtime. This target intentionally links neither production
# ponder::application nor ponder::platform because both public symbol sets are
# compiled here with PONDER_PLATFORM_USE_MOCK_RUNTIME.
ponder_add_project_test(ponder_application_tests
    SOURCES
        ApplicationTests.cpp
        "${PROJECT_SOURCE_DIR}/libs/application/src/Application.cpp"
        "${PROJECT_SOURCE_DIR}/tests/unit/platform/MockRuntime.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/Display.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/Mouse.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/NativeWindow.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/PlatformCommon.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/Process.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/Runtime.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/RuntimeChildRegistry.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/SdlError.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/Window.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/WindowProperties.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/WindowRegistry.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/WindowTextInput.cpp"
    PRIVATE_DEPS
        ponder::core
        ponder::io
        ponder::SDL3
    LABELS
        application
        unit)

target_include_directories(ponder_application_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/libs/application/include"
    "${PROJECT_SOURCE_DIR}/libs/platform/include"
    "${PROJECT_SOURCE_DIR}/libs/platform/src"
    "${PROJECT_SOURCE_DIR}/tests/unit/platform")

target_compile_definitions(ponder_application_tests PRIVATE
    PONDER_PLATFORM_USE_MOCK_RUNTIME
    PONDER_PLATFORM_PROCESS_HELPER_PATH="$<TARGET_FILE:ponder_platform_process_helper>")

add_dependencies(ponder_application_tests ponder_platform_process_helper)

set_target_properties(ponder_application_tests PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON)

add_library(ponder_application_header_tests OBJECT
    HeaderSelfContainment/ApplicationHeader.cpp
    HeaderSelfContainment/ApplicationErrorHeader.cpp
    HeaderSelfContainment/IdentifiersHeader.cpp)

target_compile_features(ponder_application_header_tests PRIVATE cxx_std_23)
target_link_libraries(ponder_application_header_tests PRIVATE ponder::application)

set_target_properties(ponder_application_header_tests PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON
    FOLDER "tests")

ponder_enable_warnings(ponder_application_header_tests)
ponder_enable_sanitizers(ponder_application_header_tests)
