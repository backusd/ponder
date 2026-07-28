#include "SdlWindowBackend.hpp"

#include <ponder/core/Exception.hpp>
#include <ponder/platform/PlatformError.hpp>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_platform_defines.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_video.h>
#include <cstdint>
#include <format>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

#include "SdlCommon.hpp"
#include "SdlError.hpp"
#include "SdlRuntimeTypes.hpp"

namespace ponder::platform::detail
{
bool IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility compatibility) noexcept
{
    switch (compatibility)
    {
    case WindowGraphicsCompatibility::Default:
        return true;
    case WindowGraphicsCompatibility::Vulkan:
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
        return true;
#else
        return false;
#endif
    case WindowGraphicsCompatibility::Metal:
#if defined(SDL_PLATFORM_MACOS)
        return true;
#else
        return false;
#endif
    }

    return false;
}

BackendNativeWindowDriver GetNativeWindowDriver(std::string_view driverName) noexcept
{
    if (driverName == "windows")
    {
        return BackendNativeWindowDriver::Win32;
    }
    if (driverName == "x11")
    {
        return BackendNativeWindowDriver::X11;
    }
    if (driverName == "wayland")
    {
        return BackendNativeWindowDriver::Wayland;
    }

    return BackendNativeWindowDriver::Unsupported;
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

    switch (desc.graphicsCompatibility)
    {
    case WindowGraphicsCompatibility::Default:
        break;
    case WindowGraphicsCompatibility::Vulkan:
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
        flags |= SDL_WINDOW_VULKAN;
#endif
        break;
    case WindowGraphicsCompatibility::Metal:
#if defined(SDL_PLATFORM_MACOS)
        flags |= SDL_WINDOW_METAL;
#endif
        break;
    }

    return flags;
}

bool IsReservedSdlWindowPosition(std::int32_t value) noexcept
{
    const int backendValue = static_cast<int>(value);
    return SDL_WINDOWPOS_ISUNDEFINED(backendValue) || SDL_WINDOWPOS_ISCENTERED(backendValue);
}

namespace
{
constexpr ponder::core::ErrorCode kUnsupportedCode = ToErrorCode(PlatformErrorCode::Unsupported);

[[noreturn]] void ThrowWindowBackendFailure(std::string_view operation, std::string_view objectContext,
                                            std::source_location location = std::source_location::current())
{
    const std::string message = CaptureSdlFailureMessage(operation, objectContext);
    throw ponder::core::MakeFormattedException(location, "Platform error [{}]: {}", PlatformErrorCode::BackendFailure, message);
}

void ThrowIfWindowOperationFailed(bool succeeded, std::string_view operation, std::string_view objectContext,
                                  std::source_location location = std::source_location::current())
{
    if (!succeeded)
    {
        ThrowWindowBackendFailure(operation, objectContext, location);
    }
}

[[noreturn]] void ThrowUnsupportedWindowOperation(BackendWindowHandle window, std::string_view operation,
                                                  std::source_location location = std::source_location::current())
{
    throw ponder::core::MakeFormattedException(location,
                                               "Platform error [{}]: {} did not apply the requested state for {}; the active video "
                                               "driver may not support this operation.",
                                               PlatformErrorCode::Unsupported, operation, GetBackendWindowContext(window));
}

[[nodiscard]] ponder::core::Result<NativeWindowHandle> MakeUnsupportedNativeHandleError(BackendWindowHandle window, std::string_view message)
{
    return ponder::core::Result<NativeWindowHandle>::FromError(
        ponder::core::Error{kUnsupportedCode, std::format("{} Context: {}.", message, GetBackendWindowContext(window))});
}

[[noreturn]] void ThrowMalformedNativeWindowProperties(BackendWindowHandle window, std::string_view message,
                                                       std::source_location location = std::source_location::current())
{
    throw ponder::core::MakeFormattedException(location, "Platform error [{}]: {} Context: {}.", PlatformErrorCode::BackendFailure, message,
                                               GetBackendWindowContext(window));
}

[[nodiscard]] NativeWindowHandle GetWin32NativeWindowHandle(SDL_PropertiesID properties, BackendWindowHandle backendWindow)
{
    void* const instance = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
    void* const window = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    return MakeWin32NativeWindowHandle(backendWindow, instance, window);
}

[[nodiscard]] NativeWindowHandle GetX11NativeWindowHandle(SDL_PropertiesID properties, BackendWindowHandle backendWindow)
{
    void* const display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    const Sint64 window = SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    return MakeX11NativeWindowHandle(backendWindow, display, window);
}

[[nodiscard]] NativeWindowHandle GetWaylandNativeWindowHandle(SDL_PropertiesID properties, BackendWindowHandle backendWindow)
{
    void* const display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    void* const surface = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    return MakeWaylandNativeWindowHandle(backendWindow, display, surface);
}
} // namespace

