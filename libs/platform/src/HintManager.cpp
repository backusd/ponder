#include <ponder/core/Assert.hpp>
#include <ponder/core/Log.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/core/String.hpp>
#include <ponder/platform/HintManager.hpp>
#include <ponder/platform/PlatformError.hpp>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_mouse.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <exception>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include "HintManagerBackend.hpp"

namespace pond::platform
{
namespace
{
constexpr std::string_view kLogCategory{"platform"};
constexpr core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kNotFoundCode = ToErrorCode(PlatformErrorCode::NotFound);

struct HintDefinition final
{
    const char* name{};
    bool beforeInitialization{};
    bool pushOnce{};
};

[[nodiscard]] std::optional<SystemCursorShape> FromSdlSystemCursorValue(int value) noexcept
{
    switch (value)
    {
    case SDL_SYSTEM_CURSOR_DEFAULT:
        return SystemCursorShape::Default;
    case SDL_SYSTEM_CURSOR_TEXT:
        return SystemCursorShape::TextInput;
    case SDL_SYSTEM_CURSOR_MOVE:
        return SystemCursorShape::Move;
    case SDL_SYSTEM_CURSOR_NS_RESIZE:
        return SystemCursorShape::ResizeNorthSouth;
    case SDL_SYSTEM_CURSOR_EW_RESIZE:
        return SystemCursorShape::ResizeEastWest;
    case SDL_SYSTEM_CURSOR_NESW_RESIZE:
        return SystemCursorShape::ResizeNortheastSouthwest;
    case SDL_SYSTEM_CURSOR_NWSE_RESIZE:
        return SystemCursorShape::ResizeNorthwestSoutheast;
    case SDL_SYSTEM_CURSOR_POINTER:
        return SystemCursorShape::Pointer;
    case SDL_SYSTEM_CURSOR_WAIT:
        return SystemCursorShape::Wait;
    case SDL_SYSTEM_CURSOR_PROGRESS:
        return SystemCursorShape::Progress;
    case SDL_SYSTEM_CURSOR_NOT_ALLOWED:
        return SystemCursorShape::NotAllowed;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] constexpr bool IsValid(SystemCursorShape value) noexcept
{
    switch (value)
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

static_assert(SDL_SYSTEM_CURSOR_DEFAULT == 0);
static_assert(SDL_SYSTEM_CURSOR_TEXT == 1);
static_assert(SDL_SYSTEM_CURSOR_WAIT == 2);
static_assert(SDL_SYSTEM_CURSOR_PROGRESS == 4);
static_assert(SDL_SYSTEM_CURSOR_NWSE_RESIZE == 5);
static_assert(SDL_SYSTEM_CURSOR_NESW_RESIZE == 6);
static_assert(SDL_SYSTEM_CURSOR_EW_RESIZE == 7);
static_assert(SDL_SYSTEM_CURSOR_NS_RESIZE == 8);
static_assert(SDL_SYSTEM_CURSOR_MOVE == 9);
static_assert(SDL_SYSTEM_CURSOR_NOT_ALLOWED == 10);
static_assert(SDL_SYSTEM_CURSOR_POINTER == 11);
} // namespace

class HintManager::Impl final
{
public:
    explicit Impl(detail::IHintBackend& backend) noexcept : m_backend(backend) {}

    ~Impl() noexcept
    {
        RestoreAllHints();
    }

    [[nodiscard]] core::VoidResult Push(const HintDefinition& definition, std::string value)
    {
        VerifyMainThread("mutation");

        core::VoidResult mutation = BeginMutation();
        RETURN_ERROR_IF_FAILED(mutation);
        auto endMutation = core::MakeScopeExit(
            [this]() noexcept
            {
                m_mutationActive = false;
            });

        core::VoidResult phaseValidation = ValidateMutationPhase(definition);
        RETURN_ERROR_IF_FAILED(phaseValidation);

        HintState& state = m_states[definition.name];
        const char* const currentValue = m_backend.GetHint(definition.name);
        if (definition.pushOnce && (state.everPushed || currentValue != nullptr))
        {
            return core::VoidResult::FromError(core::Error{
                kInvalidArgumentCode,
                std::format("{} can only be set once and is already set.", definition.name)});
        }

        const bool firstValue = state.values.empty();
        if (firstValue)
        {
            state.originalValue =
                currentValue != nullptr ? std::optional<std::string>{currentValue} : std::nullopt;
            state.originalCaptured = true;
        }
        state.values.push_back(std::move(value));
        if (firstValue)
        {
            try
            {
                m_activeHints.emplace_back(definition.name);
            }
            catch (...)
            {
                state.values.pop_back();
                state.originalValue.reset();
                state.originalCaptured = false;
                throw;
            }
        }

        core::VoidResult setResult =
            m_backend.SetHintOverride(definition.name, state.values.back().c_str());
        if (!setResult)
        {
            core::Error error = std::move(setResult).GetError();
            state.values.pop_back();
            if (firstValue)
            {
                state.originalValue.reset();
                state.originalCaptured = false;
                m_activeHints.pop_back();
            }
            return core::VoidResult::FromError(std::move(error));
        }

        state.everPushed = true;
        return core::VoidResult::Success();
    }

    [[nodiscard]] core::VoidResult Pop(const HintDefinition& definition)
    {
        VerifyMainThread("pop");

        core::VoidResult mutation = BeginMutation();
        RETURN_ERROR_IF_FAILED(mutation);
        auto endMutation = core::MakeScopeExit(
            [this]() noexcept
            {
                m_mutationActive = false;
            });

        core::VoidResult phaseValidation = ValidateMutationPhase(definition);
        RETURN_ERROR_IF_FAILED(phaseValidation);

        const auto stateIterator = m_states.find(definition.name);
        if (stateIterator == m_states.end() || stateIterator->second.values.empty())
        {
            return core::VoidResult::FromError(
                core::Error{kNotFoundCode, std::format("{} does not have a managed value to pop.",
                                                       definition.name)});
        }

        HintState& state = stateIterator->second;
        core::VoidResult restoration = state.values.size() > 1
                                           ? SetValue(definition.name, state.values.end()[-2])
                                           : RestoreOriginalValue(definition.name, state);
        RETURN_ERROR_IF_FAILED(restoration);

        state.values.pop_back();
        if (state.values.empty())
        {
            FinishActivation(definition.name, state);
        }
        return core::VoidResult::Success();
    }

    [[nodiscard]] core::VoidResult Clear(const HintDefinition& definition)
    {
        VerifyMainThread("clear");

        core::VoidResult mutation = BeginMutation();
        RETURN_ERROR_IF_FAILED(mutation);
        auto endMutation = core::MakeScopeExit(
            [this]() noexcept
            {
                m_mutationActive = false;
            });

        core::VoidResult phaseValidation = ValidateMutationPhase(definition);
        RETURN_ERROR_IF_FAILED(phaseValidation);

        const auto stateIterator = m_states.find(definition.name);
        if (stateIterator == m_states.end() || stateIterator->second.values.empty())
        {
            return core::VoidResult::Success();
        }

        HintState& state = stateIterator->second;
        core::VoidResult restoration = RestoreOriginalValue(definition.name, state);
        RETURN_ERROR_IF_FAILED(restoration);

        state.values.clear();
        FinishActivation(definition.name, state);
        return core::VoidResult::Success();
    }

    [[nodiscard]] core::Result<std::string> GetRaw(const HintDefinition& definition) const
    {
        using ResultType = core::Result<std::string>;

        VerifyMainThread("query");

        const char* const value = m_backend.GetHint(definition.name);
        if (value == nullptr)
        {
            return ResultType::FromError(core::Error{
                kNotFoundCode, std::format("{} is not currently set.", definition.name)});
        }
        return std::string{value};
    }

private:
    struct HintState final
    {
        std::optional<std::string> originalValue;
        std::vector<std::string> values;
        bool originalCaptured{};
        bool everPushed{};
    };

    void VerifyMainThread(std::string_view operation) const
    {
        PONDER_VERIFY(m_backend.IsMainThread(), "Platform hint {} must run on SDL's main thread",
                      operation);
    }

    [[nodiscard]] core::VoidResult BeginMutation()
    {
        if (m_mutationActive)
        {
            return core::VoidResult::FromError(core::Error{
                kInvalidArgumentCode, "Reentrant HintManager mutations are not supported."});
        }
        m_mutationActive = true;
        return core::VoidResult::Success();
    }

    [[nodiscard]] core::VoidResult ValidateMutationPhase(const HintDefinition& definition) const
    {
        if (definition.beforeInitialization && m_backend.HasInitializedSubsystems())
        {
            return core::VoidResult::FromError(core::Error{
                kInvalidArgumentCode,
                std::format("{} can only be changed before SDL is initialized.", definition.name)});
        }
        return core::VoidResult::Success();
    }

    [[nodiscard]] core::VoidResult SetValue(const char* name, const std::string& value)
    {
        return m_backend.SetHintOverride(name, value.c_str());
    }

    [[nodiscard]] bool MatchesOriginalValue(const char* name, const HintState& state) const noexcept
    {
        const char* const currentValue = m_backend.GetHint(name);
        return state.originalValue.has_value()
                   ? currentValue != nullptr &&
                         std::string_view{currentValue} == *state.originalValue
                   : currentValue == nullptr;
    }

    [[nodiscard]] core::VoidResult RestoreOriginalValue(const char* name, const HintState& state)
    {
        PONDER_VERIFY(state.originalCaptured,
                      "Cannot restore a platform hint without its original value");

        core::VoidResult restoration =
            state.originalValue.has_value()
                ? m_backend.SetHintOverride(name, state.originalValue->c_str())
                : m_backend.ResetHint(name);
        if (restoration)
        {
            return core::VoidResult::Success();
        }

        core::Error error = std::move(restoration).GetError();
        return MatchesOriginalValue(name, state) ? core::VoidResult::Success()
                                                 : core::VoidResult::FromError(std::move(error));
    }

    void FinishActivation(std::string_view name, HintState& state)
    {
        const auto activeIterator = std::ranges::find(m_activeHints, name);
        PONDER_VERIFY(activeIterator != m_activeHints.end(),
                      "Active platform hint is missing from restoration order");
        m_activeHints.erase(activeIterator);
        state.originalValue.reset();
        state.originalCaptured = false;
    }

    void RestoreAllHints() noexcept
    {
        m_mutationActive = true;
        auto endMutation = core::MakeScopeExit(
            [this]() noexcept
            {
                m_mutationActive = false;
            });

        for (auto iterator = m_activeHints.rbegin(); iterator != m_activeHints.rend(); ++iterator)
        {
            try
            {
                const auto stateIterator = m_states.find(*iterator);
                if (stateIterator == m_states.end())
                {
                    LOG_ERROR_CATEGORY(kLogCategory,
                                       "Platform hint restoration bookkeeping is inconsistent");
                    continue;
                }

                const core::VoidResult result =
                    RestoreOriginalValue(iterator->c_str(), stateIterator->second);
                if (!result)
                {
                    LOG_ERROR_CATEGORY(kLogCategory, "{}", result.GetError().GetMessage());
                }
            }
            catch (const std::exception& exception)
            {
                LOG_ERROR_CATEGORY(kLogCategory, "Platform hint restoration failed: {}",
                                   exception.what());
            }
            catch (...)
            {
                LOG_ERROR_CATEGORY(kLogCategory, "Platform hint restoration failed");
            }
        }

        m_activeHints.clear();
        for (auto& [name, state] : m_states)
        {
            static_cast<void>(name);
            state.values.clear();
            state.originalValue.reset();
            state.originalCaptured = false;
        }
    }

    detail::IHintBackend& m_backend;
    std::unordered_map<std::string, HintState> m_states;
    std::vector<std::string> m_activeHints;
    bool m_mutationActive{};
};

HintManager::HintManager(detail::IHintBackend& backend) : m_impl(std::make_unique<Impl>(backend)) {}

HintManager::~HintManager() noexcept = default;
#define PONDER_DEFINE_BOOLEAN_HINT(Type, Name, BeforeInitialization, PushOnce)                     \
    core::VoidResult HintManager::PushHint(const hints::Type& hint)                                \
    {                                                                                              \
        constexpr HintDefinition kDefinition{Name, BeforeInitialization, PushOnce};                \
        return m_impl->Push(kDefinition, std::format("{}", hint));                                 \
    }                                                                                              \
                                                                                                   \
    core::VoidResult HintManager::PopHint(const hints::Type&)                                      \
    {                                                                                              \
        constexpr HintDefinition kDefinition{Name, BeforeInitialization, PushOnce};                \
        return m_impl->Pop(kDefinition);                                                           \
    }                                                                                              \
                                                                                                   \
    core::VoidResult HintManager::ClearHints(const hints::Type&)                                   \
    {                                                                                              \
        constexpr HintDefinition kDefinition{Name, BeforeInitialization, PushOnce};                \
        return m_impl->Clear(kDefinition);                                                         \
    }                                                                                              \
                                                                                                   \
    core::Result<hints::Type> HintManager::GetHint(const hints::Type&) const                       \
    {                                                                                              \
        using ResultType = core::Result<hints::Type>;                                              \
        constexpr HintDefinition kDefinition{Name, BeforeInitialization, PushOnce};                \
        auto rawResult = m_impl->GetRaw(kDefinition);                                              \
        RETURN_ERROR_IF_FAILED(rawResult);                                                         \
        const std::string value = std::move(rawResult).GetValue();                                 \
        if (value.empty())                                                                         \
        {                                                                                          \
            return ResultType::FromError(                                                          \
                core::Error{kBackendFailureCode,                                                   \
                            std::format("{} has an empty boolean value.", kDefinition.name)});     \
        }                                                                                          \
        return hints::Type{value.front() != '0' && !core::EqualsCaseInsensitive(value, "false")};  \
    }

PONDER_DEFINE_BOOLEAN_HINT(AllowAltTabWhileGrabbed, SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED, false,
                           false);
PONDER_DEFINE_BOOLEAN_HINT(PollSentinel, SDL_HINT_POLL_SENTINEL, false, false);
PONDER_DEFINE_BOOLEAN_HINT(QuitOnLastWindowClose, SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, false, false);

PONDER_DEFINE_BOOLEAN_HINT(VideoAllowScreensaver, SDL_HINT_VIDEO_ALLOW_SCREENSAVER, true, true);
PONDER_DEFINE_BOOLEAN_HINT(VideoDoubleBuffer, SDL_HINT_VIDEO_DOUBLE_BUFFER, true, false);
PONDER_DEFINE_BOOLEAN_HINT(VideoForceEgl, SDL_HINT_VIDEO_FORCE_EGL, true, false);
PONDER_DEFINE_BOOLEAN_HINT(VideoSyncWindowOperations, SDL_HINT_VIDEO_SYNC_WINDOW_OPERATIONS, false,
                           false);

PONDER_DEFINE_BOOLEAN_HINT(WindowActivateWhenRaised, SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED, false,
                           false);
PONDER_DEFINE_BOOLEAN_HINT(WindowActivateWhenShown, SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, false,
                           false);
PONDER_DEFINE_BOOLEAN_HINT(WindowAllowTopmost, SDL_HINT_WINDOW_ALLOW_TOPMOST, false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowFrameUsableWhileCursorHidden,
                           SDL_HINT_WINDOW_FRAME_USABLE_WHILE_CURSOR_HIDDEN, false, false);

PONDER_DEFINE_BOOLEAN_HINT(MouseAutoCapture, SDL_HINT_MOUSE_AUTO_CAPTURE, false, false);
PONDER_DEFINE_BOOLEAN_HINT(MouseDpiScaleCursors, SDL_HINT_MOUSE_DPI_SCALE_CURSORS, true, false);
PONDER_DEFINE_BOOLEAN_HINT(MouseEmulateWarpWithRelative, SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE,
                           false, false);
PONDER_DEFINE_BOOLEAN_HINT(MouseFocusClickThrough, SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, false, false);
PONDER_DEFINE_BOOLEAN_HINT(MouseRelativeCursorVisible, SDL_HINT_MOUSE_RELATIVE_CURSOR_VISIBLE,
                           false, false);
PONDER_DEFINE_BOOLEAN_HINT(MouseRelativeModeCenter, SDL_HINT_MOUSE_RELATIVE_MODE_CENTER, false,
                           false);
PONDER_DEFINE_BOOLEAN_HINT(MouseRelativeSystemScale, SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, false,
                           false);
PONDER_DEFINE_BOOLEAN_HINT(MouseRelativeWarpMotion, SDL_HINT_MOUSE_RELATIVE_WARP_MOTION, false,
                           false);
PONDER_DEFINE_BOOLEAN_HINT(MouseTouchEvents, SDL_HINT_MOUSE_TOUCH_EVENTS, false, false);
PONDER_DEFINE_BOOLEAN_HINT(PenMouseEvents, SDL_HINT_PEN_MOUSE_EVENTS, false, false);
PONDER_DEFINE_BOOLEAN_HINT(PenTouchEvents, SDL_HINT_PEN_TOUCH_EVENTS, false, false);
PONDER_DEFINE_BOOLEAN_HINT(TouchMouseEvents, SDL_HINT_TOUCH_MOUSE_EVENTS, false, false);
PONDER_DEFINE_BOOLEAN_HINT(TrackpadIsTouchOnly, SDL_HINT_TRACKPAD_IS_TOUCH_ONLY, true, false);

#if defined(__APPLE__)
PONDER_DEFINE_BOOLEAN_HINT(MacCtrlClickEmulatesRightClick,
                           SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK, false, false);
PONDER_DEFINE_BOOLEAN_HINT(MacScrollMomentum, SDL_HINT_MAC_SCROLL_MOMENTUM, true, false);
PONDER_DEFINE_BOOLEAN_HINT(VideoMacFullscreenSpaces, SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, true,
                           false);
#endif

#if defined(_WIN32)
PONDER_DEFINE_BOOLEAN_HINT(WindowsCloseOnAltF4, SDL_HINT_WINDOWS_CLOSE_ON_ALT_F4, false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowsEnableMenuMnemonics, SDL_HINT_WINDOWS_ENABLE_MENU_MNEMONICS,
                           false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowsGameInput, SDL_HINT_WINDOWS_GAMEINPUT, true, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowsRawKeyboard, SDL_HINT_WINDOWS_RAW_KEYBOARD, false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowsRawKeyboardExcludeHotkeys,
                           SDL_HINT_WINDOWS_RAW_KEYBOARD_EXCLUDE_HOTKEYS, false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowsRawKeyboardInputSink, SDL_HINT_WINDOWS_RAW_KEYBOARD_INPUTSINK,
                           false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowsRawMouseNoLegacy, SDL_HINT_WINDOWS_RAW_MOUSE_NOLEGACY, false,
                           false);
#endif

#if defined(__linux__)
PONDER_DEFINE_BOOLEAN_HINT(VideoWaylandAllowLibdecor, SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, true,
                           false);
PONDER_DEFINE_BOOLEAN_HINT(VideoWaylandModeEmulation, SDL_HINT_VIDEO_WAYLAND_MODE_EMULATION, true,
                           false);
PONDER_DEFINE_BOOLEAN_HINT(VideoWaylandPreferLibdecor, SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR, true,
                           false);
PONDER_DEFINE_BOOLEAN_HINT(VideoWaylandScaleToDisplay, SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY,
                           true, false);
PONDER_DEFINE_BOOLEAN_HINT(VideoX11NetWmBypassCompositor,
                           SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, true, false);
PONDER_DEFINE_BOOLEAN_HINT(VideoX11Xrandr, SDL_HINT_VIDEO_X11_XRANDR, true, false);
#endif

#undef PONDER_DEFINE_BOOLEAN_HINT

#define PONDER_DEFINE_HINT_POP_AND_CLEAR(Type, Name, BeforeInitialization, PushOnce)               \
    core::VoidResult HintManager::PopHint(const hints::Type&)                                      \
    {                                                                                              \
        constexpr HintDefinition kDefinition{Name, BeforeInitialization, PushOnce};                \
        return m_impl->Pop(kDefinition);                                                           \
    }                                                                                              \
                                                                                                   \
    core::VoidResult HintManager::ClearHints(const hints::Type&)                                   \
    {                                                                                              \
        constexpr HintDefinition kDefinition{Name, BeforeInitialization, PushOnce};                \
        return m_impl->Clear(kDefinition);                                                         \
    }

core::VoidResult HintManager::PushHint(const hints::EventLogging& hint)
{
    if (!IsValid(hint.value))
    {
        return core::VoidResult::FromError(
            core::Error{kInvalidArgumentCode, "Platform event logging level is invalid."});
    }

    constexpr HintDefinition kDefinition{SDL_HINT_EVENT_LOGGING};
    return m_impl->Push(kDefinition, std::format("{}", hint));
}
PONDER_DEFINE_HINT_POP_AND_CLEAR(EventLogging, SDL_HINT_EVENT_LOGGING, false, false);
core::Result<hints::EventLogging> HintManager::GetHint(const hints::EventLogging&) const
{
    using ResultType = core::Result<hints::EventLogging>;
    constexpr HintDefinition kDefinition{SDL_HINT_EVENT_LOGGING};
    auto rawResult = m_impl->GetRaw(kDefinition);
    RETURN_ERROR_IF_FAILED(rawResult);
    const std::string value = std::move(rawResult).GetValue();

    if (value == "0")
    {
        return hints::EventLogging{hints::EventLoggingLevel::Disabled};
    }
    if (value == "1")
    {
        return hints::EventLogging{hints::EventLoggingLevel::Common};
    }
    if (value == "2")
    {
        return hints::EventLogging{hints::EventLoggingLevel::Verbose};
    }
    return ResultType::FromError(
        core::Error{kBackendFailureCode, "SDL returned an invalid event-logging platform hint."});
}

core::VoidResult HintManager::PushHint(const hints::ImeImplementedUi& hint)
{
    if (!IsValid(hint.value))
    {
        return core::VoidResult::FromError(
            core::Error{kInvalidArgumentCode, "Platform IME UI capabilities value is invalid."});
    }

    constexpr HintDefinition kDefinition{SDL_HINT_IME_IMPLEMENTED_UI, true};
    return m_impl->Push(kDefinition, std::format("{}", hint));
}
PONDER_DEFINE_HINT_POP_AND_CLEAR(ImeImplementedUi, SDL_HINT_IME_IMPLEMENTED_UI, true, false);
core::Result<hints::ImeImplementedUi> HintManager::GetHint(const hints::ImeImplementedUi&) const
{
    using ResultType = core::Result<hints::ImeImplementedUi>;
    constexpr HintDefinition kDefinition{SDL_HINT_IME_IMPLEMENTED_UI, true};
    auto rawResult = m_impl->GetRaw(kDefinition);
    RETURN_ERROR_IF_FAILED(rawResult);
    const std::string value = std::move(rawResult).GetValue();

    if (value == "0" || value == "none")
    {
        return hints::ImeImplementedUi{hints::ImeUiCapabilities::None};
    }
    if (value == "composition")
    {
        return hints::ImeImplementedUi{hints::ImeUiCapabilities::Composition};
    }
    if (value == "candidates")
    {
        return hints::ImeImplementedUi{hints::ImeUiCapabilities::Candidates};
    }
    if (value == "composition,candidates" || value == "candidates,composition")
    {
        return hints::ImeImplementedUi{hints::ImeUiCapabilities::CompositionAndCandidates};
    }
    return ResultType::FromError(
        core::Error{kBackendFailureCode, "SDL returned invalid IME UI capabilities."});
}
#define PONDER_DEFINE_STRING_HINT(Type, Name, BeforeInitialization)                                \
    core::VoidResult HintManager::PushHint(const hints::Type& hint)                                \
    {                                                                                              \
        if (hint.value.empty() || hint.value.find('\0') != std::string::npos)                      \
        {                                                                                          \
            return core::VoidResult::FromError(core::Error{                                        \
                kInvalidArgumentCode,                                                              \
                "Platform hint string value must be non-empty and contain no embedded nulls."});   \
        }                                                                                          \
        constexpr HintDefinition kDefinition{Name, BeforeInitialization};                          \
        return m_impl->Push(kDefinition, std::format("{}", hint));                                 \
    }                                                                                              \
    PONDER_DEFINE_HINT_POP_AND_CLEAR(Type, Name, BeforeInitialization, false);                     \
    core::Result<hints::Type> HintManager::GetHint(const hints::Type&) const                       \
    {                                                                                              \
        constexpr HintDefinition kDefinition{Name, BeforeInitialization};                          \
        auto rawResult = m_impl->GetRaw(kDefinition);                                              \
        RETURN_ERROR_IF_FAILED(rawResult);                                                         \
        return hints::Type{std::move(rawResult).GetValue()};                                       \
    }

PONDER_DEFINE_STRING_HINT(VideoDriver, SDL_HINT_VIDEO_DRIVER, true);
#if defined(__linux__)
PONDER_DEFINE_STRING_HINT(VideoDisplayPriority, SDL_HINT_VIDEO_DISPLAY_PRIORITY, true);
#endif
#undef PONDER_DEFINE_STRING_HINT

core::VoidResult HintManager::PushHint(const hints::VideoMinimizeOnFocusLoss& hint)
{
    if (!IsValid(hint.value))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode, "Platform fullscreen focus-loss behavior is invalid."});
    }

