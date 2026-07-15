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
        unit
        public
        value
        deterministic)

target_include_directories(ponder_render_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/libs/render/src")

ponder_add_project_test(ponder_render_backend_tests
    SOURCES
        VulkanQueueSynchronizationTests.cpp
        VulkanBootstrapTests.cpp
    PRIVATE_DEPS
        ponder::render
        ponder::volk
    LABELS
        render
        backend
        fake-dispatch
        lifecycle
        rollback
        deterministic)

ponder_add_project_test(ponder_render_lifetime_contract_tests
    SOURCES
        LifetimeContractDeathTests.cpp
    PRIVATE_DEPS
        ponder::render
        ponder::volk
    TIMEOUT
        60
    LABELS
        render
        unit
        lifetime
        death
        deterministic)

target_include_directories(ponder_render_lifetime_contract_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/libs/render/src")

target_include_directories(ponder_render_backend_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/libs/render/src")

function(ponder_add_render_build_test test_name target_name)
    set(test_command "${CMAKE_COMMAND}" --build "${PROJECT_BINARY_DIR}" --target "${target_name}")
    if(CMAKE_CONFIGURATION_TYPES)
        list(APPEND test_command --config "$<CONFIG>")
    endif()

    add_test(NAME "${test_name}" COMMAND ${test_command})
    set_tests_properties("${test_name}" PROPERTIES
        LABELS "render;build;public;deterministic"
        RESOURCE_LOCK ponder_render_nested_build
        TIMEOUT 120)
endfunction()

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

ponder_add_render_build_test(
    ponder_render_header_tests_build
    ponder_render_header_tests)
set_tests_properties(ponder_render_header_tests_build PROPERTIES
    LABELS "render;headers;public;no-pch;deterministic"
    TIMEOUT 120)

add_library(ponder_render_public_consumer_tests OBJECT
    PublicConsumer/RenderPublicConsumer.cpp)

target_compile_features(ponder_render_public_consumer_tests PRIVATE cxx_std_23)
target_link_libraries(ponder_render_public_consumer_tests PRIVATE
    ponder::core
    ponder::platform
    ponder::render)

set_target_properties(ponder_render_public_consumer_tests PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON
    FOLDER "tests")

ponder_enable_warnings(ponder_render_public_consumer_tests)
ponder_enable_sanitizers(ponder_render_public_consumer_tests)

ponder_add_render_build_test(
    ponder_render_public_consumer_tests_build
    ponder_render_public_consumer_tests)
set_tests_properties(ponder_render_public_consumer_tests_build PROPERTIES
    LABELS "render;consumer;public;no-pch;deterministic"
    TIMEOUT 120)

if(PONDER_BUILD_EXAMPLES)
    ponder_add_render_build_test(
        ponder_render_clear_window_build_smoke
        ponder_render_clear_window)
    set_tests_properties(ponder_render_clear_window_build_smoke PROPERTIES
        LABELS "render;example;build;public;deterministic"
        TIMEOUT 120)
endif()
