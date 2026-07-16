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

set(ponder_render_backend_test_deps
    ponder::render
    ponder::render_draw2d
    ponder::volk)
if(PONDER_BUILD_UI_RENDER_INTEGRATION)
    list(APPEND ponder_render_backend_test_deps ponder::ui_experimental)
endif()

ponder_add_project_test(ponder_render_backend_tests
    SOURCES
        VulkanQueueSynchronizationTests.cpp
        VulkanBootstrapTests.cpp
    PRIVATE_DEPS
        ${ponder_render_backend_test_deps}
    LABELS
        render
        backend
        fake-dispatch
        lifecycle
        rollback
        deterministic
        ui_backend_lifecycle)

ponder_add_project_test(ponder_render_draw2d_packet_tests
    SOURCES
        Draw2DPacketTests.cpp
    PRIVATE_DEPS
        ponder::render_draw2d_packet
    LABELS
        render
        unit
        private
        draw2d
        packet
        deterministic
        ui_cpu_packet)

ponder_add_project_test(ponder_render_draw2d_shader_interface_tests
    SOURCES
        Draw2DRectangleShaderInterfaceTests.cpp
    PRIVATE_DEPS
        ponder::render_draw2d_packet
        ponder::render_draw2d_rectangle_vertex_shader
        ponder::render_draw2d_rectangle_fragment_shader
    LABELS
        render
        unit
        private
        draw2d
        shader
        deterministic
        ui_backend_lifecycle)

target_compile_definitions(ponder_render_backend_tests PRIVATE
    PONDER_BUILD_UI_RENDER_INTEGRATION=$<BOOL:${PONDER_BUILD_UI_RENDER_INTEGRATION}>)

add_dependencies(ponder_render_draw2d_shader_interface_tests
    ponder_render_draw2d_rectangle_vertex_shader
    ponder_render_draw2d_rectangle_fragment_shader)

ponder_add_project_test(ponder_render_lifetime_contract_tests
    SOURCES
        LifetimeContractDeathTests.cpp
    PRIVATE_DEPS
        ponder::render
        ponder::render_draw2d
        ponder::volk
    TIMEOUT
        60
    LABELS
        render
        unit
        lifetime
        death
        deterministic
        ui_backend_lifecycle)

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
        RESOURCE_LOCK ponder_nested_build
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

ponder_add_render_build_test(
    ponder_render_draw2d_build
    ponder_render_draw2d)
set_tests_properties(ponder_render_draw2d_build PROPERTIES
    LABELS "render;build;private;draw2d;deterministic;ui_backend_lifecycle"
    TIMEOUT 120)

add_library(ponder_render_draw2d_packet_header_tests OBJECT
    HeaderSelfContainment/Draw2DPacketHeader.cpp
    HeaderSelfContainment/Draw2DRectangleShaderInterfaceHeader.cpp)

target_compile_features(ponder_render_draw2d_packet_header_tests PRIVATE cxx_std_23)
target_link_libraries(ponder_render_draw2d_packet_header_tests PRIVATE
    ponder::render_draw2d_packet)

set_target_properties(ponder_render_draw2d_packet_header_tests PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON
    FOLDER "tests")

ponder_enable_warnings(ponder_render_draw2d_packet_header_tests)
ponder_enable_sanitizers(ponder_render_draw2d_packet_header_tests)

ponder_add_render_build_test(
    ponder_render_draw2d_packet_header_tests_build
    ponder_render_draw2d_packet_header_tests)
set_tests_properties(ponder_render_draw2d_packet_header_tests_build PROPERTIES
    LABELS "render;headers;private;draw2d;no-pch;deterministic;ui_cpu_packet"
    TIMEOUT 120)

add_library(ponder_render_draw2d_header_tests OBJECT
    HeaderSelfContainment/Draw2DLayerHeader.cpp)

target_compile_features(ponder_render_draw2d_header_tests PRIVATE cxx_std_23)
target_link_libraries(ponder_render_draw2d_header_tests PRIVATE
    ponder::render_draw2d)

set_target_properties(ponder_render_draw2d_header_tests PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON
    FOLDER "tests")

ponder_enable_warnings(ponder_render_draw2d_header_tests)
ponder_enable_sanitizers(ponder_render_draw2d_header_tests)

ponder_add_render_build_test(
    ponder_render_draw2d_header_tests_build
    ponder_render_draw2d_header_tests)
