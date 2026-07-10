#include <ponder/core/PonderException.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "PlatformRuntimeBackend.hpp"

namespace
{
using pond::platform::detail::ApplicationMetadataProperty;
using pond::platform::detail::BackendClipboardTextResult;
using pond::platform::detail::BackendDialogKind;
using pond::platform::detail::BackendDialogRequestDesc;
using pond::platform::detail::BackendDisplayOrientation;
using pond::platform::detail::BackendNativeWindowDriver;
using pond::platform::detail::BackendNativeWindowHandleResult;
using pond::platform::detail::BackendNativeWindowHandleStatus;
using pond::platform::detail::BackendScreenRectangle;
using pond::platform::detail::BackendTextInputArea;
using pond::platform::detail::BackendWindowCreateDesc;
using pond::platform::detail::BackendWindowOperationResult;
using pond::platform::detail::BackendWindowProperties;
using pond::platform::detail::PlatformDisplayBackend;
using pond::platform::detail::PlatformRuntimeBackend;
using pond::platform::detail::PlatformWindowBackend;

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
    void* parentWindow{};
    std::vector<pond::platform::DialogFileFilter> filters;
    std::optional<std::string> defaultLocation;
    bool allowMultipleSelection{};
    SDL_DialogFileCallback callback{};
    void* userdata{};
};

struct FakeRuntimeBackend final
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
    void* nativeCocoaMetalView{reinterpret_cast<void*>(std::uintptr_t{0x7000})};
    void* nativeCocoaMetalLayer{reinterpret_cast<void*>(std::uintptr_t{0x8000})};
    int createMetalViewCalls{};
    int destroyMetalViewCalls{};
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
    std::string clipboardReadBuffer;
    std::string clipboardGetError;
    bool clipboardGetReturnsNull{};
    int clipboardFreeCalls{};
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
    bool policiesObservedDuringInitialization{};
    bool metadataObservedDuringInitialization{};
    bool attemptCreateDuringRestoration{};
    bool attemptedCreateDuringRestoration{};
    std::optional<pond::core::ErrorCode> restorationCreateError;
    bool cursorsPresentAtQuit{};

    std::array<std::optional<std::string>, 3> metadata;
    std::unordered_map<std::string, std::string> hints;
    std::list<FakeWindow> windows;
    std::list<FakeCursor> cursors;
    void* activeCursor{};
    std::vector<pond::platform::SystemCursorShape> createdCursorShapes;
    std::vector<pond::platform::SystemCursorShape> selectedCursorShapes;
    std::vector<pond::platform::SystemCursorShape> destroyedCursorShapes;
    std::vector<void*> destroyedMetalViews;
    std::vector<std::string> openedExternalUris;
    std::vector<FakeDialogLaunch> dialogLaunches;
    std::vector<std::uint32_t> connectedDisplayIds;
    std::unordered_map<std::uint32_t, FakeDisplay> displays;
    std::deque<QueuedBackendEvent> eventQueue;
    std::vector<std::string> calls;
};

[[nodiscard]] FakeRuntimeBackend& GetFake(void* context)
{
    return *static_cast<FakeRuntimeBackend*>(context);
}

[[nodiscard]] FakeWindow& GetFakeWindow(void* window)
{
    return *static_cast<FakeWindow*>(window);
}

[[nodiscard]] FakeCursor& GetFakeCursor(void* cursor)
{
    return *static_cast<FakeCursor*>(cursor);
}

[[nodiscard]] FakeDisplay& GetFakeDisplay(FakeRuntimeBackend& fake, std::uint32_t backendDisplayId)
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

[[nodiscard]] bool FailWindowOperation(FakeRuntimeBackend& fake, std::string_view operation)
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

[[nodiscard]] BackendWindowOperationResult GetWindowOperationResult(FakeRuntimeBackend& fake,
                                                                    std::string_view operation)
{
    if (fake.unsupportedWindowOperation == operation &&
        fake.unsupportedWindowOperationsRemaining != 0)
    {
        --fake.unsupportedWindowOperationsRemaining;
        const std::string message = "synthetic " + std::string{operation} + " unsupported";
        static_cast<void>(SDL_SetError("%s", message.c_str()));
        return BackendWindowOperationResult::Unsupported;
    }
    if (FailWindowOperation(fake, operation))
    {
        return BackendWindowOperationResult::Failed;
    }
    return BackendWindowOperationResult::Succeeded;
}

[[nodiscard]] bool FailDisplayOperation(FakeRuntimeBackend& fake, std::string_view operation)
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

[[nodiscard]] bool FailMouseOperation(FakeRuntimeBackend& fake, std::string_view operation)
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

[[nodiscard]] bool FailServiceOperation(FakeRuntimeBackend& fake, std::string_view operation)
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

[[nodiscard]] const char* GetHintValue(FakeRuntimeBackend& fake, const char* name)
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

void MaybeAttemptCreateDuringRestoration(FakeRuntimeBackend& fake)
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
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.mainThreadChecks;
    fake.calls.emplace_back("is-main-thread");
    return fake.mainThread;
}

[[nodiscard]] bool FakeHasInitializedSubsystems(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.initializedSubsystemChecks;
    fake.calls.emplace_back("was-init");
    return fake.initializedSubsystems;
}

[[nodiscard]] bool FakeHasExpectedRuntimeSubsystems(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.runtimeSubsystemChecks;
    fake.calls.emplace_back("has-runtime-subsystems");
    return fake.initializedSubsystems && fake.onlyRuntimeSubsystems;
}

[[nodiscard]] const char* FakeGetAppMetadataProperty(void* context,
                                                     ApplicationMetadataProperty property)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-metadata");
    const std::optional<std::string>& value = fake.metadata[MetadataIndex(property)];
    return value.has_value() ? value->c_str() : nullptr;
}

[[nodiscard]] bool FakeSetAppMetadataProperty(void* context, ApplicationMetadataProperty property,
                                              const char* value)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.metadataCalls;
    fake.calls.emplace_back("set-metadata");

    if (fake.failingMetadataProperty == property && fake.metadataFailuresRemaining > 0)
    {
        --fake.metadataFailuresRemaining;
        static_cast<void>(SDL_SetError("synthetic metadata failure"));
        return false;
    }

    fake.metadata[MetadataIndex(property)] =
        value != nullptr ? std::optional<std::string>{value} : std::nullopt;
    return true;
}

[[nodiscard]] const char* FakeGetHint(void* context, const char* name)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back(std::string{"get-hint:"} + name);
    return GetHintValue(fake, name);
}

[[nodiscard]] bool FakeSetHintOverride(void* context, const char* name, const char* value)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back(std::string{"set-hint:"} + name + "=" + value);
    MaybeAttemptCreateDuringRestoration(fake);

    if (fake.failingHint == name && fake.hintFailuresRemaining > 0)
    {
        --fake.hintFailuresRemaining;
        static_cast<void>(SDL_SetError("synthetic hint failure"));
        return false;
    }

    fake.hints[name] = value;
    return true;
}

