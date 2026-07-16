cmake_minimum_required(VERSION 3.28)

foreach(required_variable IN ITEMS
        PONDER_SOURCE_DIR
        PONDER_BINARY_DIR
        PONDER_TARGET_MANIFEST
        PONDER_EXPECT_RENDER
        PONDER_EXPECT_UI_RENDER_INTEGRATION
        PONDER_EXPECT_INSTALL_RULES)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required.")
    endif()
endforeach()

if(NOT DEFINED PONDER_REQUIRE_COMPILE_COMMANDS)
    set(PONDER_REQUIRE_COMPILE_COMMANDS ON)
endif()

if(NOT EXISTS "${PONDER_TARGET_MANIFEST}")
    message(FATAL_ERROR "Configured target manifest is missing: ${PONDER_TARGET_MANIFEST}")
endif()
file(READ "${PONDER_TARGET_MANIFEST}" target_manifest)

function(ponder_require_target target_name)
    if(NOT "\n${target_manifest}" MATCHES "\ntarget=${target_name}(\n|$)")
        message(FATAL_ERROR "Required configured target is missing: ${target_name}")
    endif()
endfunction()

function(ponder_require_no_target target_name)
    if("\n${target_manifest}" MATCHES "\ntarget=${target_name}(\n|$)")
        message(FATAL_ERROR "Unexpected configured target is present: ${target_name}")
    endif()
endfunction()

function(ponder_get_target_record target_name output_variable)
    string(REGEX MATCHALL "(^|\n)${target_name}\\.[^\n]*" target_records "${target_manifest}")
    set("${output_variable}" "${target_records}" PARENT_SCOPE)
endfunction()

function(ponder_require_target_record_absent target_name blocked_regex)
    ponder_get_target_record("${target_name}" target_record)
    string(TOLOWER "${target_record}" lower_record)
    if(lower_record MATCHES "${blocked_regex}")
        message(FATAL_ERROR
            "Configured target ${target_name} exposes a forbidden dependency or path: "
            "${blocked_regex}")
    endif()
endfunction()

function(ponder_require_target_record_contains target_name required_regex)
    ponder_get_target_record("${target_name}" target_record)
    string(TOLOWER "${target_record}" lower_record)
    if(NOT lower_record MATCHES "${required_regex}")
        message(FATAL_ERROR
            "Configured target ${target_name} is missing required topology: ${required_regex}")
    endif()
endfunction()

ponder_require_target(ponder_ui)
ponder_require_target(ponder_ui_paint)
ponder_require_target_record_absent(ponder_ui
    "ponder(::|_)render|libs[/\\\\]render|libs[/\\\\]ui[/\\\\]private|vulkan|sdl")
ponder_require_target_record_absent(ponder_ui_paint
    "ponder(::|_)render|libs[/\\\\]render|vulkan|sdl")
ponder_require_target_record_contains(ponder_ui "ponder::core")

if(PONDER_EXPECT_UI_RENDER_INTEGRATION AND NOT PONDER_EXPECT_RENDER)
    message(FATAL_ERROR "UI/render integration cannot be enabled while render is disabled.")
endif()

if(PONDER_EXPECT_RENDER)
    foreach(render_target IN ITEMS
            ponder_render
            ponder_render_draw2d
            ponder_render_draw2d_packet
            ponder_render_volk)
        ponder_require_target("${render_target}")
        ponder_require_target_record_absent("${render_target}"
            "ponder(::|_)ui|libs[/\\\\]ui")
    endforeach()

    if(PONDER_EXPECT_UI_RENDER_INTEGRATION)
        ponder_require_target(ponder_ui_render_integration)
        ponder_require_target(ponder_ui_experimental)
        ponder_require_target_record_contains(ponder_ui_render_integration "ponder::ui")
        ponder_require_target_record_contains(ponder_ui_render_integration "ponder::render")
        ponder_require_target_record_contains(ponder_ui_experimental "ponder::ui")
        ponder_require_target_record_contains(ponder_ui_experimental "ponder::render")
    else()
        ponder_require_no_target(ponder_ui_render_integration)
        ponder_require_no_target(ponder_ui_experimental)
    endif()
