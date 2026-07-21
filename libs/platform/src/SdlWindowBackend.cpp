#include "SdlWindowBackend.hpp"

#include <ponder/core/Assert.hpp>
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
#include <string>
#include <string_view>
#include <utility>

#include "PlatformRuntimeBackend.hpp"
#include "SdlCommon.hpp"
#include "SdlError.hpp"

namespace pond::platform::detail
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
constexpr core::ErrorCode kBackendFailureCode =
    ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr core::ErrorCode kUnsupportedCode = ToErrorCode(PlatformErrorCode::Unsupported);

[[nodiscard]] core::VoidResult CaptureWindowFailure(BackendWindowHandle window,
                                                     std::string_view operation)
{
    return core::VoidResult::FromError(
        CaptureSdlFailure(kBackendFailureCode, operation, GetBackendWindowContext(window)));
}

[[nodiscard]] core::VoidResult ToWindowResult(bool succeeded, BackendWindowHandle window,
                                               std::string_view operation)
{
    if (!succeeded)
    {
        return CaptureWindowFailure(window, operation);
    }

    return core::VoidResult::Success();
}

[[nodiscard]] core::VoidResult MakeUnsupportedWindowOperation(
    BackendWindowHandle window, std::string_view operation)
{
    return core::VoidResult::FromError(core::Error{
        kUnsupportedCode,
        std::format("{} did not apply the requested state for {}; the active video driver may "
                    "not support this operation.",
                    operation, GetBackendWindowContext(window))});
}

[[nodiscard]] core::Result<NativeWindowHandle> MakeNativeHandleError(
    core::ErrorCode code, BackendWindowHandle window, std::string_view message)
{
    return core::Result<NativeWindowHandle>::FromError(core::Error{
        code, std::format("{} Context: {}.", message, GetBackendWindowContext(window))});
}

[[nodiscard]] core::Result<NativeWindowHandle> GetWin32NativeWindowHandle(
    SDL_PropertiesID properties, BackendWindowHandle backendWindow)
{
    void* const instance =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
    void* const window =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if (instance == nullptr || window == nullptr)
    {
        return MakeNativeHandleError(kBackendFailureCode, backendWindow,
                                     "SDL window is missing Win32 native properties.");
    }

    return NativeWin32Window{.instance = instance, .window = window};
}

[[nodiscard]] core::Result<NativeWindowHandle> GetX11NativeWindowHandle(
    SDL_PropertiesID properties, BackendWindowHandle backendWindow)
{
    void* const display =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    const Sint64 window =
        SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (display == nullptr || window <= 0)
    {
        return MakeNativeHandleError(kBackendFailureCode, backendWindow,
                                     "SDL window is missing X11 native properties.");
    }
    if (!std::in_range<std::uintptr_t>(window))
    {
        return MakeNativeHandleError(kBackendFailureCode, backendWindow,
                                     "SDL X11 window property is too large for uintptr_t.");
    }

    return NativeX11Window{.display = display, .window = static_cast<std::uintptr_t>(window)};
}

[[nodiscard]] core::Result<NativeWindowHandle> GetWaylandNativeWindowHandle(
    SDL_PropertiesID properties, BackendWindowHandle backendWindow)
{
    void* const display =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    void* const surface =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    if (display == nullptr || surface == nullptr)
    {
        return MakeNativeHandleError(kBackendFailureCode, backendWindow,
                                     "SDL window is missing Wayland native properties.");
    }

    return NativeWaylandWindow{.display = display, .surface = surface};
}
} // namespace

core::Result<BackendWindowHandle> SdlWindowBackend::Create(
    const BackendWindowCreateDesc& desc)
{
    const std::string title{desc.title};
    SDL_Window* const window =
        SDL_CreateWindow(title.c_str(), desc.logicalSize.width, desc.logicalSize.height,
                         static_cast<SDL_WindowFlags>(BuildSdlWindowFlags(desc)));
    if (window == nullptr)
    {
        return core::Result<BackendWindowHandle>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_CreateWindow", "window"));
    }

    return ToBackendWindowHandle(window);
}