[[nodiscard]] bool FakeResetHint(void* context, const char* name)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back(std::string{"reset-hint:"} + name);
    MaybeAttemptCreateDuringRestoration(fake);
    return fake.hints.erase(name) == 1;
}

[[nodiscard]] bool FakeInitializeVideo(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.initializeVideoCalls;
    fake.calls.emplace_back("initialize-video");
    fake.policiesObservedDuringInitialization =
        GetHintValue(fake, pond::platform::detail::kMouseFocusClickThroughHint) ==
            std::string_view{"1"} &&
        GetHintValue(fake, pond::platform::detail::kMouseAutoCaptureHint) == std::string_view{"0"};
    fake.metadataObservedDuringInitialization =
        fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)].has_value();

    if (!fake.initializeVideoSucceeds)
    {
        static_cast<void>(SDL_SetError("synthetic video initialization failure"));
        return false;
    }

    fake.initializedSubsystems = true;
    return true;
}

void FakeQuit(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.quitCalls;
    fake.calls.emplace_back("quit");
    fake.cursorsPresentAtQuit = !fake.cursors.empty();
    fake.initializedSubsystems = false;
    fake.metadata = {};
    fake.hints.clear();
}

[[nodiscard]] std::uint64_t FakeGetTicksNanoseconds(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.tickCalls;
    fake.calls.emplace_back("ticks");
    return fake.ticks;
}

[[nodiscard]] bool FakePollEvent(void* context, SDL_Event* event)
{
    FakeRuntimeBackend& fake = GetFake(context);
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
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("supports-global-mouse");
    return fake.globalMouseSupported;
}

void FakeGetGlobalMousePosition(void* context, float* x, float* y)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-global-mouse-position");
    *x = fake.globalMousePosition.x;
    *y = fake.globalMousePosition.y;
}

[[nodiscard]] bool FakeSetMouseCapture(void* context, bool enabled)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-mouse-capture");
    if (FailMouseOperation(fake, "set-mouse-capture"))
    {
        return false;
    }

    fake.mouseCaptureEnabled = enabled;
    return true;
}

[[nodiscard]] void* FakeCreateSystemCursor(void* context, pond::platform::SystemCursorShape shape)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("create-system-cursor");
    fake.createdCursorShapes.push_back(shape);
    if (FailMouseOperation(fake, "create-system-cursor"))
    {
        return nullptr;
    }

    fake.cursors.emplace_back(FakeCursor{shape});
    return &fake.cursors.back();
}

[[nodiscard]] bool FakeSetCursor(void* context, void* cursor)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-cursor");
    fake.selectedCursorShapes.push_back(GetFakeCursor(cursor).shape);
    if (FailMouseOperation(fake, "set-cursor"))
    {
        return false;
    }

    fake.activeCursor = cursor;
    return true;
}

void FakeDestroyCursor(void* context, void* cursor)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("destroy-cursor");
    const auto iterator = std::ranges::find_if(fake.cursors,
                                               [cursor](const FakeCursor& candidate)
                                               {
                                                   return &candidate == cursor;
                                               });
    if (iterator == fake.cursors.end())
    {
        return;
    }

    fake.destroyedCursorShapes.push_back(iterator->shape);
    if (fake.activeCursor == cursor)
    {
        fake.activeCursor = nullptr;
    }
    fake.cursors.erase(iterator);
}

[[nodiscard]] bool FakeShowCursor(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("show-cursor");
    if (FailMouseOperation(fake, "show-cursor"))
    {
        return false;
    }

    fake.cursorVisible = true;
    return true;
}

[[nodiscard]] bool FakeHideCursor(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("hide-cursor");
    if (FailMouseOperation(fake, "hide-cursor"))
    {
        return false;
    }

    fake.cursorVisible = false;
    return true;
}

[[nodiscard]] bool FakeIsCursorVisible(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("is-cursor-visible");
    return fake.cursorVisible;
}

[[nodiscard]] bool FakeSupportsClipboardText(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("supports-clipboard-text");
    return fake.clipboardTextSupported;
}

[[nodiscard]] BackendClipboardTextResult FakeGetClipboardText(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-clipboard-text");
    if (fake.clipboardGetReturnsNull)
    {
        const std::string error = fake.clipboardGetError.empty()
                                      ? "synthetic get-clipboard-text failure"
                                      : fake.clipboardGetError;
        static_cast<void>(SDL_SetError("%s", error.c_str()));
        return BackendClipboardTextResult{.text = nullptr, .errorText = error};
    }

    fake.clipboardReadBuffer = fake.clipboardText;
    return BackendClipboardTextResult{.text = fake.clipboardReadBuffer.data(),
                                      .errorText = fake.clipboardGetError};
}

void FakeFreeClipboardText(void* context, char* text)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("free-clipboard-text");
    ++fake.clipboardFreeCalls;
    if (text == fake.clipboardReadBuffer.data())
    {
        fake.clipboardReadBuffer = "freed clipboard text";
    }
}

[[nodiscard]] bool FakeSetClipboardText(void* context, const char* text)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-clipboard-text");
    if (FailServiceOperation(fake, "set-clipboard-text"))
    {
        return false;
    }

    fake.clipboardText = text;
    return true;
}

[[nodiscard]] bool FakeOpenExternalUri(void* context, const char* uri)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("open-external-uri");
    if (FailServiceOperation(fake, "open-external-uri"))
    {
        return false;
    }

    fake.openedExternalUris.emplace_back(uri);
    return true;
}

void FakeShowDialog(void* context, const BackendDialogRequestDesc& desc)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("show-dialog");

    FakeDialogLaunch launch{.kind = desc.kind,
                            .parentWindow = desc.parentWindow,
                            .defaultLocation =
                                desc.defaultLocation != nullptr
                                    ? std::optional<std::string>{desc.defaultLocation}
                                    : std::nullopt,
                            .allowMultipleSelection = desc.allowMultipleSelection,
                            .callback = desc.callback,
                            .userdata = desc.userdata};
    if (desc.filters != nullptr)
    {
        for (int index = 0; index < desc.filterCount; ++index)
        {
            launch.filters.push_back(pond::platform::DialogFileFilter{
                .name = desc.filters[index].name, .pattern = desc.filters[index].pattern});
        }
    }
    fake.dialogLaunches.push_back(std::move(launch));
}

void CompleteDialogSelection(FakeDialogLaunch& launch, std::span<const std::string> paths,
                             int selectedFilter)
{
    std::vector<const char*> fileList;
    fileList.reserve(paths.size() + 1U);
    for (const std::string& path : paths)
    {
        fileList.push_back(path.c_str());
    }
    fileList.push_back(nullptr);

    launch.callback(launch.userdata, fileList.data(), selectedFilter);
}

void CompleteDialogCancellation(FakeDialogLaunch& launch)
{
    const char* const fileList[]{nullptr};
    launch.callback(launch.userdata, fileList, -1);
}

