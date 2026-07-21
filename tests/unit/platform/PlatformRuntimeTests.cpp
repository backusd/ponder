#include <ponder/core/PonderException.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_platform_defines.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <format>
#include <gtest/gtest.h>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "HintManagerBackend.hpp"
#include "PlatformRuntimeBackend.hpp"
#include "SdlError.hpp"

namespace
{
using pond::platform::detail::ApplicationMetadataProperty;
using pond::platform::detail::BackendDialogCancellation;
using pond::platform::detail::BackendDialogFailure;
using pond::platform::detail::BackendDialogKind;
using pond::platform::detail::BackendDialogRequestDesc;
using pond::platform::detail::BackendDialogSelection;
using pond::platform::detail::BackendWindowHandle;
using pond::platform::detail::BackendWindowLogicalSize;
using pond::platform::detail::BackendWindowPixelSize;
using pond::platform::detail::BackendWindowPosition;
using pond::platform::detail::CursorHandle;
using pond::platform::detail::IBackendDialogCallback;
using pond::platform::detail::BackendDisplayOrientation;
using pond::platform::detail::BackendNativeWindowDriver;


using pond::platform::detail::BackendScreenRectangle;
using pond::platform::detail::BackendTextInputArea;
using pond::platform::detail::BackendWindowCreateDesc;

using pond::platform::detail::BackendWindowProperties;
using pond::platform::detail::HintManagerAccess;
using pond::platform::detail::IHintBackend;
using pond::platform::detail::IPlatformDisplayBackend;
using pond::platform::detail::IPlatformDisplayBackendFactory;
using pond::platform::detail::IPlatformRuntimeBackend;
using pond::platform::detail::IPlatformRuntimeBackendFactory;
using pond::platform::detail::IPlatformWindowBackend;
using pond::platform::detail::IPlatformWindowBackendFactory;

[[nodiscard]] constexpr std::size_t MetadataIndex(ApplicationMetadataProperty property) noexcept
{
    return static_cast<std::size_t>(property);
}

constexpr std::array<pond::platform::SystemCursorShape,
                     pond::platform::detail::kSystemCursorShapeCount>
    kSystemCursorShapes{pond::platform::SystemCursorShape::Default,
                        pond::platform::SystemCursorShape::TextInput,
                        pond::platform::SystemCursorShape::Move,
                        pond::platform::SystemCursorShape::ResizeNorthSouth,
                        pond::platform::SystemCursorShape::ResizeEastWest,
                        pond::platform::SystemCursorShape::ResizeNortheastSouthwest,
                        pond::platform::SystemCursorShape::ResizeNorthwestSoutheast,
                        pond::platform::SystemCursorShape::Pointer,
                        pond::platform::SystemCursorShape::Wait,
                        pond::platform::SystemCursorShape::Progress,
                        pond::platform::SystemCursorShape::NotAllowed};

struct FakeWindow final
{
    std::uint32_t backendId{};
    std::string title;
    int x{};
    int y{};
    int width{};
    int height{};
    int pixelWidth{};
    int pixelHeight{};
    int minimumWidth{};
    int minimumHeight{};
    std::uint32_t backendDisplayId{1};
    float pixelDensity{1.0F};
    float displayScale{1.0F};
    bool visible{};
    bool desktopFullscreen{};
    bool borderless{};
    bool resizable{};
    bool minimized{};
    bool maximized{};
    bool pendingMinimized{};
    bool pendingMaximized{};
    bool inputFocus{};
    bool alwaysOnTop{};
    bool highPixelDensity{};
    bool textInputActive{};
    bool mouseGrabbed{};
    bool relativeMouseModeEnabled{};
    int clearedTextCompositions{};
    std::optional<BackendTextInputArea> textInputArea;
    pond::platform::WindowGraphicsCompatibility graphicsCompatibility{
        pond::platform::WindowGraphicsCompatibility::Default};
};

struct FakeCursor final
{
    pond::platform::SystemCursorShape shape{pond::platform::SystemCursorShape::Default};
};

struct FakeDisplay final
{
    std::uint32_t backendId{};
    std::string name;
    BackendScreenRectangle bounds;
    BackendScreenRectangle usableBounds;
    float refreshRateHertz{};
    BackendDisplayOrientation orientation{BackendDisplayOrientation::Unknown};
    float contentScale{1.0F};
};

struct QueuedBackendEvent final
{
    SDL_Event event{};
    std::optional<std::vector<std::uint32_t>> connectedDisplayIds;
};

struct FakeDialogLaunch final
{
    BackendDialogKind kind{BackendDialogKind::OpenFile};
    std::optional<BackendWindowHandle> parentWindow;
    std::vector<pond::platform::DialogFileFilter> filters;
    std::optional<std::string> defaultLocation;
    bool allowMultipleSelection{};
    IBackendDialogCallback* callback{};
};

struct FakeRuntimeBackendState final
{
    bool mainThread{true};
    bool initializedSubsystems{false};
    bool onlyRuntimeSubsystems{true};
    bool initializeVideoSucceeds{true};
    std::optional<ApplicationMetadataProperty> failingMetadataProperty;
    int metadataFailuresRemaining{};
    std::string failingHint;
    int hintFailuresRemaining{};
    std::uint64_t ticks{123'456};
    bool timestampCaptureOverwritesSdlError{};
    std::uint32_t nextBackendWindowId{100};
    std::uint32_t defaultWindowDisplayId{1};
    float defaultWindowPixelDensity{1.0F};
    float defaultWindowDisplayScale{1.0F};
    std::vector<std::uint32_t> scriptedBackendWindowIds;
    std::size_t scriptedBackendWindowIdIndex{};
    std::string failingWindowOperation;
    int windowFailuresRemaining{};
    std::string unsupportedWindowOperation;
    int unsupportedWindowOperationsRemaining{};
    bool applyWindowOperationEffects{true};
    std::string failingDisplayOperation;
    int displayFailuresRemaining{};
    std::string nativeVideoDriver{"windows"};
    void* nativeWin32Instance{reinterpret_cast<void*>(std::uintptr_t{0x1000})};
    void* nativeWin32Window{reinterpret_cast<void*>(std::uintptr_t{0x2000})};
    void* nativeX11Display{reinterpret_cast<void*>(std::uintptr_t{0x3000})};
    std::uintptr_t nativeX11Window{0x4000};
    void* nativeWaylandDisplay{reinterpret_cast<void*>(std::uintptr_t{0x5000})};
    void* nativeWaylandSurface{reinterpret_cast<void*>(std::uintptr_t{0x6000})};
    std::optional<std::uint32_t> disconnectDisplayOnFailure;
    bool destroyOverwritesError{};
    bool globalMouseSupported{true};
    pond::platform::LogicalPoint globalMousePosition{100.0F, 200.0F};
    bool mouseCaptureEnabled{};
    bool cursorVisible{true};
    std::string failingMouseOperation;
    int mouseFailuresRemaining{};
    bool clipboardTextSupported{true};
    std::string clipboardText;
    std::string clipboardGetError;
    bool clipboardGetReturnsNull{};
    std::string failingServiceOperation;
    int serviceFailuresRemaining{};

    int mainThreadChecks{};
    int initializedSubsystemChecks{};
    int runtimeSubsystemChecks{};
    int metadataCalls{};
    int initializeVideoCalls{};
    int quitCalls{};
    int tickCalls{};
    int createWindowCalls{};
    int destroyWindowCalls{};
    int enumerateDisplayCalls{};
    int pollEventCalls{};

    bool metadataObservedDuringInitialization{};
    bool attemptCreateDuringRestoration{};
    bool attemptedCreateDuringRestoration{};
    std::optional<pond::core::ErrorCode> restorationCreateError;
    bool cursorsPresentAtQuit{};

    std::array<std::optional<std::string>, 3> metadata;
    std::unordered_map<std::string, std::string> hints;
    std::list<FakeWindow> windows;
    std::list<FakeCursor> cursors;
    CursorHandle activeCursor;
    std::vector<pond::platform::SystemCursorShape> createdCursorShapes;
    std::vector<pond::platform::SystemCursorShape> selectedCursorShapes;
    std::vector<pond::platform::SystemCursorShape> destroyedCursorShapes;
    std::vector<std::string> openedExternalUris;
    std::vector<FakeDialogLaunch> dialogLaunches;
    std::vector<std::uint32_t> connectedDisplayIds;
    std::unordered_map<std::uint32_t, FakeDisplay> displays;
    std::deque<QueuedBackendEvent> eventQueue;
    std::vector<std::string> calls;
};

[[nodiscard]] FakeRuntimeBackendState& GetFake(void* context)
{
    return *static_cast<FakeRuntimeBackendState*>(context);
}


[[nodiscard]] FakeWindow& GetFakeWindow(BackendWindowHandle window)
{
    return *reinterpret_cast<FakeWindow*>(window.GetValue());
}

[[nodiscard]] FakeCursor& GetFakeCursor(CursorHandle cursor)
{
    return *reinterpret_cast<FakeCursor*>(cursor.GetValue());
}

[[nodiscard]] FakeDisplay& GetFakeDisplay(FakeRuntimeBackendState& fake,
                                          std::uint32_t backendDisplayId)
{
    return fake.displays.at(backendDisplayId);
}

[[nodiscard]] SDL_Event MakeQueuedQuitEvent(std::uint64_t timestamp = 1'000)
{
    SDL_Event event{};
    event.quit.type = SDL_EVENT_QUIT;
    event.quit.timestamp = timestamp;
    return event;
}

[[nodiscard]] SDL_Event MakeQueuedWindowEvent(SDL_EventType type, std::uint32_t backendWindowId,
                                              std::int32_t data1 = 0, std::int32_t data2 = 0,
                                              std::uint64_t timestamp = 1'000)
{
    SDL_Event event{};
    event.window.type = type;
    event.window.timestamp = timestamp;
    event.window.windowID = backendWindowId;
    event.window.data1 = data1;
    event.window.data2 = data2;
    return event;
}

[[nodiscard]] SDL_Event MakeQueuedDisplayEvent(SDL_EventType type, std::uint32_t backendDisplayId,
                                               std::int32_t data1 = 0, std::int32_t data2 = 0,
                                               std::uint64_t timestamp = 1'000)
{
    SDL_Event event{};
    event.display.type = type;
    event.display.timestamp = timestamp;
    event.display.displayID = backendDisplayId;
    event.display.data1 = data1;
    event.display.data2 = data2;
    return event;
}

[[nodiscard]] SDL_Event MakeQueuedKeyboardEvent(SDL_EventType type, std::uint32_t backendWindowId,
                                                SDL_Scancode scancode, SDL_Keycode key,
                                                SDL_Keymod modifiers = SDL_KMOD_NONE,
                                                bool repeat = false,
                                                std::uint64_t timestamp = 1'000)
{
    SDL_Event event{};
    event.key.type = type;
    event.key.timestamp = timestamp;
    event.key.windowID = backendWindowId;
    event.key.scancode = scancode;
    event.key.key = key;
    event.key.mod = modifiers;
    event.key.down = type == SDL_EVENT_KEY_DOWN;
    event.key.repeat = repeat;
    return event;
}

[[nodiscard]] SDL_Event MakeQueuedTextInputEvent(std::uint32_t backendWindowId, const char* text,
                                                 std::uint64_t timestamp = 1'000)
{
    SDL_Event event{};
    event.text.type = SDL_EVENT_TEXT_INPUT;
    event.text.timestamp = timestamp;
    event.text.windowID = backendWindowId;
    event.text.text = text;
    return event;
}

[[nodiscard]] SDL_Event MakeQueuedTextCompositionEvent(std::uint32_t backendWindowId,
                                                       const char* text, std::int32_t start,
                                                       std::int32_t length,
                                                       std::uint64_t timestamp = 1'000)
{
    SDL_Event event{};
    event.edit.type = SDL_EVENT_TEXT_EDITING;
    event.edit.timestamp = timestamp;
    event.edit.windowID = backendWindowId;
    event.edit.text = text;
    event.edit.start = start;
    event.edit.length = length;
    return event;
}

[[nodiscard]] SDL_Event MakeQueuedMouseMotionEvent(std::uint32_t backendWindowId, float x, float y,
                                                   float xRelative, float yRelative,
                                                   std::uint64_t timestamp = 1'000)
{
    SDL_Event event{};
    event.motion.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.timestamp = timestamp;
    event.motion.windowID = backendWindowId;
    event.motion.x = x;
    event.motion.y = y;
    event.motion.xrel = xRelative;
    event.motion.yrel = yRelative;
    return event;
}

[[nodiscard]] SDL_Event MakeQueuedMouseButtonEvent(SDL_EventType type,
                                                   std::uint32_t backendWindowId,
                                                   std::uint8_t button, float x, float y,
                                                   std::uint64_t timestamp = 1'000)
{
    SDL_Event event{};
    event.button.type = type;
    event.button.timestamp = timestamp;
    event.button.windowID = backendWindowId;
    event.button.button = button;
    event.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.x = x;
    event.button.y = y;
    return event;
}

[[nodiscard]] SDL_Event MakeQueuedMouseWheelEvent(std::uint32_t backendWindowId, float horizontal,
                                                  float vertical, float mouseX, float mouseY,
                                                  std::uint64_t timestamp = 1'000)
{
    SDL_Event event{};
    event.wheel.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.timestamp = timestamp;
    event.wheel.windowID = backendWindowId;
    event.wheel.x = horizontal;
    event.wheel.y = vertical;
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    event.wheel.mouse_x = mouseX;
    event.wheel.mouse_y = mouseY;
    return event;
}

[[nodiscard]] SDL_Event MakeQueuedDropEvent(SDL_EventType type, std::uint32_t backendWindowId,
                                            const char* data = nullptr,
                                            const char* source = nullptr, float x = 0.0F,
                                            float y = 0.0F, std::uint64_t timestamp = 1'000)
{
    SDL_Event event{};
    event.drop.type = type;
    event.drop.timestamp = timestamp;
    event.drop.windowID = backendWindowId;
    event.drop.data = data;
    event.drop.source = source;
    event.drop.x = x;
    event.drop.y = y;
    return event;
}

template <typename Event>
void ExpectPolledEvent(const std::optional<pond::platform::PlatformEvent>& event,
                       const Event& expected)
{
    ASSERT_TRUE(event.has_value());
    ASSERT_TRUE(std::holds_alternative<Event>(*event));
    EXPECT_EQ(std::get<Event>(*event), expected);
}

[[nodiscard]] bool FailWindowOperation(FakeRuntimeBackendState& fake, std::string_view operation)
{
    if (fake.failingWindowOperation != operation || fake.windowFailuresRemaining == 0)
    {
        return false;
    }

    --fake.windowFailuresRemaining;
    const std::string message = "synthetic " + std::string{operation} + " failure";
    static_cast<void>(SDL_SetError("%s", message.c_str()));
    return true;
}

[[nodiscard]] std::string GetFakeBackendWindowContext(BackendWindowHandle window)
{
    return window.IsValid() ? std::format("backend window {}", window) : "window";
}

[[nodiscard]] pond::core::Error CaptureFakeWindowFailure(
    pond::platform::PlatformErrorCode code, std::string_view operation,
    BackendWindowHandle window = {})
{
    return pond::platform::detail::CaptureSdlFailure(
        pond::platform::ToErrorCode(code), operation, GetFakeBackendWindowContext(window));
}

[[nodiscard]] pond::core::VoidResult ToFakeWindowResult(
    bool succeeded, std::string_view operation, BackendWindowHandle window)
{
    if (!succeeded)
    {
        return pond::core::VoidResult::FromError(
            CaptureFakeWindowFailure(pond::platform::PlatformErrorCode::BackendFailure,
                                     operation, window));
    }

    return pond::core::VoidResult::Success();
}

template <typename Value>
[[nodiscard]] pond::core::Result<Value> ToFakeWindowResult(
    std::optional<Value> value, std::string_view operation, BackendWindowHandle window)
{
    if (!value.has_value())
    {
        return pond::core::Result<Value>::FromError(
            CaptureFakeWindowFailure(pond::platform::PlatformErrorCode::BackendFailure,
                                     operation, window));
    }

    return std::move(*value);
}

[[nodiscard]] pond::core::VoidResult GetWindowOperationResult(
    FakeRuntimeBackendState& fake, std::string_view operation,
    std::string_view backendOperation, BackendWindowHandle window)
{
    if (fake.unsupportedWindowOperation == operation &&
        fake.unsupportedWindowOperationsRemaining != 0)
    {
        --fake.unsupportedWindowOperationsRemaining;
        return pond::core::VoidResult::FromError(pond::core::Error{
            pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported),
            std::format("{} did not apply the requested state for {}.", backendOperation,
                        GetFakeBackendWindowContext(window))});
    }
    if (FailWindowOperation(fake, operation))
    {
        return pond::core::VoidResult::FromError(
            CaptureFakeWindowFailure(pond::platform::PlatformErrorCode::BackendFailure,
                                     backendOperation, window));
    }
    return pond::core::VoidResult::Success();
}
[[nodiscard]] bool FailDisplayOperation(FakeRuntimeBackendState& fake, std::string_view operation)
{
    if (fake.failingDisplayOperation != operation || fake.displayFailuresRemaining == 0)
    {
        return false;
    }

    --fake.displayFailuresRemaining;
    const std::string message = "synthetic " + std::string{operation} + " failure";
    static_cast<void>(SDL_SetError("%s", message.c_str()));

    if (fake.disconnectDisplayOnFailure.has_value())
    {
        const std::uint32_t disconnectedId = *fake.disconnectDisplayOnFailure;
        std::erase(fake.connectedDisplayIds, disconnectedId);
        fake.disconnectDisplayOnFailure.reset();
    }
    return true;
}

[[nodiscard]] bool FailMouseOperation(FakeRuntimeBackendState& fake, std::string_view operation)
{
    if (fake.failingMouseOperation != operation || fake.mouseFailuresRemaining == 0)
    {
        return false;
    }

    --fake.mouseFailuresRemaining;
    const std::string message = "synthetic " + std::string{operation} + " failure";
    static_cast<void>(SDL_SetError("%s", message.c_str()));
    return true;
}

[[nodiscard]] bool FailServiceOperation(FakeRuntimeBackendState& fake, std::string_view operation)
{
    if (fake.failingServiceOperation != operation || fake.serviceFailuresRemaining == 0)
    {
        return false;
    }

    --fake.serviceFailuresRemaining;
    const std::string message = "synthetic " + std::string{operation} + " failure";
    static_cast<void>(SDL_SetError("%s", message.c_str()));
    return true;
}
[[nodiscard]] pond::core::Error CaptureFakeRuntimeFailure(
    std::string_view operation, std::string_view objectContext = {})
{
    return pond::platform::detail::CaptureSdlFailure(
        pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure),
        operation, objectContext);
}

[[nodiscard]] const char* GetHintValue(FakeRuntimeBackendState& fake, const char* name)
{
    const auto iterator = fake.hints.find(name);
    return iterator != fake.hints.end() ? iterator->second.c_str() : nullptr;
}

[[nodiscard]] int CountRejectedWindowStateCalls(pond::platform::Window& window)
{
    int rejectedCalls{};
    const auto invoke = [&rejectedCalls](auto&& operation)
    {
        try
        {
            operation();
        }
        catch (const pond::core::PonderException&)
        {
            ++rejectedCalls;
        }
    };

    invoke(
        [&window]()
        {
            static_cast<void>(window.GetPresentation());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(
                window.SetPresentation(pond::platform::WindowPresentation::DesktopFullscreen));
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.GetDecoration());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.SetDecoration(pond::platform::WindowDecoration::Borderless));
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.GetState());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.Minimize());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.Maximize());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.Restore());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.IsVisible());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.IsResizable());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.SetResizable(false));
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.IsFocused());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.IsAlwaysOnTop());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.SetAlwaysOnTop(true));
        });
    return rejectedCalls;
}

[[nodiscard]] int CountRejectedWindowTextInputCalls(pond::platform::Window& window)
{
    int rejectedCalls{};
    const auto invoke = [&rejectedCalls](auto&& operation)
    {
        try
        {
            operation();
        }
        catch (const pond::core::PonderException&)
        {
            ++rejectedCalls;
        }
    };

    invoke(
        [&window]()
        {
            static_cast<void>(window.StartTextInput());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.StopTextInput());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.IsTextInputActive());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.ClearTextComposition());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.SetTextInputArea(pond::platform::TextInputArea{}));
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.ClearTextInputArea());
        });
    return rejectedCalls;
}

[[nodiscard]] int CountRejectedWindowMouseCalls(pond::platform::Window& window)
{
    int rejectedCalls{};
    const auto invoke = [&rejectedCalls](auto&& operation)
    {
        try
        {
            operation();
        }
        catch (const pond::core::PonderException&)
        {
            ++rejectedCalls;
        }
    };

    invoke(
        [&window]()
        {
            static_cast<void>(window.SetMouseGrab(true));
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.IsMouseGrabbed());
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.SetRelativeMouseMode(true));
        });
    invoke(
        [&window]()
        {
            static_cast<void>(window.IsRelativeMouseModeEnabled());
        });
    return rejectedCalls;
}

[[nodiscard]] int CountRejectedRuntimeMouseCalls(pond::platform::PlatformRuntime& runtime)
{
    int rejectedCalls{};
    const auto invoke = [&rejectedCalls](auto&& operation)
    {
        try
        {
            operation();
        }
        catch (const pond::core::PonderException&)
        {
            ++rejectedCalls;
        }
    };

    invoke(
        [&runtime]()
        {
            static_cast<void>(runtime.SetMouseCapture(true));
        });
    invoke(
        [&runtime]()
        {
            static_cast<void>(runtime.GetGlobalMousePosition());
        });
    invoke(
        [&runtime]()
        {
            static_cast<void>(runtime.SetSystemCursor(pond::platform::SystemCursorShape::Default));
        });
    invoke(
        [&runtime]()
        {
            static_cast<void>(runtime.ShowCursor());
        });
    invoke(
        [&runtime]()
        {
            static_cast<void>(runtime.HideCursor());
        });
    invoke(
        [&runtime]()
        {
            static_cast<void>(runtime.IsCursorVisible());
        });
    return rejectedCalls;
}

[[nodiscard]] int CountRejectedRuntimeServiceCalls(pond::platform::PlatformRuntime& runtime)
{
    int rejectedCalls{};
    const auto invoke = [&rejectedCalls](auto&& operation)
    {
        try
        {
            operation();
        }
        catch (const pond::core::PonderException&)
        {
            ++rejectedCalls;
        }
    };

    invoke(
        [&runtime]()
        {
            static_cast<void>(runtime.GetClipboardText());
        });
    invoke(
        [&runtime]()
        {
            static_cast<void>(runtime.SetClipboardText("clipboard"));
        });
    invoke(
        [&runtime]()
        {
            static_cast<void>(runtime.OpenExternalUri("https://example.invalid/ponder"));
        });
    invoke(
        [&runtime]()
        {
            static_cast<void>(runtime.ShowOpenFileDialog(pond::platform::OpenFileDialogDesc{}));
        });
    invoke(
        [&runtime]()
        {
            static_cast<void>(runtime.ShowSaveFileDialog(pond::platform::SaveFileDialogDesc{}));
        });
    invoke(
        [&runtime]()
        {
            static_cast<void>(runtime.ShowOpenFolderDialog(pond::platform::OpenFolderDialogDesc{}));
        });
    return rejectedCalls;
}

void MaybeAttemptCreateDuringRestoration(FakeRuntimeBackendState& fake)
{
    if (!fake.attemptCreateDuringRestoration || fake.quitCalls == 0 ||
        fake.attemptedCreateDuringRestoration)
    {
        return;
    }

    fake.attemptedCreateDuringRestoration = true;
    const auto result =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    if (!result.HasValue())
    {
        fake.restorationCreateError = result.GetError().GetCode();
    }
}

[[nodiscard]] bool FakeIsMainThread(void* context)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    ++fake.mainThreadChecks;
    fake.calls.emplace_back("is-main-thread");
    return fake.mainThread;
}

[[nodiscard]] bool FakeHasInitializedSubsystems(void* context)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    ++fake.initializedSubsystemChecks;
    fake.calls.emplace_back("was-init");
    return fake.initializedSubsystems;
}

[[nodiscard]] bool FakeHasExpectedRuntimeSubsystems(void* context)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    ++fake.runtimeSubsystemChecks;
    fake.calls.emplace_back("has-runtime-subsystems");
    return fake.initializedSubsystems && fake.onlyRuntimeSubsystems;
}

[[nodiscard]] pond::core::VoidResult FakeSetAppMetadataProperty(
    void* context, ApplicationMetadataProperty property, const char* value)
{
    using ResultType = pond::core::VoidResult;

    FakeRuntimeBackendState& fake = GetFake(context);
    ++fake.metadataCalls;
    fake.calls.emplace_back("set-metadata");

    if (fake.failingMetadataProperty == property && fake.metadataFailuresRemaining > 0)
    {
        --fake.metadataFailuresRemaining;
        return ResultType::FromError(pond::core::Error{
            pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure),
            std::format("SDL_SetAppMetadataProperty ({}) failed: synthetic metadata failure",
                        property)});
    }

    fake.metadata[MetadataIndex(property)] =
        value != nullptr ? std::optional<std::string>{value} : std::nullopt;
    return ResultType::Success();
}

[[nodiscard]] const char* FakeGetHint(void* context, const char* name)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back(std::string{"get-hint:"} + name);
    return GetHintValue(fake, name);
}

[[nodiscard]] pond::core::VoidResult FakeSetHintOverride(void* context, const char* name,
                                                         const char* value)
{
    using ResultType = pond::core::VoidResult;

    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back(std::string{"set-hint:"} + name + "=" + value);
    MaybeAttemptCreateDuringRestoration(fake);

    if (fake.failingHint == name && fake.hintFailuresRemaining > 0)
    {
        --fake.hintFailuresRemaining;
        static_cast<void>(SDL_SetError("synthetic hint failure"));
        return ResultType::FromError(pond::platform::detail::CaptureSdlFailure(
            pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure),
            "SDL_SetHintWithPriority", name));
    }

    fake.hints[name] = value;
    return ResultType::Success();
}

[[nodiscard]] pond::core::VoidResult FakeResetHint(void* context, const char* name)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back(std::string{"reset-hint:"} + name);
    MaybeAttemptCreateDuringRestoration(fake);
    fake.hints.erase(name);
    return pond::core::VoidResult::Success();
}

[[nodiscard]] pond::core::VoidResult FakeInitializeVideo(void* context)
{
    using ResultType = pond::core::VoidResult;

    FakeRuntimeBackendState& fake = GetFake(context);
    ++fake.initializeVideoCalls;
    fake.calls.emplace_back("initialize-video");

    fake.metadataObservedDuringInitialization =
        fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)].has_value();

    if (!fake.initializeVideoSucceeds)
    {
        return ResultType::FromError(pond::core::Error{
            pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure),
            "SDL_Init (SDL_INIT_VIDEO) failed: synthetic video initialization failure"});
    }

    fake.initializedSubsystems = true;
    return ResultType::Success();
}

void FakeQuit(void* context)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    ++fake.quitCalls;
    fake.calls.emplace_back("quit");
    fake.cursorsPresentAtQuit = !fake.cursors.empty();
    fake.initializedSubsystems = false;
    fake.metadata = {};
    fake.hints.clear();
}

[[nodiscard]] std::uint64_t FakeGetTicksNanoseconds(void* context)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    ++fake.tickCalls;
    fake.calls.emplace_back("ticks");
    if (fake.timestampCaptureOverwritesSdlError)
    {
        static_cast<void>(SDL_SetError("timestamp capture overwrote the dialog error"));
    }
    return fake.ticks;
}

