ponder_add_project_library(platform
    PUBLIC_HEADERS
        include/ponder/platform/Display.hpp
        include/ponder/platform/Geometry.hpp
        include/ponder/platform/Identifiers.hpp
        include/ponder/platform/PlatformError.hpp
        include/ponder/platform/PlatformEvent.hpp
        include/ponder/platform/PlatformRuntime.hpp
        include/ponder/platform/Timing.hpp
        include/ponder/platform/Window.hpp
        include/ponder/platform/WindowGraphics.hpp
        include/ponder/platform/WindowState.hpp
    SOURCES
        src/Display.cpp
        src/EventPolling.cpp
        src/PlatformRuntime.cpp
        src/PlatformRuntimeBackend.cpp
        src/RuntimeChildRegistry.cpp
        src/SdlEventTranslator.cpp
        src/SdlError.cpp
        src/StringValidation.cpp
        src/Window.cpp
        src/WindowProperties.cpp
    PRIVATE_HEADERS
        src/PlatformRuntimeBackend.hpp
        src/PlatformRuntimeState.hpp
        src/RuntimeChildRegistry.hpp
        src/SdlEventTranslator.hpp
        src/SdlError.hpp
        src/StringValidation.hpp
        src/WindowImpl.hpp
    PUBLIC_DEPS
        ponder::core
    PRIVATE_DEPS
        ponder::SDL3)

# Keep platform PCH-free until platform-only measurements justify opting in.
set_target_properties(ponder_platform PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON)
