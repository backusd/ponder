ponder_add_project_executable(ponder_project_inspect
    OUTPUT_NAME ponder-project-inspect
    FOLDER tools
    SOURCES
        src/Main.cpp
    PRIVATE_DEPS
        ponder::core)