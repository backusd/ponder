ponder_add_project_library(application
    PUBLIC_HEADERS
        include/ponder/application/Application.hpp
        include/ponder/application/ApplicationError.hpp
        include/ponder/application/Identifiers.hpp
    SOURCES
        src/Application.cpp
    PUBLIC_DEPS
        ponder::core
        ponder::platform)

# The public Application state is deliberately visible in its header. Keep the
# target PCH-free so self-containment and dependency costs remain observable.
set_target_properties(ponder_application PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON)
