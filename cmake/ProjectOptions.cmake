include_guard(GLOBAL)

function(ponder_define_project_options)
    option(PONDER_BUILD_TESTS "Build ponder tests." ON)
    option(PONDER_BUILD_APPS "Build ponder applications." ON)
    option(PONDER_BUILD_TOOLS "Build ponder developer tools." ON)
    option(PONDER_BUILD_PLUGINS "Build ponder plugins." ON)
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
        "PONDER_BUILD_PLUGINS=${PONDER_BUILD_PLUGINS};"
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
