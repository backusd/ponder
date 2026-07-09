ponder_add_project_test(ponder_platform_tests
    SOURCES
        DisplayTests.cpp
        GeometryTests.cpp
        IdentifierTests.cpp
        KeyboardTests.cpp
        PlatformErrorTests.cpp
        PlatformEventTests.cpp
        PlatformRuntimeTypesTests.cpp
        TextInputTests.cpp
        TimingTests.cpp
        WindowGraphicsTests.cpp
        WindowStateTests.cpp
        WindowTypesTests.cpp
    PRIVATE_DEPS
        ponder::platform)

add_library(ponder_platform_header_tests OBJECT
    HeaderSelfContainment/DisplayHeader.cpp
    HeaderSelfContainment/GeometryHeader.cpp
    HeaderSelfContainment/IdentifiersHeader.cpp
    HeaderSelfContainment/KeyboardHeader.cpp
    HeaderSelfContainment/PlatformErrorHeader.cpp
    HeaderSelfContainment/PlatformEventHeader.cpp
    HeaderSelfContainment/PlatformRuntimeHeader.cpp
    HeaderSelfContainment/TimingHeader.cpp
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

ponder_add_project_test(ponder_platform_backend_tests
    SOURCES
        PlatformRuntimeTests.cpp
        RuntimeChildRegistryTests.cpp
        SdlEventTranslatorTests.cpp
        SdlErrorTests.cpp
        WindowBackendTests.cpp
    PRIVATE_DEPS
        ponder::platform
        ponder::SDL3)

target_include_directories(ponder_platform_backend_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/libs/platform/src")

# PLAT-008 extends this backend target with production event-translator tests.
