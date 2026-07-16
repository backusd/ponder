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

function(_ponder_json_escape output_variable input_value)
    set(escaped "${input_value}")
    string(REPLACE "\\" "\\\\" escaped "${escaped}")
    string(REPLACE "\"" "\\\"" escaped "${escaped}")
    string(ASCII 8 backspace)
    string(ASCII 9 tab)
    string(ASCII 12 form_feed)
    string(REPLACE "${backspace}" "\\b" escaped "${escaped}")
    string(REPLACE "${tab}" "\\t" escaped "${escaped}")
    string(REPLACE "\n" "\\n" escaped "${escaped}")
    string(REPLACE "${form_feed}" "\\f" escaped "${escaped}")
    string(REPLACE "\r" "\\r" escaped "${escaped}")
    foreach(control_code RANGE 1 31)
        if(control_code EQUAL 8 OR control_code EQUAL 9 OR control_code EQUAL 10 OR
           control_code EQUAL 12 OR control_code EQUAL 13)
            continue()
        endif()
        string(ASCII ${control_code} control_character)
        string(FIND "${escaped}" "${control_character}" control_index)
        if(NOT control_index EQUAL -1)
            message(FATAL_ERROR
                "Shader provenance contains unsupported ASCII control code ${control_code}.")
        endif()
    endforeach()
    set("${output_variable}" "${escaped}" PARENT_SCOPE)
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

function(_ponder_find_shader_tool output_variable provided_path tool_name display_name)
    if(NOT "${provided_path}" STREQUAL "")
        _ponder_normalize_path(tool_path "${provided_path}")
        if(NOT EXISTS "${tool_path}" OR IS_DIRECTORY "${tool_path}")
            message(FATAL_ERROR "${display_name} was set to a missing file: ${tool_path}")
        endif()
        set("${output_variable}" "${tool_path}" PARENT_SCOPE)
        return()
    endif()

    set(tool_hints)
    if(DEFINED ENV{VULKAN_SDK} AND NOT "$ENV{VULKAN_SDK}" STREQUAL "")
        list(APPEND tool_hints "$ENV{VULKAN_SDK}/Bin")
    endif()
    file(GLOB vulkan_sdk_bin_dirs LIST_DIRECTORIES TRUE "C:/VulkanSDK/*/Bin")
    if(vulkan_sdk_bin_dirs)
        list(SORT vulkan_sdk_bin_dirs COMPARE NATURAL ORDER DESCENDING)
        list(APPEND tool_hints ${vulkan_sdk_bin_dirs})
    endif()

    string(MAKE_C_IDENTIFIER "${tool_name}" tool_variable_suffix)
    set(found_tool_variable "_ponder_found_${tool_variable_suffix}")
    find_program(${found_tool_variable} NAMES "${tool_name}" HINTS ${tool_hints} NO_CACHE)
    if(NOT ${found_tool_variable})
        message(FATAL_ERROR
            "${display_name} was not found. Install the render shader toolchain or set the "
            "matching PONDER_RENDER_SHADER_* cache path. Required tools are: a DXC executable "
            "built with SPIR-V CodeGen enabled, spirv-val, and spirv-cross.")
    endif()
    _ponder_normalize_path(tool_path "${${found_tool_variable}}")
    set("${output_variable}" "${tool_path}" PARENT_SCOPE)
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

function(_ponder_append_json_string_array output_variable list_name)
    set(json "[")
    set(first TRUE)
    foreach(item IN LISTS ${list_name})
        _ponder_json_escape(item_json "${item}")
        if(first)
            set(first FALSE)
        else()
            string(APPEND json ", ")
        endif()
        string(APPEND json "\"${item_json}\"")
    endforeach()
    string(APPEND json "]")
    set("${output_variable}" "${json}" PARENT_SCOPE)
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

