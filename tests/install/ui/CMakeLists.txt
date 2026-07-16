set(ponder_ui_install_prefix
    "${PROJECT_BINARY_DIR}/install-ui-consumer/prefix")
set(ponder_ui_install_consumer_build_dir
    "${PROJECT_BINARY_DIR}/install-ui-consumer/build")

set(ponder_ui_install_consumer_command
    "${CMAKE_COMMAND}"
    -D "PONDER_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    -D "PONDER_ROOT_BINARY_DIR=${PROJECT_BINARY_DIR}"
    -D "PONDER_INSTALL_PREFIX=${ponder_ui_install_prefix}"
    -D "PONDER_CONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/consumer"
    -D "PONDER_CONSUMER_BINARY_DIR=${ponder_ui_install_consumer_build_dir}"
    -D "PONDER_CONSUMER_GENERATOR=${CMAKE_GENERATOR}"
    -D "PONDER_CONSUMER_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    -D "PONDER_CONSUMER_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
    -D "PONDER_EXPECT_UI_EXPERIMENTAL=$<BOOL:${PONDER_BUILD_UI_RENDER_INTEGRATION}>"
    -D "PONDER_BUILD_CONFIG=$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/RunUiInstallConsumer.cmake")

add_test(NAME ponder_ui_install_consumer_test
    COMMAND ${ponder_ui_install_consumer_command})

set_tests_properties(ponder_ui_install_consumer_test PROPERTIES
    LABELS "ui;install;consumer;public;headers;package;dependency;deterministic;ui_build_package"
    RESOURCE_LOCK ponder_nested_build
    TIMEOUT 300)
