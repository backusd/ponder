cmake_minimum_required(VERSION 3.28)

foreach(required_variable IN ITEMS
        PONDER_SOURCE_DIR
        PONDER_MATRIX_BINARY_ROOT
        PONDER_MATRIX_GENERATOR
        PONDER_MATRIX_COMPILER_ID
        PONDER_CTEST_COMMAND)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT PONDER_MATRIX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR
        "The clean Windows UI build matrix is an MSVC-only gate; received "
        "${PONDER_MATRIX_COMPILER_ID}.")
endif()

if(NOT PONDER_MATRIX_GENERATOR MATCHES "^Ninja")
    message(FATAL_ERROR
        "The clean Windows UI build matrix requires a Ninja-family generator so compile "
        "commands can be verified; received ${PONDER_MATRIX_GENERATOR}.")
endif()

function(ponder_run_matrix_command description)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_error
        COMMAND_ECHO NONE)
    if(NOT command_result EQUAL 0)
        message(FATAL_ERROR
            "${description} failed with exit code ${command_result}.\n"
            "stdout:\n${command_output}\n"
            "stderr:\n${command_error}")
    endif()
    message(STATUS "${description}: passed")
endfunction()

function(ponder_assert_target target_manifest target_name should_exist topology config)
    set(padded_manifest "\n${target_manifest}")
    string(FIND "${padded_manifest}" "\ntarget=${target_name}\n" target_position)
    if(should_exist AND target_position EQUAL -1)
        message(FATAL_ERROR
            "${topology} ${config} did not configure required target ${target_name}.")
    elseif(NOT should_exist AND NOT target_position EQUAL -1)
        message(FATAL_ERROR
            "${topology} ${config} unexpectedly configured target ${target_name}.")
    endif()
endfunction()

function(ponder_assert_archive build_dir archive_name should_exist topology config)
    set(matching_archives)
    foreach(archive_path IN ITEMS
            "${build_dir}/lib/${archive_name}.lib"
            "${build_dir}/lib/${config}/${archive_name}.lib")
        if(EXISTS "${archive_path}")
            list(APPEND matching_archives "${archive_path}")
        endif()
    endforeach()
    if(should_exist AND NOT matching_archives)
        message(FATAL_ERROR
            "${topology} ${config} did not produce ${archive_name}.lib.")
    elseif(NOT should_exist AND matching_archives)
        message(FATAL_ERROR
            "${topology} ${config} unexpectedly produced ${archive_name}.lib: "
            "${matching_archives}")
    endif()
endfunction()

function(ponder_verify_compile_commands build_dir topology config render_enabled)
    set(compile_commands_path "${build_dir}/compile_commands.json")
    if(NOT EXISTS "${compile_commands_path}")
        message(FATAL_ERROR
            "${topology} ${config} did not generate compile_commands.json.")
    endif()

    file(READ "${compile_commands_path}" compile_commands)
    string(JSON command_count LENGTH "${compile_commands}")
    if(command_count EQUAL 0)
        message(FATAL_ERROR "${topology} ${config} has an empty compile command database.")
    endif()

    math(EXPR last_command "${command_count} - 1")
    set(found_ui_core_command FALSE)
    set(found_render_command FALSE)
    foreach(command_index RANGE 0 ${last_command})
        string(JSON source_file GET "${compile_commands}" ${command_index} file)
        string(JSON compile_command GET "${compile_commands}" ${command_index} command)
        cmake_path(CONVERT "${source_file}" TO_CMAKE_PATH_LIST normalized_source_file NORMALIZE)

        if(normalized_source_file MATCHES "/libs/ui/src/Library\\.cpp$")
            set(found_ui_core_command TRUE)
            if(compile_command MATCHES "[/\\\\]libs[/\\\\]render[/\\\\]" OR
               compile_command MATCHES "[/\\\\]libs[/\\\\]ui[/\\\\]private[/\\\\]" OR
               compile_command MATCHES "Vulkan|vulkan|VK_USE_PLATFORM")
                message(FATAL_ERROR
                    "${topology} ${config} leaks render/backend or private UI paths into the "
                    "stable UI core compile command:\n${compile_command}")
            endif()
        endif()

        if(normalized_source_file MATCHES "/libs/render/")
            set(found_render_command TRUE)
            if(compile_command MATCHES "[/\\\\]libs[/\\\\]ui[/\\\\]" OR
               compile_command MATCHES "ponder_ui")
                message(FATAL_ERROR
                    "${topology} ${config} leaks UI into a render compile command:\n"
                    "${compile_command}")
            endif()
        endif()
    endforeach()

    if(NOT found_ui_core_command)
        message(FATAL_ERROR
            "${topology} ${config} has no stable UI core compile command.")
    endif()
    if(render_enabled AND NOT found_render_command)
        message(FATAL_ERROR "${topology} ${config} has no render compile command.")
    elseif(NOT render_enabled AND found_render_command)
        message(FATAL_ERROR
            "${topology} ${config} generated render compile commands with render disabled.")
    endif()
