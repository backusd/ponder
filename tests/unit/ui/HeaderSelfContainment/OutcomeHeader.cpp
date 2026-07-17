#include <ponder/ui/Outcome.hpp>

#include <type_traits>

namespace
{
[[maybe_unused]] constexpr pond::ui::UiDrawCounters kCounters{};
static_assert(std::is_standard_layout_v<pond::ui::UiDrawCounters>);
static_assert(std::is_trivially_copyable_v<pond::ui::UiDrawCounters>);
static_assert(std::is_nothrow_move_constructible_v<pond::ui::UiDrawOutcome>);
} // namespace
