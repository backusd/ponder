#include <ponder/core/Numbers.hpp>
#include <ponder/math/Angle.hpp>
#include <ponder/math/AxisAlignedBox.hpp>
#include <ponder/math/CollisionClassification.hpp>
#include <ponder/math/Frustum.hpp>
#include <ponder/math/MathError.hpp>
#include <ponder/math/Matrix3x3.hpp>
#include <ponder/math/Matrix4x4.hpp>
#include <ponder/math/Plane.hpp>
#include <ponder/math/Quaternion.hpp>
#include <ponder/math/Ray.hpp>
#include <ponder/math/RayBoxHit.hpp>
#include <ponder/math/RayTriangleHit.hpp>
#include <ponder/math/Sphere.hpp>
#include <ponder/math/Triangle.hpp>
#include <ponder/math/Vector2.hpp>
#include <ponder/math/Vector3.hpp>
#include <ponder/math/Vector4.hpp>
#include <ponder/math/Viewport.hpp>

#include <concepts>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
template <typename Type>
concept StableValueSemantics =
    std::is_standard_layout_v<Type> && std::is_trivially_copyable_v<Type>;

template <typename Type, std::size_t FloatCount>
concept FloatValueLayout =
    StableValueSemantics<Type> && sizeof(Type) == sizeof(float) * FloatCount &&
    alignof(Type) == alignof(float);

template <typename Type>
concept ExactNoexceptEquality = requires(Type lhs, Type rhs) {
    { lhs == rhs } noexcept -> std::same_as<bool>;
};

template <typename Type>
concept ToleranceComparison = requires(Type lhs, Type rhs, pond::core::Tolerance tolerance) {
    { pond::math::IsNear(lhs, rhs, tolerance) } noexcept -> std::same_as<bool>;
};

template <typename Type>
concept ZeroFactory = requires {
    { Type::Zero() } noexcept -> std::same_as<Type>;
};

template <typename Type>
concept IdentityFactory = requires {
    { Type::Identity() } noexcept -> std::same_as<Type>;
};

static_assert(FloatValueLayout<pond::math::Vector2, 2>);
static_assert(FloatValueLayout<pond::math::Vector3, 3>);
static_assert(FloatValueLayout<pond::math::Vector4, 4>);
static_assert(FloatValueLayout<pond::math::Matrix3x3, 9>);
static_assert(FloatValueLayout<pond::math::Matrix4x4, 16>);
static_assert(FloatValueLayout<pond::math::Quaternion, 4>);

static_assert(StableValueSemantics<pond::math::Ray>);
static_assert(StableValueSemantics<pond::math::Plane>);
static_assert(StableValueSemantics<pond::math::Sphere>);
static_assert(StableValueSemantics<pond::math::AxisAlignedBox>);
static_assert(StableValueSemantics<pond::math::Triangle>);

static_assert(std::is_copy_constructible_v<pond::math::Frustum>);
static_assert(std::is_copy_assignable_v<pond::math::Frustum>);

static_assert(std::is_default_constructible_v<pond::math::Radians>);
static_assert(std::is_default_constructible_v<pond::math::Degrees>);
static_assert(std::is_default_constructible_v<pond::math::Vector2>);
static_assert(std::is_default_constructible_v<pond::math::Vector3>);
static_assert(std::is_default_constructible_v<pond::math::Vector4>);
static_assert(std::is_default_constructible_v<pond::math::Matrix3x3>);
static_assert(std::is_default_constructible_v<pond::math::Matrix4x4>);
static_assert(std::is_default_constructible_v<pond::math::Quaternion>);

static_assert(!std::is_default_constructible_v<pond::core::Tolerance>);
static_assert(!std::is_default_constructible_v<pond::math::Viewport>);
static_assert(!std::is_default_constructible_v<pond::math::Ray>);
static_assert(!std::is_default_constructible_v<pond::math::Plane>);
static_assert(!std::is_default_constructible_v<pond::math::Sphere>);
static_assert(!std::is_default_constructible_v<pond::math::AxisAlignedBox>);
static_assert(!std::is_default_constructible_v<pond::math::Triangle>);
static_assert(!std::is_default_constructible_v<pond::math::Frustum>);
static_assert(!std::is_default_constructible_v<pond::math::RayBoxHit>);
static_assert(!std::is_default_constructible_v<pond::math::RayTriangleHit>);