endfunction()

function(ponder_verify_topology build_dir topology config render_enabled integration_enabled)
    set(cmake_cache_path "${build_dir}/CMakeCache.txt")
    file(STRINGS "${cmake_cache_path}" build_tests_cache_line
        REGEX "^PONDER_BUILD_TESTS:BOOL=" LIMIT_COUNT 1)
    if(NOT build_tests_cache_line STREQUAL "PONDER_BUILD_TESTS:BOOL=ON")
        message(FATAL_ERROR
            "${topology} ${config} did not resolve the documented "
            "PONDER_BUILD_TESTS default to ON: ${build_tests_cache_line}")
    endif()

    set(target_manifest_path
        "${build_dir}/dependency-checks/RemovedUiDependencyTargets.txt")
    if(NOT EXISTS "${target_manifest_path}")
        message(FATAL_ERROR
            "${topology} ${config} did not generate the configured-target manifest.")
    endif()
    file(READ "${target_manifest_path}" target_manifest)

    ponder_assert_target("${target_manifest}" ponder_ui TRUE "${topology}" "${config}")
    ponder_assert_target("${target_manifest}" ponder_ui_paint TRUE "${topology}" "${config}")
    ponder_assert_target("${target_manifest}" ponder_render "${render_enabled}"
        "${topology}" "${config}")
    ponder_assert_target("${target_manifest}" ponder_render_draw2d "${render_enabled}"
        "${topology}" "${config}")
    ponder_assert_target("${target_manifest}" ponder_render_draw2d_packet "${render_enabled}"
        "${topology}" "${config}")
    ponder_assert_target("${target_manifest}" ponder_render_sentinel_shader "${render_enabled}"
        "${topology}" "${config}")
    ponder_assert_target("${target_manifest}" ponder_ui_render_integration
        "${integration_enabled}" "${topology}" "${config}")
    ponder_assert_target("${target_manifest}" ponder_ui_experimental "${integration_enabled}"
        "${topology}" "${config}")

    if(render_enabled)
        if(target_manifest MATCHES
           "ponder_render\\.(LINK_LIBRARIES|INTERFACE_LINK_LIBRARIES)=[^\n]*ponder(_|::)ui")
            message(FATAL_ERROR
                "${topology} ${config} configures a forbidden render-to-UI link edge.")
        endif()
    else()
        file(READ "${cmake_cache_path}" cmake_cache)
        if(cmake_cache MATCHES
           "(^|\n)(PONDER_RENDER_SHADER_|VULKAN_HEADERS_|VOLK_|VMA_)[^\n]*:")
            message(FATAL_ERROR
                "${topology} ${config} discovered Vulkan or shader-tool configuration with "
                "render disabled.")
        endif()
    endif()

    if(integration_enabled AND NOT target_manifest MATCHES
       "ponder_ui_render_integration\\.LINK_LIBRARIES=[^\n]*ponder::render")
        message(FATAL_ERROR
            "${topology} ${config} does not expose the explicit UI-to-render integration edge.")
    endif()

    ponder_verify_compile_commands(
        "${build_dir}" "${topology}" "${config}" "${render_enabled}")
endfunction()

function(ponder_verify_archives build_dir topology config render_enabled integration_enabled)
    ponder_assert_archive("${build_dir}" ponder_ui TRUE "${topology}" "${config}")
    ponder_assert_archive("${build_dir}" ponder_render "${render_enabled}"
        "${topology}" "${config}")
    ponder_assert_archive("${build_dir}" ponder_render_draw2d "${render_enabled}"
        "${topology}" "${config}")
    ponder_assert_archive("${build_dir}" ponder_ui_render_integration
        "${integration_enabled}" "${topology}" "${config}")
    ponder_assert_archive("${build_dir}" ponder_ui_experimental "${integration_enabled}"
        "${topology}" "${config}")
endfunction()

function(ponder_run_ctest_label build_dir config label topology)
    ponder_run_matrix_command(
        "${topology} ${config} CTest label ${label}"
        "${PONDER_CTEST_COMMAND}"
        --test-dir "${build_dir}"
        -C "${config}"
        --output-on-failure
        --no-tests=error
        -L "${label}")