NativeWindowHandle MakeWin32NativeWindowHandle(BackendWindowHandle backendWindow, void* instance, void* nativeWindow)
{
    if (instance == nullptr || nativeWindow == nullptr)
    {
        ThrowMalformedNativeWindowProperties(backendWindow, "SDL window is missing Win32 native properties.");
    }

    return NativeWin32Window{.instance = instance, .window = nativeWindow};
}

NativeWindowHandle MakeX11NativeWindowHandle(BackendWindowHandle backendWindow, void* display, std::int64_t nativeWindow)
{
    if (display == nullptr || nativeWindow <= 0)
    {
        ThrowMalformedNativeWindowProperties(backendWindow, "SDL window is missing X11 native properties.");
    }
    if (!std::in_range<std::uintptr_t>(nativeWindow))
    {
        ThrowMalformedNativeWindowProperties(backendWindow, "SDL X11 window property is too large for uintptr_t.");
    }

    return NativeX11Window{.display = display, .window = static_cast<std::uintptr_t>(nativeWindow)};
}

NativeWindowHandle MakeWaylandNativeWindowHandle(BackendWindowHandle backendWindow, void* display, void* surface)
{
    if (display == nullptr || surface == nullptr)
    {
        ThrowMalformedNativeWindowProperties(backendWindow, "SDL window is missing Wayland native properties.");
    }

    return NativeWaylandWindow{.display = display, .surface = surface};
}

BackendWindowHandle SdlWindowBackend::Create(const BackendWindowCreateDesc& desc)
{
    const std::string title{desc.title};
    SDL_Window* const window =
        SDL_CreateWindow(title.c_str(), desc.logicalSize.width, desc.logicalSize.height, static_cast<SDL_WindowFlags>(BuildSdlWindowFlags(desc)));
    if (window == nullptr)
    {
        ThrowWindowBackendFailure("SDL_CreateWindow", "window");
    }

    return ToBackendWindowHandle(window);
}

void SdlWindowBackend::Destroy(BackendWindowHandle window) noexcept
{
    SDL_DestroyWindow(ToSdlWindow(window));
}

std::uint32_t SdlWindowBackend::GetId(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    const SDL_WindowID id = SDL_GetWindowID(ToSdlWindow(window));
    if (id == 0)
    {
        ThrowWindowBackendFailure("SDL_GetWindowID", context);
    }

    return id;
}

std::string SdlWindowBackend::GetTitle(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    const char* const title = SDL_GetWindowTitle(ToSdlWindow(window));
    if (title == nullptr)
    {
        ThrowWindowBackendFailure("SDL_GetWindowTitle", context);
    }
    return std::string{title};
}

void SdlWindowBackend::SetTitle(BackendWindowHandle window, std::string_view title)
{
    const std::string ownedTitle{title};
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_SetWindowTitle(ToSdlWindow(window), ownedTitle.c_str()), "SDL_SetWindowTitle", context);
}

BackendWindowPosition SdlWindowBackend::GetPosition(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    BackendWindowPosition position;
    if (!SDL_GetWindowPosition(ToSdlWindow(window), &position.x, &position.y))
    {
        ThrowWindowBackendFailure("SDL_GetWindowPosition", context);
    }
    return position;
}

void SdlWindowBackend::SetPosition(BackendWindowHandle window, BackendWindowPosition position)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_SetWindowPosition(ToSdlWindow(window), position.x, position.y), "SDL_SetWindowPosition", context);
}

BackendWindowLogicalSize SdlWindowBackend::GetSize(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    BackendWindowLogicalSize size;
    if (!SDL_GetWindowSize(ToSdlWindow(window), &size.width, &size.height))
    {
        ThrowWindowBackendFailure("SDL_GetWindowSize", context);
    }
    return size;
}

BackendWindowPixelSize SdlWindowBackend::GetSizeInPixels(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    BackendWindowPixelSize size;
    if (!SDL_GetWindowSizeInPixels(ToSdlWindow(window), &size.width, &size.height))
    {
        ThrowWindowBackendFailure("SDL_GetWindowSizeInPixels", context);
    }
    return size;
}

