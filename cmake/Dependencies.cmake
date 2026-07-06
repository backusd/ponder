include_guard(GLOBAL)

function(ponder_set_dependency_option option_name option_value option_type option_help)
    set("${option_name}" "${option_value}" CACHE "${option_type}" "${option_help}" FORCE)
endfunction()

function(ponder_add_alias alias_name target_name)
    if(NOT TARGET "${alias_name}" AND TARGET "${target_name}")
        add_library("${alias_name}" ALIAS "${target_name}")
    endif()
endfunction()

function(ponder_add_third_party_subdirectory source_dir binary_dir)
    if(NOT IS_ABSOLUTE "${source_dir}")
        set(source_dir "${PROJECT_SOURCE_DIR}/${source_dir}")
    endif()

    if(NOT IS_ABSOLUTE "${binary_dir}")
        set(binary_dir "${PROJECT_BINARY_DIR}/${binary_dir}")
    endif()

    if(NOT EXISTS "${source_dir}/CMakeLists.txt")
        message(FATAL_ERROR
            "Third-party dependency is missing a CMakeLists.txt: ${source_dir}")
    endif()

    add_subdirectory("${source_dir}" "${binary_dir}" EXCLUDE_FROM_ALL SYSTEM)
endfunction()

function(ponder_require_dependency_target target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Required dependency target is missing: ${target_name}")
    endif()
endfunction()

function(ponder_add_imgui_target)
    set(imgui_dir "${PROJECT_SOURCE_DIR}/third_party/imgui")

    if(NOT EXISTS "${imgui_dir}/imgui.cpp")
        message(FATAL_ERROR "Dear ImGui submodule is missing: ${imgui_dir}")
    endif()

    if(NOT TARGET ponder_imgui)
        add_library(ponder_imgui STATIC EXCLUDE_FROM_ALL
            "${imgui_dir}/imgui.cpp"
            "${imgui_dir}/imgui_draw.cpp"
            "${imgui_dir}/imgui_tables.cpp"
            "${imgui_dir}/imgui_widgets.cpp")

        target_include_directories(ponder_imgui SYSTEM PUBLIC
            "${imgui_dir}"
            "${imgui_dir}/backends")

        set_target_properties(ponder_imgui PROPERTIES
            FOLDER "third_party")
    endif()

    ponder_add_alias(ponder::imgui ponder_imgui)
endfunction()

function(ponder_configure_dependencies)
    ponder_set_dependency_option(SPDLOG_BUILD_EXAMPLE OFF BOOL "Disable spdlog examples.")
    ponder_set_dependency_option(SPDLOG_BUILD_EXAMPLE_HO OFF BOOL "Disable spdlog header-only examples.")
    ponder_set_dependency_option(SPDLOG_BUILD_TESTS OFF BOOL "Disable spdlog tests.")
    ponder_set_dependency_option(SPDLOG_BUILD_TESTS_HO OFF BOOL "Disable spdlog header-only tests.")
    ponder_set_dependency_option(SPDLOG_BUILD_BENCH OFF BOOL "Disable spdlog benchmarks.")
    ponder_set_dependency_option(SPDLOG_INSTALL OFF BOOL "Disable spdlog install rules.")
    ponder_set_dependency_option(SPDLOG_BUILD_SHARED OFF BOOL "Build spdlog as a static library.")

    ponder_set_dependency_option(BUILD_GMOCK ON BOOL "Build GoogleMock with GoogleTest.")
    ponder_set_dependency_option(INSTALL_GTEST OFF BOOL "Disable GoogleTest install rules.")
    ponder_set_dependency_option(gtest_force_shared_crt ON BOOL "Use the shared CRT for GoogleTest on Windows.")

    ponder_set_dependency_option(JSON_BuildTests OFF BOOL "Disable nlohmann/json tests.")
    ponder_set_dependency_option(JSON_Install OFF BOOL "Disable nlohmann/json install rules.")
    ponder_set_dependency_option(JSON_SystemInclude ON BOOL "Treat nlohmann/json includes as system includes.")

    ponder_set_dependency_option(SDL_SHARED OFF BOOL "Disable the SDL3 shared library.")
    ponder_set_dependency_option(SDL_STATIC ON BOOL "Enable the SDL3 static library.")
    ponder_set_dependency_option(SDL_TEST_LIBRARY OFF BOOL "Disable the SDL3 test library.")
    ponder_set_dependency_option(SDL_TESTS OFF BOOL "Disable SDL3 tests.")
    ponder_set_dependency_option(SDL_EXAMPLES OFF BOOL "Disable SDL3 examples.")
    ponder_set_dependency_option(SDL_INSTALL OFF BOOL "Disable SDL3 install rules.")
    ponder_set_dependency_option(SDL_INSTALL_TESTS OFF BOOL "Disable SDL3 test installation.")

    ponder_add_third_party_subdirectory(third_party/spdlog third_party/spdlog)
    ponder_add_third_party_subdirectory(third_party/googletest third_party/googletest)
    ponder_add_third_party_subdirectory(third_party/nlohmann_json third_party/nlohmann_json)
    ponder_add_third_party_subdirectory(third_party/SDL3 third_party/SDL3)
    ponder_add_imgui_target()

    ponder_require_dependency_target(spdlog)
    ponder_require_dependency_target(gtest)
    ponder_require_dependency_target(nlohmann_json)
    ponder_require_dependency_target(SDL3-static)
    ponder_require_dependency_target(ponder_imgui)

    ponder_add_alias(ponder::spdlog spdlog)
    ponder_add_alias(ponder::gtest gtest)
    ponder_add_alias(ponder::gtest_main gtest_main)
    ponder_add_alias(ponder::nlohmann_json nlohmann_json)
    ponder_add_alias(ponder::SDL3 SDL3-static)
endfunction()