    constexpr HintDefinition kDefinition{SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS};
    return m_impl->Push(kDefinition, std::format("{}", hint));
}
PONDER_DEFINE_HINT_POP_AND_CLEAR(VideoMinimizeOnFocusLoss, SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS,
                                 false, false);
core::Result<hints::VideoMinimizeOnFocusLoss> HintManager::GetHint(
    const hints::VideoMinimizeOnFocusLoss&) const
{
    using ResultType = core::Result<hints::VideoMinimizeOnFocusLoss>;
    constexpr HintDefinition kDefinition{SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS};
    auto rawResult = m_impl->GetRaw(kDefinition);
    RETURN_ERROR_IF_FAILED(rawResult);
    const std::string value = std::move(rawResult).GetValue();

    if (value == "auto")
    {
        return hints::VideoMinimizeOnFocusLoss{hints::FullscreenFocusLossBehavior::Automatic};
    }
    if (value == "1")
    {
        return hints::VideoMinimizeOnFocusLoss{hints::FullscreenFocusLossBehavior::Minimize};
    }
    if (value == "0")
    {
        return hints::VideoMinimizeOnFocusLoss{hints::FullscreenFocusLossBehavior::KeepFullscreen};
    }
    return ResultType::FromError(core::Error{
        kBackendFailureCode, "SDL returned an invalid fullscreen focus-loss platform hint."});
}