else()
    foreach(render_target IN ITEMS
            ponder_render
            ponder_render_draw2d
            ponder_render_draw2d_packet
            ponder_render_volk
            ponder_ui_render_integration
            ponder_ui_experimental)
        ponder_require_no_target("${render_target}")
    endforeach()

    if(EXISTS "${PONDER_BINARY_DIR}/generated/render")
        message(FATAL_ERROR "A clean render-disabled build generated render shader artifacts.")
    endif()
endif()

set(cache_file "${PONDER_BINARY_DIR}/CMakeCache.txt")
if(NOT EXISTS "${cache_file}")
    message(FATAL_ERROR "Configured CMake cache is missing: ${cache_file}")
endif()
file(READ "${cache_file}" cache_contents)
if(PONDER_EXPECT_RENDER)
    if(NOT cache_contents MATCHES "PONDER_BUILD_RENDER:BOOL=ON")
        message(FATAL_ERROR "The build cache does not record render as enabled.")
    endif()
else()
    if(NOT cache_contents MATCHES "PONDER_BUILD_RENDER:BOOL=OFF"
            OR NOT cache_contents MATCHES "PONDER_RENDER_ENABLE_VULKAN:BOOL=OFF"
            OR NOT cache_contents MATCHES "PONDER_RENDER_ENABLE_VALIDATION:BOOL=OFF")
        message(FATAL_ERROR "The build cache does not record the complete render-disabled state.")
    endif()
    if(cache_contents MATCHES "PONDER_RENDER_SHADER_(DXC|SPIRV_VAL|SPIRV_CROSS):")
        message(FATAL_ERROR "A render-disabled configuration discovered the shader toolchain.")
    endif()
endif()

if(PONDER_EXPECT_UI_RENDER_INTEGRATION)
    if(NOT cache_contents MATCHES "PONDER_BUILD_UI_RENDER_INTEGRATION:BOOL=ON")
        message(FATAL_ERROR "The build cache does not record UI/render integration as enabled.")
    endif()
elseif(NOT cache_contents MATCHES "PONDER_BUILD_UI_RENDER_INTEGRATION:BOOL=OFF")
    message(FATAL_ERROR "The build cache does not record UI/render integration as disabled.")
endif()

if(PONDER_EXPECT_INSTALL_RULES)
    set(package_config "${PONDER_BINARY_DIR}/cmake/ponderConfig.cmake")
    if(NOT EXISTS "${package_config}")
        message(FATAL_ERROR "The configured package file is missing: ${package_config}")
    endif()
    file(READ "${package_config}" package_contents)
    string(TOLOWER "${package_contents}" lower_package_contents)
    if(lower_package_contents MATCHES "private/include|draw2d|vulkan|test")
        message(FATAL_ERROR "The configured package file exposes private backend topology.")
    endif()
endif()

if(NOT PONDER_REQUIRE_COMPILE_COMMANDS)
    message(STATUS
        "Skipping compile-command UI topology checks because the configured generator does not "
        "provide a compile database.")
    message(STATUS
        "UI build topology verified (render=${PONDER_EXPECT_RENDER}, "
        "integration=${PONDER_EXPECT_UI_RENDER_INTEGRATION}, "
        "install=${PONDER_EXPECT_INSTALL_RULES}).")
    return()
endif()

set(compile_commands_file "${PONDER_BINARY_DIR}/compile_commands.json")
if(NOT EXISTS "${compile_commands_file}")
    message(FATAL_ERROR "compile_commands.json is required for UI build verification.")
endif()
file(READ "${compile_commands_file}" compile_commands)
string(JSON compile_command_count LENGTH "${compile_commands}")
if(compile_command_count EQUAL 0)
    message(FATAL_ERROR "compile_commands.json contains no translation units.")
