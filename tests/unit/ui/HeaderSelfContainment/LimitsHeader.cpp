#include <ponder/ui/Limits.hpp>

#include <type_traits>

namespace
{
[[maybe_unused]] constexpr pond::ui::UiHardLimits kLimits{};
static_assert(std::is_standard_layout_v<pond::ui::UiHardLimits>);
static_assert(std::is_trivially_copyable_v<pond::ui::UiHardLimits>);
static_assert(std::is_nothrow_move_constructible_v<pond::ui::UiLimitExceeded>);
}
