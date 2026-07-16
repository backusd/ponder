function(_ponder_require_defined variable_name)
    if(NOT DEFINED ${variable_name} OR "${${variable_name}}" STREQUAL "")
        message(FATAL_ERROR "${variable_name} is required.")
    endif()
endfunction()

function(_ponder_normalize_path output_variable input_path)
    get_filename_component(normalized_path "${input_path}" ABSOLUTE)
    file(TO_CMAKE_PATH "${normalized_path}" normalized_path)
    set("${output_variable}" "${normalized_path}" PARENT_SCOPE)
endfunction()

function(_ponder_json_get output_variable json_text)
    string(JSON json_value ERROR_VARIABLE json_error GET "${json_text}" ${ARGN})
    if(NOT json_error STREQUAL "NOTFOUND")
        message(FATAL_ERROR "Invalid shader manifest JSON at '${ARGN}': ${json_error}")
    endif()
    set("${output_variable}" "${json_value}" PARENT_SCOPE)
endfunction()

function(_ponder_expect_equal label actual expected)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR
            "Shader artifact ${label} mismatch.\nExpected: ${expected}\nActual: ${actual}")
    endif()
endfunction()

function(_ponder_cpp_escape output_variable input_value)
    set(escaped "${input_value}")
    string(REPLACE "\\" "\\\\" escaped "${escaped}")
    string(REPLACE "\"" "\\\"" escaped "${escaped}")
    string(REPLACE "\n" "\\n" escaped "${escaped}")
    string(REPLACE "\r" "\\r" escaped "${escaped}")
    string(REPLACE "\t" "\\t" escaped "${escaped}")
    set("${output_variable}" "${escaped}" PARENT_SCOPE)
endfunction()

function(_ponder_tool_version output_variable tool_path tool_kind)
    execute_process(
        COMMAND "${tool_path}" --version
        RESULT_VARIABLE version_result
        OUTPUT_VARIABLE version_stdout
        ERROR_VARIABLE version_stderr
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE)
    string(REPLACE "\r\n" "\n" version_stdout "${version_stdout}")
    string(REPLACE "\r\n" "\n" version_stderr "${version_stderr}")

    if(tool_kind STREQUAL "spirv-cross")
        set(version_text "${version_stdout}\n${version_stderr}")
        string(REGEX MATCH "Git commit:[^\r\n]*" version_text "${version_text}")
        if(version_text STREQUAL "")
            message(FATAL_ERROR
                "SPIRV-Cross did not report a recognizable Git commit identity.")
        endif()
    else()
        if(NOT version_result EQUAL 0)
            message(FATAL_ERROR
                "${tool_kind} --version failed with exit code ${version_result}.\n"
                "stdout:\n${version_stdout}\nstderr:\n${version_stderr}")
        endif()
        set(version_text "${version_stdout}")
        if(version_text STREQUAL "")
            set(version_text "${version_stderr}")
        endif()
        if(version_text STREQUAL "")
            message(FATAL_ERROR "${tool_kind} did not report a version.")
        endif()
    endif()

    string(STRIP "${version_text}" version_text)
    set("${output_variable}" "${version_text}" PARENT_SCOPE)
endfunction()

function(_ponder_select_tool output_variable provided_path manifest_path display_name)
    _ponder_normalize_path(manifest_normalized "${manifest_path}")
    if(NOT "${provided_path}" STREQUAL "")
        _ponder_normalize_path(provided_normalized "${provided_path}")
        _ponder_expect_equal("${display_name} path" "${provided_normalized}"
                             "${manifest_normalized}")
    endif()
    if(NOT EXISTS "${manifest_normalized}" OR IS_DIRECTORY "${manifest_normalized}")
        message(FATAL_ERROR
            "Recorded ${display_name} executable is missing: ${manifest_normalized}")
    endif()
    set("${output_variable}" "${manifest_normalized}" PARENT_SCOPE)
endfunction()

