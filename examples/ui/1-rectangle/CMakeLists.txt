ponder_add_project_executable(ponder_ui_1_rectangle
    OUTPUT_NAME ponder-ui-1-rectangle
    FOLDER examples/ui
    NO_INSTALL
    SOURCES
        Main.cpp
    PRIVATE_DEPS
        ponder::core
        ponder::platform
        ponder::render
        ponder::ui_experimental)