[[nodiscard]] bool FakePollEvent(void* context, SDL_Event* event)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    ++fake.pollEventCalls;
    fake.calls.emplace_back("poll-event");
    if (fake.eventQueue.empty())
    {
        return false;
    }

    QueuedBackendEvent queued = std::move(fake.eventQueue.front());
    fake.eventQueue.pop_front();
    if (queued.connectedDisplayIds.has_value())
    {
        fake.connectedDisplayIds = std::move(*queued.connectedDisplayIds);
    }
    *event = queued.event;
    return true;
}

[[nodiscard]] bool FakeSupportsGlobalMouse(void* context)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("supports-global-mouse");
    return fake.globalMouseSupported;
}

[[nodiscard]] pond::platform::MousePosition FakeGetGlobalMousePosition(void* context)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("get-global-mouse-position");
    return pond::platform::MousePosition{fake.globalMousePosition.x, fake.globalMousePosition.y};
}
[[nodiscard]] pond::core::VoidResult FakeSetMouseCapture(void* context, bool enabled)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-mouse-capture");
    if (FailMouseOperation(fake, "set-mouse-capture"))
    {
        return pond::core::VoidResult::FromError(CaptureFakeRuntimeFailure(
            "SDL_CaptureMouse", enabled ? "enable" : "disable"));
    }

    fake.mouseCaptureEnabled = enabled;
    return pond::core::VoidResult::Success();
}

[[nodiscard]] pond::core::Result<CursorHandle> FakeCreateSystemCursor(
    void* context, pond::platform::SystemCursorShape shape)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("create-system-cursor");
    fake.createdCursorShapes.push_back(shape);
    if (FailMouseOperation(fake, "create-system-cursor"))
    {
        return pond::core::Result<CursorHandle>::FromError(CaptureFakeRuntimeFailure(
            "SDL_CreateSystemCursor", std::format("{}", shape)));
    }

    fake.cursors.emplace_back(FakeCursor{shape});
    return CursorHandle{
        reinterpret_cast<CursorHandle::ValueType>(std::addressof(fake.cursors.back()))};
}

[[nodiscard]] pond::core::VoidResult FakeSetCursor(void* context, CursorHandle cursor)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-cursor");
    fake.selectedCursorShapes.push_back(GetFakeCursor(cursor).shape);
    if (FailMouseOperation(fake, "set-cursor"))
    {
        return pond::core::VoidResult::FromError(CaptureFakeRuntimeFailure(
            "SDL_SetCursor", std::format("cursor {}", cursor)));
    }

    fake.activeCursor = cursor;
    return pond::core::VoidResult::Success();
}

void FakeDestroyCursor(void* context, CursorHandle cursor)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("destroy-cursor");
    const auto* const cursorAddress = reinterpret_cast<const FakeCursor*>(cursor.GetValue());
    const auto iterator = std::ranges::find_if(fake.cursors,
                                               [cursorAddress](const FakeCursor& candidate)
                                               {
                                                   return std::addressof(candidate) ==
                                                          cursorAddress;
                                               });
    if (iterator == fake.cursors.end())
    {
        return;
    }

    fake.destroyedCursorShapes.push_back(iterator->shape);
    if (fake.activeCursor == cursor)
    {
        fake.activeCursor = CursorHandle{};
    }
    fake.cursors.erase(iterator);
}
[[nodiscard]] pond::core::VoidResult FakeShowCursor(void* context)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("show-cursor");
    if (FailMouseOperation(fake, "show-cursor"))
    {
        return pond::core::VoidResult::FromError(
            CaptureFakeRuntimeFailure("SDL_ShowCursor"));
    }

    fake.cursorVisible = true;
    return pond::core::VoidResult::Success();
}

[[nodiscard]] pond::core::VoidResult FakeHideCursor(void* context)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("hide-cursor");
    if (FailMouseOperation(fake, "hide-cursor"))
    {
        return pond::core::VoidResult::FromError(
            CaptureFakeRuntimeFailure("SDL_HideCursor"));
    }

    fake.cursorVisible = false;
    return pond::core::VoidResult::Success();
}

[[nodiscard]] bool FakeIsCursorVisible(void* context)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("is-cursor-visible");
    return fake.cursorVisible;
}

[[nodiscard]] bool FakeSupportsClipboardText(void* context)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("supports-clipboard-text");
    return fake.clipboardTextSupported;
}

[[nodiscard]] pond::core::Result<std::string> FakeGetClipboardText(void* context)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("get-clipboard-text");
    if (fake.clipboardGetReturnsNull ||
        (fake.clipboardText.empty() && !fake.clipboardGetError.empty()))
    {
        const std::string error = fake.clipboardGetError.empty()
                                      ? "synthetic get-clipboard-text failure"
                                      : fake.clipboardGetError;
        return pond::core::Result<std::string>::FromError(
            pond::platform::detail::CaptureSdlFailure(
                pond::platform::ToErrorCode(
                    pond::platform::PlatformErrorCode::BackendFailure),
                "SDL_GetClipboardText", "clipboard text", error));
    }

    return fake.clipboardText;
}

[[nodiscard]] pond::core::VoidResult FakeSetClipboardText(void* context, std::string_view text)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-clipboard-text");
    if (FailServiceOperation(fake, "set-clipboard-text"))
    {
        return pond::core::VoidResult::FromError(
            CaptureFakeRuntimeFailure("SDL_SetClipboardText", "clipboard text"));
    }

    fake.clipboardText.assign(text.data(), text.size());
    return pond::core::VoidResult::Success();
}

[[nodiscard]] pond::core::VoidResult FakeOpenExternalUri(void* context, std::string_view uri)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("open-external-uri");
    if (FailServiceOperation(fake, "open-external-uri"))
    {
        return pond::core::VoidResult::FromError(
            CaptureFakeRuntimeFailure("SDL_OpenURL", "external URI"));
    }

    fake.openedExternalUris.emplace_back(uri);
    return pond::core::VoidResult::Success();
}

void FakeShowDialog(void* context, const BackendDialogRequestDesc& desc)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("show-dialog");

    FakeDialogLaunch launch{
        .kind = desc.kind,
        .parentWindow = desc.parentWindow,
        .defaultLocation =
            desc.defaultLocation.has_value()
                ? std::optional<std::string>{std::string{*desc.defaultLocation}}
                : std::nullopt,
        .allowMultipleSelection = desc.allowMultipleSelection,
        .callback = std::addressof(desc.callback)};
    for (const auto& filter : desc.filters)
    {
        launch.filters.push_back(
            pond::platform::DialogFileFilter{.name = filter.name, .pattern = filter.pattern});
    }
    fake.dialogLaunches.push_back(std::move(launch));
}

void CompleteDialogSelection(FakeDialogLaunch& launch, std::span<const std::string> paths,
                             int selectedFilter)
{
    BackendDialogSelection selection;
    selection.paths.assign(paths.begin(), paths.end());
    if (selectedFilter >= 0)
    {
        selection.selectedFilterIndex = static_cast<std::size_t>(selectedFilter);
    }
    launch.callback->OnDialogCompleted(std::move(selection));
}

void CompleteDialogCancellation(FakeDialogLaunch& launch)
{
    launch.callback->OnDialogCompleted(BackendDialogCancellation{});
}

void CompleteDialogFailure(FakeDialogLaunch& launch, std::string_view message)
{
    launch.callback->OnDialogCompleted(BackendDialogFailure{.message = std::string{message}});
}
[[nodiscard]] BackendWindowHandle FakeCreateWindow(
    void* context, const BackendWindowCreateDesc& desc)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    ++fake.createWindowCalls;
    fake.calls.emplace_back("create-window");
    if (FailWindowOperation(fake, "create-window"))
    {
        return BackendWindowHandle{};
    }

    const std::uint32_t backendId =
        fake.scriptedBackendWindowIdIndex < fake.scriptedBackendWindowIds.size()
            ? fake.scriptedBackendWindowIds[fake.scriptedBackendWindowIdIndex++]
            : fake.nextBackendWindowId++;
    fake.windows.emplace_back(FakeWindow{.backendId = backendId,
                                         .title = std::string{desc.title},
                                         .width = desc.logicalSize.width,
                                         .height = desc.logicalSize.height,
                                         .pixelWidth = desc.logicalSize.width,
                                         .pixelHeight = desc.logicalSize.height,
                                         .backendDisplayId = fake.defaultWindowDisplayId,
                                         .pixelDensity = fake.defaultWindowPixelDensity,
                                         .displayScale = fake.defaultWindowDisplayScale,
                                         .resizable = desc.resizable,
                                         .highPixelDensity = desc.highPixelDensity,
                                         .graphicsCompatibility = desc.graphicsCompatibility});
    return BackendWindowHandle{reinterpret_cast<BackendWindowHandle::ValueType>(
        &fake.windows.back())};
}

void FakeDestroyWindow(void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    ++fake.destroyWindowCalls;
    fake.calls.emplace_back("destroy-window");
    if (fake.destroyOverwritesError)
    {
        static_cast<void>(SDL_SetError("cleanup overwrote the primary error"));
    }

    const auto iterator = std::ranges::find_if(fake.windows,
                                               [window](const FakeWindow& candidate)
                                               {
                                                   return BackendWindowHandle{reinterpret_cast<
                                                              BackendWindowHandle::ValueType>(
                                                              &candidate)} == window;
                                               });
    if (iterator != fake.windows.end())
    {
        fake.windows.erase(iterator);
    }
}

[[nodiscard]] std::uint32_t FakeGetWindowId(void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("get-window-id");
    return FailWindowOperation(fake, "get-window-id") ? 0 : GetFakeWindow(window).backendId;
}

[[nodiscard]] std::string FakeGetWindowTitle(void* context, BackendWindowHandle window)
{
    GetFake(context).calls.emplace_back("get-window-title");
    return GetFakeWindow(window).title;
}
[[nodiscard]] bool FakeSetWindowTitle(void* context, BackendWindowHandle window,
                                      std::string_view title)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-window-title");
    if (FailWindowOperation(fake, "set-window-title"))
    {
        return false;
    }
    GetFakeWindow(window).title = title;
    return true;
}
[[nodiscard]] std::optional<BackendWindowPosition> FakeGetWindowPosition(
    void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("get-window-position");
    if (FailWindowOperation(fake, "get-window-position"))
    {
        return std::nullopt;
    }
    const FakeWindow& fakeWindow = GetFakeWindow(window);
    return BackendWindowPosition{fakeWindow.x, fakeWindow.y};
}
[[nodiscard]] bool FakeSetWindowPosition(void* context, BackendWindowHandle window,
                                         BackendWindowPosition position)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-window-position");
    if (FailWindowOperation(fake, "set-window-position"))
    {
        return false;
    }
    FakeWindow& fakeWindow = GetFakeWindow(window);
    fakeWindow.x = position.x;
    fakeWindow.y = position.y;
    return true;
}
[[nodiscard]] std::optional<BackendWindowLogicalSize> FakeGetWindowSize(
    void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("get-window-size");
    if (FailWindowOperation(fake, "get-window-size"))
    {
        return std::nullopt;
    }
    const FakeWindow& fakeWindow = GetFakeWindow(window);
    return BackendWindowLogicalSize{fakeWindow.width, fakeWindow.height};
}
[[nodiscard]] std::optional<BackendWindowPixelSize> FakeGetWindowSizeInPixels(
    void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("get-window-size-in-pixels");
    if (FailWindowOperation(fake, "get-window-size-in-pixels"))
    {
        return std::nullopt;
    }
    const FakeWindow& fakeWindow = GetFakeWindow(window);
    return BackendWindowPixelSize{fakeWindow.pixelWidth, fakeWindow.pixelHeight};
}
[[nodiscard]] bool FakeSetWindowSize(void* context, BackendWindowHandle window,
                                     BackendWindowLogicalSize size)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-window-size");
    if (FailWindowOperation(fake, "set-window-size"))
    {
        return false;
    }
    FakeWindow& fakeWindow = GetFakeWindow(window);
    fakeWindow.width = size.width;
    fakeWindow.height = size.height;
    return true;
}
[[nodiscard]] bool FakeSetWindowMinimumSize(void* context, BackendWindowHandle window,
                                            BackendWindowLogicalSize size)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-window-minimum-size");
    if (FailWindowOperation(fake, "set-window-minimum-size"))
    {
        return false;
    }
    FakeWindow& fakeWindow = GetFakeWindow(window);
    fakeWindow.minimumWidth = size.width;
    fakeWindow.minimumHeight = size.height;
    return true;
}
[[nodiscard]] bool FakeShowWindow(void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("show-window");
    if (FailWindowOperation(fake, "show-window"))
    {
        return false;
    }
    FakeWindow& fakeWindow = GetFakeWindow(window);
    if (fakeWindow.visible)
    {
        return true;
    }
    if (fakeWindow.pendingMinimized)
    {
        fakeWindow.minimized = true;
        fakeWindow.maximized = false;
    }
    else if (fakeWindow.pendingMaximized)
    {
        fakeWindow.minimized = false;
        fakeWindow.maximized = true;
    }
    else
    {
        fakeWindow.minimized = false;
        fakeWindow.maximized = false;
    }
    fakeWindow.pendingMinimized = false;
    fakeWindow.pendingMaximized = false;
    fakeWindow.visible = true;
    return true;
}

[[nodiscard]] bool FakeHideWindow(void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("hide-window");
    if (FailWindowOperation(fake, "hide-window"))
    {
        return false;
    }
    FakeWindow& fakeWindow = GetFakeWindow(window);
    if (fakeWindow.visible)
    {
        fakeWindow.pendingMinimized = fakeWindow.minimized;
        fakeWindow.pendingMaximized = fakeWindow.maximized;
        fakeWindow.visible = false;
    }
    return true;
}

[[nodiscard]] std::optional<BackendWindowProperties> FakeGetWindowProperties(
    void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("get-window-properties");
    if (FailWindowOperation(fake, "get-window-properties"))
    {
        return std::nullopt;
    }

    const FakeWindow& fakeWindow = GetFakeWindow(window);
    return BackendWindowProperties{
        .desktopFullscreen = fakeWindow.desktopFullscreen,
        .hidden = !fakeWindow.visible,
        .borderless = fakeWindow.borderless,
        .resizable = fakeWindow.resizable,
        .minimized = fakeWindow.minimized || fakeWindow.pendingMinimized,
        .maximized = fakeWindow.maximized || fakeWindow.pendingMaximized,
        .inputFocus = fakeWindow.inputFocus,
        .alwaysOnTop = fakeWindow.alwaysOnTop};
}
[[nodiscard]] pond::core::VoidResult FakeSetFullscreenModeToDesktop(
    void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-window-fullscreen-mode-to-desktop");
    return GetWindowOperationResult(fake, "set-window-fullscreen-mode-to-desktop",
                                    "SDL_SetWindowFullscreenMode", window);
}

[[nodiscard]] pond::core::VoidResult FakeSetWindowFullscreen(
    void* context, BackendWindowHandle window, bool fullscreen)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-window-fullscreen");
    pond::core::VoidResult result =
        GetWindowOperationResult(fake, "set-window-fullscreen",
                                 "SDL_SetWindowFullscreen", window);
    if (result.HasValue() && fake.applyWindowOperationEffects)
    {
        GetFakeWindow(window).desktopFullscreen = fullscreen;
    }
    return result;
}

[[nodiscard]] pond::core::VoidResult FakeSetWindowBordered(
    void* context, BackendWindowHandle window, bool bordered)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-window-bordered");
    pond::core::VoidResult result =
        GetWindowOperationResult(fake, "set-window-bordered", "SDL_SetWindowBordered", window);
    if (result.HasValue() && fake.applyWindowOperationEffects)
    {
        GetFakeWindow(window).borderless = !bordered;
    }
    return result;
}

[[nodiscard]] pond::core::VoidResult FakeSetWindowResizable(
    void* context, BackendWindowHandle window, bool resizable)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-window-resizable");
    pond::core::VoidResult result =
        GetWindowOperationResult(fake, "set-window-resizable", "SDL_SetWindowResizable", window);
    if (result.HasValue() && fake.applyWindowOperationEffects)
    {
        GetFakeWindow(window).resizable = resizable;
    }
    return result;
}

[[nodiscard]] pond::core::VoidResult FakeSetWindowAlwaysOnTop(
    void* context, BackendWindowHandle window, bool alwaysOnTop)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-window-always-on-top");
    pond::core::VoidResult result = GetWindowOperationResult(
        fake, "set-window-always-on-top", "SDL_SetWindowAlwaysOnTop", window);
    if (result.HasValue() && fake.applyWindowOperationEffects)
    {
        GetFakeWindow(window).alwaysOnTop = alwaysOnTop;
    }
    return result;
}

[[nodiscard]] pond::core::VoidResult FakeMinimizeWindow(
    void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("minimize-window");
    pond::core::VoidResult result =
        GetWindowOperationResult(fake, "minimize-window", "SDL_MinimizeWindow", window);
    if (result.HasValue() && fake.applyWindowOperationEffects)
    {
        FakeWindow& fakeWindow = GetFakeWindow(window);
        if (fakeWindow.visible)
        {
            fakeWindow.minimized = true;
            fakeWindow.maximized = false;
        }
        else
        {
            fakeWindow.pendingMinimized = true;
        }
    }
    return result;
}

[[nodiscard]] pond::core::VoidResult FakeMaximizeWindow(
    void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("maximize-window");
    pond::core::VoidResult result =
        GetWindowOperationResult(fake, "maximize-window", "SDL_MaximizeWindow", window);
    if (result.HasValue() && fake.applyWindowOperationEffects)
    {
        FakeWindow& fakeWindow = GetFakeWindow(window);
        if (fakeWindow.visible)
        {
            fakeWindow.minimized = false;
            fakeWindow.maximized = true;
        }
        else
        {
            fakeWindow.pendingMaximized = true;
        }
    }
    return result;
}

[[nodiscard]] pond::core::VoidResult FakeRestoreWindow(
    void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("restore-window");
    pond::core::VoidResult result =
        GetWindowOperationResult(fake, "restore-window", "SDL_RestoreWindow", window);
    if (result.HasValue() && fake.applyWindowOperationEffects)
    {
        FakeWindow& fakeWindow = GetFakeWindow(window);
        if (fakeWindow.visible)
        {
            fakeWindow.minimized = false;
            fakeWindow.maximized = false;
        }
        else
        {
            fakeWindow.pendingMinimized = false;
            fakeWindow.pendingMaximized = false;
        }
    }
    return result;
}
[[nodiscard]] bool FakeStartWindowTextInput(void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("start-text-input");
    if (FailWindowOperation(fake, "start-text-input"))
    {
        return false;
    }

    GetFakeWindow(window).textInputActive = true;
    return true;
}

[[nodiscard]] bool FakeStopWindowTextInput(void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("stop-text-input");
    if (FailWindowOperation(fake, "stop-text-input"))
    {
        return false;
    }

    GetFakeWindow(window).textInputActive = false;
    return true;
}

[[nodiscard]] bool FakeIsWindowTextInputActive(void* context, BackendWindowHandle window)
{
    GetFake(context).calls.emplace_back("is-text-input-active");
    return GetFakeWindow(window).textInputActive;
}

[[nodiscard]] bool FakeClearWindowTextComposition(void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("clear-text-composition");
    if (FailWindowOperation(fake, "clear-text-composition"))
    {
        return false;
    }

    ++GetFakeWindow(window).clearedTextCompositions;
    return true;
}

[[nodiscard]] bool FakeSetWindowTextInputArea(
    void* context, BackendWindowHandle window, std::optional<BackendTextInputArea> area)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-text-input-area");
    if (FailWindowOperation(fake, "set-text-input-area"))
    {
        return false;
    }

    GetFakeWindow(window).textInputArea = area;
    return true;
}
[[nodiscard]] bool FakeSetWindowMouseGrab(void* context, BackendWindowHandle window, bool grabbed)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-window-mouse-grab");
    if (FailWindowOperation(fake, "set-window-mouse-grab"))
    {
        return false;
    }

    if (fake.applyWindowOperationEffects)
    {
        GetFakeWindow(window).mouseGrabbed = grabbed;
    }
    return true;
}

[[nodiscard]] bool FakeIsWindowMouseGrabbed(void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("is-window-mouse-grabbed");
    return GetFakeWindow(window).mouseGrabbed;
}

[[nodiscard]] bool FakeSetWindowRelativeMouseMode(
    void* context, BackendWindowHandle window, bool enabled)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("set-window-relative-mouse-mode");
    if (FailWindowOperation(fake, "set-window-relative-mouse-mode"))
    {
        return false;
    }

    if (fake.applyWindowOperationEffects)
    {
        GetFakeWindow(window).relativeMouseModeEnabled = enabled;
    }
    return true;
}

[[nodiscard]] bool FakeIsWindowRelativeMouseModeEnabled(void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("is-window-relative-mouse-mode-enabled");
    return GetFakeWindow(window).relativeMouseModeEnabled;
}

[[nodiscard]] pond::core::Result<pond::platform::NativeWindowHandle> FakeGetNativeWindowHandle(
    void* context, BackendWindowHandle window)
{
    FakeRuntimeBackendState& fake = GetFake(context);
    fake.calls.emplace_back("get-native-handle");

    pond::core::VoidResult operationResult =
        GetWindowOperationResult(fake, "get-native-handle", "get-native-handle", window);
    RETURN_ERROR_IF_FAILED(operationResult);

    const auto makeError =
        [window](pond::platform::PlatformErrorCode code, std::string_view message)
    {
        return pond::core::Result<pond::platform::NativeWindowHandle>::FromError(
            pond::core::Error{pond::platform::ToErrorCode(code),
                              std::format("{} Context: {}.", message,
                                          GetFakeBackendWindowContext(window))});
    };

    switch (pond::platform::detail::GetNativeWindowDriver(fake.nativeVideoDriver))
    {
    case BackendNativeWindowDriver::Win32:
        if (fake.nativeWin32Instance == nullptr || fake.nativeWin32Window == nullptr)
        {
            return makeError(pond::platform::PlatformErrorCode::BackendFailure,
                             "missing fake Win32 data");
        }
        return pond::platform::NativeWindowHandle{pond::platform::NativeWin32Window{
            .instance = fake.nativeWin32Instance, .window = fake.nativeWin32Window}};
    case BackendNativeWindowDriver::X11:
        if (fake.nativeX11Display == nullptr || fake.nativeX11Window == 0)
        {
            return makeError(pond::platform::PlatformErrorCode::BackendFailure,
                             "missing fake X11 data");
        }
        return pond::platform::NativeWindowHandle{pond::platform::NativeX11Window{
            .display = fake.nativeX11Display, .window = fake.nativeX11Window}};
    case BackendNativeWindowDriver::Wayland:
        if (fake.nativeWaylandDisplay == nullptr || fake.nativeWaylandSurface == nullptr)
        {
            return makeError(pond::platform::PlatformErrorCode::BackendFailure,
                             "missing fake Wayland data");
        }
        return pond::platform::NativeWindowHandle{pond::platform::NativeWaylandWindow{
            .display = fake.nativeWaylandDisplay, .surface = fake.nativeWaylandSurface}};
    case BackendNativeWindowDriver::Unsupported:
        return makeError(pond::platform::PlatformErrorCode::Unsupported,
                         "synthetic unsupported video driver");
    }

    return makeError(pond::platform::PlatformErrorCode::Unsupported,
                     "synthetic unsupported video driver");
}

class FakeDisplayBackend final : public IPlatformDisplayBackend
{
public:
    explicit FakeDisplayBackend(FakeRuntimeBackendState& state) noexcept : m_state(state) {}

    [[nodiscard]] pond::core::Result<std::vector<std::uint32_t>> Enumerate() override
    {
        ++m_state.enumerateDisplayCalls;
        m_state.calls.emplace_back("enumerate-displays");
        if (FailDisplayOperation(m_state, "enumerate-displays"))
        {
            return pond::core::Result<std::vector<std::uint32_t>>::FromError(
                CaptureFailure("SDL_GetDisplays", "displays"));
        }

        return m_state.connectedDisplayIds;
    }

    [[nodiscard]] pond::core::Result<std::string> GetName(
        std::uint32_t backendDisplayId) override
    {
        m_state.calls.emplace_back("get-display-name");
        if (FailDisplayOperation(m_state, "get-display-name"))
        {
            return pond::core::Result<std::string>::FromError(
                CaptureFailure("SDL_GetDisplayName", GetDisplayContext(backendDisplayId)));
        }
        return GetFakeDisplay(m_state, backendDisplayId).name;
    }

    [[nodiscard]] pond::core::Result<BackendScreenRectangle> GetBounds(
        std::uint32_t backendDisplayId) override
    {
        m_state.calls.emplace_back("get-display-bounds");
        if (FailDisplayOperation(m_state, "get-display-bounds"))
        {
            return pond::core::Result<BackendScreenRectangle>::FromError(
                CaptureFailure("SDL_GetDisplayBounds", GetDisplayContext(backendDisplayId)));
        }
        return GetFakeDisplay(m_state, backendDisplayId).bounds;
    }

    [[nodiscard]] pond::core::Result<BackendScreenRectangle> GetUsableBounds(
        std::uint32_t backendDisplayId) override
    {
        m_state.calls.emplace_back("get-display-usable-bounds");
        if (FailDisplayOperation(m_state, "get-display-usable-bounds"))
        {
            return pond::core::Result<BackendScreenRectangle>::FromError(
                CaptureFailure("SDL_GetDisplayUsableBounds",
                               GetDisplayContext(backendDisplayId)));
        }
        return GetFakeDisplay(m_state, backendDisplayId).usableBounds;
    }

    [[nodiscard]] pond::core::Result<float> GetCurrentRefreshRate(
        std::uint32_t backendDisplayId) override
    {
        m_state.calls.emplace_back("get-current-display-refresh-rate");
        if (FailDisplayOperation(m_state, "get-current-display-refresh-rate"))
        {
            return pond::core::Result<float>::FromError(
                CaptureFailure("SDL_GetCurrentDisplayMode",
                               GetDisplayContext(backendDisplayId)));
        }
        return GetFakeDisplay(m_state, backendDisplayId).refreshRateHertz;
    }

    [[nodiscard]] BackendDisplayOrientation GetCurrentOrientation(
        std::uint32_t backendDisplayId) override
    {
        m_state.calls.emplace_back("get-current-display-orientation");
        return GetFakeDisplay(m_state, backendDisplayId).orientation;
    }

    [[nodiscard]] pond::core::Result<float> GetContentScale(
        std::uint32_t backendDisplayId) override
    {
        m_state.calls.emplace_back("get-display-content-scale");
        if (FailDisplayOperation(m_state, "get-display-content-scale"))
        {
            return pond::core::Result<float>::FromError(
                CaptureFailure("SDL_GetDisplayContentScale",
                               GetDisplayContext(backendDisplayId)));
        }
        return GetFakeDisplay(m_state, backendDisplayId).contentScale;
    }

    [[nodiscard]] pond::core::Result<std::uint32_t> GetForWindow(
        BackendWindowHandle window) override
    {
        m_state.calls.emplace_back("get-display-for-window");
        if (FailDisplayOperation(m_state, "get-display-for-window"))
        {
            return pond::core::Result<std::uint32_t>::FromError(
                CaptureFailure("SDL_GetDisplayForWindow",
                               GetFakeBackendWindowContext(window)));
        }
        return GetFakeWindow(window).backendDisplayId;
    }

    [[nodiscard]] pond::core::Result<float> GetWindowPixelDensity(
        BackendWindowHandle window) override
    {
        m_state.calls.emplace_back("get-window-pixel-density");
        if (FailDisplayOperation(m_state, "get-window-pixel-density"))
        {
            return pond::core::Result<float>::FromError(
                CaptureFailure("SDL_GetWindowPixelDensity",
                               GetFakeBackendWindowContext(window)));
        }
        return GetFakeWindow(window).pixelDensity;
    }

