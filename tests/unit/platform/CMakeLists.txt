ponder_add_project_test(ponder_platform_tests
    SOURCES
        DialogTests.cpp
        DisplayTests.cpp
        FormattingTests.cpp
        GeometryTests.cpp
        IdentifierTests.cpp
        KeyboardTests.cpp
        MouseTests.cpp
        NativeWindowTests.cpp
        PlatformErrorTests.cpp
        PlatformCoverageAuditTests.cpp
        PlatformEventTests.cpp
        RuntimeTypesTests.cpp
        ProcessTests.cpp
        TextInputTests.cpp
        WindowGraphicsTests.cpp
        WindowStateTests.cpp
        WindowTypesTests.cpp
    PRIVATE_DEPS
        ponder::platform
    LABELS
        platform
        unit)

target_compile_definitions(ponder_platform_tests PRIVATE
    PONDER_PLATFORM_PUBLIC_INCLUDE_DIR="${PROJECT_SOURCE_DIR}/libs/platform/include/ponder/platform"
    PONDER_PLATFORM_HEADER_SELF_CONTAINMENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/HeaderSelfContainment")

add_library(ponder_platform_header_tests OBJECT
    HeaderSelfContainment/DialogsHeader.cpp
    HeaderSelfContainment/DisplayHeader.cpp
    HeaderSelfContainment/GeometryHeader.cpp
    HeaderSelfContainment/HintsHeader.cpp
    HeaderSelfContainment/IdentifiersHeader.cpp
    HeaderSelfContainment/KeyboardHeader.cpp
    HeaderSelfContainment/MouseHeader.cpp
    HeaderSelfContainment/NativeWindowHeader.cpp
    HeaderSelfContainment/PlatformErrorHeader.cpp
    HeaderSelfContainment/PlatformEventHeader.cpp
    HeaderSelfContainment/RuntimeHeader.cpp
    HeaderSelfContainment/ProcessHeader.cpp
    HeaderSelfContainment/TextInputHeader.cpp
    HeaderSelfContainment/WindowGraphicsHeader.cpp
    HeaderSelfContainment/WindowHeader.cpp
    HeaderSelfContainment/WindowStateHeader.cpp)

target_compile_features(ponder_platform_header_tests PRIVATE cxx_std_23)
target_link_libraries(ponder_platform_header_tests PRIVATE ponder::platform)

set_target_properties(ponder_platform_header_tests PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON
    FOLDER "tests")

ponder_enable_warnings(ponder_platform_header_tests)
ponder_enable_sanitizers(ponder_platform_header_tests)

ponder_add_project_executable(ponder_platform_process_helper
    NO_INSTALL
    SOURCES
        ProcessTestHelper.cpp
    FOLDER
        "tests/helpers")

set_target_properties(ponder_platform_process_helper PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON)

ponder_add_project_test(ponder_platform_backend_tests
    SOURCES
        HintsTests.cpp
        ProcessBackendTests.cpp
        RuntimeChildRegistryTests.cpp
        SdlEventTranslatorTests.cpp
        SdlErrorTests.cpp
        WindowBackendTests.cpp
    PRIVATE_DEPS
        ponder::platform
        ponder::SDL3
    LABELS
        platform
        backend)

target_include_directories(ponder_platform_backend_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/libs/platform/src")

add_dependencies(ponder_platform_backend_tests ponder_platform_process_helper)
target_compile_definitions(ponder_platform_backend_tests PRIVATE
    PONDER_PLATFORM_PROCESS_HELPER_PATH="$<TARGET_FILE:ponder_platform_process_helper>")

# Compile the runtime behavioral tests against a test-only concrete backend variant. This target
# intentionally does not link ponder::platform because it defines the same public platform symbols
# with a different private runtime backend selection.
ponder_add_project_test(ponder_platform_runtime_mock_tests
    SOURCES
        MockRuntime.cpp
        RuntimeTests.cpp
        "${PROJECT_SOURCE_DIR}/libs/platform/src/Display.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/Mouse.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/NativeWindow.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/PlatformCommon.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/Runtime.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/RuntimeChildRegistry.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/Window.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/WindowProperties.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/WindowRegistry.cpp"
        "${PROJECT_SOURCE_DIR}/libs/platform/src/WindowTextInput.cpp"
    PRIVATE_DEPS
        ponder::core
    LABELS
        platform
        backend)

target_include_directories(ponder_platform_runtime_mock_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/libs/platform/include"
    "${PROJECT_SOURCE_DIR}/libs/platform/src"
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(ponder_platform_runtime_mock_tests PRIVATE
    PONDER_PLATFORM_USE_MOCK_RUNTIME)

set_target_properties(ponder_platform_runtime_mock_tests PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON)
