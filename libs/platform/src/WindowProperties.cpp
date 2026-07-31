#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Window.hpp>
#include <ponder/platform/WindowState.hpp>

#include <optional>
#include <string_view>

#include "WindowImpl.hpp"

namespace ponder::platform
{
namespace
{
[[nodiscard]] WindowState DecodeWindowState(const detail::BackendWindowProperties& properties, std::string_view context,
                                            const std::optional<WindowState>& hiddenStateRequest)
{
    if (properties.hidden && hiddenStateRequest.has_value())
    {
        return *hiddenStateRequest;
    }
    if (properties.minimized && properties.maximized)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL_GetWindowFlags returned contradictory minimized and maximized state for {}.",
                                 context);
    }
    if (properties.minimized)
    {
        return WindowState::Minimized;
    }
    if (properties.maximized)
    {
        return WindowState::Maximized;
    }
    return WindowState::Normal;
}

} // namespace

namespace detail
{
BackendWindowProperties WindowImpl::GetProperties(std::string_view operation) const
{
    VerifyUsable(operation);
    return m_backend.GetProperties(m_backendWindow);
}

WindowPresentation WindowImpl::GetPresentation() const
{
    const BackendWindowProperties properties = GetProperties("presentation query");
    return properties.desktopFullscreen ? WindowPresentation::DesktopFullscreen : WindowPresentation::Windowed;
}

void WindowImpl::SetPresentation(WindowPresentation presentation)
{
    VerifyUsable("presentation update");
    bool fullscreen{};
    switch (presentation)
    {
    case WindowPresentation::Windowed:
        fullscreen = false;
        break;
    case WindowPresentation::DesktopFullscreen:
        fullscreen = true;
        break;
    default:
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Window presentation is invalid.");
    }

    const BackendWindowProperties properties = GetProperties("presentation update");
    const WindowPresentation observedPresentation =
        properties.desktopFullscreen ? WindowPresentation::DesktopFullscreen : WindowPresentation::Windowed;
    if (m_pendingPresentationRequest == observedPresentation)
    {
        m_pendingPresentationRequest.reset();
    }
    if (m_pendingPresentationRequest.has_value() ? *m_pendingPresentationRequest == presentation : observedPresentation == presentation)
    {
        return;
    }

    if (fullscreen)
    {
        m_backend.SetFullscreenModeToDesktop(m_backendWindow);
    }

    m_backend.SetFullscreen(m_backendWindow, fullscreen);
    m_pendingPresentationRequest = presentation;
}

WindowDecoration WindowImpl::GetDecoration() const
{
    const BackendWindowProperties properties = GetProperties("decoration query");
    return properties.borderless ? WindowDecoration::Borderless : WindowDecoration::System;
}

void WindowImpl::SetDecoration(WindowDecoration decoration)
{
    VerifyUsable("decoration update");
    bool borderless{};
    switch (decoration)
    {
    case WindowDecoration::System:
        borderless = false;
        break;
    case WindowDecoration::Borderless:
        borderless = true;
        break;
    default:
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Window decoration is invalid.");
    }

    const BackendWindowProperties properties = GetProperties("decoration update");
    if (properties.borderless == borderless)
    {
        return;
    }
    if (properties.desktopFullscreen)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::Unsupported, "Window decoration cannot change while window {} is fullscreen.", m_id);
    }

    m_backend.SetBordered(m_backendWindow, !borderless);
}

::ponder::platform::WindowState WindowImpl::GetState() const
{
    const BackendWindowProperties properties = GetProperties("state query");
    return DecodeWindowState(properties, GetErrorContext(), m_hiddenStateRequest);
}

void WindowImpl::Minimize()
{
    const BackendWindowProperties properties = GetProperties("minimize");
    SynchronizeStateRequestVisibility(properties.hidden);
    const ::ponder::platform::WindowState state = DecodeWindowState(properties, GetErrorContext(), m_hiddenStateRequest);
    if (!properties.hidden && m_pendingVisibleStateRequest == state)
    {
        m_pendingVisibleStateRequest.reset();
    }
    const std::optional<::ponder::platform::WindowState>& pendingState = properties.hidden ? m_hiddenStateRequest : m_pendingVisibleStateRequest;
    if (pendingState.has_value() ? *pendingState == ::ponder::platform::WindowState::Minimized : state == ::ponder::platform::WindowState::Minimized)
    {
        return;
    }

    if (properties.hidden && state == ::ponder::platform::WindowState::Maximized)
    {
        m_backend.Restore(m_backendWindow);
        RecordStateRequest(::ponder::platform::WindowState::Normal, true);
    }

    m_backend.Minimize(m_backendWindow);
    RecordStateRequest(::ponder::platform::WindowState::Minimized, properties.hidden);
}

