#include "PlatformRuntimeBackend.hpp"

#include <ponder/core/ScopeExit.hpp>

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_metal.h>
#include <SDL3/SDL_misc.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_platform_defines.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pond::platform::detail
{
bool IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility compatibility) noexcept
{
    switch (compatibility)
    {
    case WindowGraphicsCompatibility::Default:
        return true;
    case WindowGraphicsCompatibility::Vulkan:
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX) || defined(SDL_PLATFORM_MACOS)
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
    if (driverName == "cocoa")
    {
        return BackendNativeWindowDriver::Cocoa;
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
    return SDL_WINDOWPOS_ISUNDEFINED(backendValue) || SDL_WINDOWPOS_ISCENTERED(backendValue);
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

[[nodiscard]] constexpr BackendNativeWindowHandleResult NativeHandleSucceeded() noexcept
{
    return BackendNativeWindowHandleResult{.status = BackendNativeWindowHandleStatus::Succeeded};
}

[[nodiscard]] constexpr BackendNativeWindowHandleResult NativeHandleUnsupported(
    const char* message) noexcept
{
    return BackendNativeWindowHandleResult{.status = BackendNativeWindowHandleStatus::Unsupported,
                                           .message = message};
}

[[nodiscard]] constexpr BackendNativeWindowHandleResult NativeHandleFailure(
    const char* message) noexcept
{
    return BackendNativeWindowHandleResult{.status = BackendNativeWindowHandleStatus::Failed,
                                           .message = message};
}

[[nodiscard]] constexpr BackendNativeWindowHandleResult NativeHandleSdlFailure(
    const char* operation) noexcept
{
    return BackendNativeWindowHandleResult{.status = BackendNativeWindowHandleStatus::Failed,
                                           .operation = operation,
                                           .captureSdlError = true};
}

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

[[nodiscard]] const char* GetMetadataPropertyName(ApplicationMetadataProperty property) noexcept
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

[[nodiscard]] const char* GetAppMetadataProperty(void*,
                                                 ApplicationMetadataProperty property) noexcept
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

[[nodiscard]] bool PollEvent(void*, SDL_Event* event) noexcept
{
    return SDL_PollEvent(event);
}

[[nodiscard]] bool SupportsGlobalMouse(void*) noexcept
{
    const char* const driver = SDL_GetCurrentVideoDriver();
    if (driver == nullptr)
    {
        return false;
    }

    const std::string_view name{driver};
    return name == "windows" || name == "cocoa" || name == "x11";
}

void GetGlobalMousePosition(void*, float* x, float* y) noexcept
{
    static_cast<void>(SDL_GetGlobalMouseState(x, y));
}

[[nodiscard]] bool SetMouseCapture(void*, bool enabled) noexcept
{
    return SDL_CaptureMouse(enabled);
}

[[nodiscard]] std::optional<SDL_SystemCursor> ToSdlSystemCursor(SystemCursorShape shape) noexcept
{
    switch (shape)
    {
    case SystemCursorShape::Default:
        return SDL_SYSTEM_CURSOR_DEFAULT;
    case SystemCursorShape::TextInput:
        return SDL_SYSTEM_CURSOR_TEXT;
    case SystemCursorShape::Move:
        return SDL_SYSTEM_CURSOR_MOVE;
    case SystemCursorShape::ResizeNorthSouth:
        return SDL_SYSTEM_CURSOR_NS_RESIZE;
    case SystemCursorShape::ResizeEastWest:
        return SDL_SYSTEM_CURSOR_EW_RESIZE;
    case SystemCursorShape::ResizeNortheastSouthwest:
        return SDL_SYSTEM_CURSOR_NESW_RESIZE;
    case SystemCursorShape::ResizeNorthwestSoutheast:
        return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
    case SystemCursorShape::Pointer:
        return SDL_SYSTEM_CURSOR_POINTER;
    case SystemCursorShape::Wait:
        return SDL_SYSTEM_CURSOR_WAIT;
    case SystemCursorShape::Progress:
        return SDL_SYSTEM_CURSOR_PROGRESS;
    case SystemCursorShape::NotAllowed:
        return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    }

    return std::nullopt;
}

[[nodiscard]] void* CreateSystemCursor(void*, SystemCursorShape shape) noexcept
{
    const std::optional<SDL_SystemCursor> cursor = ToSdlSystemCursor(shape);
    return cursor.has_value() ? SDL_CreateSystemCursor(*cursor) : nullptr;
}

[[nodiscard]] bool SetCursor(void*, void* cursor) noexcept
{
    return SDL_SetCursor(static_cast<SDL_Cursor*>(cursor));
}

void DestroyCursor(void*, void* cursor) noexcept
{
    SDL_DestroyCursor(static_cast<SDL_Cursor*>(cursor));
}

[[nodiscard]] bool ShowCursor(void*) noexcept
{
    return SDL_ShowCursor();
}

[[nodiscard]] bool HideCursor(void*) noexcept
{
    return SDL_HideCursor();
}

[[nodiscard]] bool IsCursorVisible(void*) noexcept
{
    return SDL_CursorVisible();
}

[[nodiscard]] bool SupportsClipboardText(void*) noexcept
{
    return true;
}

[[nodiscard]] BackendClipboardTextResult GetClipboardText(void*)
{
    static_cast<void>(SDL_ClearError());
    char* const text = SDL_GetClipboardText();
    auto freeTextOnFailure = core::MakeScopeExit(
        [text]() noexcept
        {
            SDL_free(text);
        });
    const char* const rawError = SDL_GetError();
    BackendClipboardTextResult result{
        .text = text, .errorText = rawError != nullptr ? std::string{rawError} : std::string{}};
    freeTextOnFailure.Dismiss();
    return result;
}

void FreeClipboardText(void*, char* text) noexcept
{
    SDL_free(text);
}

[[nodiscard]] bool SetClipboardText(void*, const char* text) noexcept
{
    return SDL_SetClipboardText(text);
}

[[nodiscard]] bool OpenExternalUri(void*, const char* uri) noexcept
{
    return SDL_OpenURL(uri);
}

void ShowDialog(void*, const BackendDialogRequestDesc& desc) noexcept
{
    auto* const parentWindow = static_cast<SDL_Window*>(desc.parentWindow);
    switch (desc.kind)
    {
    case BackendDialogKind::OpenFile:
        SDL_ShowOpenFileDialog(desc.callback, desc.userdata, parentWindow, desc.filters,
                               desc.filterCount, desc.defaultLocation, desc.allowMultipleSelection);
        break;
    case BackendDialogKind::SaveFile:
        SDL_ShowSaveFileDialog(desc.callback, desc.userdata, parentWindow, desc.filters,
                               desc.filterCount, desc.defaultLocation);
        break;
    case BackendDialogKind::OpenFolder:
        SDL_ShowOpenFolderDialog(desc.callback, desc.userdata, parentWindow, desc.defaultLocation,
                                 desc.allowMultipleSelection);
        break;
    }
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

[[nodiscard]] bool GetWindowSizeInPixels(void*, void* window, int* width, int* height) noexcept
{
    return SDL_GetWindowSizeInPixels(static_cast<SDL_Window*>(window), width, height);
}

[[nodiscard]] bool SetWindowSize(void*, void* window, int width, int height) noexcept
{
    return SDL_SetWindowSize(static_cast<SDL_Window*>(window), width, height);
}

[[nodiscard]] bool SetWindowMinimumSize(void*, void* window, int width, int height) noexcept
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

[[nodiscard]] bool GetWindowProperties(void*, void* window,
                                       BackendWindowProperties* properties) noexcept
{
    const SDL_WindowFlags flags = SDL_GetWindowFlags(static_cast<SDL_Window*>(window));
    *properties = BackendWindowProperties{
        (flags & SDL_WINDOW_FULLSCREEN) != 0,  (flags & SDL_WINDOW_HIDDEN) != 0,
        (flags & SDL_WINDOW_BORDERLESS) != 0,  (flags & SDL_WINDOW_RESIZABLE) != 0,
        (flags & SDL_WINDOW_MINIMIZED) != 0,   (flags & SDL_WINDOW_MAXIMIZED) != 0,
        (flags & SDL_WINDOW_INPUT_FOCUS) != 0, (flags & SDL_WINDOW_ALWAYS_ON_TOP) != 0};
    return true;
}

[[nodiscard]] BackendWindowOperationResult SetFullscreenModeToDesktop(void*, void* window) noexcept
{
    return SDL_SetWindowFullscreenMode(static_cast<SDL_Window*>(window), nullptr)
               ? BackendWindowOperationResult::Succeeded
               : BackendWindowOperationResult::Failed;
}

[[nodiscard]] BackendWindowOperationResult SetWindowFullscreen(void*, void* window,
                                                               bool fullscreen) noexcept
{
    return SDL_SetWindowFullscreen(static_cast<SDL_Window*>(window), fullscreen)
               ? BackendWindowOperationResult::Succeeded
               : BackendWindowOperationResult::Failed;
}

[[nodiscard]] BackendWindowOperationResult SetWindowBordered(void*, void* window,
                                                             bool bordered) noexcept
{
    SDL_Window* const sdlWindow = static_cast<SDL_Window*>(window);
    const bool currentlyBordered = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_BORDERLESS) == 0;
    if (currentlyBordered == bordered)
    {
        return BackendWindowOperationResult::Succeeded;
    }

    if (!SDL_SetWindowBordered(sdlWindow, bordered))
    {
        return BackendWindowOperationResult::Failed;
    }

    const bool nowBordered = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_BORDERLESS) == 0;
    return nowBordered == bordered ? BackendWindowOperationResult::Succeeded
                                   : BackendWindowOperationResult::Unsupported;
}

[[nodiscard]] BackendWindowOperationResult SetWindowResizable(void*, void* window,
                                                              bool resizable) noexcept
{
    SDL_Window* const sdlWindow = static_cast<SDL_Window*>(window);
    const bool currentlyResizable = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_RESIZABLE) != 0;
    if (currentlyResizable == resizable)
    {
        return BackendWindowOperationResult::Succeeded;
    }

    if (!SDL_SetWindowResizable(sdlWindow, resizable))
    {
        return BackendWindowOperationResult::Failed;
    }

    const bool nowResizable = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_RESIZABLE) != 0;
    return nowResizable == resizable ? BackendWindowOperationResult::Succeeded
                                     : BackendWindowOperationResult::Unsupported;
}

