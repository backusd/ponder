#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/core/Log.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/core/String.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Runtime.hpp>

#include <atomic>
#include <exception>
#include <format>
#include <memory>
#include <optional>
#include <string>
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
constexpr std::string_view kLogCategory{"platform"};
constexpr ponder::core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);

template <typename ResultType>
[[nodiscard]] ResultType MakeUnexpectedDialogApiFailure(std::string_view operation, std::exception_ptr exceptionPointer) noexcept
{
    try
    {
        std::rethrow_exception(exceptionPointer);
    }
    catch (const ponder::core::Exception& exception)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "{} unexpectedly threw a platform exception: {}", operation, exception.GetMessage());
        return ResultType::FromError(ponder::core::Error{kBackendFailureCode, std::format("{} failed: {}", operation, exception.GetMessage()),
                                                         exception.GetStackTrace(), exception.GetLocation()});
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "{} unexpectedly threw a standard exception: {}", operation, exception.what());
        return ResultType::FromError(
            ponder::core::Error{kBackendFailureCode, std::format("{} failed with a standard exception: {}", operation, exception.what())});
    }
    catch (...)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "{} unexpectedly threw an unknown exception", operation);
        return ResultType::FromError(ponder::core::Error{kBackendFailureCode, std::format("{} failed with an unknown exception.", operation)});
    }
}

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

void Validate(std::string_view applicationName, std::optional<std::string_view> applicationVersion,
              std::optional<std::string_view> applicationIdentifier)
{
    if (applicationName.empty() || !ponder::core::IsValidUtf8WithoutEmbeddedNull(applicationName))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument,
                                 "Platform runtime application name must be non-empty UTF-8 without embedded nulls.");
    }

    if (applicationVersion.has_value() && !ponder::core::IsValidUtf8WithoutEmbeddedNull(*applicationVersion))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform runtime application version must be UTF-8 without embedded nulls.");
    }

    if (applicationVersion.has_value() && applicationVersion->empty())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform runtime application version must be absent or non-empty.");
    }

    if (applicationIdentifier.has_value() && !ponder::core::IsValidUtf8WithoutEmbeddedNull(*applicationIdentifier))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform runtime application identifier must be UTF-8 without embedded nulls.");
    }

    if (applicationIdentifier.has_value() && applicationIdentifier->empty())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform runtime application identifier must be absent or non-empty.");
    }
}
} // namespace

Runtime Runtime::Create()
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

    if (!detail::IsPlatformProcessEntryThread())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::WrongThread, "Runtime must be created on the process entry thread.");
    }

    Runtime runtime{std::make_unique<detail::RuntimeImpl>()};
    releaseInstance.Dismiss();
    return runtime;
}

void Runtime::Initialize(std::string_view applicationName, std::optional<std::string_view> applicationVersion,
                         std::optional<std::string_view> applicationIdentifier)
{
    PONDER_ASSERT(m_impl != nullptr, "Cannot initialize a moved-from Runtime");
    if (m_initialized)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Cannot initialize Runtime more than once.");
    }

    Validate(applicationName, applicationVersion, applicationIdentifier);
    const std::string ownedApplicationName{applicationName};
    const std::optional<std::string> ownedApplicationVersion =
        applicationVersion.has_value() ? std::optional<std::string>{*applicationVersion} : std::nullopt;
    const std::optional<std::string> ownedApplicationIdentifier =
        applicationIdentifier.has_value() ? std::optional<std::string>{*applicationIdentifier} : std::nullopt;

    m_impl->Initialize(ownedApplicationName,
                       ownedApplicationVersion.has_value() ? std::optional<std::string_view>{*ownedApplicationVersion} : std::nullopt,
                       ownedApplicationIdentifier.has_value() ? std::optional<std::string_view>{*ownedApplicationIdentifier} : std::nullopt);
    m_initialized = true;
}

Runtime::Runtime(std::unique_ptr<detail::RuntimeImpl> impl) noexcept :
    m_impl(std::move(impl))
{
    PONDER_ASSERT(m_impl != nullptr, "Runtime construction requires an implementation");
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
    m_impl = std::move(other.m_impl);
    m_initialized = std::exchange(other.m_initialized, false);
}

Runtime& Runtime::operator=(Runtime&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (m_impl != nullptr)
    {
        m_impl.reset();
        ReleaseRuntimeInstance();
    }

    m_impl = std::move(other.m_impl);
    m_initialized = std::exchange(other.m_initialized, false);
    return *this;
}

