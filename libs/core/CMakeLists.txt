include(BuildInfo)

ponder_configure_build_info(PONDER_CORE_BUILD_INFO_SOURCE
    TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInfo.cpp.in"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated/src")

set_source_files_properties("${PONDER_CORE_BUILD_INFO_SOURCE}" PROPERTIES
    COMPILE_DEFINITIONS "PONDER_BUILD_INFO_ACTIVE_BUILD_TYPE=\"$<CONFIG>\"")

ponder_add_project_library(core
    PUBLIC_HEADERS
        include/ponder/core/Assert.hpp
        include/ponder/core/BuildInfo.hpp
        include/ponder/core/Library.hpp
        include/ponder/core/Log.hpp
        include/ponder/core/PonderException.hpp
        include/ponder/core/Result.hpp
        include/ponder/core/ScopeExit.hpp
        include/ponder/core/StackTrace.hpp
        include/ponder/core/String.hpp
        include/ponder/core/Uuid.hpp
    SOURCES
        "${PONDER_CORE_BUILD_INFO_SOURCE}"
        src/Assert.cpp
        src/Library.cpp
        src/Log.cpp
        src/PonderException.cpp
        src/Result.cpp
        src/StackTrace.cpp
        src/String.cpp
        src/Uuid.cpp
    PRIVATE_HEADERS
        src/Pch.hpp
    PRIVATE_DEPS
        ponder::spdlog
        ponder::moodycamel)