[[nodiscard]] BackendWindowOperationResult SetWindowAlwaysOnTop(void*, void* window,
                                                                bool alwaysOnTop) noexcept
{
    SDL_Window* const sdlWindow = static_cast<SDL_Window*>(window);
    const bool currentlyAlwaysOnTop =
        (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_ALWAYS_ON_TOP) != 0;
    if (currentlyAlwaysOnTop == alwaysOnTop)
    {
        return BackendWindowOperationResult::Succeeded;
    }

    if (!SDL_SetWindowAlwaysOnTop(sdlWindow, alwaysOnTop))
    {
        return BackendWindowOperationResult::Failed;
    }

    const bool nowAlwaysOnTop = (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_ALWAYS_ON_TOP) != 0;
    return nowAlwaysOnTop == alwaysOnTop ? BackendWindowOperationResult::Succeeded
                                         : BackendWindowOperationResult::Unsupported;
}

[[nodiscard]] BackendWindowOperationResult MinimizeWindow(void*, void* window) noexcept
{
    return SDL_MinimizeWindow(static_cast<SDL_Window*>(window))
               ? BackendWindowOperationResult::Succeeded
               : BackendWindowOperationResult::Failed;
}

[[nodiscard]] BackendWindowOperationResult MaximizeWindow(void*, void* window) noexcept
{
    return SDL_MaximizeWindow(static_cast<SDL_Window*>(window))
               ? BackendWindowOperationResult::Succeeded
               : BackendWindowOperationResult::Failed;
}