void SdlWindowBackend::SetSize(BackendWindowHandle window, BackendWindowLogicalSize size)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_SetWindowSize(ToSdlWindow(window), size.width, size.height), "SDL_SetWindowSize", context);
}

void SdlWindowBackend::SetMinimumSize(BackendWindowHandle window, BackendWindowLogicalSize size)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_SetWindowMinimumSize(ToSdlWindow(window), size.width, size.height), "SDL_SetWindowMinimumSize", context);
}

void SdlWindowBackend::Show(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_ShowWindow(ToSdlWindow(window)), "SDL_ShowWindow", context);
}

void SdlWindowBackend::Hide(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_HideWindow(ToSdlWindow(window)), "SDL_HideWindow", context);
}

BackendWindowProperties SdlWindowBackend::GetProperties(BackendWindowHandle window)
{
    const SDL_WindowFlags flags = SDL_GetWindowFlags(ToSdlWindow(window));
    return BackendWindowProperties{.desktopFullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0,
                                   .hidden = (flags & SDL_WINDOW_HIDDEN) != 0,
                                   .borderless = (flags & SDL_WINDOW_BORDERLESS) != 0,
                                   .resizable = (flags & SDL_WINDOW_RESIZABLE) != 0,
                                   .minimized = (flags & SDL_WINDOW_MINIMIZED) != 0,
                                   .maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0,
                                   .inputFocus = (flags & SDL_WINDOW_INPUT_FOCUS) != 0,
                                   .alwaysOnTop = (flags & SDL_WINDOW_ALWAYS_ON_TOP) != 0};
}

void SdlWindowBackend::SetFullscreenModeToDesktop(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_SetWindowFullscreenMode(ToSdlWindow(window), nullptr), "SDL_SetWindowFullscreenMode", context);
}

void SdlWindowBackend::SetFullscreen(BackendWindowHandle window, bool fullscreen)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_SetWindowFullscreen(ToSdlWindow(window), fullscreen), "SDL_SetWindowFullscreen", context);
}

void SdlWindowBackend::SetBordered(BackendWindowHandle window, bool bordered)
{
    SDL_Window* const sdlWindow = ToSdlWindow(window);
    const bool currentlyBordered = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_BORDERLESS) == 0;
    if (currentlyBordered == bordered)
    {
        return;
    }

    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_SetWindowBordered(sdlWindow, bordered), "SDL_SetWindowBordered", context);

    const bool nowBordered = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_BORDERLESS) == 0;
    if (nowBordered != bordered)
    {
        ThrowUnsupportedWindowOperation(window, "SDL_SetWindowBordered");
    }
}

void SdlWindowBackend::SetResizable(BackendWindowHandle window, bool resizable)
{
    SDL_Window* const sdlWindow = ToSdlWindow(window);
    const bool currentlyResizable = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_RESIZABLE) != 0;
    if (currentlyResizable == resizable)
    {
        return;
    }

    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_SetWindowResizable(sdlWindow, resizable), "SDL_SetWindowResizable", context);

    const bool nowResizable = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_RESIZABLE) != 0;
    if (nowResizable != resizable)
    {
        ThrowUnsupportedWindowOperation(window, "SDL_SetWindowResizable");
    }
}

void SdlWindowBackend::SetAlwaysOnTop(BackendWindowHandle window, bool alwaysOnTop)
{
    SDL_Window* const sdlWindow = ToSdlWindow(window);
    const bool currentlyAlwaysOnTop = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_ALWAYS_ON_TOP) != 0;
    if (currentlyAlwaysOnTop == alwaysOnTop)
    {
        return;
    }

    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_SetWindowAlwaysOnTop(sdlWindow, alwaysOnTop), "SDL_SetWindowAlwaysOnTop", context);

    const bool nowAlwaysOnTop = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_ALWAYS_ON_TOP) != 0;
    if (nowAlwaysOnTop != alwaysOnTop)
    {
        ThrowUnsupportedWindowOperation(window, "SDL_SetWindowAlwaysOnTop");
    }
}

void SdlWindowBackend::Minimize(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_MinimizeWindow(ToSdlWindow(window)), "SDL_MinimizeWindow", context);
}

