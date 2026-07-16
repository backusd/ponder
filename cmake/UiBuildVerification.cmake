include_guard(GLOBAL)

function(_ponder_collect_ui_verification_targets directory output_variable)
    get_property(local_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    set(collected_targets ${local_targets})

    get_property(child_directories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child_directory IN LISTS child_directories)
        _ponder_collect_ui_verification_targets("${child_directory}" child_targets)
        list(APPEND collected_targets ${child_targets})
    endforeach()

    set("${output_variable}" ${collected_targets} PARENT_SCOPE)
endfunction()

function(ponder_add_ui_build_verification)
    if(NOT BUILD_TESTING)
        return()
    endif()

    set(manifest_dir "${PROJECT_BINARY_DIR}/ui-build-verification")
    set(target_manifest "${manifest_dir}/ConfiguredTargets.txt")
    file(MAKE_DIRECTORY "${manifest_dir}")

    _ponder_collect_ui_verification_targets("${PROJECT_SOURCE_DIR}" configured_targets)
    if(configured_targets)
        list(REMOVE_DUPLICATES configured_targets)
        list(SORT configured_targets)
    endif()

    string(CONCAT manifest_contents
        "PONDER_BUILD_RENDER=${PONDER_BUILD_RENDER}\n"
        "PONDER_BUILD_UI_RENDER_INTEGRATION=${PONDER_BUILD_UI_RENDER_INTEGRATION}\n"
        "PONDER_ENABLE_INSTALL_RULES=${PONDER_ENABLE_INSTALL_RULES}\n")
    foreach(configured_target IN LISTS configured_targets)
        string(APPEND manifest_contents "target=${configured_target}\n")
        foreach(target_property IN ITEMS
                COMPILE_DEFINITIONS
                COMPILE_OPTIONS
                EXPORT_NAME
                HEADER_SET
                INCLUDE_DIRECTORIES
                INTERFACE_COMPILE_DEFINITIONS
                INTERFACE_COMPILE_OPTIONS
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
                string(REPLACE "\n" "<newline>" property_value "${property_value}")
                string(APPEND manifest_contents
                    "${configured_target}.${target_property}=${property_value}\n")
            endif()
        endforeach()
    endforeach()
    file(WRITE "${target_manifest}" "${manifest_contents}")

    set(require_compile_commands OFF)
    if(CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
        set(require_compile_commands ON)
    endif()

    add_test(NAME ponder_ui_build_topology_verification
        COMMAND "${CMAKE_COMMAND}"
            -D "PONDER_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -D "PONDER_BINARY_DIR=${PROJECT_BINARY_DIR}"
            -D "PONDER_TARGET_MANIFEST=${target_manifest}"
            -D "PONDER_EXPECT_RENDER=$<BOOL:${PONDER_BUILD_RENDER}>"
            -D
                "PONDER_EXPECT_UI_RENDER_INTEGRATION=$<BOOL:${PONDER_BUILD_UI_RENDER_INTEGRATION}>"
            -D "PONDER_EXPECT_INSTALL_RULES=$<BOOL:${PONDER_ENABLE_INSTALL_RULES}>"
            -D "PONDER_REQUIRE_COMPILE_COMMANDS=$<BOOL:${require_compile_commands}>"
            -P "${PROJECT_SOURCE_DIR}/cmake/UiBuildVerificationCheck.cmake")
    set_tests_properties(ponder_ui_build_topology_verification PROPERTIES
        LABELS
            "ui;build;headers;package;dependency;topology;deterministic;ui_build_package")
endfunction()
