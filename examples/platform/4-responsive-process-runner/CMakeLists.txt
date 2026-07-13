ponder_add_project_executable(ponder_platform_4_responsive_process_runner
    OUTPUT_NAME ponder-platform-4-responsive-process-runner
    FOLDER examples/platform
    NO_INSTALL
    SOURCES
        Main.cpp
    PRIVATE_DEPS
        ponder::core
        ponder::io
        ponder::platform)
