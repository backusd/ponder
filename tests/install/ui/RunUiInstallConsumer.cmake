cmake_minimum_required(VERSION 3.28)

foreach(required_variable IN ITEMS
        PONDER_SOURCE_DIR
        PONDER_ROOT_BINARY_DIR
        PONDER_INSTALL_PREFIX
        PONDER_CONSUMER_SOURCE_DIR
        PONDER_CONSUMER_BINARY_DIR
        PONDER_EXPECT_UI_EXPERIMENTAL)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

function(ponder_normalize_safe_child_path input_path label output_variable)
    cmake_path(ABSOLUTE_PATH PONDER_ROOT_BINARY_DIR NORMALIZE
        OUTPUT_VARIABLE normalized_root_binary_dir)
    cmake_path(ABSOLUTE_PATH input_path BASE_DIRECTORY "${normalized_root_binary_dir}" NORMALIZE
        OUTPUT_VARIABLE normalized_path)
    cmake_path(IS_PREFIX normalized_root_binary_dir "${normalized_path}" NORMALIZE path_is_child)
    cmake_path(COMPARE "${normalized_root_binary_dir}" EQUAL "${normalized_path}" path_is_root)
    if(NOT path_is_child OR path_is_root)
        message(FATAL_ERROR
            "Refusing to remove ${label} because it is not a strict child of "
            "PONDER_ROOT_BINARY_DIR: ${normalized_path}")
    endif()
    set("${output_variable}" "${normalized_path}" PARENT_SCOPE)
endfunction()

ponder_normalize_safe_child_path(
    "${PONDER_INSTALL_PREFIX}" "the UI install prefix" normalized_install_prefix)
ponder_normalize_safe_child_path(
    "${PONDER_CONSUMER_BINARY_DIR}" "the UI consumer build directory"
    normalized_consumer_binary_dir)
set(PONDER_INSTALL_PREFIX "${normalized_install_prefix}")
set(PONDER_CONSUMER_BINARY_DIR "${normalized_consumer_binary_dir}")

if(NOT DEFINED PONDER_BUILD_CONFIG)
    set(PONDER_BUILD_CONFIG "")
endif()

function(ponder_normalize_text input_text output_variable)
    string(REPLACE "\\" "/" normalized_text "${input_text}")
    string(TOLOWER "${normalized_text}" normalized_text)
    set("${output_variable}" "${normalized_text}" PARENT_SCOPE)
endfunction()

set(removed_name_prefix "im")
set(removed_name_suffix "gui")
set(removed_name "${removed_name_prefix}${removed_name_suffix}")
set(removed_name_regex
    "dear[ _-]*${removed_name}|${removed_name}|ponder(::|_)${removed_name}|third_party[/\\]+${removed_name}")

function(ponder_require_removed_ui_dependency_absent input_text label)
    ponder_normalize_text("${input_text}" normalized_text)
    if(normalized_text MATCHES "${removed_name_regex}")
        message(FATAL_ERROR "The removed UI dependency leaked into ${label}.")
    endif()
endfunction()

function(ponder_require_clean_public_text input_text label)
    ponder_require_removed_ui_dependency_absent("${input_text}" "${label}")
    ponder_normalize_text("${input_text}" normalized_text)
    foreach(blocked_fragment IN ITEMS
            "private/include"
            "/libs/ui/src/"
            "/libs/render/src/"
            "ponder_render_draw2d"
            "ponder_render_volk"
            "ponder_ui_paint"
            "ponder_ui_render_integration"
            "ponder_render_sentinel_shader"
            "ponder_render_draw2d_rectangle_")
        string(FIND "${normalized_text}" "${blocked_fragment}" blocked_position)
        if(NOT blocked_position EQUAL -1)
            message(FATAL_ERROR
                "Private implementation topology leaked into ${label}: ${blocked_fragment}")
        endif()
    endforeach()

    foreach(project_path IN ITEMS "${PONDER_SOURCE_DIR}" "${PONDER_ROOT_BINARY_DIR}")
        ponder_normalize_text("${project_path}" normalized_project_path)
        string(FIND "${normalized_text}" "${normalized_project_path}" project_path_position)
        if(NOT project_path_position EQUAL -1)
            message(FATAL_ERROR "A project source/build path leaked into ${label}.")
        endif()
    endforeach()
endfunction()

