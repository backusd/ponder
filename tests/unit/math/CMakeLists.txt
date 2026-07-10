ponder_add_project_test(ponder_math_tests
    SOURCES
        AngleTests.cpp
        ApiContractTests.cpp
        ConsumerLinkTests.cpp
        CollisionClassificationTests.cpp
        CollisionIntersectionTests.cpp
        CollisionMilestoneTests.cpp
        RayTriangleIntersectionTests.cpp
        CollisionValueTests.cpp
        MathErrorTests.cpp
        Matrix3x3AlgebraTests.cpp
        Matrix4x4AlgebraTests.cpp
        MatrixTests.cpp
        QuaternionTests.cpp
        RendererReadyTests.cpp
        ScalarTests.cpp
        VectorAlgebraTests.cpp
        ViewportTests.cpp
        VectorTests.cpp
    PRIVATE_DEPS
        ponder::math
    LABELS
        math
        unit)

add_library(ponder_math_header_tests OBJECT
    HeaderSelfContainment/AngleHeader.cpp
    HeaderSelfContainment/AxisAlignedBoxHeader.cpp
    HeaderSelfContainment/CollisionClassificationHeader.cpp
    HeaderSelfContainment/FrustumHeader.cpp
    HeaderSelfContainment/PlaneHeader.cpp
    HeaderSelfContainment/RayHeader.cpp
    HeaderSelfContainment/RayBoxHitHeader.cpp
    HeaderSelfContainment/RayTriangleHitHeader.cpp
    HeaderSelfContainment/SphereHeader.cpp
    HeaderSelfContainment/TriangleHeader.cpp
    HeaderSelfContainment/MathErrorHeader.cpp
    HeaderSelfContainment/Matrix3x3Header.cpp
    HeaderSelfContainment/Matrix4x4Header.cpp
    HeaderSelfContainment/QuaternionHeader.cpp
    HeaderSelfContainment/ScalarHeader.cpp
    HeaderSelfContainment/Vector2Header.cpp
    HeaderSelfContainment/Vector3Header.cpp
    HeaderSelfContainment/Vector4Header.cpp
    HeaderSelfContainment/ViewportHeader.cpp)

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
