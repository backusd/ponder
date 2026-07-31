#include "MockRuntime.hpp"

#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/core/Log.hpp>
#include <ponder/core/String.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "SdlRuntime.hpp"
#include "WindowImpl.hpp"

namespace ponder::platform::detail
{
namespace
{
thread_local MockRuntimeControl* activeMockRuntimeControl{};

constexpr ponder::core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr ponder::core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr ponder::core::ErrorCode kNotFoundCode = ToErrorCode(PlatformErrorCode::NotFound);
constexpr ponder::core::ErrorCode kUnsupportedCode = ToErrorCode(PlatformErrorCode::Unsupported);
constexpr std::string_view kLogCategory{"platform"};

template <typename ResultType, typename Operation>
[[nodiscard]] ResultType InvokeDialogBoundary(MockRuntimeControl& control, std::string_view operation, Operation&& action) noexcept
{
    try
    {
        if (control.dialogOperationException != nullptr)
        {
            std::rethrow_exception(control.dialogOperationException);
        }
        return std::invoke(std::forward<Operation>(action));
    }
    catch (const ponder::core::Exception& exception)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "Mock dialog {} caught a core exception: {}", operation, exception.GetMessage());
        return ResultType::FromError(ponder::core::Error{kBackendFailureCode,
                                                         std::format("Mock dialog {} caught a core exception: {}", operation, exception.GetMessage()),
                                                         exception.GetStackTrace(), exception.GetLocation()});
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "Mock dialog {} caught a standard exception: {}", operation, exception.what());
        return ResultType::FromError(
            ponder::core::Error{kBackendFailureCode, std::format("Mock dialog {} caught a standard exception: {}", operation, exception.what())});
    }
    catch (...)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "Mock dialog {} caught an unknown exception", operation);
        return ResultType::FromError(ponder::core::Error{kBackendFailureCode, std::format("Mock dialog {} caught an unknown exception", operation)});
    }
}

[[nodiscard]] MockRuntimeControl& LoadActiveControl()
{
    PONDER_VERIFY(activeMockRuntimeControl != nullptr, "Mock platform runtime creation requires a scoped control binding");
    return *activeMockRuntimeControl;
}

[[nodiscard]] constexpr bool IsValid(SystemCursorShape shape) noexcept
{
    switch (shape)
    {
    case SystemCursorShape::Default:
    case SystemCursorShape::TextInput:
    case SystemCursorShape::Move:
    case SystemCursorShape::ResizeNorthSouth:
    case SystemCursorShape::ResizeEastWest:
    case SystemCursorShape::ResizeNortheastSouthwest:
    case SystemCursorShape::ResizeNorthwestSoutheast:
    case SystemCursorShape::Pointer:
    case SystemCursorShape::Wait:
    case SystemCursorShape::Progress:
    case SystemCursorShape::NotAllowed:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsValid(hints::EventLoggingLevel value) noexcept
{
    switch (value)
    {
    case hints::EventLoggingLevel::Disabled:
    case hints::EventLoggingLevel::Common:
    case hints::EventLoggingLevel::Verbose:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsValid(hints::ImeUiCapabilities value) noexcept
{
    switch (value)
    {
    case hints::ImeUiCapabilities::None:
    case hints::ImeUiCapabilities::Composition:
    case hints::ImeUiCapabilities::Candidates:
    case hints::ImeUiCapabilities::CompositionAndCandidates:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsValid(hints::FullscreenFocusLossBehavior value) noexcept
{
    switch (value)
    {
    case hints::FullscreenFocusLossBehavior::Automatic:
    case hints::FullscreenFocusLossBehavior::Minimize:
    case hints::FullscreenFocusLossBehavior::KeepFullscreen:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsValidFilterPatternCharacter(char character) noexcept
{
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') ||
           character == '-' || character == '_' || character == '.';
}

[[nodiscard]] bool IsValidFilterPattern(std::string_view pattern) noexcept
{
    if (pattern == "*")
    {
        return true;
    }

    bool segmentHasCharacters{};
    for (const char character : pattern)
    {
        if (character == ';')
        {
            if (!segmentHasCharacters)
            {
                return false;
            }
            segmentHasCharacters = false;
            continue;
        }
        if (!IsValidFilterPatternCharacter(character))
        {
            return false;
        }
        segmentHasCharacters = true;
    }
    return segmentHasCharacters;
}

void ValidateDefaultLocation(const std::optional<std::filesystem::path>& location)
{
    if (!location.has_value())
    {
        return;
    }

    const std::u8string utf8 = location->generic_u8string();
    const std::string text{reinterpret_cast<const char*>(utf8.data()), utf8.size()};
    if (text.empty() || !ponder::core::IsValidUtf8WithoutEmbeddedNull(text))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument,
                                 "Dialog default location must be absent or non-empty UTF-8 without embedded nulls.");
    }
}

void ValidateFilters(std::span<const dialogs::DialogFileFilter> filters)
{
    if (filters.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Dialog filter count exceeds the backend representation range.");
    }

    for (std::size_t index = 0; index < filters.size(); ++index)
    {
        const dialogs::DialogFileFilter& filter = filters[index];
        if (filter.name.empty() || !ponder::core::IsValidUtf8WithoutEmbeddedNull(filter.name))
        {
            throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Dialog filter {} name must be non-empty UTF-8 without embedded nulls.",
                                     index);
        }
        if (filter.pattern.empty() || !ponder::core::IsValidUtf8WithoutEmbeddedNull(filter.pattern) || !IsValidFilterPattern(filter.pattern))
        {
            throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument,
                                     "Dialog filter {} pattern must be '*', or a semicolon-separated list of ASCII file extensions.", index);
        }
    }
}

void ValidateDialogDesc(std::optional<WindowId> parentWindowId, const std::optional<std::filesystem::path>& defaultLocation,
                        std::span<const dialogs::DialogFileFilter> filters)
{
    if (parentWindowId.has_value() && !parentWindowId->IsValid())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Dialog parent window ID must be absent or valid.");
    }
    ValidateDefaultLocation(defaultLocation);
    ValidateFilters(filters);
}

[[nodiscard]] ponder::core::Error MakeDisplayNotFoundError(DisplayId id)
{
    return ponder::core::Error{kNotFoundCode, std::format("Display {} is not connected.", id)};
}

void ValidateDisplayInfo(const DisplayInfo& display)
{
    if (!display.id.IsValid())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Mock display backend returned an invalid display ID.");
    }
    if (display.refreshRateHertz.has_value() && (!std::isfinite(*display.refreshRateHertz) || *display.refreshRateHertz <= 0.0F))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Mock display backend returned an invalid refresh rate for {}.", display.id);
    }
    if (!std::isfinite(display.contentScale) || display.contentScale <= 0.0F)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Mock display backend returned an invalid content scale for {}.", display.id);
    }
}

