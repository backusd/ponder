#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/core/String.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Window.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#ifdef PONDER_PLATFORM_USE_MOCK_RUNTIME
#include "MockRuntime.hpp"
#else
#include "SdlRuntime.hpp"
#endif
#include "SdlRuntimeTypes.hpp"
#include "WindowImpl.hpp"

namespace ponder::platform
{
namespace
{
void ValidatePositiveSize(LogicalSize size, std::string_view context)
{
    if (size.width == 0 || size.height == 0 || !std::in_range<int>(size.width) || !std::in_range<int>(size.height))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "{} dimensions must be positive and representable by the backend.", context);
    }
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

void Validate(const WindowDesc& desc)
{
    if (!ponder::core::IsValidUtf8WithoutEmbeddedNull(desc.title))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Window title must be UTF-8 without embedded nulls.");
    }

    ValidatePositiveSize(desc.logicalSize, "Window logical size");

    if (desc.minimumLogicalSize.has_value())
    {
        ValidatePositiveSize(*desc.minimumLogicalSize, "Window minimum logical size");
    }

    if (!IsKnownGraphicsCompatibility(desc.graphicsCompatibility))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Window graphics compatibility is invalid.");
    }

    if (!detail::IsWindowGraphicsCompatibilitySupported(desc.graphicsCompatibility))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::Unsupported, "Window graphics compatibility is unsupported on this host.");
    }
}

void ValidatePosition(ScreenPosition position)
{
    if (detail::IsReservedSdlWindowPosition(position.x) || detail::IsReservedSdlWindowPosition(position.y))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Window position collides with a backend-reserved position value.");
    }
}
} // namespace

namespace detail
{
std::unique_ptr<WindowImpl> WindowImpl::Create(RuntimeImpl& runtime, const WindowDesc& desc)
{
    runtime.VerifyOwnerThread("window creation");
    Validate(desc);

    IPlatformWindowBackend& backend = runtime.GetWindowBackend();
    const BackendWindowCreateDesc backendDesc{
        .title = desc.title,
        .logicalSize = BackendWindowLogicalSize{static_cast<int>(desc.logicalSize.width), static_cast<int>(desc.logicalSize.height)},
        .resizable = desc.resizable,
        .highPixelDensity = desc.highPixelDensity,
        .graphicsCompatibility = desc.graphicsCompatibility};

    const BackendWindowHandle backendWindow = backend.Create(backendDesc);
    auto destroyBackendWindow = ponder::core::MakeScopeExit(
        [&backend, backendWindow]() noexcept
        {
            backend.Destroy(backendWindow);
        });

    if (desc.minimumLogicalSize.has_value())
    {
        backend.SetMinimumSize(backendWindow, BackendWindowLogicalSize{static_cast<int>(desc.minimumLogicalSize->width),
                                                                       static_cast<int>(desc.minimumLogicalSize->height)});
    }

    const std::uint32_t backendWindowId = backend.GetId(backendWindow);

    auto state = std::make_unique<WindowImpl>(runtime, backend, backendWindow, backendWindowId, desc.graphicsCompatibility);
    destroyBackendWindow.Dismiss();

    const WindowId id = runtime.GetNextWindowIdForRegistration();
    state->PrepareRegistration(id);

    runtime.RegisterWindow(*state, backendWindow, backendWindowId, id);

    if (desc.visible)
    {
        backend.Show(backendWindow);
    }

    state->PublishConstruction();
    return state;
}

WindowImpl::WindowImpl(RuntimeImpl& runtime, IPlatformWindowBackend& backend, BackendWindowHandle backendWindow, std::uint32_t backendWindowId,
                       WindowGraphicsCompatibility graphicsCompatibility) noexcept :
    m_runtime(&runtime),
    m_backend(backend),
    m_backendWindow(backendWindow),
    m_backendWindowId(backendWindowId),
    m_graphicsCompatibility(graphicsCompatibility)
{
}

WindowImpl::~WindowImpl() noexcept
{
    PONDER_VERIFY(m_runtime != nullptr, "Window runtime state is missing");
    m_runtime->VerifyOwnerThreadForDestruction("Window");
    PONDER_VERIFY(m_backendWindow.IsValid(), "Window backend state is missing");

    const bool restoreWindowId = m_registered && !m_constructionPublished;
    if (m_registered)
    {
        m_runtime->BeginWindowDestruction(*this, m_backendWindowId, m_id);
    }

    m_backend.Destroy(m_backendWindow);
    m_backendWindow = BackendWindowHandle{};

    if (m_registered)
    {
        m_runtime->FinishWindowDestruction(*this);
        m_registered = false;
    }

    if (restoreWindowId)
    {
        m_runtime->RestoreWindowIdAfterFailedConstruction(m_id);
    }
}

void WindowImpl::PrepareRegistration(WindowId id) noexcept
{
    PONDER_VERIFY(id.IsValid(), "Cannot prepare an invalid platform window ID");
    PONDER_VERIFY(!m_registered, "Platform window is already registered");
    PONDER_VERIFY(!m_constructionPublished, "Platform window construction is already published");
    m_id = id;

    constexpr std::string_view kPrefix{"window "};
    std::copy(kPrefix.begin(), kPrefix.end(), m_errorContext.begin());
    const auto [end, error] = std::to_chars(m_errorContext.data() + kPrefix.size(), m_errorContext.data() + m_errorContext.size(), id.GetValue());
    PONDER_VERIFY(error == std::errc{}, "Cannot format a platform window error context");
    m_errorContextLength = static_cast<std::size_t>(end - m_errorContext.data());
}

void WindowImpl::CommitRegistration() noexcept
{
    PONDER_VERIFY(m_id.IsValid(), "Cannot commit an invalid platform window ID");
    PONDER_VERIFY(m_errorContextLength != 0, "Cannot commit a platform window without diagnostic context");
    PONDER_VERIFY(!m_registered, "Platform window is already registered");
    m_registered = true;
}

void WindowImpl::PublishConstruction() noexcept
{
    PONDER_VERIFY(m_registered, "Cannot publish an unregistered platform window");
    PONDER_VERIFY(!m_constructionPublished, "Platform window construction is already published");
    m_constructionPublished = true;
}

void WindowImpl::ObserveShownEvent()
{
    const BackendWindowProperties properties = m_backend.GetProperties(m_backendWindow);
    if (properties.hidden)
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

void WindowImpl::RecordStateRequest(::ponder::platform::WindowState state, bool hidden) noexcept
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

void WindowImpl::SetTitle(std::string_view title)
{
    VerifyUsable("title update");
    if (!ponder::core::IsValidUtf8WithoutEmbeddedNull(title))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Window title must be UTF-8 without embedded nulls.");
    }

    m_backend.SetTitle(m_backendWindow, title);
}

ScreenPosition WindowImpl::GetPosition() const
{
    VerifyUsable("position query");
    const BackendWindowPosition position = m_backend.GetPosition(m_backendWindow);
    const std::string_view context = GetErrorContext();
    if (!std::in_range<std::int32_t>(position.x) || !std::in_range<std::int32_t>(position.y))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL_GetWindowPosition returned an out-of-range value for {}.", context);
    }

