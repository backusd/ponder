ponder_add_project_executable(ponder_platform_3_desktop_services_workbench
    OUTPUT_NAME ponder-platform-3-desktop-services-workbench
    FOLDER examples/platform
    NO_INSTALL
    SOURCES
        Main.cpp
    PRIVATE_DEPS
        ponder::core
        ponder::io
        ponder::platform)
