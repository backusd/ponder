# Keep the dynamic-loader implementation inside the installed render archive. Static consumers
# must not need a private, separately exported Volk target.
set(ponder_render_volk_source
    "${PROJECT_SOURCE_DIR}/third_party/volk/volk.c")
set_source_files_properties("${ponder_render_volk_source}" PROPERTIES
    # The top-level project intentionally enables only CXX.
    LANGUAGE CXX)

add_library(ponder_render_volk OBJECT
    "${ponder_render_volk_source}")
target_link_libraries(ponder_render_volk PRIVATE
    volk::volk_headers)
set_target_properties(ponder_render_volk PROPERTIES
    FOLDER "libs/render/private"
    POSITION_INDEPENDENT_CODE ON)

ponder_add_project_library(render
    PUBLIC_HEADERS
        include/ponder/render/Bootstrap.hpp
        include/ponder/render/RenderError.hpp
    SOURCES
        src/Bootstrap.cpp
        src/RenderBootstrapTestAccess.hpp
        src/VulkanBootstrap.cpp
        src/VulkanBootstrap.hpp
        src/VulkanDiagnostics.hpp
        src/VulkanQueueSynchronization.hpp
        $<TARGET_OBJECTS:ponder_render_volk>
    PUBLIC_DEPS
        ponder::core
        ponder::platform
    PRIVATE_DEPS
        ponder::vulkan_headers
        volk::volk_headers
        ponder::vma)

if(CMAKE_DL_LIBS)
    target_link_libraries(ponder_render PRIVATE "${CMAKE_DL_LIBS}")
endif()

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

