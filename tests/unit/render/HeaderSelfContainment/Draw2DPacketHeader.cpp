#include <ponder/render/draw2d/Draw2DPacket.hpp>

#include <concepts>
#include <type_traits>

namespace
{
static_assert(std::is_standard_layout_v<pond::render::draw2d::Draw2DVertex>);
static_assert(!std::copy_constructible<pond::render::draw2d::Draw2DPacket>);
static_assert(std::move_constructible<pond::render::draw2d::Draw2DPacket>);
static_assert(!std::copy_constructible<pond::render::draw2d::Draw2DPacketBuilder>);
static_assert(std::move_constructible<pond::render::draw2d::Draw2DPacketBuilder>);
[[maybe_unused]] pond::render::draw2d::Draw2DPacketBuilder builder;
} // namespace