[[nodiscard]] BackendWindowOperationResult RestoreWindow(void*, void* window) noexcept
{
    return SDL_RestoreWindow(static_cast<SDL_Window*>(window))
               ? BackendWindowOperationResult::Succeeded
               : BackendWindowOperationResult::Failed;
}

[[nodiscard]] bool StartWindowTextInput(void*, void* window) noexcept
{
    return SDL_StartTextInput(static_cast<SDL_Window*>(window));
}

[[nodiscard]] bool StopWindowTextInput(void*, void* window) noexcept
{
    return SDL_StopTextInput(static_cast<SDL_Window*>(window));
}

[[nodiscard]] bool IsWindowTextInputActive(void*, void* window) noexcept
{
    return SDL_TextInputActive(static_cast<SDL_Window*>(window));
}

[[nodiscard]] bool ClearWindowTextComposition(void*, void* window) noexcept
{
    return SDL_ClearComposition(static_cast<SDL_Window*>(window));
}

[[nodiscard]] bool SetWindowTextInputArea(void*, void* window,
                                          const BackendTextInputArea* area) noexcept
{
    SDL_Window* const sdlWindow = static_cast<SDL_Window*>(window);
    if (area == nullptr)
    {
        return SDL_SetTextInputArea(sdlWindow, nullptr, 0);
    }

    const SDL_Rect rectangle{area->x, area->y, area->width, area->height};
    return SDL_SetTextInputArea(sdlWindow, &rectangle, area->cursorOffset);
}

