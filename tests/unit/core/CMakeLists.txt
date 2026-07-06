ponder_add_project_test(ponder_core_tests
    SOURCES
        ErrorTests.cpp
        ResultTests.cpp
        SmokeTests.cpp
    PRIVATE_DEPS
        ponder::core)
