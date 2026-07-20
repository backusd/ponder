#pragma once

#include <ponder/platform/Mouse.hpp>
#include <ponder/platform/NativeWindow.hpp>
#include <ponder/platform/WindowGraphics.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "IPlatformRuntimeBackend.hpp"

namespace pond::platform::detail
{
struct BackendWindowCreateDesc final
{
    const char* title{};
    int width{};
    int height{};
    bool resizable{};
    bool highPixelDensity{};
    WindowGraphicsCompatibility graphicsCompatibility{WindowGraphicsCompatibility::Default};
};

struct BackendWindowProperties final
{
    bool desktopFullscreen{};
    bool hidden{};
    bool borderless{};
    bool resizable{};
    bool minimized{};
    bool maximized{};
    bool inputFocus{};
    bool alwaysOnTop{};
};

struct BackendTextInputArea final
{
    int x{};
    int y{};
    int width{};
    int height{};
    int cursorOffset{};
};

enum class BackendWindowOperationResult : std::uint8_t
{
    Succeeded,
    Unsupported,
    Failed
};

enum class BackendNativeWindowDriver : std::uint8_t
{
    Unsupported,
    Win32,
    X11,
    Wayland
};

enum class BackendNativeWindowHandleStatus : std::uint8_t
{
    Succeeded,
    Unsupported,
    Failed
};

struct BackendNativeWindowHandleResult final
{
    BackendNativeWindowHandleStatus status{BackendNativeWindowHandleStatus::Failed};
    const char* operation{};
    const char* message{};
    bool captureSdlError{};
};

struct PlatformWindowBackend final
{
    void* context{};
    void* (*create)(void* context, const BackendWindowCreateDesc& desc){};
    void (*destroy)(void* context, void* window){};
    std::uint32_t (*getId)(void* context, void* window){};
    const char* (*getTitle)(void* context, void* window){};
    bool (*setTitle)(void* context, void* window, const char* title){};
    bool (*getPosition)(void* context, void* window, int* x, int* y){};
    bool (*setPosition)(void* context, void* window, int x, int y){};
    bool (*getSize)(void* context, void* window, int* width, int* height){};
    bool (*getSizeInPixels)(void* context, void* window, int* width, int* height){};
    bool (*setSize)(void* context, void* window, int width, int height){};
    bool (*setMinimumSize)(void* context, void* window, int width, int height){};
    bool (*show)(void* context, void* window){};
    bool (*hide)(void* context, void* window){};
    bool (*getProperties)(void* context, void* window, BackendWindowProperties* properties){};
    BackendWindowOperationResult (*setFullscreenModeToDesktop)(void* context, void* window){};
    BackendWindowOperationResult (*setFullscreen)(void* context, void* window, bool fullscreen){};
    BackendWindowOperationResult (*setBordered)(void* context, void* window, bool bordered){};
    BackendWindowOperationResult (*setResizable)(void* context, void* window, bool resizable){};
    BackendWindowOperationResult (*setAlwaysOnTop)(void* context, void* window, bool alwaysOnTop){};
    BackendWindowOperationResult (*minimize)(void* context, void* window){};
    BackendWindowOperationResult (*maximize)(void* context, void* window){};
    BackendWindowOperationResult (*restore)(void* context, void* window){};
    bool (*startTextInput)(void* context, void* window){};
    bool (*stopTextInput)(void* context, void* window){};
    bool (*isTextInputActive)(void* context, void* window){};
    bool (*clearTextComposition)(void* context, void* window){};
    bool (*setTextInputArea)(void* context, void* window, const BackendTextInputArea* area){};
    bool (*setMouseGrab)(void* context, void* window, bool grabbed){};
    bool (*isMouseGrabbed)(void* context, void* window){};
    bool (*setRelativeMouseMode)(void* context, void* window, bool enabled){};
    bool (*isRelativeMouseModeEnabled)(void* context, void* window){};
    BackendNativeWindowHandleResult (*getNativeHandle)(void* context, void* window,
                                                       NativeWindowHandle* handle){};
};

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

struct PlatformDisplayBackend final
{
    void* context{};
    bool (*enumerate)(void* context, std::vector<std::uint32_t>& displayIds){};
    const char* (*getName)(void* context, std::uint32_t displayId){};
    bool (*getBounds)(void* context, std::uint32_t displayId, BackendScreenRectangle* bounds){};
    bool (*getUsableBounds)(void* context, std::uint32_t displayId,
                            BackendScreenRectangle* bounds){};
    bool (*getCurrentRefreshRate)(void* context, std::uint32_t displayId,
                                  float* refreshRateHertz){};
    BackendDisplayOrientation (*getCurrentOrientation)(void* context, std::uint32_t displayId){};
    float (*getContentScale)(void* context, std::uint32_t displayId){};
    std::uint32_t (*getForWindow)(void* context, void* window){};
    float (*getWindowPixelDensity)(void* context, void* window){};
    float (*getWindowDisplayScale)(void* context, void* window){};
};

class IPlatformRuntimeBackendFactory
{
public:
    virtual ~IPlatformRuntimeBackendFactory() noexcept = default;

    IPlatformRuntimeBackendFactory(const IPlatformRuntimeBackendFactory&) = delete;
    IPlatformRuntimeBackendFactory& operator=(const IPlatformRuntimeBackendFactory&) = delete;
    IPlatformRuntimeBackendFactory(IPlatformRuntimeBackendFactory&&) = delete;
    IPlatformRuntimeBackendFactory& operator=(IPlatformRuntimeBackendFactory&&) = delete;

    [[nodiscard]] virtual std::unique_ptr<IPlatformRuntimeBackend> Create() const = 0;

protected:
    IPlatformRuntimeBackendFactory() noexcept = default;
};

inline constexpr std::size_t kSystemCursorShapeCount{11};

[[nodiscard]] bool IsWindowGraphicsCompatibilitySupported(
    WindowGraphicsCompatibility compatibility) noexcept;
[[nodiscard]] BackendNativeWindowDriver GetNativeWindowDriver(std::string_view driverName) noexcept;
[[nodiscard]] std::uint64_t BuildSdlWindowFlags(const BackendWindowCreateDesc& desc) noexcept;
[[nodiscard]] bool IsReservedSdlWindowPosition(std::int32_t value) noexcept;

[[nodiscard]] std::unique_ptr<IPlatformRuntimeBackend> GetPlatformRuntimeBackend();
[[nodiscard]] PlatformWindowBackend GetPlatformWindowBackend() noexcept;
[[nodiscard]] PlatformDisplayBackend GetPlatformDisplayBackend() noexcept;

// The factory and any state borrowed by its backends must outlive each created runtime.
void SetPlatformRuntimeBackendForTesting(const IPlatformRuntimeBackendFactory* factory) noexcept;
void SetPlatformWindowBackendForTesting(const PlatformWindowBackend* backend) noexcept;
void SetPlatformDisplayBackendForTesting(const PlatformDisplayBackend* backend) noexcept;
} // namespace pond::platform::detail
