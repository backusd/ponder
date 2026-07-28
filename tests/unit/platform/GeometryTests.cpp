#include <ponder/platform/Geometry.hpp>

#include <concepts>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <limits>
#include <sstream>
#include <type_traits>

namespace
{
constexpr bool GeometryValueSemanticsAreConstexpr()
{
    constexpr ponder::platform::ScreenPosition kPosition{-1920, 40};
    constexpr ponder::platform::ScreenExtent kExtent{1920, 1080};
    constexpr ponder::platform::ScreenRectangle kScreenRectangle{kPosition, kExtent};
    constexpr ponder::platform::LogicalPoint kOrigin{-3.5F, 7.25F};
    constexpr ponder::platform::LogicalExtent kLogicalExtent{120.0F, 45.0F};
    constexpr ponder::platform::LogicalRectangle kLogicalRectangle{kOrigin, kLogicalExtent};
    constexpr ponder::platform::LogicalSize kLogicalSize{1280, 800};
    constexpr ponder::platform::PixelSize kPixelSize{2560, 1600};

    return kScreenRectangle.position.x == -1920 && kScreenRectangle.extent.height == 1080 && kLogicalRectangle.origin == kOrigin &&
           kLogicalRectangle.extent == kLogicalExtent && kLogicalSize.width == 1280 && kPixelSize.height == 1600;
}

static_assert(GeometryValueSemanticsAreConstexpr());
static_assert(ponder::platform::ScreenPosition{} == ponder::platform::ScreenPosition{});
static_assert(ponder::platform::ScreenExtent{} == ponder::platform::ScreenExtent{});
static_assert(ponder::platform::LogicalPoint{} == ponder::platform::LogicalPoint{});
static_assert(ponder::platform::LogicalExtent{} == ponder::platform::LogicalExtent{});
static_assert(ponder::platform::LogicalSize{} == ponder::platform::LogicalSize{});
static_assert(ponder::platform::PixelSize{} == ponder::platform::PixelSize{});
static_assert(!std::same_as<ponder::platform::LogicalSize, ponder::platform::PixelSize>);
static_assert(ponder::platform::IsValid(ponder::platform::LogicalPoint{-1.0F, 2.0F}));
static_assert(ponder::platform::IsValid(ponder::platform::LogicalExtent{0.0F, 0.0F}));
static_assert(!ponder::platform::IsValid(ponder::platform::LogicalExtent{-1.0F, 2.0F}));
static_assert(!ponder::platform::IsValid(ponder::platform::LogicalPoint{std::numeric_limits<float>::infinity(), 0.0F}));
static_assert(std::is_trivially_copyable_v<ponder::platform::ScreenRectangle>);
static_assert(std::is_trivially_copyable_v<ponder::platform::LogicalRectangle>);

TEST(PlatformGeometryTests, DefaultsEveryValueToZero)
{
    constexpr ponder::platform::ScreenRectangle kScreenRectangle;
    constexpr ponder::platform::LogicalRectangle kLogicalRectangle;
    constexpr ponder::platform::LogicalSize kLogicalSize;
    constexpr ponder::platform::PixelSize kPixelSize;

    EXPECT_EQ(kScreenRectangle.position.x, 0);
    EXPECT_EQ(kScreenRectangle.position.y, 0);
    EXPECT_EQ(kScreenRectangle.extent.width, 0U);
    EXPECT_EQ(kScreenRectangle.extent.height, 0U);
    EXPECT_FLOAT_EQ(kLogicalRectangle.origin.x, 0.0F);
    EXPECT_FLOAT_EQ(kLogicalRectangle.origin.y, 0.0F);
    EXPECT_FLOAT_EQ(kLogicalRectangle.extent.width, 0.0F);
    EXPECT_FLOAT_EQ(kLogicalRectangle.extent.height, 0.0F);
    EXPECT_EQ(kLogicalSize.width, 0U);
    EXPECT_EQ(kLogicalSize.height, 0U);
    EXPECT_EQ(kPixelSize.width, 0U);
    EXPECT_EQ(kPixelSize.height, 0U);
}

TEST(PlatformGeometryTests, PreservesSignedScreenCoordinates)
{
    constexpr ponder::platform::ScreenPosition kPosition{-2560, -1440};

    EXPECT_EQ(kPosition.x, -2560);
    EXPECT_EQ(kPosition.y, -1440);
}

TEST(PlatformGeometryTests, ValidatesFloatingPointGeometry)
{
    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();

    EXPECT_TRUE(ponder::platform::IsValid(ponder::platform::LogicalPoint{-2.5F, 3.5F}));
    EXPECT_FALSE(ponder::platform::IsValid(ponder::platform::LogicalPoint{kInfinity, 0.0F}));
    EXPECT_FALSE(ponder::platform::IsValid(ponder::platform::LogicalPoint{nan, 0.0F}));

    EXPECT_TRUE(ponder::platform::IsValid(ponder::platform::LogicalExtent{0.0F, 0.0F}));
    EXPECT_TRUE(ponder::platform::IsValid(ponder::platform::LogicalExtent{1.5F, 2.5F}));
    EXPECT_FALSE(ponder::platform::IsValid(ponder::platform::LogicalExtent{-1.0F, 2.0F}));
    EXPECT_FALSE(ponder::platform::IsValid(ponder::platform::LogicalExtent{1.0F, kInfinity}));
    EXPECT_FALSE(ponder::platform::IsValid(ponder::platform::LogicalExtent{1.0F, nan}));

    EXPECT_TRUE(ponder::platform::IsValid(
        ponder::platform::LogicalRectangle{ponder::platform::LogicalPoint{-1.0F, -2.0F}, ponder::platform::LogicalExtent{3.0F, 4.0F}}));
    EXPECT_FALSE(ponder::platform::IsValid(
        ponder::platform::LogicalRectangle{ponder::platform::LogicalPoint{nan, 0.0F}, ponder::platform::LogicalExtent{3.0F, 4.0F}}));
}

TEST(PlatformGeometryTests, KeepsLogicalAndPixelSizesDistinct)
{
    constexpr ponder::platform::LogicalSize kLogicalSize{1280, 800};
    constexpr ponder::platform::PixelSize kPixelSize{2560, 1600};

    EXPECT_EQ(kLogicalSize.width, 1280U);
    EXPECT_EQ(kLogicalSize.height, 800U);
    EXPECT_EQ(kPixelSize.width, 2560U);
    EXPECT_EQ(kPixelSize.height, 1600U);
}

TEST(PlatformGeometryTests, FormatsAndStreamsGeometryValues)
{
    const ponder::platform::ScreenPosition position{-12, 34};
    const ponder::platform::ScreenExtent extent{800, 600};
    const ponder::platform::ScreenRectangle rectangle{position, extent};
    const ponder::platform::LogicalPoint point{1.5F, -2.25F};
    const ponder::platform::LogicalExtent logicalExtent{3.5F, 4.25F};
    const ponder::platform::LogicalRectangle logicalRectangle{point, logicalExtent};
    const ponder::platform::LogicalSize logicalSize{320, 200};
    const ponder::platform::PixelSize pixelSize{640, 400};
    std::ostringstream stream;

    stream << rectangle;

    EXPECT_EQ(std::format("{}", position), "(-12, 34)");
    EXPECT_EQ(std::format("{}", extent), "800x600");
    EXPECT_EQ(std::format("{}", rectangle), "(-12, 34) / 800x600");
    EXPECT_EQ(stream.str(), "(-12, 34) / 800x600");
    EXPECT_EQ(std::format("{}", point), "(1.5, -2.25)");
    EXPECT_EQ(std::format("{}", logicalExtent), "3.5x4.25");
    EXPECT_EQ(std::format("{}", logicalRectangle), "(1.5, -2.25) / 3.5x4.25");
    EXPECT_EQ(std::format("{}", logicalSize), "320x200");
    EXPECT_EQ(std::format("{}", pixelSize), "640x400");
}
} // namespace
