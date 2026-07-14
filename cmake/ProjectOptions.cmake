include_guard(GLOBAL)

function(ponder_define_project_options)
    option(PONDER_BUILD_TESTS "Build ponder tests." ON)
    option(PONDER_BUILD_APPS "Build ponder applications." ON)
    option(PONDER_BUILD_TOOLS "Build ponder developer tools." ON)
    option(PONDER_BUILD_EXAMPLES "Build ponder examples." ON)
    option(PONDER_BUILD_PLUGINS "Build ponder plugins." ON)
    option(PONDER_BUILD_RENDER "Build the ponder render library." ON)

    set(ponder_render_vulkan_default OFF)
    if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(ponder_render_vulkan_default ON)
    endif()

    option(PONDER_RENDER_ENABLE_VULKAN
        "Enable the Vulkan render backend on Windows and Linux."
        "${ponder_render_vulkan_default}")
    option(PONDER_RENDER_ENABLE_VALIDATION
        "Enable Vulkan validation support in the render backend."
        "${PONDER_RENDER_ENABLE_VULKAN}")
    option(PONDER_BUILD_RENDER_INTEGRATION_TESTS
        "Build live render integration tests."
        "${PONDER_BUILD_TESTS}")

    if(NOT PONDER_BUILD_RENDER)
        set(PONDER_RENDER_ENABLE_VULKAN OFF CACHE BOOL
            "Enable the Vulkan render backend on Windows and Linux." FORCE)
        set(PONDER_RENDER_ENABLE_VALIDATION OFF CACHE BOOL
            "Enable Vulkan validation support in the render backend." FORCE)
        set(PONDER_BUILD_RENDER_INTEGRATION_TESTS OFF CACHE BOOL
            "Build live render integration tests." FORCE)
    else()
        if(APPLE)
            message(FATAL_ERROR
                "ponder_render does not have a macOS backend yet. Configure with "
                "-DPONDER_BUILD_RENDER=OFF until the native Metal backend is implemented.")
        endif()

        if(PONDER_RENDER_ENABLE_VULKAN AND NOT ponder_render_vulkan_default)
            message(FATAL_ERROR
                "PONDER_RENDER_ENABLE_VULKAN is supported only on Windows and Linux. "
                "Disable render with -DPONDER_BUILD_RENDER=OFF on this platform.")
        endif()

        if(NOT PONDER_RENDER_ENABLE_VULKAN)
            set(PONDER_RENDER_ENABLE_VALIDATION OFF CACHE BOOL
                "Enable Vulkan validation support in the render backend." FORCE)
            message(FATAL_ERROR
                "PONDER_BUILD_RENDER requires the build-selected Vulkan backend on Windows "
                "and Linux. Set -DPONDER_RENDER_ENABLE_VULKAN=ON or disable render with "
                "-DPONDER_BUILD_RENDER=OFF.")
        endif()

        if(NOT PONDER_BUILD_TESTS)
            set(PONDER_BUILD_RENDER_INTEGRATION_TESTS OFF CACHE BOOL
                "Build live render integration tests." FORCE)
        endif()
    endif()
    option(PONDER_ENABLE_PCH
        "Enable explicitly opted-in target-scoped precompiled headers." ON)
    option(PONDER_ENABLE_ASAN "Enable AddressSanitizer for supported compilers." OFF)
    option(PONDER_ENABLE_UBSAN
        "Enable UndefinedBehaviorSanitizer for supported compilers."
        OFF)
    option(PONDER_ENABLE_INSTALL_RULES "Enable install rules for ponder targets." OFF)

    set_property(GLOBAL PROPERTY PONDER_CONFIGURED_OPTIONS
        "PONDER_BUILD_TESTS=${PONDER_BUILD_TESTS};"
        "PONDER_BUILD_APPS=${PONDER_BUILD_APPS};"
        "PONDER_BUILD_TOOLS=${PONDER_BUILD_TOOLS};"
        "PONDER_BUILD_EXAMPLES=${PONDER_BUILD_EXAMPLES};"
        "PONDER_BUILD_PLUGINS=${PONDER_BUILD_PLUGINS};"
        "PONDER_BUILD_RENDER=${PONDER_BUILD_RENDER};"
        "PONDER_RENDER_ENABLE_VULKAN=${PONDER_RENDER_ENABLE_VULKAN};"
        "PONDER_RENDER_ENABLE_VALIDATION=${PONDER_RENDER_ENABLE_VALIDATION};"
        "PONDER_BUILD_RENDER_INTEGRATION_TESTS=${PONDER_BUILD_RENDER_INTEGRATION_TESTS};"
        "PONDER_ENABLE_PCH=${PONDER_ENABLE_PCH};"
        "PONDER_ENABLE_ASAN=${PONDER_ENABLE_ASAN};"
        "PONDER_ENABLE_UBSAN=${PONDER_ENABLE_UBSAN};"
        "PONDER_ENABLE_INSTALL_RULES=${PONDER_ENABLE_INSTALL_RULES}")
endfunction()

macro(ponder_configure_project_defaults)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(CMAKE_UNITY_BUILD OFF CACHE BOOL "Enable unity builds." FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries." FORCE)

    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
endmacro()
