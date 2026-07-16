#include <ponder/render/draw2d/Draw2DLayer.hpp>
#include <ponder/render/draw2d/Draw2DPacket.hpp>
#include <ponder/ui/Library.hpp>

#include <concepts>
#include <string_view>
#include <type_traits>

static_assert(!std::copy_constructible<pond::render::draw2d::Draw2DPacket>);
static_assert(std::move_constructible<pond::render::draw2d::Draw2DPacket>);
static_assert(std::is_same_v<decltype(pond::ui::GetLibraryName()), std::string_view>);

namespace pond::ui::detail
{
[[nodiscard]] bool IsRenderIntegrationTopologyLinked() noexcept
{
    return pond::render::draw2d::detail::IsDraw2DLayerTopologyLinked() && !GetLibraryName().empty();
}
} // namespace pond::ui::detail
