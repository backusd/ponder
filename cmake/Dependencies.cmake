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

    if(PONDER_ENABLE_INSTALL_RULES)
        # Dependency-owned package rules must participate in the top-level install so exported
        # static targets can resolve their link-only implementation requirements.
        add_subdirectory("${source_dir}" "${binary_dir}" SYSTEM)
    else()
        add_subdirectory("${source_dir}" "${binary_dir}" EXCLUDE_FROM_ALL SYSTEM)
    endif()
endfunction()

function(ponder_require_dependency_target target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Required dependency target is missing: ${target_name}")
    endif()
endfunction()

function(ponder_configure_vulkan_dependencies)
    if(NOT PONDER_BUILD_RENDER OR NOT PONDER_RENDER_ENABLE_VULKAN)
        return()
    endif()

    ponder_set_dependency_option(VULKAN_HEADERS_ENABLE_TESTS OFF BOOL
        "Disable Vulkan-Headers tests.")
    ponder_set_dependency_option(VULKAN_HEADERS_ENABLE_INSTALL OFF BOOL
        "Disable Vulkan-Headers install rules.")
    ponder_set_dependency_option(VULKAN_HEADERS_ENABLE_MODULE OFF BOOL
        "Disable Vulkan-Hpp named module targets.")

    set(vulkan_headers_dir "${PROJECT_SOURCE_DIR}/third_party/Vulkan-Headers")
    ponder_set_dependency_option(VULKAN_HEADERS_INSTALL_DIR
        "${vulkan_headers_dir}" PATH
        "Use the repository-managed Vulkan-Headers checkout for Volk.")
    ponder_set_dependency_option(VOLK_PULL_IN_VULKAN ON BOOL
        "Let Volk consume the repository-managed Vulkan headers.")
    ponder_set_dependency_option(VOLK_INSTALL OFF BOOL
        "Disable Volk install rules.")
    ponder_set_dependency_option(VOLK_HEADERS_ONLY OFF BOOL
        "Build the Volk static loader dispatch library.")
    ponder_set_dependency_option(VOLK_NAMESPACE OFF BOOL
        "Keep Volk C symbols un-namespaced.")

    ponder_set_dependency_option(VMA_ENABLE_INSTALL OFF BOOL
        "Disable Vulkan Memory Allocator install rules.")
    ponder_set_dependency_option(VMA_BUILD_DOCUMENTATION OFF BOOL
        "Disable Vulkan Memory Allocator documentation.")
    ponder_set_dependency_option(VMA_BUILD_SAMPLES OFF BOOL
        "Disable Vulkan Memory Allocator samples.")

    ponder_add_third_party_subdirectory(
        third_party/Vulkan-Headers
        third_party/Vulkan-Headers)
    ponder_add_third_party_subdirectory(third_party/volk third_party/volk)
    ponder_add_third_party_subdirectory(
        third_party/VulkanMemoryAllocator
        third_party/VulkanMemoryAllocator)

    ponder_require_dependency_target(Vulkan-Headers)
    ponder_require_dependency_target(volk)
    ponder_require_dependency_target(VulkanMemoryAllocator)

    target_link_libraries(VulkanMemoryAllocator INTERFACE Vulkan::Headers)
    target_compile_definitions(VulkanMemoryAllocator INTERFACE
        VMA_DYNAMIC_VULKAN_FUNCTIONS=1
        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_VULKAN_VERSION=1002000)

    set_target_properties(Vulkan-Headers volk VulkanMemoryAllocator PROPERTIES
        FOLDER "third_party")

    ponder_add_alias(ponder::vulkan_headers Vulkan-Headers)
    ponder_add_alias(ponder::volk volk)
    ponder_add_alias(ponder::vma VulkanMemoryAllocator)
endfunction()
function(ponder_configure_dependencies)
    ponder_set_dependency_option(SPDLOG_BUILD_EXAMPLE OFF BOOL "Disable spdlog examples.")
    ponder_set_dependency_option(SPDLOG_BUILD_EXAMPLE_HO OFF BOOL "Disable spdlog header-only examples.")
    ponder_set_dependency_option(SPDLOG_BUILD_TESTS OFF BOOL "Disable spdlog tests.")
    ponder_set_dependency_option(SPDLOG_BUILD_TESTS_HO OFF BOOL "Disable spdlog header-only tests.")
    ponder_set_dependency_option(SPDLOG_BUILD_BENCH OFF BOOL "Disable spdlog benchmarks.")
    ponder_set_dependency_option(SPDLOG_INSTALL "${PONDER_ENABLE_INSTALL_RULES}" BOOL
        "Install spdlog when ponder package rules are enabled.")
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
    ponder_set_dependency_option(SDL_INSTALL "${PONDER_ENABLE_INSTALL_RULES}" BOOL
        "Install SDL3 when ponder package rules are enabled.")
    ponder_set_dependency_option(SDL_INSTALL_TESTS OFF BOOL "Disable SDL3 test installation.")

    ponder_add_third_party_subdirectory(third_party/spdlog third_party/spdlog)
    ponder_add_third_party_subdirectory(third_party/googletest third_party/googletest)
    ponder_add_third_party_subdirectory(third_party/nlohmann_json third_party/nlohmann_json)
    ponder_add_third_party_subdirectory(third_party/moodycamel third_party/moodycamel)
    ponder_add_third_party_subdirectory(third_party/SDL3 third_party/SDL3)
    ponder_configure_vulkan_dependencies()

    ponder_require_dependency_target(spdlog)
    ponder_require_dependency_target(gtest)
    ponder_require_dependency_target(nlohmann_json)
    ponder_require_dependency_target(concurrentqueue)
    ponder_require_dependency_target(SDL3-static)
    if(PONDER_BUILD_RENDER AND PONDER_RENDER_ENABLE_VULKAN)
        ponder_require_dependency_target(Vulkan-Headers)
        ponder_require_dependency_target(volk)
        ponder_require_dependency_target(VulkanMemoryAllocator)
    endif()

    ponder_add_alias(ponder::spdlog spdlog)
    ponder_add_alias(ponder::gtest gtest)
    ponder_add_alias(ponder::gtest_main gtest_main)
    ponder_add_alias(ponder::nlohmann_json nlohmann_json)
    ponder_add_alias(ponder::moodycamel concurrentqueue)
    ponder_add_alias(ponder::SDL3 SDL3-static)
endfunction()
