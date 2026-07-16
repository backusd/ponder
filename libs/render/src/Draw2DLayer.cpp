#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/draw2d/Draw2DLayer.hpp>
#include <ponder/render/draw2d/Draw2DPacket.hpp>

#include <concepts>
#include <type_traits>

static_assert(!std::copy_constructible<pond::render::draw2d::Draw2DLayer>);
static_assert(std::move_constructible<pond::render::draw2d::Draw2DLayer>);
static_assert(std::is_nothrow_move_constructible_v<pond::render::draw2d::Draw2DLayer>);
static_assert(!std::copy_constructible<pond::render::draw2d::Draw2DPacket>);
static_assert(std::move_constructible<pond::render::draw2d::Draw2DPacket>);
static_assert(!std::copy_constructible<pond::render::draw2d::Draw2DPacketBuilder>);
static_assert(std::move_constructible<pond::render::draw2d::Draw2DPacketBuilder>);

namespace pond::render::draw2d::detail
{
bool IsDraw2DLayerTopologyLinked() noexcept
{
    (void)GetRequiredWindowGraphicsCompatibility();
    return true;
}
} // namespace pond::render::draw2d::detail