function(ponder_require_clean_exported_targets input_text label)
    string(REGEX MATCHALL
        "add_(library|executable)\\([ \t\r\n]*ponder::[A-Za-z0-9_.+-]+"
        exported_target_declarations "${input_text}")
    if(NOT exported_target_declarations)
        message(FATAL_ERROR "No imported ponder targets were found in ${label}.")
    endif()
    foreach(target_declaration IN LISTS exported_target_declarations)
        string(REGEX REPLACE
            "^add_(library|executable)\\([ \t\r\n]*ponder::" ""
            exported_target_name "${target_declaration}")
        string(TOLOWER "${exported_target_name}" exported_target_name)
        if(exported_target_name STREQUAL "ui_experimental")
            continue()
        endif()

        foreach(blocked_fragment IN ITEMS
                private
                draw2d
                volk
                shader
                vulkan
                vma
                backend
                internal
                generated
                test
                helper
                example
                workbench
                ui_paint
                ui_render_integration)
            string(FIND "${exported_target_name}" "${blocked_fragment}" blocked_position)
            if(NOT blocked_position EQUAL -1)
                message(FATAL_ERROR
                    "Implementation, test, or tool target ponder::${exported_target_name} "
                    "leaked into ${label}.")
            endif()
        endforeach()
    endforeach()
endfunction()

function(ponder_require_clean_consumer_commands input_text label)
    ponder_require_removed_ui_dependency_absent("${input_text}" "${label}")
    ponder_normalize_text("${input_text}" normalized_text)
    foreach(blocked_fragment IN ITEMS
            "/libs/ui/private/"
            "/libs/ui/src/"
            "/libs/render/private/"
            "/libs/render/src/"
            "ponder_render_draw2d"
            "ponder_render_volk"
            "ponder_ui_paint"
            "ponder_ui_render_integration")
        string(FIND "${normalized_text}" "${blocked_fragment}" blocked_position)
        if(NOT blocked_position EQUAL -1)
            message(FATAL_ERROR
                "A private project implementation leaked into ${label}: ${blocked_fragment}")
        endif()
    endforeach()

    if(normalized_text MATCHES "(^|[ ;\"])(/yu[^ ;\"]*|-include-pch)([ ;\"]|$)"
            OR normalized_text MATCHES "cmake_pch")
        message(FATAL_ERROR "A consumer header check unexpectedly uses a precompiled header in ${label}.")
    endif()
endfunction()

function(ponder_require_core_link_clean input_text label)
    ponder_normalize_text("${input_text}" normalized_text)
    foreach(blocked_fragment IN ITEMS
            "ponder_render.lib"
            "libponder_render.a"
            "ponder_ui_experimental"
            "ponder_ui_render_integration"
            "ponder_render_draw2d")
        string(FIND "${normalized_text}" "${blocked_fragment}" blocked_position)
        if(NOT blocked_position EQUAL -1)
            message(FATAL_ERROR
                "The stable UI-core consumer acquired an integration dependency in ${label}: "
                "${blocked_fragment}")
        endif()
    endforeach()
endfunction()

set(config_args)
set(ctest_config_args)
set(consumer_configure_args)
if(NOT PONDER_BUILD_CONFIG STREQUAL "")
    list(APPEND config_args --config "${PONDER_BUILD_CONFIG}")
    list(APPEND ctest_config_args -C "${PONDER_BUILD_CONFIG}")
    list(APPEND consumer_configure_args
        -D "CMAKE_BUILD_TYPE:STRING=${PONDER_BUILD_CONFIG}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${PONDER_ROOT_BINARY_DIR}" ${config_args}
    RESULT_VARIABLE project_build_result)
if(NOT project_build_result EQUAL 0)
    message(FATAL_ERROR "Building ponder before the UI install consumer failed: ${project_build_result}")
endif()

file(REMOVE_RECURSE "${PONDER_INSTALL_PREFIX}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${PONDER_ROOT_BINARY_DIR}"
            --prefix "${PONDER_INSTALL_PREFIX}" ${config_args}
    RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "Installing ponder for the UI consumer failed: ${install_result}")
endif()

foreach(stable_header IN ITEMS Color Error Geometry Library Limits Metrics Outcome)
    set(header_path "${PONDER_INSTALL_PREFIX}/include/ponder/ui/${stable_header}.hpp")
    if(NOT EXISTS "${header_path}")
        message(FATAL_ERROR "The installed package is missing stable UI header: ${header_path}")
    endif()
endforeach()

set(experimental_header
    "${PONDER_INSTALL_PREFIX}/include/ponder/ui/experimental/RectangleRenderer.hpp")
if(PONDER_EXPECT_UI_EXPERIMENTAL)
    if(NOT EXISTS "${experimental_header}")
        message(FATAL_ERROR "The installed package is missing the expected experimental UI header.")
    endif()
elseif(EXISTS "${experimental_header}")
    message(FATAL_ERROR "The render-independent install unexpectedly contains the experimental UI header.")
endif()

file(GLOB_RECURSE installed_headers LIST_DIRECTORIES false
    "${PONDER_INSTALL_PREFIX}/include/*")