#define PONDER_DEFINE_RUNTIME_HINT_SPECIALIZATIONS(Type)                                                                                             \
    template <>                                                                                                                                      \
    void Runtime::HintPush<hints::Type>(const hints::Type& hint)                                                                                     \
    {                                                                                                                                                \
        PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");                                                                         \
        m_impl->HintPush<hints::Type>(hint);                                                                                                         \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void Runtime::HintPop<hints::Type>()                                                                                                             \
    {                                                                                                                                                \
        PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");                                                                         \
        m_impl->HintPop<hints::Type>();                                                                                                              \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void Runtime::HintClear<hints::Type>()                                                                                                           \
    {                                                                                                                                                \
        PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");                                                                         \
        m_impl->HintClear<hints::Type>();                                                                                                            \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    std::optional<hints::Type> Runtime::HintGet<hints::Type>() const                                                                                 \
    {                                                                                                                                                \
        PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");                                                                         \
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
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->ClipboardGetText();
}

ponder::core::VoidResult Runtime::ClipboardSetText(std::string_view text)
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->ClipboardSetText(text);
}

ponder::core::Result<dialogs::DialogRequestId> Runtime::DialogShowOpenFile(const dialogs::OpenFileDialogDesc& desc) noexcept
{
    try
    {
        PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
        PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
        return m_impl->DialogShowOpenFile(desc);
    }
    catch (...)
    {
        return MakeUnexpectedDialogApiFailure<ponder::core::Result<dialogs::DialogRequestId>>("DialogShowOpenFile", std::current_exception());
    }
}

ponder::core::Result<dialogs::DialogRequestId> Runtime::DialogShowSaveFile(const dialogs::SaveFileDialogDesc& desc) noexcept
{
    try
    {
        PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
        PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
        return m_impl->DialogShowSaveFile(desc);
    }
    catch (...)
    {
        return MakeUnexpectedDialogApiFailure<ponder::core::Result<dialogs::DialogRequestId>>("DialogShowSaveFile", std::current_exception());
    }
}

ponder::core::Result<dialogs::DialogRequestId> Runtime::DialogShowOpenFolder(const dialogs::OpenFolderDialogDesc& desc) noexcept
{
    try
    {
        PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
        PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
        return m_impl->DialogShowOpenFolder(desc);
    }
    catch (...)
    {
        return MakeUnexpectedDialogApiFailure<ponder::core::Result<dialogs::DialogRequestId>>("DialogShowOpenFolder", std::current_exception());
    }
}

std::size_t Runtime::DialogGetPendingCount() const noexcept
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DialogGetPendingCount();
}

bool Runtime::DialogHasPending() const noexcept
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DialogHasPending();
}

std::vector<DialogRequestInfo> Runtime::DialogGetPending() const noexcept
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DialogGetPending();
}

std::optional<DialogCompletedEvent> Runtime::DialogPollCompletion() noexcept
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DialogPollCompletion();
}

std::size_t Runtime::DialogGetOutstandingRequestCount() const noexcept
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DialogGetOutstandingRequestCount();
}

ponder::core::VoidResult Runtime::DialogShutdown() noexcept
{
    try
    {
        PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
        PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
        return m_impl->DialogShutdown();
    }
    catch (...)
    {
        return MakeUnexpectedDialogApiFailure<ponder::core::VoidResult>("DialogShutdown", std::current_exception());
    }
}

ponder::core::Timestamp Runtime::TimeNow() const
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->TimeNow();
}

std::optional<PlatformEvent> Runtime::EventPoll()
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->EventPoll();
}

std::optional<PlatformEvent> Runtime::EventWait(ponder::core::Duration timeout)
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->EventWait(timeout);
}

void Runtime::EventWake()
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    m_impl->EventWake();
}

Window Runtime::WindowCreate(const WindowDesc& desc)
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->WindowCreate(desc);
}

std::vector<DisplayInfo> Runtime::DisplayEnumerate()
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DisplayEnumerate();
}

ponder::core::Result<DisplayInfo> Runtime::DisplayGetInfo(DisplayId id)
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->DisplayGetInfo(id);
}

ponder::core::VoidResult Runtime::MouseSetCapture(bool enabled)
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->MouseSetCapture(enabled);
}

ponder::core::Result<LogicalPoint> Runtime::MouseGetGlobalPosition() const
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->MouseGetGlobalPosition();
}

void Runtime::MouseSetSystemCursor(SystemCursorShape shape)
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    m_impl->MouseSetSystemCursor(shape);
}

void Runtime::MouseShowCursor()
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    m_impl->MouseShowCursor();
}

void Runtime::MouseHideCursor()
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    m_impl->MouseHideCursor();
}

bool Runtime::MouseIsCursorVisible() const
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->MouseIsCursorVisible();
}

ponder::core::VoidResult Runtime::UriOpenExternal(std::string_view uri)
{
    PONDER_ASSERT(m_initialized, "Cannot use Runtime services before initialization");
    PONDER_ASSERT(m_impl != nullptr, "Cannot use a moved-from Runtime");
    return m_impl->UriOpenExternal(uri);
}
} // namespace ponder::platform
