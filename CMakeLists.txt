cmake_minimum_required(VERSION 3.28)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(PreventInSourceBuilds)
ponder_prevent_in_source_builds()

project(ponder
    VERSION 0.1.0
    DESCRIPTION "Open-source computational chemistry platform"
    LANGUAGES CXX
)

include(ProjectOptions)
include(CompilerWarnings)
include(PrecompiledHeaders)
include(ProjectTargets)
include(Sanitizers)
include(Dependencies)
include(InstallRules)

ponder_define_project_options()
ponder_configure_project_defaults()

set(BUILD_TESTING "${PONDER_BUILD_TESTS}" CACHE BOOL "Build ponder tests." FORCE)
include(CTest)

ponder_configure_dependencies()

add_subdirectory(libs)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(PONDER_BUILD_APPS)
    add_subdirectory(apps)
endif()

if(PONDER_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