static_assert(ExactNoexceptEquality<pond::math::Radians>);
static_assert(ExactNoexceptEquality<pond::math::Degrees>);
static_assert(ExactNoexceptEquality<pond::core::Tolerance>);
static_assert(ExactNoexceptEquality<pond::math::Vector2>);
static_assert(ExactNoexceptEquality<pond::math::Vector3>);
static_assert(ExactNoexceptEquality<pond::math::Vector4>);
static_assert(ExactNoexceptEquality<pond::math::Matrix3x3>);
static_assert(ExactNoexceptEquality<pond::math::Matrix4x4>);
static_assert(ExactNoexceptEquality<pond::math::Quaternion>);
static_assert(ExactNoexceptEquality<pond::math::Viewport>);
static_assert(ExactNoexceptEquality<pond::math::Ray>);
static_assert(ExactNoexceptEquality<pond::math::Plane>);
static_assert(ExactNoexceptEquality<pond::math::Sphere>);
static_assert(ExactNoexceptEquality<pond::math::AxisAlignedBox>);
static_assert(ExactNoexceptEquality<pond::math::Triangle>);
static_assert(ExactNoexceptEquality<pond::math::RayBoxHit>);
static_assert(ExactNoexceptEquality<pond::math::RayTriangleHit>);

static_assert(ToleranceComparison<pond::math::Vector2>);
static_assert(ToleranceComparison<pond::math::Vector3>);
static_assert(ToleranceComparison<pond::math::Vector4>);
static_assert(ToleranceComparison<pond::math::Matrix3x3>);
static_assert(ToleranceComparison<pond::math::Matrix4x4>);
static_assert(ToleranceComparison<pond::math::Quaternion>);

static_assert(ZeroFactory<pond::math::Vector2>);
static_assert(ZeroFactory<pond::math::Vector3>);
static_assert(ZeroFactory<pond::math::Vector4>);
static_assert(ZeroFactory<pond::math::Matrix3x3>);
static_assert(ZeroFactory<pond::math::Matrix4x4>);
static_assert(IdentityFactory<pond::math::Matrix3x3>);
static_assert(IdentityFactory<pond::math::Matrix4x4>);
static_assert(IdentityFactory<pond::math::Quaternion>);

static_assert(noexcept(pond::math::ToRadians(pond::math::Degrees{180.0F})));
static_assert(noexcept(pond::math::ToDegrees(pond::math::Radians{pond::core::kPi})));
static_assert(noexcept(pond::math::Cross(pond::math::Vector3{1.0F, 0.0F, 0.0F},
                                         pond::math::Vector3{0.0F, 1.0F, 0.0F})));
static_assert(noexcept(pond::math::Matrix4x4::Translation(pond::math::Vector3{})));
static_assert(noexcept(pond::math::TransformPoint(pond::math::Matrix4x4::Identity(),
                                                  pond::math::Vector3{})));
static_assert(noexcept(pond::math::Conjugate(pond::math::Quaternion::Identity())));
static_assert(noexcept(pond::math::Rotate(pond::math::Quaternion::Identity(),
                                          pond::math::Vector3{})));

static_assert(std::same_as<decltype(pond::core::Tolerance::Create(0.0F, 0.0F)),
                           pond::core::Result<pond::core::Tolerance>>);
static_assert(std::same_as<decltype(pond::math::Normalize(pond::math::Vector3{})),
                           pond::core::Result<pond::math::Vector3>>);
static_assert(std::same_as<decltype(pond::math::Inverse(pond::math::Matrix3x3{})),
                           pond::core::Result<pond::math::Matrix3x3>>);
static_assert(std::same_as<decltype(pond::math::Inverse(pond::math::Matrix4x4{})),
                           pond::core::Result<pond::math::Matrix4x4>>);
static_assert(std::same_as<decltype(pond::math::PerspectiveDivide(pond::math::Vector4{})),
                           pond::core::Result<pond::math::Vector3>>);
static_assert(
    std::same_as<decltype(pond::math::Ray::Create(pond::math::Vector3{}, pond::math::Vector3{})),
                 pond::core::Result<pond::math::Ray>>);