void SdlWindowBackend::Maximize(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_MaximizeWindow(ToSdlWindow(window)), "SDL_MaximizeWindow", context);
}

void SdlWindowBackend::Restore(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_RestoreWindow(ToSdlWindow(window)), "SDL_RestoreWindow", context);
}

void SdlWindowBackend::StartTextInput(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_StartTextInput(ToSdlWindow(window)), "SDL_StartTextInput", context);
}

void SdlWindowBackend::StopTextInput(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_StopTextInput(ToSdlWindow(window)), "SDL_StopTextInput", context);
}

bool SdlWindowBackend::IsTextInputActive(BackendWindowHandle window) noexcept
{
    return SDL_TextInputActive(ToSdlWindow(window));
}

void SdlWindowBackend::ClearTextComposition(BackendWindowHandle window)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_ClearComposition(ToSdlWindow(window)), "SDL_ClearComposition", context);
}

void SdlWindowBackend::SetTextInputArea(BackendWindowHandle window, std::optional<BackendTextInputArea> area)
{
    SDL_Window* const sdlWindow = ToSdlWindow(window);
    const std::string context = GetBackendWindowContext(window);
    if (!area.has_value())
    {
        ThrowIfWindowOperationFailed(SDL_SetTextInputArea(sdlWindow, nullptr, 0), "SDL_SetTextInputArea", context);
        return;
    }

    const SDL_Rect rectangle{area->x, area->y, area->width, area->height};
    ThrowIfWindowOperationFailed(SDL_SetTextInputArea(sdlWindow, &rectangle, area->cursorOffset), "SDL_SetTextInputArea", context);
}

void SdlWindowBackend::SetMouseGrab(BackendWindowHandle window, bool grabbed)
{
    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_SetWindowMouseGrab(ToSdlWindow(window), grabbed), "SDL_SetWindowMouseGrab", context);
}

bool SdlWindowBackend::IsMouseGrabbed(BackendWindowHandle window) noexcept
{
    return SDL_GetWindowMouseGrab(ToSdlWindow(window));
}

void SdlWindowBackend::SetRelativeMouseMode(BackendWindowHandle window, bool enabled)
{
    SDL_Window* const sdlWindow = ToSdlWindow(window);
    if (SDL_GetWindowRelativeMouseMode(sdlWindow) == enabled)
    {
        return;
    }

    const std::string context = GetBackendWindowContext(window);
    ThrowIfWindowOperationFailed(SDL_SetWindowRelativeMouseMode(sdlWindow, enabled), "SDL_SetWindowRelativeMouseMode", context);

    if (SDL_GetWindowRelativeMouseMode(sdlWindow) != enabled)
    {
        ThrowUnsupportedWindowOperation(window, "SDL_SetWindowRelativeMouseMode");
    }
}

bool SdlWindowBackend::IsRelativeMouseModeEnabled(BackendWindowHandle window) noexcept
{
    return SDL_GetWindowRelativeMouseMode(ToSdlWindow(window));
}

ponder::core::Result<NativeWindowHandle> SdlWindowBackend::GetNativeHandle(BackendWindowHandle window)
{
    const char* const currentDriver = SDL_GetCurrentVideoDriver();
    if (currentDriver == nullptr)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL current video driver is unavailable. Context: {}.",
                                 GetBackendWindowContext(window));
    }

    const BackendNativeWindowDriver driver = GetNativeWindowDriver(currentDriver);
    if (driver == BackendNativeWindowDriver::Unsupported)
    {
        return MakeUnsupportedNativeHandleError(window, "Native window handles are unsupported by this SDL video driver.");
    }

    const std::string context = GetBackendWindowContext(window);
    const SDL_PropertiesID properties = SDL_GetWindowProperties(ToSdlWindow(window));
    if (properties == 0)
    {
        ThrowWindowBackendFailure("SDL_GetWindowProperties", context);
    }

    switch (driver)
    {
    case BackendNativeWindowDriver::Win32:
        return GetWin32NativeWindowHandle(properties, window);
    case BackendNativeWindowDriver::X11:
        return GetX11NativeWindowHandle(properties, window);
    case BackendNativeWindowDriver::Wayland:
        return GetWaylandNativeWindowHandle(properties, window);
    case BackendNativeWindowDriver::Unsupported:
        break;
    }

    return MakeUnsupportedNativeHandleError(window, "Native window handles are unsupported by this SDL video driver.");
}
} // namespace ponder::platform::detail
