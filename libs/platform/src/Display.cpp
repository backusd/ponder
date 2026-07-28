#include <ponder/core/Assert.hpp>
#include <ponder/platform/Window.hpp>

#ifdef PONDER_PLATFORM_USE_MOCK_RUNTIME
#include "MockRuntime.hpp"
#else
#include "SdlRuntime.hpp"
#endif
#include "WindowImpl.hpp"

namespace ponder::platform
{
ponder::core::Result<DisplayId> Window::GetDisplayId() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetDisplayId();
}

float Window::GetPixelDensity() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetPixelDensity();
}

float Window::GetDisplayScale() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetDisplayScale();
}

namespace detail
{
ponder::core::Result<DisplayId> WindowImpl::GetDisplayId() const
{
    VerifyUsable("display query");
    return m_runtime->GetDisplayIdForWindow(m_backendWindow, GetErrorContext());
}

float WindowImpl::GetPixelDensity() const
{
    VerifyUsable("pixel density query");
    return m_runtime->GetPixelDensityForWindow(m_backendWindow, GetErrorContext());
}

float WindowImpl::GetDisplayScale() const
{
    VerifyUsable("display scale query");
    return m_runtime->GetDisplayScaleForWindow(m_backendWindow, GetErrorContext());
}
} // namespace detail
} // namespace ponder::platform
