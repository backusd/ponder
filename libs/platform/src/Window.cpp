#include <ponder/core/Assert.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/core/String.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/platform/Window.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "PlatformRuntimeBackend.hpp"
#include "PlatformRuntimeState.hpp"

#include "WindowImpl.hpp"

namespace pond::platform
{
namespace
{
constexpr core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr core::ErrorCode kUnsupportedCode = ToErrorCode(PlatformErrorCode::Unsupported);

[[nodiscard]] core::VoidResult ValidatePositiveSize(LogicalSize size, std::string_view context)
{
    if (size.width == 0 || size.height == 0 || !std::in_range<int>(size.width) ||
        !std::in_range<int>(size.height))
    {
        return core::VoidResult::FromError(
            core::Error{kInvalidArgumentCode,
                        std::string{context} +
                            " dimensions must be positive and representable by the backend."});
    }

    return core::VoidResult::Success();
}

[[nodiscard]] bool IsKnownGraphicsCompatibility(WindowGraphicsCompatibility compatibility) noexcept
{
    switch (compatibility)
    {
    case WindowGraphicsCompatibility::Default:
    case WindowGraphicsCompatibility::Vulkan:
    case WindowGraphicsCompatibility::Metal:
        return true;
    }

    return false;
}

[[nodiscard]] core::VoidResult Validate(const WindowDesc& desc)
{
    if (!core::IsValidUtf8WithoutEmbeddedNull(desc.title))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode, "Window title must be UTF-8 without embedded nulls."});
    }

    core::VoidResult sizeValidation = ValidatePositiveSize(desc.logicalSize, "Window logical size");
    RETURN_ERROR_IF_FAILED(sizeValidation);

    if (desc.minimumLogicalSize.has_value())
    {
        core::VoidResult minimumValidation =
            ValidatePositiveSize(*desc.minimumLogicalSize, "Window minimum logical size");
        RETURN_ERROR_IF_FAILED(minimumValidation);
    }

    if (!IsKnownGraphicsCompatibility(desc.graphicsCompatibility))
    {
        return core::VoidResult::FromError(
            core::Error{kInvalidArgumentCode, "Window graphics compatibility is invalid."});
    }

    if (!detail::IsWindowGraphicsCompatibilitySupported(desc.graphicsCompatibility))
    {
        return core::VoidResult::FromError(core::Error{
            kUnsupportedCode, "Window graphics compatibility is unsupported on this host."});
    }

    return core::VoidResult::Success();
}

[[nodiscard]] core::VoidResult ValidatePosition(ScreenPosition position)
{
    if (detail::IsReservedSdlWindowPosition(position.x) ||
        detail::IsReservedSdlWindowPosition(position.y))
    {
        return core::VoidResult::FromError(
            core::Error{kInvalidArgumentCode,
                        "Window position collides with a backend-reserved position value."});
    }

    return core::VoidResult::Success();
}

[[nodiscard]] core::Error MakeInvalidBackendSizeError(std::string_view operation,
                                                      std::string_view context)
{
    return core::Error{kBackendFailureCode, std::string{operation} +
                                                " returned a negative size for " +
                                                std::string{context} + "."};
}
} // namespace

