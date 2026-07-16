#include <ponder/ui/Color.hpp>

#include <type_traits>

namespace
{
[[maybe_unused]] constexpr pond::ui::SrgbStraightAlphaColor kColor{};
static_assert(std::is_standard_layout_v<pond::ui::SrgbStraightAlphaColor>);
static_assert(std::is_trivially_copyable_v<pond::ui::SrgbStraightAlphaColor>);
static_assert(std::is_nothrow_move_constructible_v<pond::ui::LinearPremultipliedColor>);
static_assert(std::is_standard_layout_v<pond::ui::PackedLinearPremultipliedRgba8>);
}