function(_ponder_compute_contract_hashes output_options_sha output_schema_sha output_input_sha
         source_path source_sha preprocessed_sha dxc_path dxc_sha dxc_version
         spirv_val_path spirv_val_sha spirv_val_version spirv_cross_path spirv_cross_sha
         spirv_cross_version generator_sha check_sha)
    string(SHA256 options_sha "${PONDER_SHADER_DXC_OPTIONS}")
    set(schema_contract
        "shaderName=${PONDER_SHADER_NAME}\n"
        "entryPoint=${PONDER_SHADER_ENTRY_POINT}\n"
        "profile=${PONDER_SHADER_PROFILE}\n"
        "stage=${PONDER_SHADER_STAGE}\n"
        "targetEnvironment=vulkan1.2\n"
        "schemaFingerprint=${PONDER_SHADER_SCHEMA_FINGERPRINT}\n")
    string(SHA256 schema_sha "${schema_contract}")
    set(input_contract
        "source=${source_path}\n"
        "sourceSha256=${source_sha}\n"
        "preprocessedSourceSha256=${preprocessed_sha}\n"
        "dxcOptionsSha256=${options_sha}\n"
        "schemaSha256=${schema_sha}\n"
        "dxcPath=${dxc_path}\n"
        "dxcSha256=${dxc_sha}\n"
        "dxcVersion=${dxc_version}\n"
        "spirvValPath=${spirv_val_path}\n"
        "spirvValSha256=${spirv_val_sha}\n"
        "spirvValVersion=${spirv_val_version}\n"
        "spirvCrossPath=${spirv_cross_path}\n"
        "spirvCrossSha256=${spirv_cross_sha}\n"
        "spirvCrossVersion=${spirv_cross_version}\n"
        "generatorSha256=${generator_sha}\n"
        "checkSha256=${check_sha}\n")
    string(SHA256 input_sha "${input_contract}")
    set("${output_options_sha}" "${options_sha}" PARENT_SCOPE)
    set("${output_schema_sha}" "${schema_sha}" PARENT_SCOPE)
    set("${output_input_sha}" "${input_sha}" PARENT_SCOPE)
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
        PONDER_SHADER_CHECK_SCRIPT)
    _ponder_require_defined("${required_variable}")
endforeach()

foreach(required_file IN ITEMS
        "${PONDER_SHADER_SOURCE}"
        "${PONDER_SHADER_OUTPUT_HEADER}"
        "${PONDER_SHADER_OUTPUT_SOURCE}"
        "${PONDER_SHADER_OUTPUT_SPIRV}"
        "${PONDER_SHADER_OUTPUT_REFLECTION}"
        "${PONDER_SHADER_OUTPUT_MANIFEST}"
        "${PONDER_SHADER_OUTPUT_STAMP}"
        "${PONDER_SHADER_GENERATOR_SCRIPT}"
        "${PONDER_SHADER_CHECK_SCRIPT}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Expected shader input or artifact is missing: ${required_file}")
    endif()
endforeach()

file(READ "${PONDER_SHADER_OUTPUT_MANIFEST}" manifest_json)
string(STRIP "${manifest_json}" manifest_json_stripped)
string(LENGTH "${manifest_json_stripped}" manifest_json_length)
if(manifest_json_length LESS 2)
    message(FATAL_ERROR "Shader manifest JSON is empty or truncated.")
endif()
string(SUBSTRING "${manifest_json_stripped}" 0 1 manifest_first_character)
math(EXPR manifest_last_index "${manifest_json_length} - 1")
string(SUBSTRING "${manifest_json_stripped}" ${manifest_last_index} 1
       manifest_last_character)
if(NOT manifest_first_character STREQUAL "{" OR
   NOT manifest_last_character STREQUAL "}")
    message(FATAL_ERROR "Shader manifest contains leading or trailing non-JSON content.")
endif()
foreach(control_code RANGE 1 31)
    if(control_code EQUAL 10 OR control_code EQUAL 13)
        continue()
    endif()
    string(ASCII ${control_code} control_character)
    string(FIND "${manifest_json}" "${control_character}" control_index)
    if(NOT control_index EQUAL -1)
        message(FATAL_ERROR
            "Shader manifest contains unescaped ASCII control code ${control_code}.")
    endif()
endforeach()
_ponder_json_get(manifest_schema_version "${manifest_json}" schemaVersion)
_ponder_expect_equal("manifest schema version" "${manifest_schema_version}" "2")

_ponder_json_get(manifest_dxc_path "${manifest_json}" dxc path)
_ponder_json_get(manifest_spirv_val_path "${manifest_json}" spirvVal path)
_ponder_json_get(manifest_spirv_cross_path "${manifest_json}" spirvCross path)
_ponder_select_tool(dxc_path "${PONDER_SHADER_DXC}" "${manifest_dxc_path}" "dxc")
_ponder_select_tool(spirv_val_path "${PONDER_SHADER_SPIRV_VAL}"
                    "${manifest_spirv_val_path}" "spirv-val")