[[nodiscard]] bool SetWindowMouseGrab(void*, void* window, bool grabbed) noexcept
{
    return SDL_SetWindowMouseGrab(static_cast<SDL_Window*>(window), grabbed);
}

[[nodiscard]] bool IsWindowMouseGrabbed(void*, void* window) noexcept
{
    return SDL_GetWindowMouseGrab(static_cast<SDL_Window*>(window));
}

[[nodiscard]] bool SetWindowRelativeMouseMode(void*, void* window, bool enabled) noexcept
{
    return SDL_SetWindowRelativeMouseMode(static_cast<SDL_Window*>(window), enabled);
}

[[nodiscard]] bool IsWindowRelativeMouseModeEnabled(void*, void* window) noexcept
{
    return SDL_GetWindowRelativeMouseMode(static_cast<SDL_Window*>(window));
}

[[nodiscard]] BackendNativeWindowHandleResult GetWin32NativeWindowHandle(
    SDL_PropertiesID properties, NativeWindowHandle* handle) noexcept
{
    void* const instance =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
    void* const window =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if (instance == nullptr || window == nullptr)
    {
        return NativeHandleFailure("SDL window is missing Win32 native properties.");
    }

    *handle = NativeWin32Window{.instance = instance, .window = window};
    return NativeHandleSucceeded();
}

[[nodiscard]] BackendNativeWindowHandleResult GetX11NativeWindowHandle(
    SDL_PropertiesID properties, NativeWindowHandle* handle) noexcept
{
    void* const display =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    const Sint64 window = SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (display == nullptr || window <= 0)
    {
        return NativeHandleFailure("SDL window is missing X11 native properties.");
    }
    if (!std::in_range<std::uintptr_t>(window))
    {
        return NativeHandleFailure("SDL X11 window property is too large for uintptr_t.");
    }

    *handle = NativeX11Window{.display = display, .window = static_cast<std::uintptr_t>(window)};
    return NativeHandleSucceeded();
}

[[nodiscard]] BackendNativeWindowHandleResult GetWaylandNativeWindowHandle(
    SDL_PropertiesID properties, NativeWindowHandle* handle) noexcept
{
    void* const display =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
    void* const surface =
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    if (display == nullptr || surface == nullptr)
    {
        return NativeHandleFailure("SDL window is missing Wayland native properties.");
    }

    *handle = NativeWaylandWindow{.display = display, .surface = surface};
    return NativeHandleSucceeded();
}