void CompleteDialogFailure(FakeDialogLaunch& launch, std::string_view message)
{
    static_cast<void>(SDL_SetError("%.*s", static_cast<int>(message.size()), message.data()));
    launch.callback(launch.userdata, nullptr, -1);
    static_cast<void>(SDL_ClearError());
}

[[nodiscard]] void* FakeCreateWindow(void* context, const BackendWindowCreateDesc& desc)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.createWindowCalls;
    fake.calls.emplace_back("create-window");
    if (FailWindowOperation(fake, "create-window"))
    {
        return nullptr;
    }

    const std::uint32_t backendId =
        fake.scriptedBackendWindowIdIndex < fake.scriptedBackendWindowIds.size()
            ? fake.scriptedBackendWindowIds[fake.scriptedBackendWindowIdIndex++]
            : fake.nextBackendWindowId++;
    fake.windows.emplace_back(FakeWindow{.backendId = backendId,
                                         .title = desc.title,
                                         .width = desc.width,
                                         .height = desc.height,
                                         .pixelWidth = desc.width,
                                         .pixelHeight = desc.height,
                                         .backendDisplayId = fake.defaultWindowDisplayId,
                                         .pixelDensity = fake.defaultWindowPixelDensity,
                                         .displayScale = fake.defaultWindowDisplayScale,
                                         .resizable = desc.resizable,
                                         .highPixelDensity = desc.highPixelDensity,
                                         .graphicsCompatibility = desc.graphicsCompatibility});
    return &fake.windows.back();
}

void FakeDestroyWindow(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.destroyWindowCalls;
    fake.calls.emplace_back("destroy-window");
    if (fake.destroyOverwritesError)
    {
        static_cast<void>(SDL_SetError("cleanup overwrote the primary error"));
    }

    const auto iterator = std::ranges::find_if(fake.windows,
                                               [window](const FakeWindow& candidate)
                                               {
                                                   return &candidate == window;
                                               });
    if (iterator != fake.windows.end())
    {
        fake.windows.erase(iterator);
    }
}

[[nodiscard]] std::uint32_t FakeGetWindowId(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-window-id");
    return FailWindowOperation(fake, "get-window-id") ? 0 : GetFakeWindow(window).backendId;
}

[[nodiscard]] const char* FakeGetWindowTitle(void* context, void* window)
{
    GetFake(context).calls.emplace_back("get-window-title");
    return GetFakeWindow(window).title.c_str();
}

[[nodiscard]] bool FakeSetWindowTitle(void* context, void* window, const char* title)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-window-title");
    if (FailWindowOperation(fake, "set-window-title"))
    {
        return false;
    }
    GetFakeWindow(window).title = title;
    return true;
}

[[nodiscard]] bool FakeGetWindowPosition(void* context, void* window, int* x, int* y)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-window-position");
    if (FailWindowOperation(fake, "get-window-position"))
    {
        return false;
    }
    const FakeWindow& fakeWindow = GetFakeWindow(window);
    *x = fakeWindow.x;
    *y = fakeWindow.y;
    return true;
}

[[nodiscard]] bool FakeSetWindowPosition(void* context, void* window, int x, int y)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-window-position");
    if (FailWindowOperation(fake, "set-window-position"))
    {
        return false;
    }
    FakeWindow& fakeWindow = GetFakeWindow(window);
    fakeWindow.x = x;
    fakeWindow.y = y;
    return true;
}

[[nodiscard]] bool FakeGetWindowSize(void* context, void* window, int* width, int* height)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-window-size");
    if (FailWindowOperation(fake, "get-window-size"))
    {
        return false;
    }
    const FakeWindow& fakeWindow = GetFakeWindow(window);
    *width = fakeWindow.width;
    *height = fakeWindow.height;
    return true;
}

[[nodiscard]] bool FakeGetWindowSizeInPixels(void* context, void* window, int* width, int* height)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-window-size-in-pixels");
    if (FailWindowOperation(fake, "get-window-size-in-pixels"))
    {
        return false;
    }
    const FakeWindow& fakeWindow = GetFakeWindow(window);
    *width = fakeWindow.pixelWidth;
    *height = fakeWindow.pixelHeight;
    return true;
}

[[nodiscard]] bool FakeSetWindowSize(void* context, void* window, int width, int height)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-window-size");
    if (FailWindowOperation(fake, "set-window-size"))
    {
        return false;
    }
    FakeWindow& fakeWindow = GetFakeWindow(window);
    fakeWindow.width = width;
    fakeWindow.height = height;
    return true;
}

[[nodiscard]] bool FakeSetWindowMinimumSize(void* context, void* window, int width, int height)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-window-minimum-size");
    if (FailWindowOperation(fake, "set-window-minimum-size"))
    {
        return false;
    }
    FakeWindow& fakeWindow = GetFakeWindow(window);
    fakeWindow.minimumWidth = width;
    fakeWindow.minimumHeight = height;
    return true;
}

[[nodiscard]] bool FakeShowWindow(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
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

[[nodiscard]] bool FakeHideWindow(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
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

[[nodiscard]] bool FakeGetWindowProperties(void* context, void* window,
                                           BackendWindowProperties* properties)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-window-properties");
    if (FailWindowOperation(fake, "get-window-properties"))
    {
        return false;
    }

    const FakeWindow& fakeWindow = GetFakeWindow(window);
    *properties =
        BackendWindowProperties{.desktopFullscreen = fakeWindow.desktopFullscreen,
                                .hidden = !fakeWindow.visible,
                                .borderless = fakeWindow.borderless,
                                .resizable = fakeWindow.resizable,
                                .minimized = fakeWindow.minimized || fakeWindow.pendingMinimized,
                                .maximized = fakeWindow.maximized || fakeWindow.pendingMaximized,
                                .inputFocus = fakeWindow.inputFocus,
                                .alwaysOnTop = fakeWindow.alwaysOnTop};
    return true;
}

[[nodiscard]] BackendWindowOperationResult FakeSetFullscreenModeToDesktop(void* context, void*)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-window-fullscreen-mode-to-desktop");
    return GetWindowOperationResult(fake, "set-window-fullscreen-mode-to-desktop");
}

[[nodiscard]] BackendWindowOperationResult FakeSetWindowFullscreen(void* context, void* window,
                                                                   bool fullscreen)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-window-fullscreen");
    const BackendWindowOperationResult result =
        GetWindowOperationResult(fake, "set-window-fullscreen");
    if (result == BackendWindowOperationResult::Succeeded && fake.applyWindowOperationEffects)
    {
        GetFakeWindow(window).desktopFullscreen = fullscreen;
    }
    return result;
}

[[nodiscard]] BackendWindowOperationResult FakeSetWindowBordered(void* context, void* window,
                                                                 bool bordered)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-window-bordered");
    const BackendWindowOperationResult result =
        GetWindowOperationResult(fake, "set-window-bordered");
    if (result == BackendWindowOperationResult::Succeeded && fake.applyWindowOperationEffects)
    {
        GetFakeWindow(window).borderless = !bordered;
    }
    return result;
}

