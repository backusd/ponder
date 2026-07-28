#include "SdlDisplayBackend.hpp"

#include <ponder/core/Exception.hpp>
#include <ponder/platform/PlatformError.hpp>

#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>
#include <format>
#include <memory>
#include <string>
#include <vector>

#include "SdlCommon.hpp"
#include "SdlError.hpp"

namespace ponder::platform::detail
{
namespace
{
struct SdlDisplayListDeleter final
{
    void operator()(SDL_DisplayID* displays) const noexcept
    {
        SDL_free(displays);
    }
};

[[nodiscard]] std::string GetBackendDisplayContext(std::uint32_t displayId)
{
    return std::format("backend display {}", displayId);
}
} // namespace

std::vector<std::uint32_t> SdlDisplayBackend::Enumerate()
{
    int count{};
    SDL_DisplayID* const rawDisplays = SDL_GetDisplays(&count);
    if (rawDisplays == nullptr)
    {
        const std::string message = CaptureSdlFailureMessage("SDL_GetDisplays", "displays");
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", message);
    }

    std::unique_ptr<SDL_DisplayID, SdlDisplayListDeleter> displays{rawDisplays};
    if (count < 0)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL_GetDisplays returned an invalid negative display count.");
    }

    return std::vector<std::uint32_t>{displays.get(), displays.get() + count};
}

std::string SdlDisplayBackend::GetName(std::uint32_t displayId)
{
    const std::string context = GetBackendDisplayContext(displayId);
    const char* const name = SDL_GetDisplayName(static_cast<SDL_DisplayID>(displayId));
    if (name == nullptr)
    {
        const std::string message = CaptureSdlFailureMessage("SDL_GetDisplayName", context);
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", message);
    }

    return std::string{name};
}

BackendScreenRectangle SdlDisplayBackend::GetBounds(std::uint32_t displayId)
{
    const std::string context = GetBackendDisplayContext(displayId);
    SDL_Rect rectangle{};
    if (!SDL_GetDisplayBounds(static_cast<SDL_DisplayID>(displayId), &rectangle))
    {
        const std::string message = CaptureSdlFailureMessage("SDL_GetDisplayBounds", context);
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", message);
    }

    return BackendScreenRectangle{rectangle.x, rectangle.y, rectangle.w, rectangle.h};
}

BackendScreenRectangle SdlDisplayBackend::GetUsableBounds(std::uint32_t displayId)
{
    const std::string context = GetBackendDisplayContext(displayId);
    SDL_Rect rectangle{};
    if (!SDL_GetDisplayUsableBounds(static_cast<SDL_DisplayID>(displayId), &rectangle))
    {
        const std::string message = CaptureSdlFailureMessage("SDL_GetDisplayUsableBounds", context);
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", message);
    }

    return BackendScreenRectangle{rectangle.x, rectangle.y, rectangle.w, rectangle.h};
}

float SdlDisplayBackend::GetCurrentRefreshRate(std::uint32_t displayId)
{
    const std::string context = GetBackendDisplayContext(displayId);
    const SDL_DisplayMode* const mode = SDL_GetCurrentDisplayMode(static_cast<SDL_DisplayID>(displayId));
    if (mode == nullptr)
    {
        const std::string message = CaptureSdlFailureMessage("SDL_GetCurrentDisplayMode", context);
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", message);
    }

    return mode->refresh_rate;
}

BackendDisplayOrientation SdlDisplayBackend::GetCurrentOrientation(std::uint32_t displayId) noexcept
{
    switch (SDL_GetCurrentDisplayOrientation(static_cast<SDL_DisplayID>(displayId)))
    {
    case SDL_ORIENTATION_LANDSCAPE:
        return BackendDisplayOrientation::Landscape;
    case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
        return BackendDisplayOrientation::LandscapeFlipped;
    case SDL_ORIENTATION_PORTRAIT:
        return BackendDisplayOrientation::Portrait;
    case SDL_ORIENTATION_PORTRAIT_FLIPPED:
        return BackendDisplayOrientation::PortraitFlipped;
    case SDL_ORIENTATION_UNKNOWN:
        return BackendDisplayOrientation::Unknown;
    }

    return BackendDisplayOrientation::Unknown;
}

float SdlDisplayBackend::GetContentScale(std::uint32_t displayId)
{
    const std::string context = GetBackendDisplayContext(displayId);
    const float scale = SDL_GetDisplayContentScale(static_cast<SDL_DisplayID>(displayId));
    if (scale == 0.0F)
    {
        const std::string message = CaptureSdlFailureMessage("SDL_GetDisplayContentScale", context);
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", message);
    }

    return scale;
}

std::uint32_t SdlDisplayBackend::GetForWindow(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    const SDL_DisplayID displayId = SDL_GetDisplayForWindow(ToSdlWindow(window));
    if (displayId == 0)
    {
        const std::string message = CaptureSdlFailureMessage("SDL_GetDisplayForWindow", context);
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", message);
    }

    return displayId;
}

float SdlDisplayBackend::GetWindowPixelDensity(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    const float density = SDL_GetWindowPixelDensity(ToSdlWindow(window));
    if (density == 0.0F)
    {
        const std::string message = CaptureSdlFailureMessage("SDL_GetWindowPixelDensity", context);
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", message);
    }

    return density;
}

float SdlDisplayBackend::GetWindowDisplayScale(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    const float scale = SDL_GetWindowDisplayScale(ToSdlWindow(window));
    if (scale == 0.0F)
    {
        const std::string message = CaptureSdlFailureMessage("SDL_GetWindowDisplayScale", context);
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", message);
    }

    return scale;
}
} // namespace ponder::platform::detail
