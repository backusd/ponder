function(_ponder_require_defined variable_name)
    if(NOT DEFINED ${variable_name} OR "${${variable_name}}" STREQUAL "")
        message(FATAL_ERROR "${variable_name} is required.")
    endif()
endfunction()

foreach(required_variable IN ITEMS
        PONDER_SHADER_SOURCE
        PONDER_SHADER_NAME
        PONDER_SHADER_ENTRY_POINT
        PONDER_SHADER_PROFILE
        PONDER_SHADER_STAGE
        PONDER_SHADER_SCHEMA_FINGERPRINT
        PONDER_SHADER_DXC_OPTIONS
        PONDER_SHADER_OUTPUT_HEADER
        PONDER_SHADER_OUTPUT_SOURCE
        PONDER_SHADER_OUTPUT_SPIRV
        PONDER_SHADER_OUTPUT_REFLECTION
        PONDER_SHADER_OUTPUT_MANIFEST
        PONDER_SHADER_OUTPUT_STAMP
        PONDER_SHADER_GENERATOR_SCRIPT
        PONDER_SHADER_CHECK_SCRIPT
        PONDER_SHADER_STALENESS_WORK_DIR)
    _ponder_require_defined("${required_variable}")
endforeach()

function(_ponder_run_hostile_check output_result output_text)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -D "PONDER_SHADER_DXC=${test_dxc}"
            -D "PONDER_SHADER_SPIRV_VAL=${test_spirv_val}"
            -D "PONDER_SHADER_SPIRV_CROSS=${test_spirv_cross}"
            -D "PONDER_SHADER_SOURCE=${test_source_input}"
            -D "PONDER_SHADER_NAME=${PONDER_SHADER_NAME}"
            -D "PONDER_SHADER_ENTRY_POINT=${PONDER_SHADER_ENTRY_POINT}"
            -D "PONDER_SHADER_PROFILE=${PONDER_SHADER_PROFILE}"
            -D "PONDER_SHADER_STAGE=${PONDER_SHADER_STAGE}"
            -D "PONDER_SHADER_SCHEMA_FINGERPRINT=${test_schema_fingerprint}"
            -D "PONDER_SHADER_DXC_OPTIONS=${test_dxc_options}"
            -D "PONDER_SHADER_OUTPUT_HEADER=${test_header}"
            -D "PONDER_SHADER_OUTPUT_SOURCE=${test_source}"
            -D "PONDER_SHADER_OUTPUT_SPIRV=${test_spirv}"
            -D "PONDER_SHADER_OUTPUT_REFLECTION=${test_reflection}"
            -D "PONDER_SHADER_OUTPUT_MANIFEST=${test_manifest}"
            -D "PONDER_SHADER_OUTPUT_STAMP=${test_stamp}"
            -D "PONDER_SHADER_GENERATOR_SCRIPT=${PONDER_SHADER_GENERATOR_SCRIPT}"
            -D "PONDER_SHADER_CHECK_SCRIPT=${PONDER_SHADER_CHECK_SCRIPT}"
            -P "${PONDER_SHADER_CHECK_SCRIPT}"
        RESULT_VARIABLE check_result
        OUTPUT_VARIABLE check_stdout
        ERROR_VARIABLE check_stderr)
    set("${output_result}" "${check_result}" PARENT_SCOPE)
    set("${output_text}" "${check_stdout}\n${check_stderr}" PARENT_SCOPE)
endfunction()

function(_ponder_expect_check_failure label)
    _ponder_run_hostile_check(check_result check_text)
    if(check_result EQUAL 0)
        message(FATAL_ERROR
            "Hostile shader artifact case unexpectedly passed: ${label}")
    endif()
endfunction()

file(MAKE_DIRECTORY "${PONDER_SHADER_STALENESS_WORK_DIR}")
set(test_header "${PONDER_SHADER_STALENESS_WORK_DIR}/Shader.hpp")
set(test_source "${PONDER_SHADER_STALENESS_WORK_DIR}/Shader.cpp")
set(test_spirv "${PONDER_SHADER_STALENESS_WORK_DIR}/Shader.spv")
set(test_reflection "${PONDER_SHADER_STALENESS_WORK_DIR}/Shader.reflection.json")
set(test_manifest "${PONDER_SHADER_STALENESS_WORK_DIR}/Shader.manifest.json")
set(test_stamp "${PONDER_SHADER_STALENESS_WORK_DIR}/Shader.stamp")
set(test_source_input "${PONDER_SHADER_SOURCE}")
set(test_dxc "${PONDER_SHADER_DXC}")
set(test_spirv_val "${PONDER_SHADER_SPIRV_VAL}")
set(test_spirv_cross "${PONDER_SHADER_SPIRV_CROSS}")
set(test_schema_fingerprint "${PONDER_SHADER_SCHEMA_FINGERPRINT}")
set(test_dxc_options "${PONDER_SHADER_DXC_OPTIONS}")