[[nodiscard]] BackendWindowOperationResult FakeSetWindowResizable(void* context, void* window,
                                                                  bool resizable)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-window-resizable");
    const BackendWindowOperationResult result =
        GetWindowOperationResult(fake, "set-window-resizable");
    if (result == BackendWindowOperationResult::Succeeded && fake.applyWindowOperationEffects)
    {
        GetFakeWindow(window).resizable = resizable;
    }
    return result;
}

[[nodiscard]] BackendWindowOperationResult FakeSetWindowAlwaysOnTop(void* context, void* window,
                                                                    bool alwaysOnTop)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-window-always-on-top");
    const BackendWindowOperationResult result =
        GetWindowOperationResult(fake, "set-window-always-on-top");
    if (result == BackendWindowOperationResult::Succeeded && fake.applyWindowOperationEffects)
    {
        GetFakeWindow(window).alwaysOnTop = alwaysOnTop;
    }
    return result;
}

[[nodiscard]] BackendWindowOperationResult FakeMinimizeWindow(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("minimize-window");
    const BackendWindowOperationResult result = GetWindowOperationResult(fake, "minimize-window");
    if (result == BackendWindowOperationResult::Succeeded && fake.applyWindowOperationEffects)
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

[[nodiscard]] BackendWindowOperationResult FakeMaximizeWindow(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("maximize-window");
    const BackendWindowOperationResult result = GetWindowOperationResult(fake, "maximize-window");
    if (result == BackendWindowOperationResult::Succeeded && fake.applyWindowOperationEffects)
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

[[nodiscard]] BackendWindowOperationResult FakeRestoreWindow(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("restore-window");
    const BackendWindowOperationResult result = GetWindowOperationResult(fake, "restore-window");
    if (result == BackendWindowOperationResult::Succeeded && fake.applyWindowOperationEffects)
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

[[nodiscard]] bool FakeStartWindowTextInput(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("start-text-input");
    if (FailWindowOperation(fake, "start-text-input"))
    {
        return false;
    }

    GetFakeWindow(window).textInputActive = true;
    return true;
}

[[nodiscard]] bool FakeStopWindowTextInput(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("stop-text-input");
    if (FailWindowOperation(fake, "stop-text-input"))
    {
        return false;
    }

    GetFakeWindow(window).textInputActive = false;
    return true;
}

[[nodiscard]] bool FakeIsWindowTextInputActive(void* context, void* window)
{
    GetFake(context).calls.emplace_back("is-text-input-active");
    return GetFakeWindow(window).textInputActive;
}

[[nodiscard]] bool FakeClearWindowTextComposition(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("clear-text-composition");
    if (FailWindowOperation(fake, "clear-text-composition"))
    {
        return false;
    }

    ++GetFakeWindow(window).clearedTextCompositions;
    return true;
}

[[nodiscard]] bool FakeSetWindowTextInputArea(void* context, void* window,
                                              const BackendTextInputArea* area)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-text-input-area");
    if (FailWindowOperation(fake, "set-text-input-area"))
    {
        return false;
    }

    GetFakeWindow(window).textInputArea =
        area != nullptr ? std::optional<BackendTextInputArea>{*area} : std::nullopt;
    return true;
}

[[nodiscard]] bool FakeSetWindowMouseGrab(void* context, void* window, bool grabbed)
{
    FakeRuntimeBackend& fake = GetFake(context);
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

[[nodiscard]] bool FakeIsWindowMouseGrabbed(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("is-window-mouse-grabbed");
    return GetFakeWindow(window).mouseGrabbed;
}

[[nodiscard]] bool FakeSetWindowRelativeMouseMode(void* context, void* window, bool enabled)
{
    FakeRuntimeBackend& fake = GetFake(context);
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

[[nodiscard]] bool FakeIsWindowRelativeMouseModeEnabled(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("is-window-relative-mouse-mode-enabled");
    return GetFakeWindow(window).relativeMouseModeEnabled;
}

[[nodiscard]] BackendNativeWindowHandleResult FakeGetNativeWindowHandle(
    void* context, void*, void** cachedMetalView, pond::platform::NativeWindowHandle* handle)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-native-handle");
    const BackendWindowOperationResult operationResult =
        GetWindowOperationResult(fake, "get-native-handle");
    if (operationResult == BackendWindowOperationResult::Unsupported)
    {
        return BackendNativeWindowHandleResult{.status =
                                                   BackendNativeWindowHandleStatus::Unsupported,
                                               .message = "synthetic native window unsupported"};
    }
    if (operationResult == BackendWindowOperationResult::Failed)
    {
        return BackendNativeWindowHandleResult{.status = BackendNativeWindowHandleStatus::Failed,
                                               .operation = "get-native-handle",
                                               .captureSdlError = true};
    }

    switch (pond::platform::detail::GetNativeWindowDriver(fake.nativeVideoDriver))
    {
    case BackendNativeWindowDriver::Win32:
        if (fake.nativeWin32Instance == nullptr || fake.nativeWin32Window == nullptr)
        {
            return BackendNativeWindowHandleResult{.status =
                                                       BackendNativeWindowHandleStatus::Failed,
                                                   .message = "missing fake Win32 data"};
        }
        *handle = pond::platform::NativeWin32Window{.instance = fake.nativeWin32Instance,
                                                    .window = fake.nativeWin32Window};
        break;
    case BackendNativeWindowDriver::X11:
        if (fake.nativeX11Display == nullptr || fake.nativeX11Window == 0)
        {
            return BackendNativeWindowHandleResult{.status =
                                                       BackendNativeWindowHandleStatus::Failed,
                                                   .message = "missing fake X11 data"};
        }
        *handle = pond::platform::NativeX11Window{.display = fake.nativeX11Display,
                                                  .window = fake.nativeX11Window};
        break;
    case BackendNativeWindowDriver::Wayland:
        if (fake.nativeWaylandDisplay == nullptr || fake.nativeWaylandSurface == nullptr)
        {
            return BackendNativeWindowHandleResult{.status =
                                                       BackendNativeWindowHandleStatus::Failed,
                                                   .message = "missing fake Wayland data"};
        }
        *handle = pond::platform::NativeWaylandWindow{.display = fake.nativeWaylandDisplay,
                                                      .surface = fake.nativeWaylandSurface};
        break;
    case BackendNativeWindowDriver::Cocoa:
        if (cachedMetalView == nullptr)
        {
            return BackendNativeWindowHandleResult{.status =
                                                       BackendNativeWindowHandleStatus::Failed,
                                                   .message = "missing fake Cocoa cache storage"};
        }
        if (*cachedMetalView == nullptr)
        {
            ++fake.createMetalViewCalls;
            fake.calls.emplace_back("create-metal-view");
            *cachedMetalView = fake.nativeCocoaMetalView;
        }
        *handle = pond::platform::NativeCocoaWindow{.metalLayer = fake.nativeCocoaMetalLayer};
        break;
    case BackendNativeWindowDriver::Unsupported:
        return BackendNativeWindowHandleResult{.status =
                                                   BackendNativeWindowHandleStatus::Unsupported,
                                               .message = "synthetic unsupported video driver"};
    }

    return BackendNativeWindowHandleResult{.status = BackendNativeWindowHandleStatus::Succeeded};
}

void FakeDestroyMetalView(void* context, void* metalView)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.destroyMetalViewCalls;
    fake.destroyedMetalViews.push_back(metalView);
    fake.calls.emplace_back("destroy-metal-view");
}

[[nodiscard]] bool FakeEnumerateDisplays(void* context, std::vector<std::uint32_t>& displayIds)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.enumerateDisplayCalls;
    fake.calls.emplace_back("enumerate-displays");
    if (FailDisplayOperation(fake, "enumerate-displays"))
    {
        return false;
    }

    displayIds = fake.connectedDisplayIds;
    return true;
}

[[nodiscard]] const char* FakeGetDisplayName(void* context, std::uint32_t backendDisplayId)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-display-name");
    if (FailDisplayOperation(fake, "get-display-name"))
    {
        return nullptr;
    }
    return GetFakeDisplay(fake, backendDisplayId).name.c_str();
}