endfunction()

function(ponder_run_package_checks build_dir config topology render_enabled)
    set(package_tests
        ponder_ui_build_topology_verification
        ponder_removed_ui_dependency_absence_check
        ponder_ui_install_consumer_test)
    if(render_enabled)
        list(APPEND package_tests ponder_render_install_consumer_test)
    endif()

    foreach(package_test IN LISTS package_tests)
        ponder_run_matrix_command(
            "${topology} ${config} package check ${package_test}"
            "${PONDER_CTEST_COMMAND}"
            --test-dir "${build_dir}"
            -C "${config}"
            --output-on-failure
            --no-tests=error
            -R "^${package_test}$")
    endforeach()
endfunction()

cmake_path(ABSOLUTE_PATH PONDER_MATRIX_BINARY_ROOT NORMALIZE
    OUTPUT_VARIABLE normalized_matrix_root)
file(MAKE_DIRECTORY "${normalized_matrix_root}")

set(generator_args -G "${PONDER_MATRIX_GENERATOR}")
if(DEFINED PONDER_MATRIX_GENERATOR_PLATFORM AND
   NOT PONDER_MATRIX_GENERATOR_PLATFORM STREQUAL "")
    list(APPEND generator_args -A "${PONDER_MATRIX_GENERATOR_PLATFORM}")
endif()
if(DEFINED PONDER_MATRIX_GENERATOR_TOOLSET AND
   NOT PONDER_MATRIX_GENERATOR_TOOLSET STREQUAL "")
    list(APPEND generator_args -T "${PONDER_MATRIX_GENERATOR_TOOLSET}")
endif()

set(common_cache_args
    -D PONDER_UI_BUILD_MATRIX_CHILD:BOOL=ON
    -D PONDER_BUILD_APPS:BOOL=OFF
    -D PONDER_BUILD_TOOLS:BOOL=OFF
    -D PONDER_BUILD_EXAMPLES:BOOL=OFF
    -D PONDER_BUILD_PLUGINS:BOOL=OFF
    -D PONDER_ENABLE_INSTALL_RULES:BOOL=ON
    -D PONDER_RENDER_REQUIRE_LIVE_TESTS:BOOL=OFF)
if(DEFINED PONDER_MATRIX_CXX_COMPILER AND NOT PONDER_MATRIX_CXX_COMPILER STREQUAL "")
    list(APPEND common_cache_args
        -D "CMAKE_CXX_COMPILER:FILEPATH=${PONDER_MATRIX_CXX_COMPILER}")
endif()
if(DEFINED PONDER_MATRIX_MAKE_PROGRAM AND NOT PONDER_MATRIX_MAKE_PROGRAM STREQUAL "")
    list(APPEND common_cache_args
        -D "CMAKE_MAKE_PROGRAM:FILEPATH=${PONDER_MATRIX_MAKE_PROGRAM}")
endif()
if(DEFINED PONDER_MATRIX_TOOLCHAIN_FILE AND NOT PONDER_MATRIX_TOOLCHAIN_FILE STREQUAL "")
    list(APPEND common_cache_args
        -D "CMAKE_TOOLCHAIN_FILE:FILEPATH=${PONDER_MATRIX_TOOLCHAIN_FILE}")
endif()

set(render_shader_cache_args)
foreach(shader_tool_variable IN ITEMS
        PONDER_RENDER_SHADER_DXC
        PONDER_RENDER_SHADER_SPIRV_VAL
        PONDER_RENDER_SHADER_SPIRV_CROSS)
    if(DEFINED ${shader_tool_variable} AND NOT "${${shader_tool_variable}}" STREQUAL "")
        list(APPEND render_shader_cache_args
            -D "${shader_tool_variable}:FILEPATH=${${shader_tool_variable}}")
    endif()
endforeach()

set(matrix_topologies render_off render_generic full_integration)
if(DEFINED PONDER_MATRIX_TOPOLOGIES AND NOT PONDER_MATRIX_TOPOLOGIES STREQUAL "")
    string(REPLACE "," ";" matrix_topologies "${PONDER_MATRIX_TOPOLOGIES}")
endif()
set(matrix_configs Debug Release)
if(DEFINED PONDER_MATRIX_CONFIGS AND NOT PONDER_MATRIX_CONFIGS STREQUAL "")
    string(REPLACE "," ";" matrix_configs "${PONDER_MATRIX_CONFIGS}")
endif()

