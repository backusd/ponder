#include <ponder/ui/Error.hpp>

#include <type_traits>

static_assert(std::is_trivially_copyable_v<pond::ui::UiErrorCode>);