    [[nodiscard]] pond::core::Result<float> GetWindowDisplayScale(
        BackendWindowHandle window) override
    {
        m_state.calls.emplace_back("get-window-display-scale");
        if (FailDisplayOperation(m_state, "get-window-display-scale"))
        {
            return pond::core::Result<float>::FromError(
                CaptureFailure("SDL_GetWindowDisplayScale",
                               GetFakeBackendWindowContext(window)));
        }
        return GetFakeWindow(window).displayScale;
    }

private:
    [[nodiscard]] static pond::core::Error CaptureFailure(
        std::string_view operation, std::string_view context)
    {
        return pond::platform::detail::CaptureSdlFailure(
            pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure),
            operation, context);
    }

    [[nodiscard]] static std::string GetDisplayContext(std::uint32_t backendDisplayId)
    {
        return std::format("backend display {}", backendDisplayId);
    }

    FakeRuntimeBackendState& m_state;
};

class FakeDisplayBackendFactory final : public IPlatformDisplayBackendFactory
{
public:
    explicit FakeDisplayBackendFactory(FakeRuntimeBackendState& state) noexcept : m_state(state) {}

    [[nodiscard]] std::unique_ptr<IPlatformDisplayBackend> Create() const override
    {
        return std::make_unique<FakeDisplayBackend>(m_state);
    }

private:
    FakeRuntimeBackendState& m_state;
};
class FakeHintBackend final : public IHintBackend
{
public:
    explicit FakeHintBackend(FakeRuntimeBackendState& state) noexcept : m_state(state)
    {
    }

    [[nodiscard]] bool IsMainThread() const noexcept override
    {
        return FakeIsMainThread(&m_state);
    }

    [[nodiscard]] bool HasInitializedSubsystems() const noexcept override
    {
        return FakeHasInitializedSubsystems(&m_state);
    }

    [[nodiscard]] const char* GetHint(const char* name) const noexcept override
    {
        return FakeGetHint(&m_state, name);
    }

    [[nodiscard]] pond::core::VoidResult SetHintOverride(const char* name,
                                                         const char* value) override
    {
        return FakeSetHintOverride(&m_state, name, value);
    }

    [[nodiscard]] pond::core::VoidResult ResetHint(const char* name) override
    {
        return FakeResetHint(&m_state, name);
    }

private:
    FakeRuntimeBackendState& m_state;
};

class FakeRuntimeBackend final : public IPlatformRuntimeBackend
{
public:
    explicit FakeRuntimeBackend(FakeRuntimeBackendState& state)
        : m_state(state), m_hintBackend(state),
          m_hintManager(HintManagerAccess::Create(m_hintBackend))
    {
    }

    [[nodiscard]] pond::platform::HintManager& GetHintManager() noexcept override
    {
        return m_hintManager;
    }

    [[nodiscard]] bool IsMainThread() override
    {
        return FakeIsMainThread(&m_state);
    }

    [[nodiscard]] bool HasInitializedSubsystems() override
    {
        return FakeHasInitializedSubsystems(&m_state);
    }

    [[nodiscard]] bool HasExpectedRuntimeSubsystems() override
    {
        return FakeHasExpectedRuntimeSubsystems(&m_state);
    }

    [[nodiscard]] pond::core::VoidResult SetAppMetadataProperty(
        ApplicationMetadataProperty property, const char* value) override
    {
        return FakeSetAppMetadataProperty(&m_state, property, value);
    }

    [[nodiscard]] pond::core::VoidResult InitializeVideo() override
    {
        return FakeInitializeVideo(&m_state);
    }

    void Quit() override
    {
        FakeQuit(&m_state);
    }

    [[nodiscard]] std::uint64_t GetTicksNanoseconds() override
    {
        return FakeGetTicksNanoseconds(&m_state);
    }

    [[nodiscard]] bool PollEvent(SDL_Event* event) override
    {
        return FakePollEvent(&m_state, event);
    }

    [[nodiscard]] bool SupportsGlobalMouse() override
    {
        return FakeSupportsGlobalMouse(&m_state);
    }

    [[nodiscard]] pond::platform::MousePosition GetGlobalMousePosition() override
    {
        return FakeGetGlobalMousePosition(&m_state);
    }

    [[nodiscard]] pond::core::VoidResult SetMouseCapture(bool enabled) override
    {
        return FakeSetMouseCapture(&m_state, enabled);
    }

    [[nodiscard]] pond::core::Result<CursorHandle> CreateSystemCursor(
        pond::platform::SystemCursorShape shape) override
    {
        return FakeCreateSystemCursor(&m_state, shape);
    }

    [[nodiscard]] pond::core::VoidResult SetCursor(CursorHandle cursor) override
    {
        return FakeSetCursor(&m_state, cursor);
    }

    void DestroyCursor(CursorHandle cursor) override
    {
        FakeDestroyCursor(&m_state, cursor);
    }

    [[nodiscard]] pond::core::VoidResult ShowCursor() override
    {
        return FakeShowCursor(&m_state);
    }

    [[nodiscard]] pond::core::VoidResult HideCursor() override
    {
        return FakeHideCursor(&m_state);
    }

    [[nodiscard]] bool IsCursorVisible() override
    {
        return FakeIsCursorVisible(&m_state);
    }

    [[nodiscard]] bool SupportsClipboardText() override
    {
        return FakeSupportsClipboardText(&m_state);
    }

    [[nodiscard]] pond::core::Result<std::string> GetClipboardText() override
    {
        return FakeGetClipboardText(&m_state);
    }

    [[nodiscard]] pond::core::VoidResult SetClipboardText(std::string_view text) override
    {
        return FakeSetClipboardText(&m_state, text);
    }

    [[nodiscard]] pond::core::VoidResult OpenExternalUri(std::string_view uri) override
    {
        return FakeOpenExternalUri(&m_state, uri);
    }

    void ShowDialog(const BackendDialogRequestDesc& desc) override
    {
        FakeShowDialog(&m_state, desc);
    }

private:
    FakeRuntimeBackendState& m_state;
    FakeHintBackend m_hintBackend;
    pond::platform::HintManager m_hintManager;
};

class FakeRuntimeBackendFactory final : public IPlatformRuntimeBackendFactory
{
public:
    explicit FakeRuntimeBackendFactory(FakeRuntimeBackendState& state) noexcept : m_state(state)
    {
    }

    [[nodiscard]] std::unique_ptr<IPlatformRuntimeBackend> Create() const override
    {
        return std::make_unique<FakeRuntimeBackend>(m_state);
    }

private:
    FakeRuntimeBackendState& m_state;
};

class FakeWindowBackend final : public IPlatformWindowBackend
{
public:
    explicit FakeWindowBackend(FakeRuntimeBackendState& state) noexcept : m_state(state) {}

    [[nodiscard]] pond::core::Result<BackendWindowHandle> Create(
        const BackendWindowCreateDesc& desc) override
    {
        const BackendWindowHandle window = FakeCreateWindow(&m_state, desc);
        if (!window.IsValid())
        {
            return pond::core::Result<BackendWindowHandle>::FromError(
                CaptureFakeWindowFailure(pond::platform::PlatformErrorCode::BackendFailure,
                                         "SDL_CreateWindow"));
        }
        return window;
    }

    void Destroy(BackendWindowHandle window) noexcept override
    {
        FakeDestroyWindow(&m_state, window);
    }

    [[nodiscard]] pond::core::Result<std::uint32_t> GetId(
        BackendWindowHandle window) override
    {
        const std::uint32_t id = FakeGetWindowId(&m_state, window);
        if (id == 0)
        {
            return pond::core::Result<std::uint32_t>::FromError(
                CaptureFakeWindowFailure(pond::platform::PlatformErrorCode::BackendFailure,
                                         "SDL_GetWindowID", window));
        }
        return id;
    }

    [[nodiscard]] std::string GetTitle(BackendWindowHandle window) override
    {
        return FakeGetWindowTitle(&m_state, window);
    }

    [[nodiscard]] pond::core::VoidResult SetTitle(
        BackendWindowHandle window, std::string_view title) override
    {
        return ToFakeWindowResult(FakeSetWindowTitle(&m_state, window, title),
                                  "SDL_SetWindowTitle", window);
    }

    [[nodiscard]] pond::core::Result<BackendWindowPosition> GetPosition(
        BackendWindowHandle window) override
    {
        return ToFakeWindowResult(FakeGetWindowPosition(&m_state, window),
                                  "SDL_GetWindowPosition", window);
    }

    [[nodiscard]] pond::core::VoidResult SetPosition(
        BackendWindowHandle window, BackendWindowPosition position) override
    {
        return ToFakeWindowResult(FakeSetWindowPosition(&m_state, window, position),
                                  "SDL_SetWindowPosition", window);
    }

    [[nodiscard]] pond::core::Result<BackendWindowLogicalSize> GetSize(
        BackendWindowHandle window) override
    {
        return ToFakeWindowResult(FakeGetWindowSize(&m_state, window),
                                  "SDL_GetWindowSize", window);
    }

    [[nodiscard]] pond::core::Result<BackendWindowPixelSize> GetSizeInPixels(
        BackendWindowHandle window) override
    {
        return ToFakeWindowResult(FakeGetWindowSizeInPixels(&m_state, window),
                                  "SDL_GetWindowSizeInPixels", window);
    }

    [[nodiscard]] pond::core::VoidResult SetSize(
        BackendWindowHandle window, BackendWindowLogicalSize size) override
    {
        return ToFakeWindowResult(FakeSetWindowSize(&m_state, window, size),
                                  "SDL_SetWindowSize", window);
    }

    [[nodiscard]] pond::core::VoidResult SetMinimumSize(
        BackendWindowHandle window, BackendWindowLogicalSize size) override
    {
        return ToFakeWindowResult(FakeSetWindowMinimumSize(&m_state, window, size),
                                  "SDL_SetWindowMinimumSize", window);
    }

    [[nodiscard]] pond::core::VoidResult Show(BackendWindowHandle window) override
    {
        return ToFakeWindowResult(FakeShowWindow(&m_state, window), "SDL_ShowWindow", window);
    }

    [[nodiscard]] pond::core::VoidResult Hide(BackendWindowHandle window) override
    {
        return ToFakeWindowResult(FakeHideWindow(&m_state, window), "SDL_HideWindow", window);
    }

    [[nodiscard]] pond::core::Result<BackendWindowProperties> GetProperties(
        BackendWindowHandle window) override
    {
        return ToFakeWindowResult(FakeGetWindowProperties(&m_state, window),
                                  "SDL_GetWindowFlags", window);
    }

    [[nodiscard]] pond::core::VoidResult SetFullscreenModeToDesktop(
        BackendWindowHandle window) override
    {
        return FakeSetFullscreenModeToDesktop(&m_state, window);
    }

    [[nodiscard]] pond::core::VoidResult SetFullscreen(
        BackendWindowHandle window, bool fullscreen) override
    {
        return FakeSetWindowFullscreen(&m_state, window, fullscreen);
    }

    [[nodiscard]] pond::core::VoidResult SetBordered(
        BackendWindowHandle window, bool bordered) override
    {
        return FakeSetWindowBordered(&m_state, window, bordered);
    }

    [[nodiscard]] pond::core::VoidResult SetResizable(
        BackendWindowHandle window, bool resizable) override
    {
        return FakeSetWindowResizable(&m_state, window, resizable);
    }

    [[nodiscard]] pond::core::VoidResult SetAlwaysOnTop(
        BackendWindowHandle window, bool alwaysOnTop) override
    {
        return FakeSetWindowAlwaysOnTop(&m_state, window, alwaysOnTop);
    }

    [[nodiscard]] pond::core::VoidResult Minimize(BackendWindowHandle window) override
    {
        return FakeMinimizeWindow(&m_state, window);
    }

    [[nodiscard]] pond::core::VoidResult Maximize(BackendWindowHandle window) override
    {
        return FakeMaximizeWindow(&m_state, window);
    }

    [[nodiscard]] pond::core::VoidResult Restore(BackendWindowHandle window) override
    {
        return FakeRestoreWindow(&m_state, window);
    }

    [[nodiscard]] pond::core::VoidResult StartTextInput(
        BackendWindowHandle window) override
    {
        return ToFakeWindowResult(FakeStartWindowTextInput(&m_state, window),
                                  "SDL_StartTextInput", window);
    }

    [[nodiscard]] pond::core::VoidResult StopTextInput(
        BackendWindowHandle window) override
    {
        return ToFakeWindowResult(FakeStopWindowTextInput(&m_state, window),
                                  "SDL_StopTextInput", window);
    }

    [[nodiscard]] bool IsTextInputActive(BackendWindowHandle window) override
    {
        return FakeIsWindowTextInputActive(&m_state, window);
    }

    [[nodiscard]] pond::core::VoidResult ClearTextComposition(
        BackendWindowHandle window) override
    {
        return ToFakeWindowResult(FakeClearWindowTextComposition(&m_state, window),
                                  "SDL_ClearComposition", window);
    }

    [[nodiscard]] pond::core::VoidResult SetTextInputArea(
        BackendWindowHandle window, std::optional<BackendTextInputArea> area) override
    {
        return ToFakeWindowResult(
            FakeSetWindowTextInputArea(&m_state, window, std::move(area)),
            "SDL_SetTextInputArea", window);
    }

    [[nodiscard]] pond::core::VoidResult SetMouseGrab(
        BackendWindowHandle window, bool grabbed) override
    {
        return ToFakeWindowResult(FakeSetWindowMouseGrab(&m_state, window, grabbed),
                                  "SDL_SetWindowMouseGrab", window);
    }

    [[nodiscard]] bool IsMouseGrabbed(BackendWindowHandle window) override
    {
        return FakeIsWindowMouseGrabbed(&m_state, window);
    }

    [[nodiscard]] pond::core::VoidResult SetRelativeMouseMode(
        BackendWindowHandle window, bool enabled) override
    {
        if (FakeIsWindowRelativeMouseModeEnabled(&m_state, window) == enabled)
        {
            return pond::core::VoidResult::Success();
        }

        RETURN_ERROR_IF_FAILED(ToFakeWindowResult(
            FakeSetWindowRelativeMouseMode(&m_state, window, enabled),
            "SDL_SetWindowRelativeMouseMode", window));

        if (FakeIsWindowRelativeMouseModeEnabled(&m_state, window) != enabled)
        {
            return pond::core::VoidResult::FromError(pond::core::Error{
                pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported),
                std::format("SDL_SetWindowRelativeMouseMode did not apply the requested state "
                            "for {}; the backend does not support this operation.",
                            GetFakeBackendWindowContext(window))});
        }

        return pond::core::VoidResult::Success();
    }

    [[nodiscard]] bool IsRelativeMouseModeEnabled(
        BackendWindowHandle window) override
    {
        return FakeIsWindowRelativeMouseModeEnabled(&m_state, window);
    }

    [[nodiscard]] pond::core::Result<pond::platform::NativeWindowHandle> GetNativeHandle(
        BackendWindowHandle window) override
    {
        return FakeGetNativeWindowHandle(&m_state, window);
    }

private:
    FakeRuntimeBackendState& m_state;
};
class FakeWindowBackendFactory final : public IPlatformWindowBackendFactory
{
public:
    explicit FakeWindowBackendFactory(FakeRuntimeBackendState& state) noexcept : m_state(state) {}

    [[nodiscard]] std::unique_ptr<IPlatformWindowBackend> Create() const override
    {
        return std::make_unique<FakeWindowBackend>(m_state);
    }

private:
    FakeRuntimeBackendState& m_state;
};

class PlatformRuntimeBackendTests : public testing::Test
{
protected:
    void SetUp() override
    {
        static_cast<void>(SDL_ClearError());
        pond::platform::detail::SetPlatformRuntimeBackendForTesting(&m_backendFactory);
        pond::platform::detail::SetPlatformWindowBackendForTesting(&m_windowBackendFactory);
        pond::platform::detail::SetPlatformDisplayBackendForTesting(
            &m_displayBackendFactory);
    }

    void TearDown() override
    {
        pond::platform::detail::SetPlatformDisplayBackendForTesting(nullptr);
        pond::platform::detail::SetPlatformWindowBackendForTesting(nullptr);
        pond::platform::detail::SetPlatformRuntimeBackendForTesting(nullptr);
        static_cast<void>(SDL_ClearError());
    }

    FakeRuntimeBackendState m_fake;
    FakeRuntimeBackendFactory m_backendFactory{m_fake};
    FakeWindowBackendFactory m_windowBackendFactory{m_fake};
    FakeDisplayBackendFactory m_displayBackendFactory{m_fake};
};

TEST(ApplicationMetadataPropertyTests, FormatsAsErrorContext)
{
    EXPECT_EQ(std::format("{}", ApplicationMetadataProperty::Name), "name");
    EXPECT_EQ(std::format("{}", ApplicationMetadataProperty::Version), "version");
    EXPECT_EQ(std::format("{}", ApplicationMetadataProperty::Identifier), "identifier");
}

TEST_F(PlatformRuntimeBackendTests, AppliesMetadataWithoutChangingMouseHintsAndProvidesTimestamps)
{
    m_fake.hints[SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH] = "previous";
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)] = "Prior App";
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)] = "1.0";
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)] = "org.ponder.prior";
    m_fake.ticks = 987'654'321;

    const pond::platform::PlatformRuntimeDesc desc{.applicationName = "Ponder Workbench",
                                                   .applicationVersion = std::string{"2.4.1"},
                                                   .applicationIdentifier =
                                                       std::string{"org.ponder.workbench"}};

    {
        auto result = pond::platform::PlatformRuntime::Create(desc);
        ASSERT_TRUE(result.HasValue());
        pond::platform::PlatformRuntime runtime = std::move(result).GetValue();

        EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)],
                  "Ponder Workbench");
        EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)], "2.4.1");
        EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)],
                  "org.ponder.workbench");
        EXPECT_EQ(GetHintValue(m_fake, SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH),
                  std::string_view{"previous"});
        EXPECT_EQ(GetHintValue(m_fake, SDL_HINT_MOUSE_AUTO_CAPTURE), nullptr);
        EXPECT_EQ(m_fake.hints.size(), 1U);
        EXPECT_TRUE(m_fake.metadataObservedDuringInitialization);
        EXPECT_EQ(runtime.Now().GetTimeSinceEpoch(), std::chrono::nanoseconds{987'654'321});
    }

    EXPECT_EQ(m_fake.quitCalls, 1);
    EXPECT_EQ(GetHintValue(m_fake, SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH), nullptr);
    EXPECT_EQ(GetHintValue(m_fake, SDL_HINT_MOUSE_AUTO_CAPTURE), nullptr);
    EXPECT_FALSE(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)].has_value());
    EXPECT_FALSE(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)].has_value());
    EXPECT_FALSE(
        m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)].has_value());
    EXPECT_EQ(m_fake.runtimeSubsystemChecks, 1);
}

TEST_F(PlatformRuntimeBackendTests, ConfiguresHintsBeforeInitializationAndExposesManager)
{
    using pond::platform::PlatformErrorCode;

    pond::platform::PlatformRuntimeDesc desc;
    desc.configureHintsBeforeInitialization = [](pond::platform::HintManager& hintManager)
    {
        using namespace pond::platform::hints;

        EXPECT_TRUE(hintManager.PushHint(VideoDriver{"dummy"}).HasValue());
        EXPECT_TRUE(
            hintManager.PushHint(ImeImplementedUi{ImeUiCapabilities::CompositionAndCandidates})
                .HasValue());
        EXPECT_TRUE(hintManager.PushHint(MouseFocusClickThrough{false}).HasValue());
    };

    {
        auto result = pond::platform::PlatformRuntime::Create(desc);
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        pond::platform::PlatformRuntime runtime = std::move(result).GetValue();
        pond::platform::HintManager& hintManager = runtime.GetHintManager();

        using namespace pond::platform::hints;

        EXPECT_EQ(hintManager.GetHint(VideoDriver{}).GetValue().value, "dummy");
        EXPECT_EQ(hintManager.GetHint(ImeImplementedUi{}).GetValue().value,
                  ImeUiCapabilities::CompositionAndCandidates);
        EXPECT_FALSE(hintManager.GetHint(MouseFocusClickThrough{}).GetValue().value);

        const auto lateVideoDriver = hintManager.PushHint(VideoDriver{"offscreen"});
        ASSERT_FALSE(lateVideoDriver.HasValue());
        EXPECT_EQ(lateVideoDriver.GetError().GetCode(),
                  pond::platform::ToErrorCode(PlatformErrorCode::InvalidArgument));

        ASSERT_TRUE(hintManager.PushHint(WindowActivateWhenShown{false}).HasValue());
        EXPECT_FALSE(hintManager.GetHint(WindowActivateWhenShown{}).GetValue().value);
        EXPECT_TRUE(hintManager.ClearHints(WindowActivateWhenShown{}).HasValue());

        const auto callbackCall = std::ranges::find(
            m_fake.calls, std::string{"set-hint:"} + SDL_HINT_VIDEO_DRIVER + "=dummy");
        const auto initializationCall = std::ranges::find(m_fake.calls, "initialize-video");
        ASSERT_NE(callbackCall, m_fake.calls.end());
        ASSERT_NE(initializationCall, m_fake.calls.end());
        EXPECT_LT(callbackCall, initializationCall);
    }

    EXPECT_FALSE(m_fake.hints.contains(SDL_HINT_VIDEO_DRIVER));
    EXPECT_FALSE(m_fake.hints.contains(SDL_HINT_IME_IMPLEMENTED_UI));
    EXPECT_FALSE(m_fake.hints.contains(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH));
}
TEST_F(PlatformRuntimeBackendTests, RestoresAOriginallyPresentEmptyHint)
{
    m_fake.hints[SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH] = "";

    {
        auto result =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(result.HasValue());
        pond::platform::PlatformRuntime runtime = std::move(result).GetValue();
        ASSERT_TRUE(runtime.GetHintManager()
                        .PushHint(pond::platform::hints::MouseFocusClickThrough{true})
                        .HasValue());
    }

    const auto iterator = m_fake.hints.find(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH);
    ASSERT_NE(iterator, m_fake.hints.end());
    EXPECT_TRUE(iterator->second.empty());
}

TEST_F(PlatformRuntimeBackendTests, ClearsAbsentOptionalMetadata)
{
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)] = "prior-version";
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)] = "org.ponder.prior";

    {
        auto result =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(result.HasValue());
        pond::platform::PlatformRuntime runtime = std::move(result).GetValue();

        EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)], "ponder");
        EXPECT_FALSE(
            m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)].has_value());
        EXPECT_FALSE(
            m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)].has_value());
    }

    EXPECT_FALSE(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)].has_value());
    EXPECT_FALSE(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)].has_value());
    EXPECT_FALSE(
        m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)].has_value());
}

TEST_F(PlatformRuntimeBackendTests, DuplicateCreationDoesNoBackendWork)
{
    auto firstResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(firstResult.HasValue());
    pond::platform::PlatformRuntime first = std::move(firstResult).GetValue();
    const std::size_t callCount = m_fake.calls.size();

    pond::platform::PlatformRuntimeDesc invalidDesc;
    invalidDesc.applicationName.clear();
    const auto duplicateResult = pond::platform::PlatformRuntime::Create(invalidDesc);

    ASSERT_FALSE(duplicateResult.HasValue());
    EXPECT_EQ(duplicateResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::RuntimeAlreadyActive));
    EXPECT_EQ(m_fake.calls.size(), callCount);
}

TEST_F(PlatformRuntimeBackendTests, CompleteShutdownAllowsAnotherRuntime)
{
    {
        auto firstResult =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(firstResult.HasValue());
        pond::platform::PlatformRuntime first = std::move(firstResult).GetValue();
    }

    auto secondResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(secondResult.HasValue());
    pond::platform::PlatformRuntime second = std::move(secondResult).GetValue();

    EXPECT_EQ(m_fake.initializeVideoCalls, 2);
    EXPECT_EQ(m_fake.quitCalls, 1);
}

TEST_F(PlatformRuntimeBackendTests, RejectsInvalidMetadataBeforeCallingBackend)
{
    pond::platform::PlatformRuntimeDesc emptyName;
    emptyName.applicationName.clear();
    auto result = pond::platform::PlatformRuntime::Create(emptyName);
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));

    pond::platform::PlatformRuntimeDesc emptyVersion;
    emptyVersion.applicationVersion = std::string{};
    result = pond::platform::PlatformRuntime::Create(emptyVersion);
    ASSERT_FALSE(result.HasValue());

    pond::platform::PlatformRuntimeDesc invalidUtf8;
    invalidUtf8.applicationIdentifier = std::string{"\xC0\xAF", 2};
    result = pond::platform::PlatformRuntime::Create(invalidUtf8);
    ASSERT_FALSE(result.HasValue());

    pond::platform::PlatformRuntimeDesc embeddedNull;
    embeddedNull.applicationName = std::string{"pon\0der", 7};
    result = pond::platform::PlatformRuntime::Create(embeddedNull);
    ASSERT_FALSE(result.HasValue());

    EXPECT_TRUE(m_fake.calls.empty());
}

TEST_F(PlatformRuntimeBackendTests, RejectsCreationOffTheBackendMainThread)
{
    m_fake.mainThread = false;

    EXPECT_THROW(static_cast<void>(pond::platform::PlatformRuntime::Create(
                     pond::platform::PlatformRuntimeDesc{})),
                 pond::core::PonderException);

    m_fake.mainThread = true;
    auto retry = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();
}

TEST_F(PlatformRuntimeBackendTests, RejectsWorkerThreadBeforeConsultingSdl)
{
    std::atomic_bool rejectedWorkerThread{false};
    std::thread worker{[&rejectedWorkerThread]()
                       {
                           try
                           {
                               static_cast<void>(pond::platform::PlatformRuntime::Create(
                                   pond::platform::PlatformRuntimeDesc{}));
                           }
                           catch (const pond::core::PonderException&)
                           {
                               rejectedWorkerThread.store(true);
                           }
                       }};
    worker.join();

    EXPECT_TRUE(rejectedWorkerThread.load());
    EXPECT_TRUE(m_fake.calls.empty());

    auto retry = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();
}

TEST_F(PlatformRuntimeBackendTests, RejectsExternallyInitializedSdl)
{
    m_fake.initializedSubsystems = true;

    auto result = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_EQ(m_fake.initializeVideoCalls, 0);
    EXPECT_EQ(m_fake.quitCalls, 0);

    m_fake.initializedSubsystems = false;
    auto retry = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();
}

TEST_F(PlatformRuntimeBackendTests, RestoresExplicitHintsAfterMutationFailureAndPermitsRetry)
{
    m_fake.hints[SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH] = "old-focus";
    m_fake.hints[SDL_HINT_MOUSE_AUTO_CAPTURE] = "old-capture";

    {
        auto runtimeResult =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(runtimeResult.HasValue());
        pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

        ASSERT_TRUE(runtime.GetHintManager()
                        .PushHint(pond::platform::hints::MouseFocusClickThrough{true})
                        .HasValue());

        m_fake.failingHint = SDL_HINT_MOUSE_AUTO_CAPTURE;
        m_fake.hintFailuresRemaining = 1;
        auto hintResult = runtime.GetHintManager().PushHint(
            pond::platform::hints::MouseAutoCapture{false});

        ASSERT_FALSE(hintResult.HasValue());
        EXPECT_EQ(
            hintResult.GetError().GetCode(),
            pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
        EXPECT_NE(hintResult.GetError().GetMessage().find("SDL_SetHintWithPriority"),
                  std::string_view::npos);
        EXPECT_NE(hintResult.GetError().GetMessage().find("synthetic hint failure"),
                  std::string_view::npos);
        EXPECT_EQ(GetHintValue(m_fake, SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH),
                  std::string_view{"1"});
        EXPECT_EQ(GetHintValue(m_fake, SDL_HINT_MOUSE_AUTO_CAPTURE),
                  std::string_view{"old-capture"});
        EXPECT_EQ(m_fake.initializeVideoCalls, 1);
    }

    EXPECT_EQ(GetHintValue(m_fake, SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH),
              std::string_view{"old-focus"});
    EXPECT_EQ(GetHintValue(m_fake, SDL_HINT_MOUSE_AUTO_CAPTURE), nullptr);

    auto retry = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();
}

TEST_F(PlatformRuntimeBackendTests, KeepsAppliedMetadataAfterFailureAndPermitsRetry)
{
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)] = "Prior App";
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)] = "1.0";
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)] = "org.ponder.prior";
    m_fake.failingMetadataProperty = ApplicationMetadataProperty::Version;
    m_fake.metadataFailuresRemaining = 1;

    auto result = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(result.GetError().GetMessage().find("synthetic metadata failure"),
              std::string_view::npos);
    EXPECT_NE(result.GetError().GetMessage().find("version"), std::string_view::npos);
    EXPECT_EQ(m_fake.initializeVideoCalls, 0);
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)], "ponder");
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)], "1.0");
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)],
              "org.ponder.prior");

    auto retry = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();

    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)], "ponder");
    EXPECT_FALSE(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)].has_value());
    EXPECT_FALSE(
        m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)].has_value());
}