function(_ponder_run_hostile_generate)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -D "PONDER_SHADER_DXC=${test_dxc}"
            -D "PONDER_SHADER_SPIRV_VAL=${test_spirv_val}"
            -D "PONDER_SHADER_SPIRV_CROSS=${test_spirv_cross}"
            -D "PONDER_SHADER_SOURCE=${test_source_input}"
            -D "PONDER_SHADER_NAME=${PONDER_SHADER_NAME}"
            -D "PONDER_SHADER_ENTRY_POINT=${PONDER_SHADER_ENTRY_POINT}"
            -D "PONDER_SHADER_PROFILE=${PONDER_SHADER_PROFILE}"
            -D "PONDER_SHADER_STAGE=${PONDER_SHADER_STAGE}"
            -D "PONDER_SHADER_SCHEMA_FINGERPRINT=${test_schema_fingerprint}"
            -D "PONDER_SHADER_DXC_OPTIONS=${test_dxc_options}"
            -D "PONDER_SHADER_OUTPUT_HEADER=${test_header}"
            -D "PONDER_SHADER_OUTPUT_SOURCE=${test_source}"
            -D "PONDER_SHADER_OUTPUT_SPIRV=${test_spirv}"
            -D "PONDER_SHADER_OUTPUT_REFLECTION=${test_reflection}"
            -D "PONDER_SHADER_OUTPUT_MANIFEST=${test_manifest}"
            -D "PONDER_SHADER_OUTPUT_STAMP=${test_stamp}"
            -D "PONDER_SHADER_GENERATOR_SCRIPT=${PONDER_SHADER_GENERATOR_SCRIPT}"
            -D "PONDER_SHADER_CHECK_SCRIPT=${PONDER_SHADER_CHECK_SCRIPT}"
            -P "${PONDER_SHADER_GENERATOR_SCRIPT}"
        RESULT_VARIABLE generate_result
        OUTPUT_VARIABLE generate_stdout
        ERROR_VARIABLE generate_stderr)
    if(NOT generate_result EQUAL 0)
        message(FATAL_ERROR
            "Hostile shader artifact setup failed to generate scratch artifacts.\n"
            "${generate_stdout}\n${generate_stderr}")
    endif()
endfunction()

macro(_ponder_restore_artifacts)
    file(COPY_FILE "${PONDER_SHADER_OUTPUT_HEADER}" "${test_header}")
    file(COPY_FILE "${PONDER_SHADER_OUTPUT_SOURCE}" "${test_source}")
    file(COPY_FILE "${PONDER_SHADER_OUTPUT_SPIRV}" "${test_spirv}")
    file(COPY_FILE "${PONDER_SHADER_OUTPUT_REFLECTION}" "${test_reflection}")
    file(COPY_FILE "${PONDER_SHADER_OUTPUT_MANIFEST}" "${test_manifest}")
    file(COPY_FILE "${PONDER_SHADER_OUTPUT_STAMP}" "${test_stamp}")
endmacro()

_ponder_restore_artifacts()
_ponder_run_hostile_check(baseline_result baseline_text)
if(NOT baseline_result EQUAL 0)
    message(FATAL_ERROR
        "Copied shader artifact failed the baseline hostile-check setup.\n${baseline_text}")
endif()

file(APPEND "${test_header}" "\n// hostile stale header\n")
_ponder_expect_check_failure("generated header content")
_ponder_restore_artifacts()

file(APPEND "${test_source}" "\n// hostile stale source\n")
_ponder_expect_check_failure("generated source content")
_ponder_restore_artifacts()

file(APPEND "${test_spirv}" "x")
_ponder_expect_check_failure("SPIR-V content")
_ponder_restore_artifacts()

file(APPEND "${test_reflection}" " ")
_ponder_expect_check_failure("reflection content")
_ponder_restore_artifacts()

file(APPEND "${test_manifest}" "{")
_ponder_expect_check_failure("strict manifest JSON")
_ponder_restore_artifacts()

file(APPEND "${test_stamp}" "stale")
_ponder_expect_check_failure("input signature stamp")
_ponder_restore_artifacts()

set(test_schema_fingerprint "${PONDER_SHADER_SCHEMA_FINGERPRINT}-hostile")
_ponder_expect_check_failure("schema fingerprint")
set(test_schema_fingerprint "${PONDER_SHADER_SCHEMA_FINGERPRINT}")

set(test_dxc_options "${PONDER_SHADER_DXC_OPTIONS}|-O0")
_ponder_expect_check_failure("compiler options")
set(test_dxc_options "${PONDER_SHADER_DXC_OPTIONS}")

set(hostile_source "${PONDER_SHADER_STALENESS_WORK_DIR}/HostileSource.hlsl")
file(COPY_FILE "${PONDER_SHADER_SOURCE}" "${hostile_source}")
set(test_source_input "${hostile_source}")
_ponder_expect_check_failure("source identity")
set(test_source_input "${PONDER_SHADER_SOURCE}")

set(hostile_source_content "${PONDER_SHADER_STALENESS_WORK_DIR}/HostileSourceContent.hlsl")
file(COPY_FILE "${PONDER_SHADER_SOURCE}" "${hostile_source_content}")
set(test_source_input "${hostile_source_content}")
_ponder_run_hostile_generate()
file(APPEND "${hostile_source_content}" "\n// hostile stale source content\n")
_ponder_expect_check_failure("source content")
set(test_source_input "${PONDER_SHADER_SOURCE}")
_ponder_restore_artifacts()

# A different existing file proves that the checker binds provenance to the selected executable
# path before it ever attempts to invoke a substituted tool.
set(test_dxc "${PONDER_SHADER_OUTPUT_SPIRV}")
_ponder_expect_check_failure("tool identity")
set(test_dxc "${PONDER_SHADER_DXC}")

file(REMOVE
    "${test_header}"
    "${test_source}"
    "${test_spirv}"
    "${test_reflection}"
    "${test_manifest}"
    "${test_stamp}"
    "${hostile_source}"
    "${hostile_source_content}")
message(STATUS
    "Hostile shader artifact checks passed for ${PONDER_SHADER_NAME}.")

