#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/core/Numbers.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Window.hpp>

#include <optional>

#include "WindowImpl.hpp"

namespace ponder::platform
{
namespace
{
[[nodiscard]] detail::BackendTextInputArea ToBackendTextInputArea(TextInputArea area)
{
    if (!IsValid(area))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument,
                                 "Text input area must have finite coordinates, finite nonnegative extents, and a "
                                 "finite cursor offset.");
    }

    const std::optional<int> x = ponder::core::RoundToInteger<int>(area.rectangle.origin.x);
    const std::optional<int> y = ponder::core::RoundToInteger<int>(area.rectangle.origin.y);
    const std::optional<int> width = ponder::core::RoundToInteger<int>(area.rectangle.extent.width);
    const std::optional<int> height = ponder::core::RoundToInteger<int>(area.rectangle.extent.height);
    const std::optional<int> cursorOffset = ponder::core::RoundToInteger<int>(area.cursorOffset);
    if (!x.has_value() || !y.has_value() || !width.has_value() || !height.has_value() || !cursorOffset.has_value())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument,
                                 "Text input area and cursor offset must round to values representable by the backend.");
    }

    return detail::BackendTextInputArea{*x, *y, *width, *height, *cursorOffset};
}
} // namespace

namespace detail
{
void WindowImpl::StartTextInput()
{
    VerifyUsable("text input start");
    if (m_backend.IsTextInputActive(m_backendWindow))
    {
        return;
    }

    m_backend.StartTextInput(m_backendWindow);
}

void WindowImpl::StopTextInput()
{
    VerifyUsable("text input stop");
    if (!m_backend.IsTextInputActive(m_backendWindow))
    {
        return;
    }

    m_backend.StopTextInput(m_backendWindow);
}

bool WindowImpl::IsTextInputActive() const
{
    VerifyUsable("text input active-state query");
    return m_backend.IsTextInputActive(m_backendWindow);
}

void WindowImpl::ClearTextComposition()
{
    VerifyUsable("text composition clear");
    m_backend.ClearTextComposition(m_backendWindow);
}

void WindowImpl::SetTextInputArea(TextInputArea area)
{
    VerifyUsable("text input area update");
    m_backend.SetTextInputArea(m_backendWindow, ToBackendTextInputArea(area));
}

void WindowImpl::ClearTextInputArea()
{
    VerifyUsable("text input area clear");
    m_backend.SetTextInputArea(m_backendWindow, std::nullopt);
}
} // namespace detail
void Window::StartTextInput()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->StartTextInput();
}

void Window::StopTextInput()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->StopTextInput();
}

bool Window::IsTextInputActive() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsTextInputActive();
}

void Window::ClearTextComposition()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->ClearTextComposition();
}

void Window::SetTextInputArea(TextInputArea area)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->SetTextInputArea(area);
}

void Window::ClearTextInputArea()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    m_state->ClearTextInputArea();
}
} // namespace ponder::platform
