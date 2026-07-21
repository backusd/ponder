#include <ponder/core/Assert.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/platform/Window.hpp>

#include <cstddef>
#include <optional>
#include <utility>

#include "PlatformRuntimeState.hpp"
#include "WindowImpl.hpp"

namespace pond::platform
{
namespace
{
constexpr core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr core::ErrorCode kUnsupportedCode = ToErrorCode(PlatformErrorCode::Unsupported);

[[nodiscard]] std::optional<std::size_t> GetCursorIndex(SystemCursorShape shape) noexcept
{
    switch (shape)
    {
    case SystemCursorShape::Default:
        return 0;
    case SystemCursorShape::TextInput:
        return 1;
    case SystemCursorShape::Move:
        return 2;
    case SystemCursorShape::ResizeNorthSouth:
        return 3;
    case SystemCursorShape::ResizeEastWest:
        return 4;
    case SystemCursorShape::ResizeNortheastSouthwest:
        return 5;
    case SystemCursorShape::ResizeNorthwestSoutheast:
        return 6;
    case SystemCursorShape::Pointer:
        return 7;
    case SystemCursorShape::Wait:
        return 8;
    case SystemCursorShape::Progress:
        return 9;
    case SystemCursorShape::NotAllowed:
        return 10;
    }

    return std::nullopt;
}

} // namespace

namespace detail
{
core::VoidResult PlatformRuntimeState::SetMouseCapture(bool enabled)
{
    VerifyOwnerThread("mouse capture update");
    if (!m_backend->SupportsGlobalMouse())
    {
        if (!enabled)
        {
            return core::VoidResult::Success();
        }

        return core::VoidResult::FromError(core::Error{
            kUnsupportedCode, "Global mouse capture is unsupported by the active video driver."});
    }

    return m_backend->SetMouseCapture(enabled);
}

core::Result<LogicalPoint> PlatformRuntimeState::GetGlobalMousePosition() const
{
    VerifyOwnerThread("global mouse-position query");
    if (!m_backend->SupportsGlobalMouse())
    {
        return core::Result<LogicalPoint>::FromError(core::Error{
            kUnsupportedCode, "Global mouse position is unsupported by the active video driver."});
    }

    const MousePosition backendPosition = m_backend->GetGlobalMousePosition();
    const LogicalPoint position{backendPosition.x, backendPosition.y};
    if (!IsValid(position))
    {
        return core::Result<LogicalPoint>::FromError(core::Error{
            kBackendFailureCode, "SDL_GetGlobalMouseState returned non-finite coordinates."});
    }

    return position;
}

core::VoidResult PlatformRuntimeState::SetSystemCursor(SystemCursorShape shape)
{
    VerifyOwnerThread("system cursor update");
    const std::optional<std::size_t> cursorIndex = GetCursorIndex(shape);
    if (!cursorIndex.has_value())
    {
        return core::VoidResult::FromError(
            core::Error{kInvalidArgumentCode, "System cursor shape is invalid."});
    }

    CursorHandle& cursor = m_systemCursors[*cursorIndex];
    if (!cursor.IsValid())
    {
        auto cursorResult = m_backend->CreateSystemCursor(shape);
        RETURN_ERROR_IF_FAILED(cursorResult);
        cursor = std::move(cursorResult).GetValue();
    }

    return m_backend->SetCursor(cursor);
}

core::VoidResult PlatformRuntimeState::ShowCursor()
{
    VerifyOwnerThread("cursor visibility update");
    if (m_backend->IsCursorVisible())
    {
        return core::VoidResult::Success();
    }

    return m_backend->ShowCursor();
}

core::VoidResult PlatformRuntimeState::HideCursor()
{
    VerifyOwnerThread("cursor visibility update");
    if (!m_backend->IsCursorVisible())
    {
        return core::VoidResult::Success();
    }

    return m_backend->HideCursor();
}

bool PlatformRuntimeState::IsCursorVisible() const
{
    VerifyOwnerThread("cursor visibility query");
    return m_backend->IsCursorVisible();
}

void PlatformRuntimeState::DestroySystemCursors() noexcept
{
    for (CursorHandle& cursor : m_systemCursors)
    {
        if (cursor.IsValid())
        {
            m_backend->DestroyCursor(cursor);
            cursor = CursorHandle{};
        }
    }
}

core::VoidResult WindowImpl::SetMouseGrab(bool grabbed)
{
    VerifyUsable("mouse-grab update");
    return m_backend.SetMouseGrab(m_backendWindow, grabbed);
}

bool WindowImpl::IsMouseGrabbed() const
{
    VerifyUsable("mouse-grab query");
    return m_backend.IsMouseGrabbed(m_backendWindow);
}

core::VoidResult WindowImpl::SetRelativeMouseMode(bool enabled)
{
    VerifyUsable("relative mouse-mode update");
    return m_backend.SetRelativeMouseMode(m_backendWindow, enabled);
}

bool WindowImpl::IsRelativeMouseModeEnabled() const
{
    VerifyUsable("relative mouse-mode query");
    return m_backend.IsRelativeMouseModeEnabled(m_backendWindow);
}
} // namespace detail

core::VoidResult PlatformRuntime::SetMouseCapture(bool enabled)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->SetMouseCapture(enabled);
}

core::Result<LogicalPoint> PlatformRuntime::GetGlobalMousePosition() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->GetGlobalMousePosition();
}

core::VoidResult PlatformRuntime::SetSystemCursor(SystemCursorShape shape)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->SetSystemCursor(shape);
}

core::VoidResult PlatformRuntime::ShowCursor()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->ShowCursor();
}

core::VoidResult PlatformRuntime::HideCursor()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->HideCursor();
}

bool PlatformRuntime::IsCursorVisible() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->IsCursorVisible();
}

core::VoidResult Window::SetMouseGrab(bool grabbed)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->SetMouseGrab(grabbed);
}

bool Window::IsMouseGrabbed() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsMouseGrabbed();
}

core::VoidResult Window::SetRelativeMouseMode(bool enabled)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->SetRelativeMouseMode(enabled);
}

bool Window::IsRelativeMouseModeEnabled() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsRelativeMouseModeEnabled();
}
} // namespace pond::platform