core::VoidResult HintManager::PushHint(const hints::MouseDefaultSystemCursor& hint)
{
    if (!IsValid(hint.value))
    {
        return core::VoidResult::FromError(
            core::Error{kInvalidArgumentCode, "Platform hint system cursor value is invalid."});
    }

    constexpr HintDefinition kDefinition{SDL_HINT_MOUSE_DEFAULT_SYSTEM_CURSOR, true};
    return m_impl->Push(kDefinition, std::format("{}", hint));
}
PONDER_DEFINE_HINT_POP_AND_CLEAR(MouseDefaultSystemCursor, SDL_HINT_MOUSE_DEFAULT_SYSTEM_CURSOR,
                                 true, false);
core::Result<hints::MouseDefaultSystemCursor> HintManager::GetHint(
    const hints::MouseDefaultSystemCursor&) const
{
    using ResultType = core::Result<hints::MouseDefaultSystemCursor>;
    constexpr HintDefinition kDefinition{SDL_HINT_MOUSE_DEFAULT_SYSTEM_CURSOR, true};
    auto rawResult = m_impl->GetRaw(kDefinition);
    RETURN_ERROR_IF_FAILED(rawResult);
    const std::string value = std::move(rawResult).GetValue();
    const std::optional<int> parsed = core::ParseNumber<int>(value);
    if (!parsed.has_value())
    {
        return ResultType::FromError(core::Error{
            kBackendFailureCode, "SDL returned an invalid system-cursor platform hint."});
    }

    const std::optional<SystemCursorShape> cursor = FromSdlSystemCursorValue(*parsed);
    if (!cursor.has_value())
    {
        return ResultType::FromError(core::Error{
            kBackendFailureCode, "SDL returned an unsupported system-cursor platform hint."});
    }
    return hints::MouseDefaultSystemCursor{*cursor};
}

