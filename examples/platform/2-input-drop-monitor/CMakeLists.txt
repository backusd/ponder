ponder_add_project_executable(ponder_platform_2_input_drop_monitor
    OUTPUT_NAME ponder-platform-2-input-drop-monitor
    FOLDER examples/platform
    NO_INSTALL
    SOURCES
        Main.cpp
    PRIVATE_DEPS
        ponder::core
        ponder::io
        ponder::platform)
