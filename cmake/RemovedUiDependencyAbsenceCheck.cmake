cmake_minimum_required(VERSION 3.28)

foreach(required_variable IN ITEMS
        PONDER_SOURCE_DIR
        PONDER_BINARY_DIR
        PONDER_REMOVED_UI_DEPENDENCY_TARGET_MANIFEST)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

set(name_prefix "im")
set(name_suffix "gui")
set(removed_dependency "${name_prefix}${name_suffix}")
set(blocked_regex
    "dear[ _-]*${removed_dependency}|${removed_dependency}|ponder(::|_)${removed_dependency}|third_party[/\\\\]+${removed_dependency}")

function(ponder_require_absent_from_text text label)
    string(TOLOWER "${text}" lower_text)
    if(lower_text MATCHES "${blocked_regex}")
        message(FATAL_ERROR "Removed UI dependency reference found in ${label}.")
    endif()
endfunction()

function(ponder_require_absent_from_file file_path)
    if(NOT EXISTS "${file_path}")
        return()
    endif()

    file(READ "${file_path}" file_contents)
    file(RELATIVE_PATH relative_path "${PONDER_SOURCE_DIR}" "${file_path}")
    if(relative_path MATCHES "^\\.\\.")
        file(RELATIVE_PATH relative_path "${PONDER_BINARY_DIR}" "${file_path}")
    endif()
    ponder_require_absent_from_text("${file_contents}" "${relative_path}")
endfunction()

if(EXISTS "${PONDER_SOURCE_DIR}/third_party/${removed_dependency}")
    message(FATAL_ERROR "Removed UI dependency checkout still exists under third_party.")
endif()

if(NOT EXISTS "${PONDER_REMOVED_UI_DEPENDENCY_TARGET_MANIFEST}")
    message(FATAL_ERROR
        "Configured target manifest is missing: ${PONDER_REMOVED_UI_DEPENDENCY_TARGET_MANIFEST}")
endif()
ponder_require_absent_from_file("${PONDER_REMOVED_UI_DEPENDENCY_TARGET_MANIFEST}")

foreach(metadata_file IN ITEMS
        "${PONDER_SOURCE_DIR}/third_party/README.md"
        "${PONDER_SOURCE_DIR}/third_party/licenses.md")
    ponder_require_absent_from_file("${metadata_file}")
endforeach()

set(active_files)
foreach(root_file IN ITEMS
        "${PONDER_SOURCE_DIR}/.gitmodules"
        "${PONDER_SOURCE_DIR}/CMakeLists.txt"
        "${PONDER_SOURCE_DIR}/CMakePresets.json"
        "${PONDER_SOURCE_DIR}/LICENSE"
        "${PONDER_SOURCE_DIR}/LICENSE.md"
        "${PONDER_SOURCE_DIR}/NOTICE"
        "${PONDER_SOURCE_DIR}/NOTICE.md")
    if(EXISTS "${root_file}")
        list(APPEND active_files "${root_file}")
    endif()
endforeach()

foreach(active_root IN ITEMS apps cmake examples libs tests tools)
    if(EXISTS "${PONDER_SOURCE_DIR}/${active_root}")
        file(GLOB_RECURSE root_files LIST_DIRECTORIES false
            "${PONDER_SOURCE_DIR}/${active_root}/*.c"
            "${PONDER_SOURCE_DIR}/${active_root}/*.cc"
            "${PONDER_SOURCE_DIR}/${active_root}/*.cmake"
            "${PONDER_SOURCE_DIR}/${active_root}/*.cmake.in"
            "${PONDER_SOURCE_DIR}/${active_root}/*.cpp"
            "${PONDER_SOURCE_DIR}/${active_root}/*.cxx"
            "${PONDER_SOURCE_DIR}/${active_root}/*.h"
            "${PONDER_SOURCE_DIR}/${active_root}/*.hpp"
            "${PONDER_SOURCE_DIR}/${active_root}/*.hxx"
            "${PONDER_SOURCE_DIR}/${active_root}/*.md"
            "${PONDER_SOURCE_DIR}/${active_root}/CMakeLists.txt")
        list(APPEND active_files ${root_files})
    endif()
endforeach()

if(active_files)
    list(REMOVE_DUPLICATES active_files)
    list(SORT active_files)
endif()

foreach(active_file IN LISTS active_files)
    file(TO_CMAKE_PATH "${active_file}" normalized_file)
    if(normalized_file MATCHES "/docs/"
            OR normalized_file MATCHES "/third_party/"
            OR normalized_file MATCHES "/build[^/]*/")
        continue()
    endif()

    ponder_require_absent_from_file("${active_file}")
endforeach()

set(generated_verification_files
    "${PONDER_BINARY_DIR}/compile_commands.json"
    "${PONDER_BINARY_DIR}/cmake/ponderConfig.cmake"
    "${PONDER_BINARY_DIR}/cmake/ponderConfigVersion.cmake")
foreach(generated_file IN LISTS generated_verification_files)
    ponder_require_absent_from_file("${generated_file}")
endforeach()