void SdlWindowBackend::Destroy(BackendWindowHandle window) noexcept
{
    SDL_DestroyWindow(ToSdlWindow(window));
}

core::Result<std::uint32_t> SdlWindowBackend::GetId(BackendWindowHandle window)
{
    const SDL_WindowID id = SDL_GetWindowID(ToSdlWindow(window));
    if (id == 0)
    {
        return core::Result<std::uint32_t>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetWindowID", GetBackendWindowContext(window)));
    }

    return id;
}

std::string SdlWindowBackend::GetTitle(BackendWindowHandle window)
{
    const char* const title = SDL_GetWindowTitle(ToSdlWindow(window));
    PONDER_VERIFY(title != nullptr, "SDL returned a null window title");
    return std::string{title};
}

core::VoidResult SdlWindowBackend::SetTitle(BackendWindowHandle window,
                                            std::string_view title)
{
    const std::string ownedTitle{title};
    return ToWindowResult(SDL_SetWindowTitle(ToSdlWindow(window), ownedTitle.c_str()), window,
                          "SDL_SetWindowTitle");
}

core::Result<BackendWindowPosition> SdlWindowBackend::GetPosition(
    BackendWindowHandle window)
{
    BackendWindowPosition position;
    if (!SDL_GetWindowPosition(ToSdlWindow(window), &position.x, &position.y))
    {
        return core::Result<BackendWindowPosition>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetWindowPosition", GetBackendWindowContext(window)));
    }
    return position;
}

core::VoidResult SdlWindowBackend::SetPosition(BackendWindowHandle window,
                                               BackendWindowPosition position)
{
    return ToWindowResult(SDL_SetWindowPosition(ToSdlWindow(window), position.x, position.y),
                          window, "SDL_SetWindowPosition");
}

core::Result<BackendWindowLogicalSize> SdlWindowBackend::GetSize(
    BackendWindowHandle window)
{
    BackendWindowLogicalSize size;
    if (!SDL_GetWindowSize(ToSdlWindow(window), &size.width, &size.height))
    {
        return core::Result<BackendWindowLogicalSize>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetWindowSize", GetBackendWindowContext(window)));
    }
    return size;
}

core::Result<BackendWindowPixelSize> SdlWindowBackend::GetSizeInPixels(
    BackendWindowHandle window)
{
    BackendWindowPixelSize size;
    if (!SDL_GetWindowSizeInPixels(ToSdlWindow(window), &size.width, &size.height))
    {
        return core::Result<BackendWindowPixelSize>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetWindowSizeInPixels", GetBackendWindowContext(window)));
    }
    return size;
}

core::VoidResult SdlWindowBackend::SetSize(BackendWindowHandle window,
                                           BackendWindowLogicalSize size)
{
    return ToWindowResult(SDL_SetWindowSize(ToSdlWindow(window), size.width, size.height),
                          window, "SDL_SetWindowSize");
}

core::VoidResult SdlWindowBackend::SetMinimumSize(BackendWindowHandle window,
                                                  BackendWindowLogicalSize size)
{
    return ToWindowResult(
        SDL_SetWindowMinimumSize(ToSdlWindow(window), size.width, size.height), window,
        "SDL_SetWindowMinimumSize");
}

core::VoidResult SdlWindowBackend::Show(BackendWindowHandle window)
{
    return ToWindowResult(SDL_ShowWindow(ToSdlWindow(window)), window, "SDL_ShowWindow");
}

core::VoidResult SdlWindowBackend::Hide(BackendWindowHandle window)
{
    return ToWindowResult(SDL_HideWindow(ToSdlWindow(window)), window, "SDL_HideWindow");
}

core::Result<BackendWindowProperties> SdlWindowBackend::GetProperties(
    BackendWindowHandle window)
{
    const SDL_WindowFlags flags = SDL_GetWindowFlags(ToSdlWindow(window));
    return BackendWindowProperties{
        .desktopFullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0,
        .hidden = (flags & SDL_WINDOW_HIDDEN) != 0,
        .borderless = (flags & SDL_WINDOW_BORDERLESS) != 0,
        .resizable = (flags & SDL_WINDOW_RESIZABLE) != 0,
        .minimized = (flags & SDL_WINDOW_MINIMIZED) != 0,
        .maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0,
        .inputFocus = (flags & SDL_WINDOW_INPUT_FOCUS) != 0,
        .alwaysOnTop = (flags & SDL_WINDOW_ALWAYS_ON_TOP) != 0};
}

