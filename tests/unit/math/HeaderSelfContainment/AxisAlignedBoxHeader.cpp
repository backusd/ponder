#include <ponder/math/AxisAlignedBox.hpp>

namespace
{
[[nodiscard]] constexpr bool AxisAlignedBoxFactoryIsConstantEvaluable()
{
    const auto box = pond::math::AxisAlignedBox::Create(pond::math::Vector3{-1.0F, -2.0F, -3.0F},
                                                        pond::math::Vector3{1.0F, 2.0F, 3.0F});
    return box.HasValue() &&
           box.GetValue().GetMinimum() == pond::math::Vector3{-1.0F, -2.0F, -3.0F} &&
           box.GetValue().GetMaximum() == pond::math::Vector3{1.0F, 2.0F, 3.0F};
}

static_assert(AxisAlignedBoxFactoryIsConstantEvaluable());

[[maybe_unused]] void UseAxisAlignedBoxHeader()
{
    auto box = pond::math::AxisAlignedBox::Create(pond::math::Vector3{-1.0F, -2.0F, -3.0F},
                                                  pond::math::Vector3{1.0F, 2.0F, 3.0F});
    (void)box;
}
} // namespace
