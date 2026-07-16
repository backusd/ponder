ponder_add_project_test(ponder_ui_tests
    SOURCES
        FoundationValueTests.cpp
        SmokeTests.cpp
    PRIVATE_DEPS
        ponder::ui
    LABELS
        ui
        unit
        public
        deterministic
        ui_cpu_packet)

ponder_add_project_test(ponder_ui_paint_tests
    SOURCES
        PaintRecorderTests.cpp
    PRIVATE_DEPS
        ponder::ui_paint
    LABELS
        ui
        unit
        private
        paint
        deterministic
        ui_cpu_packet)

function(ponder_add_ui_build_test test_name target_name)
    set(test_command "${CMAKE_COMMAND}" --build "${PROJECT_BINARY_DIR}" --target "${target_name}")
    if(CMAKE_CONFIGURATION_TYPES)
        list(APPEND test_command --config "$<CONFIG>")
    endif()

    add_test(NAME "${test_name}" COMMAND ${test_command})
    set_tests_properties("${test_name}" PROPERTIES
        LABELS "ui;build;public;deterministic"
        RESOURCE_LOCK ponder_nested_build
        TIMEOUT 120)
endfunction()

ponder_add_ui_build_test(
    ponder_ui_paint_build
    ponder_ui_paint)
set_tests_properties(ponder_ui_paint_build PROPERTIES
    LABELS "ui;paint;build;private;deterministic"
    TIMEOUT 120)

add_library(ponder_ui_header_tests OBJECT
    HeaderSelfContainment/ColorHeader.cpp
    HeaderSelfContainment/ErrorHeader.cpp
    HeaderSelfContainment/GeometryHeader.cpp
    HeaderSelfContainment/LibraryHeader.cpp
    HeaderSelfContainment/LimitsHeader.cpp
    HeaderSelfContainment/MetricsHeader.cpp
    HeaderSelfContainment/OutcomeHeader.cpp)

target_compile_features(ponder_ui_header_tests PRIVATE cxx_std_23)
target_link_libraries(ponder_ui_header_tests PRIVATE ponder::ui)

set_target_properties(ponder_ui_header_tests PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON
    FOLDER "tests")

ponder_enable_warnings(ponder_ui_header_tests)
ponder_enable_sanitizers(ponder_ui_header_tests)

ponder_add_ui_build_test(
    ponder_ui_header_tests_build
    ponder_ui_header_tests)
set_tests_properties(ponder_ui_header_tests_build PROPERTIES
    LABELS "ui;headers;public;no-pch;deterministic;ui_cpu_packet"
    TIMEOUT 120)

if(PONDER_BUILD_UI_RENDER_INTEGRATION)
    ponder_add_project_test(ponder_ui_paint_compiler_tests
        SOURCES
            PaintCompilerTests.cpp
        PRIVATE_DEPS
            ponder::ui_paint
            ponder::ui_render_integration
            ponder::render_draw2d_packet
        LABELS
            ui
            unit
            private
            paint
            compiler
            draw2d
            deterministic
            ui_cpu_packet)

    ponder_add_project_test(ponder_ui_cpu_packet_verification_tests
        SOURCES
            CpuPacketVerificationTests.cpp
        PRIVATE_DEPS
            ponder::ui_paint
            ponder::ui_render_integration
            ponder::render_draw2d_packet
        LABELS
            ui
            unit
            private
            paint
            compiler
            draw2d
            packet
            cpu
            verification
            deterministic
            ui_cpu_packet)

    ponder_add_ui_build_test(
        ponder_ui_render_integration_library_build
        ponder_ui_render_integration)
    set_tests_properties(ponder_ui_render_integration_library_build PROPERTIES
        LABELS "ui;render;integration;build;private;deterministic;ui_backend_lifecycle"
        TIMEOUT 120)

    ponder_add_project_test(ponder_ui_render_integration_topology_tests
        SOURCES
            RenderIntegrationTopology.cpp
        PRIVATE_DEPS
            ponder::ui_render_integration
        LABELS
            ui
            render
            integration
            link
            private
            deterministic
            ui_backend_lifecycle)

    ponder_add_ui_build_test(
        ponder_ui_experimental_library_build
        ponder_ui_experimental)
    set_tests_properties(ponder_ui_experimental_library_build PROPERTIES
        LABELS "ui;render;experimental;build;public;deterministic;ui_backend_lifecycle"
        TIMEOUT 120)

    add_library(ponder_ui_experimental_header_tests OBJECT
        HeaderSelfContainment/ExperimentalRectangleRendererHeader.cpp)

    target_compile_features(ponder_ui_experimental_header_tests PRIVATE cxx_std_23)
    target_link_libraries(ponder_ui_experimental_header_tests PRIVATE ponder::ui_experimental)

    set_target_properties(ponder_ui_experimental_header_tests PROPERTIES
        DISABLE_PRECOMPILE_HEADERS ON
        FOLDER "tests")

    ponder_enable_warnings(ponder_ui_experimental_header_tests)
    ponder_enable_sanitizers(ponder_ui_experimental_header_tests)

    ponder_add_ui_build_test(
        ponder_ui_experimental_header_tests_build
        ponder_ui_experimental_header_tests)
    set_tests_properties(ponder_ui_experimental_header_tests_build PROPERTIES
        LABELS "ui;headers;render;experimental;public;no-pch;deterministic;ui_cpu_packet;ui_backend_lifecycle"
        TIMEOUT 120)

    if(PONDER_BUILD_EXAMPLES)
        ponder_add_ui_build_test(
            ponder_ui_1_rectangle_build_smoke
            ponder_ui_1_rectangle)
        set_tests_properties(ponder_ui_1_rectangle_build_smoke PROPERTIES
            LABELS "ui;example;build;public;deterministic"
            TIMEOUT 120)
    endif()

    set(ponder_ui_cpu_packet_gate_command
        "${CMAKE_CTEST_COMMAND}"
        --test-dir "${PROJECT_BINARY_DIR}"
        --output-on-failure
        --no-tests=error
        -L ui_cpu_packet)
    if(CMAKE_CONFIGURATION_TYPES)
        list(APPEND ponder_ui_cpu_packet_gate_command -C "$<CONFIG>")
    endif()

    add_test(NAME ponder_ui_cpu_packet_verification_gate
        COMMAND ${ponder_ui_cpu_packet_gate_command})
    set_tests_properties(ponder_ui_cpu_packet_verification_gate PROPERTIES
        LABELS "ui;cpu;packet;verification;gate;deterministic"
        RESOURCE_LOCK "ponder_nested_ctest;ponder_nested_build"
        TIMEOUT 300)

    set(ponder_ui_backend_lifecycle_gate_command
        "${CMAKE_CTEST_COMMAND}"
        --test-dir "${PROJECT_BINARY_DIR}"
        --output-on-failure
        --no-tests=error
        -L ui_backend_lifecycle)
    if(CMAKE_CONFIGURATION_TYPES)
        list(APPEND ponder_ui_backend_lifecycle_gate_command -C "$<CONFIG>")
    endif()

    add_test(NAME ponder_ui_backend_lifecycle_verification_gate
        COMMAND ${ponder_ui_backend_lifecycle_gate_command})
    set_tests_properties(ponder_ui_backend_lifecycle_verification_gate PROPERTIES
        LABELS "ui;render;backend;lifecycle;verification;gate;deterministic"
        RESOURCE_LOCK "ponder_nested_ctest;ponder_nested_build"
        TIMEOUT 900)
endif()
