#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/core/Timing.hpp>
#include <ponder/platform/Display.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Hints.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/Mouse.hpp>
#include <ponder/platform/PlatformEvent.hpp>
#include <ponder/platform/Window.hpp>

#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace ponder::platform
{
struct DialogFileFilter final
{
    std::string name;
    std::string pattern;

    [[nodiscard]] friend bool operator==(const DialogFileFilter& lhs, const DialogFileFilter& rhs) = default;
};

struct OpenFileDialogDesc final
{
    std::optional<WindowId> parentWindowId;
    std::optional<std::filesystem::path> defaultLocation;
    std::vector<DialogFileFilter> filters;
    bool allowMultipleSelection{};
};

struct SaveFileDialogDesc final
{
    std::optional<WindowId> parentWindowId;
    std::optional<std::filesystem::path> defaultLocation;
    std::vector<DialogFileFilter> filters;
};

struct OpenFolderDialogDesc final
{
    std::optional<WindowId> parentWindowId;
    std::optional<std::filesystem::path> defaultLocation;
    bool allowMultipleSelection{};
};

class Runtime;

using ConfigureHintsBeforeInitialization = void (*)(Runtime&);

struct RuntimeDesc final
{
    std::string applicationName{"ponder"};
    std::optional<std::string> applicationVersion;
    std::optional<std::string> applicationIdentifier;
    ConfigureHintsBeforeInitialization configureHintsBeforeInitialization{};
};

namespace detail
{
#ifdef PONDER_PLATFORM_USE_MOCK_RUNTIME
class MockRuntime;
using RuntimeImpl = MockRuntime;
#else
class SdlRuntime;
using RuntimeImpl = SdlRuntime;
#endif
} // namespace detail

class Runtime final
{
public:
    [[nodiscard]] static Runtime Create(const RuntimeDesc& desc);

    ~Runtime() noexcept;

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) noexcept;
    Runtime& operator=(Runtime&&) noexcept;

    template <typename Hint>
    void HintPush(const Hint& hint) = delete;
    template <typename Hint>
    void HintPop() = delete;
    template <typename Hint>
    void HintClear() = delete;
    template <typename Hint>
    [[nodiscard]] std::optional<Hint> HintGet() const = delete;

    [[nodiscard]] ponder::core::Result<std::string> ClipboardGetText() const;
    [[nodiscard]] ponder::core::VoidResult ClipboardSetText(std::string_view text);

    [[nodiscard]] DialogRequestId DialogShowOpenFile(const OpenFileDialogDesc& desc);
    [[nodiscard]] DialogRequestId DialogShowSaveFile(const SaveFileDialogDesc& desc);
    [[nodiscard]] DialogRequestId DialogShowOpenFolder(const OpenFolderDialogDesc& desc);
    [[nodiscard]] std::size_t DialogGetPendingCount() const;
    [[nodiscard]] bool DialogHasPending() const;
    [[nodiscard]] std::vector<DialogRequestInfo> DialogGetPending() const;
    [[nodiscard]] std::optional<DialogCompletedEvent> DialogPollCompletion();
    [[nodiscard]] std::size_t DialogGetOutstandingRequestCount() const;
    void DialogShutdown();

    [[nodiscard]] ponder::core::Timestamp TimeNow() const;
    [[nodiscard]] std::optional<PlatformEvent> EventPoll();

    [[nodiscard]] Window WindowCreate(const WindowDesc& desc);

    [[nodiscard]] std::vector<DisplayInfo> DisplayEnumerate();
    [[nodiscard]] ponder::core::Result<DisplayInfo> DisplayGetInfo(DisplayId id);

    [[nodiscard]] ponder::core::VoidResult MouseSetCapture(bool enabled);
    [[nodiscard]] ponder::core::Result<LogicalPoint> MouseGetGlobalPosition() const;
    void MouseSetSystemCursor(SystemCursorShape shape);
    void MouseShowCursor();
    void MouseHideCursor();
    [[nodiscard]] bool MouseIsCursorVisible() const;

    [[nodiscard]] ponder::core::VoidResult UriOpenExternal(std::string_view uri);

private:
    explicit Runtime(std::unique_ptr<detail::RuntimeImpl> impl) noexcept;

    std::unique_ptr<detail::RuntimeImpl> m_impl;
    bool m_hintConfigurationActive{};
};

#define PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(Type)                                                                                            \
    template <>                                                                                                                                      \
    void Runtime::HintPush<hints::Type>(const hints::Type& hint);                                                                                    \
    template <>                                                                                                                                      \
    void Runtime::HintPop<hints::Type>();                                                                                                            \
    template <>                                                                                                                                      \
    void Runtime::HintClear<hints::Type>();                                                                                                          \
    template <>                                                                                                                                      \
    [[nodiscard]] std::optional<hints::Type> Runtime::HintGet<hints::Type>() const

PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(AllowAltTabWhileGrabbed);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(EventLogging);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(ImeImplementedUi);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(PollSentinel);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(QuitOnLastWindowClose);

PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoAllowScreensaver);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoDoubleBuffer);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoDriver);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoForceEgl);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoMinimizeOnFocusLoss);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoSyncWindowOperations);

PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowActivateWhenRaised);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowActivateWhenShown);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowAllowTopmost);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowFrameUsableWhileCursorHidden);

PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseAutoCapture);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseDefaultSystemCursor);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseDoubleClickRadius);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseDoubleClickTime);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseDpiScaleCursors);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseEmulateWarpWithRelative);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseFocusClickThrough);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseNormalSpeedScale);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeCursorVisible);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeModeCenter);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeSpeedScale);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeSystemScale);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeWarpMotion);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MouseTouchEvents);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(PenMouseEvents);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(PenTouchEvents);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(TouchMouseEvents);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(TrackpadIsTouchOnly);

#if defined(__APPLE__)
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MacCtrlClickEmulatesRightClick);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(MacScrollMomentum);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoMacFullscreenSpaces);
#endif

#if defined(_WIN32)
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsCloseOnAltF4);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsEnableMenuMnemonics);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsGameInput);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboard);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboardExcludeHotkeys);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboardInputSink);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawMouseNoLegacy);
#endif

#if defined(__linux__)
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoDisplayPriority);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandAllowLibdecor);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandModeEmulation);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandPreferLibdecor);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandScaleToDisplay);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoX11NetWmBypassCompositor);
PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS(VideoX11Xrandr);
#endif

#undef PONDER_DECLARE_RUNTIME_HINT_SPECIALIZATIONS
} // namespace ponder::platform

namespace std
{
template <>
struct formatter<ponder::platform::DialogFileFilter> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::DialogFileFilter& filter, FormatContext& context) const
    {
        return formatter<string>::format(std::format("'{}' ({})", filter.name, filter.pattern), context);
    }
};

template <>
struct formatter<ponder::platform::OpenFileDialogDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::OpenFileDialogDesc& desc, FormatContext& context) const
    {
        const string parent = desc.parentWindowId.has_value() ? std::format("{}", *desc.parentWindowId) : "none";
        const string location = desc.defaultLocation.has_value() ? std::format("'{}'", desc.defaultLocation->string()) : "none";
        return formatter<string>::format(std::format("open_file_dialog(parent={}, defaultLocation={}, filterCount={}, "
                                                     "allowMultipleSelection={})",
                                                     parent, location, desc.filters.size(), desc.allowMultipleSelection),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::SaveFileDialogDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::SaveFileDialogDesc& desc, FormatContext& context) const
    {
        const string parent = desc.parentWindowId.has_value() ? std::format("{}", *desc.parentWindowId) : "none";
        const string location = desc.defaultLocation.has_value() ? std::format("'{}'", desc.defaultLocation->string()) : "none";
        return formatter<string>::format(
            std::format("save_file_dialog(parent={}, defaultLocation={}, filterCount={})", parent, location, desc.filters.size()), context);
    }
};

template <>
struct formatter<ponder::platform::OpenFolderDialogDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::OpenFolderDialogDesc& desc, FormatContext& context) const
    {
        const string parent = desc.parentWindowId.has_value() ? std::format("{}", *desc.parentWindowId) : "none";
        const string location = desc.defaultLocation.has_value() ? std::format("'{}'", desc.defaultLocation->string()) : "none";
        return formatter<string>::format(std::format("open_folder_dialog(parent={}, defaultLocation={}, "
                                                     "allowMultipleSelection={})",
                                                     parent, location, desc.allowMultipleSelection),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::RuntimeDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::RuntimeDesc& desc, FormatContext& context) const
    {
        const string version = desc.applicationVersion.has_value() ? *desc.applicationVersion : "none";
        const string identifier = desc.applicationIdentifier.has_value() ? *desc.applicationIdentifier : "none";
        return formatter<string>::format(std::format("applicationName='{}', applicationVersion='{}', "
                                                     "applicationIdentifier='{}', configuresHints={}",
                                                     desc.applicationName, version, identifier, desc.configureHintsBeforeInitialization != nullptr),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::Runtime> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(const ponder::platform::Runtime&, FormatContext& context) const
    {
        return formatter<string_view>::format("platform-runtime", context);
    }
};
} // namespace std

namespace ponder::platform
{
inline std::ostream& operator<<(std::ostream& output, const DialogFileFilter& filter)
{
    return output << std::format("{}", filter);
}

inline std::ostream& operator<<(std::ostream& output, const OpenFileDialogDesc& desc)
{
    return output << std::format("{}", desc);
}

inline std::ostream& operator<<(std::ostream& output, const SaveFileDialogDesc& desc)
{
    return output << std::format("{}", desc);
}

inline std::ostream& operator<<(std::ostream& output, const OpenFolderDialogDesc& desc)
{
    return output << std::format("{}", desc);
}

inline std::ostream& operator<<(std::ostream& output, const RuntimeDesc& desc)
{
    return output << std::format("{}", desc);
}

inline std::ostream& operator<<(std::ostream& output, const Runtime& runtime)
{
    return output << std::format("{}", runtime);
}
} // namespace ponder::platform