namespace detail
{
core::Result<std::unique_ptr<WindowImpl>> WindowImpl::Create(PlatformRuntimeState& runtime,
                                                             const WindowDesc& desc)
{
    runtime.VerifyOwnerThread("window creation");

    core::VoidResult validation = Validate(desc);
    RETURN_ERROR_IF_FAILED(validation);

    PONDER_VERIFY(runtime.m_windowBackend != nullptr, "Platform window backend is unavailable");
    IPlatformWindowBackend& backend = *runtime.m_windowBackend;
    const BackendWindowCreateDesc backendDesc{
        .title = desc.title,
        .logicalSize = BackendWindowLogicalSize{static_cast<int>(desc.logicalSize.width),
                                                static_cast<int>(desc.logicalSize.height)},
        .resizable = desc.resizable,
        .highPixelDensity = desc.highPixelDensity,
        .graphicsCompatibility = desc.graphicsCompatibility};

    auto backendWindowResult = backend.Create(backendDesc);
    RETURN_ERROR_IF_FAILED(backendWindowResult);

    const BackendWindowHandle backendWindow = std::move(backendWindowResult).GetValue();
    auto destroyBackendWindow = core::MakeScopeExit(
        [&backend, backendWindow]() noexcept
        {
            backend.Destroy(backendWindow);
        });

    if (desc.minimumLogicalSize.has_value())
    {
        RETURN_ERROR_IF_FAILED(backend.SetMinimumSize(
            backendWindow,
            BackendWindowLogicalSize{static_cast<int>(desc.minimumLogicalSize->width),
                                     static_cast<int>(desc.minimumLogicalSize->height)}));
    }

    auto backendWindowIdResult = backend.GetId(backendWindow);
    RETURN_ERROR_IF_FAILED(backendWindowIdResult);
    const std::uint32_t backendWindowId = std::move(backendWindowIdResult).GetValue();

    auto state = std::make_unique<WindowImpl>(runtime, backend, backendWindow, backendWindowId,
                                              desc.graphicsCompatibility);
    destroyBackendWindow.Dismiss();

    const WindowId id = runtime.RegisterWindow(state.get(), backendWindowId);
    state->CommitRegistration(id);

    if (desc.visible)
    {
        RETURN_ERROR_IF_FAILED(backend.Show(backendWindow));
    }

    return core::Result<std::unique_ptr<WindowImpl>>::FromValue(std::move(state));
}
WindowImpl::WindowImpl(PlatformRuntimeState& runtime, IPlatformWindowBackend& backend,
                       BackendWindowHandle backendWindow, std::uint32_t backendWindowId,
                       WindowGraphicsCompatibility graphicsCompatibility) noexcept
    : m_runtime(&runtime), m_backend(backend), m_backendWindow(backendWindow),
      m_backendWindowId(backendWindowId), m_graphicsCompatibility(graphicsCompatibility)
{
}

WindowImpl::~WindowImpl() noexcept
{
    PONDER_VERIFY(m_runtime != nullptr, "Window runtime state is missing");
    m_runtime->VerifyOwnerThread("window destruction");
    PONDER_VERIFY(m_pendingDialogRequestCount == 0,
                  "Cannot destroy a platform window with {} pending dialog requests",
                  m_pendingDialogRequestCount);
    PONDER_VERIFY(m_backendWindow.IsValid(), "Window backend state is missing");

    if (m_registered)
    {
        m_runtime->BeginWindowDestruction(this, m_backendWindowId, m_id);
    }

    m_backend.Destroy(m_backendWindow);
    m_backendWindow = BackendWindowHandle{};

    if (m_registered)
    {
        m_runtime->FinishWindowDestruction(this);
        m_registered = false;
    }
}

void WindowImpl::CommitRegistration(WindowId id) noexcept
{
    PONDER_VERIFY(id.IsValid(), "Cannot commit an invalid platform window ID");
    PONDER_VERIFY(!m_registered, "Platform window is already registered");
    m_id = id;

    constexpr std::string_view kPrefix{"window "};
    std::copy(kPrefix.begin(), kPrefix.end(), m_errorContext.begin());
    const auto [end, error] =
        std::to_chars(m_errorContext.data() + kPrefix.size(),
                      m_errorContext.data() + m_errorContext.size(), id.GetValue());
    PONDER_VERIFY(error == std::errc{}, "Cannot format a platform window error context");
    m_errorContextLength = static_cast<std::size_t>(end - m_errorContext.data());
    m_registered = true;
}

void WindowImpl::ObserveShownEvent() noexcept
{
    const core::Result<BackendWindowProperties> properties =
        m_backend.GetProperties(m_backendWindow);
    if (!properties || properties.GetValue().hidden)
    {
        return;
    }

    SynchronizeStateRequestVisibility(false);
}

void WindowImpl::SynchronizeStateRequestVisibility(bool hidden) noexcept
{
    if (hidden)
    {
        if (!m_hiddenStateRequest.has_value() && m_pendingVisibleStateRequest.has_value())
        {
            m_hiddenStateRequest = m_pendingVisibleStateRequest;
        }
        m_pendingVisibleStateRequest.reset();
        return;
    }

    if (!m_pendingVisibleStateRequest.has_value() && m_hiddenStateRequest.has_value())
    {
        m_pendingVisibleStateRequest = m_hiddenStateRequest;
    }
    m_hiddenStateRequest.reset();
}

void WindowImpl::RecordStateRequest(::pond::platform::WindowState state, bool hidden) noexcept
{
    if (hidden)
    {
        m_hiddenStateRequest = state;
        m_pendingVisibleStateRequest.reset();
        return;
    }

    m_pendingVisibleStateRequest = state;
    m_hiddenStateRequest.reset();
}

void WindowImpl::IncrementPendingDialogRequestCount() noexcept
{
    ++m_pendingDialogRequestCount;
}

void WindowImpl::DecrementPendingDialogRequestCount() noexcept
{
    PONDER_VERIFY(m_pendingDialogRequestCount > 0,
                  "Cannot complete an unknown pending dialog request for a platform window");
    --m_pendingDialogRequestCount;
}

void WindowImpl::VerifyUsable(std::string_view operation) const
{
    PONDER_VERIFY(m_runtime != nullptr, "Cannot use a window without runtime state");
    PONDER_VERIFY(m_registered, "Cannot use an unregistered window");
    PONDER_VERIFY(m_backendWindow.IsValid(), "Cannot use a destroyed window");
    m_runtime->VerifyOwnerThread(operation);
}

std::string_view WindowImpl::GetErrorContext() const
{
    PONDER_VERIFY(m_errorContextLength != 0, "Platform window error context is not initialized");
    return std::string_view{m_errorContext.data(), m_errorContextLength};
}

WindowId WindowImpl::GetId() const
{
    VerifyUsable("ID query");
    return m_id;
}

WindowGraphicsCompatibility WindowImpl::GetGraphicsCompatibility() const
{
    VerifyUsable("graphics compatibility query");
    return m_graphicsCompatibility;
}

std::string WindowImpl::GetTitle() const
{
    VerifyUsable("title query");
    return m_backend.GetTitle(m_backendWindow);
}

core::VoidResult WindowImpl::SetTitle(std::string_view title)
{
    VerifyUsable("title update");
    if (!core::IsValidUtf8WithoutEmbeddedNull(title))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode, "Window title must be UTF-8 without embedded nulls."});
    }

    return m_backend.SetTitle(m_backendWindow, title);
}

