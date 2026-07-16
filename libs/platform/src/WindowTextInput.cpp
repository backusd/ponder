#include <ponder/core/Assert.hpp>
#include <ponder/core/Numbers.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Window.hpp>

#include <optional>
#include <string_view>
#include <utility>

#include "SdlError.hpp"
#include "WindowImpl.hpp"

namespace pond::platform
{
namespace
{
constexpr core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);

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
    if (m_backend.isTextInputActive(m_backend.context, m_nativeWindow))
    {
        return core::VoidResult::Success();
    }

    const std::string_view context = GetErrorContext();
    if (!m_backend.startTextInput(m_backend.context, m_nativeWindow))
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_StartTextInput", context));
    }

    return core::VoidResult::Success();
}

core::VoidResult WindowImpl::StopTextInput()
{
    VerifyUsable("text input stop");
    if (!m_backend.isTextInputActive(m_backend.context, m_nativeWindow))
    {
        return core::VoidResult::Success();
    }

    const std::string_view context = GetErrorContext();
    if (!m_backend.stopTextInput(m_backend.context, m_nativeWindow))
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_StopTextInput", context));
    }

    return core::VoidResult::Success();
}

bool WindowImpl::IsTextInputActive() const
{
    VerifyUsable("text input active-state query");
    return m_backend.isTextInputActive(m_backend.context, m_nativeWindow);
}

core::VoidResult WindowImpl::ClearTextComposition()
{
    VerifyUsable("text composition clear");
    const std::string_view context = GetErrorContext();
    if (!m_backend.clearTextComposition(m_backend.context, m_nativeWindow))
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_ClearComposition", context));
    }

    return core::VoidResult::Success();
}

core::VoidResult WindowImpl::SetTextInputArea(TextInputArea area)
{
    VerifyUsable("text input area update");
    auto backendAreaResult = ToBackendTextInputArea(area);
    RETURN_ERROR_IF_FAILED(backendAreaResult);

    const BackendTextInputArea backendArea = std::move(backendAreaResult).GetValue();
    const std::string_view context = GetErrorContext();
    if (!m_backend.setTextInputArea(m_backend.context, m_nativeWindow, &backendArea))
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_SetTextInputArea", context));
    }

    return core::VoidResult::Success();
}

core::VoidResult WindowImpl::ClearTextInputArea()
{
    VerifyUsable("text input area clear");
    const std::string_view context = GetErrorContext();
    if (!m_backend.setTextInputArea(m_backend.context, m_nativeWindow, nullptr))
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_SetTextInputArea", context));
    }

    return core::VoidResult::Success();
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
