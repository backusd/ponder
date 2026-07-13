ponder_add_project_executable(ponder_platform_1_window_display_lab
    OUTPUT_NAME ponder-platform-1-window-display-lab
    FOLDER examples/platform
    NO_INSTALL
    SOURCES
        Main.cpp
    PRIVATE_DEPS
        ponder::core
        ponder::platform)
