#include "PlatformRuntimeBackend.hpp"

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_platform_defines.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <atomic>
#include <memory>
#include <vector>

namespace pond::platform::detail
{
bool IsWindowGraphicsCompatibilitySupported(
    WindowGraphicsCompatibility compatibility) noexcept
{
    switch (compatibility)
    {
    case WindowGraphicsCompatibility::Default:
        return true;
    case WindowGraphicsCompatibility::Vulkan:
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX) || \
    defined(SDL_PLATFORM_MACOS)
        return true;
#else
        return false;
#endif
    }

    return false;
}

std::uint64_t BuildSdlWindowFlags(const BackendWindowCreateDesc& desc) noexcept
{
    SDL_WindowFlags flags = SDL_WINDOW_HIDDEN;
    if (desc.resizable)
    {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (desc.highPixelDensity)
    {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }

    if (desc.graphicsCompatibility == WindowGraphicsCompatibility::Vulkan)
    {
#if defined(SDL_PLATFORM_MACOS)
        flags |= SDL_WINDOW_METAL;
#else
        flags |= SDL_WINDOW_VULKAN;
#endif
    }

    return flags;
}

bool IsReservedSdlWindowPosition(std::int32_t value) noexcept
{
    const int backendValue = static_cast<int>(value);
    return SDL_WINDOWPOS_ISUNDEFINED(backendValue) ||
           SDL_WINDOWPOS_ISCENTERED(backendValue);
}

namespace
{
struct SdlDisplayListDeleter final
{
    void operator()(SDL_DisplayID* displays) const noexcept
    {
        SDL_free(displays);
    }
};

[[nodiscard]] bool IsMainThread(void*) noexcept
{
    return SDL_IsMainThread();
}

[[nodiscard]] bool HasInitializedSubsystems(void*) noexcept
{
    return SDL_WasInit(0) != 0;
}

[[nodiscard]] bool HasExpectedRuntimeSubsystems(void*) noexcept
{
    constexpr SDL_InitFlags kRuntimeSubsystems = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
    const SDL_InitFlags initializedSubsystems = SDL_WasInit(0);
    return (initializedSubsystems & kRuntimeSubsystems) == kRuntimeSubsystems &&
           (initializedSubsystems & ~kRuntimeSubsystems) == 0;
}

[[nodiscard]] const char* GetMetadataPropertyName(
    ApplicationMetadataProperty property) noexcept
{
    switch (property)
    {
    case ApplicationMetadataProperty::Name:
        return SDL_PROP_APP_METADATA_NAME_STRING;
    case ApplicationMetadataProperty::Version:
        return SDL_PROP_APP_METADATA_VERSION_STRING;
    case ApplicationMetadataProperty::Identifier:
        return SDL_PROP_APP_METADATA_IDENTIFIER_STRING;
    }

    return nullptr;
}

[[nodiscard]] const char* GetAppMetadataProperty(
    void*, ApplicationMetadataProperty property) noexcept
{
    const char* const propertyName = GetMetadataPropertyName(property);
    return propertyName != nullptr ? SDL_GetAppMetadataProperty(propertyName) : nullptr;
}

[[nodiscard]] bool SetAppMetadataProperty(void*, ApplicationMetadataProperty property,
                                          const char* value) noexcept
{
    const char* const propertyName = GetMetadataPropertyName(property);
    return propertyName != nullptr && SDL_SetAppMetadataProperty(propertyName, value);
}

[[nodiscard]] const char* GetHint(void*, const char* name) noexcept
{
    return SDL_GetHint(name);
}

[[nodiscard]] bool SetHintOverride(void*, const char* name, const char* value) noexcept
{
    return SDL_SetHintWithPriority(name, value, SDL_HINT_OVERRIDE);
}

[[nodiscard]] bool ResetHint(void*, const char* name) noexcept
{
    return SDL_ResetHint(name);
}

[[nodiscard]] bool InitializeVideo(void*) noexcept
{
    return SDL_Init(SDL_INIT_VIDEO);
}

void Quit(void*) noexcept
{
    SDL_Quit();
}

[[nodiscard]] std::uint64_t GetTicksNanoseconds(void*) noexcept
{
    return SDL_GetTicksNS();
}

[[nodiscard]] void* CreateWindow(void*, const BackendWindowCreateDesc& desc) noexcept
{
    return SDL_CreateWindow(desc.title, desc.width, desc.height,
                            static_cast<SDL_WindowFlags>(BuildSdlWindowFlags(desc)));
}

void DestroyWindow(void*, void* window) noexcept
{
    SDL_DestroyWindow(static_cast<SDL_Window*>(window));
}

[[nodiscard]] std::uint32_t GetWindowId(void*, void* window) noexcept
{
    return SDL_GetWindowID(static_cast<SDL_Window*>(window));
}

[[nodiscard]] const char* GetWindowTitle(void*, void* window) noexcept
{
    return SDL_GetWindowTitle(static_cast<SDL_Window*>(window));
}

[[nodiscard]] bool SetWindowTitle(void*, void* window, const char* title) noexcept
{
    return SDL_SetWindowTitle(static_cast<SDL_Window*>(window), title);
}

[[nodiscard]] bool GetWindowPosition(void*, void* window, int* x, int* y) noexcept
{
    return SDL_GetWindowPosition(static_cast<SDL_Window*>(window), x, y);
}

[[nodiscard]] bool SetWindowPosition(void*, void* window, int x, int y) noexcept
{
    return SDL_SetWindowPosition(static_cast<SDL_Window*>(window), x, y);
}

[[nodiscard]] bool GetWindowSize(void*, void* window, int* width, int* height) noexcept
{
    return SDL_GetWindowSize(static_cast<SDL_Window*>(window), width, height);
}

[[nodiscard]] bool GetWindowSizeInPixels(void*, void* window, int* width,
                                         int* height) noexcept
{
    return SDL_GetWindowSizeInPixels(static_cast<SDL_Window*>(window), width, height);
}

[[nodiscard]] bool SetWindowSize(void*, void* window, int width, int height) noexcept
{
    return SDL_SetWindowSize(static_cast<SDL_Window*>(window), width, height);
}

[[nodiscard]] bool SetWindowMinimumSize(void*, void* window, int width,
                                        int height) noexcept
{
    return SDL_SetWindowMinimumSize(static_cast<SDL_Window*>(window), width, height);
}

[[nodiscard]] bool ShowWindow(void*, void* window) noexcept
{
    return SDL_ShowWindow(static_cast<SDL_Window*>(window));
}

[[nodiscard]] bool HideWindow(void*, void* window) noexcept
{
    return SDL_HideWindow(static_cast<SDL_Window*>(window));
}

[[nodiscard]] bool EnumerateDisplays(void*,
                                     std::vector<std::uint32_t>& displayIds)
{
    int count{};
    std::unique_ptr<SDL_DisplayID, SdlDisplayListDeleter> displays{
        SDL_GetDisplays(&count)};
    if (displays == nullptr)
    {
        return false;
    }

    displayIds.assign(displays.get(), displays.get() + count);
    return true;
}

[[nodiscard]] const char* GetDisplayName(void*, std::uint32_t displayId) noexcept
{
    return SDL_GetDisplayName(displayId);
}

[[nodiscard]] bool GetDisplayBounds(void*, std::uint32_t displayId,
                                    BackendScreenRectangle* bounds) noexcept
{
    SDL_Rect rectangle{};
    if (!SDL_GetDisplayBounds(displayId, &rectangle))
    {
        return false;
    }

    *bounds = BackendScreenRectangle{
        rectangle.x, rectangle.y, rectangle.w, rectangle.h};
    return true;
}

[[nodiscard]] bool GetDisplayUsableBounds(
    void*, std::uint32_t displayId, BackendScreenRectangle* bounds) noexcept
{
    SDL_Rect rectangle{};
    if (!SDL_GetDisplayUsableBounds(displayId, &rectangle))
    {
        return false;
    }

    *bounds = BackendScreenRectangle{
        rectangle.x, rectangle.y, rectangle.w, rectangle.h};
    return true;
}

[[nodiscard]] bool GetCurrentDisplayRefreshRate(
    void*, std::uint32_t displayId, float* refreshRateHertz) noexcept
{
    const SDL_DisplayMode* const mode = SDL_GetCurrentDisplayMode(displayId);
    if (mode == nullptr)
    {
        return false;
    }

    *refreshRateHertz = mode->refresh_rate;
    return true;
}

[[nodiscard]] BackendDisplayOrientation GetCurrentDisplayOrientation(
    void*, std::uint32_t displayId) noexcept
{
    switch (SDL_GetCurrentDisplayOrientation(displayId))
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

[[nodiscard]] float GetDisplayContentScale(void*,
                                           std::uint32_t displayId) noexcept
{
    return SDL_GetDisplayContentScale(displayId);
}

[[nodiscard]] std::uint32_t GetDisplayForWindow(void*, void* window) noexcept
{
    return SDL_GetDisplayForWindow(static_cast<SDL_Window*>(window));
}

[[nodiscard]] float GetWindowPixelDensity(void*, void* window) noexcept
{
    return SDL_GetWindowPixelDensity(static_cast<SDL_Window*>(window));
}

[[nodiscard]] float GetWindowDisplayScale(void*, void* window) noexcept
{
    return SDL_GetWindowDisplayScale(static_cast<SDL_Window*>(window));
}

constexpr PlatformWindowBackend kSdlWindowBackend{
    nullptr,
    CreateWindow,
    DestroyWindow,
    GetWindowId,
    GetWindowTitle,
    SetWindowTitle,
    GetWindowPosition,
    SetWindowPosition,
    GetWindowSize,
    GetWindowSizeInPixels,
    SetWindowSize,
    SetWindowMinimumSize,
    ShowWindow,
    HideWindow};

constexpr PlatformDisplayBackend kSdlDisplayBackend{
    nullptr,
    EnumerateDisplays,
    GetDisplayName,
    GetDisplayBounds,
    GetDisplayUsableBounds,
    GetCurrentDisplayRefreshRate,
    GetCurrentDisplayOrientation,
    GetDisplayContentScale,
    GetDisplayForWindow,
    GetWindowPixelDensity,
    GetWindowDisplayScale};

constexpr PlatformRuntimeBackend kSdlBackend{
    nullptr,
    IsMainThread,
    HasInitializedSubsystems,
    HasExpectedRuntimeSubsystems,
    GetAppMetadataProperty,
    SetAppMetadataProperty,
    GetHint,
    SetHintOverride,
    ResetHint,
    InitializeVideo,
    Quit,
    GetTicksNanoseconds};

std::atomic<const PlatformRuntimeBackend*> backendOverride{nullptr};
std::atomic<const PlatformWindowBackend*> windowBackendOverride{nullptr};
std::atomic<const PlatformDisplayBackend*> displayBackendOverride{nullptr};
} // namespace

PlatformRuntimeBackend GetPlatformRuntimeBackend() noexcept
{
    const PlatformRuntimeBackend* const overrideBackend =
        backendOverride.load(std::memory_order_acquire);
    return overrideBackend != nullptr ? *overrideBackend : kSdlBackend;
}

PlatformWindowBackend GetPlatformWindowBackend() noexcept
{
    const PlatformWindowBackend* const overrideBackend =
        windowBackendOverride.load(std::memory_order_acquire);
    return overrideBackend != nullptr ? *overrideBackend : kSdlWindowBackend;
}

PlatformDisplayBackend GetPlatformDisplayBackend() noexcept
{
    const PlatformDisplayBackend* const overrideBackend =
        displayBackendOverride.load(std::memory_order_acquire);
    return overrideBackend != nullptr ? *overrideBackend : kSdlDisplayBackend;
}

void SetPlatformRuntimeBackendForTesting(const PlatformRuntimeBackend* backend) noexcept
{
    backendOverride.store(backend, std::memory_order_release);
}

void SetPlatformWindowBackendForTesting(const PlatformWindowBackend* backend) noexcept
{
    windowBackendOverride.store(backend, std::memory_order_release);
}

void SetPlatformDisplayBackendForTesting(const PlatformDisplayBackend* backend) noexcept
{
    displayBackendOverride.store(backend, std::memory_order_release);
}
} // namespace pond::platform::detail