set_tests_properties(ponder_render_draw2d_header_tests_build PROPERTIES
    LABELS "render;headers;private;draw2d;no-pch;deterministic;ui_backend_lifecycle"
    TIMEOUT 120)

ponder_add_render_build_test(
    ponder_render_sentinel_shader_build
    ponder_render_sentinel_shader)
set_tests_properties(ponder_render_sentinel_shader_build PROPERTIES
    LABELS "render;shader;build;private;deterministic"
    TIMEOUT 120)

ponder_add_render_build_test(
    ponder_render_sentinel_shader_artifact_check_build
    ponder_render_sentinel_shader_artifact_check)
set_tests_properties(ponder_render_sentinel_shader_artifact_check_build PROPERTIES
    LABELS "render;shader;artifact;reflection;staleness;private;deterministic"
    TIMEOUT 120)

ponder_add_render_build_test(
    ponder_render_sentinel_shader_artifact_staleness_check_build
    ponder_render_sentinel_shader_artifact_staleness_check)
set_tests_properties(ponder_render_sentinel_shader_artifact_staleness_check_build PROPERTIES
    LABELS "render;shader;artifact;staleness;hostile;private;deterministic"
    TIMEOUT 120)

ponder_add_render_build_test(
    ponder_render_draw2d_rectangle_vertex_shader_build
    ponder_render_draw2d_rectangle_vertex_shader)
set_tests_properties(ponder_render_draw2d_rectangle_vertex_shader_build PROPERTIES
    LABELS "render;shader;build;private;draw2d;deterministic;ui_backend_lifecycle"
    TIMEOUT 120)

ponder_add_render_build_test(
    ponder_render_draw2d_rectangle_fragment_shader_build
    ponder_render_draw2d_rectangle_fragment_shader)
set_tests_properties(ponder_render_draw2d_rectangle_fragment_shader_build PROPERTIES
    LABELS "render;shader;build;private;draw2d;deterministic;ui_backend_lifecycle"
    TIMEOUT 120)

ponder_add_render_build_test(
    ponder_render_draw2d_rectangle_vertex_shader_artifact_check_build
    ponder_render_draw2d_rectangle_vertex_shader_artifact_check)
set_tests_properties(ponder_render_draw2d_rectangle_vertex_shader_artifact_check_build PROPERTIES
    LABELS "render;shader;artifact;reflection;private;draw2d;deterministic;ui_backend_lifecycle"
    TIMEOUT 120)

ponder_add_render_build_test(
    ponder_render_draw2d_rectangle_fragment_shader_artifact_check_build
    ponder_render_draw2d_rectangle_fragment_shader_artifact_check)
set_tests_properties(ponder_render_draw2d_rectangle_fragment_shader_artifact_check_build PROPERTIES
    LABELS "render;shader;artifact;reflection;private;draw2d;deterministic;ui_backend_lifecycle"
    TIMEOUT 120)

ponder_add_render_build_test(
    ponder_render_draw2d_rectangle_vertex_shader_artifact_staleness_check_build
    ponder_render_draw2d_rectangle_vertex_shader_artifact_staleness_check)
set_tests_properties(ponder_render_draw2d_rectangle_vertex_shader_artifact_staleness_check_build
    PROPERTIES
    LABELS "render;shader;artifact;staleness;hostile;private;draw2d;deterministic;ui_backend_lifecycle"
    TIMEOUT 120)

ponder_add_render_build_test(
    ponder_render_draw2d_rectangle_fragment_shader_artifact_staleness_check_build
    ponder_render_draw2d_rectangle_fragment_shader_artifact_staleness_check)
set_tests_properties(ponder_render_draw2d_rectangle_fragment_shader_artifact_staleness_check_build
    PROPERTIES
    LABELS "render;shader;artifact;staleness;hostile;private;draw2d;deterministic;ui_backend_lifecycle"
    TIMEOUT 120)

ponder_add_render_build_test(
    ponder_render_draw2d_rectangle_shader_interface_check_build
    ponder_render_draw2d_rectangle_shader_interface_check)
set_tests_properties(ponder_render_draw2d_rectangle_shader_interface_check_build PROPERTIES
    LABELS "render;shader;artifact;reflection;interface;private;draw2d;deterministic;ui_backend_lifecycle"
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
