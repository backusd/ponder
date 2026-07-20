#include <ponder/core/Assert.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/platform/Window.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "PlatformRuntimeState.hpp"
#include "SdlError.hpp"
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

[[nodiscard]] std::string_view GetCursorName(SystemCursorShape shape) noexcept
{
    switch (shape)
    {
    case SystemCursorShape::Default:
        return "default";
    case SystemCursorShape::TextInput:
        return "text input";
    case SystemCursorShape::Move:
        return "move";
    case SystemCursorShape::ResizeNorthSouth:
        return "north-south resize";
    case SystemCursorShape::ResizeEastWest:
        return "east-west resize";
    case SystemCursorShape::ResizeNortheastSouthwest:
        return "northeast-southwest resize";
    case SystemCursorShape::ResizeNorthwestSoutheast:
        return "northwest-southeast resize";
    case SystemCursorShape::Pointer:
        return "pointer";
    case SystemCursorShape::Wait:
        return "wait";
    case SystemCursorShape::Progress:
        return "progress";
    case SystemCursorShape::NotAllowed:
        return "not allowed";
    }

    return "invalid";
}

[[nodiscard]] core::VoidResult MakeUnsupportedWindowMouseResult(std::string_view operation,
                                                                std::string_view context)
{
    return core::VoidResult::FromError(
        core::Error{kUnsupportedCode,
                    std::string{operation} + " is unsupported for " + std::string{context} + "."});
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

    if (!m_backend->SetMouseCapture(enabled))
    {
        return core::VoidResult::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_CaptureMouse", enabled ? "enable" : "disable"));
    }

    return core::VoidResult::Success();
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
        cursor = m_backend->CreateSystemCursor(shape);
        if (!cursor.IsValid())
        {
            return core::VoidResult::FromError(CaptureSdlFailure(
                kBackendFailureCode, "SDL_CreateSystemCursor", GetCursorName(shape)));
        }
    }

    if (!m_backend->SetCursor(cursor))
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_SetCursor", GetCursorName(shape)));
    }

    return core::VoidResult::Success();
}

core::VoidResult PlatformRuntimeState::ShowCursor()
{
    VerifyOwnerThread("cursor visibility update");
    if (m_backend->IsCursorVisible())
    {
        return core::VoidResult::Success();
    }

    if (!m_backend->ShowCursor())
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_ShowCursor"));
    }

    return core::VoidResult::Success();
}

core::VoidResult PlatformRuntimeState::HideCursor()
{
    VerifyOwnerThread("cursor visibility update");
    if (!m_backend->IsCursorVisible())
    {
        return core::VoidResult::Success();
    }

    if (!m_backend->HideCursor())
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_HideCursor"));
    }

    return core::VoidResult::Success();
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
    const std::string_view context = GetErrorContext();
    if (!m_backend.setMouseGrab(m_backend.context, m_nativeWindow, grabbed))
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_SetWindowMouseGrab", context));
    }

    return core::VoidResult::Success();
}

bool WindowImpl::IsMouseGrabbed() const
{
    VerifyUsable("mouse-grab query");
    return m_backend.isMouseGrabbed(m_backend.context, m_nativeWindow);
}

core::VoidResult WindowImpl::SetRelativeMouseMode(bool enabled)
{
    VerifyUsable("relative mouse-mode update");
    if (m_backend.isRelativeMouseModeEnabled(m_backend.context, m_nativeWindow) == enabled)
    {
        return core::VoidResult::Success();
    }

    const std::string_view context = GetErrorContext();
    if (!m_backend.setRelativeMouseMode(m_backend.context, m_nativeWindow, enabled))
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_SetWindowRelativeMouseMode", context));
    }

    if (m_backend.isRelativeMouseModeEnabled(m_backend.context, m_nativeWindow) != enabled)
    {
        return MakeUnsupportedWindowMouseResult("SDL_SetWindowRelativeMouseMode", context);
    }

    return core::VoidResult::Success();
}

bool WindowImpl::IsRelativeMouseModeEnabled() const
{
    VerifyUsable("relative mouse-mode query");
    return m_backend.isRelativeMouseModeEnabled(m_backend.context, m_nativeWindow);
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
