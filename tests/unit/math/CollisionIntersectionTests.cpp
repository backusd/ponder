#include <ponder/math/RayBoxHit.hpp>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <random>

namespace
{
struct DoubleRayBoxHit final
{
    double entryDistance{0.0};
    double exitDistance{0.0};
};

[[nodiscard]] pond::math::Ray MakeRay(pond::math::Vector3 origin, pond::math::Vector3 direction)
{
    auto ray = pond::math::Ray::Create(origin, direction);
    EXPECT_TRUE(ray.HasValue());
    return ray.GetValue();
}

[[nodiscard]] pond::math::AxisAlignedBox MakeBox(pond::math::Vector3 minimum,
                                                 pond::math::Vector3 maximum)
{
    auto box = pond::math::AxisAlignedBox::Create(minimum, maximum);
    EXPECT_TRUE(box.HasValue());
    return box.GetValue();
}

void ExpectHitNear(std::optional<pond::math::RayBoxHit> hit, float expectedEntry,
                   float expectedExit, float tolerance = 1.0e-5F)
{
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(hit->GetEntryDistance(), expectedEntry, tolerance);
    EXPECT_NEAR(hit->GetExitDistance(), expectedExit, tolerance);
}

void ExpectPointNear(pond::math::Vector3 actual, pond::math::Vector3 expected,
                     float tolerance = 1.0e-5F)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

[[nodiscard]] bool UpdateDoubleOracleInterval(float origin, float direction, float minimum,
                                              float maximum, double& entry, double& exit)
{
    if (direction == 0.0F)
    {
        return origin >= minimum && origin <= maximum;
    }

    const double directionValue = static_cast<double>(direction);
    double nearDistance =
        (static_cast<double>(minimum) - static_cast<double>(origin)) / directionValue;
    double farDistance =
        (static_cast<double>(maximum) - static_cast<double>(origin)) / directionValue;
    if (farDistance < nearDistance)
    {
        std::swap(nearDistance, farDistance);
    }

    entry = std::max(entry, nearDistance);
    exit = std::min(exit, farDistance);
    return entry <= exit;
}

[[nodiscard]] std::optional<DoubleRayBoxHit> IntersectDoubleOracle(
    const pond::math::Ray& ray, const pond::math::AxisAlignedBox& box)
{
    const pond::math::Vector3 origin = ray.GetOrigin();
    const pond::math::Vector3 direction = ray.GetDirection();
    const pond::math::Vector3 minimum = box.GetMinimum();
    const pond::math::Vector3 maximum = box.GetMaximum();
    double entry = -std::numeric_limits<double>::infinity();
    double exit = std::numeric_limits<double>::infinity();

    if (!UpdateDoubleOracleInterval(origin.x, direction.x, minimum.x, maximum.x, entry, exit) ||
        !UpdateDoubleOracleInterval(origin.y, direction.y, minimum.y, maximum.y, entry, exit) ||
        !UpdateDoubleOracleInterval(origin.z, direction.z, minimum.z, maximum.z, entry, exit) ||
        exit < 0.0)
    {
        return std::nullopt;
    }

    return DoubleRayBoxHit{std::max(0.0, entry), exit};
}

TEST(CollisionIntersectionTests, ReturnsEntryAndExitDistancesForOutsideHits)
{
    const auto ray =
        MakeRay(pond::math::Vector3{-3.0F, 0.0F, 0.0F}, pond::math::Vector3{1.0F, 0.0F, 0.0F});
    const auto box =
        MakeBox(pond::math::Vector3{-1.0F, -1.0F, -1.0F}, pond::math::Vector3{1.0F, 1.0F, 1.0F});

    const auto hit = pond::math::Intersect(ray, box);
    ExpectHitNear(hit, 2.0F, 4.0F);

    ExpectPointNear(ray.GetOrigin() + ray.GetDirection() * hit->GetEntryDistance(),
                    pond::math::Vector3{-1.0F, 0.0F, 0.0F});
    ExpectPointNear(ray.GetOrigin() + ray.GetDirection() * hit->GetExitDistance(),
                    pond::math::Vector3{1.0F, 0.0F, 0.0F});
}

TEST(CollisionIntersectionTests, ReturnsZeroEntryForInsideAndBoundaryOrigins)
{
    const auto box =
        MakeBox(pond::math::Vector3{-1.0F, -1.0F, -1.0F}, pond::math::Vector3{1.0F, 1.0F, 1.0F});

    ExpectHitNear(pond::math::Intersect(MakeRay(pond::math::Vector3{0.0F, 0.0F, 0.0F},
                                                pond::math::Vector3{1.0F, 0.0F, 0.0F}),
                                        box),
                  0.0F, 1.0F);
    ExpectHitNear(pond::math::Intersect(MakeRay(pond::math::Vector3{-1.0F, 0.0F, 0.0F},
                                                pond::math::Vector3{1.0F, 0.0F, 0.0F}),
                                        box),
                  0.0F, 2.0F);
    ExpectHitNear(pond::math::Intersect(MakeRay(pond::math::Vector3{-1.0F, 0.0F, 0.0F},
                                                pond::math::Vector3{-1.0F, 0.0F, 0.0F}),
                                        box),
                  0.0F, 0.0F);
}

TEST(CollisionIntersectionTests, RejectsMissesIncludingBehindOriginAndParallelOutsideSlabs)
{
    const auto box =
        MakeBox(pond::math::Vector3{-1.0F, -1.0F, -1.0F}, pond::math::Vector3{1.0F, 1.0F, 1.0F});

    EXPECT_FALSE(pond::math::Intersect(
        MakeRay(pond::math::Vector3{3.0F, 0.0F, 0.0F}, pond::math::Vector3{1.0F, 0.0F, 0.0F}),
        box));
    EXPECT_FALSE(pond::math::Intersect(
        MakeRay(pond::math::Vector3{0.0F, 2.0F, 0.0F}, pond::math::Vector3{1.0F, 0.0F, 0.0F}),
        box));
    EXPECT_FALSE(pond::math::Intersect(
        MakeRay(pond::math::Vector3{-3.0F, 3.0F, 0.0F}, pond::math::Vector3{1.0F, 0.0F, 0.0F}),
        box));
}

TEST(CollisionIntersectionTests, PreservesZeroWidthIntervalsForTangencyAndPointBoxes)
{
    const auto box =
        MakeBox(pond::math::Vector3{0.0F, 0.0F, 0.0F}, pond::math::Vector3{1.0F, 1.0F, 1.0F});
    const auto tangentRay =
        MakeRay(pond::math::Vector3{-1.0F, 1.0F, 0.5F}, pond::math::Vector3{1.0F, -1.0F, 0.0F});
    const float tangentDistance = std::sqrt(2.0F);

    ExpectHitNear(pond::math::Intersect(tangentRay, box), tangentDistance, tangentDistance);
    ExpectHitNear(
        pond::math::Intersect(
            MakeRay(pond::math::Vector3{0.0F, 0.0F, 0.0F}, pond::math::Vector3{1.0F, 0.0F, 0.0F}),
            MakeBox(pond::math::Vector3{1.0F, 0.0F, 0.0F}, pond::math::Vector3{1.0F, 0.0F, 0.0F})),
        1.0F, 1.0F);
}

TEST(CollisionIntersectionTests, HandlesFlatBoxesAndParallelAxes)
{
    const auto flatBox =
        MakeBox(pond::math::Vector3{-1.0F, -1.0F, 0.0F}, pond::math::Vector3{1.0F, 1.0F, 0.0F});

    ExpectHitNear(pond::math::Intersect(MakeRay(pond::math::Vector3{0.0F, -2.0F, 0.0F},
                                                pond::math::Vector3{0.0F, 1.0F, 0.0F}),
                                        flatBox),
                  1.0F, 3.0F);
    EXPECT_FALSE(pond::math::Intersect(
        MakeRay(pond::math::Vector3{0.0F, -2.0F, 1.0F}, pond::math::Vector3{0.0F, 1.0F, 0.0F}),
        flatBox));
}

TEST(CollisionIntersectionTests, HandlesVerySmallAndVeryLargeFiniteCases)
{
    ExpectHitNear(pond::math::Intersect(MakeRay(pond::math::Vector3{0.0F, 0.0F, 0.0F},
                                                pond::math::Vector3{1.0F, 0.0F, 0.0F}),
                                        MakeBox(pond::math::Vector3{1.0e-6F, -1.0e-6F, -1.0e-6F},
                                                pond::math::Vector3{2.0e-6F, 1.0e-6F, 1.0e-6F})),
                  1.0e-6F, 2.0e-6F, 1.0e-10F);

    ExpectHitNear(pond::math::Intersect(MakeRay(pond::math::Vector3{0.0F, 0.0F, 0.0F},
                                                pond::math::Vector3{1.0F, 0.0F, 0.0F}),
                                        MakeBox(pond::math::Vector3{1.0e16F, -1.0F, -1.0F},
                                                pond::math::Vector3{1.0001e16F, 1.0F, 1.0F})),
                  1.0e16F, 1.0001e16F, 1.0e11F);
}

TEST(CollisionIntersectionTests, SeededCasesAgreeWithDoublePrecisionSlabOracle)
{
    std::mt19937 generator{0x52415842U};
    std::uniform_real_distribution<float> centerDistribution{-20.0F, 20.0F};
    std::uniform_real_distribution<float> extentDistribution{0.0F, 5.0F};
    std::uniform_real_distribution<float> originDistribution{-30.0F, 30.0F};
    std::uniform_real_distribution<float> directionDistribution{-1.0F, 1.0F};

    for (int index = 0; index < 256; ++index)
    {
        const pond::math::Vector3 center{centerDistribution(generator),
                                         centerDistribution(generator),
                                         centerDistribution(generator)};
        const pond::math::Vector3 extent{extentDistribution(generator),
                                         extentDistribution(generator),
                                         extentDistribution(generator)};
        const auto box = MakeBox(center - extent, center + extent);

        pond::math::Vector3 direction{directionDistribution(generator),
                                      directionDistribution(generator),
                                      directionDistribution(generator)};
        if (pond::math::SquaredLength(direction) == 0.0F)
        {
            direction = pond::math::Vector3{1.0F, 0.0F, 0.0F};
        }

        const auto ray = MakeRay(pond::math::Vector3{originDistribution(generator),
                                                     originDistribution(generator),
                                                     originDistribution(generator)},
                                 direction);
        const auto actual = pond::math::Intersect(ray, box);
        const auto expected = IntersectDoubleOracle(ray, box);

        EXPECT_EQ(actual.has_value(), expected.has_value());
        if (actual.has_value() && expected.has_value())
        {
            const double entryMagnitude = std::max(1.0, std::abs(expected->entryDistance));
            const double exitMagnitude = std::max(1.0, std::abs(expected->exitDistance));
            EXPECT_NEAR(actual->GetEntryDistance(), static_cast<float>(expected->entryDistance),
                        static_cast<float>(1.0e-5 * entryMagnitude));
            EXPECT_NEAR(actual->GetExitDistance(), static_cast<float>(expected->exitDistance),
                        static_cast<float>(1.0e-5 * exitMagnitude));
        }
    }
}
} // namespace
