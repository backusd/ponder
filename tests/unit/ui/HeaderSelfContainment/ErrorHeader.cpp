#include <ponder/ui/Error.hpp>

#include <type_traits>

namespace
{
[[maybe_unused]] constexpr pond::core::ErrorCode kCode =
    pond::ui::ToErrorCode(pond::ui::UiErrorCode::InvalidPaintValue);
static_assert(std::is_trivially_copyable_v<pond::ui::UiErrorCode>);
static_assert(std::is_nothrow_move_constructible_v<pond::ui::UiErrorCode>);
}