[[nodiscard]] bool FakeGetDisplayBounds(void* context, std::uint32_t backendDisplayId,
                                        BackendScreenRectangle* bounds)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-display-bounds");
    if (FailDisplayOperation(fake, "get-display-bounds"))
    {
        return false;
    }
    *bounds = GetFakeDisplay(fake, backendDisplayId).bounds;
    return true;
}

[[nodiscard]] bool FakeGetDisplayUsableBounds(void* context, std::uint32_t backendDisplayId,
                                              BackendScreenRectangle* bounds)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-display-usable-bounds");
    if (FailDisplayOperation(fake, "get-display-usable-bounds"))
    {
        return false;
    }
    *bounds = GetFakeDisplay(fake, backendDisplayId).usableBounds;
    return true;
}

[[nodiscard]] bool FakeGetCurrentDisplayRefreshRate(void* context, std::uint32_t backendDisplayId,
                                                    float* refreshRateHertz)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-current-display-refresh-rate");
    if (FailDisplayOperation(fake, "get-current-display-refresh-rate"))
    {
        return false;
    }
    *refreshRateHertz = GetFakeDisplay(fake, backendDisplayId).refreshRateHertz;
    return true;
}

[[nodiscard]] BackendDisplayOrientation FakeGetCurrentDisplayOrientation(
    void* context, std::uint32_t backendDisplayId)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-current-display-orientation");
    return GetFakeDisplay(fake, backendDisplayId).orientation;
}

[[nodiscard]] float FakeGetDisplayContentScale(void* context, std::uint32_t backendDisplayId)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-display-content-scale");
    if (FailDisplayOperation(fake, "get-display-content-scale"))
    {
        return 0.0F;
    }
    return GetFakeDisplay(fake, backendDisplayId).contentScale;
}

[[nodiscard]] std::uint32_t FakeGetDisplayForWindow(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-display-for-window");
    if (FailDisplayOperation(fake, "get-display-for-window"))
    {
        return 0;
    }
    return GetFakeWindow(window).backendDisplayId;
}

[[nodiscard]] float FakeGetWindowPixelDensity(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-window-pixel-density");
    if (FailDisplayOperation(fake, "get-window-pixel-density"))
    {
        return 0.0F;
    }
    return GetFakeWindow(window).pixelDensity;
}

[[nodiscard]] float FakeGetWindowDisplayScale(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-window-display-scale");
    if (FailDisplayOperation(fake, "get-window-display-scale"))
    {
        return 0.0F;
    }
    return GetFakeWindow(window).displayScale;
}

[[nodiscard]] PlatformRuntimeBackend MakeBackend(FakeRuntimeBackend& fake)
{
    return PlatformRuntimeBackend{&fake,
                                  FakeIsMainThread,
                                  FakeHasInitializedSubsystems,
                                  FakeHasExpectedRuntimeSubsystems,
                                  FakeGetAppMetadataProperty,
                                  FakeSetAppMetadataProperty,
                                  FakeGetHint,
                                  FakeSetHintOverride,
                                  FakeResetHint,
                                  FakeInitializeVideo,
                                  FakeQuit,
                                  FakeGetTicksNanoseconds,
                                  FakePollEvent,
                                  FakeSupportsGlobalMouse,
                                  FakeGetGlobalMousePosition,
                                  FakeSetMouseCapture,
                                  FakeCreateSystemCursor,
                                  FakeSetCursor,
                                  FakeDestroyCursor,
                                  FakeShowCursor,
                                  FakeHideCursor,
                                  FakeIsCursorVisible,
                                  FakeSupportsClipboardText,
                                  FakeGetClipboardText,
                                  FakeFreeClipboardText,
                                  FakeSetClipboardText,
                                  FakeOpenExternalUri,
                                  FakeShowDialog};
}

[[nodiscard]] PlatformWindowBackend MakeWindowBackend(FakeRuntimeBackend& fake)
{
    return PlatformWindowBackend{&fake,
                                 FakeCreateWindow,
                                 FakeDestroyWindow,
                                 FakeGetWindowId,
                                 FakeGetWindowTitle,
                                 FakeSetWindowTitle,
                                 FakeGetWindowPosition,
                                 FakeSetWindowPosition,
                                 FakeGetWindowSize,
                                 FakeGetWindowSizeInPixels,
                                 FakeSetWindowSize,
                                 FakeSetWindowMinimumSize,
                                 FakeShowWindow,
                                 FakeHideWindow,
                                 FakeGetWindowProperties,
                                 FakeSetFullscreenModeToDesktop,
                                 FakeSetWindowFullscreen,
                                 FakeSetWindowBordered,
                                 FakeSetWindowResizable,
                                 FakeSetWindowAlwaysOnTop,
                                 FakeMinimizeWindow,
                                 FakeMaximizeWindow,
                                 FakeRestoreWindow,
                                 FakeStartWindowTextInput,
                                 FakeStopWindowTextInput,
                                 FakeIsWindowTextInputActive,
                                 FakeClearWindowTextComposition,
                                 FakeSetWindowTextInputArea,
                                 FakeSetWindowMouseGrab,
                                 FakeIsWindowMouseGrabbed,
                                 FakeSetWindowRelativeMouseMode,
                                 FakeIsWindowRelativeMouseModeEnabled,
                                 FakeGetNativeWindowHandle,
                                 FakeDestroyMetalView};
}

