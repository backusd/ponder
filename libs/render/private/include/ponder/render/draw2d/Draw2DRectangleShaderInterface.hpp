#pragma once

#include <ponder/render/draw2d/Draw2DPacket.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace pond::render::draw2d
{
inline constexpr std::string_view kDraw2DRectangleShaderTargetEnvironment{"vulkan1.2"};
inline constexpr std::string_view kDraw2DRectangleShaderSchemaFingerprint{"0x05c436a9fda7b4f7"};

inline constexpr std::string_view kDraw2DRectangleVertexShaderName{"Draw2DRectangleVertex"};
inline constexpr std::string_view kDraw2DRectangleVertexEntryPoint{"MainVs"};
inline constexpr std::string_view kDraw2DRectangleVertexProfile{"vs_6_0"};
inline constexpr std::string_view kDraw2DRectangleVertexStage{"vertex"};

inline constexpr std::string_view kDraw2DRectangleFragmentShaderName{"Draw2DRectangleFragment"};
inline constexpr std::string_view kDraw2DRectangleFragmentEntryPoint{"MainPs"};
inline constexpr std::string_view kDraw2DRectangleFragmentProfile{"ps_6_0"};
inline constexpr std::string_view kDraw2DRectangleFragmentStage{"fragment"};

enum class Draw2DRectangleVertexAttributeFormat : std::uint8_t
{
    Float32x2,
    Unorm8x4
};

struct Draw2DRectangleVertexAttribute final
{
    std::uint32_t binding{};
    std::uint32_t location{};
    std::uint32_t offset{};
    Draw2DRectangleVertexAttributeFormat format{};

    [[nodiscard]] friend constexpr bool operator==(
        const Draw2DRectangleVertexAttribute& lhs,
        const Draw2DRectangleVertexAttribute& rhs) noexcept = default;
};

inline constexpr std::uint32_t kDraw2DRectangleVertexBinding{0U};
inline constexpr std::uint32_t kDraw2DRectangleVertexStride{sizeof(Draw2DVertex)};
inline constexpr Draw2DRectangleVertexAttribute kDraw2DRectanglePositionAttribute{
    .binding = kDraw2DRectangleVertexBinding,
    .location = 0U,
    .offset = offsetof(Draw2DVertex, x),
    .format = Draw2DRectangleVertexAttributeFormat::Float32x2};
inline constexpr Draw2DRectangleVertexAttribute kDraw2DRectangleColorAttribute{
    .binding = kDraw2DRectangleVertexBinding,
    .location = 1U,
    .offset = offsetof(Draw2DVertex, color),
    .format = Draw2DRectangleVertexAttributeFormat::Unorm8x4};
inline constexpr std::array<Draw2DRectangleVertexAttribute, 2U> kDraw2DRectangleVertexAttributes{
    kDraw2DRectanglePositionAttribute,
    kDraw2DRectangleColorAttribute,
};

struct Draw2DRectangleConstants final
{
    std::uint32_t pixelExtentWidth{};
    std::uint32_t pixelExtentHeight{};

    [[nodiscard]] friend constexpr bool operator==(const Draw2DRectangleConstants& lhs,
                                                   const Draw2DRectangleConstants& rhs) noexcept =
        default;
};

enum class Draw2DRectangleShaderStage : std::uint8_t
{
    Vertex,
    Fragment
};

inline constexpr Draw2DRectangleShaderStage kDraw2DRectangleConstantsStage{
    Draw2DRectangleShaderStage::Vertex};
inline constexpr std::uint32_t kDraw2DRectangleConstantsOffset{0U};
inline constexpr std::uint32_t kDraw2DRectangleConstantsSize{sizeof(Draw2DRectangleConstants)};
inline constexpr std::uint32_t kDraw2DRectangleColorOutputLocation{0U};
inline constexpr std::uint32_t kDraw2DRectangleDescriptorSetCount{0U};
inline constexpr std::uint32_t kDraw2DRectangleDescriptorBindingCount{0U};
inline constexpr bool kDraw2DRectangleUsesDescriptors{false};

static_assert(kDraw2DRectangleShaderSchemaFingerprint == "0x05c436a9fda7b4f7");
static_assert(kDraw2DSchemaFingerprint == 0x05c436a9fda7b4f7ULL);
static_assert(std::is_standard_layout_v<Draw2DRectangleConstants>);
static_assert(std::is_trivially_copyable_v<Draw2DRectangleConstants>);
static_assert(sizeof(Draw2DRectangleConstants) == sizeof(Draw2DPixelExtent));
static_assert(alignof(Draw2DRectangleConstants) == alignof(std::uint32_t));
static_assert(offsetof(Draw2DRectangleConstants, pixelExtentWidth) == 0U);
static_assert(offsetof(Draw2DRectangleConstants, pixelExtentHeight) == 4U);
static_assert(kDraw2DRectangleVertexStride == 12U);
static_assert(kDraw2DRectanglePositionAttribute.location == 0U);
static_assert(kDraw2DRectanglePositionAttribute.offset == 0U);
static_assert(kDraw2DRectanglePositionAttribute.format ==
              Draw2DRectangleVertexAttributeFormat::Float32x2);
static_assert(kDraw2DRectangleColorAttribute.location == 1U);
static_assert(kDraw2DRectangleColorAttribute.offset == 8U);
static_assert(kDraw2DRectangleColorAttribute.format ==
              Draw2DRectangleVertexAttributeFormat::Unorm8x4);
static_assert(kDraw2DRectangleConstantsOffset == 0U);
static_assert(kDraw2DRectangleConstantsSize == 8U);
static_assert(kDraw2DRectangleConstantsStage == Draw2DRectangleShaderStage::Vertex);
static_assert(kDraw2DRectangleColorOutputLocation == 0U);
static_assert(kDraw2DRectangleDescriptorSetCount == 0U);
static_assert(kDraw2DRectangleDescriptorBindingCount == 0U);
static_assert(!kDraw2DRectangleUsesDescriptors);
} // namespace pond::render::draw2d
