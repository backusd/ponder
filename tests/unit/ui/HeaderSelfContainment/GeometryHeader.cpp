#include <ponder/ui/Geometry.hpp>

#include <type_traits>

namespace
{
[[maybe_unused]] constexpr pond::ui::LogicalRect kRect{};
static_assert(std::is_standard_layout_v<pond::ui::LogicalPoint>);
static_assert(std::is_trivially_copyable_v<pond::ui::LogicalPoint>);
static_assert(std::is_nothrow_move_constructible_v<pond::ui::LogicalRect>);
static_assert(std::is_standard_layout_v<pond::ui::LogicalSize>);
} // namespace
