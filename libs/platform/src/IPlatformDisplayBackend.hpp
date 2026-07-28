#pragma once

#include <cstdint>
#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "IPlatformWindowBackend.hpp"

namespace ponder::platform::detail
{
enum class BackendDisplayOrientation : std::uint8_t
{
    Unknown,
    Landscape,
    LandscapeFlipped,
    Portrait,
    PortraitFlipped
};

struct BackendScreenRectangle final
{
    int x{};
    int y{};
    int width{};
    int height{};
};

class IPlatformDisplayBackend
{
public:
    virtual ~IPlatformDisplayBackend() noexcept = default;

    IPlatformDisplayBackend(const IPlatformDisplayBackend&) = delete;
    IPlatformDisplayBackend& operator=(const IPlatformDisplayBackend&) = delete;
    IPlatformDisplayBackend(IPlatformDisplayBackend&&) = delete;
    IPlatformDisplayBackend& operator=(IPlatformDisplayBackend&&) = delete;

    [[nodiscard]] virtual std::vector<std::uint32_t> Enumerate() = 0;
    [[nodiscard]] virtual std::string GetName(std::uint32_t displayId) = 0;
    [[nodiscard]] virtual BackendScreenRectangle GetBounds(std::uint32_t displayId) = 0;
    [[nodiscard]] virtual BackendScreenRectangle GetUsableBounds(std::uint32_t displayId) = 0;
    [[nodiscard]] virtual float GetCurrentRefreshRate(std::uint32_t displayId) = 0;
    [[nodiscard]] virtual BackendDisplayOrientation GetCurrentOrientation(std::uint32_t displayId) = 0;
    [[nodiscard]] virtual float GetContentScale(std::uint32_t displayId) = 0;
    [[nodiscard]] virtual std::uint32_t GetForWindow(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual float GetWindowPixelDensity(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual float GetWindowDisplayScale(BackendWindowHandle window) = 0;

protected:
    IPlatformDisplayBackend() noexcept = default;
};

} // namespace ponder::platform::detail

namespace std
{
template <>
struct formatter<ponder::platform::detail::BackendDisplayOrientation> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::detail::BackendDisplayOrientation orientation, FormatContext& context) const
    {
        using ponder::platform::detail::BackendDisplayOrientation;

        string_view name{"unknown"};
        switch (orientation)
        {
        case BackendDisplayOrientation::Unknown:
            break;
        case BackendDisplayOrientation::Landscape:
            name = "landscape";
            break;
        case BackendDisplayOrientation::LandscapeFlipped:
            name = "landscape_flipped";
            break;
        case BackendDisplayOrientation::Portrait:
            name = "portrait";
            break;
        case BackendDisplayOrientation::PortraitFlipped:
            name = "portrait_flipped";
            break;
        }

        return formatter<string_view>::format(name, context);
    }
};

template <>
struct formatter<ponder::platform::detail::BackendScreenRectangle> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::detail::BackendScreenRectangle rectangle, FormatContext& context) const
    {
        return formatter<string>::format(std::format("({}, {}) / {}x{}", rectangle.x, rectangle.y, rectangle.width, rectangle.height), context);
    }
};
} // namespace std

namespace ponder::platform::detail
{
inline std::ostream& operator<<(std::ostream& output, BackendDisplayOrientation orientation)
{
    return output << std::format("{}", orientation);
}

inline std::ostream& operator<<(std::ostream& output, BackendScreenRectangle rectangle)
{
    return output << std::format("{}", rectangle);
}
} // namespace ponder::platform::detail