[[nodiscard]] PlatformDisplayBackend MakeDisplayBackend(FakeRuntimeBackend& fake)
{
    return PlatformDisplayBackend{&fake,
                                  FakeEnumerateDisplays,
                                  FakeGetDisplayName,
                                  FakeGetDisplayBounds,
                                  FakeGetDisplayUsableBounds,
                                  FakeGetCurrentDisplayRefreshRate,
                                  FakeGetCurrentDisplayOrientation,
                                  FakeGetDisplayContentScale,
                                  FakeGetDisplayForWindow,
                                  FakeGetWindowPixelDensity,
                                  FakeGetWindowDisplayScale};
}

class PlatformRuntimeBackendTests : public testing::Test
{
protected:
    void SetUp() override
    {
        static_cast<void>(SDL_ClearError());
        m_backend = MakeBackend(m_fake);
        m_windowBackend = MakeWindowBackend(m_fake);
        m_displayBackend = MakeDisplayBackend(m_fake);
        pond::platform::detail::SetPlatformRuntimeBackendForTesting(&m_backend);
        pond::platform::detail::SetPlatformWindowBackendForTesting(&m_windowBackend);
        pond::platform::detail::SetPlatformDisplayBackendForTesting(&m_displayBackend);
    }

    void TearDown() override
    {
        pond::platform::detail::SetPlatformDisplayBackendForTesting(nullptr);
        pond::platform::detail::SetPlatformWindowBackendForTesting(nullptr);
        pond::platform::detail::SetPlatformRuntimeBackendForTesting(nullptr);
        static_cast<void>(SDL_ClearError());
    }

    FakeRuntimeBackend m_fake;
    PlatformRuntimeBackend m_backend;
    PlatformWindowBackend m_windowBackend;
    PlatformDisplayBackend m_displayBackend;
};

TEST_F(PlatformRuntimeBackendTests, AppliesMetadataPoliciesAndProvidesTimestamps)
{
    m_fake.hints[pond::platform::detail::kMouseFocusClickThroughHint] = "previous";
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
        EXPECT_EQ(GetHintValue(m_fake, pond::platform::detail::kMouseFocusClickThroughHint),
                  std::string_view{"1"});
        EXPECT_EQ(GetHintValue(m_fake, pond::platform::detail::kMouseAutoCaptureHint),
                  std::string_view{"0"});
        EXPECT_TRUE(m_fake.policiesObservedDuringInitialization);
        EXPECT_TRUE(m_fake.metadataObservedDuringInitialization);
        EXPECT_EQ(runtime.Now().GetTimeSinceEpoch(), std::chrono::nanoseconds{987'654'321});
    }

    EXPECT_EQ(m_fake.quitCalls, 1);
    EXPECT_EQ(GetHintValue(m_fake, pond::platform::detail::kMouseFocusClickThroughHint),
              std::string_view{"previous"});
    EXPECT_EQ(GetHintValue(m_fake, pond::platform::detail::kMouseAutoCaptureHint), nullptr);
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)], "Prior App");
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)], "1.0");
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)],
              "org.ponder.prior");
    EXPECT_EQ(m_fake.runtimeSubsystemChecks, 1);
}

TEST_F(PlatformRuntimeBackendTests, RestoresAOriginallyPresentEmptyHint)
{
    m_fake.hints[pond::platform::detail::kMouseFocusClickThroughHint] = "";

    {
        auto result =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(result.HasValue());
        pond::platform::PlatformRuntime runtime = std::move(result).GetValue();
    }

    const auto iterator = m_fake.hints.find(pond::platform::detail::kMouseFocusClickThroughHint);
    ASSERT_NE(iterator, m_fake.hints.end());
    EXPECT_TRUE(iterator->second.empty());
}

TEST_F(PlatformRuntimeBackendTests, PassesAbsentOptionalMetadataAsNull)
{
    auto result = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(result.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(result).GetValue();

    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)], "ponder");
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

TEST_F(PlatformRuntimeBackendTests, RestoresHintsAfterHintFailureAndPermitsRetry)
{
    m_fake.hints[pond::platform::detail::kMouseFocusClickThroughHint] = "old-focus";
    m_fake.hints[pond::platform::detail::kMouseAutoCaptureHint] = "old-capture";
    m_fake.failingHint = pond::platform::detail::kMouseAutoCaptureHint;
    m_fake.hintFailuresRemaining = 1;

    auto result = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(result.GetError().GetMessage().find("synthetic hint failure"),
              std::string_view::npos);
    EXPECT_EQ(GetHintValue(m_fake, pond::platform::detail::kMouseFocusClickThroughHint),
              std::string_view{"old-focus"});
    EXPECT_EQ(GetHintValue(m_fake, pond::platform::detail::kMouseAutoCaptureHint),
              std::string_view{"old-capture"});
    EXPECT_EQ(m_fake.initializeVideoCalls, 0);

    auto retry = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();
}

TEST_F(PlatformRuntimeBackendTests, RollsBackMetadataFailureAndPermitsRetry)
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
    EXPECT_EQ(m_fake.initializeVideoCalls, 0);
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)], "Prior App");
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)], "1.0");
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)],
              "org.ponder.prior");

    auto retry = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();
}

