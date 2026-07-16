include_guard(GLOBAL)

set(PONDER_RENDER_SHADER_DXC "" CACHE FILEPATH
    "Path to a DirectXShaderCompiler dxc executable built with SPIR-V CodeGen enabled.")
set(PONDER_RENDER_SHADER_SPIRV_VAL "" CACHE FILEPATH
    "Path to the SPIRV-Tools spirv-val executable.")
set(PONDER_RENDER_SHADER_SPIRV_CROSS "" CACHE FILEPATH
    "Path to the SPIRV-Cross executable used for JSON reflection.")

function(ponder_add_render_shader)
    set(options)
    set(one_value_args
        TARGET
        SHADER_NAME
        SOURCE
        ENTRY_POINT
        PROFILE
        STAGE
        SCHEMA_FINGERPRINT)
    cmake_parse_arguments(PONDER_SHADER
        "${options}"
        "${one_value_args}"
        ""
        ${ARGN})

    foreach(required_arg IN ITEMS
            TARGET SHADER_NAME SOURCE ENTRY_POINT PROFILE STAGE SCHEMA_FINGERPRINT)
        if(NOT PONDER_SHADER_${required_arg})
            message(FATAL_ERROR "ponder_add_render_shader missing ${required_arg}.")
        endif()
    endforeach()

    if(NOT PONDER_SHADER_SHADER_NAME MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
        message(FATAL_ERROR "Render shader names must be valid C++ identifiers.")
    endif()
    if(NOT PONDER_SHADER_ENTRY_POINT MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
        message(FATAL_ERROR "Render shader entry points must be valid identifiers.")
    endif()
    if(NOT PONDER_SHADER_PROFILE MATCHES "^[a-z][a-z]_[0-9]+_[0-9]+$")
        message(FATAL_ERROR "Render shader profiles must use the dxc stage_major_minor form.")
    endif()
    if(NOT PONDER_SHADER_STAGE MATCHES
       "^(vertex|fragment|compute|geometry|tessellation_control|tessellation_evaluation)$")
        message(FATAL_ERROR "Unsupported render shader stage: ${PONDER_SHADER_STAGE}")
    endif()

    if(NOT IS_ABSOLUTE "${PONDER_SHADER_SOURCE}")
        set(PONDER_SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${PONDER_SHADER_SOURCE}")
    endif()
    cmake_path(NORMAL_PATH PONDER_SHADER_SOURCE)

    set(generated_dir
        "${PROJECT_BINARY_DIR}/generated/render/shaders/${PONDER_SHADER_SHADER_NAME}")
    set(generated_header "${generated_dir}/${PONDER_SHADER_SHADER_NAME}Shader.hpp")
    set(generated_source "${generated_dir}/${PONDER_SHADER_SHADER_NAME}Shader.cpp")
    set(generated_spirv "${generated_dir}/${PONDER_SHADER_SHADER_NAME}.spv")
    set(generated_reflection "${generated_dir}/${PONDER_SHADER_SHADER_NAME}.reflection.json")
    set(generated_manifest "${generated_dir}/${PONDER_SHADER_SHADER_NAME}.manifest.json")
    set(generated_stamp "${generated_dir}/${PONDER_SHADER_SHADER_NAME}.stamp")
    set(generator_script "${PROJECT_SOURCE_DIR}/cmake/GenerateRenderShaderArtifact.cmake")
    set(check_script "${PROJECT_SOURCE_DIR}/cmake/CheckRenderShaderArtifact.cmake")
    set(staleness_test_script
        "${PROJECT_SOURCE_DIR}/cmake/TestRenderShaderArtifactStaleness.cmake")

    set(dxc_options
        -spirv
        -fspv-target-env=vulkan1.2
        -HV
        2021
        -Ges
        -O3)
    list(JOIN dxc_options "|" dxc_options_arg)

    set(shader_arguments
        -D "PONDER_SHADER_DXC=${PONDER_RENDER_SHADER_DXC}"
        -D "PONDER_SHADER_SPIRV_VAL=${PONDER_RENDER_SHADER_SPIRV_VAL}"
        -D "PONDER_SHADER_SPIRV_CROSS=${PONDER_RENDER_SHADER_SPIRV_CROSS}"
        -D "PONDER_SHADER_SOURCE=${PONDER_SHADER_SOURCE}"
        -D "PONDER_SHADER_NAME=${PONDER_SHADER_SHADER_NAME}"
        -D "PONDER_SHADER_ENTRY_POINT=${PONDER_SHADER_ENTRY_POINT}"
        -D "PONDER_SHADER_PROFILE=${PONDER_SHADER_PROFILE}"
        -D "PONDER_SHADER_STAGE=${PONDER_SHADER_STAGE}"
        -D "PONDER_SHADER_SCHEMA_FINGERPRINT=${PONDER_SHADER_SCHEMA_FINGERPRINT}"
        -D "PONDER_SHADER_DXC_OPTIONS=${dxc_options_arg}"
        -D "PONDER_SHADER_OUTPUT_HEADER=${generated_header}"
        -D "PONDER_SHADER_OUTPUT_SOURCE=${generated_source}"
        -D "PONDER_SHADER_OUTPUT_SPIRV=${generated_spirv}"
        -D "PONDER_SHADER_OUTPUT_REFLECTION=${generated_reflection}"
        -D "PONDER_SHADER_OUTPUT_MANIFEST=${generated_manifest}"
        -D "PONDER_SHADER_OUTPUT_STAMP=${generated_stamp}"
        -D "PONDER_SHADER_GENERATOR_SCRIPT=${generator_script}"
        -D "PONDER_SHADER_CHECK_SCRIPT=${check_script}")

    # Run whenever the library is requested. The script fingerprints fully preprocessed source,
    # tool binaries/versions, options, schema, and its implementation, then returns without
    # rewriting artifacts when the complete signature is unchanged. This catches tool replacement
    # and transitive include changes while preserving build-time-only tool discovery.
    set(generate_target "${PONDER_SHADER_TARGET}_artifact_generate")
    add_custom_target("${generate_target}"
        COMMAND "${CMAKE_COMMAND}" ${shader_arguments} -P "${generator_script}"
        BYPRODUCTS
            "${generated_header}"
            "${generated_source}"
            "${generated_spirv}"
            "${generated_reflection}"
            "${generated_manifest}"
            "${generated_stamp}"
        DEPENDS
            "${PONDER_SHADER_SOURCE}"
            "${generator_script}"
            "${check_script}"
        COMMENT "Verifying or generating render shader artifact ${PONDER_SHADER_SHADER_NAME}"
        VERBATIM)
    set_target_properties("${generate_target}" PROPERTIES
        FOLDER "libs/render/generated")

    set_source_files_properties(
        "${generated_header}"
        "${generated_source}"
        "${generated_spirv}"
        "${generated_reflection}"
        "${generated_manifest}"
        "${generated_stamp}"
        PROPERTIES GENERATED TRUE)

    add_library("${PONDER_SHADER_TARGET}" STATIC EXCLUDE_FROM_ALL
        "${generated_source}"
        "${generated_header}")
    add_dependencies("${PONDER_SHADER_TARGET}" "${generate_target}")
    if(PONDER_SHADER_TARGET MATCHES "^ponder_(.+)$")
        add_library("ponder::${CMAKE_MATCH_1}" ALIAS "${PONDER_SHADER_TARGET}")
    else()
        add_library("ponder::${PONDER_SHADER_TARGET}" ALIAS "${PONDER_SHADER_TARGET}")
    endif()
    target_compile_features("${PONDER_SHADER_TARGET}" PRIVATE cxx_std_23)
    target_include_directories("${PONDER_SHADER_TARGET}" PUBLIC
        "${generated_dir}")
    set_target_properties("${PONDER_SHADER_TARGET}" PROPERTIES
        DISABLE_PRECOMPILE_HEADERS ON
        FOLDER "libs/render/generated"
        POSITION_INDEPENDENT_CODE ON)

    set(check_target "${PONDER_SHADER_TARGET}_artifact_check")
    add_custom_target("${check_target}"
        COMMAND "${CMAKE_COMMAND}" ${shader_arguments} -P "${check_script}"
        DEPENDS "${PONDER_SHADER_TARGET}"
        COMMENT "Checking render shader artifact ${PONDER_SHADER_SHADER_NAME}"
        VERBATIM)
    set_target_properties("${check_target}" PROPERTIES
        FOLDER "libs/render/generated")

    set(staleness_target "${PONDER_SHADER_TARGET}_artifact_staleness_check")
    add_custom_target("${staleness_target}"
        COMMAND "${CMAKE_COMMAND}" ${shader_arguments}
            -D "PONDER_SHADER_STALENESS_WORK_DIR=${generated_dir}/staleness-check"
            -P "${staleness_test_script}"
        DEPENDS
            "${PONDER_SHADER_TARGET}"
            "${staleness_test_script}"
        COMMENT "Exercising hostile shader artifact checks for ${PONDER_SHADER_SHADER_NAME}"
        VERBATIM)
    set_target_properties("${staleness_target}" PROPERTIES
        FOLDER "libs/render/generated")
endfunction()