static_assert(std::same_as<decltype(pond::math::Plane::Create(pond::math::Vector3{}, 0.0F)),
                           pond::core::Result<pond::math::Plane>>);
static_assert(std::same_as<decltype(pond::math::Sphere::Create(pond::math::Vector3{}, 0.0F)),
                           pond::core::Result<pond::math::Sphere>>);
static_assert(std::same_as<decltype(pond::math::AxisAlignedBox::Create(pond::math::Vector3{},
                                                                       pond::math::Vector3{})),
                           pond::core::Result<pond::math::AxisAlignedBox>>);
static_assert(
    std::same_as<decltype(pond::math::Triangle::Create(pond::math::Vector3{}, pond::math::Vector3{},
                                                       pond::math::Vector3{})),
                 pond::core::Result<pond::math::Triangle>>);
static_assert(std::same_as<decltype(pond::math::Frustum::FromWorldToClip(pond::math::Matrix4x4{})),
                           pond::core::Result<pond::math::Frustum>>);
static_assert(std::same_as<decltype(pond::math::Viewport::Create(0.0F, 0.0F, 1.0F, 1.0F)),
                           pond::core::Result<pond::math::Viewport>>);
static_assert(std::same_as<decltype(pond::math::Project(pond::math::Matrix4x4{},
                                                        std::declval<pond::math::Viewport>(),
                                                        pond::math::Vector3{})),
                           pond::core::Result<pond::math::Vector3>>);
static_assert(std::same_as<decltype(pond::math::Unproject(pond::math::Matrix4x4{},
                                                          std::declval<pond::math::Viewport>(),
                                                          pond::math::Vector3{})),
                           pond::core::Result<pond::math::Vector3>>);
static_assert(std::same_as<decltype(pond::math::UnprojectFromClipToWorld(
                               pond::math::Matrix4x4{}, std::declval<pond::math::Viewport>(),
                               pond::math::Vector3{})),
                           pond::core::Result<pond::math::Vector3>>);
static_assert(
    std::same_as<decltype(pond::math::Intersect(std::declval<const pond::math::Ray&>(),
                                                std::declval<const pond::math::AxisAlignedBox&>())),
                 std::optional<pond::math::RayBoxHit>>);
static_assert(
    std::same_as<decltype(pond::math::Intersect(std::declval<const pond::math::Ray&>(),
                                                std::declval<const pond::math::Triangle&>())),
                 std::optional<pond::math::RayTriangleHit>>);

constexpr pond::math::Matrix4x4 kColumnVectorTransform =
    pond::math::Matrix4x4::Translation(pond::math::Vector3{1.0F, 2.0F, 3.0F}) *
    pond::math::Matrix4x4::Scale(pond::math::Vector3{2.0F, 3.0F, 4.0F});
static_assert(pond::math::TransformPoint(kColumnVectorTransform,
                                         pond::math::Vector3{2.0F, 2.0F, 2.0F}) ==
              pond::math::Vector3{5.0F, 8.0F, 11.0F});
static_assert(pond::math::TransformVector(kColumnVectorTransform,
                                          pond::math::Vector3{2.0F, 2.0F, 2.0F}) ==
              pond::math::Vector3{4.0F, 6.0F, 8.0F});
static_assert(pond::math::Cross(pond::math::Vector3{1.0F, 0.0F, 0.0F},
                                pond::math::Vector3{0.0F, 1.0F, 0.0F}) ==
              pond::math::Vector3{0.0F, 0.0F, 1.0F});

template <typename Result>
void ExpectRecoverableFailure(Result result, pond::math::MathErrorCode expectedCode,
                              std::string_view expectedContext)
{
    ASSERT_FALSE(result.HasValue());

    const auto& error = result.GetError();
    EXPECT_EQ(error.GetCode(), pond::math::ToErrorCode(expectedCode));
    EXPECT_FALSE(error.GetMessage().empty());
    EXPECT_NE(error.GetMessage().find(expectedContext), std::string_view::npos);
}

[[nodiscard]] pond::core::Tolerance RequireTolerance()
{
    auto tolerance = pond::core::Tolerance::Create(1.0e-5F, 1.0e-5F);
    EXPECT_TRUE(tolerance.HasValue());
    return tolerance.GetValue();
}

