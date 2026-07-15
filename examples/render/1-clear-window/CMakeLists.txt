ponder_add_project_executable(ponder_render_clear_window
    OUTPUT_NAME ponder-render-clear-window
    FOLDER examples/render
    NO_INSTALL
    SOURCES
        Main.cpp
    PRIVATE_DEPS
        ponder::core
        ponder::platform
        ponder::render)
