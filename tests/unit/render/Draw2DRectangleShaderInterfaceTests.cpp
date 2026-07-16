#include <ponder/render/draw2d/Draw2DRectangleShaderInterface.hpp>

#include <Draw2DRectangleFragmentShader.hpp>
#include <Draw2DRectangleVertexShader.hpp>
#include <gtest/gtest.h>

namespace
{
namespace draw2d = pond::render::draw2d;
namespace shaders = pond::render::shaders;

static_assert(shaders::kDraw2DRectangleVertexShaderName ==
              draw2d::kDraw2DRectangleVertexShaderName);
static_assert(shaders::kDraw2DRectangleVertexShaderEntryPoint ==
              draw2d::kDraw2DRectangleVertexEntryPoint);
static_assert(shaders::kDraw2DRectangleVertexShaderProfile ==
              draw2d::kDraw2DRectangleVertexProfile);
static_assert(shaders::kDraw2DRectangleVertexShaderStage == draw2d::kDraw2DRectangleVertexStage);
static_assert(shaders::kDraw2DRectangleVertexShaderSchemaFingerprint ==
              draw2d::kDraw2DRectangleShaderSchemaFingerprint);

static_assert(shaders::kDraw2DRectangleFragmentShaderName ==
              draw2d::kDraw2DRectangleFragmentShaderName);
static_assert(shaders::kDraw2DRectangleFragmentShaderEntryPoint ==
              draw2d::kDraw2DRectangleFragmentEntryPoint);
static_assert(shaders::kDraw2DRectangleFragmentShaderProfile ==
              draw2d::kDraw2DRectangleFragmentProfile);
static_assert(shaders::kDraw2DRectangleFragmentShaderStage ==
              draw2d::kDraw2DRectangleFragmentStage);
static_assert(shaders::kDraw2DRectangleFragmentShaderSchemaFingerprint ==
              draw2d::kDraw2DRectangleShaderSchemaFingerprint);

TEST(Draw2DRectangleShaderInterfaceTests, VertexAttributesMatchPacketLayout)
{
    ASSERT_EQ(draw2d::kDraw2DRectangleVertexAttributes.size(), 2U);
    EXPECT_EQ(draw2d::kDraw2DRectangleVertexAttributes[0],
              draw2d::kDraw2DRectanglePositionAttribute);
    EXPECT_EQ(draw2d::kDraw2DRectangleVertexAttributes[1], draw2d::kDraw2DRectangleColorAttribute);
}

TEST(Draw2DRectangleShaderInterfaceTests, GeneratedShaderBlobsAreEmbedded)
{
    EXPECT_GT(shaders::kDraw2DRectangleVertexShaderSpirvWords.size(), 0U);
    EXPECT_GT(shaders::kDraw2DRectangleFragmentShaderSpirvWords.size(), 0U);
    EXPECT_FALSE(shaders::kDraw2DRectangleVertexShaderSpirvSha256.empty());
    EXPECT_FALSE(shaders::kDraw2DRectangleFragmentShaderSpirvSha256.empty());
    EXPECT_FALSE(shaders::kDraw2DRectangleVertexShaderInputSha256.empty());
    EXPECT_FALSE(shaders::kDraw2DRectangleFragmentShaderInputSha256.empty());
}

TEST(Draw2DRectangleShaderInterfaceTests, StaticContractUsesNoDescriptors)
{
    EXPECT_EQ(draw2d::kDraw2DRectangleDescriptorSetCount, 0U);
    EXPECT_EQ(draw2d::kDraw2DRectangleDescriptorBindingCount, 0U);
    EXPECT_FALSE(draw2d::kDraw2DRectangleUsesDescriptors);
}
} // namespace
