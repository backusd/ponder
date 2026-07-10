ponder_add_project_test(ponder_math_tests
    SOURCES
        AngleTests.cpp
        ConsumerLinkTests.cpp
        MathErrorTests.cpp
        ScalarTests.cpp
    PRIVATE_DEPS
        ponder::math
    LABELS
        math
        unit)

add_library(ponder_math_header_tests OBJECT
    HeaderSelfContainment/AngleHeader.cpp
    HeaderSelfContainment/MathErrorHeader.cpp
    HeaderSelfContainment/ScalarHeader.cpp)

target_compile_features(ponder_math_header_tests PRIVATE cxx_std_23)
target_link_libraries(ponder_math_header_tests PRIVATE ponder::math)

set_target_properties(ponder_math_header_tests PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON
    FOLDER "tests")

ponder_enable_warnings(ponder_math_header_tests)
ponder_enable_sanitizers(ponder_math_header_tests)

if(WIN32)
    ponder_add_project_test(ponder_math_reference_tests
        SOURCES
            DirectXReferenceTests.cpp
        PRIVATE_DEPS
            ponder::math
        LABELS
            math
            reference)

    set_target_properties(ponder_math_reference_tests PROPERTIES
        DISABLE_PRECOMPILE_HEADERS ON)
endif()