endif()

math(EXPR last_compile_command "${compile_command_count} - 1")
set(saw_ui_core FALSE)
set(saw_ui_headers FALSE)
set(saw_render FALSE)
set(saw_experimental_headers FALSE)
foreach(command_index RANGE 0 ${last_compile_command})
    string(JSON command GET "${compile_commands}" ${command_index} command)
    string(JSON source_file GET "${compile_commands}" ${command_index} file)
    string(JSON output_file ERROR_VARIABLE output_error
        GET "${compile_commands}" ${command_index} output)
    if(output_error)
        set(output_file "")
    endif()

    string(TOLOWER "${command}" lower_command)
    string(TOLOWER "${source_file}" lower_source_file)
    string(TOLOWER "${output_file}" lower_output_file)
    string(REPLACE "\\" "/" lower_command "${lower_command}")
    string(REPLACE "\\" "/" lower_source_file "${lower_source_file}")
    string(REPLACE "\\" "/" lower_output_file "${lower_output_file}")

    if(lower_output_file MATCHES "/cmakefiles/ponder_ui\\.dir/")
        set(saw_ui_core TRUE)
        if(lower_command MATCHES "libs/render|vulkan|sdl3|ponder(::|_)render")
            message(FATAL_ERROR "The UI core compile command leaks a renderer/backend dependency.")
        endif()
    endif()

    if(lower_output_file MATCHES "/cmakefiles/ponder_ui_header_tests\\.dir/")
        set(saw_ui_headers TRUE)
        if(lower_command MATCHES "(/yu|cmake_pch|-include-pch|-include[ =][^ ]*pch)")
            message(FATAL_ERROR "Stable public UI headers were compiled with a PCH.")
        endif()
    endif()

    if(lower_output_file MATCHES "/cmakefiles/ponder_ui_experimental_header_tests\\.dir/")
        set(saw_experimental_headers TRUE)
        if(lower_command MATCHES "(/yu|cmake_pch|-include-pch|-include[ =][^ ]*pch)")
            message(FATAL_ERROR "Experimental public UI headers were compiled with a PCH.")
        endif()
    endif()

    if(lower_output_file MATCHES "/cmakefiles/ponder_render\\.dir/")
        set(saw_render TRUE)
        if(lower_command MATCHES "libs/ui|ponder(::|_)ui")
            message(FATAL_ERROR "The render library compile command leaks a UI dependency.")
        endif()
    endif()

    if(NOT PONDER_EXPECT_RENDER AND
            (lower_source_file MATCHES "/libs/render/"
             OR lower_output_file MATCHES "/libs/render/"
             OR lower_output_file MATCHES "/generated/render/"))
        message(FATAL_ERROR "A render-disabled build compiled a render translation unit.")
    endif()

    if(NOT PONDER_EXPECT_UI_RENDER_INTEGRATION AND
            (lower_output_file MATCHES "ponder_ui_experimental"
             OR lower_output_file MATCHES "ponder_ui_render_integration"))
        message(FATAL_ERROR "A build without UI integration compiled an integration target.")
    endif()
endforeach()

if(NOT saw_ui_core OR NOT saw_ui_headers)
    message(FATAL_ERROR "UI core and its PCH-free public-header consumers must both compile.")
endif()
if(PONDER_EXPECT_RENDER AND NOT saw_render)
    message(FATAL_ERROR "The render-enabled compile database contains no render library entry.")
endif()
if(PONDER_EXPECT_UI_RENDER_INTEGRATION AND NOT saw_experimental_headers)
    message(FATAL_ERROR "The full build did not compile the experimental PCH-free header consumer.")
endif()

message(STATUS
    "UI build topology verified (render=${PONDER_EXPECT_RENDER}, "
    "integration=${PONDER_EXPECT_UI_RENDER_INTEGRATION}, "
    "install=${PONDER_EXPECT_INSTALL_RULES}).")