core::VoidResult SdlWindowBackend::SetFullscreenModeToDesktop(
    BackendWindowHandle window)
{
    return ToWindowResult(SDL_SetWindowFullscreenMode(ToSdlWindow(window), nullptr), window,
                          "SDL_SetWindowFullscreenMode");
}

core::VoidResult SdlWindowBackend::SetFullscreen(BackendWindowHandle window, bool fullscreen)
{
    return ToWindowResult(SDL_SetWindowFullscreen(ToSdlWindow(window), fullscreen), window,
                          "SDL_SetWindowFullscreen");
}

core::VoidResult SdlWindowBackend::SetBordered(BackendWindowHandle window, bool bordered)
{
    SDL_Window* const sdlWindow = ToSdlWindow(window);
    const bool currentlyBordered =
        (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_BORDERLESS) == 0;
    if (currentlyBordered == bordered)
    {
        return core::VoidResult::Success();
    }

    RETURN_ERROR_IF_FAILED(ToWindowResult(SDL_SetWindowBordered(sdlWindow, bordered), window,
                                          "SDL_SetWindowBordered"));

    const bool nowBordered = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_BORDERLESS) == 0;
    return nowBordered == bordered
               ? core::VoidResult::Success()
               : MakeUnsupportedWindowOperation(window, "SDL_SetWindowBordered");
}

core::VoidResult SdlWindowBackend::SetResizable(BackendWindowHandle window, bool resizable)
{
    SDL_Window* const sdlWindow = ToSdlWindow(window);
    const bool currentlyResizable =
        (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_RESIZABLE) != 0;
    if (currentlyResizable == resizable)
    {
        return core::VoidResult::Success();
    }

    RETURN_ERROR_IF_FAILED(ToWindowResult(SDL_SetWindowResizable(sdlWindow, resizable), window,
                                          "SDL_SetWindowResizable"));

    const bool nowResizable = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_RESIZABLE) != 0;
    return nowResizable == resizable
               ? core::VoidResult::Success()
               : MakeUnsupportedWindowOperation(window, "SDL_SetWindowResizable");
}

core::VoidResult SdlWindowBackend::SetAlwaysOnTop(BackendWindowHandle window,
                                                  bool alwaysOnTop)
{
    SDL_Window* const sdlWindow = ToSdlWindow(window);
    const bool currentlyAlwaysOnTop =
        (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_ALWAYS_ON_TOP) != 0;
    if (currentlyAlwaysOnTop == alwaysOnTop)
    {
        return core::VoidResult::Success();
    }

    RETURN_ERROR_IF_FAILED(ToWindowResult(
        SDL_SetWindowAlwaysOnTop(sdlWindow, alwaysOnTop), window,
        "SDL_SetWindowAlwaysOnTop"));

    const bool nowAlwaysOnTop =
        (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_ALWAYS_ON_TOP) != 0;
    return nowAlwaysOnTop == alwaysOnTop
               ? core::VoidResult::Success()
               : MakeUnsupportedWindowOperation(window, "SDL_SetWindowAlwaysOnTop");
}

core::VoidResult SdlWindowBackend::Minimize(BackendWindowHandle window)
{
    return ToWindowResult(SDL_MinimizeWindow(ToSdlWindow(window)), window,
                          "SDL_MinimizeWindow");
}

core::VoidResult SdlWindowBackend::Maximize(BackendWindowHandle window)
{
    return ToWindowResult(SDL_MaximizeWindow(ToSdlWindow(window)), window,
                          "SDL_MaximizeWindow");
}

core::VoidResult SdlWindowBackend::Restore(BackendWindowHandle window)
{
    return ToWindowResult(SDL_RestoreWindow(ToSdlWindow(window)), window,
                          "SDL_RestoreWindow");
}

