#include <ponder/math/Frustum.hpp>

namespace
{
[[nodiscard]] constexpr bool EmptyOptionalFrustumPlaneIsConstantEvaluable()
{
    const auto plane = pond::math::detail::CreateOptionalFrustumPlane(
        pond::math::detail::FrustumPlaneCoefficients{0.0, 0.0, 0.0, 1.0});
    return plane.HasValue() && !plane.GetValue().has_value();
}

static_assert(EmptyOptionalFrustumPlaneIsConstantEvaluable());

[[maybe_unused]] void UseFrustumHeader()
{
    auto frustum = pond::math::Frustum::FromWorldToClip(pond::math::Matrix4x4::Identity());
    (void)frustum;
}
} // namespace
