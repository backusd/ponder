ponder_add_project_test(ponder_render_tests
    SOURCES
        BootstrapValueTests.cpp
        RenderApiTraitsTests.cpp
        RenderErrorTests.cpp
        ThreadingHandoffTests.cpp
    PRIVATE_DEPS
        ponder::render
    LABELS
        render
        unit)


ponder_add_project_test(ponder_render_backend_tests
    SOURCES
        VulkanBootstrapTests.cpp
    PRIVATE_DEPS
        ponder::render
        ponder::volk
    LABELS
        render
        backend)

target_include_directories(ponder_render_backend_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/libs/render/src")
add_library(ponder_render_header_tests OBJECT
    HeaderSelfContainment/BootstrapHeader.cpp
    HeaderSelfContainment/RenderErrorHeader.cpp)

target_compile_features(ponder_render_header_tests PRIVATE cxx_std_23)
target_link_libraries(ponder_render_header_tests PRIVATE ponder::render)

set_target_properties(ponder_render_header_tests PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON
    FOLDER "tests")

ponder_enable_warnings(ponder_render_header_tests)
ponder_enable_sanitizers(ponder_render_header_tests)