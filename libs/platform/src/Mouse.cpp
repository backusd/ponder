#include <ponder/core/Assert.hpp>
#include <ponder/platform/Window.hpp>

#include "WindowImpl.hpp"

namespace ponder::platform
{
namespace detail
{
void WindowImpl::SetMouseGrab(bool grabbed)
{
    VerifyUsable("mouse-grab update");
    m_backend.SetMouseGrab(m_backendWindow, grabbed);
}

bool WindowImpl::IsMouseGrabbed() const
{
    VerifyUsable("mouse-grab query");
    return m_backend.IsMouseGrabbed(m_backendWindow);
}

void WindowImpl::SetRelativeMouseMode(bool enabled)
{
    VerifyUsable("relative mouse-mode update");
    m_backend.SetRelativeMouseMode(m_backendWindow, enabled);
}

bool WindowImpl::IsRelativeMouseModeEnabled() const
{
    VerifyUsable("relative mouse-mode query");
    return m_backend.IsRelativeMouseModeEnabled(m_backendWindow);
}
} // namespace detail

void Window::SetMouseGrab(bool grabbed)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->SetMouseGrab(grabbed);
}

bool Window::IsMouseGrabbed() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsMouseGrabbed();
}

void Window::SetRelativeMouseMode(bool enabled)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->SetRelativeMouseMode(enabled);
}

bool Window::IsRelativeMouseModeEnabled() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsRelativeMouseModeEnabled();
}
} // namespace ponder::platform
