ponder_add_project_executable(ponder_desktop
    OUTPUT_NAME ponder-desktop
    FOLDER apps
    SOURCES
        src/Main.cpp
    PRIVATE_DEPS
        ponder::core)