function(_ponder_run_check output_result output_stdout output_stderr
         header source spirv reflection manifest stamp preprocessed_sha)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            -D "PONDER_SHADER_DXC=${dxc_path}"
            -D "PONDER_SHADER_SPIRV_VAL=${spirv_val_path}"
            -D "PONDER_SHADER_SPIRV_CROSS=${spirv_cross_path}"
            -D "PONDER_SHADER_SOURCE=${source_path}"
            -D "PONDER_SHADER_NAME=${PONDER_SHADER_NAME}"
            -D "PONDER_SHADER_ENTRY_POINT=${PONDER_SHADER_ENTRY_POINT}"
            -D "PONDER_SHADER_PROFILE=${PONDER_SHADER_PROFILE}"
            -D "PONDER_SHADER_STAGE=${PONDER_SHADER_STAGE}"
            -D "PONDER_SHADER_SCHEMA_FINGERPRINT=${PONDER_SHADER_SCHEMA_FINGERPRINT}"
            -D "PONDER_SHADER_DXC_OPTIONS=${PONDER_SHADER_DXC_OPTIONS}"
            -D "PONDER_SHADER_OUTPUT_HEADER=${header}"
            -D "PONDER_SHADER_OUTPUT_SOURCE=${source}"
            -D "PONDER_SHADER_OUTPUT_SPIRV=${spirv}"
            -D "PONDER_SHADER_OUTPUT_REFLECTION=${reflection}"
            -D "PONDER_SHADER_OUTPUT_MANIFEST=${manifest}"
            -D "PONDER_SHADER_OUTPUT_STAMP=${stamp}"
            -D "PONDER_SHADER_GENERATOR_SCRIPT=${PONDER_SHADER_GENERATOR_SCRIPT}"
            -D "PONDER_SHADER_CHECK_SCRIPT=${PONDER_SHADER_CHECK_SCRIPT}"
            -D "PONDER_SHADER_PREPROCESSED_SHA256=${preprocessed_sha}"
            -P "${PONDER_SHADER_CHECK_SCRIPT}"
        RESULT_VARIABLE check_result
        OUTPUT_VARIABLE check_stdout
        ERROR_VARIABLE check_stderr)
    set("${output_result}" "${check_result}" PARENT_SCOPE)
    set("${output_stdout}" "${check_stdout}" PARENT_SCOPE)
    set("${output_stderr}" "${check_stderr}" PARENT_SCOPE)
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

if(NOT EXISTS "${PONDER_SHADER_SOURCE}")
    message(FATAL_ERROR "Shader source is missing: ${PONDER_SHADER_SOURCE}")
endif()
if(NOT EXISTS "${PONDER_SHADER_GENERATOR_SCRIPT}" OR
   NOT EXISTS "${PONDER_SHADER_CHECK_SCRIPT}")
    message(FATAL_ERROR "Shader generator/check scripts are missing.")
endif()

_ponder_normalize_path(source_path "${PONDER_SHADER_SOURCE}")
_ponder_find_shader_tool(dxc_path "${PONDER_SHADER_DXC}" dxc
                         "DirectXShaderCompiler dxc")
_ponder_find_shader_tool(spirv_val_path "${PONDER_SHADER_SPIRV_VAL}" spirv-val
                         "SPIR-V validator spirv-val")
_ponder_find_shader_tool(spirv_cross_path "${PONDER_SHADER_SPIRV_CROSS}" spirv-cross
                         "SPIRV-Cross reflector")
_ponder_tool_version(dxc_version "${dxc_path}" "dxc")
_ponder_tool_version(spirv_val_version "${spirv_val_path}" "spirv-val")
_ponder_tool_version(spirv_cross_version "${spirv_cross_path}" "spirv-cross")
file(SHA256 "${source_path}" source_sha256)
file(SHA256 "${dxc_path}" dxc_sha256)
file(SHA256 "${spirv_val_path}" spirv_val_sha256)
file(SHA256 "${spirv_cross_path}" spirv_cross_sha256)
file(SHA256 "${PONDER_SHADER_GENERATOR_SCRIPT}" generator_sha256)
file(SHA256 "${PONDER_SHADER_CHECK_SCRIPT}" check_sha256)

get_filename_component(output_dir "${PONDER_SHADER_OUTPUT_SPIRV}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")
string(REPLACE "|" ";" dxc_options "${PONDER_SHADER_DXC_OPTIONS}")
set(preprocessed_file "${PONDER_SHADER_OUTPUT_STAMP}.preprocessed.tmp")
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
        "DXC failed to preprocess ${PONDER_SHADER_NAME}.\n"
        "stdout:\n${preprocess_stdout}\nstderr:\n${preprocess_stderr}")
endif()
file(SHA256 "${preprocessed_file}" preprocessed_sha256)

_ponder_compute_contract_hashes(
    options_sha256 schema_sha256 input_sha256
    "${source_path}" "${source_sha256}" "${preprocessed_sha256}"
    "${dxc_path}" "${dxc_sha256}" "${dxc_version}"
    "${spirv_val_path}" "${spirv_val_sha256}" "${spirv_val_version}"
    "${spirv_cross_path}" "${spirv_cross_sha256}" "${spirv_cross_version}"
    "${generator_sha256}" "${check_sha256}")

