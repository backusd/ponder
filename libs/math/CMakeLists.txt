ponder_add_project_library(math
    PUBLIC_HEADERS
        include/ponder/math/Angle.hpp
        include/ponder/math/AxisAlignedBox.hpp
        include/ponder/math/CollisionClassification.hpp
        include/ponder/math/Frustum.hpp
        include/ponder/math/MathError.hpp
        include/ponder/math/Matrix3x3.hpp
        include/ponder/math/Matrix4x4.hpp
        include/ponder/math/Plane.hpp
        include/ponder/math/Quaternion.hpp
        include/ponder/math/Ray.hpp
        include/ponder/math/RayBoxHit.hpp
        include/ponder/math/RayTriangleHit.hpp
        include/ponder/math/Scalar.hpp
        include/ponder/math/Sphere.hpp
        include/ponder/math/Triangle.hpp
        include/ponder/math/Viewport.hpp
        include/ponder/math/Vector2.hpp
        include/ponder/math/Vector3.hpp
        include/ponder/math/Vector4.hpp
    SOURCES
        src/MathError.cpp
    PUBLIC_DEPS
        ponder::core)
add_library(ponder_math_examples OBJECT
    docs/examples/RendererExamples.cpp)

target_compile_features(ponder_math_examples PRIVATE cxx_std_23)
target_link_libraries(ponder_math_examples PRIVATE ponder::math)

set_target_properties(ponder_math_examples PROPERTIES
    DISABLE_PRECOMPILE_HEADERS ON
    FOLDER "docs")

ponder_enable_warnings(ponder_math_examples)
ponder_enable_sanitizers(ponder_math_examples)
