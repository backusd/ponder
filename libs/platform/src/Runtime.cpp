#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/core/String.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Runtime.hpp>

#include <atomic>
#include <memory>
#include <string_view>
#include <utility>

#include "PlatformCommon.hpp"

#ifdef PONDER_PLATFORM_USE_MOCK_RUNTIME
#include "MockRuntime.hpp"
#else
#include "SdlRuntime.hpp"
#endif

namespace ponder::platform
{
namespace
{
std::atomic_uint runtimeInstanceCount{};

[[nodiscard]] bool TryAcquireRuntimeInstance() noexcept
{
    unsigned int expected{};
    return runtimeInstanceCount.compare_exchange_strong(expected, 1U, std::memory_order_acq_rel, std::memory_order_acquire);
}

void ReleaseRuntimeInstance() noexcept
{
    const unsigned int previous = runtimeInstanceCount.exchange(0U, std::memory_order_acq_rel);
    PONDER_VERIFY(previous == 1U, "Platform runtime instance counter was not acquired");
}

void Validate(const RuntimeDesc& desc)
{
    if (desc.applicationName.empty() || !ponder::core::IsValidUtf8WithoutEmbeddedNull(desc.applicationName))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument,
                                 "Platform runtime application name must be non-empty UTF-8 without embedded nulls.");
    }

    if (desc.applicationVersion.has_value() && !ponder::core::IsValidUtf8WithoutEmbeddedNull(*desc.applicationVersion))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform runtime application version must be UTF-8 without embedded nulls.");
    }

    if (desc.applicationVersion.has_value() && desc.applicationVersion->empty())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform runtime application version must be absent or non-empty.");
    }

    if (desc.applicationIdentifier.has_value() && !ponder::core::IsValidUtf8WithoutEmbeddedNull(*desc.applicationIdentifier))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform runtime application identifier must be UTF-8 without embedded nulls.");
    }

    if (desc.applicationIdentifier.has_value() && desc.applicationIdentifier->empty())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform runtime application identifier must be absent or non-empty.");
    }
}
} // namespace

Runtime Runtime::Create(const RuntimeDesc& desc)
{
    if (!TryAcquireRuntimeInstance())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::RuntimeAlreadyActive, "A platform runtime is already active.");
    }

    auto releaseInstance = ponder::core::MakeScopeExit(
        []() noexcept
        {
            ReleaseRuntimeInstance();
        });

    Validate(desc);

    if (!detail::IsPlatformProcessEntryThread())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::WrongThread, "Runtime must be created on the process entry thread.");
    }

    Runtime runtime{std::make_unique<detail::RuntimeImpl>()};
    releaseInstance.Dismiss();

    if (desc.configureHintsBeforeInitialization != nullptr)
    {
        runtime.m_hintConfigurationActive = true;
        auto finishHintConfiguration = ponder::core::MakeScopeExit(
            [&runtime]() noexcept
            {
                runtime.m_hintConfigurationActive = false;
            });
        desc.configureHintsBeforeInitialization(runtime);
    }

    runtime.m_impl->Initialize(desc);
    return runtime;
}

Runtime::Runtime(std::unique_ptr<detail::RuntimeImpl> impl) noexcept :
    m_impl(std::move(impl))
{
}

Runtime::~Runtime() noexcept
{
    if (m_impl != nullptr)
    {
        m_impl.reset();
        ReleaseRuntimeInstance();
    }
}

Runtime::Runtime(Runtime&& other) noexcept
{
    PONDER_VERIFY(!other.m_hintConfigurationActive, "Cannot move Runtime during pre-initialization hint configuration");
    m_impl = std::move(other.m_impl);
}

Runtime& Runtime::operator=(Runtime&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    PONDER_VERIFY(!m_hintConfigurationActive && !other.m_hintConfigurationActive, "Cannot move Runtime during pre-initialization hint configuration");

    if (m_impl != nullptr)
    {
        m_impl.reset();
        ReleaseRuntimeInstance();
    }
    m_impl = std::move(other.m_impl);
    return *this;
}

#define PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(Type)                                                                                             \
    template <>                                                                                                                                      \
    void Runtime::HintPush<hints::Type>(const hints::Type& hint)                                                                                     \
    {                                                                                                                                                \
        PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");                                                                         \
        m_impl->HintPush<hints::Type>(hint);                                                                                                         \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void Runtime::HintPop<hints::Type>()                                                                                                             \
    {                                                                                                                                                \
        PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");                                                                         \
        m_impl->HintPop<hints::Type>();                                                                                                              \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void Runtime::HintClear<hints::Type>()                                                                                                           \
    {                                                                                                                                                \
        PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");                                                                         \
        m_impl->HintClear<hints::Type>();                                                                                                            \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    std::optional<hints::Type> Runtime::HintGet<hints::Type>() const                                                                                 \
    {                                                                                                                                                \
        PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");                                                                         \
        return m_impl->HintGet<hints::Type>();                                                                                                       \
    }

PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(AllowAltTabWhileGrabbed)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(EventLogging)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(ImeImplementedUi)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(PollSentinel)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(QuitOnLastWindowClose)

PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoAllowScreensaver)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoDoubleBuffer)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoDriver)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoForceEgl)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoMinimizeOnFocusLoss)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoSyncWindowOperations)

PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(WindowActivateWhenRaised)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(WindowActivateWhenShown)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(WindowAllowTopmost)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(WindowFrameUsableWhileCursorHidden)

PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseAutoCapture)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseDefaultSystemCursor)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseDoubleClickRadius)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseDoubleClickTime)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseDpiScaleCursors)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseEmulateWarpWithRelative)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseFocusClickThrough)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseNormalSpeedScale)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeCursorVisible)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeModeCenter)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeSpeedScale)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeSystemScale)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseRelativeWarpMotion)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MouseTouchEvents)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(PenMouseEvents)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(PenTouchEvents)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(TouchMouseEvents)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(TrackpadIsTouchOnly)

#if defined(__APPLE__)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MacCtrlClickEmulatesRightClick)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(MacScrollMomentum)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoMacFullscreenSpaces)
#endif

#if defined(_WIN32)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(WindowsCloseOnAltF4)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(WindowsEnableMenuMnemonics)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(WindowsGameInput)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboard)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboardExcludeHotkeys)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawKeyboardInputSink)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(WindowsRawMouseNoLegacy)
#endif

#if defined(__linux__)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoDisplayPriority)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandAllowLibdecor)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandModeEmulation)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandPreferLibdecor)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoWaylandScaleToDisplay)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoX11NetWmBypassCompositor)
PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(VideoX11Xrandr)
#endif

#undef PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS

ponder::core::Result<std::string> Runtime::ClipboardGetText() const
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->ClipboardGetText();
}

ponder::core::VoidResult Runtime::ClipboardSetText(std::string_view text)
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->ClipboardSetText(text);
}

DialogRequestId Runtime::DialogShowOpenFile(const OpenFileDialogDesc& desc)
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DialogShowOpenFile(desc);
}

DialogRequestId Runtime::DialogShowSaveFile(const SaveFileDialogDesc& desc)
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DialogShowSaveFile(desc);
}

DialogRequestId Runtime::DialogShowOpenFolder(const OpenFolderDialogDesc& desc)
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DialogShowOpenFolder(desc);
}

std::size_t Runtime::DialogGetPendingCount() const
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DialogGetPendingCount();
}

bool Runtime::DialogHasPending() const
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DialogHasPending();
}

std::vector<DialogRequestInfo> Runtime::DialogGetPending() const
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DialogGetPending();
}

std::optional<DialogCompletedEvent> Runtime::DialogPollCompletion()
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DialogPollCompletion();
}

std::size_t Runtime::DialogGetOutstandingRequestCount() const
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DialogGetOutstandingRequestCount();
}

void Runtime::DialogShutdown()
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    m_impl->DialogShutdown();
}

ponder::core::Timestamp Runtime::TimeNow() const
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->TimeNow();
}

std::optional<PlatformEvent> Runtime::EventPoll()
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->EventPoll();
}

Window Runtime::WindowCreate(const WindowDesc& desc)
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->WindowCreate(desc);
}

std::vector<DisplayInfo> Runtime::DisplayEnumerate()
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DisplayEnumerate();
}

ponder::core::Result<DisplayInfo> Runtime::DisplayGetInfo(DisplayId id)
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DisplayGetInfo(id);
}

ponder::core::VoidResult Runtime::MouseSetCapture(bool enabled)
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->MouseSetCapture(enabled);
}

ponder::core::Result<LogicalPoint> Runtime::MouseGetGlobalPosition() const
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->MouseGetGlobalPosition();
}

void Runtime::MouseSetSystemCursor(SystemCursorShape shape)
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    m_impl->MouseSetSystemCursor(shape);
}

void Runtime::MouseShowCursor()
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    m_impl->MouseShowCursor();
}

void Runtime::MouseHideCursor()
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    m_impl->MouseHideCursor();
}

bool Runtime::MouseIsCursorVisible() const
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->MouseIsCursorVisible();
}

ponder::core::VoidResult Runtime::UriOpenExternal(std::string_view uri)
{
    PONDER_VERIFY(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->UriOpenExternal(uri);
}
} // namespace ponder::platform
