#include <ponder/core/Result.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/WindowGraphics.hpp>
#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/RenderError.hpp>

namespace
{
static_assert(pond::render::IsValid(pond::render::RenderBootstrapDesc{}));
static_assert(pond::render::IsValid(pond::render::RenderDeviceDesc{}));
[[maybe_unused]] const bool kDefaultAdapterSelectionDescIsValid =
    pond::render::IsValid(pond::render::RenderAdapterSelectionDesc{});
static_assert(pond::render::IsValid(pond::render::QueuedFrameLatency{}));
static_assert(pond::render::IsValid(pond::render::ClearColor{
    .red = 0.25F, .green = 0.5F, .blue = 0.75F, .alpha = 1.0F}));

[[maybe_unused]] constexpr pond::render::RenderTargetSnapshot kSnapshot{
    pond::platform::WindowId{7U},
    pond::platform::PixelSize{320U, 240U},
    pond::platform::LogicalSize{320U, 240U},
    true,
    pond::platform::WindowState::Normal,
    pond::render::PresentationEnvironmentRevision{1U},
    1U};
static_assert(pond::render::IsValid(kSnapshot));

[[maybe_unused]] constexpr pond::render::RenderFrameMetrics kFrameMetrics{
    .windowId = pond::platform::WindowId{7U},
    .logicalSize = pond::platform::LogicalSize{320U, 240U},
    .pixelSize = pond::platform::PixelSize{640U, 480U},
    .metricsRevision = pond::render::PresentationEnvironmentRevision{2U},
    .targetRevision = 3U};
static_assert(pond::render::IsValid(kFrameMetrics));
[[maybe_unused]] constexpr pond::render::SurfacePreparationDesc kSurfaceDesc{
    .targetSnapshot = kSnapshot, .reason = pond::render::SurfacePreparationReason::Initial};
static_assert(pond::render::IsValid(kSurfaceDesc));

[[maybe_unused]] constexpr pond::render::RenderTargetDesc kTargetDesc{
    .targetSnapshot = kSnapshot,
    .presentation = {.policy = pond::render::PresentationPolicy::VSync,
                     .strength = pond::render::RequirementStrength::Preferred},
    .queuedLatency = {},
    .clearColor = {}};
static_assert(pond::render::IsValid(kTargetDesc));

[[maybe_unused]] const pond::core::ErrorCode kRenderErrorCode =
    pond::render::ToErrorCode(pond::render::RenderErrorCode::UnsupportedBackend);
[[maybe_unused]] constexpr pond::platform::WindowGraphicsCompatibility kRequiredCompatibility =
    pond::platform::WindowGraphicsCompatibility::Vulkan;
} // namespace