core::Result<ScreenPosition> WindowImpl::GetPosition() const
{
    VerifyUsable("position query");
    auto positionResult = m_backend.GetPosition(m_backendWindow);
    RETURN_ERROR_IF_FAILED(positionResult);

    const BackendWindowPosition position = std::move(positionResult).GetValue();
    const std::string_view context = GetErrorContext();
    if (!std::in_range<std::int32_t>(position.x) ||
        !std::in_range<std::int32_t>(position.y))
    {
        return core::Result<ScreenPosition>::FromError(core::Error{
            kBackendFailureCode, "SDL_GetWindowPosition returned an out-of-range value for " +
                                     std::string{context} + "."});
    }

    return ScreenPosition{static_cast<std::int32_t>(position.x),
                          static_cast<std::int32_t>(position.y)};
}

core::VoidResult WindowImpl::SetPosition(ScreenPosition position)
{
    VerifyUsable("position update");
    core::VoidResult validation = ValidatePosition(position);
    RETURN_ERROR_IF_FAILED(validation);

    return m_backend.SetPosition(
        m_backendWindow,
        BackendWindowPosition{static_cast<int>(position.x), static_cast<int>(position.y)});
}

core::Result<LogicalSize> WindowImpl::GetLogicalSize() const
{
    VerifyUsable("logical size query");
    auto sizeResult = m_backend.GetSize(m_backendWindow);
    RETURN_ERROR_IF_FAILED(sizeResult);

    const BackendWindowLogicalSize size = std::move(sizeResult).GetValue();
    if (size.width < 0 || size.height < 0)
    {
        return core::Result<LogicalSize>::FromError(
            MakeInvalidBackendSizeError("SDL_GetWindowSize", GetErrorContext()));
    }

    return LogicalSize{static_cast<std::uint32_t>(size.width),
                       static_cast<std::uint32_t>(size.height)};
}

