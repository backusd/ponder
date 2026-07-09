#pragma once

#include <ponder/platform/Mouse.hpp>
#include <ponder/platform/NativeWindow.hpp>
#include <ponder/platform/WindowGraphics.hpp>

#include <SDL3/SDL_dialog.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

union SDL_Event;

namespace pond::platform::detail
{
enum class ApplicationMetadataProperty : std::uint8_t
{
    Name,
    Version,
    Identifier
};

struct BackendWindowCreateDesc final
{
    const char* title{};
    int width{};
    int height{};
    bool resizable{};
    bool highPixelDensity{};
    WindowGraphicsCompatibility graphicsCompatibility{
        WindowGraphicsCompatibility::Default};
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
    Wayland,
    Cocoa
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

struct BackendClipboardTextResult final
{
    char* text{};
    std::string errorText;
};

enum class BackendDialogKind : std::uint8_t
{
    OpenFile,
    SaveFile,
    OpenFolder
};

struct BackendDialogRequestDesc final
{
    BackendDialogKind kind{BackendDialogKind::OpenFile};
    SDL_DialogFileCallback callback{};
    void* userdata{};
    void* parentWindow{};
    const SDL_DialogFileFilter* filters{};
    int filterCount{};
    const char* defaultLocation{};
    bool allowMultipleSelection{};
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
    bool (*getProperties)(void* context, void* window,
                          BackendWindowProperties* properties){};
    BackendWindowOperationResult (*setFullscreenModeToDesktop)(void* context,
                                                                void* window){};
    BackendWindowOperationResult (*setFullscreen)(void* context, void* window,
                                                   bool fullscreen){};
    BackendWindowOperationResult (*setBordered)(void* context, void* window,
                                                 bool bordered){};
    BackendWindowOperationResult (*setResizable)(void* context, void* window,
                                                  bool resizable){};
    BackendWindowOperationResult (*setAlwaysOnTop)(void* context, void* window,
                                                    bool alwaysOnTop){};
    BackendWindowOperationResult (*minimize)(void* context, void* window){};
    BackendWindowOperationResult (*maximize)(void* context, void* window){};
    BackendWindowOperationResult (*restore)(void* context, void* window){};
    bool (*startTextInput)(void* context, void* window){};
    bool (*stopTextInput)(void* context, void* window){};
    bool (*isTextInputActive)(void* context, void* window){};
    bool (*clearTextComposition)(void* context, void* window){};
    bool (*setTextInputArea)(void* context, void* window,
                             const BackendTextInputArea* area){};
    bool (*setMouseGrab)(void* context, void* window, bool grabbed){};
    bool (*isMouseGrabbed)(void* context, void* window){};
    bool (*setRelativeMouseMode)(void* context, void* window, bool enabled){};
    bool (*isRelativeMouseModeEnabled)(void* context, void* window){};
    BackendNativeWindowHandleResult (*getNativeHandle)(
        void* context, void* window, void** cachedMetalView,
        NativeWindowHandle* handle){};
    void (*destroyMetalView)(void* context, void* metalView){};
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
    bool (*getBounds)(void* context, std::uint32_t displayId,
                      BackendScreenRectangle* bounds){};
    bool (*getUsableBounds)(void* context, std::uint32_t displayId,
                            BackendScreenRectangle* bounds){};
    bool (*getCurrentRefreshRate)(void* context, std::uint32_t displayId,
                                  float* refreshRateHertz){};
    BackendDisplayOrientation (*getCurrentOrientation)(void* context,
                                                       std::uint32_t displayId){};
    float (*getContentScale)(void* context, std::uint32_t displayId){};
    std::uint32_t (*getForWindow)(void* context, void* window){};
    float (*getWindowPixelDensity)(void* context, void* window){};
    float (*getWindowDisplayScale)(void* context, void* window){};
};

struct PlatformRuntimeBackend final
{
    void* context{};
    bool (*isMainThread)(void* context){};
    bool (*hasInitializedSubsystems)(void* context){};
    bool (*hasExpectedRuntimeSubsystems)(void* context){};
    const char* (*getAppMetadataProperty)(void* context,
                                          ApplicationMetadataProperty property){};
    bool (*setAppMetadataProperty)(void* context, ApplicationMetadataProperty property,
                                   const char* value){};
    const char* (*getHint)(void* context, const char* name){};
    bool (*setHintOverride)(void* context, const char* name, const char* value){};
    bool (*resetHint)(void* context, const char* name){};
    bool (*initializeVideo)(void* context){};
    void (*quit)(void* context){};
    std::uint64_t (*getTicksNanoseconds)(void* context){};
    bool (*pollEvent)(void* context, SDL_Event* event){};
    bool (*supportsGlobalMouse)(void* context){};
    void (*getGlobalMousePosition)(void* context, float* x, float* y){};
    bool (*setMouseCapture)(void* context, bool enabled){};
    void* (*createSystemCursor)(void* context, SystemCursorShape shape){};
    bool (*setCursor)(void* context, void* cursor){};
    void (*destroyCursor)(void* context, void* cursor){};
    bool (*showCursor)(void* context){};
    bool (*hideCursor)(void* context){};
    bool (*isCursorVisible)(void* context){};
    bool (*supportsClipboardText)(void* context){};
    BackendClipboardTextResult (*getClipboardText)(void* context){};
    void (*freeClipboardText)(void* context, char* text){};
    bool (*setClipboardText)(void* context, const char* text){};
    bool (*openExternalUri)(void* context, const char* uri){};
    void (*showDialog)(void* context, const BackendDialogRequestDesc& desc){};
};

inline constexpr std::size_t kSystemCursorShapeCount{11};

inline constexpr char kMouseFocusClickThroughHint[]{"SDL_MOUSE_FOCUS_CLICKTHROUGH"};
inline constexpr char kMouseAutoCaptureHint[]{"SDL_MOUSE_AUTO_CAPTURE"};

[[nodiscard]] bool IsWindowGraphicsCompatibilitySupported(
    WindowGraphicsCompatibility compatibility) noexcept;
[[nodiscard]] BackendNativeWindowDriver GetNativeWindowDriver(
    std::string_view driverName) noexcept;
[[nodiscard]] std::uint64_t BuildSdlWindowFlags(
    const BackendWindowCreateDesc& desc) noexcept;
[[nodiscard]] bool IsReservedSdlWindowPosition(std::int32_t value) noexcept;

[[nodiscard]] PlatformRuntimeBackend GetPlatformRuntimeBackend() noexcept;
[[nodiscard]] PlatformWindowBackend GetPlatformWindowBackend() noexcept;
[[nodiscard]] PlatformDisplayBackend GetPlatformDisplayBackend() noexcept;

// The override must outlive every runtime created while it is installed.
void SetPlatformRuntimeBackendForTesting(const PlatformRuntimeBackend* backend) noexcept;
void SetPlatformWindowBackendForTesting(const PlatformWindowBackend* backend) noexcept;
void SetPlatformDisplayBackendForTesting(const PlatformDisplayBackend* backend) noexcept;
} // namespace pond::platform::detail
