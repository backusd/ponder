#pragma once

#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>

#include <cstdint>
#include <optional>
#include <string>

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

struct DisplayInfo final
{
    DisplayId id;
    std::string name;
    ScreenRectangle bounds;
    ScreenRectangle usableBounds;
    std::optional<float> refreshRateHertz;
    DisplayOrientation orientation{DisplayOrientation::Unknown};
    float contentScale{};

    [[nodiscard]] friend bool operator==(const DisplayInfo& lhs,
                                         const DisplayInfo& rhs) = default;
};
} // namespace pond::platform
