function(_ponder_require_defined variable_name)
    if(NOT DEFINED ${variable_name} OR "${${variable_name}}" STREQUAL "")
        message(FATAL_ERROR "${variable_name} is required.")
    endif()
endfunction()

foreach(required_variable IN ITEMS
        PONDER_DRAW2D_RECTANGLE_VERTEX_REFLECTION
        PONDER_DRAW2D_RECTANGLE_FRAGMENT_REFLECTION)
    _ponder_require_defined("${required_variable}")
endforeach()

function(_ponder_expect_equal label actual expected)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR
            "Draw2D rectangle shader interface ${label} mismatch. "
            "Expected '${expected}', got '${actual}'.")
    endif()
endfunction()

function(_ponder_json_get output_variable json_text)
    string(JSON json_value ERROR_VARIABLE json_error GET "${json_text}" ${ARGN})
    if(NOT json_error STREQUAL "NOTFOUND")
        message(FATAL_ERROR "Failed to read JSON field ${ARGN}: ${json_error}")
    endif()
    set("${output_variable}" "${json_value}" PARENT_SCOPE)
endfunction()

function(_ponder_json_length_or_zero output_variable json_text)
    string(JSON json_length ERROR_VARIABLE json_error LENGTH "${json_text}" ${ARGN})
    if(json_error STREQUAL "NOTFOUND")
        set("${output_variable}" "${json_length}" PARENT_SCOPE)
    else()
        set("${output_variable}" "0" PARENT_SCOPE)
    endif()
endfunction()

function(_ponder_expect_entry_point json_text expected_name expected_mode)
    _ponder_json_length_or_zero(entry_count "${json_text}" entryPoints)
    _ponder_expect_equal("entry point count" "${entry_count}" "1")

    _ponder_json_get(entry_name "${json_text}" entryPoints 0 name)
    _ponder_json_get(entry_mode "${json_text}" entryPoints 0 mode)
    _ponder_expect_equal("entry point name" "${entry_name}" "${expected_name}")
    _ponder_expect_equal("entry point mode" "${entry_mode}" "${expected_mode}")
endfunction()

function(_ponder_expect_location_type
        label json_text array_name expected_count expected_location expected_type expected_name)
    _ponder_json_length_or_zero(interface_count "${json_text}" "${array_name}")
    _ponder_expect_equal("${label} ${array_name} count" "${interface_count}" "${expected_count}")

    set(location_count 0)
    math(EXPR last_interface_index "${interface_count} - 1")
    foreach(interface_index RANGE 0 ${last_interface_index})
        _ponder_json_get(reflected_location "${json_text}" "${array_name}" ${interface_index} location)
        if(reflected_location STREQUAL expected_location)
            _ponder_json_get(reflected_type "${json_text}" "${array_name}" ${interface_index} type)
            _ponder_json_get(reflected_name "${json_text}" "${array_name}" ${interface_index} name)
            _ponder_expect_equal("${label} type" "${reflected_type}" "${expected_type}")
            _ponder_expect_equal("${label} name" "${reflected_name}" "${expected_name}")
            math(EXPR location_count "${location_count} + 1")
        endif()
    endforeach()

    _ponder_expect_equal(
        "${label} location ${expected_location} occurrence count" "${location_count}" "1")
endfunction()

function(_ponder_expect_array_absent_or_empty label json_text array_name)
    _ponder_json_length_or_zero(array_count "${json_text}" "${array_name}")
    if(array_count GREATER 0)
        message(FATAL_ERROR
            "Draw2D rectangle shader reflection unexpectedly contains ${label} resources.")
    endif()
endfunction()

function(_ponder_expect_no_descriptor_resources label json_text)
    foreach(resource_array IN ITEMS
            acceleration_structures
            atomic_counters
            images
            separate_images
            separate_samplers
            sampled_images
            specialization_constants
            ssbos
            storage_images
            subpass_inputs
            textures
            ubos)
        _ponder_expect_array_absent_or_empty("${label} ${resource_array}" "${json_text}"
            "${resource_array}")
    endforeach()
endfunction()

function(_ponder_expect_push_constants json_text)
    _ponder_json_length_or_zero(push_constant_count "${json_text}" push_constants)
    _ponder_expect_equal("push constant count" "${push_constant_count}" "1")

    _ponder_json_get(push_constant_type "${json_text}" push_constants 0 type)
    _ponder_json_get(push_constant_name "${json_text}" push_constants 0 name)
    _ponder_json_get(push_constant_marker "${json_text}" push_constants 0 push_constant)
    _ponder_expect_equal("push constant name" "${push_constant_name}" "draw2DFrame")
    _ponder_expect_equal("push constant marker" "${push_constant_marker}" "ON")

    _ponder_json_length_or_zero(type_count "${json_text}" types)
    _ponder_expect_equal("reflected type count" "${type_count}" "1")
    _ponder_json_get(push_constant_type_name "${json_text}" types "${push_constant_type}" name)
    _ponder_expect_equal("push constant type name" "${push_constant_type_name}"
        "type.ConstantBuffer.Draw2DFrameConstants")

    _ponder_json_length_or_zero(member_count "${json_text}" types "${push_constant_type}" members)
    _ponder_expect_equal("push constant member count" "${member_count}" "1")
    _ponder_json_get(member_name "${json_text}" types "${push_constant_type}" members 0 name)
    _ponder_json_get(member_type "${json_text}" types "${push_constant_type}" members 0 type)
    _ponder_json_get(member_offset "${json_text}" types "${push_constant_type}" members 0 offset)
    _ponder_expect_equal("push constant member name" "${member_name}" "pixelExtent")
    _ponder_expect_equal("push constant member type" "${member_type}" "uvec2")
    _ponder_expect_equal("push constant member offset" "${member_offset}" "0")
endfunction()

file(READ "${PONDER_DRAW2D_RECTANGLE_VERTEX_REFLECTION}" vertex_reflection_json)
file(READ "${PONDER_DRAW2D_RECTANGLE_FRAGMENT_REFLECTION}" fragment_reflection_json)

_ponder_expect_entry_point("${vertex_reflection_json}" MainVs vert)
_ponder_expect_entry_point("${fragment_reflection_json}" MainPs frag)
_ponder_expect_no_descriptor_resources("vertex" "${vertex_reflection_json}")
_ponder_expect_no_descriptor_resources("fragment" "${fragment_reflection_json}")
_ponder_expect_push_constants("${vertex_reflection_json}")
_ponder_expect_array_absent_or_empty("fragment push constants" "${fragment_reflection_json}"
    push_constants)
_ponder_expect_location_type(
    "vertex position input" "${vertex_reflection_json}" inputs 2 0 vec2 "in.var.POSITION0")
_ponder_expect_location_type(
    "vertex color input" "${vertex_reflection_json}" inputs 2 1 vec4 "in.var.COLOR0")
_ponder_expect_location_type(
    "vertex color output" "${vertex_reflection_json}" outputs 1 0 vec4 "out.var.COLOR0")
_ponder_expect_location_type(
    "fragment color input" "${fragment_reflection_json}" inputs 1 0 vec4 "in.var.COLOR0")
_ponder_expect_location_type(
    "fragment color output" "${fragment_reflection_json}" outputs 1 0 vec4
    "out.var.SV_Target0")

message(STATUS "Draw2D rectangle shader reflected interface matches the static packet contract.")
