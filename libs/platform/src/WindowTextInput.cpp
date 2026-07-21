#include <ponder/core/Assert.hpp>
#include <ponder/core/Numbers.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Window.hpp>

#include <optional>
#include <string_view>
#include <utility>

#include "WindowImpl.hpp"

namespace pond::platform
{
namespace
{
constexpr core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);

[[nodiscard]] core::Result<detail::BackendTextInputArea> ToBackendTextInputArea(TextInputArea area)
{
    if (!IsValid(area.rectangle))
    {
        return core::Result<detail::BackendTextInputArea>::FromError(core::Error{
            kInvalidArgumentCode, "Text input area must have finite coordinates and finite, "
                                  "nonnegative extents."});
    }

    const std::optional<int> x = core::RoundToInteger<int>(area.rectangle.origin.x);
    const std::optional<int> y = core::RoundToInteger<int>(area.rectangle.origin.y);
    const std::optional<int> width = core::RoundToInteger<int>(area.rectangle.extent.width);
    const std::optional<int> height = core::RoundToInteger<int>(area.rectangle.extent.height);
    const std::optional<int> cursorOffset = core::RoundToInteger<int>(area.cursorOffset);
    if (!x.has_value() || !y.has_value() || !width.has_value() || !height.has_value() ||
        !cursorOffset.has_value())
    {
        return core::Result<detail::BackendTextInputArea>::FromError(core::Error{
            kInvalidArgumentCode, "Text input area and cursor offset must round to values "
                                  "representable by the backend."});
    }

    return detail::BackendTextInputArea{*x, *y, *width, *height, *cursorOffset};
}
} // namespace

namespace detail
{
core::VoidResult WindowImpl::StartTextInput()
{
    VerifyUsable("text input start");
    if (m_backend.IsTextInputActive(m_backendWindow))
    {
        return core::VoidResult::Success();
    }

    return m_backend.StartTextInput(m_backendWindow);
}

core::VoidResult WindowImpl::StopTextInput()
{
    VerifyUsable("text input stop");
    if (!m_backend.IsTextInputActive(m_backendWindow))
    {
        return core::VoidResult::Success();
    }

    return m_backend.StopTextInput(m_backendWindow);
}

bool WindowImpl::IsTextInputActive() const
{
    VerifyUsable("text input active-state query");
    return m_backend.IsTextInputActive(m_backendWindow);
}

core::VoidResult WindowImpl::ClearTextComposition()
{
    VerifyUsable("text composition clear");
    return m_backend.ClearTextComposition(m_backendWindow);
}

core::VoidResult WindowImpl::SetTextInputArea(TextInputArea area)
{
    VerifyUsable("text input area update");
    auto backendAreaResult = ToBackendTextInputArea(area);
    RETURN_ERROR_IF_FAILED(backendAreaResult);

    return m_backend.SetTextInputArea(m_backendWindow,
                                      std::move(backendAreaResult).GetValue());
}

core::VoidResult WindowImpl::ClearTextInputArea()
{
    VerifyUsable("text input area clear");
    return m_backend.SetTextInputArea(m_backendWindow, std::nullopt);
}
} // namespace detail
core::VoidResult Window::StartTextInput()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->StartTextInput();
}

core::VoidResult Window::StopTextInput()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->StopTextInput();
}

bool Window::IsTextInputActive() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsTextInputActive();
}

core::VoidResult Window::ClearTextComposition()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->ClearTextComposition();
}

core::VoidResult Window::SetTextInputArea(TextInputArea area)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->SetTextInputArea(area);
}

core::VoidResult Window::ClearTextInputArea()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->ClearTextInputArea();
}
} // namespace pond::platform