foreach(topology IN LISTS matrix_topologies)
    if(topology STREQUAL "render_off")
        set(render_enabled OFF)
        set(integration_enabled OFF)
        set(build_targets
            ponder_ui
            ponder_ui_paint
            ponder_ui_tests
            ponder_ui_paint_tests
            ponder_ui_header_tests)
    elseif(topology STREQUAL "render_generic")
        set(render_enabled ON)
        set(integration_enabled OFF)
        set(build_targets
            ponder_ui
            ponder_ui_paint
            ponder_ui_tests
            ponder_ui_paint_tests
            ponder_ui_header_tests
            ponder_render
            ponder_render_draw2d
            ponder_render_tests
            ponder_render_backend_tests
            ponder_render_draw2d_packet_tests
            ponder_render_draw2d_shader_interface_tests
            ponder_render_lifetime_contract_tests
            ponder_render_header_tests
            ponder_render_draw2d_packet_header_tests
            ponder_render_draw2d_header_tests)
    elseif(topology STREQUAL "full_integration")
        set(render_enabled ON)
        set(integration_enabled ON)
        set(build_targets
            ponder_ui
            ponder_ui_paint
            ponder_ui_tests
            ponder_ui_paint_tests
            ponder_ui_header_tests
            ponder_render
            ponder_render_draw2d
            ponder_render_tests
            ponder_render_backend_tests
            ponder_render_draw2d_packet_tests
            ponder_render_draw2d_shader_interface_tests
            ponder_render_lifetime_contract_tests
            ponder_render_header_tests
            ponder_render_draw2d_packet_header_tests
            ponder_render_draw2d_header_tests
            ponder_ui_render_integration
            ponder_ui_experimental
            ponder_ui_paint_compiler_tests
            ponder_ui_cpu_packet_verification_tests
            ponder_ui_render_integration_topology_tests
            ponder_ui_experimental_header_tests)
    else()
        message(FATAL_ERROR "Unknown UI build matrix topology: ${topology}")
    endif()

    foreach(config IN LISTS matrix_configs)
        if(NOT config STREQUAL "Debug" AND NOT config STREQUAL "Release")
            message(FATAL_ERROR "Unknown UI build matrix configuration: ${config}")
        endif()
        set(build_dir "${normalized_matrix_root}/${topology}-${config}")
        cmake_path(NORMAL_PATH build_dir OUTPUT_VARIABLE normalized_build_dir)
        cmake_path(IS_PREFIX normalized_matrix_root "${normalized_build_dir}"
            NORMALIZE build_is_safe)
        if(NOT build_is_safe OR normalized_build_dir STREQUAL normalized_matrix_root)
            message(FATAL_ERROR "Refusing to clean unsafe matrix build path: ${build_dir}")
        endif()
        file(REMOVE_RECURSE "${normalized_build_dir}")

        set(topology_cache_args
            -D "PONDER_BUILD_RENDER:BOOL=${render_enabled}"
            -D "PONDER_BUILD_UI_RENDER_INTEGRATION:BOOL=${integration_enabled}"
            -D "CMAKE_BUILD_TYPE:STRING=${config}")
        if(render_enabled)
            list(APPEND topology_cache_args ${render_shader_cache_args})
        endif()

        ponder_run_matrix_command(
            "${topology} ${config} clean configure"
            "${CMAKE_COMMAND}"
            -S "${PONDER_SOURCE_DIR}"
            -B "${normalized_build_dir}"
            ${generator_args}
            ${common_cache_args}
            ${topology_cache_args})

        ponder_verify_topology(
            "${normalized_build_dir}" "${topology}" "${config}"
            "${render_enabled}" "${integration_enabled}")

        ponder_run_matrix_command(
            "${topology} ${config} focused build"
            "${CMAKE_COMMAND}"
            --build "${normalized_build_dir}"
            --config "${config}"
            --parallel
            --target ${build_targets})

        ponder_verify_archives(
            "${normalized_build_dir}" "${topology}" "${config}"
            "${render_enabled}" "${integration_enabled}")

        ponder_run_ctest_label(
            "${normalized_build_dir}" "${config}" ui_cpu_packet "${topology}")
        if(render_enabled)
            ponder_run_ctest_label(
                "${normalized_build_dir}" "${config}" ui_backend_lifecycle "${topology}")
        endif()
        ponder_run_package_checks(
            "${normalized_build_dir}" "${config}" "${topology}" "${render_enabled}")
    endforeach()
endforeach()

list(JOIN matrix_topologies ", " verified_topologies)
list(JOIN matrix_configs ", " verified_configs)
message(STATUS
    "Clean UI build matrix passed for topologies [${verified_topologies}] in MSVC "
    "configurations [${verified_configs}].")