TEST_F(PlatformRuntimeBackendTests, PropagatesInitializationFailureWithoutRestoringMetadata)
{
    m_fake.hints[SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH] = "old-focus";
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)] = "Prior App";
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)] = "1.0";
    m_fake.initializeVideoSucceeds = false;

    auto result = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(result.GetError().GetMessage().find("synthetic video initialization failure"),
              std::string_view::npos);
    EXPECT_EQ(m_fake.quitCalls, 1);
    EXPECT_EQ(GetHintValue(m_fake, SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH), nullptr);
    EXPECT_FALSE(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)].has_value());
    EXPECT_FALSE(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)].has_value());

    m_fake.initializeVideoSucceeds = true;
    auto retry = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();
}

TEST_F(PlatformRuntimeBackendTests, MovesOwnershipWithoutDuplicateShutdown)
{
    {
        auto result =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(result.HasValue());
        pond::platform::PlatformRuntime original = std::move(result).GetValue();

        pond::platform::PlatformRuntime moved = std::move(original);
        EXPECT_THROW(static_cast<void>(original.Now()), pond::core::PonderException);
        EXPECT_EQ(m_fake.quitCalls, 0);

        moved = std::move(moved);
        EXPECT_EQ(moved.Now().GetTimeSinceEpoch(), std::chrono::nanoseconds{m_fake.ticks});

        original = std::move(moved);
        EXPECT_THROW(static_cast<void>(moved.Now()), pond::core::PonderException);
        EXPECT_EQ(original.Now().GetTimeSinceEpoch(), std::chrono::nanoseconds{m_fake.ticks});
        EXPECT_EQ(m_fake.quitCalls, 0);
    }

    EXPECT_EQ(m_fake.quitCalls, 1);
}

TEST_F(PlatformRuntimeBackendTests, RejectsWrongThreadAndOutOfRangeTimestamps)
{
    auto result = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(result.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(result).GetValue();

    std::atomic_bool rejectedWrongThread{false};
    std::thread worker{[&runtime, &rejectedWrongThread]()
                       {
                           try
                           {
                               static_cast<void>(runtime.Now());
                           }
                           catch (const pond::core::PonderException&)
                           {
                               rejectedWrongThread.store(true);
                           }
                       }};
    worker.join();

    EXPECT_TRUE(rejectedWrongThread.load());
    EXPECT_EQ(m_fake.tickCalls, 0);

    m_fake.ticks = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1U;
    EXPECT_THROW(static_cast<void>(runtime.Now()), pond::core::PonderException);
    EXPECT_EQ(m_fake.tickCalls, 1);
}

TEST_F(PlatformRuntimeBackendTests, HoldsSingletonUntilHintRestorationCompletes)
{
    m_fake.hints[SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH] = "old-focus";
    m_fake.attemptCreateDuringRestoration = true;

    {
        auto result =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(result.HasValue());
        pond::platform::PlatformRuntime runtime = std::move(result).GetValue();
        ASSERT_TRUE(runtime.GetHintManager()
                        .PushHint(pond::platform::hints::MouseFocusClickThrough{true})
                        .HasValue());
    }

    ASSERT_TRUE(m_fake.attemptedCreateDuringRestoration);
    ASSERT_TRUE(m_fake.restorationCreateError.has_value());
    EXPECT_EQ(*m_fake.restorationCreateError,
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::RuntimeAlreadyActive));
}

TEST_F(PlatformRuntimeBackendTests, CreatesMultipleWindowsWithMonotonicIdsAndReusedBackendIds)
{
    m_fake.scriptedBackendWindowIds = {41, 73, 73, 99};

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    const pond::platform::WindowDesc desc{
        .title = "Hidden Test Window",
        .logicalSize = {640, 480},
        .visible = false,
        .resizable = false,
        .highPixelDensity = false,
        .minimumLogicalSize = pond::platform::LogicalSize{320, 240},
        .graphicsCompatibility = pond::platform::WindowGraphicsCompatibility::Default};

    auto firstResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(firstResult.HasValue());
    std::optional<pond::platform::Window> first;
    first.emplace(std::move(firstResult).GetValue());

    auto secondResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(secondResult.HasValue());
    std::optional<pond::platform::Window> second;
    second.emplace(std::move(secondResult).GetValue());

    EXPECT_EQ(first->GetId(), pond::platform::WindowId{1});
    EXPECT_EQ(second->GetId(), pond::platform::WindowId{2});
    ASSERT_EQ(m_fake.windows.size(), 2U);
    const FakeWindow& firstBackendWindow = m_fake.windows.front();
    EXPECT_EQ(firstBackendWindow.title, "Hidden Test Window");
    EXPECT_EQ(firstBackendWindow.width, 640);
    EXPECT_EQ(firstBackendWindow.height, 480);
    EXPECT_EQ(firstBackendWindow.minimumWidth, 320);
    EXPECT_EQ(firstBackendWindow.minimumHeight, 240);
    EXPECT_FALSE(firstBackendWindow.visible);
    EXPECT_FALSE(firstBackendWindow.resizable);
    EXPECT_FALSE(firstBackendWindow.highPixelDensity);
    EXPECT_EQ(firstBackendWindow.graphicsCompatibility,
              pond::platform::WindowGraphicsCompatibility::Default);

    second.reset();
    EXPECT_EQ(m_fake.destroyWindowCalls, 1);

    auto thirdResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(thirdResult.HasValue());
    std::optional<pond::platform::Window> third;
    third.emplace(std::move(thirdResult).GetValue());
    EXPECT_EQ(third->GetId(), pond::platform::WindowId{3});

    first.reset();
    third.reset();
    EXPECT_TRUE(m_fake.windows.empty());
    EXPECT_EQ(m_fake.destroyWindowCalls, 3);

    auto fourthResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(fourthResult.HasValue());
    pond::platform::Window fourth = std::move(fourthResult).GetValue();
    EXPECT_EQ(fourth.GetId(), pond::platform::WindowId{4});
}

TEST_F(PlatformRuntimeBackendTests, RestartsProjectWindowIdsForANewRuntime)
{
    pond::platform::WindowDesc desc;
    desc.visible = false;

    {
        auto runtimeResult =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(runtimeResult.HasValue());
        pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
        auto windowResult = runtime.CreateWindow(desc);
        ASSERT_TRUE(windowResult.HasValue());
        pond::platform::Window window = std::move(windowResult).GetValue();
        EXPECT_EQ(window.GetId(), pond::platform::WindowId{1});
    }

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    EXPECT_EQ(window.GetId(), pond::platform::WindowId{1});
}

TEST_F(PlatformRuntimeBackendTests, ValidatesWindowDescriptorsBeforeBackendCalls)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    const auto expectInvalid = [&runtime, this](const pond::platform::WindowDesc& desc)
    {
        const int callCount = m_fake.createWindowCalls;
        const auto result = runtime.CreateWindow(desc);
        EXPECT_FALSE(result.HasValue());
        if (!result.HasValue())
        {
            EXPECT_EQ(
                result.GetError().GetCode(),
                pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));
        }
        EXPECT_EQ(m_fake.createWindowCalls, callCount);
    };

    pond::platform::WindowDesc desc;
    desc.logicalSize.width = 0;
    expectInvalid(desc);

    desc = {};
    desc.logicalSize.height = 0;
    expectInvalid(desc);

    desc = {};
    desc.logicalSize.width = std::numeric_limits<std::uint32_t>::max();
    expectInvalid(desc);

    desc = {};
    desc.minimumLogicalSize = pond::platform::LogicalSize{0, 100};
    expectInvalid(desc);

    desc = {};
    desc.minimumLogicalSize =
        pond::platform::LogicalSize{100, std::numeric_limits<std::uint32_t>::max()};
    expectInvalid(desc);

    desc = {};
    desc.title = std::string{"\xC0\xAF", 2};
    expectInvalid(desc);

    desc = {};
    desc.title = std::string{"bad\0title", 9};
    expectInvalid(desc);

    desc = {};
    desc.graphicsCompatibility = static_cast<pond::platform::WindowGraphicsCompatibility>(0xFF);
    expectInvalid(desc);

    desc = {};
    desc.title.clear();
    desc.visible = false;
    auto emptyTitleResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(emptyTitleResult.HasValue());
    pond::platform::Window emptyTitleWindow = std::move(emptyTitleResult).GetValue();
    EXPECT_TRUE(emptyTitleWindow.GetTitle().empty());
}

TEST_F(PlatformRuntimeBackendTests, RollsBackEveryWindowCreationFailure)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    m_fake.destroyOverwritesError = true;

    const auto expectFailure = [&runtime, this](std::string operation,
                                                pond::platform::WindowDesc desc,
                                                int expectedDestroyCalls)
    {
        m_fake.failingWindowOperation = operation;
        m_fake.windowFailuresRemaining = 1;
        auto result = runtime.CreateWindow(desc);
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
        EXPECT_NE(result.GetError().GetMessage().find("synthetic " + operation),
                  std::string_view::npos);
        EXPECT_EQ(m_fake.destroyWindowCalls, expectedDestroyCalls);
        EXPECT_TRUE(m_fake.windows.empty());
    };

    pond::platform::WindowDesc hiddenDesc;
    hiddenDesc.visible = false;
    expectFailure("create-window", hiddenDesc, 0);

    pond::platform::WindowDesc minimumDesc = hiddenDesc;
    minimumDesc.minimumLogicalSize = pond::platform::LogicalSize{100, 80};
    expectFailure("set-window-minimum-size", minimumDesc, 1);
    expectFailure("get-window-id", hiddenDesc, 2);

    pond::platform::WindowDesc visibleDesc = hiddenDesc;
    visibleDesc.visible = true;
    expectFailure("show-window", visibleDesc, 3);

    m_fake.failingWindowOperation.clear();
    auto retryResult = runtime.CreateWindow(hiddenDesc);
    ASSERT_TRUE(retryResult.HasValue());
    pond::platform::Window retry = std::move(retryResult).GetValue();
    EXPECT_EQ(retry.GetId(), pond::platform::WindowId{2});
}

TEST_F(PlatformRuntimeBackendTests, ShowsVisibleWindowsAfterNativeSetupCompletes)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.minimumLogicalSize = pond::platform::LogicalSize{320, 200};
    const std::size_t firstWindowCall = m_fake.calls.size();

    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const std::vector<std::string> expectedCalls{"create-window", "set-window-minimum-size",
                                                 "get-window-id", "show-window"};
    ASSERT_GE(m_fake.calls.size(), firstWindowCall + expectedCalls.size());
    EXPECT_TRUE(std::ranges::equal(
        expectedCalls, std::span{m_fake.calls}.subspan(firstWindowCall, expectedCalls.size())));
    ASSERT_EQ(m_fake.windows.size(), 1U);
    EXPECT_TRUE(m_fake.windows.front().visible);
}

TEST_F(PlatformRuntimeBackendTests, ExposesOwnedBasicWindowOperations)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.title = "Initial";
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    FakeWindow& backendWindow = m_fake.windows.front();
    backendWindow.x = -120;
    backendWindow.y = 45;
    backendWindow.width = 700;
    backendWindow.height = 500;
    backendWindow.pixelWidth = 1400;
    backendWindow.pixelHeight = 1000;

    EXPECT_EQ(window.GetTitle(), "Initial");
    const std::string titleBuffer{"Updated plus ignored suffix"};
    ASSERT_TRUE(window.SetTitle(std::string_view{titleBuffer.data(), 7}).HasValue());
    EXPECT_EQ(window.GetTitle(), "Updated");

    auto position = window.GetPosition();
    ASSERT_TRUE(position.HasValue());
    EXPECT_EQ(position.GetValue(), (pond::platform::ScreenPosition{-120, 45}));
    ASSERT_TRUE(window.SetPosition({-55, 90}).HasValue());
    EXPECT_EQ(backendWindow.x, -55);
    EXPECT_EQ(backendWindow.y, 90);

    auto logicalSize = window.GetLogicalSize();
    ASSERT_TRUE(logicalSize.HasValue());
    EXPECT_EQ(logicalSize.GetValue(), (pond::platform::LogicalSize{700, 500}));
    auto pixelSize = window.GetPixelSize();
    ASSERT_TRUE(pixelSize.HasValue());
    EXPECT_EQ(pixelSize.GetValue(), (pond::platform::PixelSize{1400, 1000}));

    ASSERT_TRUE(window.SetLogicalSize({900, 600}).HasValue());
    EXPECT_EQ(backendWindow.width, 900);
    EXPECT_EQ(backendWindow.height, 600);
    ASSERT_TRUE(window.Show().HasValue());
    EXPECT_TRUE(backendWindow.visible);
    ASSERT_TRUE(window.Hide().HasValue());
    EXPECT_FALSE(backendWindow.visible);
    EXPECT_EQ(window.GetGraphicsCompatibility(),
              pond::platform::WindowGraphicsCompatibility::Default);
}

TEST_F(PlatformRuntimeBackendTests, RejectsInvalidWindowMutatorsWithoutBackendCalls)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto expectNoBackendCall = [this](const auto& result, std::size_t callCount)
    {
        EXPECT_FALSE(result.HasValue());
        if (!result.HasValue())
        {
            EXPECT_EQ(
                result.GetError().GetCode(),
                pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));
        }
        EXPECT_EQ(m_fake.calls.size(), callCount);
    };

    std::size_t callCount = m_fake.calls.size();
    expectNoBackendCall(window.SetTitle(std::string{"\xC0\xAF", 2}), callCount);
    callCount = m_fake.calls.size();
    expectNoBackendCall(window.SetTitle(std::string{"bad\0title", 9}), callCount);
    callCount = m_fake.calls.size();
    expectNoBackendCall(window.SetLogicalSize({0, 100}), callCount);
    callCount = m_fake.calls.size();
    expectNoBackendCall(window.SetLogicalSize({std::numeric_limits<std::uint32_t>::max(), 100}),
                        callCount);
    callCount = m_fake.calls.size();
    expectNoBackendCall(window.SetPosition({0x1FFF0000, 10}), callCount);
    callCount = m_fake.calls.size();
    expectNoBackendCall(window.SetPosition({10, 0x2FFF0000}), callCount);
}

TEST_F(PlatformRuntimeBackendTests, ReturnsContextualWindowOperationFailures)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto configureFailure = [this](std::string operation)
    {
        m_fake.failingWindowOperation = std::move(operation);
        m_fake.windowFailuresRemaining = 1;
    };
    const auto expectBackendFailure = [](const auto& result, std::string_view operation,
                                         std::string_view context = "backend window")
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
        EXPECT_NE(result.GetError().GetMessage().find(operation), std::string_view::npos);
        EXPECT_NE(result.GetError().GetMessage().find(context), std::string_view::npos);
    };

    configureFailure("set-window-title");
    expectBackendFailure(window.SetTitle("new"), "SDL_SetWindowTitle");
    configureFailure("get-window-position");
    expectBackendFailure(window.GetPosition(), "SDL_GetWindowPosition");
    configureFailure("set-window-position");
    expectBackendFailure(window.SetPosition({1, 2}), "SDL_SetWindowPosition");
    configureFailure("get-window-size");
    expectBackendFailure(window.GetLogicalSize(), "SDL_GetWindowSize");
    configureFailure("get-window-size-in-pixels");
    expectBackendFailure(window.GetPixelSize(), "SDL_GetWindowSizeInPixels");
    configureFailure("set-window-size");
    expectBackendFailure(window.SetLogicalSize({200, 100}), "SDL_SetWindowSize");
    configureFailure("show-window");
    expectBackendFailure(window.Show(), "SDL_ShowWindow");
    configureFailure("hide-window");
    expectBackendFailure(window.Hide(), "SDL_HideWindow");

    m_fake.failingWindowOperation.clear();
    m_fake.windows.front().width = -1;
    expectBackendFailure(window.GetLogicalSize(), "SDL_GetWindowSize", "window 1");
    m_fake.windows.front().width = 100;
    m_fake.windows.front().pixelHeight = -1;
    expectBackendFailure(window.GetPixelSize(), "SDL_GetWindowSizeInPixels", "window 1");
}

TEST_F(PlatformRuntimeBackendTests, MovesWindowsWithoutChangingTheirStableIdentity)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto firstResult = runtime.CreateWindow(desc);
    auto secondResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(firstResult.HasValue());
    ASSERT_TRUE(secondResult.HasValue());
    pond::platform::Window first = std::move(firstResult).GetValue();
    pond::platform::Window second = std::move(secondResult).GetValue();

    pond::platform::Window moved = std::move(first);
    EXPECT_THROW(static_cast<void>(first.GetId()), pond::core::PonderException);
    EXPECT_EQ(moved.GetId(), pond::platform::WindowId{1});
    moved = std::move(moved);
    EXPECT_EQ(moved.GetId(), pond::platform::WindowId{1});

    second = std::move(moved);
    EXPECT_EQ(m_fake.destroyWindowCalls, 1);
    EXPECT_THROW(static_cast<void>(moved.GetId()), pond::core::PonderException);
    EXPECT_EQ(second.GetId(), pond::platform::WindowId{1});
}

TEST_F(PlatformRuntimeBackendTests, RejectsWindowUseFromTheWrongThread)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    const std::size_t callCount = m_fake.calls.size();

    std::atomic_bool rejected{false};
    std::thread worker{[&window, &rejected]()
                       {
                           try
                           {
                               static_cast<void>(window.SetTitle("wrong thread"));
                           }
                           catch (const pond::core::PonderException&)
                           {
                               rejected.store(true);
                           }
                       }};
    worker.join();

    EXPECT_TRUE(rejected.load());
    EXPECT_EQ(m_fake.calls.size(), callCount);
}

TEST_F(PlatformRuntimeBackendTests, ExposesIndependentOrthogonalWindowProperties)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    desc.resizable = true;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    auto presentation = window.GetPresentation();
    ASSERT_TRUE(presentation.HasValue());
    EXPECT_EQ(presentation.GetValue(), pond::platform::WindowPresentation::Windowed);
    auto decoration = window.GetDecoration();
    ASSERT_TRUE(decoration.HasValue());
    EXPECT_EQ(decoration.GetValue(), pond::platform::WindowDecoration::System);
    auto state = window.GetState();
    ASSERT_TRUE(state.HasValue());
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Normal);
    auto visible = window.IsVisible();
    ASSERT_TRUE(visible.HasValue());
    EXPECT_FALSE(visible.GetValue());
    auto resizable = window.IsResizable();
    ASSERT_TRUE(resizable.HasValue());
    EXPECT_TRUE(resizable.GetValue());
    auto focused = window.IsFocused();
    ASSERT_TRUE(focused.HasValue());
    EXPECT_FALSE(focused.GetValue());
    auto alwaysOnTop = window.IsAlwaysOnTop();
    ASSERT_TRUE(alwaysOnTop.HasValue());
    EXPECT_FALSE(alwaysOnTop.GetValue());

    FakeWindow& backendWindow = m_fake.windows.front();
    backendWindow.inputFocus = true;
    focused = window.IsFocused();
    ASSERT_TRUE(focused.HasValue());
    EXPECT_TRUE(focused.GetValue());

    ASSERT_TRUE(
        window.SetPresentation(pond::platform::WindowPresentation::DesktopFullscreen).HasValue());
    presentation = window.GetPresentation();
    ASSERT_TRUE(presentation.HasValue());
    EXPECT_EQ(presentation.GetValue(), pond::platform::WindowPresentation::DesktopFullscreen);
    EXPECT_TRUE(backendWindow.desktopFullscreen);
    EXPECT_TRUE(backendWindow.inputFocus);

    ASSERT_TRUE(window.SetPresentation(pond::platform::WindowPresentation::Windowed).HasValue());
    ASSERT_TRUE(window.SetDecoration(pond::platform::WindowDecoration::Borderless).HasValue());
    decoration = window.GetDecoration();
    ASSERT_TRUE(decoration.HasValue());
    EXPECT_EQ(decoration.GetValue(), pond::platform::WindowDecoration::Borderless);

    ASSERT_TRUE(window.SetResizable(false).HasValue());
    resizable = window.IsResizable();
    ASSERT_TRUE(resizable.HasValue());
    EXPECT_FALSE(resizable.GetValue());
    ASSERT_TRUE(window.SetAlwaysOnTop(true).HasValue());
    alwaysOnTop = window.IsAlwaysOnTop();
    ASSERT_TRUE(alwaysOnTop.HasValue());
    EXPECT_TRUE(alwaysOnTop.GetValue());

    ASSERT_TRUE(window.Minimize().HasValue());
    state = window.GetState();
    ASSERT_TRUE(state.HasValue());
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Minimized);
    ASSERT_TRUE(window.Restore().HasValue());
    state = window.GetState();
    ASSERT_TRUE(state.HasValue());
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Normal);

    ASSERT_TRUE(window.SetResizable(true).HasValue());
    ASSERT_TRUE(window.Maximize().HasValue());
    state = window.GetState();
    ASSERT_TRUE(state.HasValue());
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Maximized);
    ASSERT_TRUE(window.Restore().HasValue());

    visible = window.IsVisible();
    ASSERT_TRUE(visible.HasValue());
    EXPECT_FALSE(visible.GetValue());
    decoration = window.GetDecoration();
    ASSERT_TRUE(decoration.HasValue());
    EXPECT_EQ(decoration.GetValue(), pond::platform::WindowDecoration::Borderless);
    focused = window.IsFocused();
    ASSERT_TRUE(focused.HasValue());
    EXPECT_TRUE(focused.GetValue());
    alwaysOnTop = window.IsAlwaysOnTop();
    ASSERT_TRUE(alwaysOnTop.HasValue());
    EXPECT_TRUE(alwaysOnTop.GetValue());

    ASSERT_TRUE(window.Show().HasValue());
    visible = window.IsVisible();
    ASSERT_TRUE(visible.HasValue());
    EXPECT_TRUE(visible.GetValue());
    ASSERT_TRUE(window.Hide().HasValue());
    visible = window.IsVisible();
    ASSERT_TRUE(visible.HasValue());
    EXPECT_FALSE(visible.GetValue());
}

TEST_F(PlatformRuntimeBackendTests, NormalizesOppositeStateTransitionsForHiddenWindows)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    ASSERT_TRUE(window.Maximize().HasValue());
    std::size_t firstCall = m_fake.calls.size();
    ASSERT_TRUE(window.Minimize().HasValue());
    const std::array<std::string_view, 3> minimizeCalls{"get-window-properties", "restore-window",
                                                        "minimize-window"};
    ASSERT_GE(m_fake.calls.size(), firstCall + minimizeCalls.size());
    EXPECT_TRUE(std::ranges::equal(
        minimizeCalls, std::span{m_fake.calls}.subspan(firstCall, minimizeCalls.size())));
    auto state = window.GetState();
    ASSERT_TRUE(state.HasValue());
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Minimized);

    firstCall = m_fake.calls.size();
    ASSERT_TRUE(window.Maximize().HasValue());
    const std::array<std::string_view, 3> maximizeCalls{"get-window-properties", "restore-window",
                                                        "maximize-window"};
    ASSERT_GE(m_fake.calls.size(), firstCall + maximizeCalls.size());
    EXPECT_TRUE(std::ranges::equal(
        maximizeCalls, std::span{m_fake.calls}.subspan(firstCall, maximizeCalls.size())));
    state = window.GetState();
    ASSERT_TRUE(state.HasValue());
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Maximized);
}

TEST_F(PlatformRuntimeBackendTests, LastHiddenStateRequestWinsAfterHidingAStatefulVisibleWindow)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    FakeWindow& backendWindow = m_fake.windows.front();

    ASSERT_TRUE(window.Show().HasValue());
    ASSERT_TRUE(window.Maximize().HasValue());
    ASSERT_TRUE(backendWindow.maximized);
    ASSERT_FALSE(backendWindow.pendingMaximized);
    ASSERT_TRUE(window.Hide().HasValue());
    ASSERT_TRUE(backendWindow.maximized);
    ASSERT_TRUE(backendWindow.pendingMaximized);

    ASSERT_TRUE(window.Minimize().HasValue());
    ASSERT_TRUE(backendWindow.maximized);
    ASSERT_TRUE(backendWindow.pendingMinimized);
    ASSERT_FALSE(backendWindow.pendingMaximized);
    auto state = window.GetState();
    ASSERT_TRUE(state.HasValue()) << state.GetError().GetMessage();
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Minimized);

    ASSERT_TRUE(window.Restore().HasValue());
    state = window.GetState();
    ASSERT_TRUE(state.HasValue()) << state.GetError().GetMessage();
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Normal);
    ASSERT_TRUE(window.Show().HasValue());
    state = window.GetState();
    ASSERT_TRUE(state.HasValue()) << state.GetError().GetMessage();
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Normal);

    ASSERT_TRUE(window.Minimize().HasValue());
    ASSERT_TRUE(backendWindow.minimized);
    ASSERT_FALSE(backendWindow.pendingMinimized);
    ASSERT_TRUE(window.Hide().HasValue());
    ASSERT_TRUE(backendWindow.minimized);
    ASSERT_TRUE(backendWindow.pendingMinimized);

    ASSERT_TRUE(window.Maximize().HasValue());
    ASSERT_TRUE(backendWindow.minimized);
    ASSERT_FALSE(backendWindow.pendingMinimized);
    ASSERT_TRUE(backendWindow.pendingMaximized);
    state = window.GetState();
    ASSERT_TRUE(state.HasValue()) << state.GetError().GetMessage();
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Maximized);

    ASSERT_TRUE(window.Restore().HasValue());
    state = window.GetState();
    ASSERT_TRUE(state.HasValue()) << state.GetError().GetMessage();
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Normal);
    ASSERT_TRUE(window.Show().HasValue());
    state = window.GetState();
    ASSERT_TRUE(state.HasValue()) << state.GetError().GetMessage();
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Normal);
}

TEST_F(PlatformRuntimeBackendTests, SuccessfulRequestsDoNotOptimisticallyChangeObservedProperties)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    ASSERT_TRUE(window.Show().HasValue());
    m_fake.applyWindowOperationEffects = false;

    ASSERT_TRUE(
        window.SetPresentation(pond::platform::WindowPresentation::DesktopFullscreen).HasValue());
    auto presentation = window.GetPresentation();
    ASSERT_TRUE(presentation.HasValue());
    EXPECT_EQ(presentation.GetValue(), pond::platform::WindowPresentation::Windowed);

    ASSERT_TRUE(window.SetDecoration(pond::platform::WindowDecoration::Borderless).HasValue());
    auto decoration = window.GetDecoration();
    ASSERT_TRUE(decoration.HasValue());
    EXPECT_EQ(decoration.GetValue(), pond::platform::WindowDecoration::System);

    ASSERT_TRUE(window.SetResizable(false).HasValue());
    auto resizable = window.IsResizable();
    ASSERT_TRUE(resizable.HasValue());
    EXPECT_TRUE(resizable.GetValue());

    ASSERT_TRUE(window.SetAlwaysOnTop(true).HasValue());
    auto alwaysOnTop = window.IsAlwaysOnTop();
    ASSERT_TRUE(alwaysOnTop.HasValue());
    EXPECT_FALSE(alwaysOnTop.GetValue());

    ASSERT_TRUE(window.Minimize().HasValue());
    auto state = window.GetState();
    ASSERT_TRUE(state.HasValue());
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Normal);
    ASSERT_TRUE(window.Maximize().HasValue());
    state = window.GetState();
    ASSERT_TRUE(state.HasValue());
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Normal);
}

