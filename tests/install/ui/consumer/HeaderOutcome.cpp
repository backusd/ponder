#include <ponder/ui/Outcome.hpp>

#include <type_traits>

static_assert(std::is_standard_layout_v<pond::ui::UiDrawCounters>);
static_assert(std::is_trivially_copyable_v<pond::ui::UiDrawCounters>);