[[nodiscard]] BackendWindowPixelSize CalculatePixelSize(BackendWindowLogicalSize logicalSize, float density, bool highPixelDensity) noexcept
{
    if (!highPixelDensity || !std::isfinite(density) || density <= 0.0F)
    {
        return BackendWindowPixelSize{logicalSize.width, logicalSize.height};
    }

    const double width = std::round(static_cast<double>(logicalSize.width) * density);
    const double height = std::round(static_cast<double>(logicalSize.height) * density);
    if (width < 0.0 || height < 0.0 || width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max())
    {
        return BackendWindowPixelSize{logicalSize.width, logicalSize.height};
    }
    return BackendWindowPixelSize{static_cast<int>(width), static_cast<int>(height)};
}
} // namespace

bool IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility compatibility) noexcept
{
    switch (compatibility)
    {
    case WindowGraphicsCompatibility::Default:
        return true;
    case WindowGraphicsCompatibility::Vulkan:
#if defined(_WIN32) || defined(__linux__)
        return true;
#else
        return false;
#endif
    case WindowGraphicsCompatibility::Metal:
#if defined(__APPLE__)
        return true;
#else
        return false;
#endif
    }
    return false;
}

bool IsReservedSdlWindowPosition(std::int32_t value) noexcept
{
    constexpr std::uint32_t kCategoryMask{0xFFFF0000U};
    constexpr std::uint32_t kUndefinedMask{0x1FFF0000U};
    constexpr std::uint32_t kCenteredMask{0x2FFF0000U};
    const std::uint32_t encoded = static_cast<std::uint32_t>(value);
    return (encoded & kCategoryMask) == kUndefinedMask || (encoded & kCategoryMask) == kCenteredMask;
}

MockRuntimeControl::MockRuntimeControl() :
    windowDisplayId(DisplayId{1}),
    displays{DisplayInfo{.id = DisplayId{1},
                         .name = "Mock Display",
                         .bounds = ScreenRectangle{{0, 0}, {1920, 1080}},
                         .usableBounds = ScreenRectangle{{0, 0}, {1920, 1040}},
                         .refreshRateHertz = 60.0F,
                         .orientation = DisplayOrientation::Landscape,
                         .contentScale = 1.0F}}
{
}

ScopedMockRuntimeBinding::ScopedMockRuntimeBinding(MockRuntimeControl& control) noexcept :
    m_control(std::addressof(control)),
    m_previous(activeMockRuntimeControl)
{
    activeMockRuntimeControl = m_control;
}

ScopedMockRuntimeBinding::~ScopedMockRuntimeBinding() noexcept
{
    PONDER_VERIFY(activeMockRuntimeControl == m_control, "Mock platform runtime bindings must unwind in stack order");
    activeMockRuntimeControl = m_previous;
}

MockWindowBackend::MockWindowBackend(MockRuntimeControl& control) noexcept :
    m_control(control)
{
}

MockWindowBackend::~MockWindowBackend() noexcept
{
    PONDER_VERIFY(m_windows.empty(), "Mock window backend destroyed with {} live windows", m_windows.size());
}

BackendWindowHandle MockWindowBackend::Create(const BackendWindowCreateDesc& desc)
{
    ++m_control.windowCreationCount;
    if (m_control.failWindowCreation)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Mock window backend rejected window creation.");
    }

    PONDER_VERIFY(m_nextHandle != 0, "Mock backend window handle space is exhausted");
    PONDER_VERIFY(m_nextBackendId != 0, "Mock backend window ID space is exhausted");

    const BackendWindowHandle handle{m_nextHandle++};
    MockWindowState state{.backendId = m_nextBackendId++,
                          .title = std::string{desc.title},
                          .position = {},
                          .logicalSize = desc.logicalSize,
                          .pixelSize = CalculatePixelSize(desc.logicalSize, m_control.windowPixelDensity, desc.highPixelDensity),
                          .minimumSize = std::nullopt,
                          .properties = BackendWindowProperties{.hidden = true, .resizable = desc.resizable},
                          .graphicsCompatibility = desc.graphicsCompatibility,
                          .textInputArea = std::nullopt,
                          .highPixelDensity = desc.highPixelDensity};
    const auto [iterator, inserted] = m_windows.emplace(handle.GetValue(), std::move(state));
    static_cast<void>(iterator);
    PONDER_VERIFY(inserted, "Mock backend window handle {} is already registered", handle);
    ++m_control.liveWindowCount;
    return handle;
}

void MockWindowBackend::Destroy(BackendWindowHandle window) noexcept
{
    const std::size_t removed = m_windows.erase(window.GetValue());
    PONDER_VERIFY(removed == 1, "Mock backend window {} is not registered", window);
    PONDER_VERIFY(m_control.liveWindowCount > 0, "Mock live-window count underflow");
    --m_control.liveWindowCount;
    ++m_control.windowDestructionCount;
}

std::uint32_t MockWindowBackend::GetId(BackendWindowHandle window)
{
    return Get(window).backendId;
}

std::string MockWindowBackend::GetTitle(BackendWindowHandle window)
{
    return Get(window).title;
}

void MockWindowBackend::SetTitle(BackendWindowHandle window, std::string_view title)
{
    Get(window).title = title;
}

BackendWindowPosition MockWindowBackend::GetPosition(BackendWindowHandle window)
{
    return Get(window).position;
}

void MockWindowBackend::SetPosition(BackendWindowHandle window, BackendWindowPosition position)
{
    Get(window).position = position;
}

BackendWindowLogicalSize MockWindowBackend::GetSize(BackendWindowHandle window)
{
    return Get(window).logicalSize;
}

