include_guard(GLOBAL)

function(ponder_escape_cpp_string input output_variable)
    set(escaped "${input}")
    string(REPLACE "\\" "\\\\" escaped "${escaped}")
    string(REPLACE "\"" "\\\"" escaped "${escaped}")
    string(REPLACE "\r" "\\r" escaped "${escaped}")
    string(REPLACE "\n" "\\n" escaped "${escaped}")
    set("${output_variable}" "${escaped}" PARENT_SCOPE)
endfunction()

function(ponder_detect_git_commit output_variable)
    set(git_commit "")

    find_package(Git QUIET)
    if(Git_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --verify HEAD
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
            RESULT_VARIABLE git_result
            OUTPUT_VARIABLE git_output
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE)

        if(git_result EQUAL 0)
            set(git_commit "${git_output}")
        endif()
    endif()

    set("${output_variable}" "${git_commit}" PARENT_SCOPE)
endfunction()

function(ponder_configure_build_info output_variable)
    set(options)
    set(one_value_args TEMPLATE OUTPUT_DIRECTORY)
    set(multi_value_args)

    cmake_parse_arguments(
        PONDER_BUILD_INFO
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN})

    if(NOT PONDER_BUILD_INFO_TEMPLATE)
        message(FATAL_ERROR "ponder_configure_build_info requires TEMPLATE.")
    endif()

    if(NOT PONDER_BUILD_INFO_OUTPUT_DIRECTORY)
        set(PONDER_BUILD_INFO_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/src")
    endif()

    ponder_detect_git_commit(PONDER_BUILD_INFO_GIT_COMMIT)

    if(CMAKE_BUILD_TYPE)
        set(PONDER_BUILD_INFO_CONFIGURED_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
    elseif(CMAKE_CONFIGURATION_TYPES)
        set(PONDER_BUILD_INFO_CONFIGURED_BUILD_TYPE "multi-config")
    else()
        set(PONDER_BUILD_INFO_CONFIGURED_BUILD_TYPE "")
    endif()

    if(CMAKE_SIZEOF_VOID_P)
        math(EXPR PONDER_BUILD_INFO_POINTER_WIDTH_BITS "${CMAKE_SIZEOF_VOID_P} * 8")
    else()
        set(PONDER_BUILD_INFO_POINTER_WIDTH_BITS 0)
    endif()

    set(PONDER_BUILD_INFO_PROJECT_NAME "${PROJECT_NAME}")
    set(PONDER_BUILD_INFO_PROJECT_VERSION "${PROJECT_VERSION}")
    set(PONDER_BUILD_INFO_COMPILER_NAME "${CMAKE_CXX_COMPILER_ID}")
    set(PONDER_BUILD_INFO_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")
    set(PONDER_BUILD_INFO_TARGET_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}")
    set(PONDER_BUILD_INFO_TARGET_SYSTEM_PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}")
    set(PONDER_BUILD_INFO_CMAKE_GENERATOR "${CMAKE_GENERATOR}")
    set(PONDER_BUILD_INFO_CMAKE_VERSION "${CMAKE_VERSION}")

    foreach(build_info_variable IN ITEMS
        PONDER_BUILD_INFO_PROJECT_NAME
        PONDER_BUILD_INFO_PROJECT_VERSION
        PONDER_BUILD_INFO_GIT_COMMIT
        PONDER_BUILD_INFO_CONFIGURED_BUILD_TYPE
        PONDER_BUILD_INFO_COMPILER_NAME
        PONDER_BUILD_INFO_COMPILER_VERSION
        PONDER_BUILD_INFO_TARGET_SYSTEM_NAME
        PONDER_BUILD_INFO_TARGET_SYSTEM_PROCESSOR
        PONDER_BUILD_INFO_CMAKE_GENERATOR
        PONDER_BUILD_INFO_CMAKE_VERSION)
        ponder_escape_cpp_string(
            "${${build_info_variable}}"
            "${build_info_variable}_CPP")
    endforeach()

    file(MAKE_DIRECTORY "${PONDER_BUILD_INFO_OUTPUT_DIRECTORY}")
    set(build_info_source "${PONDER_BUILD_INFO_OUTPUT_DIRECTORY}/BuildInfo.cpp")

    configure_file(
        "${PONDER_BUILD_INFO_TEMPLATE}"
        "${build_info_source}"
        @ONLY
        NEWLINE_STYLE LF)

    set("${output_variable}" "${build_info_source}" PARENT_SCOPE)
endfunction()