core::Result<PixelSize> WindowImpl::GetPixelSize() const
{
    VerifyUsable("pixel size query");
    auto sizeResult = m_backend.GetSizeInPixels(m_backendWindow);
    RETURN_ERROR_IF_FAILED(sizeResult);

    const BackendWindowPixelSize size = std::move(sizeResult).GetValue();
    if (size.width < 0 || size.height < 0)
    {
        return core::Result<PixelSize>::FromError(
            MakeInvalidBackendSizeError("SDL_GetWindowSizeInPixels", GetErrorContext()));
    }

    return PixelSize{static_cast<std::uint32_t>(size.width),
                     static_cast<std::uint32_t>(size.height)};
}

core::VoidResult WindowImpl::SetLogicalSize(LogicalSize size)
{
    VerifyUsable("logical size update");
    core::VoidResult validation = ValidatePositiveSize(size, "Window logical size");
    RETURN_ERROR_IF_FAILED(validation);

    return m_backend.SetSize(
        m_backendWindow,
        BackendWindowLogicalSize{static_cast<int>(size.width), static_cast<int>(size.height)});
}

core::VoidResult WindowImpl::Show()
{
    VerifyUsable("show");
    RETURN_ERROR_IF_FAILED(m_backend.Show(m_backendWindow));

    SynchronizeStateRequestVisibility(false);
    return core::VoidResult::Success();
}

core::VoidResult WindowImpl::Hide()
{
    VerifyUsable("hide");
    RETURN_ERROR_IF_FAILED(m_backend.Hide(m_backendWindow));

    SynchronizeStateRequestVisibility(true);
    return core::VoidResult::Success();
}
} // namespace detail
core::Result<Window> PlatformRuntime::CreateWindow(const WindowDesc& desc)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");

    auto stateResult = detail::WindowImpl::Create(*m_state, desc);
    RETURN_ERROR_IF_FAILED(stateResult);

    return Window{std::move(stateResult).GetValue()};
}

Window::Window(std::unique_ptr<detail::WindowImpl> state) noexcept : m_state(std::move(state)) {}

Window::~Window() noexcept = default;

Window::Window(Window&& other) noexcept : m_state(std::move(other.m_state)) {}

Window& Window::operator=(Window&& other) noexcept
{
    if (this != &other)
    {
        m_state = std::move(other.m_state);
    }
    return *this;
}

WindowId Window::GetId() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetId();
}

WindowGraphicsCompatibility Window::GetGraphicsCompatibility() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetGraphicsCompatibility();
}

std::string Window::GetTitle() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetTitle();
}

core::VoidResult Window::SetTitle(std::string_view title)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->SetTitle(title);
}

core::Result<ScreenPosition> Window::GetPosition() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetPosition();
}

core::VoidResult Window::SetPosition(ScreenPosition position)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->SetPosition(position);
}

core::Result<LogicalSize> Window::GetLogicalSize() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetLogicalSize();
}

core::Result<PixelSize> Window::GetPixelSize() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetPixelSize();
}

core::VoidResult Window::SetLogicalSize(LogicalSize size)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->SetLogicalSize(size);
}

core::VoidResult Window::Show()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->Show();
}

core::VoidResult Window::Hide()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->Hide();
}
} // namespace pond::platform