_ponder_select_tool(spirv_cross_path "${PONDER_SHADER_SPIRV_CROSS}"
                    "${manifest_spirv_cross_path}" "spirv-cross")

_ponder_normalize_path(source_path "${PONDER_SHADER_SOURCE}")
file(SHA256 "${source_path}" source_sha256)
file(SHA256 "${dxc_path}" dxc_sha256)
file(SHA256 "${spirv_val_path}" spirv_val_sha256)
file(SHA256 "${spirv_cross_path}" spirv_cross_sha256)
file(SHA256 "${PONDER_SHADER_GENERATOR_SCRIPT}" generator_sha256)
file(SHA256 "${PONDER_SHADER_CHECK_SCRIPT}" check_sha256)
_ponder_tool_version(dxc_version "${dxc_path}" "dxc")
_ponder_tool_version(spirv_val_version "${spirv_val_path}" "spirv-val")
_ponder_tool_version(spirv_cross_version "${spirv_cross_path}" "spirv-cross")

if(DEFINED PONDER_SHADER_PREPROCESSED_SHA256 AND
   NOT PONDER_SHADER_PREPROCESSED_SHA256 STREQUAL "")
    set(preprocessed_sha256 "${PONDER_SHADER_PREPROCESSED_SHA256}")
else()
    string(REPLACE "|" ";" dxc_options "${PONDER_SHADER_DXC_OPTIONS}")
    set(preprocessed_file "${PONDER_SHADER_OUTPUT_STAMP}.artifact-check.preprocessed.tmp")
    file(REMOVE "${preprocessed_file}")
    execute_process(
        COMMAND "${dxc_path}" ${dxc_options}
                -P -Fi "${preprocessed_file}"
                -T "${PONDER_SHADER_PROFILE}"
                -E "${PONDER_SHADER_ENTRY_POINT}"
                "${source_path}"
        RESULT_VARIABLE preprocess_result
        OUTPUT_VARIABLE preprocess_stdout
        ERROR_VARIABLE preprocess_stderr)
    if(NOT preprocess_result EQUAL 0 OR NOT EXISTS "${preprocessed_file}")
        message(FATAL_ERROR
            "DXC preprocessing failed while checking ${PONDER_SHADER_NAME}.\n"
            "stdout:\n${preprocess_stdout}\nstderr:\n${preprocess_stderr}")
    endif()
    file(SHA256 "${preprocessed_file}" preprocessed_sha256)
    file(REMOVE "${preprocessed_file}")
endif()

_ponder_compute_contract_hashes(
    options_sha256 schema_sha256 input_sha256
    "${source_path}" "${source_sha256}" "${preprocessed_sha256}"
    "${dxc_path}" "${dxc_sha256}" "${dxc_version}"
    "${spirv_val_path}" "${spirv_val_sha256}" "${spirv_val_version}"
    "${spirv_cross_path}" "${spirv_cross_sha256}" "${spirv_cross_version}"
    "${generator_sha256}" "${check_sha256}")

foreach(manifest_field IN ITEMS
        shaderName entryPoint profile stage schemaFingerprint targetEnvironment source
        sourceSha256 preprocessedSourceSha256 dxcOptionsSha256 schemaSha256 inputSha256
        generatorSha256 checkSha256)
    _ponder_json_get(manifest_${manifest_field} "${manifest_json}" "${manifest_field}")
endforeach()
_ponder_expect_equal("shader name" "${manifest_shaderName}" "${PONDER_SHADER_NAME}")
_ponder_expect_equal("entry point" "${manifest_entryPoint}" "${PONDER_SHADER_ENTRY_POINT}")
_ponder_expect_equal("profile" "${manifest_profile}" "${PONDER_SHADER_PROFILE}")
_ponder_expect_equal("stage" "${manifest_stage}" "${PONDER_SHADER_STAGE}")
_ponder_expect_equal("schema fingerprint" "${manifest_schemaFingerprint}"
                     "${PONDER_SHADER_SCHEMA_FINGERPRINT}")
_ponder_expect_equal("target environment" "${manifest_targetEnvironment}" "vulkan1.2")
_ponder_expect_equal("source path" "${manifest_source}" "${source_path}")
_ponder_expect_equal("source hash" "${manifest_sourceSha256}" "${source_sha256}")
_ponder_expect_equal("preprocessed source hash" "${manifest_preprocessedSourceSha256}"
                     "${preprocessed_sha256}")