[[nodiscard]] BackendNativeWindowHandleResult GetCocoaNativeWindowHandle(
    void* window, void** cachedMetalView, NativeWindowHandle* handle) noexcept
{
    if (cachedMetalView == nullptr)
    {
        return NativeHandleFailure("Cocoa Metal view storage is unavailable.");
    }

    SDL_MetalView view = static_cast<SDL_MetalView>(*cachedMetalView);
    if (view == nullptr)
    {
        view = SDL_Metal_CreateView(static_cast<SDL_Window*>(window));
        if (view == nullptr)
        {
            return NativeHandleSdlFailure("SDL_Metal_CreateView");
        }
        *cachedMetalView = view;
    }

    void* const layer = SDL_Metal_GetLayer(view);
    if (layer == nullptr)
    {
        return NativeHandleSdlFailure("SDL_Metal_GetLayer");
    }

    *handle = NativeCocoaWindow{.metalLayer = layer};
    return NativeHandleSucceeded();
}

[[nodiscard]] BackendNativeWindowHandleResult GetNativeWindowHandle(
    void*, void* window, void** cachedMetalView, NativeWindowHandle* handle) noexcept
{
    if (handle == nullptr)
    {
        return NativeHandleFailure("Native window handle output storage is unavailable.");
    }

    const char* const currentDriver = SDL_GetCurrentVideoDriver();
    if (currentDriver == nullptr)
    {
        return NativeHandleFailure("SDL current video driver is unavailable.");
    }

    const BackendNativeWindowDriver driver = GetNativeWindowDriver(currentDriver);
    if (driver == BackendNativeWindowDriver::Cocoa)
    {
        return GetCocoaNativeWindowHandle(window, cachedMetalView, handle);
    }
    if (driver == BackendNativeWindowDriver::Unsupported)
    {
        return NativeHandleUnsupported(
            "Native window handles are unsupported by this SDL video driver.");
    }

    const SDL_PropertiesID properties = SDL_GetWindowProperties(static_cast<SDL_Window*>(window));
    if (properties == 0)
    {
        return NativeHandleSdlFailure("SDL_GetWindowProperties");
    }

    switch (driver)
    {
    case BackendNativeWindowDriver::Win32:
        return GetWin32NativeWindowHandle(properties, handle);
    case BackendNativeWindowDriver::X11:
        return GetX11NativeWindowHandle(properties, handle);
    case BackendNativeWindowDriver::Wayland:
        return GetWaylandNativeWindowHandle(properties, handle);
    case BackendNativeWindowDriver::Cocoa:
    case BackendNativeWindowDriver::Unsupported:
        break;
    }

    return NativeHandleUnsupported(
        "Native window handles are unsupported by this SDL video driver.");
}

void DestroyMetalView(void*, void* metalView) noexcept
{
    SDL_Metal_DestroyView(static_cast<SDL_MetalView>(metalView));
}

[[nodiscard]] bool EnumerateDisplays(void*, std::vector<std::uint32_t>& displayIds)
{
    int count{};
    std::unique_ptr<SDL_DisplayID, SdlDisplayListDeleter> displays{SDL_GetDisplays(&count)};
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

    *bounds = BackendScreenRectangle{rectangle.x, rectangle.y, rectangle.w, rectangle.h};
    return true;
}

[[nodiscard]] bool GetDisplayUsableBounds(void*, std::uint32_t displayId,
                                          BackendScreenRectangle* bounds) noexcept
{
    SDL_Rect rectangle{};
    if (!SDL_GetDisplayUsableBounds(displayId, &rectangle))
    {
        return false;
    }

    *bounds = BackendScreenRectangle{rectangle.x, rectangle.y, rectangle.w, rectangle.h};
    return true;
}

[[nodiscard]] bool GetCurrentDisplayRefreshRate(void*, std::uint32_t displayId,
                                                float* refreshRateHertz) noexcept
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

