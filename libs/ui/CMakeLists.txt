ponder_add_project_library(ui
    PUBLIC_HEADERS
        include/ponder/ui/Color.hpp
        include/ponder/ui/Error.hpp
        include/ponder/ui/Geometry.hpp
        include/ponder/ui/Library.hpp
        include/ponder/ui/Limits.hpp
        include/ponder/ui/Metrics.hpp
        include/ponder/ui/Outcome.hpp
    SOURCES
        src/Library.cpp
    PUBLIC_DEPS
        ponder::core)

add_library(ponder_ui_paint STATIC
    src/PaintRecorder.cpp)
add_library(ponder::ui_paint ALIAS ponder_ui_paint)
target_compile_features(ponder_ui_paint PUBLIC cxx_std_23)
target_include_directories(ponder_ui_paint PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/private/include>")
target_link_libraries(ponder_ui_paint PUBLIC
    ponder::ui)
set_target_properties(ponder_ui_paint PROPERTIES
    FOLDER "libs/ui/private"
    POSITION_INDEPENDENT_CODE ON)

ponder_enable_warnings(ponder_ui_paint)
ponder_enable_sanitizers(ponder_ui_paint)

if(PONDER_BUILD_UI_RENDER_INTEGRATION)
    add_library(ponder_ui_render_integration STATIC
        src/PaintCompiler.cpp
        src/RenderIntegration.cpp
        src/RenderIntegrationTopology.cpp)
    add_library(ponder::ui_render_integration ALIAS ponder_ui_render_integration)
    target_compile_features(ponder_ui_render_integration PRIVATE cxx_std_23)
    target_include_directories(ponder_ui_render_integration PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/private/include>")
    target_link_libraries(ponder_ui_render_integration
        PUBLIC
            ponder::ui
            ponder::render
        PRIVATE
            ponder::ui_paint
            ponder::render_draw2d
            ponder::render_draw2d_packet)
    set_target_properties(ponder_ui_render_integration PROPERTIES
        FOLDER "libs/ui/private"
        POSITION_INDEPENDENT_CODE ON)

    ponder_enable_warnings(ponder_ui_render_integration)
    ponder_enable_sanitizers(ponder_ui_render_integration)

    ponder_add_project_library(ui_experimental
        PUBLIC_HEADERS
            include/ponder/ui/experimental/RectangleRenderer.hpp
        SOURCES
            src/PaintRecorder.cpp
            src/PaintCompiler.cpp
            src/RenderIntegration.cpp
            src/RectangleRenderer.cpp
        PUBLIC_DEPS
            ponder::ui
            ponder::render
        PRIVATE_DEPS
            ponder::render_draw2d
            ponder::render_draw2d_packet)

    target_include_directories(ponder_ui_experimental PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/private/include>")
endif()