core::VoidResult HintManager::PushHint(const hints::MouseDoubleClickRadius& hint)
{
    if (hint.value > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode, "Platform hint integer value exceeds SDL's supported range."});
    }

    constexpr HintDefinition kDefinition{SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS};
    return m_impl->Push(kDefinition, std::format("{}", hint));
}
PONDER_DEFINE_HINT_POP_AND_CLEAR(MouseDoubleClickRadius, SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS, false,
                                 false);
core::Result<hints::MouseDoubleClickRadius> HintManager::GetHint(
    const hints::MouseDoubleClickRadius&) const
{
    using ResultType = core::Result<hints::MouseDoubleClickRadius>;
    constexpr HintDefinition kDefinition{SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS};
    auto rawResult = m_impl->GetRaw(kDefinition);
    RETURN_ERROR_IF_FAILED(rawResult);
    const std::string value = std::move(rawResult).GetValue();
    const std::optional<std::uint32_t> parsed = core::ParseNumber<std::uint32_t>(value);
    if (!parsed.has_value() ||
        *parsed > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
    {
        return ResultType::FromError(core::Error{
            kBackendFailureCode, "SDL returned an invalid unsigned-integer platform hint."});
    }
    return hints::MouseDoubleClickRadius{*parsed};
}

core::VoidResult HintManager::PushHint(const hints::MouseDoubleClickTime& hint)
{
    if (hint.value.count() < 0 || hint.value.count() > std::numeric_limits<int>::max())
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode,
            "Platform hint duration must fit in SDL's non-negative millisecond range."});
    }

    constexpr HintDefinition kDefinition{SDL_HINT_MOUSE_DOUBLE_CLICK_TIME};
    return m_impl->Push(kDefinition, std::format("{}", hint));
}
PONDER_DEFINE_HINT_POP_AND_CLEAR(MouseDoubleClickTime, SDL_HINT_MOUSE_DOUBLE_CLICK_TIME, false,
                                 false);