[[nodiscard]] float GetDisplayContentScale(void*, std::uint32_t displayId) noexcept
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
    .context = nullptr,
    .create = CreateWindow,
    .destroy = DestroyWindow,
    .getId = GetWindowId,
    .getTitle = GetWindowTitle,
    .setTitle = SetWindowTitle,
    .getPosition = GetWindowPosition,
    .setPosition = SetWindowPosition,
    .getSize = GetWindowSize,
    .getSizeInPixels = GetWindowSizeInPixels,
    .setSize = SetWindowSize,
    .setMinimumSize = SetWindowMinimumSize,
    .show = ShowWindow,
    .hide = HideWindow,
    .getProperties = GetWindowProperties,
    .setFullscreenModeToDesktop = SetFullscreenModeToDesktop,
    .setFullscreen = SetWindowFullscreen,
    .setBordered = SetWindowBordered,
    .setResizable = SetWindowResizable,
    .setAlwaysOnTop = SetWindowAlwaysOnTop,
    .minimize = MinimizeWindow,
    .maximize = MaximizeWindow,
    .restore = RestoreWindow,
    .startTextInput = StartWindowTextInput,
    .stopTextInput = StopWindowTextInput,
    .isTextInputActive = IsWindowTextInputActive,
    .clearTextComposition = ClearWindowTextComposition,
    .setTextInputArea = SetWindowTextInputArea,
    .setMouseGrab = SetWindowMouseGrab,
    .isMouseGrabbed = IsWindowMouseGrabbed,
    .setRelativeMouseMode = SetWindowRelativeMouseMode,
    .isRelativeMouseModeEnabled = IsWindowRelativeMouseModeEnabled,
    .getNativeHandle = GetNativeWindowHandle,
    .destroyMetalView = DestroyMetalView,
};

constexpr PlatformDisplayBackend kSdlDisplayBackend{
    .context = nullptr,
    .enumerate = EnumerateDisplays,
    .getName = GetDisplayName,
    .getBounds = GetDisplayBounds,
    .getUsableBounds = GetDisplayUsableBounds,
    .getCurrentRefreshRate = GetCurrentDisplayRefreshRate,
    .getCurrentOrientation = GetCurrentDisplayOrientation,
    .getContentScale = GetDisplayContentScale,
    .getForWindow = GetDisplayForWindow,
    .getWindowPixelDensity = GetWindowPixelDensity,
    .getWindowDisplayScale = GetWindowDisplayScale,
};

constexpr PlatformRuntimeBackend kSdlBackend{
    .context = nullptr,
    .isMainThread = IsMainThread,
    .hasInitializedSubsystems = HasInitializedSubsystems,
    .hasExpectedRuntimeSubsystems = HasExpectedRuntimeSubsystems,
    .getAppMetadataProperty = GetAppMetadataProperty,
    .setAppMetadataProperty = SetAppMetadataProperty,
    .getHint = GetHint,
    .setHintOverride = SetHintOverride,
    .resetHint = ResetHint,
    .initializeVideo = InitializeVideo,
    .quit = Quit,
    .getTicksNanoseconds = GetTicksNanoseconds,
    .pollEvent = PollEvent,
    .supportsGlobalMouse = SupportsGlobalMouse,
    .getGlobalMousePosition = GetGlobalMousePosition,
    .setMouseCapture = SetMouseCapture,
    .createSystemCursor = CreateSystemCursor,
    .setCursor = SetCursor,
    .destroyCursor = DestroyCursor,
    .showCursor = ShowCursor,
    .hideCursor = HideCursor,
    .isCursorVisible = IsCursorVisible,
    .supportsClipboardText = SupportsClipboardText,
    .getClipboardText = GetClipboardText,
    .freeClipboardText = FreeClipboardText,
    .setClipboardText = SetClipboardText,
    .openExternalUri = OpenExternalUri,
    .showDialog = ShowDialog,
};

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