BackendWindowPixelSize MockWindowBackend::GetSizeInPixels(BackendWindowHandle window)
{
    return Get(window).pixelSize;
}

void MockWindowBackend::SetSize(BackendWindowHandle window, BackendWindowLogicalSize size)
{
    MockWindowState& state = Get(window);
    state.logicalSize = size;
    state.pixelSize = CalculatePixelSize(size, m_control.windowPixelDensity, state.highPixelDensity);
}

void MockWindowBackend::SetMinimumSize(BackendWindowHandle window, BackendWindowLogicalSize size)
{
    Get(window).minimumSize = size;
}

void MockWindowBackend::Show(BackendWindowHandle window)
{
    Get(window).properties.hidden = false;
}

void MockWindowBackend::Hide(BackendWindowHandle window)
{
    Get(window).properties.hidden = true;
}

BackendWindowProperties MockWindowBackend::GetProperties(BackendWindowHandle window)
{
    return Get(window).properties;
}

void MockWindowBackend::SetFullscreenModeToDesktop(BackendWindowHandle window)
{
    static_cast<void>(Get(window));
}

void MockWindowBackend::SetFullscreen(BackendWindowHandle window, bool fullscreen)
{
    Get(window).properties.desktopFullscreen = fullscreen;
}

void MockWindowBackend::SetBordered(BackendWindowHandle window, bool bordered)
{
    Get(window).properties.borderless = !bordered;
}

void MockWindowBackend::SetResizable(BackendWindowHandle window, bool resizable)
{
    Get(window).properties.resizable = resizable;
}

void MockWindowBackend::SetAlwaysOnTop(BackendWindowHandle window, bool alwaysOnTop)
{
    Get(window).properties.alwaysOnTop = alwaysOnTop;
}

void MockWindowBackend::Minimize(BackendWindowHandle window)
{
    BackendWindowProperties& properties = Get(window).properties;
    properties.minimized = true;
    properties.maximized = false;
}

void MockWindowBackend::Maximize(BackendWindowHandle window)
{
    BackendWindowProperties& properties = Get(window).properties;
    properties.minimized = false;
    properties.maximized = true;
}

void MockWindowBackend::Restore(BackendWindowHandle window)
{
    BackendWindowProperties& properties = Get(window).properties;
    properties.minimized = false;
    properties.maximized = false;
}

void MockWindowBackend::StartTextInput(BackendWindowHandle window)
{
    Get(window).textInputActive = true;
}

void MockWindowBackend::StopTextInput(BackendWindowHandle window)
{
    Get(window).textInputActive = false;
}

bool MockWindowBackend::IsTextInputActive(BackendWindowHandle window) noexcept
{
    return Get(window).textInputActive;
}

void MockWindowBackend::ClearTextComposition(BackendWindowHandle window)
{
    static_cast<void>(Get(window));
}

void MockWindowBackend::SetTextInputArea(BackendWindowHandle window, std::optional<BackendTextInputArea> area)
{
    Get(window).textInputArea = area;
}

void MockWindowBackend::SetMouseGrab(BackendWindowHandle window, bool grabbed)
{
    Get(window).mouseGrabbed = grabbed;
}

bool MockWindowBackend::IsMouseGrabbed(BackendWindowHandle window) noexcept
{
    return Get(window).mouseGrabbed;
}

void MockWindowBackend::SetRelativeMouseMode(BackendWindowHandle window, bool enabled)
{
    Get(window).relativeMouseMode = enabled;
}

bool MockWindowBackend::IsRelativeMouseModeEnabled(BackendWindowHandle window) noexcept
{
    return Get(window).relativeMouseMode;
}

ponder::core::Result<NativeWindowHandle> MockWindowBackend::GetNativeHandle(BackendWindowHandle window)
{
    static_cast<void>(Get(window));
    return ponder::core::Result<NativeWindowHandle>::FromError(
        ponder::core::Error{kUnsupportedCode, "Native window handles are unsupported by the mock runtime."});
}

MockWindowState& MockWindowBackend::Get(BackendWindowHandle window)
{
    const auto iterator = m_windows.find(window.GetValue());
    PONDER_VERIFY(iterator != m_windows.end(), "Mock backend window {} is not registered", window);
    return iterator->second;
}

const MockWindowState& MockWindowBackend::Get(BackendWindowHandle window) const
{
    const auto iterator = m_windows.find(window.GetValue());
    PONDER_VERIFY(iterator != m_windows.end(), "Mock backend window {} is not registered", window);
    return iterator->second;
}

MockRuntime::MockRuntime() :
    m_windowRegistry(m_ownerThread),
    m_control(std::addressof(LoadActiveControl())),
    m_windowBackend(*m_control)
{
    ++m_control->constructionCount;
}

MockRuntime::~MockRuntime() noexcept
{
    VerifyOwnerThreadForDestruction("Runtime");
    PONDER_VERIFY(m_registry.IsEmpty(), "Cannot destroy Runtime with {} children", m_registry.GetChildCount());
    PONDER_VERIFY(m_windowRegistry.IsEmpty(), "Cannot destroy Runtime with {} registered windows", m_windowRegistry.GetWindowCount());
    PONDER_VERIFY(m_dialogRequests.empty(), "Cannot destroy Runtime with {} outstanding dialog requests", m_dialogRequests.size());
    PONDER_VERIFY(m_dialogParentLeases.empty() && m_dialogCompletions.empty() && m_dialogCompletionTimestamps.empty() &&
                      m_completedDialogRequests.empty(),
                  "Mock Runtime dialog registries are inconsistent during destruction");

    if (m_initialized)
    {
        PONDER_VERIFY(m_control->runtimeActive, "Mock runtime control is unexpectedly inactive");
        m_control->runtimeActive = false;
        m_initialized = false;
    }
    ++m_control->destructionCount;
}

void MockRuntime::Initialize(std::string_view applicationName, std::optional<std::string_view> applicationVersion,
                             std::optional<std::string_view> applicationIdentifier)
{
    m_ownerThread.Verify("runtime initialization");
    PONDER_VERIFY(!m_initialized, "Cannot initialize MockRuntime more than once");
    ++m_control->initializationAttemptCount;
    m_control->lastApplicationName = applicationName;
    m_control->lastApplicationVersion = applicationVersion.has_value() ? std::optional<std::string>{*applicationVersion} : std::nullopt;
    m_control->lastApplicationIdentifier = applicationIdentifier.has_value() ? std::optional<std::string>{*applicationIdentifier} : std::nullopt;
    if (m_control->failInitialization)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Mock runtime initialization failed.");
    }

    PONDER_VERIFY(!m_control->runtimeActive, "Mock runtime control already has an active runtime");
    m_initialized = true;
    m_control->runtimeActive = true;
    ++m_control->successfulInitializationCount;
}