void WindowImpl::Maximize()
{
    const BackendWindowProperties properties = GetProperties("maximize");
    SynchronizeStateRequestVisibility(properties.hidden);
    const ::ponder::platform::WindowState state = DecodeWindowState(properties, GetErrorContext(), m_hiddenStateRequest);
    if (!properties.hidden && m_pendingVisibleStateRequest == state)
    {
        m_pendingVisibleStateRequest.reset();
    }
    const std::optional<::ponder::platform::WindowState>& pendingState = properties.hidden ? m_hiddenStateRequest : m_pendingVisibleStateRequest;
    if (pendingState.has_value() ? *pendingState == ::ponder::platform::WindowState::Maximized : state == ::ponder::platform::WindowState::Maximized)
    {
        return;
    }
    if (!properties.resizable)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::Unsupported, "A non-resizable window cannot be maximized.");
    }

    if (properties.hidden && state == ::ponder::platform::WindowState::Minimized)
    {
        m_backend.Restore(m_backendWindow);
        RecordStateRequest(::ponder::platform::WindowState::Normal, true);
    }

    m_backend.Maximize(m_backendWindow);
    RecordStateRequest(::ponder::platform::WindowState::Maximized, properties.hidden);
}

void WindowImpl::Restore()
{
    const BackendWindowProperties properties = GetProperties("restore");
    SynchronizeStateRequestVisibility(properties.hidden);
    const ::ponder::platform::WindowState state = DecodeWindowState(properties, GetErrorContext(), m_hiddenStateRequest);
    if (!properties.hidden && m_pendingVisibleStateRequest == state)
    {
        m_pendingVisibleStateRequest.reset();
    }
    const std::optional<::ponder::platform::WindowState>& pendingState = properties.hidden ? m_hiddenStateRequest : m_pendingVisibleStateRequest;
    if (pendingState.has_value() ? *pendingState == ::ponder::platform::WindowState::Normal : state == ::ponder::platform::WindowState::Normal)
    {
        return;
    }

    m_backend.Restore(m_backendWindow);
    RecordStateRequest(::ponder::platform::WindowState::Normal, properties.hidden);
}

bool WindowImpl::IsVisible() const
{
    return !GetProperties("visibility query").hidden;
}

bool WindowImpl::IsResizable() const
{
    return GetProperties("resizability query").resizable;
}

void WindowImpl::SetResizable(bool resizable)
{
    const BackendWindowProperties properties = GetProperties("resizability update");
    if (properties.resizable == resizable)
    {
        return;
    }
    if (properties.desktopFullscreen)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::Unsupported, "Window resizability cannot change while window {} is fullscreen.", m_id);
    }

    m_backend.SetResizable(m_backendWindow, resizable);
}

bool WindowImpl::IsFocused() const
{
    return GetProperties("focus query").inputFocus;
}

bool WindowImpl::IsAlwaysOnTop() const
{
    return GetProperties("always-on-top query").alwaysOnTop;
}

void WindowImpl::SetAlwaysOnTop(bool alwaysOnTop)
{
    const BackendWindowProperties properties = GetProperties("always-on-top update");
    if (properties.alwaysOnTop == alwaysOnTop)
    {
        return;
    }

    m_backend.SetAlwaysOnTop(m_backendWindow, alwaysOnTop);
}
} // namespace detail
WindowPresentation Window::GetPresentation() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetPresentation();
}

void Window::SetPresentation(WindowPresentation presentation)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->SetPresentation(presentation);
}

WindowDecoration Window::GetDecoration() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetDecoration();
}

void Window::SetDecoration(WindowDecoration decoration)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->SetDecoration(decoration);
}

WindowState Window::GetState() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetState();
}

void Window::Minimize()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->Minimize();
}

void Window::Maximize()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->Maximize();
}

void Window::Restore()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->Restore();
}

bool Window::IsVisible() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsVisible();
}

bool Window::IsResizable() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsResizable();
}

void Window::SetResizable(bool resizable)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->SetResizable(resizable);
}

bool Window::IsFocused() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsFocused();
}

bool Window::IsAlwaysOnTop() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsAlwaysOnTop();
}

void Window::SetAlwaysOnTop(bool alwaysOnTop)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->SetAlwaysOnTop(alwaysOnTop);
}
} // namespace ponder::platform