TEST_F(PlatformRuntimeBackendTests, ForwardsImmediateReversalsOfAcceptedAsyncWindowRequests)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    ASSERT_TRUE(window.Show().HasValue());
    m_fake.applyWindowOperationEffects = false;

    const auto countCalls = [this](std::string_view operation)
    {
        return std::ranges::count(m_fake.calls, operation);
    };

    ASSERT_TRUE(
        window.SetPresentation(pond::platform::WindowPresentation::DesktopFullscreen).HasValue());
    ASSERT_TRUE(
        window.SetPresentation(pond::platform::WindowPresentation::DesktopFullscreen).HasValue());
    EXPECT_EQ(countCalls("set-window-fullscreen"), 1);
    ASSERT_TRUE(window.SetPresentation(pond::platform::WindowPresentation::Windowed).HasValue());
    ASSERT_TRUE(window.SetPresentation(pond::platform::WindowPresentation::Windowed).HasValue());
    EXPECT_EQ(countCalls("set-window-fullscreen"), 2);

    ASSERT_TRUE(window.Minimize().HasValue());
    ASSERT_TRUE(window.Minimize().HasValue());
    EXPECT_EQ(countCalls("minimize-window"), 1);
    ASSERT_TRUE(window.Restore().HasValue());
    ASSERT_TRUE(window.Restore().HasValue());
    EXPECT_EQ(countCalls("minimize-window"), 1);
    EXPECT_EQ(countCalls("restore-window"), 1);

    ASSERT_TRUE(window.Minimize().HasValue());
    EXPECT_EQ(countCalls("minimize-window"), 2);
    ASSERT_TRUE(window.Restore().HasValue());
    EXPECT_EQ(countCalls("restore-window"), 2);

    ASSERT_TRUE(window.Maximize().HasValue());
    EXPECT_EQ(countCalls("maximize-window"), 1);
    ASSERT_TRUE(window.Maximize().HasValue());
    EXPECT_EQ(countCalls("maximize-window"), 1);
    ASSERT_TRUE(window.Restore().HasValue());
    ASSERT_TRUE(window.Restore().HasValue());
    EXPECT_EQ(countCalls("restore-window"), 3);
    ASSERT_TRUE(window.Maximize().HasValue());
    EXPECT_EQ(countCalls("maximize-window"), 2);
}

TEST_F(PlatformRuntimeBackendTests, RejectsForgedWindowPropertyEnumsWithoutBackendCalls)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto expectInvalidArgument = [](const pond::core::VoidResult& result)
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));
    };

    const std::size_t callCount = m_fake.calls.size();
    expectInvalidArgument(
        window.SetPresentation(static_cast<pond::platform::WindowPresentation>(0xFF)));
    EXPECT_EQ(m_fake.calls.size(), callCount);
    expectInvalidArgument(
        window.SetDecoration(static_cast<pond::platform::WindowDecoration>(0xFF)));
    EXPECT_EQ(m_fake.calls.size(), callCount);
}

TEST_F(PlatformRuntimeBackendTests, AvoidsBackendMutationsForIdempotentStateRequests)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto countCalls = [this](std::string_view operation)
    {
        return std::ranges::count(m_fake.calls, operation);
    };

    ASSERT_TRUE(window.SetPresentation(pond::platform::WindowPresentation::Windowed).HasValue());
    ASSERT_TRUE(window.SetDecoration(pond::platform::WindowDecoration::System).HasValue());
    ASSERT_TRUE(window.SetResizable(true).HasValue());
    ASSERT_TRUE(window.SetAlwaysOnTop(false).HasValue());
    ASSERT_TRUE(window.Restore().HasValue());
    EXPECT_EQ(countCalls("set-window-fullscreen-mode-to-desktop"), 0);
    EXPECT_EQ(countCalls("set-window-fullscreen"), 0);
    EXPECT_EQ(countCalls("set-window-bordered"), 0);
    EXPECT_EQ(countCalls("set-window-resizable"), 0);
    EXPECT_EQ(countCalls("set-window-always-on-top"), 0);
    EXPECT_EQ(countCalls("restore-window"), 0);

    ASSERT_TRUE(window.Minimize().HasValue());
    EXPECT_EQ(countCalls("minimize-window"), 1);
    ASSERT_TRUE(window.Minimize().HasValue());
    EXPECT_EQ(countCalls("minimize-window"), 1);

    ASSERT_TRUE(window.Restore().HasValue());
    const auto restoreCallCount = countCalls("restore-window");
    ASSERT_TRUE(window.Restore().HasValue());
    EXPECT_EQ(countCalls("restore-window"), restoreCallCount);

    ASSERT_TRUE(window.Maximize().HasValue());
    EXPECT_EQ(countCalls("maximize-window"), 1);
    ASSERT_TRUE(window.Maximize().HasValue());
    EXPECT_EQ(countCalls("maximize-window"), 1);
}

TEST_F(PlatformRuntimeBackendTests, ReportsNonResizableAndBackendUnsupportedWindowOperations)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    desc.resizable = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto expectUnsupported =
        [](const pond::core::VoidResult& result, std::string_view operation)
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));
        EXPECT_NE(result.GetError().GetMessage().find(operation), std::string_view::npos);
    };
    const auto configureUnsupported = [this](std::string operation)
    {
        m_fake.unsupportedWindowOperation = std::move(operation);
        m_fake.unsupportedWindowOperationsRemaining = 1;
    };

    const auto maximizeCallsBefore =
        std::ranges::count(m_fake.calls, std::string_view{"maximize-window"});
    expectUnsupported(window.Maximize(), "non-resizable");
    EXPECT_EQ(std::ranges::count(m_fake.calls, std::string_view{"maximize-window"}),
              maximizeCallsBefore);

    FakeWindow& backendWindow = m_fake.windows.front();
    backendWindow.resizable = true;

    configureUnsupported("minimize-window");
    auto result = window.Minimize();
    expectUnsupported(result, "SDL_MinimizeWindow");
    EXPECT_NE(result.GetError().GetMessage().find("backend window"), std::string_view::npos);

    configureUnsupported("maximize-window");
    result = window.Maximize();
    expectUnsupported(result, "SDL_MaximizeWindow");
    EXPECT_NE(result.GetError().GetMessage().find("backend window"), std::string_view::npos);

    backendWindow.minimized = true;
    configureUnsupported("restore-window");
    result = window.Restore();
    expectUnsupported(result, "SDL_RestoreWindow");
    EXPECT_NE(result.GetError().GetMessage().find("backend window"), std::string_view::npos);
}

TEST_F(PlatformRuntimeBackendTests, ReportsTypedUnsupportedPropertyRequests)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto configureUnsupported = [this](std::string operation)
    {
        m_fake.unsupportedWindowOperation = std::move(operation);
        m_fake.unsupportedWindowOperationsRemaining = 1;
    };
    const auto expectUnsupported =
        [](const pond::core::VoidResult& result, std::string_view operation,
           std::string_view context = "backend window")
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));
        EXPECT_NE(result.GetError().GetMessage().find(operation), std::string_view::npos);
        EXPECT_NE(result.GetError().GetMessage().find(context), std::string_view::npos);
    };

    configureUnsupported("set-window-fullscreen-mode-to-desktop");
    expectUnsupported(window.SetPresentation(pond::platform::WindowPresentation::DesktopFullscreen),
                      "SDL_SetWindowFullscreenMode");

    configureUnsupported("set-window-fullscreen");
    expectUnsupported(window.SetPresentation(pond::platform::WindowPresentation::DesktopFullscreen),
                      "SDL_SetWindowFullscreen");

    configureUnsupported("set-window-bordered");
    expectUnsupported(window.SetDecoration(pond::platform::WindowDecoration::Borderless),
                      "SDL_SetWindowBordered");
    configureUnsupported("set-window-resizable");
    expectUnsupported(window.SetResizable(false), "SDL_SetWindowResizable");
    configureUnsupported("set-window-always-on-top");
    expectUnsupported(window.SetAlwaysOnTop(true), "SDL_SetWindowAlwaysOnTop");

    FakeWindow& backendWindow = m_fake.windows.front();
    backendWindow.desktopFullscreen = true;
    const std::size_t callCount = m_fake.calls.size();
    expectUnsupported(window.SetDecoration(pond::platform::WindowDecoration::Borderless),
                      "decoration", "window 1");
    EXPECT_EQ(m_fake.calls.size(), callCount + 1);
    expectUnsupported(window.SetResizable(false), "resizability", "window 1");
    EXPECT_EQ(m_fake.calls.size(), callCount + 2);
}

TEST_F(PlatformRuntimeBackendTests, ReportsContextualWindowPropertyFailures)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto configureFailure = [this](std::string operation)
    {
        m_fake.failingWindowOperation = std::move(operation);
        m_fake.windowFailuresRemaining = 1;
    };
    const auto expectBackendFailure = [](const auto& result, std::string_view operation,
                                         std::string_view context = "backend window")
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
        EXPECT_NE(result.GetError().GetMessage().find(operation), std::string_view::npos);
        EXPECT_NE(result.GetError().GetMessage().find(context), std::string_view::npos);
    };

    configureFailure("set-window-fullscreen-mode-to-desktop");
    expectBackendFailure(
        window.SetPresentation(pond::platform::WindowPresentation::DesktopFullscreen),
        "SDL_SetWindowFullscreenMode");
    configureFailure("set-window-fullscreen");
    expectBackendFailure(
        window.SetPresentation(pond::platform::WindowPresentation::DesktopFullscreen),
        "SDL_SetWindowFullscreen");
    configureFailure("set-window-bordered");
    expectBackendFailure(window.SetDecoration(pond::platform::WindowDecoration::Borderless),
                         "SDL_SetWindowBordered");
    configureFailure("set-window-resizable");
    expectBackendFailure(window.SetResizable(false), "SDL_SetWindowResizable");
    configureFailure("set-window-always-on-top");
    expectBackendFailure(window.SetAlwaysOnTop(true), "SDL_SetWindowAlwaysOnTop");
    configureFailure("minimize-window");
    expectBackendFailure(window.Minimize(), "SDL_MinimizeWindow");
    configureFailure("maximize-window");
    expectBackendFailure(window.Maximize(), "SDL_MaximizeWindow");

    m_fake.windows.front().minimized = true;
    configureFailure("restore-window");
    expectBackendFailure(window.Restore(), "SDL_RestoreWindow");
}

TEST_F(PlatformRuntimeBackendTests, ReportsPropertyQueryFailuresAndContradictoryWindowState)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto configurePropertiesFailure = [this]()
    {
        m_fake.failingWindowOperation = "get-window-properties";
        m_fake.windowFailuresRemaining = 1;
    };
    const auto expectPropertiesFailure = [](const auto& result)
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
        EXPECT_NE(result.GetError().GetMessage().find("SDL_GetWindowFlags"),
                  std::string_view::npos);
        EXPECT_NE(result.GetError().GetMessage().find("backend window"), std::string_view::npos);
    };

    configurePropertiesFailure();
    expectPropertiesFailure(window.GetPresentation());
    configurePropertiesFailure();
    expectPropertiesFailure(window.GetDecoration());
    configurePropertiesFailure();
    expectPropertiesFailure(window.GetState());
    configurePropertiesFailure();
    expectPropertiesFailure(window.IsVisible());
    configurePropertiesFailure();
    expectPropertiesFailure(window.IsResizable());
    configurePropertiesFailure();
    expectPropertiesFailure(window.IsFocused());
    configurePropertiesFailure();
    expectPropertiesFailure(window.IsAlwaysOnTop());
    configurePropertiesFailure();
    expectPropertiesFailure(window.SetAlwaysOnTop(true));

    FakeWindow& backendWindow = m_fake.windows.front();
    backendWindow.minimized = true;
    backendWindow.maximized = true;
    auto state = window.GetState();
    ASSERT_FALSE(state.HasValue());
    EXPECT_EQ(state.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(state.GetError().GetMessage().find("contradictory"), std::string_view::npos);

    const auto minimizeCalls =
        std::ranges::count(m_fake.calls, std::string_view{"minimize-window"});
    const auto maximizeCalls =
        std::ranges::count(m_fake.calls, std::string_view{"maximize-window"});
    const auto restoreCalls = std::ranges::count(m_fake.calls, std::string_view{"restore-window"});
    const auto expectContradictory = [](const pond::core::VoidResult& result)
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
        EXPECT_NE(result.GetError().GetMessage().find("contradictory"), std::string_view::npos);
    };
    expectContradictory(window.Minimize());
    expectContradictory(window.Maximize());
    expectContradictory(window.Restore());
    EXPECT_EQ(std::ranges::count(m_fake.calls, std::string_view{"minimize-window"}), minimizeCalls);
    EXPECT_EQ(std::ranges::count(m_fake.calls, std::string_view{"maximize-window"}), maximizeCalls);
    EXPECT_EQ(std::ranges::count(m_fake.calls, std::string_view{"restore-window"}), restoreCalls);
}

TEST_F(PlatformRuntimeBackendTests, GuardsEveryWindowStateApiAfterMoveAndAcrossThreads)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    pond::platform::Window movedWindow = std::move(window);

    const std::size_t callCount = m_fake.calls.size();
    EXPECT_EQ(CountRejectedWindowStateCalls(window), 14);
    EXPECT_EQ(m_fake.calls.size(), callCount);

    std::atomic_int rejectedCalls{};
    std::thread worker{[&movedWindow, &rejectedCalls]()
                       {
                           rejectedCalls.store(CountRejectedWindowStateCalls(movedWindow));
                       }};
    worker.join();

    EXPECT_EQ(rejectedCalls.load(), 14);
    EXPECT_EQ(m_fake.calls.size(), callCount);
}

TEST_F(PlatformRuntimeBackendTests, EnumeratesOwnedDisplaySnapshots)
{
    m_fake.connectedDisplayIds = {41, 73};
    m_fake.displays.emplace(41, FakeDisplay{41,
                                            "Primary Display",
                                            {-1920, 0, 1920, 1080},
                                            {-1920, 40, 1920, 1040},
                                            59.94F,
                                            BackendDisplayOrientation::Landscape,
                                            1.25F});
    m_fake.displays.emplace(73, FakeDisplay{73,
                                            "Portrait Display",
                                            {0, -120, 1440, 2560},
                                            {0, -80, 1440, 2520},
                                            0.0F,
                                            BackendDisplayOrientation::PortraitFlipped,
                                            2.0F});

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto result = runtime.EnumerateDisplays();
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    const std::vector<pond::platform::DisplayInfo>& displays = result.GetValue();
    ASSERT_EQ(displays.size(), 2U);

    EXPECT_EQ(displays[0].id, pond::platform::DisplayId{1});
    EXPECT_EQ(displays[0].name, "Primary Display");
    EXPECT_EQ(displays[0].bounds, (pond::platform::ScreenRectangle{{-1920, 0}, {1920, 1080}}));
    EXPECT_EQ(displays[0].usableBounds,
              (pond::platform::ScreenRectangle{{-1920, 40}, {1920, 1040}}));
    ASSERT_TRUE(displays[0].refreshRateHertz.has_value());
    EXPECT_FLOAT_EQ(*displays[0].refreshRateHertz, 59.94F);
    EXPECT_EQ(displays[0].orientation, pond::platform::DisplayOrientation::Landscape);
    EXPECT_FLOAT_EQ(displays[0].contentScale, 1.25F);

    EXPECT_EQ(displays[1].id, pond::platform::DisplayId{2});
    EXPECT_EQ(displays[1].name, "Portrait Display");
    EXPECT_EQ(displays[1].bounds, (pond::platform::ScreenRectangle{{0, -120}, {1440, 2560}}));
    EXPECT_EQ(displays[1].usableBounds, (pond::platform::ScreenRectangle{{0, -80}, {1440, 2520}}));
    EXPECT_FALSE(displays[1].refreshRateHertz.has_value());
    EXPECT_EQ(displays[1].orientation, pond::platform::DisplayOrientation::PortraitFlipped);
    EXPECT_FLOAT_EQ(displays[1].contentScale, 2.0F);

    m_fake.displays.at(41).name = "Backend Storage Changed";
    m_fake.displays.at(73).name.clear();
    EXPECT_EQ(displays[0].name, "Primary Display");
    EXPECT_EQ(displays[1].name, "Portrait Display");
}

TEST_F(PlatformRuntimeBackendTests, PreservesDisplayIdsAcrossReorderRemovalAndReconnect)
{
    m_fake.connectedDisplayIds = {10, 20};
    m_fake.displays.emplace(10, FakeDisplay{10,
                                            "First",
                                            {0, 0, 800, 600},
                                            {0, 0, 800, 560},
                                            60.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.0F});
    m_fake.displays.emplace(20, FakeDisplay{20,
                                            "Second",
                                            {800, 0, 1024, 768},
                                            {800, 0, 1024, 728},
                                            75.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.5F});

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto firstEnumeration = runtime.EnumerateDisplays();
    ASSERT_TRUE(firstEnumeration.HasValue());
    ASSERT_EQ(firstEnumeration.GetValue().size(), 2U);
    const pond::platform::DisplayId firstId = firstEnumeration.GetValue()[0].id;
    const pond::platform::DisplayId secondId = firstEnumeration.GetValue()[1].id;
    EXPECT_EQ(firstId, pond::platform::DisplayId{1});
    EXPECT_EQ(secondId, pond::platform::DisplayId{2});

    m_fake.connectedDisplayIds = {20, 10};
    auto reordered = runtime.EnumerateDisplays();
    ASSERT_TRUE(reordered.HasValue());
    ASSERT_EQ(reordered.GetValue().size(), 2U);
    EXPECT_EQ(reordered.GetValue()[0].id, secondId);
    EXPECT_EQ(reordered.GetValue()[1].id, firstId);

    m_fake.connectedDisplayIds = {20};
    auto stale = runtime.GetDisplayInfo(firstId);
    ASSERT_FALSE(stale.HasValue());
    EXPECT_EQ(stale.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::NotFound));

    m_fake.displays.emplace(30, FakeDisplay{30,
                                            "First Reconnected",
                                            {0, 0, 800, 600},
                                            {0, 0, 800, 560},
                                            60.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.0F});
    m_fake.connectedDisplayIds = {20, 30};
    auto reconnected = runtime.EnumerateDisplays();
    ASSERT_TRUE(reconnected.HasValue());
    ASSERT_EQ(reconnected.GetValue().size(), 2U);
    EXPECT_EQ(reconnected.GetValue()[0].id, secondId);
    EXPECT_EQ(reconnected.GetValue()[1].id, pond::platform::DisplayId{3});
    EXPECT_NE(reconnected.GetValue()[1].id, firstId);

    auto invalid = runtime.GetDisplayInfo(pond::platform::DisplayId::Invalid());
    ASSERT_FALSE(invalid.HasValue());
    EXPECT_EQ(invalid.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));

    auto unknown = runtime.GetDisplayInfo(pond::platform::DisplayId{999});
    ASSERT_FALSE(unknown.HasValue());
    EXPECT_EQ(unknown.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::NotFound));
}

TEST_F(PlatformRuntimeBackendTests, RejectsMalformedRefreshWithoutChangingDisplayIdentityState)
{
    m_fake.connectedDisplayIds = {10};
    m_fake.displays.emplace(10, FakeDisplay{10,
                                            "Original",
                                            {0, 0, 800, 600},
                                            {0, 0, 800, 560},
                                            60.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.0F});
    m_fake.displays.emplace(20, FakeDisplay{20,
                                            "Added",
                                            {800, 0, 1024, 768},
                                            {800, 0, 1024, 728},
                                            75.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.5F});

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto initial = runtime.EnumerateDisplays();
    ASSERT_TRUE(initial.HasValue());
    ASSERT_EQ(initial.GetValue().size(), 1U);
    const pond::platform::DisplayId originalId = initial.GetValue().front().id;
    EXPECT_EQ(originalId, pond::platform::DisplayId{1});

    m_fake.connectedDisplayIds = {20, 20};
    auto malformed = runtime.EnumerateDisplays();
    ASSERT_FALSE(malformed.HasValue());
    EXPECT_EQ(malformed.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));

    m_fake.connectedDisplayIds = {10};
    auto unchanged = runtime.EnumerateDisplays();
    ASSERT_TRUE(unchanged.HasValue());
    ASSERT_EQ(unchanged.GetValue().size(), 1U);
    EXPECT_EQ(unchanged.GetValue().front().id, originalId);

    m_fake.connectedDisplayIds = {10, 20};
    auto validAddition = runtime.EnumerateDisplays();
    ASSERT_TRUE(validAddition.HasValue());
    ASSERT_EQ(validAddition.GetValue().size(), 2U);
    EXPECT_EQ(validAddition.GetValue()[0].id, originalId);
    EXPECT_EQ(validAddition.GetValue()[1].id, pond::platform::DisplayId{2});
}
TEST_F(PlatformRuntimeBackendTests, RestartsProjectDisplayIdsForANewRuntime)
{
    m_fake.connectedDisplayIds = {81};
    m_fake.displays.emplace(81, FakeDisplay{81,
                                            "Display",
                                            {0, 0, 1280, 720},
                                            {0, 0, 1280, 680},
                                            60.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.0F});

    {
        auto runtimeResult =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(runtimeResult.HasValue());
        pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
        auto displays = runtime.EnumerateDisplays();
        ASSERT_TRUE(displays.HasValue());
        ASSERT_EQ(displays.GetValue().size(), 1U);
        EXPECT_EQ(displays.GetValue()[0].id, pond::platform::DisplayId{1});
    }

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    auto displays = runtime.EnumerateDisplays();
    ASSERT_TRUE(displays.HasValue());
    ASSERT_EQ(displays.GetValue().size(), 1U);
    EXPECT_EQ(displays.GetValue()[0].id, pond::platform::DisplayId{1});
}

TEST_F(PlatformRuntimeBackendTests, ReportsContextualDisplayBackendFailures)
{
    m_fake.connectedDisplayIds = {55};
    m_fake.displays.emplace(55, FakeDisplay{55,
                                            "Display",
                                            {0, 0, 1920, 1080},
                                            {0, 0, 1920, 1040},
                                            60.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.0F});

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    auto initial = runtime.EnumerateDisplays();
    ASSERT_TRUE(initial.HasValue());
    const pond::platform::DisplayId displayId = initial.GetValue().front().id;

    m_fake.failingDisplayOperation = "enumerate-displays";
    m_fake.displayFailuresRemaining = 1;
    auto enumerationFailure = runtime.EnumerateDisplays();
    ASSERT_FALSE(enumerationFailure.HasValue());
    EXPECT_EQ(enumerationFailure.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(enumerationFailure.GetError().GetMessage().find("SDL_GetDisplays"),
              std::string_view::npos);
    EXPECT_NE(
        enumerationFailure.GetError().GetMessage().find("synthetic enumerate-displays failure"),
        std::string_view::npos);

    const auto expectQueryFailure =
        [&runtime, &displayId, this](std::string operation, std::string_view backendOperation)
    {
        m_fake.failingDisplayOperation = std::move(operation);
        m_fake.displayFailuresRemaining = 1;
        auto result = runtime.GetDisplayInfo(displayId);
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
        EXPECT_NE(result.GetError().GetMessage().find(backendOperation), std::string_view::npos);
        EXPECT_NE(result.GetError().GetMessage().find("backend display 55"),
                  std::string_view::npos);
    };

    expectQueryFailure("get-display-name", "SDL_GetDisplayName");
    expectQueryFailure("get-display-bounds", "SDL_GetDisplayBounds");
    expectQueryFailure("get-display-usable-bounds", "SDL_GetDisplayUsableBounds");
    expectQueryFailure("get-current-display-refresh-rate", "SDL_GetCurrentDisplayMode");
    expectQueryFailure("get-display-content-scale", "SDL_GetDisplayContentScale");
}

TEST_F(PlatformRuntimeBackendTests, RejectsMalformedDisplayBackendData)
{
    m_fake.connectedDisplayIds = {91};
    m_fake.displays.emplace(91, FakeDisplay{91,
                                            "Display",
                                            {0, 0, 1600, 900},
                                            {0, 0, 1600, 860},
                                            0.0F,
                                            BackendDisplayOrientation::Unknown,
                                            1.0F});

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    m_fake.connectedDisplayIds = {0};
    auto zeroId = runtime.EnumerateDisplays();
    ASSERT_FALSE(zeroId.HasValue());
    EXPECT_EQ(zeroId.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));

    m_fake.connectedDisplayIds = {91, 91};
    auto duplicateId = runtime.EnumerateDisplays();
    ASSERT_FALSE(duplicateId.HasValue());
    EXPECT_EQ(duplicateId.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));

    m_fake.connectedDisplayIds = {91};
    auto valid = runtime.EnumerateDisplays();
    ASSERT_TRUE(valid.HasValue());
    ASSERT_EQ(valid.GetValue().size(), 1U);
    const pond::platform::DisplayId displayId = valid.GetValue()[0].id;
    EXPECT_FALSE(valid.GetValue()[0].refreshRateHertz.has_value());

    const auto expectMalformed = [&runtime, &displayId](const auto& configure)
    {
        configure();
        auto result = runtime.GetDisplayInfo(displayId);
        EXPECT_FALSE(result.HasValue());
        if (!result.HasValue())
        {
            EXPECT_EQ(
                result.GetError().GetCode(),
                pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
        }
    };

    expectMalformed(
        [this]()
        {
            m_fake.displays.at(91).bounds.width = -1;
        });
    m_fake.displays.at(91).bounds.width = 1600;

    expectMalformed(
        [this]()
        {
            m_fake.displays.at(91).usableBounds.height = -1;
        });
    m_fake.displays.at(91).usableBounds.height = 860;

    expectMalformed(
        [this]()
        {
            m_fake.displays.at(91).refreshRateHertz = -1.0F;
        });
    m_fake.displays.at(91).refreshRateHertz = 60.0F;

    expectMalformed(
        [this]()
        {
            m_fake.displays.at(91).refreshRateHertz = std::numeric_limits<float>::quiet_NaN();
        });
    m_fake.displays.at(91).refreshRateHertz = 60.0F;

    expectMalformed(
        [this]()
        {
            m_fake.displays.at(91).contentScale = -0.5F;
        });
    m_fake.displays.at(91).contentScale = 1.0F;

    expectMalformed(
        [this]()
        {
            m_fake.displays.at(91).contentScale = std::numeric_limits<float>::infinity();
        });
    m_fake.displays.at(91).contentScale = 1.0F;

    m_fake.displays.at(91).orientation = static_cast<BackendDisplayOrientation>(0xFF);
    auto unknownOrientation = runtime.GetDisplayInfo(displayId);
    ASSERT_TRUE(unknownOrientation.HasValue());
    EXPECT_EQ(unknownOrientation.GetValue().orientation,
              pond::platform::DisplayOrientation::Unknown);
}

