#include <ponder/platform/Geometry.hpp>

#include <gtest/gtest.h>

#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace
{
constexpr bool GeometryValueSemanticsAreConstexpr()
{
    constexpr pond::platform::ScreenPosition kPosition{-1920, 40};
    constexpr pond::platform::ScreenExtent kExtent{1920, 1080};
    constexpr pond::platform::ScreenRectangle kScreenRectangle{kPosition, kExtent};
    constexpr pond::platform::LogicalPoint kOrigin{-3.5F, 7.25F};
    constexpr pond::platform::LogicalExtent kLogicalExtent{120.0F, 45.0F};
    constexpr pond::platform::LogicalRectangle kLogicalRectangle{kOrigin, kLogicalExtent};
    constexpr pond::platform::LogicalSize kLogicalSize{1280, 800};
    constexpr pond::platform::PixelSize kPixelSize{2560, 1600};

    return kScreenRectangle.position.x == -1920 && kScreenRectangle.extent.height == 1080 &&
           kLogicalRectangle.origin == kOrigin &&
           kLogicalRectangle.extent == kLogicalExtent && kLogicalSize.width == 1280 &&
           kPixelSize.height == 1600;
}

static_assert(GeometryValueSemanticsAreConstexpr());
static_assert(pond::platform::ScreenPosition{} == pond::platform::ScreenPosition{});
static_assert(pond::platform::ScreenExtent{} == pond::platform::ScreenExtent{});
static_assert(pond::platform::LogicalPoint{} == pond::platform::LogicalPoint{});
static_assert(pond::platform::LogicalExtent{} == pond::platform::LogicalExtent{});
static_assert(pond::platform::LogicalSize{} == pond::platform::LogicalSize{});
static_assert(pond::platform::PixelSize{} == pond::platform::PixelSize{});
static_assert(!std::same_as<pond::platform::LogicalSize, pond::platform::PixelSize>);
static_assert(pond::platform::IsValid(pond::platform::LogicalPoint{-1.0F, 2.0F}));
static_assert(pond::platform::IsValid(pond::platform::LogicalExtent{0.0F, 0.0F}));
static_assert(!pond::platform::IsValid(pond::platform::LogicalExtent{-1.0F, 2.0F}));
static_assert(!pond::platform::IsValid(pond::platform::LogicalPoint{
    std::numeric_limits<float>::infinity(), 0.0F}));
static_assert(std::is_trivially_copyable_v<pond::platform::ScreenRectangle>);
static_assert(std::is_trivially_copyable_v<pond::platform::LogicalRectangle>);

TEST(PlatformGeometryTests, DefaultsEveryValueToZero)
{
    constexpr pond::platform::ScreenRectangle kScreenRectangle;
    constexpr pond::platform::LogicalRectangle kLogicalRectangle;
    constexpr pond::platform::LogicalSize kLogicalSize;
    constexpr pond::platform::PixelSize kPixelSize;

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
    constexpr pond::platform::ScreenPosition kPosition{-2560, -1440};

    EXPECT_EQ(kPosition.x, -2560);
    EXPECT_EQ(kPosition.y, -1440);
}

TEST(PlatformGeometryTests, ValidatesFloatingPointGeometry)
{
    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();

    EXPECT_TRUE(pond::platform::IsValid(pond::platform::LogicalPoint{-2.5F, 3.5F}));
    EXPECT_FALSE(pond::platform::IsValid(pond::platform::LogicalPoint{kInfinity, 0.0F}));
    EXPECT_FALSE(pond::platform::IsValid(pond::platform::LogicalPoint{nan, 0.0F}));

    EXPECT_TRUE(pond::platform::IsValid(pond::platform::LogicalExtent{0.0F, 0.0F}));
    EXPECT_TRUE(pond::platform::IsValid(pond::platform::LogicalExtent{1.5F, 2.5F}));
    EXPECT_FALSE(pond::platform::IsValid(pond::platform::LogicalExtent{-1.0F, 2.0F}));
    EXPECT_FALSE(pond::platform::IsValid(pond::platform::LogicalExtent{1.0F, kInfinity}));
    EXPECT_FALSE(pond::platform::IsValid(pond::platform::LogicalExtent{1.0F, nan}));

    EXPECT_TRUE(pond::platform::IsValid(pond::platform::LogicalRectangle{
        pond::platform::LogicalPoint{-1.0F, -2.0F},
        pond::platform::LogicalExtent{3.0F, 4.0F}}));
    EXPECT_FALSE(pond::platform::IsValid(pond::platform::LogicalRectangle{
        pond::platform::LogicalPoint{nan, 0.0F},
        pond::platform::LogicalExtent{3.0F, 4.0F}}));
}

TEST(PlatformGeometryTests, KeepsLogicalAndPixelSizesDistinct)
{
    constexpr pond::platform::LogicalSize kLogicalSize{1280, 800};
    constexpr pond::platform::PixelSize kPixelSize{2560, 1600};

    EXPECT_EQ(kLogicalSize.width, 1280U);
    EXPECT_EQ(kLogicalSize.height, 800U);
    EXPECT_EQ(kPixelSize.width, 2560U);
    EXPECT_EQ(kPixelSize.height, 1600U);
}
} // namespace