TEST_F(PlatformRuntimeBackendTests, CapturesInitializationFailureBeforeRollback)
{
    m_fake.hints[pond::platform::detail::kMouseFocusClickThroughHint] = "old-focus";
    m_fake.initializeVideoSucceeds = false;

    auto result = pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(result.GetError().GetMessage().find("synthetic video initialization failure"),
              std::string_view::npos);
    EXPECT_EQ(m_fake.quitCalls, 1);
    EXPECT_EQ(GetHintValue(m_fake, pond::platform::detail::kMouseFocusClickThroughHint),
              std::string_view{"old-focus"});

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
    m_fake.hints[pond::platform::detail::kMouseFocusClickThroughHint] = "old-focus";
    m_fake.attemptCreateDuringRestoration = true;

    {
        auto result =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(result.HasValue());
        pond::platform::PlatformRuntime runtime = std::move(result).GetValue();
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
        .graphicsCompatibility = pond::platform::WindowGraphicsCompatibility::Vulkan};

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
              pond::platform::WindowGraphicsCompatibility::Vulkan);

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
    const auto expectBackendFailure = [](const auto& result, std::string_view operation)
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
        EXPECT_NE(result.GetError().GetMessage().find(operation), std::string_view::npos);
        EXPECT_NE(result.GetError().GetMessage().find("window 1"), std::string_view::npos);
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
    expectBackendFailure(window.GetLogicalSize(), "SDL_GetWindowSize");
    m_fake.windows.front().width = 100;
    m_fake.windows.front().pixelHeight = -1;
    expectBackendFailure(window.GetPixelSize(), "SDL_GetWindowSizeInPixels");
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
    EXPECT_NE(result.GetError().GetMessage().find("synthetic minimize-window unsupported"),
              std::string_view::npos);

    configureUnsupported("maximize-window");
    result = window.Maximize();
    expectUnsupported(result, "SDL_MaximizeWindow");
    EXPECT_NE(result.GetError().GetMessage().find("synthetic maximize-window unsupported"),
              std::string_view::npos);

    backendWindow.minimized = true;
    configureUnsupported("restore-window");
    result = window.Restore();
    expectUnsupported(result, "SDL_RestoreWindow");
    EXPECT_NE(result.GetError().GetMessage().find("synthetic restore-window unsupported"),
              std::string_view::npos);
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
        [](const pond::core::VoidResult& result, std::string_view operation)
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));
        EXPECT_NE(result.GetError().GetMessage().find(operation), std::string_view::npos);
        EXPECT_NE(result.GetError().GetMessage().find("window 1"), std::string_view::npos);
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
                      "decoration");
    EXPECT_EQ(m_fake.calls.size(), callCount + 1);
    expectUnsupported(window.SetResizable(false), "resizability");
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
    const auto expectBackendFailure = [](const auto& result, std::string_view operation)
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
        EXPECT_NE(result.GetError().GetMessage().find(operation), std::string_view::npos);
        EXPECT_NE(result.GetError().GetMessage().find("window 1"), std::string_view::npos);
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
        EXPECT_NE(result.GetError().GetMessage().find("window 1"), std::string_view::npos);
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
        EXPECT_NE(result.GetError().GetMessage().find("display 1"), std::string_view::npos);
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
        EXPECT_NE(result.GetError().GetMessage().find("window 1"), std::string_view::npos);
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
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    desc.graphicsCompatibility = pond::platform::WindowGraphicsCompatibility::Vulkan;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    std::optional<pond::platform::Window> window;
    window.emplace(std::move(windowResult).GetValue());

    m_fake.nativeVideoDriver = "windows";
    auto win32Result = window->GetNativeHandle();
    ASSERT_TRUE(win32Result.HasValue()) << win32Result.GetError().GetMessage();
    ASSERT_TRUE(std::holds_alternative<pond::platform::NativeWin32Window>(win32Result.GetValue()));
    EXPECT_EQ(std::get<pond::platform::NativeWin32Window>(win32Result.GetValue()),
              (pond::platform::NativeWin32Window{.instance = m_fake.nativeWin32Instance,
                                                 .window = m_fake.nativeWin32Window}));

    m_fake.nativeVideoDriver = "x11";
    auto x11Result = window->GetNativeHandle();
    ASSERT_TRUE(x11Result.HasValue()) << x11Result.GetError().GetMessage();
    ASSERT_TRUE(std::holds_alternative<pond::platform::NativeX11Window>(x11Result.GetValue()));
    EXPECT_EQ(std::get<pond::platform::NativeX11Window>(x11Result.GetValue()),
              (pond::platform::NativeX11Window{.display = m_fake.nativeX11Display,
                                               .window = m_fake.nativeX11Window}));

    m_fake.nativeVideoDriver = "wayland";
    auto waylandResult = window->GetNativeHandle();
    ASSERT_TRUE(waylandResult.HasValue()) << waylandResult.GetError().GetMessage();
    ASSERT_TRUE(
        std::holds_alternative<pond::platform::NativeWaylandWindow>(waylandResult.GetValue()));
    EXPECT_EQ(std::get<pond::platform::NativeWaylandWindow>(waylandResult.GetValue()),
              (pond::platform::NativeWaylandWindow{.display = m_fake.nativeWaylandDisplay,
                                                   .surface = m_fake.nativeWaylandSurface}));

    m_fake.nativeVideoDriver = "cocoa";
    auto cocoaResult = window->GetNativeHandle();
    ASSERT_TRUE(cocoaResult.HasValue()) << cocoaResult.GetError().GetMessage();
    ASSERT_TRUE(std::holds_alternative<pond::platform::NativeCocoaWindow>(cocoaResult.GetValue()));
    EXPECT_EQ(std::get<pond::platform::NativeCocoaWindow>(cocoaResult.GetValue()),
              (pond::platform::NativeCocoaWindow{.metalLayer = m_fake.nativeCocoaMetalLayer}));
    EXPECT_EQ(m_fake.createMetalViewCalls, 1);

    auto secondCocoaResult = window->GetNativeHandle();
    ASSERT_TRUE(secondCocoaResult.HasValue()) << secondCocoaResult.GetError().GetMessage();
    EXPECT_EQ(std::get<pond::platform::NativeCocoaWindow>(secondCocoaResult.GetValue()),
              (pond::platform::NativeCocoaWindow{.metalLayer = m_fake.nativeCocoaMetalLayer}));
    EXPECT_EQ(m_fake.createMetalViewCalls, 1);

    window.reset();
    EXPECT_EQ(m_fake.destroyMetalViewCalls, 1);
    ASSERT_EQ(m_fake.destroyedMetalViews.size(), 1U);
    EXPECT_EQ(m_fake.destroyedMetalViews.front(), m_fake.nativeCocoaMetalView);
    const auto metalDestroy = std::ranges::find(m_fake.calls, "destroy-metal-view");
    const auto windowDestroy = std::ranges::find(m_fake.calls, "destroy-window");
    ASSERT_NE(metalDestroy, m_fake.calls.end());
    ASSERT_NE(windowDestroy, m_fake.calls.end());
    EXPECT_LT(metalDestroy, windowDestroy);
}

TEST_F(PlatformRuntimeBackendTests, RejectsNativeHandlesForDefaultWindowsAndUnsupportedDrivers)
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
}