foreach(installed_header IN LISTS installed_headers)
    file(RELATIVE_PATH relative_header "${PONDER_INSTALL_PREFIX}/include" "${installed_header}")
    file(TO_CMAKE_PATH "${relative_header}" relative_header)
    ponder_require_removed_ui_dependency_absent(
        "${relative_header}" "installed header path ${relative_header}")
    string(TOLOWER "${relative_header}" lower_relative_header)
    if(lower_relative_header MATCHES
            "(^|/)(private|src|tests?|generated)(/|$)"
            OR lower_relative_header MATCHES "draw2dpacket\\.hpp$"
            OR lower_relative_header MATCHES "(paintrecorder|paintcompiler)\\.hpp$")
        message(FATAL_ERROR "A private/generated/test header was installed: ${relative_header}")
    endif()

    file(READ "${installed_header}" installed_header_contents)
    ponder_require_clean_public_text(
        "${installed_header_contents}" "installed header ${relative_header}")
endforeach()

set(package_dir "${PONDER_INSTALL_PREFIX}/lib/cmake/ponder")
set(ponder_targets_file "${package_dir}/ponderTargets.cmake")
if(NOT EXISTS "${ponder_targets_file}")
    message(FATAL_ERROR "The installed package export is missing: ${ponder_targets_file}")
endif()

file(GLOB installed_package_files LIST_DIRECTORIES false "${package_dir}/*.cmake")
if(NOT installed_package_files)
    message(FATAL_ERROR "The installed package contains no CMake package files.")
endif()

set(installed_package_contents "")
foreach(package_file IN LISTS installed_package_files)
    file(READ "${package_file}" package_contents)
    file(RELATIVE_PATH relative_package_file "${PONDER_INSTALL_PREFIX}" "${package_file}")
    ponder_require_clean_public_text(
        "${package_contents}" "installed package file ${relative_package_file}")
    string(APPEND installed_package_contents "\n${package_contents}")
endforeach()
ponder_require_clean_exported_targets(
    "${installed_package_contents}" "the installed package export")

if(NOT installed_package_contents MATCHES "ponder::ui([^_a-z0-9]|$)")
    message(FATAL_ERROR "The installed package does not export ponder::ui.")
endif()
set(package_has_experimental FALSE)
if(installed_package_contents MATCHES "ponder::ui_experimental([^a-z0-9_]|$)")
    set(package_has_experimental TRUE)
endif()
if(PONDER_EXPECT_UI_EXPERIMENTAL AND NOT package_has_experimental)
    message(FATAL_ERROR "The installed package is missing ponder::ui_experimental.")
elseif(NOT PONDER_EXPECT_UI_EXPERIMENTAL AND package_has_experimental)
    message(FATAL_ERROR "The render-independent package unexpectedly exports ui_experimental.")
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
             ${consumer_configure_args}
             -D "CMAKE_PREFIX_PATH=${PONDER_INSTALL_PREFIX}"
            -D "CMAKE_EXPORT_COMPILE_COMMANDS=ON"
            -D "PONDER_EXPECT_UI_EXPERIMENTAL=${PONDER_EXPECT_UI_EXPERIMENTAL}"
    RESULT_VARIABLE configure_result)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR "Configuring the installed UI consumer failed: ${configure_result}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${PONDER_CONSUMER_BINARY_DIR}"
            --target ponder_ui_core_install_consumer ${config_args} --verbose
    RESULT_VARIABLE core_build_result
    OUTPUT_VARIABLE core_build_output
    ERROR_VARIABLE core_build_error)
if(NOT core_build_result EQUAL 0)
    message(FATAL_ERROR
        "Building the stable installed UI consumer failed: ${core_build_result}\n"
        "${core_build_output}\n${core_build_error}")
endif()
set(core_build_log "${core_build_output}\n${core_build_error}")
ponder_require_clean_consumer_commands("${core_build_log}" "stable UI consumer build commands")
ponder_require_core_link_clean("${core_build_log}" "stable UI consumer build commands")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${PONDER_CONSUMER_BINARY_DIR}"
            ${config_args} --verbose
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "Building the installed UI consumers failed: ${build_result}\n"
        "${build_output}\n${build_error}")
endif()
set(build_log "${build_output}\n${build_error}")
ponder_require_clean_consumer_commands("${build_log}" "UI consumer build commands")

set(compile_commands_file "${PONDER_CONSUMER_BINARY_DIR}/compile_commands.json")
if(EXISTS "${compile_commands_file}")
    file(READ "${compile_commands_file}" compile_commands)
    ponder_require_clean_consumer_commands("${compile_commands}" "UI consumer compile_commands.json")
else()
    message(STATUS
        "The selected generator does not emit compile_commands.json; verbose build commands were checked.")
endif()

execute_process(
    COMMAND "${CMAKE_CTEST_COMMAND}"
            --test-dir "${PONDER_CONSUMER_BINARY_DIR}"
            ${ctest_config_args}
            --output-on-failure
            --no-tests=error
    RESULT_VARIABLE run_result)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "Running the installed UI consumers failed: ${run_result}")
endif()