set(all_outputs_exist TRUE)
foreach(output_file IN ITEMS
        "${PONDER_SHADER_OUTPUT_HEADER}"
        "${PONDER_SHADER_OUTPUT_SOURCE}"
        "${PONDER_SHADER_OUTPUT_SPIRV}"
        "${PONDER_SHADER_OUTPUT_REFLECTION}"
        "${PONDER_SHADER_OUTPUT_MANIFEST}"
        "${PONDER_SHADER_OUTPUT_STAMP}")
    if(NOT EXISTS "${output_file}")
        set(all_outputs_exist FALSE)
    endif()
endforeach()
if(all_outputs_exist)
    _ponder_run_check(existing_check_result existing_check_stdout existing_check_stderr
        "${PONDER_SHADER_OUTPUT_HEADER}"
        "${PONDER_SHADER_OUTPUT_SOURCE}"
        "${PONDER_SHADER_OUTPUT_SPIRV}"
        "${PONDER_SHADER_OUTPUT_REFLECTION}"
        "${PONDER_SHADER_OUTPUT_MANIFEST}"
        "${PONDER_SHADER_OUTPUT_STAMP}"
        "${preprocessed_sha256}")
    if(existing_check_result EQUAL 0)
        file(REMOVE "${preprocessed_file}")
        message(STATUS
            "Render shader artifact ${PONDER_SHADER_NAME} is current (input ${input_sha256}).")
        return()
    endif()
endif()

set(temp_header "${PONDER_SHADER_OUTPUT_HEADER}.tmp")
set(temp_source "${PONDER_SHADER_OUTPUT_SOURCE}.tmp")
set(temp_spirv "${PONDER_SHADER_OUTPUT_SPIRV}.tmp")
set(temp_reflection "${PONDER_SHADER_OUTPUT_REFLECTION}.tmp")
set(temp_manifest "${PONDER_SHADER_OUTPUT_MANIFEST}.tmp")
set(temp_stamp "${PONDER_SHADER_OUTPUT_STAMP}.tmp")
file(REMOVE
    "${temp_header}"
    "${temp_source}"
    "${temp_spirv}"
    "${temp_reflection}"
    "${temp_manifest}"
    "${temp_stamp}")

execute_process(
    COMMAND "${dxc_path}"
            ${dxc_options}
            -T "${PONDER_SHADER_PROFILE}"
            -E "${PONDER_SHADER_ENTRY_POINT}"
            -Fo "${temp_spirv}"
            "${source_path}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_stdout
    ERROR_VARIABLE compile_stderr)
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR
        "DXC failed to compile ${PONDER_SHADER_NAME}. DXC must include SPIR-V CodeGen.\n"
        "stdout:\n${compile_stdout}\nstderr:\n${compile_stderr}")
endif()

execute_process(
    COMMAND "${spirv_val_path}" --target-env vulkan1.2 "${temp_spirv}"
    RESULT_VARIABLE validate_result
    OUTPUT_VARIABLE validate_stdout
    ERROR_VARIABLE validate_stderr)
if(NOT validate_result EQUAL 0)
    message(FATAL_ERROR
        "spirv-val failed for ${PONDER_SHADER_NAME}.\n"
        "stdout:\n${validate_stdout}\nstderr:\n${validate_stderr}")
endif()

execute_process(
    COMMAND "${spirv_cross_path}" "${temp_spirv}" --reflect
    RESULT_VARIABLE reflect_result
    OUTPUT_VARIABLE reflect_stdout
    ERROR_VARIABLE reflect_stderr)
if(NOT reflect_result EQUAL 0)
    message(FATAL_ERROR
        "spirv-cross reflection failed for ${PONDER_SHADER_NAME}.\n"
        "stdout:\n${reflect_stdout}\nstderr:\n${reflect_stderr}")
endif()
file(WRITE "${temp_reflection}" "${reflect_stdout}")

file(SHA256 "${temp_spirv}" spirv_sha256)
file(SHA256 "${temp_reflection}" reflection_sha256)
file(READ "${temp_spirv}" spirv_hex HEX)
string(LENGTH "${spirv_hex}" spirv_hex_length)
math(EXPR spirv_remainder "${spirv_hex_length} % 8")
if(NOT spirv_remainder EQUAL 0)
    message(FATAL_ERROR "SPIR-V byte length is not a multiple of 32-bit words.")
endif()
math(EXPR spirv_word_count "${spirv_hex_length} / 8")

