include(RenderShaders)

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

add_library(ponder_render_draw2d_packet INTERFACE)
add_library(ponder::render_draw2d_packet ALIAS ponder_render_draw2d_packet)
target_compile_features(ponder_render_draw2d_packet INTERFACE cxx_std_23)
target_include_directories(ponder_render_draw2d_packet INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/private/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(ponder_render_draw2d_packet INTERFACE
    ponder::core)
set_target_properties(ponder_render_draw2d_packet PROPERTIES
    FOLDER "libs/render/private")

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
        ponder::render_draw2d_packet
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

add_library(ponder_render_draw2d STATIC
    private/include/ponder/render/draw2d/Draw2DLayer.hpp
    private/include/ponder/render/draw2d/Draw2DRectangleShaderInterface.hpp
    src/Draw2DLayer.cpp)
add_library(ponder::render_draw2d ALIAS ponder_render_draw2d)
target_compile_features(ponder_render_draw2d PRIVATE cxx_std_23)
target_link_libraries(ponder_render_draw2d
    PUBLIC
        ponder::render_draw2d_packet
        ponder::render)
set_target_properties(ponder_render_draw2d PROPERTIES
    FOLDER "libs/render/private"
    POSITION_INDEPENDENT_CODE ON)

ponder_enable_warnings(ponder_render_draw2d)
ponder_enable_sanitizers(ponder_render_draw2d)

set(ponder_render_draw2d_schema_fingerprint "0x05c436a9fda7b4f7")

ponder_add_render_shader(
    TARGET ponder_render_sentinel_shader
    SHADER_NAME Sentinel
    SOURCE shaders/Sentinel.hlsl
    ENTRY_POINT MainVs
    PROFILE vs_6_0
    STAGE vertex
    SCHEMA_FINGERPRINT ponder-render-sentinel-shader-toolchain-v1)

ponder_add_render_shader(
    TARGET ponder_render_draw2d_rectangle_vertex_shader
    SHADER_NAME Draw2DRectangleVertex
    SOURCE shaders/Draw2DRectangleVertex.hlsl
    ENTRY_POINT MainVs
    PROFILE vs_6_0
    STAGE vertex
    SCHEMA_FINGERPRINT "${ponder_render_draw2d_schema_fingerprint}"
)

ponder_add_render_shader(
    TARGET ponder_render_draw2d_rectangle_fragment_shader
    SHADER_NAME Draw2DRectangleFragment
    SOURCE shaders/Draw2DRectangleFragment.hlsl
    ENTRY_POINT MainPs
    PROFILE ps_6_0
    STAGE fragment
    SCHEMA_FINGERPRINT "${ponder_render_draw2d_schema_fingerprint}"
)

set(ponder_render_draw2d_rectangle_vertex_shader_generated_dir
    "${PROJECT_BINARY_DIR}/generated/render/shaders/Draw2DRectangleVertex")
set(ponder_render_draw2d_rectangle_fragment_shader_generated_dir
    "${PROJECT_BINARY_DIR}/generated/render/shaders/Draw2DRectangleFragment")

target_sources(ponder_render PRIVATE
    src/VulkanDraw2DPipeline.cpp
    src/VulkanDraw2DUpload.cpp
    "${ponder_render_draw2d_rectangle_vertex_shader_generated_dir}/Draw2DRectangleVertexShader.cpp"
    "${ponder_render_draw2d_rectangle_fragment_shader_generated_dir}/Draw2DRectangleFragmentShader.cpp")

target_include_directories(ponder_render PRIVATE
    "${ponder_render_draw2d_rectangle_vertex_shader_generated_dir}"
    "${ponder_render_draw2d_rectangle_fragment_shader_generated_dir}")

add_dependencies(ponder_render
    ponder_render_draw2d_rectangle_vertex_shader_artifact_generate
    ponder_render_draw2d_rectangle_fragment_shader_artifact_generate)

target_link_libraries(ponder_render_draw2d
    PRIVATE
        ponder::vulkan_headers
        volk::volk_headers
        ponder::vma
        ponder::render_draw2d_rectangle_vertex_shader
        ponder::render_draw2d_rectangle_fragment_shader)

add_custom_target(ponder_render_draw2d_rectangle_shader_interface_check
    COMMAND "${CMAKE_COMMAND}"
        -D
            "PONDER_DRAW2D_RECTANGLE_VERTEX_REFLECTION=${PROJECT_BINARY_DIR}/generated/render/shaders/Draw2DRectangleVertex/Draw2DRectangleVertex.reflection.json"
        -D
            "PONDER_DRAW2D_RECTANGLE_FRAGMENT_REFLECTION=${PROJECT_BINARY_DIR}/generated/render/shaders/Draw2DRectangleFragment/Draw2DRectangleFragment.reflection.json"
        -P "${PROJECT_SOURCE_DIR}/cmake/CheckDraw2DRectangleShaderInterface.cmake"
    DEPENDS
        ponder_render_draw2d_rectangle_vertex_shader_artifact_check
        ponder_render_draw2d_rectangle_fragment_shader_artifact_check
    COMMENT "Checking Draw2D rectangle shader reflected interface"
    VERBATIM)
set_target_properties(ponder_render_draw2d_rectangle_shader_interface_check PROPERTIES
    FOLDER "libs/render/generated")

# The production archive consumes the generated shader interfaces, so a normal render build must
# reject reflection that has drifted from the packet and pipeline contract.
add_dependencies(ponder_render ponder_render_draw2d_rectangle_shader_interface_check)
