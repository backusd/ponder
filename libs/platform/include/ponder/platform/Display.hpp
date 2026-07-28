#pragma once

#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>

#include <cstdint>
#include <format>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace ponder::platform
{
enum class DisplayOrientation : std::uint8_t
{
    Unknown = 0,
    Landscape,
    LandscapeFlipped,
    Portrait,
    PortraitFlipped
};

[[nodiscard]] constexpr std::string_view GetDisplayOrientationName(DisplayOrientation orientation) noexcept
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
} // namespace ponder::platform

namespace std
{
template <>
struct formatter<ponder::platform::DisplayOrientation> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::DisplayOrientation orientation, FormatContext& context) const
    {
        return formatter<string_view>::format(ponder::platform::GetDisplayOrientationName(orientation), context);
    }
};
template <>
struct formatter<ponder::platform::DisplayInfo> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DisplayInfo& info, FormatContext& context) const
    {
        const string refreshRate = info.refreshRateHertz.has_value() ? std::format("{} Hz", *info.refreshRateHertz) : "unknown";
        return formatter<string>::format(std::format("display {} '{}': bounds={}, usableBounds={}, refreshRate={}, "
                                                     "orientation={}, contentScale={}",
                                                     info.id, info.name, info.bounds, info.usableBounds, refreshRate, info.orientation,
                                                     info.contentScale),
                                         context);
    }
};
} // namespace std
namespace ponder::platform
{
inline std::ostream& operator<<(std::ostream& output, DisplayOrientation orientation)
{
    return output << std::format("{}", orientation);
}

inline std::ostream& operator<<(std::ostream& output, const DisplayInfo& info)
{
    return output << std::format("{}", info);
}
} // namespace ponder::platform
