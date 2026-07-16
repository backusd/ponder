#include <ponder/render/draw2d/Draw2DLayer.hpp>

#include <concepts>
#include <type_traits>

namespace
{
static_assert(!std::copy_constructible<pond::render::draw2d::Draw2DLayer>);
static_assert(std::move_constructible<pond::render::draw2d::Draw2DLayer>);
static_assert(std::is_nothrow_move_constructible_v<pond::render::draw2d::Draw2DLayer>);
[[maybe_unused]] pond::render::draw2d::Draw2DLayer layer;
} // namespace