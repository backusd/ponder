#pragma once

#include <ponder/render/Bootstrap.hpp>

#include <cstdint>

namespace pond::render::detail
{
/// Backend-neutral device-scoped Draw2D counters for live integration tests.
///
/// This private source-tree seam deliberately exposes no native handles or Vulkan types. It is not
/// installed and is not part of the render API.
struct Draw2DDeviceLiveStats final
{
    std::uint64_t pipelineCreationCount{};
    std::uint64_t pipelineReuseCount{};
    std::uint64_t pipelineReplacementCount{};
    std::uint32_t activeLayerCount{};
    bool hasPipeline{};

    [[nodiscard]] friend constexpr bool operator==(
        const Draw2DDeviceLiveStats& lhs, const Draw2DDeviceLiveStats& rhs) noexcept = default;
};

/// Backend-neutral target-scoped Draw2D upload counters for live integration tests.
struct Draw2DTargetLiveStats final
{
    std::uint64_t currentCapacityBytes{};
    std::uint64_t reservedBytes{};
    std::uint64_t uploadedBytes{};
    std::uint64_t highWaterReservedBytes{};
    std::uint64_t allocationCount{};
    std::uint64_t growthCount{};
    std::uint64_t generationCount{};
    std::uint64_t flushCount{};
    std::uint64_t retirementCount{};
    std::uint32_t slotCount{};
    std::uint32_t idleSlotCount{};
    std::uint32_t reservedSlotCount{};
    std::uint32_t submittedSlotCount{};
    bool hasUploadArena{};

    [[nodiscard]] friend constexpr bool operator==(
        const Draw2DTargetLiveStats& lhs, const Draw2DTargetLiveStats& rhs) noexcept = default;
};

class RenderLiveTestAccess final
{
public:
    [[nodiscard]] static Draw2DDeviceLiveStats GetDraw2DDeviceStats(
        const RenderDevice& device) noexcept;
    [[nodiscard]] static Draw2DTargetLiveStats GetDraw2DTargetStats(
        const RenderTarget& target) noexcept;
};
} // namespace pond::render::detail
