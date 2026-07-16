#include <ponder/ui/experimental/RectangleRenderer.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<pond::ui::experimental::RectangleRenderer>);
static_assert(
    std::is_nothrow_move_constructible_v<pond::ui::experimental::RectangleRenderer>);
