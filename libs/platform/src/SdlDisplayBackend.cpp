#include "SdlDisplayBackend.hpp"

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

namespace pond::platform::detail
{
namespace
{
constexpr core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);

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

core::Result<std::vector<std::uint32_t>> SdlDisplayBackend::Enumerate()
{
    int count{};
    std::unique_ptr<SDL_DisplayID, SdlDisplayListDeleter> displays{SDL_GetDisplays(&count)};
    if (displays == nullptr)
    {
        return core::Result<std::vector<std::uint32_t>>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetDisplays", "displays"));
    }

    return std::vector<std::uint32_t>{displays.get(), displays.get() + count};
}

core::Result<std::string> SdlDisplayBackend::GetName(std::uint32_t displayId)
{
    const char* const name = SDL_GetDisplayName(static_cast<SDL_DisplayID>(displayId));
    if (name == nullptr)
    {
        return core::Result<std::string>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetDisplayName", GetBackendDisplayContext(displayId)));
    }

    return std::string{name};
}

core::Result<BackendScreenRectangle> SdlDisplayBackend::GetBounds(std::uint32_t displayId)
{
    SDL_Rect rectangle{};
    if (!SDL_GetDisplayBounds(static_cast<SDL_DisplayID>(displayId), &rectangle))
    {
        return core::Result<BackendScreenRectangle>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetDisplayBounds", GetBackendDisplayContext(displayId)));
    }

    return BackendScreenRectangle{rectangle.x, rectangle.y, rectangle.w, rectangle.h};
}

core::Result<BackendScreenRectangle> SdlDisplayBackend::GetUsableBounds(
    std::uint32_t displayId)
{
    SDL_Rect rectangle{};
    if (!SDL_GetDisplayUsableBounds(static_cast<SDL_DisplayID>(displayId), &rectangle))
    {
        return core::Result<BackendScreenRectangle>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetDisplayUsableBounds",
                              GetBackendDisplayContext(displayId)));
    }

    return BackendScreenRectangle{rectangle.x, rectangle.y, rectangle.w, rectangle.h};
}

core::Result<float> SdlDisplayBackend::GetCurrentRefreshRate(std::uint32_t displayId)
{
    const SDL_DisplayMode* const mode =
        SDL_GetCurrentDisplayMode(static_cast<SDL_DisplayID>(displayId));
    if (mode == nullptr)
    {
        return core::Result<float>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetCurrentDisplayMode", GetBackendDisplayContext(displayId)));
    }

    return mode->refresh_rate;
}

BackendDisplayOrientation SdlDisplayBackend::GetCurrentOrientation(
    std::uint32_t displayId) noexcept
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

core::Result<float> SdlDisplayBackend::GetContentScale(std::uint32_t displayId)
{
    const float scale = SDL_GetDisplayContentScale(static_cast<SDL_DisplayID>(displayId));
    if (scale == 0.0F)
    {
        return core::Result<float>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetDisplayContentScale",
            GetBackendDisplayContext(displayId)));
    }

    return scale;
}

core::Result<std::uint32_t> SdlDisplayBackend::GetForWindow(BackendWindowHandle window)
{
    const SDL_DisplayID displayId = SDL_GetDisplayForWindow(ToSdlWindow(window));
    if (displayId == 0)
    {
        return core::Result<std::uint32_t>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetDisplayForWindow", GetBackendWindowContext(window)));
    }

    return displayId;
}

core::Result<float> SdlDisplayBackend::GetWindowPixelDensity(BackendWindowHandle window)
{
    const float density = SDL_GetWindowPixelDensity(ToSdlWindow(window));
    if (density == 0.0F)
    {
        return core::Result<float>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetWindowPixelDensity", GetBackendWindowContext(window)));
    }

    return density;
}

core::Result<float> SdlDisplayBackend::GetWindowDisplayScale(BackendWindowHandle window)
{
    const float scale = SDL_GetWindowDisplayScale(ToSdlWindow(window));
    if (scale == 0.0F)
    {
        return core::Result<float>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetWindowDisplayScale", GetBackendWindowContext(window)));
    }

    return scale;
}
} // namespace pond::platform::detail