core::VoidResult SdlWindowBackend::StartTextInput(BackendWindowHandle window)
{
    return ToWindowResult(SDL_StartTextInput(ToSdlWindow(window)), window,
                          "SDL_StartTextInput");
}

core::VoidResult SdlWindowBackend::StopTextInput(BackendWindowHandle window)
{
    return ToWindowResult(SDL_StopTextInput(ToSdlWindow(window)), window,
                          "SDL_StopTextInput");
}

bool SdlWindowBackend::IsTextInputActive(BackendWindowHandle window)
{
    return SDL_TextInputActive(ToSdlWindow(window));
}

core::VoidResult SdlWindowBackend::ClearTextComposition(BackendWindowHandle window)
{
    return ToWindowResult(SDL_ClearComposition(ToSdlWindow(window)), window,
                          "SDL_ClearComposition");
}

core::VoidResult SdlWindowBackend::SetTextInputArea(
    BackendWindowHandle window, std::optional<BackendTextInputArea> area)
{
    SDL_Window* const sdlWindow = ToSdlWindow(window);
    if (!area.has_value())
    {
        return ToWindowResult(SDL_SetTextInputArea(sdlWindow, nullptr, 0), window,
                              "SDL_SetTextInputArea");
    }

    const SDL_Rect rectangle{area->x, area->y, area->width, area->height};
    return ToWindowResult(
        SDL_SetTextInputArea(sdlWindow, &rectangle, area->cursorOffset), window,
        "SDL_SetTextInputArea");
}

core::VoidResult SdlWindowBackend::SetMouseGrab(BackendWindowHandle window, bool grabbed)
{
    return ToWindowResult(SDL_SetWindowMouseGrab(ToSdlWindow(window), grabbed), window,
                          "SDL_SetWindowMouseGrab");
}

bool SdlWindowBackend::IsMouseGrabbed(BackendWindowHandle window)
{
    return SDL_GetWindowMouseGrab(ToSdlWindow(window));
}

core::VoidResult SdlWindowBackend::SetRelativeMouseMode(BackendWindowHandle window,
                                                        bool enabled)
{
    SDL_Window* const sdlWindow = ToSdlWindow(window);
    if (SDL_GetWindowRelativeMouseMode(sdlWindow) == enabled)
    {
        return core::VoidResult::Success();
    }

    RETURN_ERROR_IF_FAILED(ToWindowResult(
        SDL_SetWindowRelativeMouseMode(sdlWindow, enabled), window,
        "SDL_SetWindowRelativeMouseMode"));

    return SDL_GetWindowRelativeMouseMode(sdlWindow) == enabled
               ? core::VoidResult::Success()
               : MakeUnsupportedWindowOperation(window, "SDL_SetWindowRelativeMouseMode");
}

bool SdlWindowBackend::IsRelativeMouseModeEnabled(BackendWindowHandle window)
{
    return SDL_GetWindowRelativeMouseMode(ToSdlWindow(window));
}

core::Result<NativeWindowHandle> SdlWindowBackend::GetNativeHandle(
    BackendWindowHandle window)
{
    const char* const currentDriver = SDL_GetCurrentVideoDriver();
    if (currentDriver == nullptr)
    {
        return MakeNativeHandleError(kBackendFailureCode, window,
                                     "SDL current video driver is unavailable.");
    }

    const BackendNativeWindowDriver driver = GetNativeWindowDriver(currentDriver);
    if (driver == BackendNativeWindowDriver::Unsupported)
    {
        return MakeNativeHandleError(
            kUnsupportedCode, window,
            "Native window handles are unsupported by this SDL video driver.");
    }

    const SDL_PropertiesID properties = SDL_GetWindowProperties(ToSdlWindow(window));
    if (properties == 0)
    {
        return core::Result<NativeWindowHandle>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetWindowProperties", GetBackendWindowContext(window)));
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

    return MakeNativeHandleError(
        kUnsupportedCode, window,
        "Native window handles are unsupported by this SDL video driver.");
}
} // namespace pond::platform::detail
