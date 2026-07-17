#include <ponder/ui/experimental/RectangleRenderer.hpp>

#include <concepts>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

using RectangleBatch = std::span<const pond::ui::experimental::RectanglePaint>;

static_assert(!std::copy_constructible<pond::ui::experimental::RectangleRenderer>);
static_assert(std::move_constructible<pond::ui::experimental::RectangleRenderer>);
static_assert(!std::is_copy_assignable_v<pond::ui::experimental::RectangleRenderer>);
static_assert(std::is_move_assignable_v<pond::ui::experimental::RectangleRenderer>);
static_assert(std::is_nothrow_default_constructible_v<pond::ui::experimental::RectangleRenderer>);
static_assert(std::is_nothrow_move_constructible_v<pond::ui::experimental::RectangleRenderer>);
static_assert(std::is_nothrow_move_assignable_v<pond::ui::experimental::RectangleRenderer>);
static_assert(std::is_same_v<decltype(pond::ui::experimental::kRectangleFacadeExperimentalNotice),
                             const std::string_view>);
static_assert(
    std::is_same_v<decltype(std::declval<pond::ui::experimental::RectangleRenderer&>().Record(
                       std::declval<pond::render::RenderFrame&>(),
                       std::declval<const pond::ui::UiTargetMetrics&>(),
                       std::declval<pond::ui::experimental::RectanglePaint>())),
                   pond::core::Result<pond::ui::experimental::RectangleRecordOutcome>>);
static_assert(std::is_same_v<decltype(std::declval<pond::ui::experimental::RectangleRenderer&>()
                                          .Record(std::declval<pond::render::RenderFrame&>(),
                                                  std::declval<const pond::ui::UiTargetMetrics&>(),
                                                  std::declval<RectangleBatch>())),
                             pond::core::Result<pond::ui::experimental::RectangleRecordOutcome>>);
