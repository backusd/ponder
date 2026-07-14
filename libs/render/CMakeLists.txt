ponder_add_project_library(render
    PUBLIC_HEADERS
        include/ponder/render/Bootstrap.hpp
        include/ponder/render/RenderError.hpp
    SOURCES
        src/Bootstrap.cpp
        src/VulkanBootstrap.cpp
        src/VulkanBootstrap.hpp
    PUBLIC_DEPS
        ponder::core
        ponder::platform
    PRIVATE_DEPS
        ponder::vulkan_headers
        ponder::volk
        ponder::vma)

target_compile_definitions(ponder_render PRIVATE
    PONDER_RENDER_ENABLE_VALIDATION=$<BOOL:${PONDER_RENDER_ENABLE_VALIDATION}>
    PONDER_RENDER_DEVELOPER_BUILD=$<CONFIG:Debug>)
if(WIN32)
    target_compile_definitions(ponder_render PRIVATE
        VK_USE_PLATFORM_WIN32_KHR
        WIN32_LEAN_AND_MEAN
        NOMINMAX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
    find_path(PONDER_WAYLAND_CLIENT_INCLUDE_DIR
        NAMES wayland-client.h
        REQUIRED)

    target_compile_definitions(ponder_render PRIVATE
        VK_USE_PLATFORM_XLIB_KHR
        VK_USE_PLATFORM_WAYLAND_KHR)
    target_include_directories(ponder_render SYSTEM PRIVATE
        ${X11_INCLUDE_DIR}
        ${PONDER_WAYLAND_CLIENT_INCLUDE_DIR})
endif()

