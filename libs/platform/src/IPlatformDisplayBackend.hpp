#pragma once

#include <ponder/core/Result.hpp>

#include <cstdint>
#include <format>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "IPlatformWindowBackend.hpp"

namespace pond::platform::detail
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

    [[nodiscard]] virtual core::Result<std::vector<std::uint32_t>> Enumerate() = 0;
    [[nodiscard]] virtual core::Result<std::string> GetName(std::uint32_t displayId) = 0;
    [[nodiscard]] virtual core::Result<BackendScreenRectangle> GetBounds(
        std::uint32_t displayId) = 0;
    [[nodiscard]] virtual core::Result<BackendScreenRectangle> GetUsableBounds(
        std::uint32_t displayId) = 0;
    [[nodiscard]] virtual core::Result<float> GetCurrentRefreshRate(
        std::uint32_t displayId) = 0;
    [[nodiscard]] virtual BackendDisplayOrientation GetCurrentOrientation(
        std::uint32_t displayId) = 0;
    [[nodiscard]] virtual core::Result<float> GetContentScale(std::uint32_t displayId) = 0;
    [[nodiscard]] virtual core::Result<std::uint32_t> GetForWindow(
        BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::Result<float> GetWindowPixelDensity(
        BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::Result<float> GetWindowDisplayScale(
        BackendWindowHandle window) = 0;

protected:
    IPlatformDisplayBackend() noexcept = default;
};

class IPlatformDisplayBackendFactory
{
public:
    virtual ~IPlatformDisplayBackendFactory() noexcept = default;

    IPlatformDisplayBackendFactory(const IPlatformDisplayBackendFactory&) = delete;
    IPlatformDisplayBackendFactory& operator=(const IPlatformDisplayBackendFactory&) = delete;
    IPlatformDisplayBackendFactory(IPlatformDisplayBackendFactory&&) = delete;
    IPlatformDisplayBackendFactory& operator=(IPlatformDisplayBackendFactory&&) = delete;

    [[nodiscard]] virtual std::unique_ptr<IPlatformDisplayBackend> Create() const = 0;

protected:
    IPlatformDisplayBackendFactory() noexcept = default;
};
} // namespace pond::platform::detail

namespace std
{
template <>
struct formatter<pond::platform::detail::BackendDisplayOrientation> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::BackendDisplayOrientation orientation,
                FormatContext& context) const
    {
        using pond::platform::detail::BackendDisplayOrientation;

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
struct formatter<pond::platform::detail::BackendScreenRectangle> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::BackendScreenRectangle rectangle,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("({}, {}) / {}x{}", rectangle.x, rectangle.y, rectangle.width,
                        rectangle.height),
            context);
    }
};
} // namespace std

namespace pond::platform::detail
{
inline std::ostream& operator<<(std::ostream& output, BackendDisplayOrientation orientation)
{
    return output << std::format("{}", orientation);
}

inline std::ostream& operator<<(std::ostream& output, BackendScreenRectangle rectangle)
{
    return output << std::format("{}", rectangle);
}
} // namespace pond::platform::detail