    return ScreenPosition{static_cast<std::int32_t>(position.x), static_cast<std::int32_t>(position.y)};
}

void WindowImpl::SetPosition(ScreenPosition position)
{
    VerifyUsable("position update");
    ValidatePosition(position);

    m_backend.SetPosition(m_backendWindow, BackendWindowPosition{static_cast<int>(position.x), static_cast<int>(position.y)});
}

LogicalSize WindowImpl::GetLogicalSize() const
{
    VerifyUsable("logical size query");
    const BackendWindowLogicalSize size = m_backend.GetSize(m_backendWindow);
    if (size.width < 0 || size.height < 0)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL_GetWindowSize returned a negative size for {}.", GetErrorContext());
    }

    return LogicalSize{static_cast<std::uint32_t>(size.width), static_cast<std::uint32_t>(size.height)};
}

PixelSize WindowImpl::GetPixelSize() const
{
    VerifyUsable("pixel size query");
    const BackendWindowPixelSize size = m_backend.GetSizeInPixels(m_backendWindow);
    if (size.width < 0 || size.height < 0)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL_GetWindowSizeInPixels returned a negative size for {}.", GetErrorContext());
    }

    return PixelSize{static_cast<std::uint32_t>(size.width), static_cast<std::uint32_t>(size.height)};
}

void WindowImpl::SetLogicalSize(LogicalSize size)
{
    VerifyUsable("logical size update");
    ValidatePositiveSize(size, "Window logical size");

    m_backend.SetSize(m_backendWindow, BackendWindowLogicalSize{static_cast<int>(size.width), static_cast<int>(size.height)});
}

void WindowImpl::Show()
{
    VerifyUsable("show");
    m_backend.Show(m_backendWindow);

    SynchronizeStateRequestVisibility(false);
}

void WindowImpl::Hide()
{
    VerifyUsable("hide");
    m_backend.Hide(m_backendWindow);

    SynchronizeStateRequestVisibility(true);
}
} // namespace detail

Window::Window(std::unique_ptr<detail::WindowImpl> state) noexcept :
    m_state(std::move(state))
{
}

Window::~Window() noexcept = default;

Window::Window(Window&& other) noexcept :
    m_state(std::move(other.m_state))
{
}

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

void Window::SetTitle(std::string_view title)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->SetTitle(title);
}

ScreenPosition Window::GetPosition() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetPosition();
}

void Window::SetPosition(ScreenPosition position)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->SetPosition(position);
}

LogicalSize Window::GetLogicalSize() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetLogicalSize();
}

PixelSize Window::GetPixelSize() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetPixelSize();
}

void Window::SetLogicalSize(LogicalSize size)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->SetLogicalSize(size);
}

void Window::Show()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->Show();
}

void Window::Hide()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->Hide();
}
} // namespace ponder::platform
