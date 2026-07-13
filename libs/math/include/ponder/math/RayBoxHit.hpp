#pragma once

#include <ponder/math/AxisAlignedBox.hpp>
#include <ponder/math/Ray.hpp>

#include <limits>
#include <optional>

namespace pond::math
{
class RayBoxHit;

[[nodiscard]] inline constexpr std::optional<RayBoxHit> Intersect(
    const Ray& ray, const AxisAlignedBox& box) noexcept;

namespace detail
{
[[nodiscard]] constexpr bool UpdateRayBoxIntervalForSlab(float origin, float direction,
                                                         float minimum, float maximum,
                                                         double& entry, double& exit) noexcept
{
    if (direction == 0.0F)
    {
        return origin >= minimum && origin <= maximum;
    }

    double slabEntry = (static_cast<double>(minimum) - static_cast<double>(origin)) /
                       static_cast<double>(direction);
    double slabExit = (static_cast<double>(maximum) - static_cast<double>(origin)) /
                      static_cast<double>(direction);
    if (slabExit < slabEntry)
    {
        const double temporary = slabEntry;
        slabEntry = slabExit;
        slabExit = temporary;
    }

    if (entry < slabEntry)
    {
        entry = slabEntry;
    }
    if (slabExit < exit)
    {
        exit = slabExit;
    }

    return entry <= exit;
}

struct RayBoxInterval final
{
    double entryDistance{0.0};
    double exitDistance{0.0};
};

[[nodiscard]] constexpr std::optional<RayBoxInterval> IntersectRayBoxValues(
    Vector3 origin, Vector3 direction, Vector3 minimum, Vector3 maximum) noexcept
{
    double entry = -std::numeric_limits<double>::infinity();
    double exit = std::numeric_limits<double>::infinity();

    if (!UpdateRayBoxIntervalForSlab(origin.x, direction.x, minimum.x, maximum.x, entry, exit) ||
        !UpdateRayBoxIntervalForSlab(origin.y, direction.y, minimum.y, maximum.y, entry, exit) ||
        !UpdateRayBoxIntervalForSlab(origin.z, direction.z, minimum.z, maximum.z, entry, exit) ||
        exit < 0.0)
    {
        return std::nullopt;
    }

    if (entry < 0.0)
    {
        entry = 0.0;
    }

    return RayBoxInterval{entry, exit};
}
} // namespace detail

class RayBoxHit final
{
public:
    [[nodiscard]] constexpr float GetEntryDistance() const noexcept
    {
        return m_entryDistance;
    }

    [[nodiscard]] constexpr float GetExitDistance() const noexcept
    {
        return m_exitDistance;
    }

    [[nodiscard]] friend constexpr bool operator==(const RayBoxHit& lhs,
                                                   const RayBoxHit& rhs) noexcept = default;

private:
    friend constexpr std::optional<RayBoxHit> Intersect(const Ray& ray,
                                                        const AxisAlignedBox& box) noexcept;

    constexpr RayBoxHit(float entryDistance, float exitDistance) noexcept
        : m_entryDistance(entryDistance), m_exitDistance(exitDistance)
    {
    }

    float m_entryDistance;
    float m_exitDistance;
};

[[nodiscard]] inline constexpr std::optional<RayBoxHit> Intersect(
    const Ray& ray, const AxisAlignedBox& box) noexcept
{
    const Vector3 origin = ray.GetOrigin();
    const Vector3 direction = ray.GetDirection();
    const Vector3 minimum = box.GetMinimum();
    const Vector3 maximum = box.GetMaximum();
    const auto interval = detail::IntersectRayBoxValues(origin, direction, minimum, maximum);
    if (!interval.has_value())
    {
        return std::nullopt;
    }

    const float entryDistance = static_cast<float>(interval->entryDistance);
    const float exitDistance = static_cast<float>(interval->exitDistance);
    if (!core::IsFinite(entryDistance) || !core::IsFinite(exitDistance) ||
        exitDistance < entryDistance)
    {
        return std::nullopt;
    }

    return RayBoxHit{entryDistance, exitDistance};
}
} // namespace pond::math