TEST_F(PlatformRuntimeBackendTests, ConvertsSnapshotFailureToNotFoundWhenDisplayDisconnects)
{
    m_fake.connectedDisplayIds = {64};
    m_fake.displays.emplace(64, FakeDisplay{64,
                                            "Transient",
                                            {0, 0, 800, 600},
                                            {0, 0, 800, 560},
                                            60.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.0F});

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    auto initial = runtime.EnumerateDisplays();
    ASSERT_TRUE(initial.HasValue());
    const pond::platform::DisplayId id = initial.GetValue().front().id;

    m_fake.failingDisplayOperation = "get-display-name";
    m_fake.displayFailuresRemaining = 1;
    m_fake.disconnectDisplayOnFailure = 64;
    auto result = runtime.GetDisplayInfo(id);

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::NotFound));
    EXPECT_EQ(m_fake.connectedDisplayIds.size(), 0U);
}

TEST_F(PlatformRuntimeBackendTests, ExposesWindowDisplayAndScaleQueries)
{
    m_fake.connectedDisplayIds = {77};
    m_fake.displays.emplace(77, FakeDisplay{77,
                                            "Window Display",
                                            {0, 0, 2560, 1440},
                                            {0, 0, 2560, 1400},
                                            144.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.5F});
    m_fake.defaultWindowDisplayId = 77;
    m_fake.defaultWindowPixelDensity = 2.0F;
    m_fake.defaultWindowDisplayScale = 1.75F;

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto firstResult = runtime.CreateWindow(desc);
    auto secondResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(firstResult.HasValue());
    ASSERT_TRUE(secondResult.HasValue());
    pond::platform::Window first = std::move(firstResult).GetValue();
    pond::platform::Window second = std::move(secondResult).GetValue();

    auto firstDisplay = first.GetDisplayId();
    auto secondDisplay = second.GetDisplayId();
    ASSERT_TRUE(firstDisplay.HasValue());
    ASSERT_TRUE(secondDisplay.HasValue());
    EXPECT_EQ(firstDisplay.GetValue(), pond::platform::DisplayId{1});
    EXPECT_EQ(secondDisplay.GetValue(), firstDisplay.GetValue());

    auto density = first.GetPixelDensity();
    ASSERT_TRUE(density.HasValue());
    EXPECT_FLOAT_EQ(density.GetValue(), 2.0F);
    auto scale = first.GetDisplayScale();
    ASSERT_TRUE(scale.HasValue());
    EXPECT_FLOAT_EQ(scale.GetValue(), 1.75F);

    auto displays = runtime.EnumerateDisplays();
    ASSERT_TRUE(displays.HasValue());
    ASSERT_EQ(displays.GetValue().size(), 1U);
    EXPECT_EQ(displays.GetValue()[0].id, firstDisplay.GetValue());
}

TEST_F(PlatformRuntimeBackendTests, ReportsWindowDisplayAndScaleFailures)
{
    m_fake.connectedDisplayIds = {17};
    m_fake.displays.emplace(17, FakeDisplay{17,
                                            "Window Display",
                                            {0, 0, 1280, 720},
                                            {0, 0, 1280, 680},
                                            60.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.0F});
    m_fake.defaultWindowDisplayId = 17;

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto configureFailure = [this](std::string operation)
    {
        m_fake.failingDisplayOperation = std::move(operation);
        m_fake.displayFailuresRemaining = 1;
    };
    const auto expectBackendFailure = [](const auto& result, std::string_view operation)
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
        EXPECT_NE(result.GetError().GetMessage().find(operation), std::string_view::npos);
        EXPECT_NE(result.GetError().GetMessage().find("window"), std::string_view::npos);
    };

    configureFailure("get-display-for-window");
    expectBackendFailure(window.GetDisplayId(), "SDL_GetDisplayForWindow");
    configureFailure("get-window-pixel-density");
    expectBackendFailure(window.GetPixelDensity(), "SDL_GetWindowPixelDensity");
    configureFailure("get-window-display-scale");
    expectBackendFailure(window.GetDisplayScale(), "SDL_GetWindowDisplayScale");

    m_fake.windows.front().backendDisplayId = 999;
    auto disconnected = window.GetDisplayId();
    ASSERT_FALSE(disconnected.HasValue());
    EXPECT_EQ(disconnected.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::NotFound));

    m_fake.windows.front().pixelDensity = -1.0F;
    expectBackendFailure(window.GetPixelDensity(), "SDL_GetWindowPixelDensity");
    m_fake.windows.front().pixelDensity = 1.0F;
    m_fake.windows.front().displayScale = std::numeric_limits<float>::quiet_NaN();
    expectBackendFailure(window.GetDisplayScale(), "SDL_GetWindowDisplayScale");
}

TEST_F(PlatformRuntimeBackendTests, GuardsNewDisplayApisAfterMovesAndAcrossThreads)
{
    m_fake.connectedDisplayIds = {5};
    m_fake.displays.emplace(5, FakeDisplay{5,
                                           "Display",
                                           {0, 0, 1024, 768},
                                           {0, 0, 1024, 728},
                                           60.0F,
                                           BackendDisplayOrientation::Landscape,
                                           1.0F});
    m_fake.defaultWindowDisplayId = 5;

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    auto initialDisplays = runtime.EnumerateDisplays();
    ASSERT_TRUE(initialDisplays.HasValue());
    const pond::platform::DisplayId displayId = initialDisplays.GetValue().front().id;

    pond::platform::PlatformRuntime movedRuntime = std::move(runtime);
    const std::size_t movedRuntimeCallCount = m_fake.calls.size();
    EXPECT_THROW(static_cast<void>(runtime.EnumerateDisplays()), pond::core::PonderException);
    EXPECT_THROW(static_cast<void>(runtime.GetDisplayInfo(displayId)), pond::core::PonderException);
    EXPECT_EQ(m_fake.calls.size(), movedRuntimeCallCount);

    std::atomic_bool rejectedRuntimeThread{false};
    std::thread runtimeWorker{[&movedRuntime, &rejectedRuntimeThread]()
                              {
                                  try
                                  {
                                      static_cast<void>(movedRuntime.EnumerateDisplays());
                                  }
                                  catch (const pond::core::PonderException&)
                                  {
                                      rejectedRuntimeThread.store(true);
                                  }
                              }};
    runtimeWorker.join();
    EXPECT_TRUE(rejectedRuntimeThread.load());
    EXPECT_EQ(m_fake.calls.size(), movedRuntimeCallCount);
}

TEST_F(PlatformRuntimeBackendTests, GuardsNewWindowDisplayApisAfterMoveAndAcrossThreads)
{
    m_fake.connectedDisplayIds = {6};
    m_fake.displays.emplace(6, FakeDisplay{6,
                                           "Display",
                                           {0, 0, 1024, 768},
                                           {0, 0, 1024, 728},
                                           60.0F,
                                           BackendDisplayOrientation::Landscape,
                                           1.0F});
    m_fake.defaultWindowDisplayId = 6;

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    pond::platform::Window movedWindow = std::move(window);

    const std::size_t callCount = m_fake.calls.size();
    EXPECT_THROW(static_cast<void>(window.GetDisplayId()), pond::core::PonderException);
    EXPECT_THROW(static_cast<void>(window.GetPixelDensity()), pond::core::PonderException);
    EXPECT_THROW(static_cast<void>(window.GetDisplayScale()), pond::core::PonderException);
    EXPECT_THROW(static_cast<void>(window.GetNativeHandle()), pond::core::PonderException);
    EXPECT_EQ(m_fake.calls.size(), callCount);

    std::atomic_int rejectedCalls{};
    std::thread worker{[&movedWindow, &rejectedCalls]()
                       {
                           try
                           {
                               static_cast<void>(movedWindow.GetDisplayId());
                           }
                           catch (const pond::core::PonderException&)
                           {
                               ++rejectedCalls;
                           }
                           try
                           {
                               static_cast<void>(movedWindow.GetPixelDensity());
                           }
                           catch (const pond::core::PonderException&)
                           {
                               ++rejectedCalls;
                           }
                           try
                           {
                               static_cast<void>(movedWindow.GetDisplayScale());
                           }
                           catch (const pond::core::PonderException&)
                           {
                               ++rejectedCalls;
                           }
                           try
                           {
                               static_cast<void>(movedWindow.GetNativeHandle());
                           }
                           catch (const pond::core::PonderException&)
                           {
                               ++rejectedCalls;
                           }
                       }};
    worker.join();

    EXPECT_EQ(rejectedCalls.load(), 4);
    EXPECT_EQ(m_fake.calls.size(), callCount);
}

TEST_F(PlatformRuntimeBackendTests, ExposesNativeHandlesForApprovedDrivers)
{
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    desc.graphicsCompatibility = pond::platform::WindowGraphicsCompatibility::Vulkan;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    m_fake.nativeVideoDriver = "windows";
    auto win32Result = window.GetNativeHandle();
    ASSERT_TRUE(win32Result.HasValue()) << win32Result.GetError().GetMessage();
    ASSERT_TRUE(std::holds_alternative<pond::platform::NativeWin32Window>(win32Result.GetValue()));
    EXPECT_EQ(std::get<pond::platform::NativeWin32Window>(win32Result.GetValue()),
              (pond::platform::NativeWin32Window{.instance = m_fake.nativeWin32Instance,
                                                 .window = m_fake.nativeWin32Window}));

    m_fake.nativeVideoDriver = "x11";
    auto x11Result = window.GetNativeHandle();
    ASSERT_TRUE(x11Result.HasValue()) << x11Result.GetError().GetMessage();
    ASSERT_TRUE(std::holds_alternative<pond::platform::NativeX11Window>(x11Result.GetValue()));
    EXPECT_EQ(std::get<pond::platform::NativeX11Window>(x11Result.GetValue()),
              (pond::platform::NativeX11Window{.display = m_fake.nativeX11Display,
                                               .window = m_fake.nativeX11Window}));

    m_fake.nativeVideoDriver = "wayland";
    auto waylandResult = window.GetNativeHandle();
    ASSERT_TRUE(waylandResult.HasValue()) << waylandResult.GetError().GetMessage();
    ASSERT_TRUE(
        std::holds_alternative<pond::platform::NativeWaylandWindow>(waylandResult.GetValue()));
    EXPECT_EQ(std::get<pond::platform::NativeWaylandWindow>(waylandResult.GetValue()),
              (pond::platform::NativeWaylandWindow{.display = m_fake.nativeWaylandDisplay,
                                                   .surface = m_fake.nativeWaylandSurface}));
#else
    GTEST_SKIP() << "Vulkan native-window payloads are supported only on Windows and Linux.";
#endif
}

TEST_F(PlatformRuntimeBackendTests, RejectsNativeHandlesForDefaultAndUnsupportedDrivers)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc defaultDesc;
    defaultDesc.visible = false;
    auto defaultWindowResult = runtime.CreateWindow(defaultDesc);
    ASSERT_TRUE(defaultWindowResult.HasValue());
    pond::platform::Window defaultWindow = std::move(defaultWindowResult).GetValue();

    const std::size_t callCount = m_fake.calls.size();
    auto invalidResult = defaultWindow.GetNativeHandle();
    ASSERT_FALSE(invalidResult.HasValue());
    EXPECT_EQ(invalidResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));
    EXPECT_EQ(m_fake.calls.size(), callCount);

#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
    pond::platform::WindowDesc vulkanDesc;
    vulkanDesc.visible = false;
    vulkanDesc.graphicsCompatibility = pond::platform::WindowGraphicsCompatibility::Vulkan;
    auto vulkanWindowResult = runtime.CreateWindow(vulkanDesc);
    ASSERT_TRUE(vulkanWindowResult.HasValue());
    pond::platform::Window vulkanWindow = std::move(vulkanWindowResult).GetValue();

    m_fake.nativeVideoDriver = "dummy";
    auto unsupportedResult = vulkanWindow.GetNativeHandle();
    ASSERT_FALSE(unsupportedResult.HasValue());
    EXPECT_EQ(unsupportedResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));
#elif defined(SDL_PLATFORM_MACOS)
    pond::platform::WindowDesc metalDesc;
    metalDesc.visible = false;
    metalDesc.graphicsCompatibility = pond::platform::WindowGraphicsCompatibility::Metal;
    auto metalWindowResult = runtime.CreateWindow(metalDesc);
    ASSERT_TRUE(metalWindowResult.HasValue());
    pond::platform::Window metalWindow = std::move(metalWindowResult).GetValue();

    auto unsupportedResult = metalWindow.GetNativeHandle();
    ASSERT_FALSE(unsupportedResult.HasValue());
    EXPECT_EQ(unsupportedResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));
    EXPECT_NE(unsupportedResult.GetError().GetMessage().find("Metal"), std::string::npos);
    EXPECT_EQ(std::ranges::count(m_fake.calls, "get-native-handle"), 0);
#endif
}

TEST_F(PlatformRuntimeBackendTests, RejectsGraphicsCompatibilityUnavailableOnCurrentHost)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
    desc.graphicsCompatibility = pond::platform::WindowGraphicsCompatibility::Metal;
#else
    desc.graphicsCompatibility = pond::platform::WindowGraphicsCompatibility::Vulkan;
#endif

    const int createWindowCalls = m_fake.createWindowCalls;
    auto result = runtime.CreateWindow(desc);
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));
    EXPECT_EQ(m_fake.createWindowCalls, createWindowCalls);
}

TEST_F(PlatformRuntimeBackendTests, ReportsNativeHandleBackendFailures)
{
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    desc.graphicsCompatibility = pond::platform::WindowGraphicsCompatibility::Vulkan;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    m_fake.failingWindowOperation = "get-native-handle";
    m_fake.windowFailuresRemaining = 1;
    auto failedResult = window.GetNativeHandle();
    ASSERT_FALSE(failedResult.HasValue());
    EXPECT_EQ(failedResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(failedResult.GetError().GetMessage().find("get-native-handle"), std::string::npos);

    m_fake.nativeWin32Window = nullptr;
    auto missingResult = window.GetNativeHandle();
    ASSERT_FALSE(missingResult.HasValue());
    EXPECT_EQ(missingResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(missingResult.GetError().GetMessage().find("missing fake Win32"), std::string::npos);
#else
    GTEST_SKIP() << "Vulkan native-window payloads are supported only on Windows and Linux.";
#endif
}

TEST_F(PlatformRuntimeBackendTests, PollingContinuesPastIgnoredEventsUntilAProjectEventIsAvailable)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    SDL_Event unknown{};
    unknown.type = SDL_EVENT_USER;
    unknown.common.timestamp = 100;
    m_fake.eventQueue.push_back({.event = unknown});

    SDL_Event deferred{};
    deferred.key.type = SDL_EVENT_KEY_DOWN;
    deferred.key.timestamp = 200;
    deferred.key.windowID = 999;
    m_fake.eventQueue.push_back({.event = deferred});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_RESIZED, 999, -1, 480, 300)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED, 999, 0, 0, 400)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDisplayEvent(SDL_EVENT_DISPLAY_MOVED, 999, 0, 0, 500)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedQuitEvent(std::numeric_limits<std::uint64_t>::max())});
    m_fake.eventQueue.push_back({.event = MakeQueuedQuitEvent(700)});

    ExpectPolledEvent(runtime.PollEvent(), pond::platform::QuitRequestedEvent{
                                               .timestamp = pond::platform::Timestamp{
                                                   std::chrono::nanoseconds{700}}});
    EXPECT_TRUE(m_fake.eventQueue.empty());
    EXPECT_EQ(m_fake.pollEventCalls, 7);

    EXPECT_FALSE(runtime.PollEvent().has_value());
    EXPECT_EQ(m_fake.pollEventCalls, 8);
}

TEST_F(PlatformRuntimeBackendTests, PollingReturnsEmptyOnlyAfterDrainingEveryIgnoredEvent)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    SDL_Event unknown{};
    unknown.type = SDL_EVENT_USER;
    unknown.common.timestamp = 100;
    m_fake.eventQueue.push_back({.event = unknown});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED, 404, 0, 0, 200)});

    EXPECT_FALSE(runtime.PollEvent().has_value());
    EXPECT_TRUE(m_fake.eventQueue.empty());
    EXPECT_EQ(m_fake.pollEventCalls, 3);
}

TEST_F(PlatformRuntimeBackendTests, RoutesInterleavedEventsForMultipleWindows)
{
    m_fake.scriptedBackendWindowIds = {41, 73};
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto firstResult = runtime.CreateWindow(desc);
    auto secondResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(firstResult.HasValue());
    ASSERT_TRUE(secondResult.HasValue());
    pond::platform::Window first = std::move(firstResult).GetValue();
    pond::platform::Window second = std::move(secondResult).GetValue();
    ASSERT_EQ(first.GetId(), pond::platform::WindowId{1});
    ASSERT_EQ(second.GetId(), pond::platform::WindowId{2});

    m_fake.eventQueue.push_back(
        {.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_MOVED, 73, -250, 90, 100)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_FOCUS_GAINED, 41, 0, 0, 200)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED, 73, 0, 0, 300)});

    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowMovedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{100}},
            .windowId = second.GetId(),
            .position = {-250, 90}});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowFocusChangedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{200}},
            .windowId = first.GetId(),
            .focused = true});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowCloseRequestedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{300}},
            .windowId = second.GetId()});
    EXPECT_EQ(m_fake.destroyWindowCalls, 0);
    EXPECT_EQ(first.GetId(), pond::platform::WindowId{1});
    EXPECT_EQ(second.GetId(), pond::platform::WindowId{2});
}

TEST_F(PlatformRuntimeBackendTests,
       KeepsGlobalQuitSeparateFromWindowCloseWithoutDestroyingTheWindow)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    const std::uint32_t backendWindowId = m_fake.windows.front().backendId;

    m_fake.eventQueue.push_back({.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED,
                                                                backendWindowId, 0, 0, 100)});
    m_fake.eventQueue.push_back({.event = MakeQueuedQuitEvent(200)});

    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowCloseRequestedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{100}},
            .windowId = window.GetId()});
    EXPECT_EQ(m_fake.destroyWindowCalls, 0);
    EXPECT_EQ(window.GetId(), pond::platform::WindowId{1});

    ExpectPolledEvent(runtime.PollEvent(), pond::platform::QuitRequestedEvent{
                                               .timestamp = pond::platform::Timestamp{
                                                   std::chrono::nanoseconds{200}}});
    EXPECT_EQ(m_fake.destroyWindowCalls, 0);
}

TEST_F(PlatformRuntimeBackendTests, PollingShownInvalidatesTheHiddenStateDisambiguationMarker)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    const std::uint32_t backendWindowId = m_fake.windows.front().backendId;

    ASSERT_TRUE(window.Maximize().HasValue());
    FakeWindow& backendWindow = m_fake.windows.front();
    ASSERT_TRUE(backendWindow.pendingMaximized);

    backendWindow.visible = true;
    backendWindow.maximized = true;
    backendWindow.pendingMaximized = false;
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_SHOWN, backendWindowId, 0, 0, 100)});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowVisibilityChangedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{100}},
            .windowId = window.GetId(),
            .visible = true});

    backendWindow.visible = false;
    backendWindow.maximized = false;
    auto state = window.GetState();
    ASSERT_TRUE(state.HasValue());
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Normal);
}

TEST_F(PlatformRuntimeBackendTests, DelayedShownEventPreservesNewerHiddenStateRequest)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    FakeWindow& backendWindow = m_fake.windows.front();
    const std::uint32_t backendWindowId = backendWindow.backendId;

    ASSERT_TRUE(window.Show().HasValue());
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_SHOWN, backendWindowId, 0, 0, 100)});
    ASSERT_TRUE(window.Maximize().HasValue());
    ASSERT_TRUE(window.Hide().HasValue());
    ASSERT_TRUE(window.Minimize().HasValue());
    ASSERT_TRUE(backendWindow.maximized);
    ASSERT_TRUE(backendWindow.pendingMinimized);

    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowVisibilityChangedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{100}},
            .windowId = window.GetId(),
            .visible = true});

    auto state = window.GetState();
    ASSERT_TRUE(state.HasValue()) << state.GetError().GetMessage();
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Minimized);
}

TEST_F(PlatformRuntimeBackendTests, DelayedShownEventDoesNotOverwriteNewerVisibleStateRequest)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    FakeWindow& backendWindow = m_fake.windows.front();
    const std::uint32_t backendWindowId = backendWindow.backendId;

    ASSERT_TRUE(window.Maximize().HasValue());
    ASSERT_TRUE(backendWindow.pendingMaximized);

    backendWindow.visible = true;
    backendWindow.maximized = true;
    backendWindow.pendingMaximized = false;
    ASSERT_TRUE(window.Restore().HasValue());
    ASSERT_FALSE(backendWindow.pendingMaximized);
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_SHOWN, backendWindowId, 0, 0, 100)});

    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowVisibilityChangedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{100}},
            .windowId = window.GetId(),
            .visible = true});

    ASSERT_TRUE(window.Hide().HasValue());
    auto state = window.GetState();
    ASSERT_TRUE(state.HasValue()) << state.GetError().GetMessage();
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Normal);
}

TEST_F(PlatformRuntimeBackendTests, IgnoresDestroyedWindowEventsAndDoesNotReuseProjectWindowIds)
{
    m_fake.scriptedBackendWindowIds = {55, 55};
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto firstResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(firstResult.HasValue());
    std::optional<pond::platform::Window> first;
    first.emplace(std::move(firstResult).GetValue());
    const pond::platform::WindowId firstId = first->GetId();
    first.reset();
    ASSERT_EQ(m_fake.destroyWindowCalls, 1);

    m_fake.eventQueue.push_back(
        {.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED, 55, 0, 0, 100)});
    m_fake.eventQueue.push_back({.event = MakeQueuedQuitEvent(200)});
    ExpectPolledEvent(runtime.PollEvent(), pond::platform::QuitRequestedEvent{
                                               .timestamp = pond::platform::Timestamp{
                                                   std::chrono::nanoseconds{200}}});
    EXPECT_EQ(m_fake.pollEventCalls, 2);

    auto secondResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(secondResult.HasValue());
    pond::platform::Window second = std::move(secondResult).GetValue();
    EXPECT_EQ(second.GetId(), pond::platform::WindowId{2});
    EXPECT_NE(second.GetId(), firstId);

    m_fake.eventQueue.push_back(
        {.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_MOVED, 55, 8, 9, 300)});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowMovedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{300}},
            .windowId = second.GetId(),
            .position = {8, 9}});
}

TEST_F(PlatformRuntimeBackendTests, RoutesDisplayAdditionRemovalAndReconnectionWithStableTombstones)
{
    m_fake.connectedDisplayIds = {10};
    m_fake.displays.emplace(10, FakeDisplay{10,
                                            "Original",
                                            {0, 0, 1280, 720},
                                            {0, 0, 1280, 680},
                                            60.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.0F});
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto initialDisplays = runtime.EnumerateDisplays();
    ASSERT_TRUE(initialDisplays.HasValue());
    ASSERT_EQ(initialDisplays.GetValue().size(), 1U);
    const pond::platform::DisplayId originalId = initialDisplays.GetValue().front().id;
    EXPECT_EQ(originalId, pond::platform::DisplayId{1});

    m_fake.displays.emplace(20, FakeDisplay{20,
                                            "Added",
                                            {1280, 0, 1920, 1080},
                                            {1280, 0, 1920, 1040},
                                            120.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.5F});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDisplayEvent(SDL_EVENT_DISPLAY_ADDED, 20, 0, 0, 100),
         .connectedDisplayIds = std::vector<std::uint32_t>{10, 20}});
    std::optional<pond::platform::PlatformEvent> added = runtime.PollEvent();
    ASSERT_TRUE(added.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::DisplayAddedEvent>(*added));
    const pond::platform::DisplayId addedId =
        std::get<pond::platform::DisplayAddedEvent>(*added).displayId;
    EXPECT_EQ(addedId, pond::platform::DisplayId{2});
    EXPECT_TRUE(runtime.GetDisplayInfo(addedId).HasValue());

    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDisplayEvent(SDL_EVENT_DISPLAY_REMOVED, 10, 0, 0, 200),
         .connectedDisplayIds = std::vector<std::uint32_t>{20}});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::DisplayRemovedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{200}},
            .displayId = originalId});

    auto stale = runtime.GetDisplayInfo(originalId);
    ASSERT_FALSE(stale.HasValue());
    EXPECT_EQ(stale.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::NotFound));

    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDisplayEvent(SDL_EVENT_DISPLAY_ADDED, 10, 0, 0, 300),
         .connectedDisplayIds = std::vector<std::uint32_t>{20, 10}});
    std::optional<pond::platform::PlatformEvent> readded = runtime.PollEvent();
    ASSERT_TRUE(readded.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::DisplayAddedEvent>(*readded));
    const pond::platform::DisplayId readdedId =
        std::get<pond::platform::DisplayAddedEvent>(*readded).displayId;
    EXPECT_EQ(readdedId, pond::platform::DisplayId{3});
    EXPECT_NE(readdedId, originalId);
    EXPECT_TRUE(runtime.GetDisplayInfo(readdedId).HasValue());

    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDisplayEvent(SDL_EVENT_DISPLAY_REMOVED, 999, 0, 0, 400),
         .connectedDisplayIds = std::vector<std::uint32_t>{20, 10}});
    m_fake.eventQueue.push_back({.event = MakeQueuedQuitEvent(500)});
    ExpectPolledEvent(runtime.PollEvent(), pond::platform::QuitRequestedEvent{
                                               .timestamp = pond::platform::Timestamp{
                                                   std::chrono::nanoseconds{500}}});
}

TEST_F(PlatformRuntimeBackendTests, RoutesInitialDisplayEventsBeforeAnyDisplayQuery)
{
    m_fake.connectedDisplayIds = {31};
    m_fake.displays.emplace(31, FakeDisplay{31,
                                            "Initial",
                                            {0, 0, 1920, 1080},
                                            {0, 0, 1920, 1040},
                                            60.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.0F});
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDisplayEvent(SDL_EVENT_DISPLAY_MOVED, 31, 0, 0, 100)});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::DisplayMovedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{100}},
            .displayId = pond::platform::DisplayId{1}});

    auto displays = runtime.EnumerateDisplays();
    ASSERT_TRUE(displays.HasValue());
    ASSERT_EQ(displays.GetValue().size(), 1U);
    EXPECT_EQ(displays.GetValue().front().id, pond::platform::DisplayId{1});
}

TEST_F(PlatformRuntimeBackendTests, RetainsDisplayIdentityWhenAQueryObservesRemovalBeforePolling)
{
    m_fake.connectedDisplayIds = {10};
    m_fake.displays.emplace(10, FakeDisplay{10,
                                            "Original",
                                            {0, 0, 1280, 720},
                                            {0, 0, 1280, 680},
                                            60.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.0F});
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto initialDisplays = runtime.EnumerateDisplays();
    ASSERT_TRUE(initialDisplays.HasValue());
    ASSERT_EQ(initialDisplays.GetValue().size(), 1U);
    const pond::platform::DisplayId removedId = initialDisplays.GetValue().front().id;

    m_fake.connectedDisplayIds.clear();
    auto disconnectedDisplays = runtime.EnumerateDisplays();
    ASSERT_TRUE(disconnectedDisplays.HasValue());
    EXPECT_TRUE(disconnectedDisplays.GetValue().empty());

    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDisplayEvent(SDL_EVENT_DISPLAY_REMOVED, 10, 0, 0, 100)});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::DisplayRemovedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{100}},
            .displayId = removedId});

    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDisplayEvent(SDL_EVENT_DISPLAY_REMOVED, 10, 0, 0, 200)});
    m_fake.eventQueue.push_back({.event = MakeQueuedQuitEvent(300)});
    ExpectPolledEvent(runtime.PollEvent(), pond::platform::QuitRequestedEvent{
                                               .timestamp = pond::platform::Timestamp{
                                                   std::chrono::nanoseconds{300}}});
}