_ponder_expect_equal("options hash" "${manifest_dxcOptionsSha256}" "${options_sha256}")
_ponder_expect_equal("schema hash" "${manifest_schemaSha256}" "${schema_sha256}")
_ponder_expect_equal("input hash" "${manifest_inputSha256}" "${input_sha256}")
_ponder_expect_equal("generator hash" "${manifest_generatorSha256}" "${generator_sha256}")
_ponder_expect_equal("check hash" "${manifest_checkSha256}" "${check_sha256}")

foreach(tool_name IN ITEMS dxc spirvVal spirvCross)
    _ponder_json_get(manifest_${tool_name}_sha "${manifest_json}" "${tool_name}" sha256)
    _ponder_json_get(manifest_${tool_name}_version "${manifest_json}" "${tool_name}" version)
endforeach()
_ponder_expect_equal("dxc hash" "${manifest_dxc_sha}" "${dxc_sha256}")
_ponder_expect_equal("dxc version" "${manifest_dxc_version}" "${dxc_version}")
_ponder_expect_equal("spirv-val hash" "${manifest_spirvVal_sha}" "${spirv_val_sha256}")
_ponder_expect_equal("spirv-val version" "${manifest_spirvVal_version}" "${spirv_val_version}")
_ponder_expect_equal("spirv-cross hash" "${manifest_spirvCross_sha}" "${spirv_cross_sha256}")
_ponder_expect_equal("spirv-cross version" "${manifest_spirvCross_version}"
                     "${spirv_cross_version}")

string(REPLACE "|" ";" expected_dxc_options "${PONDER_SHADER_DXC_OPTIONS}")
list(LENGTH expected_dxc_options expected_option_count)
string(JSON manifest_option_count ERROR_VARIABLE options_error
       LENGTH "${manifest_json}" dxcOptions)
if(NOT options_error STREQUAL "NOTFOUND")
    message(FATAL_ERROR "Shader manifest dxcOptions is invalid: ${options_error}")
endif()
_ponder_expect_equal("dxc option count" "${manifest_option_count}" "${expected_option_count}")
if(expected_option_count GREATER 0)
    math(EXPR last_option_index "${expected_option_count} - 1")
    foreach(option_index RANGE 0 ${last_option_index})
        list(GET expected_dxc_options ${option_index} expected_option)
        _ponder_json_get(manifest_option "${manifest_json}" dxcOptions ${option_index})
        _ponder_expect_equal("dxc option ${option_index}" "${manifest_option}"
                             "${expected_option}")
    endforeach()
endif()

file(SHA256 "${PONDER_SHADER_OUTPUT_SPIRV}" spirv_sha256)
file(SHA256 "${PONDER_SHADER_OUTPUT_REFLECTION}" reflection_sha256)
file(SHA256 "${PONDER_SHADER_OUTPUT_HEADER}" header_sha256)
file(SHA256 "${PONDER_SHADER_OUTPUT_SOURCE}" generated_source_sha256)
foreach(hash_field IN ITEMS
        spirvSha256 reflectionSha256 generatedHeaderSha256 generatedSourceSha256)
    _ponder_json_get(manifest_${hash_field} "${manifest_json}" "${hash_field}")
endforeach()
_ponder_expect_equal("SPIR-V hash" "${manifest_spirvSha256}" "${spirv_sha256}")
_ponder_expect_equal("reflection hash" "${manifest_reflectionSha256}"
                     "${reflection_sha256}")
_ponder_expect_equal("generated header hash" "${manifest_generatedHeaderSha256}"
                     "${header_sha256}")
_ponder_expect_equal("generated source hash" "${manifest_generatedSourceSha256}"
                     "${generated_source_sha256}")

file(SIZE "${PONDER_SHADER_OUTPUT_SPIRV}" spirv_byte_count)
math(EXPR spirv_remainder "${spirv_byte_count} % 4")
if(NOT spirv_remainder EQUAL 0)
    message(FATAL_ERROR "SPIR-V byte length is not a multiple of 32-bit words.")
endif()
math(EXPR spirv_word_count "${spirv_byte_count} / 4")
_ponder_json_get(manifest_word_count "${manifest_json}" spirvWordCount)
_ponder_expect_equal("SPIR-V word count" "${manifest_word_count}" "${spirv_word_count}")

