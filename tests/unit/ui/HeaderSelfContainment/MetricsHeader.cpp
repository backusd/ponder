#include <ponder/ui/Metrics.hpp>

#include <type_traits>

namespace
{
[[maybe_unused]] constexpr pond::ui::UiTargetMetrics kMetrics{};
static_assert(std::is_standard_layout_v<pond::ui::UiTargetMetrics>);
static_assert(std::is_trivially_copyable_v<pond::ui::UiTargetMetrics>);
static_assert(std::is_nothrow_move_constructible_v<pond::ui::UiTargetMetrics>);
} // namespace