set(spirv_words "")
if(spirv_word_count GREATER 0)
    math(EXPR last_offset "${spirv_hex_length} - 8")
    foreach(offset RANGE 0 ${last_offset} 8)
        string(SUBSTRING "${spirv_hex}" ${offset} 2 byte0)
        math(EXPR byte1_offset "${offset} + 2")
        math(EXPR byte2_offset "${offset} + 4")
        math(EXPR byte3_offset "${offset} + 6")
        string(SUBSTRING "${spirv_hex}" ${byte1_offset} 2 byte1)
        string(SUBSTRING "${spirv_hex}" ${byte2_offset} 2 byte2)
        string(SUBSTRING "${spirv_hex}" ${byte3_offset} 2 byte3)
        string(APPEND spirv_words "    0x${byte3}${byte2}${byte1}${byte0}u")
        if(NOT offset EQUAL last_offset)
            string(APPEND spirv_words ",")
        endif()
        string(APPEND spirv_words "\n")
    endforeach()
endif()

set(symbol_prefix "${PONDER_SHADER_NAME}Shader")
_ponder_cpp_escape(shader_name_cpp "${PONDER_SHADER_NAME}")
_ponder_cpp_escape(entry_point_cpp "${PONDER_SHADER_ENTRY_POINT}")
_ponder_cpp_escape(profile_cpp "${PONDER_SHADER_PROFILE}")
_ponder_cpp_escape(stage_cpp "${PONDER_SHADER_STAGE}")
_ponder_cpp_escape(schema_cpp "${PONDER_SHADER_SCHEMA_FINGERPRINT}")
file(WRITE "${temp_header}" "#pragma once\n\n")
file(APPEND "${temp_header}"
    "#include <array>\n#include <cstdint>\n#include <string_view>\n\n")
file(APPEND "${temp_header}" "namespace pond::render::shaders\n{\n")
file(APPEND "${temp_header}"
    "inline constexpr std::string_view k${symbol_prefix}Name{\"${shader_name_cpp}\"};\n")
file(APPEND "${temp_header}"
    "inline constexpr std::string_view k${symbol_prefix}EntryPoint{\"${entry_point_cpp}\"};\n")
file(APPEND "${temp_header}"
    "inline constexpr std::string_view k${symbol_prefix}Profile{\"${profile_cpp}\"};\n")
file(APPEND "${temp_header}"
    "inline constexpr std::string_view k${symbol_prefix}Stage{\"${stage_cpp}\"};\n")
file(APPEND "${temp_header}"
    "inline constexpr std::string_view k${symbol_prefix}SchemaFingerprint{\"${schema_cpp}\"};\n")
file(APPEND "${temp_header}"
    "inline constexpr std::string_view k${symbol_prefix}SpirvSha256{\"${spirv_sha256}\"};\n")
file(APPEND "${temp_header}"
    "inline constexpr std::string_view k${symbol_prefix}InputSha256{\"${input_sha256}\"};\n")
file(APPEND "${temp_header}"
    "extern const std::array<std::uint32_t, ${spirv_word_count}> "
    "k${symbol_prefix}SpirvWords;\n")
file(APPEND "${temp_header}" "} // namespace pond::render::shaders\n")

file(WRITE "${temp_source}" "#include \"${PONDER_SHADER_NAME}Shader.hpp\"\n\n")
file(APPEND "${temp_source}" "namespace pond::render::shaders\n{\n")
file(APPEND "${temp_source}"
    "extern const std::array<std::uint32_t, ${spirv_word_count}> "
    "k${symbol_prefix}SpirvWords{{\n")
file(APPEND "${temp_source}" "${spirv_words}")
file(APPEND "${temp_source}" "}};\n")
file(APPEND "${temp_source}" "} // namespace pond::render::shaders\n")
file(SHA256 "${temp_header}" generated_header_sha256)
file(SHA256 "${temp_source}" generated_source_sha256)

_ponder_append_json_string_array(dxc_options_json dxc_options)
foreach(json_value IN ITEMS
        PONDER_SHADER_NAME
        PONDER_SHADER_ENTRY_POINT
        PONDER_SHADER_PROFILE
        PONDER_SHADER_STAGE
        PONDER_SHADER_SCHEMA_FINGERPRINT)
    _ponder_json_escape(${json_value}_json "${${json_value}}")
endforeach()
_ponder_json_escape(source_json "${source_path}")
_ponder_json_escape(dxc_path_json "${dxc_path}")
_ponder_json_escape(spirv_val_path_json "${spirv_val_path}")
_ponder_json_escape(spirv_cross_path_json "${spirv_cross_path}")
_ponder_json_escape(dxc_version_json "${dxc_version}")
_ponder_json_escape(spirv_val_version_json "${spirv_val_version}")
_ponder_json_escape(spirv_cross_version_json "${spirv_cross_version}")

