#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/ui/Color.hpp>
#include <ponder/ui/Geometry.hpp>
#include <ponder/ui/Metrics.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace pond::render
{
class RenderDevice;
class RenderFrame;
} // namespace pond::render

namespace pond::ui::experimental
{
inline constexpr std::string_view kRectangleFacadeExperimentalNotice{
    "pond::ui::experimental::RectangleRenderer is a temporary milestone API with no source or "
    "ABI stability guarantee."};

enum class RectangleRecordOutcome : std::uint8_t
{
    Recorded,
    SkippedSuspended,
    SkippedEmpty
};

struct RectanglePaint final
{
    LogicalRect rectangle{};
    SrgbStraightAlphaColor color{};

    [[nodiscard]] friend constexpr bool operator==(const RectanglePaint& lhs,
                                                   const RectanglePaint& rhs) noexcept = default;
};

/// Copies the immutable metrics rendezvous from a live frame token.
///
/// Call this on the frame's render thread while the frame is active. The returned value owns no
/// frame or target reference and remains a plain UI value after the call.
[[nodiscard]] core::Result<UiTargetMetrics> MakeUiTargetMetricsForFrame(
    const render::RenderFrame& frame);

/// Temporary render-thread-affine owner for the rectangle milestone path.
///
/// Create binds an instance to the supplied device and its render thread. Treat every operation on
/// a live instance, including moves and destruction, as confined to that thread with no concurrent
/// access. In particular, move assignment must run there when it releases an already-live
/// destination.
class RectangleRenderer final
{
public:
    RectangleRenderer() noexcept;
    RectangleRenderer(const RectangleRenderer&) = delete;
    RectangleRenderer& operator=(const RectangleRenderer&) = delete;
    RectangleRenderer(RectangleRenderer&& other) noexcept;
    RectangleRenderer& operator=(RectangleRenderer&& other) noexcept;
    ~RectangleRenderer();

    /// Creates an owner bound to `device`; call on the device's render thread.
    [[nodiscard]] static core::Result<RectangleRenderer> Create(render::RenderDevice& device);

    [[nodiscard]] bool IsValid() const noexcept;

    /// Records the rectangle into an active frame from the bound device.
    ///
    /// Call after `RenderFrame::Clear()` and before `RenderFrame::FinishAndPresent()`, on the bound
    /// render thread, with metrics copied from that same frame. Validation and recoverable
    /// preflight failures do not consume the frame's 2D stage, so the caller may retry or finish it
    /// clear-only.
    /// Device loss remains terminal according to the render-device contract.
    [[nodiscard]] core::Result<RectangleRecordOutcome> Record(render::RenderFrame& frame,
                                                              const UiTargetMetrics& metrics,
                                                              RectanglePaint rectangle);

    /// Records an ordered rectangle batch into one active-frame 2D stage.
    ///
    /// Paints retain their input order, so later rectangles are blended over earlier rectangles.
    /// Paint values are validated before recorder state is changed. Recording and hard-limit
    /// failures remain recoverable without consuming the frame's 2D stage, and a successful batch
    /// is submitted as one packet.
    [[nodiscard]] core::Result<RectangleRecordOutcome> Record(
        render::RenderFrame& frame, const UiTargetMetrics& metrics,
        std::span<const RectanglePaint> rectangles);

private:
    struct State;

    explicit RectangleRenderer(std::unique_ptr<State> state) noexcept;

    std::unique_ptr<State> m_state;
};
} // namespace pond::ui::experimental