TEST_F(PlatformRuntimeBackendTests, ReusesDisplayIdentityWhenAQueryObservesAdditionBeforePolling)
{
    m_fake.connectedDisplayIds = {10};
    m_fake.displays.emplace(10, FakeDisplay{10,
                                            "Original",
                                            {0, 0, 1280, 720},
                                            {0, 0, 1280, 680},
                                            60.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.0F});
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto initialDisplays = runtime.EnumerateDisplays();
    ASSERT_TRUE(initialDisplays.HasValue());
    ASSERT_EQ(initialDisplays.GetValue().size(), 1U);

    m_fake.displays.emplace(20, FakeDisplay{20,
                                            "Added",
                                            {1280, 0, 1920, 1080},
                                            {1280, 0, 1920, 1040},
                                            120.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.5F});
    m_fake.connectedDisplayIds = {10, 20};
    auto refreshedDisplays = runtime.EnumerateDisplays();
    ASSERT_TRUE(refreshedDisplays.HasValue());
    ASSERT_EQ(refreshedDisplays.GetValue().size(), 2U);
    const pond::platform::DisplayId refreshedId = refreshedDisplays.GetValue()[1].id;

    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDisplayEvent(SDL_EVENT_DISPLAY_ADDED, 20, 0, 0, 100)});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::DisplayAddedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{100}},
            .displayId = refreshedId});
}

TEST_F(PlatformRuntimeBackendTests,
       RoutesOptionalWindowDestinationDisplaysThroughTheRuntimeRegistry)
{
    m_fake.connectedDisplayIds = {31};
    m_fake.displays.emplace(31, FakeDisplay{31,
                                            "Destination",
                                            {0, 0, 1920, 1080},
                                            {0, 0, 1920, 1040},
                                            60.0F,
                                            BackendDisplayOrientation::Landscape,
                                            1.0F});
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto displays = runtime.EnumerateDisplays();
    ASSERT_TRUE(displays.HasValue());
    ASSERT_EQ(displays.GetValue().size(), 1U);
    const pond::platform::DisplayId displayId = displays.GetValue().front().id;

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    const std::uint32_t backendWindowId = m_fake.windows.front().backendId;

    m_fake.eventQueue.push_back({.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_DISPLAY_CHANGED,
                                                                backendWindowId, 31, 0, 100)});
    m_fake.eventQueue.push_back({.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_DISPLAY_CHANGED,
                                                                backendWindowId, 999, 0, 200)});
    m_fake.eventQueue.push_back({.event = MakeQueuedWindowEvent(SDL_EVENT_WINDOW_DISPLAY_CHANGED,
                                                                backendWindowId, 0, 0, 300)});

    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowDisplayChangedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{100}},
            .windowId = window.GetId(),
            .displayId = displayId});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowDisplayChangedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{200}},
            .windowId = window.GetId(),
            .displayId = std::nullopt});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowDisplayChangedEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{300}},
            .windowId = window.GetId(),
            .displayId = std::nullopt});
}

TEST_F(PlatformRuntimeBackendTests, RoutesInterleavedKeyboardTextAndCompositionEvents)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto firstResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(firstResult.HasValue());
    pond::platform::Window first = std::move(firstResult).GetValue();
    const std::uint32_t firstBackendId = m_fake.windows.back().backendId;

    auto secondResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(secondResult.HasValue());
    pond::platform::Window second = std::move(secondResult).GetValue();
    const std::uint32_t secondBackendId = m_fake.windows.back().backendId;

    const std::string inputText{"\xF0\x9F\xA7\xAA", 4};
    const std::string compositionText{"mol"};
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedKeyboardEvent(SDL_EVENT_KEY_DOWN, 999, SDL_SCANCODE_Z, SDLK_Z)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedKeyboardEvent(
             SDL_EVENT_KEY_DOWN, firstBackendId, SDL_SCANCODE_A, SDLK_A,
             static_cast<SDL_Keymod>(SDL_KMOD_LCTRL | SDL_KMOD_CAPS), true, 100)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedTextInputEvent(secondBackendId, inputText.c_str(), 200)});
    m_fake.eventQueue.push_back({.event = MakeQueuedTextCompositionEvent(
                                     firstBackendId, compositionText.c_str(), 0, 0, 300)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedKeyboardEvent(SDL_EVENT_KEY_UP, 0, SDL_SCANCODE_UNKNOWN, SDLK_UNKNOWN,
                                          SDL_KMOD_NONE, false, 400)});

    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::KeyboardKeyEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{100}},
            .windowId = first.GetId(),
            .physicalKey = pond::platform::PhysicalKey::A,
            .logicalKey = pond::platform::LogicalKey::FromCharacter(U'a'),
            .modifiers =
                pond::platform::KeyModifiers::LeftControl | pond::platform::KeyModifiers::CapsLock,
            .pressed = true,
            .repeat = true});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::TextInputEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{200}},
            .windowId = second.GetId(),
            .text = inputText});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::TextCompositionEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{300}},
            .windowId = first.GetId(),
            .text = compositionText,
            .selection = pond::platform::TextCompositionRange{0, 0}});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::KeyboardKeyEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{400}},
            .windowId = std::nullopt,
            .physicalKey = pond::platform::PhysicalKey::Unknown,
            .logicalKey = pond::platform::LogicalKey::Unknown(),
            .modifiers = pond::platform::KeyModifiers::None,
            .pressed = false,
            .repeat = false});
    EXPECT_EQ(m_fake.pollEventCalls, 5);
}

TEST_F(PlatformRuntimeBackendTests, ExposesIdempotentTextInputLifecycleAndImeArea)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    EXPECT_FALSE(window.IsTextInputActive());
    ASSERT_TRUE(window.StartTextInput().HasValue());
    EXPECT_TRUE(window.IsTextInputActive());
    EXPECT_EQ(std::ranges::count(m_fake.calls, "start-text-input"), 1);

    ASSERT_TRUE(window.StartTextInput().HasValue());
    EXPECT_EQ(std::ranges::count(m_fake.calls, "start-text-input"), 1);

    const pond::platform::TextInputArea area{.rectangle = {{12.4F, -6.6F}, {100.5F, 20.4F}},
                                             .cursorOffset = 3.5F};
    ASSERT_TRUE(window.SetTextInputArea(area).HasValue());
    ASSERT_TRUE(m_fake.windows.front().textInputArea.has_value());
    const BackendTextInputArea& backendArea = *m_fake.windows.front().textInputArea;
    EXPECT_EQ(backendArea.x, 12);
    EXPECT_EQ(backendArea.y, -7);
    EXPECT_EQ(backendArea.width, 101);
    EXPECT_EQ(backendArea.height, 20);
    EXPECT_EQ(backendArea.cursorOffset, 4);

    ASSERT_TRUE(window.ClearTextComposition().HasValue());
    EXPECT_EQ(m_fake.windows.front().clearedTextCompositions, 1);

    ASSERT_TRUE(window.ClearTextInputArea().HasValue());
    EXPECT_FALSE(m_fake.windows.front().textInputArea.has_value());

    ASSERT_TRUE(window.StopTextInput().HasValue());
    EXPECT_FALSE(window.IsTextInputActive());
    EXPECT_EQ(std::ranges::count(m_fake.calls, "stop-text-input"), 1);

    ASSERT_TRUE(window.StopTextInput().HasValue());
    EXPECT_EQ(std::ranges::count(m_fake.calls, "stop-text-input"), 1);
}

TEST_F(PlatformRuntimeBackendTests, RejectsInvalidTextInputAreasBeforeBackendCalls)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    constexpr float kMaximumFloat = std::numeric_limits<float>::max();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const std::array<pond::platform::TextInputArea, 4> invalidAreas{{
        {.rectangle = {{nan, 0.0F}, {1.0F, 1.0F}}, .cursorOffset = 0.0F},
        {.rectangle = {{0.0F, 0.0F}, {-1.0F, 1.0F}}, .cursorOffset = 0.0F},
        {.rectangle = {{kMaximumFloat, 0.0F}, {1.0F, 1.0F}}, .cursorOffset = 0.0F},
        {.rectangle = {{0.0F, 0.0F}, {1.0F, 1.0F}}, .cursorOffset = infinity},
    }};

    const auto callsBefore = m_fake.calls;
    for (const pond::platform::TextInputArea area : invalidAreas)
    {
        const pond::core::VoidResult result = window.SetTextInputArea(area);
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));
    }
    EXPECT_EQ(m_fake.calls, callsBefore);
    EXPECT_FALSE(m_fake.windows.front().textInputArea.has_value());
}

TEST_F(PlatformRuntimeBackendTests, ReportsContextualTextInputBackendFailures)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto expectBackendFailure =
        [](const pond::core::VoidResult& result, std::string_view operation)
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
        EXPECT_NE(result.GetError().GetMessage().find(operation), std::string_view::npos);
        EXPECT_NE(result.GetError().GetMessage().find("backend window"), std::string_view::npos);
    };

    m_fake.failingWindowOperation = "start-text-input";
    m_fake.windowFailuresRemaining = 1;
    expectBackendFailure(window.StartTextInput(), "SDL_StartTextInput");
    EXPECT_FALSE(window.IsTextInputActive());

    m_fake.failingWindowOperation.clear();
    ASSERT_TRUE(window.StartTextInput().HasValue());
    m_fake.failingWindowOperation = "stop-text-input";
    m_fake.windowFailuresRemaining = 1;
    expectBackendFailure(window.StopTextInput(), "SDL_StopTextInput");
    EXPECT_TRUE(window.IsTextInputActive());

    m_fake.failingWindowOperation = "clear-text-composition";
    m_fake.windowFailuresRemaining = 1;
    expectBackendFailure(window.ClearTextComposition(), "SDL_ClearComposition");

    const pond::platform::TextInputArea area{.rectangle = {{1.0F, 2.0F}, {3.0F, 4.0F}},
                                             .cursorOffset = 1.0F};
    m_fake.failingWindowOperation = "set-text-input-area";
    m_fake.windowFailuresRemaining = 1;
    expectBackendFailure(window.SetTextInputArea(area), "SDL_SetTextInputArea");
    EXPECT_FALSE(m_fake.windows.front().textInputArea.has_value());

    m_fake.windowFailuresRemaining = 1;
    expectBackendFailure(window.ClearTextInputArea(), "SDL_SetTextInputArea");
}

TEST_F(PlatformRuntimeBackendTests, GuardsEveryTextInputApiAfterMoveAndAcrossThreads)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    pond::platform::Window movedWindow = std::move(window);

    const auto callsBefore = m_fake.calls;
    EXPECT_EQ(CountRejectedWindowTextInputCalls(window), 6);
    EXPECT_EQ(m_fake.calls, callsBefore);

    std::atomic_int rejectedCalls{};
    std::thread worker{[&movedWindow, &rejectedCalls]()
                       {
                           rejectedCalls.store(CountRejectedWindowTextInputCalls(movedWindow));
                       }};
    worker.join();
    EXPECT_EQ(rejectedCalls.load(), 6);
    EXPECT_EQ(m_fake.calls, callsBefore);
}

TEST_F(PlatformRuntimeBackendTests, ExposesLiveWindowMouseGrabAndRelativeModeState)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto windowResult = runtime.CreateWindow(pond::platform::WindowDesc{});
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    EXPECT_FALSE(window.IsMouseGrabbed());
    ASSERT_TRUE(window.SetMouseGrab(true).HasValue());
    EXPECT_TRUE(window.IsMouseGrabbed());
    ASSERT_TRUE(window.SetMouseGrab(true).HasValue());
    EXPECT_EQ(std::ranges::count(m_fake.calls, "set-window-mouse-grab"), 2);

    ASSERT_TRUE(window.SetMouseGrab(false).HasValue());
    EXPECT_FALSE(window.IsMouseGrabbed());
    EXPECT_EQ(std::ranges::count(m_fake.calls, "set-window-mouse-grab"), 3);

    EXPECT_FALSE(window.IsRelativeMouseModeEnabled());
    ASSERT_TRUE(window.SetRelativeMouseMode(true).HasValue());
    EXPECT_TRUE(window.IsRelativeMouseModeEnabled());
    ASSERT_TRUE(window.SetRelativeMouseMode(true).HasValue());
    EXPECT_EQ(std::ranges::count(m_fake.calls, "set-window-relative-mouse-mode"), 1);

    ASSERT_TRUE(window.SetRelativeMouseMode(false).HasValue());
    EXPECT_FALSE(window.IsRelativeMouseModeEnabled());
    ASSERT_TRUE(window.SetRelativeMouseMode(false).HasValue());
    EXPECT_EQ(std::ranges::count(m_fake.calls, "set-window-relative-mouse-mode"), 2);
}

TEST_F(PlatformRuntimeBackendTests, RoutesInterleavedMouseEventsAcrossWindowsAndSkipsStaleTargets)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto firstResult = runtime.CreateWindow(pond::platform::WindowDesc{});
    ASSERT_TRUE(firstResult.HasValue());
    pond::platform::Window first = std::move(firstResult).GetValue();
    auto secondResult = runtime.CreateWindow(pond::platform::WindowDesc{});
    ASSERT_TRUE(secondResult.HasValue());
    pond::platform::Window second = std::move(secondResult).GetValue();

    const std::uint32_t firstBackendId = m_fake.windows.front().backendId;
    const std::uint32_t secondBackendId = m_fake.windows.back().backendId;
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedMouseMotionEvent(firstBackendId, 10.5F, -2.25F, 1.5F, -0.5F, 100)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedMouseButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN, secondBackendId,
                                             SDL_BUTTON_X2, 3.0F, 4.0F, 200)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedMouseWheelEvent(firstBackendId, -0.5F, 1.25F, 8.0F, 9.0F, 300)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedMouseMotionEvent(999, 1.0F, 2.0F, 3.0F, 4.0F, 400)});
    m_fake.eventQueue.push_back({.event = MakeQueuedQuitEvent(500)});

    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::MouseMotionEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{100}},
            .windowId = first.GetId(),
            .position = {10.5F, -2.25F},
            .relativeMovement = {1.5F, -0.5F}});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::MouseButtonEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{200}},
            .windowId = second.GetId(),
            .position = {3.0F, 4.0F},
            .button = pond::platform::MouseButton::X2,
            .pressed = true});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::MouseWheelEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{300}},
            .windowId = first.GetId(),
            .position = {8.0F, 9.0F},
            .horizontal = -0.5F,
            .vertical = 1.25F});
    ExpectPolledEvent(runtime.PollEvent(), pond::platform::QuitRequestedEvent{
                                               .timestamp = pond::platform::Timestamp{
                                                   std::chrono::nanoseconds{500}}});
    EXPECT_EQ(m_fake.pollEventCalls, 5);
}

TEST_F(PlatformRuntimeBackendTests, RoutesInterleavedDropEventsAcrossWindowsAndSkipsStaleTargets)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto firstResult = runtime.CreateWindow(pond::platform::WindowDesc{});
    ASSERT_TRUE(firstResult.HasValue());
    pond::platform::Window first = std::move(firstResult).GetValue();
    auto secondResult = runtime.CreateWindow(pond::platform::WindowDesc{});
    ASSERT_TRUE(secondResult.HasValue());
    pond::platform::Window second = std::move(secondResult).GetValue();

    const std::uint32_t firstBackendId = m_fake.windows.front().backendId;
    const std::uint32_t secondBackendId = m_fake.windows.back().backendId;
    const std::string source{"source-app"};
    const std::string path{"C:/tmp/dropped.sdf"};
    const std::string text{"C1=CC=CC=C1"};

    m_fake.eventQueue.push_back({.event = MakeQueuedDropEvent(SDL_EVENT_DROP_TEXT, 999, "stale",
                                                              source.c_str(), 1.0F, 2.0F, 50)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDropEvent(SDL_EVENT_DROP_BEGIN, firstBackendId, nullptr, source.c_str(),
                                      0.0F, 0.0F, 100)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDropEvent(SDL_EVENT_DROP_FILE, secondBackendId, path.c_str(),
                                      source.c_str(), 3.0F, 4.0F, 200)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDropEvent(SDL_EVENT_DROP_TEXT, secondBackendId, text.c_str(), nullptr,
                                      5.0F, 6.0F, 300)});
    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDropEvent(SDL_EVENT_DROP_POSITION, firstBackendId, nullptr,
                                      source.c_str(), 7.0F, 8.0F, 400)});
    m_fake.eventQueue.push_back({.event = MakeQueuedDropEvent(SDL_EVENT_DROP_COMPLETE, 0, nullptr,
                                                              nullptr, 9.0F, 10.0F, 500)});
    m_fake.eventQueue.push_back({.event = MakeQueuedQuitEvent(600)});

    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::DropBeginEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{100}},
            .windowId = first.GetId(),
            .sourceApplication = std::optional<std::string>{"source-app"}});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::DroppedFileEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{200}},
            .windowId = second.GetId(),
            .path = std::filesystem::path{"C:/tmp/dropped.sdf"},
            .position = {3.0F, 4.0F},
            .sourceApplication = std::optional<std::string>{"source-app"}});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::DroppedTextEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{300}},
            .windowId = second.GetId(),
            .text = text,
            .position = {5.0F, 6.0F},
            .sourceApplication = std::nullopt});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::DropPositionEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{400}},
            .windowId = first.GetId(),
            .position = {7.0F, 8.0F},
            .sourceApplication = std::optional<std::string>{"source-app"}});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::DropCompleteEvent{
            .timestamp = pond::platform::Timestamp{std::chrono::nanoseconds{500}},
            .windowId = std::nullopt,
            .position = {9.0F, 10.0F},
            .sourceApplication = std::nullopt});
    ExpectPolledEvent(runtime.PollEvent(), pond::platform::QuitRequestedEvent{
                                               .timestamp = pond::platform::Timestamp{
                                                   std::chrono::nanoseconds{600}}});
    EXPECT_EQ(m_fake.pollEventCalls, 7);
}

TEST_F(PlatformRuntimeBackendTests, ReportsWindowMouseFailuresWithoutCachingRequestedState)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto windowResult = runtime.CreateWindow(pond::platform::WindowDesc{});
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto expectError = [](const pond::core::VoidResult& result,
                                pond::platform::PlatformErrorCode code, std::string_view operation)
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(), pond::platform::ToErrorCode(code));
        EXPECT_NE(result.GetError().GetMessage().find(operation), std::string_view::npos);
        EXPECT_NE(result.GetError().GetMessage().find("backend window"), std::string_view::npos);
    };

    m_fake.failingWindowOperation = "set-window-mouse-grab";
    m_fake.windowFailuresRemaining = 1;
    expectError(window.SetMouseGrab(true), pond::platform::PlatformErrorCode::BackendFailure,
                "SDL_SetWindowMouseGrab");
    EXPECT_FALSE(window.IsMouseGrabbed());

    m_fake.failingWindowOperation.clear();
    m_fake.applyWindowOperationEffects = false;
    EXPECT_TRUE(window.SetMouseGrab(true).HasValue());
    EXPECT_FALSE(window.IsMouseGrabbed());

    m_fake.applyWindowOperationEffects = true;
    m_fake.failingWindowOperation = "set-window-relative-mouse-mode";
    m_fake.windowFailuresRemaining = 1;
    expectError(window.SetRelativeMouseMode(true),
                pond::platform::PlatformErrorCode::BackendFailure,
                "SDL_SetWindowRelativeMouseMode");
    EXPECT_FALSE(window.IsRelativeMouseModeEnabled());

    m_fake.failingWindowOperation.clear();
    m_fake.applyWindowOperationEffects = false;
    expectError(window.SetRelativeMouseMode(true), pond::platform::PlatformErrorCode::Unsupported,
                "SDL_SetWindowRelativeMouseMode");
    EXPECT_FALSE(window.IsRelativeMouseModeEnabled());
}

TEST_F(PlatformRuntimeBackendTests, HandlesGlobalMouseCapabilityCaptureAndPosition)
{
    m_fake.globalMouseSupported = false;
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    const pond::core::VoidResult unsupportedCapture = runtime.SetMouseCapture(true);
    ASSERT_FALSE(unsupportedCapture.HasValue());
    EXPECT_EQ(unsupportedCapture.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));
    EXPECT_FALSE(m_fake.mouseCaptureEnabled);
    EXPECT_EQ(std::ranges::count(m_fake.calls, "set-mouse-capture"), 0);

    EXPECT_TRUE(runtime.SetMouseCapture(false).HasValue());
    EXPECT_EQ(std::ranges::count(m_fake.calls, "set-mouse-capture"), 0);

    const auto unsupportedPosition = runtime.GetGlobalMousePosition();
    ASSERT_FALSE(unsupportedPosition.HasValue());
    EXPECT_EQ(unsupportedPosition.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));
    EXPECT_EQ(std::ranges::count(m_fake.calls, "get-global-mouse-position"), 0);

    m_fake.globalMouseSupported = true;
    m_fake.globalMousePosition = {-12.5F, 300.25F};
    const auto position = runtime.GetGlobalMousePosition();
    ASSERT_TRUE(position.HasValue());
    EXPECT_EQ(position.GetValue(), m_fake.globalMousePosition);

    ASSERT_TRUE(runtime.SetMouseCapture(true).HasValue());
    EXPECT_TRUE(m_fake.mouseCaptureEnabled);
    ASSERT_TRUE(runtime.SetMouseCapture(false).HasValue());
    EXPECT_FALSE(m_fake.mouseCaptureEnabled);

    m_fake.failingMouseOperation = "set-mouse-capture";
    m_fake.mouseFailuresRemaining = 1;
    const pond::core::VoidResult failedCapture = runtime.SetMouseCapture(true);
    ASSERT_FALSE(failedCapture.HasValue());
    EXPECT_EQ(failedCapture.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(failedCapture.GetError().GetMessage().find("SDL_CaptureMouse"),
              std::string_view::npos);
    EXPECT_FALSE(m_fake.mouseCaptureEnabled);
}

TEST_F(PlatformRuntimeBackendTests, RejectsNonFiniteGlobalMousePositions)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    const std::array<pond::platform::LogicalPoint, 2> invalidPositions{{
        {std::numeric_limits<float>::quiet_NaN(), 1.0F},
        {1.0F, std::numeric_limits<float>::infinity()},
    }};
    for (const pond::platform::LogicalPoint position : invalidPositions)
    {
        m_fake.globalMousePosition = position;
        const auto result = runtime.GetGlobalMousePosition();
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
        EXPECT_NE(result.GetError().GetMessage().find("SDL_GetGlobalMouseState"),
                  std::string_view::npos);
    }
}

TEST_F(PlatformRuntimeBackendTests, LazilyCachesEverySystemCursorAndDestroysThemBeforeQuit)
{
    {
        auto runtimeResult =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(runtimeResult.HasValue());
        pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

        for (const pond::platform::SystemCursorShape shape : kSystemCursorShapes)
        {
            ASSERT_TRUE(runtime.SetSystemCursor(shape).HasValue());
        }
        EXPECT_TRUE(std::ranges::equal(m_fake.createdCursorShapes, kSystemCursorShapes));
        EXPECT_TRUE(std::ranges::equal(m_fake.selectedCursorShapes, kSystemCursorShapes));
        EXPECT_EQ(m_fake.cursors.size(), kSystemCursorShapes.size());

        for (const pond::platform::SystemCursorShape shape : kSystemCursorShapes)
        {
            ASSERT_TRUE(runtime.SetSystemCursor(shape).HasValue());
        }
        EXPECT_EQ(m_fake.createdCursorShapes.size(), kSystemCursorShapes.size());
        EXPECT_EQ(m_fake.selectedCursorShapes.size(), kSystemCursorShapes.size() * 2);

        const std::size_t createCalls = m_fake.createdCursorShapes.size();
        const std::size_t setCalls = m_fake.selectedCursorShapes.size();
        const auto invalidShape = static_cast<pond::platform::SystemCursorShape>(255);
        const pond::core::VoidResult invalidResult = runtime.SetSystemCursor(invalidShape);
        ASSERT_FALSE(invalidResult.HasValue());
        EXPECT_EQ(invalidResult.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));
        EXPECT_EQ(m_fake.createdCursorShapes.size(), createCalls);
        EXPECT_EQ(m_fake.selectedCursorShapes.size(), setCalls);
    }

    EXPECT_FALSE(m_fake.cursorsPresentAtQuit);
    EXPECT_TRUE(m_fake.cursors.empty());
    EXPECT_FALSE(m_fake.activeCursor.IsValid());
    EXPECT_TRUE(std::ranges::equal(m_fake.destroyedCursorShapes, kSystemCursorShapes));
    EXPECT_EQ(std::ranges::count(m_fake.calls, "destroy-cursor"),
              static_cast<std::ptrdiff_t>(kSystemCursorShapes.size()));

    const auto quit = std::ranges::find(m_fake.calls, "quit");
    ASSERT_NE(quit, m_fake.calls.end());
    EXPECT_EQ(std::find(quit, m_fake.calls.end(), "destroy-cursor"), m_fake.calls.end());
}

TEST_F(PlatformRuntimeBackendTests, RetriesCursorCreationAndReusesCursorAfterSelectionFailure)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    m_fake.failingMouseOperation = "create-system-cursor";
    m_fake.mouseFailuresRemaining = 1;
    const pond::core::VoidResult createFailure =
        runtime.SetSystemCursor(pond::platform::SystemCursorShape::TextInput);
    ASSERT_FALSE(createFailure.HasValue());
    EXPECT_NE(createFailure.GetError().GetMessage().find("SDL_CreateSystemCursor"),
              std::string_view::npos);
    EXPECT_TRUE(m_fake.cursors.empty());

    ASSERT_TRUE(runtime.SetSystemCursor(pond::platform::SystemCursorShape::TextInput).HasValue());
    EXPECT_EQ(std::ranges::count(m_fake.createdCursorShapes,
                                 pond::platform::SystemCursorShape::TextInput),
              2);

    m_fake.failingMouseOperation = "set-cursor";
    m_fake.mouseFailuresRemaining = 1;
    const pond::core::VoidResult setFailure =
        runtime.SetSystemCursor(pond::platform::SystemCursorShape::Pointer);
    ASSERT_FALSE(setFailure.HasValue());
    EXPECT_NE(setFailure.GetError().GetMessage().find("SDL_SetCursor"), std::string_view::npos);
    EXPECT_EQ(
        std::ranges::count(m_fake.createdCursorShapes, pond::platform::SystemCursorShape::Pointer),
        1);

    ASSERT_TRUE(runtime.SetSystemCursor(pond::platform::SystemCursorShape::Pointer).HasValue());
    EXPECT_EQ(
        std::ranges::count(m_fake.createdCursorShapes, pond::platform::SystemCursorShape::Pointer),
        1);
    EXPECT_EQ(
        std::ranges::count(m_fake.selectedCursorShapes, pond::platform::SystemCursorShape::Pointer),
        2);
}

TEST_F(PlatformRuntimeBackendTests, MakesCursorVisibilityIdempotentAndPreservesStateOnFailure)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    EXPECT_TRUE(runtime.IsCursorVisible());
    ASSERT_TRUE(runtime.ShowCursor().HasValue());
    EXPECT_EQ(std::ranges::count(m_fake.calls, "show-cursor"), 0);

    ASSERT_TRUE(runtime.HideCursor().HasValue());
    EXPECT_FALSE(runtime.IsCursorVisible());
    ASSERT_TRUE(runtime.HideCursor().HasValue());
    EXPECT_EQ(std::ranges::count(m_fake.calls, "hide-cursor"), 1);

    ASSERT_TRUE(runtime.ShowCursor().HasValue());
    EXPECT_TRUE(runtime.IsCursorVisible());
    EXPECT_EQ(std::ranges::count(m_fake.calls, "show-cursor"), 1);

    m_fake.failingMouseOperation = "hide-cursor";
    m_fake.mouseFailuresRemaining = 1;
    const pond::core::VoidResult hideFailure = runtime.HideCursor();
    ASSERT_FALSE(hideFailure.HasValue());
    EXPECT_NE(hideFailure.GetError().GetMessage().find("SDL_HideCursor"), std::string_view::npos);
    EXPECT_TRUE(runtime.IsCursorVisible());

    m_fake.cursorVisible = false;
    m_fake.failingMouseOperation = "show-cursor";
    m_fake.mouseFailuresRemaining = 1;
    const pond::core::VoidResult showFailure = runtime.ShowCursor();
    ASSERT_FALSE(showFailure.HasValue());
    EXPECT_NE(showFailure.GetError().GetMessage().find("SDL_ShowCursor"), std::string_view::npos);
    EXPECT_FALSE(runtime.IsCursorVisible());
}

