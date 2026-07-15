include_guard(GLOBAL)

function(_ponder_collect_build_targets directory output_variable)
    get_property(local_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    set(collected_targets ${local_targets})

    get_property(child_directories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child_directory IN LISTS child_directories)
        _ponder_collect_build_targets("${child_directory}" child_targets)
        list(APPEND collected_targets ${child_targets})
    endforeach()

    set("${output_variable}" ${collected_targets} PARENT_SCOPE)
endfunction()

function(ponder_add_removed_ui_dependency_absence_check)
    if(NOT BUILD_TESTING)
        return()
    endif()

    set(manifest_dir "${PROJECT_BINARY_DIR}/dependency-checks")
    set(target_manifest "${manifest_dir}/RemovedUiDependencyTargets.txt")
    file(MAKE_DIRECTORY "${manifest_dir}")

    _ponder_collect_build_targets("${PROJECT_SOURCE_DIR}" configured_targets)
    if(configured_targets)
        list(REMOVE_DUPLICATES configured_targets)
        list(SORT configured_targets)
    endif()

    set(manifest_contents "")
    foreach(configured_target IN LISTS configured_targets)
        string(APPEND manifest_contents "target=${configured_target}\n")
        foreach(target_property IN ITEMS
                ALIASED_TARGET
                COMPILE_DEFINITIONS
                INCLUDE_DIRECTORIES
                INTERFACE_COMPILE_DEFINITIONS
                INTERFACE_INCLUDE_DIRECTORIES
                INTERFACE_LINK_LIBRARIES
                INTERFACE_LINK_OPTIONS
                INTERFACE_SOURCES
                LINK_LIBRARIES
                LINK_OPTIONS
                SOURCES
                TYPE)
            get_target_property(property_value "${configured_target}" "${target_property}")
            if(property_value AND NOT property_value MATCHES "-NOTFOUND$")
                string(APPEND manifest_contents
                    "${configured_target}.${target_property}=${property_value}\n")
            endif()
        endforeach()
    endforeach()
    file(WRITE "${target_manifest}" "${manifest_contents}")

    add_test(NAME ponder_removed_ui_dependency_absence_check
        COMMAND "${CMAKE_COMMAND}"
            -D "PONDER_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -D "PONDER_BINARY_DIR=${PROJECT_BINARY_DIR}"
            -D "PONDER_REMOVED_UI_DEPENDENCY_TARGET_MANIFEST=${target_manifest}"
            -P "${PROJECT_SOURCE_DIR}/cmake/RemovedUiDependencyAbsenceCheck.cmake")
    set_tests_properties(ponder_removed_ui_dependency_absence_check PROPERTIES
        LABELS "ui;build;dependency;deterministic")
endfunction()