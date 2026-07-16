set(PONDER_UI_BUILD_MATRIX_CHILD OFF CACHE BOOL
    "Suppress the parent clean UI build matrix inside matrix child builds.")
mark_as_advanced(PONDER_UI_BUILD_MATRIX_CHILD)

if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND
   CMAKE_GENERATOR MATCHES "^Ninja" AND
   NOT PONDER_UI_BUILD_MATRIX_CHILD)
    set(ponder_ui_build_matrix_command
        "${CMAKE_COMMAND}"
        -D "PONDER_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -D "PONDER_MATRIX_BINARY_ROOT=${PROJECT_BINARY_DIR}/ui-build-matrix"
        -D "PONDER_MATRIX_GENERATOR=${CMAKE_GENERATOR}"
        -D "PONDER_MATRIX_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        -D "PONDER_MATRIX_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        -D "PONDER_MATRIX_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        -D "PONDER_MATRIX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}"
        -D "PONDER_MATRIX_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
        -D "PONDER_MATRIX_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        -D "PONDER_RENDER_SHADER_DXC=${PONDER_RENDER_SHADER_DXC}"
        -D "PONDER_RENDER_SHADER_SPIRV_VAL=${PONDER_RENDER_SHADER_SPIRV_VAL}"
        -D "PONDER_RENDER_SHADER_SPIRV_CROSS=${PONDER_RENDER_SHADER_SPIRV_CROSS}"
        -D "PONDER_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/RunUiBuildMatrix.cmake")

    add_test(NAME ponder_ui_clean_windows_build_matrix
        COMMAND ${ponder_ui_build_matrix_command})
    set_tests_properties(ponder_ui_clean_windows_build_matrix PROPERTIES
        LABELS
            "ui;render;build;headers;install;consumer;package;dependency;matrix;windows;ui_build_package"
        RESOURCE_LOCK ponder_nested_build
        TIMEOUT 7200)
endif()