TEST(ApiContractTests, RecoverableFailuresReturnStableCodesAndUsefulContext)
{
    constexpr float infinity = std::numeric_limits<float>::infinity();
    constexpr float quietNaN = std::numeric_limits<float>::quiet_NaN();
    const pond::core::Tolerance tolerance = RequireTolerance();

    ExpectRecoverableFailure(pond::math::Normalize(pond::math::Vector3{}),
                             pond::math::MathErrorCode::DegenerateInput, "Vector3");
    ExpectRecoverableFailure(pond::math::Matrix3x3{}.At(3, 0),
                             pond::math::MathErrorCode::InvalidArgument, "Matrix3x3");
    ExpectRecoverableFailure(pond::math::Inverse(pond::math::Matrix3x3::Zero()),
                             pond::math::MathErrorCode::SingularMatrix, "Matrix3x3");
    ExpectRecoverableFailure(pond::math::Inverse(pond::math::Matrix4x4::Zero()),
                             pond::math::MathErrorCode::SingularMatrix, "Matrix4x4");
    ExpectRecoverableFailure(pond::math::PerspectiveDivide(pond::math::Vector4{}),
                             pond::math::MathErrorCode::UndefinedHomogeneousCoordinate,
                             "Perspective division");
    ExpectRecoverableFailure(pond::math::Matrix4x4::LookTo(pond::math::Vector3{},
                                                           pond::math::Vector3{},
                                                           pond::math::Vector3{0.0F, 1.0F, 0.0F}),
                             pond::math::MathErrorCode::DegenerateInput, "Matrix4x4");
    ExpectRecoverableFailure(
        pond::math::Matrix4x4::Perspective(pond::math::Radians{}, 1.0F, 1.0F, 10.0F,
                                           pond::math::ProjectionDepth::ForwardZ),
        pond::math::MathErrorCode::InvalidArgument, "Matrix4x4");
    ExpectRecoverableFailure(pond::math::Quaternion::FromComponents(0.0F, 0.0F, 0.0F, 0.0F),
                             pond::math::MathErrorCode::DegenerateInput, "Quaternion");
    ExpectRecoverableFailure(pond::math::Slerp(pond::math::Quaternion::Identity(),
                                               pond::math::Quaternion::Identity(), infinity),
                             pond::math::MathErrorCode::NonFiniteInput, "Quaternion");
    ExpectRecoverableFailure(pond::math::ToQuaternion(pond::math::Matrix3x3::Zero(), tolerance),
                             pond::math::MathErrorCode::SingularMatrix, "Quaternion");
    ExpectRecoverableFailure(pond::math::Ray::Create(pond::math::Vector3{}, pond::math::Vector3{}),
                             pond::math::MathErrorCode::DegenerateInput, "Vector3");
    ExpectRecoverableFailure(pond::math::Plane::Create(pond::math::Vector3{}, 0.0F),
                             pond::math::MathErrorCode::DegenerateInput, "Plane");
    ExpectRecoverableFailure(pond::math::Sphere::Create(pond::math::Vector3{}, -1.0F),
                             pond::math::MathErrorCode::InvalidArgument, "Sphere");
    ExpectRecoverableFailure(pond::math::AxisAlignedBox::Create(
                                 pond::math::Vector3{1.0F, 0.0F, 0.0F}, pond::math::Vector3{}),
                             pond::math::MathErrorCode::InvalidArgument, "AxisAlignedBox");
    ExpectRecoverableFailure(pond::math::Triangle::Create(pond::math::Vector3{quietNaN, 0.0F, 0.0F},
                                                          pond::math::Vector3{},
                                                          pond::math::Vector3{}),
                             pond::math::MathErrorCode::NonFiniteInput, "Triangle");
    ExpectRecoverableFailure(pond::math::Frustum::FromWorldToClip(pond::math::Matrix4x4::Zero()),
                             pond::math::MathErrorCode::DegenerateInput, "Frustum");
    ExpectRecoverableFailure(pond::math::Viewport::Create(0.0F, 0.0F, 0.0F, 1.0F),
                             pond::math::MathErrorCode::InvalidArgument, "Viewport");
}
} // namespace
