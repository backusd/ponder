#include <ponder/math/AxisAlignedBox.hpp>

namespace
{
[[maybe_unused]] void UseAxisAlignedBoxHeader()
{
    auto box = pond::math::AxisAlignedBox::Create(pond::math::Vector3{-1.0F, -2.0F, -3.0F},
                                                  pond::math::Vector3{1.0F, 2.0F, 3.0F});
    (void)box;
}
} // namespace