file(READ "${PONDER_SHADER_OUTPUT_STAMP}" stamp_text)
string(STRIP "${stamp_text}" stamp_text)
_ponder_expect_equal("stamp input hash" "${stamp_text}" "${input_sha256}")

file(READ "${PONDER_SHADER_OUTPUT_HEADER}" generated_header)
file(READ "${PONDER_SHADER_OUTPUT_SOURCE}" generated_source)
set(symbol_prefix "${PONDER_SHADER_NAME}Shader")
foreach(metadata_pair IN ITEMS
        "Name|${PONDER_SHADER_NAME}"
        "EntryPoint|${PONDER_SHADER_ENTRY_POINT}"
        "Profile|${PONDER_SHADER_PROFILE}"
        "Stage|${PONDER_SHADER_STAGE}"
        "SchemaFingerprint|${PONDER_SHADER_SCHEMA_FINGERPRINT}"
        "SpirvSha256|${spirv_sha256}"
        "InputSha256|${input_sha256}")
    string(REPLACE "|" ";" metadata_parts "${metadata_pair}")
    list(GET metadata_parts 0 metadata_suffix)
    list(GET metadata_parts 1 metadata_value)
    _ponder_cpp_escape(metadata_value_cpp "${metadata_value}")
    set(expected_declaration
        "k${symbol_prefix}${metadata_suffix}{\"${metadata_value_cpp}\"};")
    string(FIND "${generated_header}" "${expected_declaration}" declaration_index)
    if(declaration_index EQUAL -1)
        message(FATAL_ERROR
            "Generated shader header is missing current ${metadata_suffix} metadata.")
    endif()
endforeach()
string(FIND "${generated_header}"
    "std::array<std::uint32_t, ${spirv_word_count}> k${symbol_prefix}SpirvWords"
    header_words_index)
string(FIND "${generated_source}"
    "std::array<std::uint32_t, ${spirv_word_count}> k${symbol_prefix}SpirvWords"
    source_words_index)
if(header_words_index EQUAL -1 OR source_words_index EQUAL -1)
    message(FATAL_ERROR "Generated C++ SPIR-V word count or symbol is stale.")
endif()

file(READ "${PONDER_SHADER_OUTPUT_REFLECTION}" reflection_json)
string(JSON reflection_entry_count ERROR_VARIABLE reflection_error
       LENGTH "${reflection_json}" entryPoints)
if(NOT reflection_error STREQUAL "NOTFOUND" OR reflection_entry_count LESS 1)
    message(FATAL_ERROR "Shader reflection JSON has no valid entryPoints array.")
endif()
if(PONDER_SHADER_STAGE STREQUAL "vertex")
    set(expected_reflection_mode "vert")
elseif(PONDER_SHADER_STAGE STREQUAL "fragment")
    set(expected_reflection_mode "frag")
elseif(PONDER_SHADER_STAGE STREQUAL "compute")
    set(expected_reflection_mode "comp")
elseif(PONDER_SHADER_STAGE STREQUAL "geometry")
    set(expected_reflection_mode "geom")
elseif(PONDER_SHADER_STAGE STREQUAL "tessellation_control")
    set(expected_reflection_mode "tesc")
else()
    set(expected_reflection_mode "tese")
endif()
set(found_entry_point FALSE)
math(EXPR last_entry_index "${reflection_entry_count} - 1")
foreach(entry_index RANGE 0 ${last_entry_index})
    string(JSON reflected_name GET "${reflection_json}" entryPoints ${entry_index} name)
    string(JSON reflected_mode GET "${reflection_json}" entryPoints ${entry_index} mode)
    if(reflected_name STREQUAL PONDER_SHADER_ENTRY_POINT AND
       reflected_mode STREQUAL expected_reflection_mode)
        set(found_entry_point TRUE)
    endif()
endforeach()
if(NOT found_entry_point)
    message(FATAL_ERROR
        "Shader reflection does not contain the expected entry point and stage.")
endif()

execute_process(
    COMMAND "${spirv_val_path}" --target-env vulkan1.2 "${PONDER_SHADER_OUTPUT_SPIRV}"
    RESULT_VARIABLE validate_result
    OUTPUT_VARIABLE validate_stdout
    ERROR_VARIABLE validate_stderr)
if(NOT validate_result EQUAL 0)
    message(FATAL_ERROR
        "spirv-val rejected the checked shader artifact.\n"
        "stdout:\n${validate_stdout}\nstderr:\n${validate_stderr}")
endif()