TEST_F(PlatformRuntimeBackendTests, ReportsNativeHandleBackendFailures)
{
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
                                               .timestamp = pond::platform::PlatformTimestamp{
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
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{100}},
            .windowId = second.GetId(),
            .position = {-250, 90}});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowFocusChangedEvent{
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{200}},
            .windowId = first.GetId(),
            .focused = true});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowCloseRequestedEvent{
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{300}},
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
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{100}},
            .windowId = window.GetId()});
    EXPECT_EQ(m_fake.destroyWindowCalls, 0);
    EXPECT_EQ(window.GetId(), pond::platform::WindowId{1});

    ExpectPolledEvent(runtime.PollEvent(), pond::platform::QuitRequestedEvent{
                                               .timestamp = pond::platform::PlatformTimestamp{
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
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{100}},
            .windowId = window.GetId(),
            .visible = true});

    backendWindow.visible = false;
    backendWindow.maximized = false;
    auto state = window.GetState();
    ASSERT_TRUE(state.HasValue());
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
                                               .timestamp = pond::platform::PlatformTimestamp{
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
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{300}},
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
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{200}},
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
                                               .timestamp = pond::platform::PlatformTimestamp{
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
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{100}},
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
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{100}},
            .displayId = removedId});

    m_fake.eventQueue.push_back(
        {.event = MakeQueuedDisplayEvent(SDL_EVENT_DISPLAY_REMOVED, 10, 0, 0, 200)});
    m_fake.eventQueue.push_back({.event = MakeQueuedQuitEvent(300)});
    ExpectPolledEvent(runtime.PollEvent(), pond::platform::QuitRequestedEvent{
                                               .timestamp = pond::platform::PlatformTimestamp{
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
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{100}},
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
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{100}},
            .windowId = window.GetId(),
            .displayId = displayId});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowDisplayChangedEvent{
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{200}},
            .windowId = window.GetId(),
            .displayId = std::nullopt});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::WindowDisplayChangedEvent{
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{300}},
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
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{100}},
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
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{200}},
            .windowId = second.GetId(),
            .text = inputText});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::TextCompositionEvent{
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{300}},
            .windowId = first.GetId(),
            .text = compositionText,
            .selection = pond::platform::TextCompositionRange{0, 0}});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::KeyboardKeyEvent{
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{400}},
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
        EXPECT_NE(result.GetError().GetMessage().find("window 1"), std::string_view::npos);
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
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{100}},
            .windowId = first.GetId(),
            .position = {10.5F, -2.25F},
            .relativeMovement = {1.5F, -0.5F}});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::MouseButtonEvent{
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{200}},
            .windowId = second.GetId(),
            .position = {3.0F, 4.0F},
            .button = pond::platform::MouseButton::X2,
            .pressed = true});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::MouseWheelEvent{
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{300}},
            .windowId = first.GetId(),
            .position = {8.0F, 9.0F},
            .horizontal = -0.5F,
            .vertical = 1.25F});
    ExpectPolledEvent(runtime.PollEvent(), pond::platform::QuitRequestedEvent{
                                               .timestamp = pond::platform::PlatformTimestamp{
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
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{100}},
            .windowId = first.GetId(),
            .sourceApplication = std::optional<std::string>{"source-app"}});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::DroppedFileEvent{
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{200}},
            .windowId = second.GetId(),
            .path = std::filesystem::path{"C:/tmp/dropped.sdf"},
            .position = {3.0F, 4.0F},
            .sourceApplication = std::optional<std::string>{"source-app"}});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::DroppedTextEvent{
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{300}},
            .windowId = second.GetId(),
            .text = text,
            .position = {5.0F, 6.0F},
            .sourceApplication = std::nullopt});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::DropPositionEvent{
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{400}},
            .windowId = first.GetId(),
            .position = {7.0F, 8.0F},
            .sourceApplication = std::optional<std::string>{"source-app"}});
    ExpectPolledEvent(
        runtime.PollEvent(),
        pond::platform::DropCompleteEvent{
            .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{500}},
            .windowId = std::nullopt,
            .position = {9.0F, 10.0F},
            .sourceApplication = std::nullopt});
    ExpectPolledEvent(runtime.PollEvent(), pond::platform::QuitRequestedEvent{
                                               .timestamp = pond::platform::PlatformTimestamp{
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
        EXPECT_NE(result.GetError().GetMessage().find("window 1"), std::string_view::npos);
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
    EXPECT_EQ(m_fake.activeCursor, nullptr);
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
        .filters = {{.name = "Molecules", .pattern = "sdf;mol"}, {.name = "All", .pattern = "*"}},
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
    EXPECT_EQ(m_fake.dialogLaunches[0].parentWindow, &m_fake.windows.front());
    ASSERT_EQ(m_fake.dialogLaunches[0].filters.size(), 2U);
    EXPECT_EQ(m_fake.dialogLaunches[0].filters[0].name, "Molecules");
    EXPECT_EQ(m_fake.dialogLaunches[0].filters[0].pattern, "sdf;mol");
    EXPECT_EQ(m_fake.dialogLaunches[0].defaultLocation, "C:/tmp/input.sdf");
    EXPECT_TRUE(m_fake.dialogLaunches[0].allowMultipleSelection);

    EXPECT_EQ(m_fake.dialogLaunches[1].kind, BackendDialogKind::SaveFile);
    EXPECT_FALSE(m_fake.dialogLaunches[1].parentWindow != nullptr);
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

    CompleteDialogCancellation(m_fake.dialogLaunches[1]);
    const std::vector<std::string> selectedPaths{"C:/tmp/a.sdf", "C:/tmp/b.mol"};
    CompleteDialogSelection(m_fake.dialogLaunches[0], selectedPaths, 0);
    CompleteDialogFailure(m_fake.dialogLaunches[2], "synthetic dialog failure");

    std::optional<pond::platform::PlatformEvent> event = runtime.PollEvent();
    ASSERT_TRUE(event.has_value());
    const auto* completed = std::get_if<pond::platform::DialogCompletedEvent>(&*event);
    ASSERT_NE(completed, nullptr);
    EXPECT_EQ(completed->requestId, secondId.GetValue());
    EXPECT_TRUE(std::holds_alternative<pond::platform::DialogCancellation>(completed->outcome));

    event = runtime.PollEvent();
    ASSERT_TRUE(event.has_value());
    completed = std::get_if<pond::platform::DialogCompletedEvent>(&*event);
    ASSERT_NE(completed, nullptr);
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
    EXPECT_EQ(completed->requestId, thirdId.GetValue());
    const auto& failure = std::get<pond::platform::DialogFailure>(completed->outcome);
    EXPECT_EQ(failure.error.GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(failure.error.GetMessage().find("synthetic dialog failure"), std::string_view::npos);

    EXPECT_FALSE(runtime.PollEvent().has_value());
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
    EXPECT_EQ(m_fake.clipboardFreeCalls, 1);
    EXPECT_EQ(m_fake.clipboardReadBuffer, "freed clipboard text");

    m_fake.clipboardText.clear();
    m_fake.clipboardGetError.clear();
    auto emptyText = runtime.GetClipboardText();
    ASSERT_TRUE(emptyText.HasValue());
    EXPECT_TRUE(emptyText.GetValue().empty());
    EXPECT_EQ(m_fake.clipboardFreeCalls, 2);

    m_fake.clipboardGetError = "synthetic empty clipboard failure";
    auto emptyFailure = runtime.GetClipboardText();
    ASSERT_FALSE(emptyFailure.HasValue());
    EXPECT_EQ(emptyFailure.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(emptyFailure.GetError().GetMessage().find("SDL_GetClipboardText"),
              std::string_view::npos);
    EXPECT_NE(emptyFailure.GetError().GetMessage().find("synthetic empty clipboard failure"),
              std::string_view::npos);
    EXPECT_EQ(m_fake.clipboardFreeCalls, 3);

    m_fake.clipboardGetReturnsNull = true;
    m_fake.clipboardGetError = "synthetic null clipboard failure";
    auto nullFailure = runtime.GetClipboardText();
    ASSERT_FALSE(nullFailure.HasValue());
    EXPECT_EQ(nullFailure.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(nullFailure.GetError().GetMessage().find("synthetic null clipboard failure"),
              std::string_view::npos);
    EXPECT_EQ(m_fake.clipboardFreeCalls, 3);
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
                                                    .timestamp = pond::platform::PlatformTimestamp{
                                                        std::chrono::nanoseconds{100}}});
    EXPECT_EQ(m_fake.pollEventCalls, 1);
}
} // namespace