#define PONDER_DEFINE_SIMPLE_HINT(Type, BeforeInitialization, PushOnce)                                                                              \
    template <>                                                                                                                                      \
    void MockRuntime::HintPush<hints::Type>(const hints::Type& hint)                                                                                 \
    {                                                                                                                                                \
        HintPushValue(hint, BeforeInitialization, PushOnce);                                                                                         \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void MockRuntime::HintPop<hints::Type>()                                                                                                         \
    {                                                                                                                                                \
        HintPopValue<hints::Type>(BeforeInitialization);                                                                                             \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void MockRuntime::HintClear<hints::Type>()                                                                                                       \
    {                                                                                                                                                \
        HintClearValue<hints::Type>(BeforeInitialization);                                                                                           \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    std::optional<hints::Type> MockRuntime::HintGet<hints::Type>() const                                                                             \
    {                                                                                                                                                \
        return HintGetValue<hints::Type>();                                                                                                          \
    }

PONDER_DEFINE_SIMPLE_HINT(AllowAltTabWhileGrabbed, false, false)
PONDER_DEFINE_SIMPLE_HINT(PollSentinel, false, false)
PONDER_DEFINE_SIMPLE_HINT(QuitOnLastWindowClose, false, false)
PONDER_DEFINE_SIMPLE_HINT(VideoAllowScreensaver, true, true)
PONDER_DEFINE_SIMPLE_HINT(VideoDoubleBuffer, true, false)
PONDER_DEFINE_SIMPLE_HINT(VideoForceEgl, true, false)
PONDER_DEFINE_SIMPLE_HINT(VideoSyncWindowOperations, false, false)
PONDER_DEFINE_SIMPLE_HINT(WindowActivateWhenRaised, false, false)
PONDER_DEFINE_SIMPLE_HINT(WindowActivateWhenShown, false, false)
PONDER_DEFINE_SIMPLE_HINT(WindowAllowTopmost, false, false)
PONDER_DEFINE_SIMPLE_HINT(WindowFrameUsableWhileCursorHidden, false, false)
PONDER_DEFINE_SIMPLE_HINT(MouseAutoCapture, false, false)
PONDER_DEFINE_SIMPLE_HINT(MouseDpiScaleCursors, true, false)
PONDER_DEFINE_SIMPLE_HINT(MouseEmulateWarpWithRelative, false, false)
PONDER_DEFINE_SIMPLE_HINT(MouseFocusClickThrough, false, false)
PONDER_DEFINE_SIMPLE_HINT(MouseRelativeCursorVisible, false, false)
PONDER_DEFINE_SIMPLE_HINT(MouseRelativeModeCenter, false, false)
PONDER_DEFINE_SIMPLE_HINT(MouseRelativeSystemScale, false, false)
PONDER_DEFINE_SIMPLE_HINT(MouseRelativeWarpMotion, false, false)
PONDER_DEFINE_SIMPLE_HINT(MouseTouchEvents, false, false)
PONDER_DEFINE_SIMPLE_HINT(PenMouseEvents, false, false)
PONDER_DEFINE_SIMPLE_HINT(PenTouchEvents, false, false)
PONDER_DEFINE_SIMPLE_HINT(TouchMouseEvents, false, false)
PONDER_DEFINE_SIMPLE_HINT(TrackpadIsTouchOnly, true, false)

#if defined(__APPLE__)
PONDER_DEFINE_SIMPLE_HINT(MacCtrlClickEmulatesRightClick, false, false)
PONDER_DEFINE_SIMPLE_HINT(MacScrollMomentum, true, false)
PONDER_DEFINE_SIMPLE_HINT(VideoMacFullscreenSpaces, true, false)
#endif

#if defined(_WIN32)
PONDER_DEFINE_SIMPLE_HINT(WindowsCloseOnAltF4, false, false)
PONDER_DEFINE_SIMPLE_HINT(WindowsEnableMenuMnemonics, false, false)
PONDER_DEFINE_SIMPLE_HINT(WindowsGameInput, true, false)
PONDER_DEFINE_SIMPLE_HINT(WindowsRawKeyboard, false, false)
PONDER_DEFINE_SIMPLE_HINT(WindowsRawKeyboardExcludeHotkeys, false, false)
PONDER_DEFINE_SIMPLE_HINT(WindowsRawKeyboardInputSink, false, false)
PONDER_DEFINE_SIMPLE_HINT(WindowsRawMouseNoLegacy, false, false)
#endif

#if defined(__linux__)
PONDER_DEFINE_SIMPLE_HINT(VideoWaylandAllowLibdecor, true, false)
PONDER_DEFINE_SIMPLE_HINT(VideoWaylandModeEmulation, true, false)
PONDER_DEFINE_SIMPLE_HINT(VideoWaylandPreferLibdecor, true, false)
PONDER_DEFINE_SIMPLE_HINT(VideoWaylandScaleToDisplay, true, false)
PONDER_DEFINE_SIMPLE_HINT(VideoX11NetWmBypassCompositor, true, false)
PONDER_DEFINE_SIMPLE_HINT(VideoX11Xrandr, true, false)
#endif

#undef PONDER_DEFINE_SIMPLE_HINT

#define PONDER_DEFINE_VALIDATED_ENUM_HINT(Type, BeforeInitialization, ValidationMessage)                                                             \
    template <>                                                                                                                                      \
    void MockRuntime::HintPush<hints::Type>(const hints::Type& hint)                                                                                 \
    {                                                                                                                                                \
        if (!IsValid(hint.value))                                                                                                                    \
        {                                                                                                                                            \
            throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, ValidationMessage);                                                         \
        }                                                                                                                                            \
        HintPushValue(hint, BeforeInitialization);                                                                                                   \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void MockRuntime::HintPop<hints::Type>()                                                                                                         \
    {                                                                                                                                                \
        HintPopValue<hints::Type>(BeforeInitialization);                                                                                             \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void MockRuntime::HintClear<hints::Type>()                                                                                                       \
    {                                                                                                                                                \
        HintClearValue<hints::Type>(BeforeInitialization);                                                                                           \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    std::optional<hints::Type> MockRuntime::HintGet<hints::Type>() const                                                                             \
    {                                                                                                                                                \
        return HintGetValue<hints::Type>();                                                                                                          \
    }

PONDER_DEFINE_VALIDATED_ENUM_HINT(EventLogging, false, "Platform event logging level is invalid.")
PONDER_DEFINE_VALIDATED_ENUM_HINT(ImeImplementedUi, true, "Platform IME UI capabilities value is invalid.")
PONDER_DEFINE_VALIDATED_ENUM_HINT(VideoMinimizeOnFocusLoss, false, "Platform fullscreen focus-loss behavior is invalid.")
PONDER_DEFINE_VALIDATED_ENUM_HINT(MouseDefaultSystemCursor, true, "Platform hint system cursor value is invalid.")

#undef PONDER_DEFINE_VALIDATED_ENUM_HINT

#define PONDER_DEFINE_STRING_HINT(Type, BeforeInitialization)                                                                                        \
    template <>                                                                                                                                      \
    void MockRuntime::HintPush<hints::Type>(const hints::Type& hint)                                                                                 \
    {                                                                                                                                                \
        if (hint.value.empty() || hint.value.find('\0') != std::string::npos)                                                                        \
        {                                                                                                                                            \
            throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument,                                                                             \
                                     "Platform hint string value must be non-empty and contain no embedded nulls.");                                 \
        }                                                                                                                                            \
        HintPushValue(hint, BeforeInitialization);                                                                                                   \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void MockRuntime::HintPop<hints::Type>()                                                                                                         \
    {                                                                                                                                                \
        HintPopValue<hints::Type>(BeforeInitialization);                                                                                             \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void MockRuntime::HintClear<hints::Type>()                                                                                                       \
    {                                                                                                                                                \
        HintClearValue<hints::Type>(BeforeInitialization);                                                                                           \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    std::optional<hints::Type> MockRuntime::HintGet<hints::Type>() const                                                                             \
    {                                                                                                                                                \
        return HintGetValue<hints::Type>();                                                                                                          \
    }

PONDER_DEFINE_STRING_HINT(VideoDriver, true)
#if defined(__linux__)
PONDER_DEFINE_STRING_HINT(VideoDisplayPriority, true)
#endif

#undef PONDER_DEFINE_STRING_HINT

template <>
void MockRuntime::HintPush<hints::MouseDoubleClickRadius>(const hints::MouseDoubleClickRadius& hint)
{
    if (hint.value > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform hint integer value exceeds the supported range.");
    }
    HintPushValue(hint, false);
}

template <>
void MockRuntime::HintPop<hints::MouseDoubleClickRadius>()
{
    HintPopValue<hints::MouseDoubleClickRadius>(false);
}

template <>
void MockRuntime::HintClear<hints::MouseDoubleClickRadius>()
{
    HintClearValue<hints::MouseDoubleClickRadius>(false);
}

template <>
std::optional<hints::MouseDoubleClickRadius> MockRuntime::HintGet<hints::MouseDoubleClickRadius>() const
{
    return HintGetValue<hints::MouseDoubleClickRadius>();
}

template <>
void MockRuntime::HintPush<hints::MouseDoubleClickTime>(const hints::MouseDoubleClickTime& hint)
{
    if (hint.value.count() < 0 || hint.value.count() > std::numeric_limits<int>::max())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform hint duration must fit in the non-negative millisecond range.");
    }
    HintPushValue(hint, false);
}

template <>
void MockRuntime::HintPop<hints::MouseDoubleClickTime>()
{
    HintPopValue<hints::MouseDoubleClickTime>(false);
}

template <>
void MockRuntime::HintClear<hints::MouseDoubleClickTime>()
{
    HintClearValue<hints::MouseDoubleClickTime>(false);
}

template <>
std::optional<hints::MouseDoubleClickTime> MockRuntime::HintGet<hints::MouseDoubleClickTime>() const
{
    return HintGetValue<hints::MouseDoubleClickTime>();
}

#define PONDER_DEFINE_FLOAT_HINT(Type)                                                                                                               \
    template <>                                                                                                                                      \
    void MockRuntime::HintPush<hints::Type>(const hints::Type& hint)                                                                                 \
    {                                                                                                                                                \
        if (!std::isfinite(hint.value))                                                                                                              \
        {                                                                                                                                            \
            throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform hint floating-point value must be finite.");                      \
        }                                                                                                                                            \
        HintPushValue(hint, false);                                                                                                                  \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void MockRuntime::HintPop<hints::Type>()                                                                                                         \
    {                                                                                                                                                \
        HintPopValue<hints::Type>(false);                                                                                                            \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void MockRuntime::HintClear<hints::Type>()                                                                                                       \
    {                                                                                                                                                \
        HintClearValue<hints::Type>(false);                                                                                                          \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    std::optional<hints::Type> MockRuntime::HintGet<hints::Type>() const                                                                             \
    {                                                                                                                                                \
        return HintGetValue<hints::Type>();                                                                                                          \
    }

PONDER_DEFINE_FLOAT_HINT(MouseNormalSpeedScale)
PONDER_DEFINE_FLOAT_HINT(MouseRelativeSpeedScale)

#undef PONDER_DEFINE_FLOAT_HINT

ponder::core::Result<std::string> MockRuntime::ClipboardGetText() const
{
    VerifyOwnerThread("clipboard text query");
    if (m_control->clipboardGetError.has_value())
    {
        return ponder::core::Result<std::string>::FromError(*m_control->clipboardGetError);
    }
    if (!ponder::core::IsValidUtf8WithoutEmbeddedNull(m_control->clipboardText))
    {
        return ponder::core::Result<std::string>::FromError(
            ponder::core::Error{kBackendFailureCode, "Mock clipboard returned text that is not null-free UTF-8."});
    }
    return m_control->clipboardText;
}

ponder::core::VoidResult MockRuntime::ClipboardSetText(std::string_view text)
{
    VerifyOwnerThread("clipboard text update");
    if (!ponder::core::IsValidUtf8WithoutEmbeddedNull(text))
    {
        return ponder::core::VoidResult::FromError(ponder::core::Error{kInvalidArgumentCode, "Clipboard text must be null-free UTF-8."});
    }
    if (m_control->clipboardSetError.has_value())
    {
        return ponder::core::VoidResult::FromError(*m_control->clipboardSetError);
    }
    m_control->clipboardText = text;
    return ponder::core::VoidResult::Success();
}

ponder::core::Result<dialogs::DialogRequestId> MockRuntime::DialogShowOpenFile(const dialogs::OpenFileDialogDesc& desc) noexcept
{
    return InvokeDialogBoundary<ponder::core::Result<dialogs::DialogRequestId>>(
        *m_control, "open-file submission",
        [this, &desc]()
        {
            DialogValidateAccess("dialog request");
            ValidateDialogDesc(desc.parentWindowId, desc.defaultLocation, desc.filters);
            return DialogShow(dialogs::DialogKind::OpenFile, desc.parentWindowId, desc.filters.size(), desc.allowMultipleSelection);
        });
}

ponder::core::Result<dialogs::DialogRequestId> MockRuntime::DialogShowSaveFile(const dialogs::SaveFileDialogDesc& desc) noexcept
{
    return InvokeDialogBoundary<ponder::core::Result<dialogs::DialogRequestId>>(
        *m_control, "save-file submission",
        [this, &desc]()
        {
            DialogValidateAccess("dialog request");
            ValidateDialogDesc(desc.parentWindowId, desc.defaultLocation, desc.filters);
            return DialogShow(dialogs::DialogKind::SaveFile, desc.parentWindowId, desc.filters.size(), false);
        });
}

ponder::core::Result<dialogs::DialogRequestId> MockRuntime::DialogShowOpenFolder(const dialogs::OpenFolderDialogDesc& desc) noexcept
{
    return InvokeDialogBoundary<ponder::core::Result<dialogs::DialogRequestId>>(
        *m_control, "open-folder submission",
        [this, &desc]()
        {
            DialogValidateAccess("dialog request");
            ValidateDialogDesc(desc.parentWindowId, desc.defaultLocation, {});
            return DialogShow(dialogs::DialogKind::OpenFolder, desc.parentWindowId, 0, desc.allowMultipleSelection);
        });
}

dialogs::DialogRequestId MockRuntime::DialogShow(dialogs::DialogKind kind, std::optional<WindowId> parentWindowId, std::size_t filterCount,
                                                 bool allowMultipleSelection)
{
    if (m_dialogShutdown)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Cannot show a dialog after dialog services shutdown.");
    }
    if (m_nextDialogRequestId == 0)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Platform dialog request ID space is exhausted.");
    }

    std::optional<DialogParentLease> parentLease;
    if (parentWindowId.has_value())
    {
        parentLease.emplace(m_windowRegistry.AcquireDialogLease(*parentWindowId));
    }

    const dialogs::DialogRequestId id{m_nextDialogRequestId++};
    const DialogRequestInfo info{.id = id,
                                 .kind = kind,
                                 .requestedAt = m_control->currentTime,
                                 .parentWindowId = parentWindowId,
                                 .filterCount = filterCount,
                                 .allowMultipleSelection = allowMultipleSelection};
    const auto [requestIterator, requestInserted] = m_dialogRequests.emplace(id, info);
    static_cast<void>(requestIterator);
    PONDER_VERIFY(requestInserted, "Dialog request {} is already registered", id);

    if (parentLease.has_value())
    {
        const auto [leaseIterator, leaseInserted] = m_dialogParentLeases.emplace(id, std::move(*parentLease));
        static_cast<void>(leaseIterator);
        PONDER_VERIFY(leaseInserted, "Dialog request {} already has a parent lease", id);
    }

    if (!m_control->dialogOutcomesOnShow.empty())
    {
        DialogOutcome outcome = std::move(m_control->dialogOutcomesOnShow.front());
        m_control->dialogOutcomesOnShow.pop_front();
        const auto [completionIterator, completionInserted] = m_dialogCompletions.emplace(id, std::move(outcome));
        static_cast<void>(completionIterator);
        PONDER_VERIFY(completionInserted, "Dialog request {} already has a completion", id);
        const auto [timestampIterator, timestampInserted] = m_dialogCompletionTimestamps.emplace(id, m_control->currentTime);
        static_cast<void>(timestampIterator);
        PONDER_VERIFY(timestampInserted, "Dialog request {} already has a completion timestamp", id);
        m_completedDialogRequests.push_back(id);
    }

    return id;
}

std::size_t MockRuntime::DialogGetPendingCount() const noexcept
{
    PONDER_ASSERT(m_ownerThread.IsOwnerThread(), "Dialog pending-state queries must run on the Runtime owner thread.");
    PONDER_ASSERT(m_initialized, "Cannot query pending dialogs before Runtime initialization.");
    return m_dialogRequests.size();
}

bool MockRuntime::DialogHasPending() const noexcept
{
    PONDER_ASSERT(m_ownerThread.IsOwnerThread(), "Dialog pending-state queries must run on the Runtime owner thread.");
    PONDER_ASSERT(m_initialized, "Cannot query pending dialogs before Runtime initialization.");
    return !m_dialogRequests.empty();
}

std::vector<DialogRequestInfo> MockRuntime::DialogGetPending() const noexcept
{
    PONDER_ASSERT(m_ownerThread.IsOwnerThread(), "Dialog pending-state queries must run on the Runtime owner thread.");
    PONDER_ASSERT(m_initialized, "Cannot query pending dialogs before Runtime initialization.");

    std::vector<DialogRequestInfo> pending;
    pending.reserve(m_dialogRequests.size());
    for (const auto& [id, request] : m_dialogRequests)
    {
        static_cast<void>(id);
        pending.push_back(request);
    }
    std::ranges::sort(pending, {}, &DialogRequestInfo::id);
    return pending;
}

std::optional<DialogCompletedEvent> MockRuntime::DialogPollCompletion() noexcept
{
    PONDER_ASSERT(m_ownerThread.IsOwnerThread(), "Dialog completion polling must run on the Runtime owner thread.");
    PONDER_ASSERT(m_initialized, "Cannot poll dialog completions before Runtime initialization.");

    if (m_completedDialogRequests.empty())
    {
        return std::nullopt;
    }

    const dialogs::DialogRequestId id = m_completedDialogRequests.front();
    m_completedDialogRequests.pop_front();
    const auto request = m_dialogRequests.find(id);
    const auto completion = m_dialogCompletions.find(id);
    const auto timestamp = m_dialogCompletionTimestamps.find(id);
    PONDER_VERIFY(request != m_dialogRequests.end(), "Completed dialog request {} is not registered", id);
    PONDER_VERIFY(completion != m_dialogCompletions.end(), "Completed dialog request {} has no outcome", id);
    PONDER_VERIFY(timestamp != m_dialogCompletionTimestamps.end(), "Completed dialog request {} has no timestamp", id);

    DialogCompletedEvent event{.timestamp = timestamp->second, .request = request->second, .outcome = std::move(completion->second)};
    m_dialogParentLeases.erase(id);
    m_dialogCompletionTimestamps.erase(timestamp);
    m_dialogCompletions.erase(completion);
    m_dialogRequests.erase(request);
    return event;
}

std::size_t MockRuntime::DialogGetOutstandingRequestCount() const noexcept
{
    PONDER_ASSERT(m_ownerThread.IsOwnerThread(), "Dialog outstanding-request queries must run on the Runtime owner thread.");
    PONDER_ASSERT(m_initialized, "Cannot query outstanding dialogs before Runtime initialization.");
    return m_dialogRequests.size();
}

ponder::core::VoidResult MockRuntime::DialogShutdown() noexcept
{
    return InvokeDialogBoundary<ponder::core::VoidResult>(
        *m_control, "services shutdown",
        [this]() -> ponder::core::VoidResult
        {
            DialogValidateAccess("dialog services shutdown");
            if (m_dialogShutdown)
            {
                return ponder::core::VoidResult::Success();
            }
            if (!m_dialogRequests.empty())
            {
                return ponder::core::VoidResult::FromError(
                    ponder::core::Error{kInvalidArgumentCode, std::format("Cannot shut down runtime dialog services with {} outstanding requests.",
                                                                          m_dialogRequests.size())});
            }
            PONDER_VERIFY(m_dialogParentLeases.empty() && m_dialogCompletions.empty() && m_dialogCompletionTimestamps.empty() &&
                              m_completedDialogRequests.empty(),
                          "Mock Runtime dialog registries are inconsistent during shutdown");
            m_dialogShutdown = true;
            return ponder::core::VoidResult::Success();
        });
}

void MockRuntime::DialogValidateAccess(std::string_view operation) const
{
    m_ownerThread.Verify(operation);
    VerifyInitialized(operation);
}

ponder::core::Timestamp MockRuntime::TimeNow() const
{
    VerifyOwnerThread("timestamp query");
    return m_control->currentTime;
}

std::optional<PlatformEvent> MockRuntime::EventPoll()
{
    VerifyOwnerThread("event polling");
    std::optional<DialogCompletedEvent> completion = DialogPollCompletion();
    if (completion.has_value())
    {
        return PlatformEvent{std::move(*completion)};
    }
    std::scoped_lock lock{m_eventMutex};
    if (m_control->events.empty())
    {
        return std::nullopt;
    }

    PlatformEvent event = std::move(m_control->events.front());
    m_control->events.pop_front();
    return event;
}

std::optional<PlatformEvent> MockRuntime::EventWait(ponder::core::Duration timeout)
{
    VerifyOwnerThread("event waiting");
    const std::int32_t timeoutMilliseconds = GetEventWaitTimeoutMilliseconds(timeout);

    if (std::optional<PlatformEvent> event = EventPoll(); event.has_value())
    {
        return event;
    }

    std::unique_lock lock{m_eventMutex};
    if (!m_wakePending && m_control->events.empty() && timeoutMilliseconds != 0)
    {
        static_cast<void>(m_eventCondition.wait_for(lock, std::chrono::milliseconds{timeoutMilliseconds},
                                                    [this]
                                                    {
                                                        return m_wakePending || !m_control->events.empty();
                                                    }));
    }

    if (!m_control->events.empty())
    {
        PlatformEvent event = std::move(m_control->events.front());
        m_control->events.pop_front();
        return event;
    }
    m_wakePending = false;
    return std::nullopt;
}

void MockRuntime::EventWake()
{
    {
        std::scoped_lock lock{m_eventMutex};
        if (!m_initialized)
        {
            throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Cannot wake the platform event loop before Runtime initialization.");
        }
        m_wakePending = true;
    }
    m_eventCondition.notify_one();
}

Window MockRuntime::WindowCreate(const WindowDesc& desc)
{
    VerifyOwnerThread("window creation");
    return Window{WindowImpl::Create(*this, desc)};
}

std::vector<DisplayInfo> MockRuntime::DisplayEnumerate()
{
    VerifyOwnerThread("display enumeration");
    for (const DisplayInfo& display : m_control->displays)
    {
        ValidateDisplayInfo(display);
    }
    return m_control->displays;
}

ponder::core::Result<DisplayInfo> MockRuntime::DisplayGetInfo(DisplayId id)
{
    VerifyOwnerThread("display query");
    if (!id.IsValid())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Display ID must be valid.");
    }

    const auto display = std::ranges::find(m_control->displays, id, &DisplayInfo::id);
    if (display == m_control->displays.end())
    {
        return ponder::core::Result<DisplayInfo>::FromError(MakeDisplayNotFoundError(id));
    }
    ValidateDisplayInfo(*display);
    return *display;
}

ponder::core::VoidResult MockRuntime::MouseSetCapture(bool enabled)
{
    VerifyOwnerThread("mouse capture update");
    if (!m_control->globalMouseSupported)
    {
        if (!enabled)
        {
            m_control->mouseCaptured = false;
            return ponder::core::VoidResult::Success();
        }
        return ponder::core::VoidResult::FromError(ponder::core::Error{kUnsupportedCode, "Global mouse capture is unsupported by the mock runtime."});
    }
    if (m_control->mouseCaptureError.has_value())
    {
        return ponder::core::VoidResult::FromError(*m_control->mouseCaptureError);
    }
    m_control->mouseCaptured = enabled;
    return ponder::core::VoidResult::Success();
}

ponder::core::Result<LogicalPoint> MockRuntime::MouseGetGlobalPosition() const
{
    VerifyOwnerThread("global mouse-position query");
    if (!m_control->globalMouseSupported)
    {
        return ponder::core::Result<LogicalPoint>::FromError(
            ponder::core::Error{kUnsupportedCode, "Global mouse position is unsupported by the mock runtime."});
    }
    if (m_control->globalMousePositionError.has_value())
    {
        return ponder::core::Result<LogicalPoint>::FromError(*m_control->globalMousePositionError);
    }
    if (!ponder::platform::IsValid(m_control->globalMousePosition))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Mock global mouse-position backend returned non-finite coordinates.");
    }
    return m_control->globalMousePosition;
}

void MockRuntime::MouseSetSystemCursor(SystemCursorShape shape)
{
    VerifyOwnerThread("system cursor update");
    if (!IsValid(shape))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "System cursor shape is invalid.");
    }
    m_control->selectedCursor = shape;
}

void MockRuntime::MouseShowCursor()
{
    VerifyOwnerThread("cursor visibility update");
    m_control->cursorVisible = true;
}

void MockRuntime::MouseHideCursor()
{
    VerifyOwnerThread("cursor visibility update");
    m_control->cursorVisible = false;
}

bool MockRuntime::MouseIsCursorVisible() const
{
    VerifyOwnerThread("cursor visibility query");
    return m_control->cursorVisible;
}

ponder::core::VoidResult MockRuntime::UriOpenExternal(std::string_view uri)
{
    VerifyOwnerThread("external URI opening");
    if (uri.empty())
    {
        return ponder::core::VoidResult::FromError(ponder::core::Error{kInvalidArgumentCode, "External URI must be non-empty."});
    }

    ponder::core::VoidResult validation = ValidateNullTerminatedUtf8(uri, "External URI");
    if (!validation)
    {
        return validation;
    }
    if (m_control->externalUriError.has_value())
    {
        return ponder::core::VoidResult::FromError(*m_control->externalUriError);
    }
    m_control->openedUris.emplace_back(uri);
    return ponder::core::VoidResult::Success();
}

void MockRuntime::VerifyOwnerThread(std::string_view operation) const
{
    m_ownerThread.Verify(operation);
    VerifyInitialized(operation);
}

void MockRuntime::VerifyOwnerThreadForDestruction(std::string_view object) const noexcept
{
    m_ownerThread.VerifyForDestruction(object);
}

void MockRuntime::VerifyInitialized(std::string_view operation) const
{
    if (!m_initialized)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Cannot perform {} before Runtime initialization.", operation);
    }
}

void MockRuntime::RegisterChild(const void* child)
{
    VerifyOwnerThread("child registration");
    m_registry.RegisterChild(child);
}

void MockRuntime::UnregisterChild(const void* child)
{
    VerifyOwnerThread("child unregistration");
    m_registry.UnregisterChild(child);
}

WindowId MockRuntime::GetNextWindowIdForRegistration() const
{
    VerifyOwnerThread("window registration preparation");
    return m_windowRegistry.GetNextWindowId();
}

void MockRuntime::RegisterWindow(WindowImpl& window, BackendWindowHandle backendWindow, std::uint32_t backendWindowId, WindowId id)
{
    VerifyOwnerThread("window registration");
    m_windowRegistry.Register(window, backendWindow, backendWindowId, id);
    try
    {
        m_registry.RegisterChild(std::addressof(window));
    }
    catch (...)
    {
        m_windowRegistry.RollbackRegistration(window, backendWindowId, id);
        throw;
    }
    window.CommitRegistration();
}

void MockRuntime::BeginWindowDestruction(WindowImpl& window, std::uint32_t backendWindowId, WindowId id)
{
    VerifyOwnerThread("window destruction");
    m_windowRegistry.Unregister(window, backendWindowId, id);
}

void MockRuntime::FinishWindowDestruction(WindowImpl& window)
{
    VerifyOwnerThread("window destruction");
    m_registry.UnregisterChild(std::addressof(window));
}

void MockRuntime::RestoreWindowIdAfterFailedConstruction(WindowId id) noexcept
{
    m_windowRegistry.RestoreWindowIdAfterFailedConstruction(id);
}

ponder::core::Result<DisplayId> MockRuntime::GetDisplayIdForWindow(BackendWindowHandle window, std::string_view windowContext)
{
    VerifyOwnerThread("window display query");
    static_cast<void>(m_windowBackend.GetId(window));
    if (!m_control->windowDisplayId.has_value())
    {
        return ponder::core::Result<DisplayId>::FromError(ponder::core::Error{kNotFoundCode, "The window's display is not connected."});
    }
    if (!m_control->windowDisplayId->IsValid())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Mock display backend returned an invalid display ID for {}.", windowContext);
    }
    const auto display = std::ranges::find(m_control->displays, *m_control->windowDisplayId, &DisplayInfo::id);
    if (display == m_control->displays.end())
    {
        return ponder::core::Result<DisplayId>::FromError(ponder::core::Error{kNotFoundCode, "The window's display is not connected."});
    }
    return *m_control->windowDisplayId;
}

float MockRuntime::GetPixelDensityForWindow(BackendWindowHandle window, std::string_view windowContext) const
{
    VerifyOwnerThread("window pixel density query");
    static_cast<void>(window);
    if (!std::isfinite(m_control->windowPixelDensity) || m_control->windowPixelDensity <= 0.0F)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Mock display backend returned an invalid pixel density for {}.", windowContext);
    }
    return m_control->windowPixelDensity;
}

float MockRuntime::GetDisplayScaleForWindow(BackendWindowHandle window, std::string_view windowContext) const
{
    VerifyOwnerThread("window display scale query");
    static_cast<void>(window);
    if (!std::isfinite(m_control->windowDisplayScale) || m_control->windowDisplayScale <= 0.0F)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Mock display backend returned an invalid display scale for {}.", windowContext);
    }
    return m_control->windowDisplayScale;
}

IPlatformWindowBackend& MockRuntime::GetWindowBackend() noexcept
{
    return m_windowBackend;
}
} // namespace ponder::platform::detail
