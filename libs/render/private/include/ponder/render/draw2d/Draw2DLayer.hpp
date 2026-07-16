#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/draw2d/Draw2DPacket.hpp>

#include <memory>
#include <thread>

namespace pond::render::draw2d
{
class Draw2DLayer final
{
public:
    Draw2DLayer() noexcept;
    Draw2DLayer(const Draw2DLayer&) = delete;
    Draw2DLayer& operator=(const Draw2DLayer&) = delete;
    Draw2DLayer(Draw2DLayer&& other) noexcept;
    Draw2DLayer& operator=(Draw2DLayer&& other) noexcept;
    ~Draw2DLayer();

    [[nodiscard]] static core::Result<Draw2DLayer> Create(RenderDevice& device);

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] core::VoidResult VerifyRenderThread() const;
    [[nodiscard]] core::VoidResult ValidateFrameAccess(RenderFrame& frame) const;
    [[nodiscard]] core::VoidResult Record(RenderFrame& frame, const Draw2DPacket& packet);

private:
    explicit Draw2DLayer(std::shared_ptr<RenderDevice::State> state) noexcept;

    void Release() noexcept;

    std::shared_ptr<RenderDevice::State> m_state;
    std::thread::id m_renderThread{};
};

namespace detail
{
[[nodiscard]] bool IsDraw2DLayerTopologyLinked() noexcept;
} // namespace detail
} // namespace pond::render::draw2d