#pragma once

#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>

#include <cstdint>
#include <format>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace pond::platform
{
enum class DisplayOrientation : std::uint8_t
{
    Unknown = 0,
    Landscape,
    LandscapeFlipped,
    Portrait,
    PortraitFlipped
};

[[nodiscard]] constexpr std::string_view GetDisplayOrientationName(
    DisplayOrientation orientation) noexcept
{
    switch (orientation)
    {
    case DisplayOrientation::Unknown:
        return "unknown";
    case DisplayOrientation::Landscape:
        return "landscape";
    case DisplayOrientation::LandscapeFlipped:
        return "landscape_flipped";
    case DisplayOrientation::Portrait:
        return "portrait";
    case DisplayOrientation::PortraitFlipped:
        return "portrait_flipped";
    }

    return "unrecognized";
}

inline std::ostream& operator<<(std::ostream& output, DisplayOrientation orientation)
{
    return output << GetDisplayOrientationName(orientation);
}

struct DisplayInfo final
{
    DisplayId id;
    std::string name;
    ScreenRectangle bounds;
    ScreenRectangle usableBounds;
    std::optional<float> refreshRateHertz;
    DisplayOrientation orientation{DisplayOrientation::Unknown};
    float contentScale{};

    [[nodiscard]] friend bool operator==(const DisplayInfo& lhs, const DisplayInfo& rhs) = default;
};
} // namespace pond::platform

namespace std
{
template <>
struct formatter<pond::platform::DisplayOrientation> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::DisplayOrientation orientation, FormatContext& context) const
    {
        return formatter<string_view>::format(
            pond::platform::GetDisplayOrientationName(orientation), context);
    }
};
} // namespace std