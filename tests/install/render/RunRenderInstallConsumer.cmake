if(NOT DEFINED PONDER_ROOT_BINARY_DIR)
    message(FATAL_ERROR "PONDER_ROOT_BINARY_DIR is required.")
endif()
if(NOT DEFINED PONDER_INSTALL_PREFIX)
    message(FATAL_ERROR "PONDER_INSTALL_PREFIX is required.")
endif()
if(NOT DEFINED PONDER_CONSUMER_SOURCE_DIR)
    message(FATAL_ERROR "PONDER_CONSUMER_SOURCE_DIR is required.")
endif()
if(NOT DEFINED PONDER_CONSUMER_BINARY_DIR)
    message(FATAL_ERROR "PONDER_CONSUMER_BINARY_DIR is required.")
endif()
if(NOT DEFINED PONDER_BUILD_CONFIG)
    set(PONDER_BUILD_CONFIG "")
endif()

set(config_args)
set(ctest_config_args)
if(NOT PONDER_BUILD_CONFIG STREQUAL "")
    list(APPEND config_args --config "${PONDER_BUILD_CONFIG}")
    list(APPEND ctest_config_args -C "${PONDER_BUILD_CONFIG}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${PONDER_ROOT_BINARY_DIR}" ${config_args}
    RESULT_VARIABLE project_build_result)
if(NOT project_build_result EQUAL 0)
    message(FATAL_ERROR "Building ponder before the install consumer failed: ${project_build_result}")
endif()

file(REMOVE_RECURSE "${PONDER_INSTALL_PREFIX}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${PONDER_ROOT_BINARY_DIR}"
            --prefix "${PONDER_INSTALL_PREFIX}" ${config_args}
    RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "Installing ponder for the render consumer failed: ${install_result}")
endif()

file(GLOB_RECURSE installed_test_helpers
    "${PONDER_INSTALL_PREFIX}/*ponder_platform_process_helper*")
if(installed_test_helpers)
    message(FATAL_ERROR
        "The product install contains a test-helper executable: ${installed_test_helpers}")
endif()

file(REMOVE_RECURSE "${PONDER_CONSUMER_BINARY_DIR}")
set(generator_args)
if(DEFINED PONDER_CONSUMER_GENERATOR AND NOT PONDER_CONSUMER_GENERATOR STREQUAL "")
    list(APPEND generator_args -G "${PONDER_CONSUMER_GENERATOR}")
endif()
if(DEFINED PONDER_CONSUMER_GENERATOR_PLATFORM AND NOT PONDER_CONSUMER_GENERATOR_PLATFORM STREQUAL "")
    list(APPEND generator_args -A "${PONDER_CONSUMER_GENERATOR_PLATFORM}")
endif()
if(DEFINED PONDER_CONSUMER_GENERATOR_TOOLSET AND NOT PONDER_CONSUMER_GENERATOR_TOOLSET STREQUAL "")
    list(APPEND generator_args -T "${PONDER_CONSUMER_GENERATOR_TOOLSET}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
            -S "${PONDER_CONSUMER_SOURCE_DIR}"
            -B "${PONDER_CONSUMER_BINARY_DIR}"
            ${generator_args}
            -D "CMAKE_PREFIX_PATH=${PONDER_INSTALL_PREFIX}"
    RESULT_VARIABLE configure_result)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "Configuring the installed render consumer failed: ${configure_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${PONDER_CONSUMER_BINARY_DIR}"
            --target ponder_render_install_consumer ${config_args}
    RESULT_VARIABLE build_result)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Building the installed render consumer failed: ${build_result}")
endif()

execute_process(
    COMMAND "${CMAKE_CTEST_COMMAND}"
            --test-dir "${PONDER_CONSUMER_BINARY_DIR}"
            ${ctest_config_args}
            --output-on-failure
            --no-tests=error
    RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "Running the installed render consumer failed: ${run_result}")
endif()