file(WRITE "${temp_manifest}" "{\n")
file(APPEND "${temp_manifest}" "  \"schemaVersion\": 2,\n")
file(APPEND "${temp_manifest}"
    "  \"shaderName\": \"${PONDER_SHADER_NAME_json}\",\n")
file(APPEND "${temp_manifest}"
    "  \"stage\": \"${PONDER_SHADER_STAGE_json}\",\n")
file(APPEND "${temp_manifest}"
    "  \"entryPoint\": \"${PONDER_SHADER_ENTRY_POINT_json}\",\n")
file(APPEND "${temp_manifest}"
    "  \"profile\": \"${PONDER_SHADER_PROFILE_json}\",\n")
file(APPEND "${temp_manifest}" "  \"targetEnvironment\": \"vulkan1.2\",\n")
file(APPEND "${temp_manifest}"
    "  \"schemaFingerprint\": \"${PONDER_SHADER_SCHEMA_FINGERPRINT_json}\",\n")
file(APPEND "${temp_manifest}" "  \"schemaSha256\": \"${schema_sha256}\",\n")
file(APPEND "${temp_manifest}" "  \"source\": \"${source_json}\",\n")
file(APPEND "${temp_manifest}" "  \"sourceSha256\": \"${source_sha256}\",\n")
file(APPEND "${temp_manifest}"
    "  \"preprocessedSourceSha256\": \"${preprocessed_sha256}\",\n")
file(APPEND "${temp_manifest}" "  \"dxcOptionsSha256\": \"${options_sha256}\",\n")
file(APPEND "${temp_manifest}" "  \"inputSha256\": \"${input_sha256}\",\n")
file(APPEND "${temp_manifest}" "  \"generatorSha256\": \"${generator_sha256}\",\n")
file(APPEND "${temp_manifest}" "  \"checkSha256\": \"${check_sha256}\",\n")
file(APPEND "${temp_manifest}" "  \"spirvSha256\": \"${spirv_sha256}\",\n")
file(APPEND "${temp_manifest}" "  \"spirvWordCount\": ${spirv_word_count},\n")
file(APPEND "${temp_manifest}"
    "  \"reflectionSha256\": \"${reflection_sha256}\",\n")
file(APPEND "${temp_manifest}"
    "  \"generatedHeaderSha256\": \"${generated_header_sha256}\",\n")
file(APPEND "${temp_manifest}"
    "  \"generatedSourceSha256\": \"${generated_source_sha256}\",\n")
file(APPEND "${temp_manifest}"
    "  \"dxc\": {\"path\": \"${dxc_path_json}\", \"version\": \"${dxc_version_json}\", "
    "\"sha256\": \"${dxc_sha256}\"},\n")
file(APPEND "${temp_manifest}"
    "  \"spirvVal\": {\"path\": \"${spirv_val_path_json}\", "
    "\"version\": \"${spirv_val_version_json}\", \"sha256\": \"${spirv_val_sha256}\"},\n")
file(APPEND "${temp_manifest}"
    "  \"spirvCross\": {\"path\": \"${spirv_cross_path_json}\", "
    "\"version\": \"${spirv_cross_version_json}\", \"sha256\": \"${spirv_cross_sha256}\"},\n")
file(APPEND "${temp_manifest}" "  \"dxcOptions\": ${dxc_options_json}\n")
file(APPEND "${temp_manifest}" "}\n")
file(WRITE "${temp_stamp}" "${input_sha256}\n")

_ponder_run_check(temp_check_result temp_check_stdout temp_check_stderr
    "${temp_header}"
    "${temp_source}"
    "${temp_spirv}"
    "${temp_reflection}"
    "${temp_manifest}"
    "${temp_stamp}"
    "${preprocessed_sha256}")
if(NOT temp_check_result EQUAL 0)
    message(FATAL_ERROR
        "Generated shader artifact ${PONDER_SHADER_NAME} failed its pre-commit check.\n"
        "stdout:\n${temp_check_stdout}\nstderr:\n${temp_check_stderr}")
endif()

foreach(output_kind IN ITEMS header source spirv reflection manifest stamp)
    string(TOUPPER "${output_kind}" output_kind_upper)
    file(COPY_FILE "${temp_${output_kind}}"
         "${PONDER_SHADER_OUTPUT_${output_kind_upper}}"
         ONLY_IF_DIFFERENT)
endforeach()
file(REMOVE
    "${preprocessed_file}"
    "${temp_header}"
    "${temp_source}"
    "${temp_spirv}"
    "${temp_reflection}"
    "${temp_manifest}"
    "${temp_stamp}")
message(STATUS
    "Generated render shader artifact ${PONDER_SHADER_NAME} (input ${input_sha256}).")