TEST_F(PlatformRuntimeBackendTests, LaunchesAsynchronousDialogsWithOwnedDescriptorsAndStableIds)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto windowResult = runtime.CreateWindow(pond::platform::WindowDesc{});
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    pond::platform::OpenFileDialogDesc openDesc{
        .parentWindowId = window.GetId(),
        .defaultLocation = std::filesystem::path{"C:/tmp/input.sdf"},
        .filters = {{.name = "Molecules", .pattern = "SDF-2_x.foo;mol"},
                    {.name = "All", .pattern = "*"}},
        .allowMultipleSelection = true};
    auto openId = runtime.ShowOpenFileDialog(openDesc);
    ASSERT_TRUE(openId.HasValue()) << openId.GetError().GetMessage();
    EXPECT_EQ(openId.GetValue(), pond::platform::DialogRequestId{1});

    openDesc.filters.front().name = "mutated";
    openDesc.filters.front().pattern = "xyz";
    openDesc.defaultLocation = std::filesystem::path{"C:/tmp/mutated"};

    pond::platform::SaveFileDialogDesc saveDesc{
        .defaultLocation = std::filesystem::path{"C:/tmp/output.sdf"},
        .filters = {{.name = "Molecules", .pattern = "sdf"}}};
    auto saveId = runtime.ShowSaveFileDialog(saveDesc);
    ASSERT_TRUE(saveId.HasValue()) << saveId.GetError().GetMessage();
    EXPECT_EQ(saveId.GetValue(), pond::platform::DialogRequestId{2});

    pond::platform::OpenFolderDialogDesc folderDesc{
        .defaultLocation = std::filesystem::path{"C:/tmp"}, .allowMultipleSelection = true};
    auto folderId = runtime.ShowOpenFolderDialog(folderDesc);
    ASSERT_TRUE(folderId.HasValue()) << folderId.GetError().GetMessage();
    EXPECT_EQ(folderId.GetValue(), pond::platform::DialogRequestId{3});

    ASSERT_EQ(m_fake.dialogLaunches.size(), 3U);
    EXPECT_EQ(m_fake.dialogLaunches[0].kind, BackendDialogKind::OpenFile);
    ASSERT_TRUE(m_fake.dialogLaunches[0].parentWindow.has_value());
    EXPECT_EQ(m_fake.dialogLaunches[0].parentWindow->GetValue(),
              reinterpret_cast<BackendWindowHandle::ValueType>(
                  std::addressof(m_fake.windows.front())));
    ASSERT_EQ(m_fake.dialogLaunches[0].filters.size(), 2U);
    EXPECT_EQ(m_fake.dialogLaunches[0].filters[0].name, "Molecules");
    EXPECT_EQ(m_fake.dialogLaunches[0].filters[0].pattern, "SDF-2_x.foo;mol");
    EXPECT_EQ(m_fake.dialogLaunches[0].defaultLocation, "C:/tmp/input.sdf");
    EXPECT_TRUE(m_fake.dialogLaunches[0].allowMultipleSelection);

    EXPECT_EQ(m_fake.dialogLaunches[1].kind, BackendDialogKind::SaveFile);
    EXPECT_FALSE(m_fake.dialogLaunches[1].parentWindow.has_value());
    ASSERT_EQ(m_fake.dialogLaunches[1].filters.size(), 1U);
    EXPECT_FALSE(m_fake.dialogLaunches[1].allowMultipleSelection);

    EXPECT_EQ(m_fake.dialogLaunches[2].kind, BackendDialogKind::OpenFolder);
    EXPECT_TRUE(m_fake.dialogLaunches[2].filters.empty());
    EXPECT_EQ(m_fake.dialogLaunches[2].defaultLocation, "C:/tmp");
    EXPECT_TRUE(m_fake.dialogLaunches[2].allowMultipleSelection);

    CompleteDialogCancellation(m_fake.dialogLaunches[0]);
    CompleteDialogCancellation(m_fake.dialogLaunches[1]);
    CompleteDialogCancellation(m_fake.dialogLaunches[2]);
    EXPECT_TRUE(runtime.PollEvent().has_value());
    EXPECT_TRUE(runtime.PollEvent().has_value());
    EXPECT_TRUE(runtime.PollEvent().has_value());
}

#if GTEST_HAS_DEATH_TEST
TEST_F(PlatformRuntimeBackendTests, RejectsDestroyingParentWindowWithPendingDialog)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto windowResult = runtime.CreateWindow(pond::platform::WindowDesc{});
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    pond::platform::OpenFileDialogDesc desc{.parentWindowId = window.GetId()};
    auto request = runtime.ShowOpenFileDialog(desc);
    ASSERT_TRUE(request.HasValue()) << request.GetError().GetMessage();
    ASSERT_EQ(m_fake.dialogLaunches.size(), 1U);

    EXPECT_DEATH(
        {
            std::optional<pond::platform::Window> doomedWindow;
            doomedWindow.emplace(std::move(window));
            doomedWindow.reset();
        },
        "Cannot destroy a platform window");

    CompleteDialogCancellation(m_fake.dialogLaunches.front());
    EXPECT_TRUE(runtime.PollEvent().has_value());
}

TEST_F(PlatformRuntimeBackendTests, RejectsDestroyingRuntimeWithUnconsumedDialogCompletion)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto request = runtime.ShowOpenFolderDialog(pond::platform::OpenFolderDialogDesc{});
    ASSERT_TRUE(request.HasValue()) << request.GetError().GetMessage();
    ASSERT_EQ(m_fake.dialogLaunches.size(), 1U);
    CompleteDialogCancellation(m_fake.dialogLaunches.front());

    EXPECT_DEATH(
        {
            std::optional<pond::platform::PlatformRuntime> doomedRuntime;
            doomedRuntime.emplace(std::move(runtime));
            doomedRuntime.reset();
        },
        "Cannot destroy PlatformRuntime");

    EXPECT_TRUE(runtime.PollEvent().has_value());
}
#endif

TEST_F(PlatformRuntimeBackendTests, RejectsInvalidDialogDescriptorsAndParentsBeforeBackendLaunch)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::OpenFileDialogDesc invalidParent;
    invalidParent.parentWindowId = pond::platform::WindowId::Invalid();
    auto invalidParentResult = runtime.ShowOpenFileDialog(invalidParent);
    ASSERT_FALSE(invalidParentResult.HasValue());
    EXPECT_EQ(invalidParentResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));

    pond::platform::OpenFileDialogDesc missingParent;
    missingParent.parentWindowId = pond::platform::WindowId{999};
    auto missingParentResult = runtime.ShowOpenFileDialog(missingParent);
    ASSERT_FALSE(missingParentResult.HasValue());
    EXPECT_EQ(missingParentResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::NotFound));

    pond::platform::OpenFileDialogDesc emptyDefault;
    emptyDefault.defaultLocation = std::filesystem::path{};
    auto emptyDefaultResult = runtime.ShowOpenFileDialog(emptyDefault);
    ASSERT_FALSE(emptyDefaultResult.HasValue());
    EXPECT_EQ(emptyDefaultResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));

    pond::platform::OpenFileDialogDesc invalidFilterName;
    invalidFilterName.filters.push_back(
        pond::platform::DialogFileFilter{.name = "", .pattern = "sdf"});
    auto invalidFilterNameResult = runtime.ShowOpenFileDialog(invalidFilterName);
    ASSERT_FALSE(invalidFilterNameResult.HasValue());
    EXPECT_EQ(invalidFilterNameResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));

    pond::platform::OpenFileDialogDesc invalidFilterPattern;
    invalidFilterPattern.filters.push_back(
        pond::platform::DialogFileFilter{.name = "Molecules", .pattern = "s df"});
    auto invalidFilterPatternResult = runtime.ShowOpenFileDialog(invalidFilterPattern);
    ASSERT_FALSE(invalidFilterPatternResult.HasValue());
    EXPECT_EQ(invalidFilterPatternResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));

    pond::platform::OpenFileDialogDesc nonAsciiFilterPattern;
    nonAsciiFilterPattern.filters.push_back(
        pond::platform::DialogFileFilter{.name = "Molecules", .pattern = "sdf;\xC3\xA9"});
    auto nonAsciiFilterPatternResult = runtime.ShowOpenFileDialog(nonAsciiFilterPattern);
    ASSERT_FALSE(nonAsciiFilterPatternResult.HasValue());
    EXPECT_EQ(nonAsciiFilterPatternResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));

    const std::string invalidUtf8{static_cast<char>(0xC0), static_cast<char>(0xAF)};
    pond::platform::OpenFileDialogDesc invalidUtf8Filter;
    invalidUtf8Filter.filters.push_back(
        pond::platform::DialogFileFilter{.name = invalidUtf8, .pattern = "sdf"});
    auto invalidUtf8Result = runtime.ShowOpenFileDialog(invalidUtf8Filter);
    ASSERT_FALSE(invalidUtf8Result.HasValue());
    EXPECT_EQ(invalidUtf8Result.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));

    EXPECT_TRUE(m_fake.dialogLaunches.empty());
    EXPECT_EQ(std::ranges::count(m_fake.calls, "show-dialog"), 0);
}

TEST_F(PlatformRuntimeBackendTests, DeliversDialogCompletionsInCallbackFifoOrder)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto firstId = runtime.ShowOpenFileDialog(pond::platform::OpenFileDialogDesc{
        .filters = {{.name = "Molecules", .pattern = "sdf;mol"}}});
    auto secondId = runtime.ShowSaveFileDialog(pond::platform::SaveFileDialogDesc{});
    auto thirdId = runtime.ShowOpenFolderDialog(pond::platform::OpenFolderDialogDesc{});
    ASSERT_TRUE(firstId.HasValue());
    ASSERT_TRUE(secondId.HasValue());
    ASSERT_TRUE(thirdId.HasValue());
    ASSERT_EQ(m_fake.dialogLaunches.size(), 3U);

    m_fake.ticks = 10'000;
    CompleteDialogCancellation(m_fake.dialogLaunches[1]);
    const std::vector<std::string> selectedPaths{"C:/tmp/a.sdf", "C:/tmp/b.mol"};
    m_fake.ticks = 20'000;
    CompleteDialogSelection(m_fake.dialogLaunches[0], selectedPaths, 0);
    m_fake.ticks = 30'000;
    CompleteDialogFailure(m_fake.dialogLaunches[2], "synthetic dialog failure");
    m_fake.ticks = 40'000;

    std::optional<pond::platform::PlatformEvent> event = runtime.PollEvent();
    ASSERT_TRUE(event.has_value());
    const auto* completed = std::get_if<pond::platform::DialogCompletedEvent>(&*event);
    ASSERT_NE(completed, nullptr);
    EXPECT_EQ(completed->timestamp.GetTimeSinceEpoch(), std::chrono::nanoseconds{10'000});
    EXPECT_EQ(completed->requestId, secondId.GetValue());
    EXPECT_TRUE(std::holds_alternative<pond::platform::DialogCancellation>(completed->outcome));

    event = runtime.PollEvent();
    ASSERT_TRUE(event.has_value());
    completed = std::get_if<pond::platform::DialogCompletedEvent>(&*event);
    ASSERT_NE(completed, nullptr);
    EXPECT_EQ(completed->timestamp.GetTimeSinceEpoch(), std::chrono::nanoseconds{20'000});
    EXPECT_EQ(completed->requestId, firstId.GetValue());
    const auto& selection = std::get<pond::platform::DialogSelection>(completed->outcome);
    ASSERT_EQ(selection.paths.size(), 2U);
    EXPECT_EQ(selection.paths[0], std::filesystem::path{"C:/tmp/a.sdf"});
    EXPECT_EQ(selection.paths[1], std::filesystem::path{"C:/tmp/b.mol"});
    ASSERT_TRUE(selection.selectedFilterIndex.has_value());
    EXPECT_EQ(*selection.selectedFilterIndex, 0U);

    event = runtime.PollEvent();
    ASSERT_TRUE(event.has_value());
    completed = std::get_if<pond::platform::DialogCompletedEvent>(&*event);
    ASSERT_NE(completed, nullptr);
    EXPECT_EQ(completed->timestamp.GetTimeSinceEpoch(), std::chrono::nanoseconds{30'000});
    EXPECT_EQ(completed->requestId, thirdId.GetValue());
    const auto& failure = std::get<pond::platform::DialogFailure>(completed->outcome);
    EXPECT_EQ(failure.error.GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(failure.error.GetMessage().find("synthetic dialog failure"), std::string_view::npos);

    EXPECT_FALSE(runtime.PollEvent().has_value());
    EXPECT_EQ(m_fake.tickCalls, 3);
}

TEST_F(PlatformRuntimeBackendTests, CapturesDialogFailureBeforeSamplingCompletionTimestamp)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto requestId = runtime.ShowOpenFileDialog(pond::platform::OpenFileDialogDesc{});
    ASSERT_TRUE(requestId.HasValue());
    ASSERT_EQ(m_fake.dialogLaunches.size(), 1U);

    m_fake.ticks = 41'000;
    m_fake.timestampCaptureOverwritesSdlError = true;
    CompleteDialogFailure(m_fake.dialogLaunches.front(), "original dialog failure");

    const std::optional<pond::platform::PlatformEvent> event = runtime.PollEvent();
    ASSERT_TRUE(event.has_value());
    const auto* completed = std::get_if<pond::platform::DialogCompletedEvent>(&*event);
    ASSERT_NE(completed, nullptr);
    EXPECT_EQ(completed->timestamp.GetTimeSinceEpoch(), std::chrono::nanoseconds{41'000});
    EXPECT_EQ(completed->requestId, requestId.GetValue());
    const auto* failure = std::get_if<pond::platform::DialogFailure>(&completed->outcome);
    ASSERT_NE(failure, nullptr);
    EXPECT_NE(failure->error.GetMessage().find("original dialog failure"), std::string_view::npos);
    EXPECT_EQ(failure->error.GetMessage().find("timestamp capture overwrote"),
              std::string_view::npos);
}

TEST_F(PlatformRuntimeBackendTests, CopiesDialogCallbackPayloadsAndAcceptsCrossThreadCompletion)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto requestId = runtime.ShowOpenFileDialog(pond::platform::OpenFileDialogDesc{});
    ASSERT_TRUE(requestId.HasValue());
    ASSERT_EQ(m_fake.dialogLaunches.size(), 1U);

    std::vector<std::string> paths{"C:/tmp/threaded.sdf"};
    std::thread worker{[this, &paths]()
                       {
                           CompleteDialogSelection(m_fake.dialogLaunches.front(), paths, -1);
                           paths.front() = "C:/tmp/mutated-after-callback.sdf";
                       }};
    worker.join();

    std::optional<pond::platform::PlatformEvent> event = runtime.PollEvent();
    ASSERT_TRUE(event.has_value());
    const auto* completed = std::get_if<pond::platform::DialogCompletedEvent>(&*event);
    ASSERT_NE(completed, nullptr);
    EXPECT_EQ(completed->requestId, requestId.GetValue());
    const auto& selection = std::get<pond::platform::DialogSelection>(completed->outcome);
    ASSERT_EQ(selection.paths.size(), 1U);
    EXPECT_EQ(selection.paths.front(), std::filesystem::path{"C:/tmp/threaded.sdf"});
    EXPECT_FALSE(selection.selectedFilterIndex.has_value());
}

TEST_F(PlatformRuntimeBackendTests, DeliversDialogCompletionExactlyOnceForDuplicateCallbacks)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto requestId = runtime.ShowOpenFileDialog(pond::platform::OpenFileDialogDesc{});
    ASSERT_TRUE(requestId.HasValue());
    ASSERT_EQ(m_fake.dialogLaunches.size(), 1U);

    CompleteDialogCancellation(m_fake.dialogLaunches.front());
    const std::vector<std::string> latePaths{"C:/tmp/late.sdf"};
    CompleteDialogSelection(m_fake.dialogLaunches.front(), latePaths, -1);

    std::optional<pond::platform::PlatformEvent> event = runtime.PollEvent();
    ASSERT_TRUE(event.has_value());
    const auto* completed = std::get_if<pond::platform::DialogCompletedEvent>(&*event);
    ASSERT_NE(completed, nullptr);
    EXPECT_EQ(completed->requestId, requestId.GetValue());
    EXPECT_TRUE(std::holds_alternative<pond::platform::DialogCancellation>(completed->outcome));
    EXPECT_FALSE(runtime.PollEvent().has_value());
}

TEST_F(PlatformRuntimeBackendTests, ConvertsDialogCallbackExceptionsToPrebuiltFailureCompletion)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto requestId = runtime.ShowOpenFileDialog(pond::platform::OpenFileDialogDesc{});
    ASSERT_TRUE(requestId.HasValue());
    ASSERT_EQ(m_fake.dialogLaunches.size(), 1U);

    m_fake.ticks = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1U;
    CompleteDialogCancellation(m_fake.dialogLaunches.front());

    std::optional<pond::platform::PlatformEvent> event = runtime.PollEvent();
    ASSERT_TRUE(event.has_value());
    const auto* completed = std::get_if<pond::platform::DialogCompletedEvent>(&*event);
    ASSERT_NE(completed, nullptr);
    EXPECT_EQ(completed->timestamp.GetTimeSinceEpoch(), std::chrono::nanoseconds{0});
    EXPECT_EQ(completed->requestId, requestId.GetValue());
    const auto* failure = std::get_if<pond::platform::DialogFailure>(&completed->outcome);
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->error.GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(failure->error.GetMessage().find(
                  "Dialog callback failed before a completion could be safely enqueued"),
              std::string_view::npos);
    EXPECT_FALSE(runtime.PollEvent().has_value());
}

TEST_F(PlatformRuntimeBackendTests, GetsClipboardTextAndDistinguishesEmptyTextFromFailure)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    m_fake.clipboardText = "C6H6";
    auto text = runtime.GetClipboardText();
    ASSERT_TRUE(text.HasValue());
    EXPECT_EQ(text.GetValue(), "C6H6");

    m_fake.clipboardText.clear();
    m_fake.clipboardGetError.clear();
    auto emptyText = runtime.GetClipboardText();
    ASSERT_TRUE(emptyText.HasValue());
    EXPECT_TRUE(emptyText.GetValue().empty());

    m_fake.clipboardGetError = "synthetic empty clipboard failure";
    auto emptyFailure = runtime.GetClipboardText();
    ASSERT_FALSE(emptyFailure.HasValue());
    EXPECT_EQ(emptyFailure.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(emptyFailure.GetError().GetMessage().find("SDL_GetClipboardText"),
              std::string_view::npos);
    EXPECT_NE(emptyFailure.GetError().GetMessage().find("synthetic empty clipboard failure"),
              std::string_view::npos);

    m_fake.clipboardGetReturnsNull = true;
    m_fake.clipboardGetError = "synthetic null clipboard failure";
    auto nullFailure = runtime.GetClipboardText();
    ASSERT_FALSE(nullFailure.HasValue());
    EXPECT_EQ(nullFailure.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(nullFailure.GetError().GetMessage().find("synthetic null clipboard failure"),
              std::string_view::npos);
}

TEST_F(PlatformRuntimeBackendTests, HandlesClipboardCapabilityValidationAndSetFailures)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    ASSERT_TRUE(runtime.SetClipboardText("H2O").HasValue());
    EXPECT_EQ(m_fake.clipboardText, "H2O");
    ASSERT_TRUE(runtime.SetClipboardText("").HasValue());
    EXPECT_TRUE(m_fake.clipboardText.empty());

    const auto setCallsBeforeValidation = std::ranges::count(m_fake.calls, "set-clipboard-text");
    const std::string embeddedNullText{"Na\0Cl", 5};
    const pond::core::VoidResult embeddedNull = runtime.SetClipboardText(embeddedNullText);
    ASSERT_FALSE(embeddedNull.HasValue());
    EXPECT_EQ(embeddedNull.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));

    const std::string invalidUtf8{static_cast<char>(0xC0), static_cast<char>(0xAF)};
    const pond::core::VoidResult invalidUtf8Result = runtime.SetClipboardText(invalidUtf8);
    ASSERT_FALSE(invalidUtf8Result.HasValue());
    EXPECT_EQ(invalidUtf8Result.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));
    EXPECT_EQ(std::ranges::count(m_fake.calls, "set-clipboard-text"), setCallsBeforeValidation);

    m_fake.clipboardTextSupported = false;
    const pond::core::VoidResult unsupported = runtime.SetClipboardText("unsupported");
    ASSERT_FALSE(unsupported.HasValue());
    EXPECT_EQ(unsupported.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));
    EXPECT_EQ(std::ranges::count(m_fake.calls, "set-clipboard-text"), setCallsBeforeValidation);

    const auto unsupportedRead = runtime.GetClipboardText();
    ASSERT_FALSE(unsupportedRead.HasValue());
    EXPECT_EQ(unsupportedRead.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));
    EXPECT_EQ(std::ranges::count(m_fake.calls, "get-clipboard-text"), 0);

    m_fake.clipboardTextSupported = true;
    m_fake.failingServiceOperation = "set-clipboard-text";
    m_fake.serviceFailuresRemaining = 1;
    const pond::core::VoidResult backendFailure = runtime.SetClipboardText("backend failure");
    ASSERT_FALSE(backendFailure.HasValue());
    EXPECT_EQ(backendFailure.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(backendFailure.GetError().GetMessage().find("SDL_SetClipboardText"),
              std::string_view::npos);
    EXPECT_TRUE(m_fake.clipboardText.empty());
}

TEST_F(PlatformRuntimeBackendTests, OpensExternalUrisThroughTheDeterministicBackendSeam)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    const pond::core::VoidResult empty = runtime.OpenExternalUri("");
    ASSERT_FALSE(empty.HasValue());
    EXPECT_EQ(empty.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));

    const std::string embeddedNullUri{"https://example.invalid/\0tail", 29};
    const pond::core::VoidResult embeddedNull = runtime.OpenExternalUri(embeddedNullUri);
    ASSERT_FALSE(embeddedNull.HasValue());
    EXPECT_EQ(embeddedNull.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));

    const std::string invalidUtf8{static_cast<char>(0xC0), static_cast<char>(0xAF)};
    const pond::core::VoidResult invalidUtf8Result = runtime.OpenExternalUri(invalidUtf8);
    ASSERT_FALSE(invalidUtf8Result.HasValue());
    EXPECT_EQ(invalidUtf8Result.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));
    EXPECT_TRUE(m_fake.openedExternalUris.empty());

    ASSERT_TRUE(runtime.OpenExternalUri("https://example.invalid/ponder?molecule=H2O").HasValue());
    ASSERT_EQ(m_fake.openedExternalUris.size(), 1U);
    EXPECT_EQ(m_fake.openedExternalUris.back(), "https://example.invalid/ponder?molecule=H2O");

    m_fake.failingServiceOperation = "open-external-uri";
    m_fake.serviceFailuresRemaining = 1;
    const pond::core::VoidResult backendFailure =
        runtime.OpenExternalUri("https://example.invalid/failure");
    ASSERT_FALSE(backendFailure.HasValue());
    EXPECT_EQ(backendFailure.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(backendFailure.GetError().GetMessage().find("SDL_OpenURL"), std::string_view::npos);
    EXPECT_EQ(m_fake.openedExternalUris.size(), 1U);
}

TEST_F(PlatformRuntimeBackendTests, GuardsClipboardAndExternalUriApisAfterMoveAndAcrossThreads)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    pond::platform::PlatformRuntime movedRuntime = std::move(runtime);

    const auto callsBefore = m_fake.calls;
    EXPECT_EQ(CountRejectedRuntimeServiceCalls(runtime), 6);
    EXPECT_EQ(m_fake.calls, callsBefore);

    std::atomic_int rejectedCalls{};
    std::thread worker{[&movedRuntime, &rejectedCalls]()
                       {
                           rejectedCalls.store(CountRejectedRuntimeServiceCalls(movedRuntime));
                       }};
    worker.join();
    EXPECT_EQ(rejectedCalls.load(), 6);
    EXPECT_EQ(m_fake.calls, callsBefore);

    ASSERT_TRUE(movedRuntime.SetClipboardText("owner thread").HasValue());
    auto text = movedRuntime.GetClipboardText();
    ASSERT_TRUE(text.HasValue());
    EXPECT_EQ(text.GetValue(), "owner thread");
    ASSERT_TRUE(movedRuntime.OpenExternalUri("https://example.invalid/owner-thread").HasValue());
    ASSERT_EQ(m_fake.openedExternalUris.size(), 1U);
}

TEST_F(PlatformRuntimeBackendTests, GuardsEveryMouseApiAfterMoveAndAcrossThreads)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    {
        auto windowResult = runtime.CreateWindow(pond::platform::WindowDesc{});
        ASSERT_TRUE(windowResult.HasValue());
        pond::platform::Window window = std::move(windowResult).GetValue();
        pond::platform::Window movedWindow = std::move(window);

        const auto callsBefore = m_fake.calls;
        EXPECT_EQ(CountRejectedWindowMouseCalls(window), 4);
        EXPECT_EQ(m_fake.calls, callsBefore);

        std::atomic_int rejectedCalls{};
        std::thread worker{[&movedWindow, &rejectedCalls]()
                           {
                               rejectedCalls.store(CountRejectedWindowMouseCalls(movedWindow));
                           }};
        worker.join();
        EXPECT_EQ(rejectedCalls.load(), 4);
        EXPECT_EQ(m_fake.calls, callsBefore);
    }

    pond::platform::PlatformRuntime movedRuntime = std::move(runtime);
    const auto callsBefore = m_fake.calls;
    EXPECT_EQ(CountRejectedRuntimeMouseCalls(runtime), 6);
    EXPECT_EQ(m_fake.calls, callsBefore);

    std::atomic_int rejectedCalls{};
    std::thread worker{[&movedRuntime, &rejectedCalls]()
                       {
                           rejectedCalls.store(CountRejectedRuntimeMouseCalls(movedRuntime));
                       }};
    worker.join();
    EXPECT_EQ(rejectedCalls.load(), 6);
    EXPECT_EQ(m_fake.calls, callsBefore);
}

TEST_F(PlatformRuntimeBackendTests, GuardsPollEventAfterMoveAndAcrossThreads)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    pond::platform::PlatformRuntime movedRuntime = std::move(runtime);
    m_fake.eventQueue.push_back({.event = MakeQueuedQuitEvent(100)});

    EXPECT_THROW(static_cast<void>(runtime.PollEvent()), pond::core::PonderException);
    EXPECT_EQ(m_fake.pollEventCalls, 0);

    std::atomic_bool rejected{false};
    std::thread worker{[&movedRuntime, &rejected]()
                       {
                           try
                           {
                               static_cast<void>(movedRuntime.PollEvent());
                           }
                           catch (const pond::core::PonderException&)
                           {
                               rejected.store(true);
                           }
                       }};
    worker.join();
    EXPECT_TRUE(rejected.load());
    EXPECT_EQ(m_fake.pollEventCalls, 0);

    ExpectPolledEvent(movedRuntime.PollEvent(), pond::platform::QuitRequestedEvent{
                                                    .timestamp = pond::platform::Timestamp{
                                                        std::chrono::nanoseconds{100}}});
    EXPECT_EQ(m_fake.pollEventCalls, 1);
}
} // namespace
