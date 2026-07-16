#include <ponder/render/Bootstrap.hpp>

#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

namespace
{
template <typename Owner>
constexpr bool IsRenderOwnerRole()
{
    return std::is_default_constructible_v<Owner> && !std::is_copy_constructible_v<Owner> &&
           !std::is_copy_assignable_v<Owner> && std::is_nothrow_move_constructible_v<Owner> &&
           std::is_nothrow_move_assignable_v<Owner> && std::is_nothrow_destructible_v<Owner>;
}

static_assert(IsRenderOwnerRole<pond::render::RenderBootstrap>());
static_assert(IsRenderOwnerRole<pond::render::RenderDevice>());
static_assert(IsRenderOwnerRole<pond::render::PreparedSurface>());
static_assert(IsRenderOwnerRole<pond::render::RenderTarget>());
static_assert(IsRenderOwnerRole<pond::render::RenderFrame>());
static_assert(noexcept(pond::render::GetRequiredWindowGraphicsCompatibility()));
static_assert(std::is_same_v<decltype(pond::render::GetRequiredWindowGraphicsCompatibility()),
                             pond::platform::WindowGraphicsCompatibility>);
static_assert(
    std::is_same_v<decltype(std::declval<const pond::render::RenderDevice&>().GetSelectedAdapter()),
                   const pond::render::RenderAdapterSnapshot&>);
static_assert(noexcept(std::declval<const pond::render::RenderDevice&>().GetSelectedAdapter()));
static_assert(!noexcept(std::declval<const pond::render::RenderDevice&>().GetDiagnostics()));
static_assert(
    noexcept(std::declval<const pond::render::RenderTarget&>().GetSelectedPresentationConfig()));
static_assert(!noexcept(std::declval<const pond::render::RenderTarget&>().GetDiagnostics()));
static_assert(noexcept(std::declval<const pond::render::RenderFrame&>().GetMetrics()));

TEST(RenderApiTraitsTests, DefaultOwnerRolesAreEmptyAndMoveStable)
{
    pond::render::RenderBootstrap bootstrap;
    pond::render::RenderDevice device;
    pond::render::PreparedSurface surface;
    pond::render::RenderTarget target;
    pond::render::RenderFrame frame;

    EXPECT_FALSE(bootstrap.IsValid());
    EXPECT_FALSE(device.IsValid());
    EXPECT_FALSE(surface.IsValid());
    EXPECT_FALSE(target.IsValid());
    EXPECT_FALSE(frame.IsValid());

    pond::render::RenderBootstrap movedBootstrap{std::move(bootstrap)};
    pond::render::RenderDevice movedDevice{std::move(device)};
    pond::render::PreparedSurface movedSurface{std::move(surface)};
    pond::render::RenderTarget movedTarget{std::move(target)};
    pond::render::RenderFrame movedFrame{std::move(frame)};

    EXPECT_FALSE(movedBootstrap.IsValid());
    EXPECT_FALSE(movedDevice.IsValid());
    EXPECT_FALSE(movedSurface.IsValid());
    EXPECT_FALSE(movedTarget.IsValid());
    EXPECT_FALSE(movedFrame.IsValid());
}
} // namespace