core::Result<hints::MouseDoubleClickTime> HintManager::GetHint(
    const hints::MouseDoubleClickTime&) const
{
    using ResultType = core::Result<hints::MouseDoubleClickTime>;
    constexpr HintDefinition kDefinition{SDL_HINT_MOUSE_DOUBLE_CLICK_TIME};
    auto rawResult = m_impl->GetRaw(kDefinition);
    RETURN_ERROR_IF_FAILED(rawResult);
    const std::string value = std::move(rawResult).GetValue();
    const std::optional<std::int64_t> parsed = core::ParseNumber<std::int64_t>(value);
    if (!parsed.has_value() || *parsed < 0 || *parsed > std::numeric_limits<int>::max())
    {
        return ResultType::FromError(
            core::Error{kBackendFailureCode, "SDL returned an invalid duration platform hint."});
    }
    return hints::MouseDoubleClickTime{std::chrono::milliseconds{*parsed}};
}
#define PONDER_DEFINE_FLOAT_HINT(Type, Name)                                                       \
    core::VoidResult HintManager::PushHint(const hints::Type& hint)                                \
    {                                                                                              \
        if (!std::isfinite(hint.value))                                                            \
        {                                                                                          \
            return core::VoidResult::FromError(core::Error{                                        \
                kInvalidArgumentCode, "Platform hint floating-point value must be finite."});      \
        }                                                                                          \
        constexpr HintDefinition kDefinition{Name};                                                \
        return m_impl->Push(kDefinition, std::format("{}", hint));                                 \
    }                                                                                              \
    PONDER_DEFINE_HINT_POP_AND_CLEAR(Type, Name, false, false);                                    \
    core::Result<hints::Type> HintManager::GetHint(const hints::Type&) const                       \
    {                                                                                              \
        using ResultType = core::Result<hints::Type>;                                              \
        constexpr HintDefinition kDefinition{Name};                                                \
        auto rawResult = m_impl->GetRaw(kDefinition);                                              \
        RETURN_ERROR_IF_FAILED(rawResult);                                                         \
        const std::string value = std::move(rawResult).GetValue();                                 \
        const std::optional<float> parsed =                                                 \
            core::ParseNumber<float>(value);                                                \
        if (!parsed.has_value() || !std::isfinite(*parsed))                                        \
        {                                                                                          \
            return ResultType::FromError(core::Error{                                              \
                kBackendFailureCode, "SDL returned an invalid floating-point platform hint."});    \
        }                                                                                          \
        return hints::Type{*parsed};                                                               \
    }

PONDER_DEFINE_FLOAT_HINT(MouseNormalSpeedScale, SDL_HINT_MOUSE_NORMAL_SPEED_SCALE);
PONDER_DEFINE_FLOAT_HINT(MouseRelativeSpeedScale, SDL_HINT_MOUSE_RELATIVE_SPEED_SCALE);

#undef PONDER_DEFINE_FLOAT_HINT
#undef PONDER_DEFINE_HINT_POP_AND_CLEAR
} // namespace pond::platform