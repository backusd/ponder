#include <ponder/core/Assert.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Window.hpp>
#include <ponder/platform/WindowState.hpp>

#include <string>
#include <string_view>
#include <utility>

#include "PlatformRuntimeBackend.hpp"
#include "SdlError.hpp"
#include "WindowImpl.hpp"

namespace pond::platform
{
namespace
{
constexpr core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr core::ErrorCode kUnsupportedCode = ToErrorCode(PlatformErrorCode::Unsupported);

[[nodiscard]] core::Error MakeUnsupportedError(std::string message)
{
    return core::Error{kUnsupportedCode, std::move(message)};
}

[[nodiscard]] core::Result<WindowState> DecodeWindowState(
    const detail::BackendWindowProperties& properties, std::string_view context,
    const std::optional<WindowState>& hiddenStateRequest)
{
    if (properties.hidden && hiddenStateRequest.has_value())
    {
        return *hiddenStateRequest;
    }
    if (properties.minimized && properties.maximized)
    {
        return core::Result<WindowState>::FromError(
            core::Error{kBackendFailureCode,
                        "SDL_GetWindowFlags returned contradictory minimized and maximized "
                        "state for " +
                            std::string{context} + "."});
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

[[nodiscard]] core::VoidResult ConvertOperationResult(detail::BackendWindowOperationResult result,
                                                      std::string_view backendOperation,
                                                      std::string_view context,
                                                      bool unsupportedIsSdlFailure)
{
    switch (result)
    {
    case detail::BackendWindowOperationResult::Succeeded:
        return core::VoidResult::Success();
    case detail::BackendWindowOperationResult::Unsupported:
        if (unsupportedIsSdlFailure)
        {
            return core::VoidResult::FromError(
                detail::CaptureSdlFailure(kUnsupportedCode, backendOperation, context));
        }
        return core::VoidResult::FromError(MakeUnsupportedError(
            std::string{backendOperation} + " is unsupported for " + std::string{context} + "."));
    case detail::BackendWindowOperationResult::Failed:
        return core::VoidResult::FromError(
            detail::CaptureSdlFailure(kBackendFailureCode, backendOperation, context));
    }

    return core::VoidResult::FromError(
        core::Error{kBackendFailureCode, "The backend returned an unknown result for " +
                                             std::string{backendOperation} + " on " +
                                             std::string{context} + "."});
}
} // namespace

namespace detail
{
core::Result<BackendWindowProperties> WindowImpl::GetProperties(std::string_view operation) const
{
    VerifyUsable(operation);
    BackendWindowProperties properties;
    const std::string_view context = GetErrorContext();
    if (!m_backend.getProperties(m_backend.context, m_nativeWindow, &properties))
    {
        return core::Result<BackendWindowProperties>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetWindowFlags", context));
    }
    return properties;
}

core::Result<WindowPresentation> WindowImpl::GetPresentation() const
{
    auto properties = GetProperties("presentation query");
    RETURN_ERROR_IF_FAILED(properties);
    return properties.GetValue().desktopFullscreen ? WindowPresentation::DesktopFullscreen
                                                   : WindowPresentation::Windowed;
}

core::VoidResult WindowImpl::SetPresentation(WindowPresentation presentation)
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
        return core::VoidResult::FromError(
            core::Error{kInvalidArgumentCode, "Window presentation is invalid."});
    }

    auto properties = GetProperties("presentation update");
    RETURN_ERROR_IF_FAILED(properties);
    const WindowPresentation observedPresentation = properties.GetValue().desktopFullscreen
                                                        ? WindowPresentation::DesktopFullscreen
                                                        : WindowPresentation::Windowed;
    if (m_pendingPresentationRequest == observedPresentation)
    {
        m_pendingPresentationRequest.reset();
    }
    if (m_pendingPresentationRequest.has_value() ? *m_pendingPresentationRequest == presentation
                                                 : observedPresentation == presentation)
    {
        return core::VoidResult::Success();
    }

    const std::string_view context = GetErrorContext();
    if (fullscreen)
    {
        core::VoidResult modeResult = ConvertOperationResult(
            m_backend.setFullscreenModeToDesktop(m_backend.context, m_nativeWindow),
            "SDL_SetWindowFullscreenMode", context, false);
        RETURN_ERROR_IF_FAILED(modeResult);
    }

    core::VoidResult presentationResult = ConvertOperationResult(
        m_backend.setFullscreen(m_backend.context, m_nativeWindow, fullscreen),
        "SDL_SetWindowFullscreen", context, false);
    if (presentationResult.HasValue())
    {
        m_pendingPresentationRequest = presentation;
    }
    return presentationResult;
}

core::Result<WindowDecoration> WindowImpl::GetDecoration() const
{
    auto properties = GetProperties("decoration query");
    RETURN_ERROR_IF_FAILED(properties);
    return properties.GetValue().borderless ? WindowDecoration::Borderless
                                            : WindowDecoration::System;
}

core::VoidResult WindowImpl::SetDecoration(WindowDecoration decoration)
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
        return core::VoidResult::FromError(
            core::Error{kInvalidArgumentCode, "Window decoration is invalid."});
    }

    auto properties = GetProperties("decoration update");
    RETURN_ERROR_IF_FAILED(properties);
    if (properties.GetValue().borderless == borderless)
    {
        return core::VoidResult::Success();
    }
    if (properties.GetValue().desktopFullscreen)
    {
        return core::VoidResult::FromError(
            MakeUnsupportedError("Window decoration cannot change while window " +
                                 std::to_string(m_id.GetValue()) + " is fullscreen."));
    }

    const std::string_view context = GetErrorContext();
    return ConvertOperationResult(
        m_backend.setBordered(m_backend.context, m_nativeWindow, !borderless),
        "SDL_SetWindowBordered", context, false);
}

core::Result<::pond::platform::WindowState> WindowImpl::GetState() const
{
    auto properties = GetProperties("state query");
    RETURN_ERROR_IF_FAILED(properties);
    return DecodeWindowState(properties.GetValue(), GetErrorContext(), m_hiddenStateRequest);
}

core::VoidResult WindowImpl::Minimize()
{
    auto properties = GetProperties("minimize");
    RETURN_ERROR_IF_FAILED(properties);
    SynchronizeStateRequestVisibility(properties.GetValue().hidden);
    auto state = DecodeWindowState(properties.GetValue(), GetErrorContext(), m_hiddenStateRequest);
    RETURN_ERROR_IF_FAILED(state);
    if (!properties.GetValue().hidden && m_pendingVisibleStateRequest == state.GetValue())
    {
        m_pendingVisibleStateRequest.reset();
    }
    const std::optional<::pond::platform::WindowState>& pendingState =
        properties.GetValue().hidden ? m_hiddenStateRequest : m_pendingVisibleStateRequest;
    if (pendingState.has_value() ? *pendingState == ::pond::platform::WindowState::Minimized
                                 : state.GetValue() == ::pond::platform::WindowState::Minimized)
    {
        return core::VoidResult::Success();
    }

    const std::string_view context = GetErrorContext();
    if (properties.GetValue().hidden &&
        state.GetValue() == ::pond::platform::WindowState::Maximized)
    {
        core::VoidResult restoreResult =
            ConvertOperationResult(m_backend.restore(m_backend.context, m_nativeWindow),
                                   "SDL_RestoreWindow", context, true);
        RETURN_ERROR_IF_FAILED(restoreResult);
        RecordStateRequest(::pond::platform::WindowState::Normal, true);
    }
    core::VoidResult minimizeResult = ConvertOperationResult(
        m_backend.minimize(m_backend.context, m_nativeWindow), "SDL_MinimizeWindow", context, true);
    if (minimizeResult.HasValue())
    {
        RecordStateRequest(::pond::platform::WindowState::Minimized, properties.GetValue().hidden);
    }
    return minimizeResult;
}

core::VoidResult WindowImpl::Maximize()
{
    auto properties = GetProperties("maximize");
    RETURN_ERROR_IF_FAILED(properties);
    SynchronizeStateRequestVisibility(properties.GetValue().hidden);
    auto state = DecodeWindowState(properties.GetValue(), GetErrorContext(), m_hiddenStateRequest);
    RETURN_ERROR_IF_FAILED(state);
    if (!properties.GetValue().hidden && m_pendingVisibleStateRequest == state.GetValue())
    {
        m_pendingVisibleStateRequest.reset();
    }
    const std::optional<::pond::platform::WindowState>& pendingState =
        properties.GetValue().hidden ? m_hiddenStateRequest : m_pendingVisibleStateRequest;
    if (pendingState.has_value() ? *pendingState == ::pond::platform::WindowState::Maximized
                                 : state.GetValue() == ::pond::platform::WindowState::Maximized)
    {
        return core::VoidResult::Success();
    }
    if (!properties.GetValue().resizable)
    {
        return core::VoidResult::FromError(
            MakeUnsupportedError("A non-resizable window cannot be maximized."));
    }

    const std::string_view context = GetErrorContext();
    if (properties.GetValue().hidden &&
        state.GetValue() == ::pond::platform::WindowState::Minimized)
    {
        core::VoidResult restoreResult =
            ConvertOperationResult(m_backend.restore(m_backend.context, m_nativeWindow),
                                   "SDL_RestoreWindow", context, true);
        RETURN_ERROR_IF_FAILED(restoreResult);
        RecordStateRequest(::pond::platform::WindowState::Normal, true);
    }
    core::VoidResult maximizeResult = ConvertOperationResult(
        m_backend.maximize(m_backend.context, m_nativeWindow), "SDL_MaximizeWindow", context, true);
    if (maximizeResult.HasValue())
    {
        RecordStateRequest(::pond::platform::WindowState::Maximized, properties.GetValue().hidden);
    }
    return maximizeResult;
}

core::VoidResult WindowImpl::Restore()
{
    auto properties = GetProperties("restore");
    RETURN_ERROR_IF_FAILED(properties);
    SynchronizeStateRequestVisibility(properties.GetValue().hidden);
    auto state = DecodeWindowState(properties.GetValue(), GetErrorContext(), m_hiddenStateRequest);
    RETURN_ERROR_IF_FAILED(state);
    if (!properties.GetValue().hidden && m_pendingVisibleStateRequest == state.GetValue())
    {
        m_pendingVisibleStateRequest.reset();
    }
    const std::optional<::pond::platform::WindowState>& pendingState =
        properties.GetValue().hidden ? m_hiddenStateRequest : m_pendingVisibleStateRequest;
    if (pendingState.has_value() ? *pendingState == ::pond::platform::WindowState::Normal
                                 : state.GetValue() == ::pond::platform::WindowState::Normal)
    {
        return core::VoidResult::Success();
    }

    const std::string_view context = GetErrorContext();
    core::VoidResult restoreResult = ConvertOperationResult(
        m_backend.restore(m_backend.context, m_nativeWindow), "SDL_RestoreWindow", context, true);
    if (restoreResult.HasValue())
    {
        RecordStateRequest(::pond::platform::WindowState::Normal, properties.GetValue().hidden);
    }
    return restoreResult;
}

core::Result<bool> WindowImpl::IsVisible() const
{
    auto properties = GetProperties("visibility query");
    RETURN_ERROR_IF_FAILED(properties);
    return !properties.GetValue().hidden;
}

core::Result<bool> WindowImpl::IsResizable() const
{
    auto properties = GetProperties("resizability query");
    RETURN_ERROR_IF_FAILED(properties);
    return properties.GetValue().resizable;
}

core::VoidResult WindowImpl::SetResizable(bool resizable)
{
    auto properties = GetProperties("resizability update");
    RETURN_ERROR_IF_FAILED(properties);
    if (properties.GetValue().resizable == resizable)
    {
        return core::VoidResult::Success();
    }
    if (properties.GetValue().desktopFullscreen)
    {
        return core::VoidResult::FromError(
            MakeUnsupportedError("Window resizability cannot change while window " +
                                 std::to_string(m_id.GetValue()) + " is fullscreen."));
    }

    const std::string_view context = GetErrorContext();
    return ConvertOperationResult(
        m_backend.setResizable(m_backend.context, m_nativeWindow, resizable),
        "SDL_SetWindowResizable", context, false);
}

core::Result<bool> WindowImpl::IsFocused() const
{
    auto properties = GetProperties("focus query");
    RETURN_ERROR_IF_FAILED(properties);
    return properties.GetValue().inputFocus;
}

core::Result<bool> WindowImpl::IsAlwaysOnTop() const
{
    auto properties = GetProperties("always-on-top query");
    RETURN_ERROR_IF_FAILED(properties);
    return properties.GetValue().alwaysOnTop;
}

core::VoidResult WindowImpl::SetAlwaysOnTop(bool alwaysOnTop)
{
    auto properties = GetProperties("always-on-top update");
    RETURN_ERROR_IF_FAILED(properties);
    if (properties.GetValue().alwaysOnTop == alwaysOnTop)
    {
        return core::VoidResult::Success();
    }

    const std::string_view context = GetErrorContext();
    return ConvertOperationResult(
        m_backend.setAlwaysOnTop(m_backend.context, m_nativeWindow, alwaysOnTop),
        "SDL_SetWindowAlwaysOnTop", context, false);
}
} // namespace detail

core::Result<WindowPresentation> Window::GetPresentation() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetPresentation();
}

core::VoidResult Window::SetPresentation(WindowPresentation presentation)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->SetPresentation(presentation);
}

core::Result<WindowDecoration> Window::GetDecoration() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetDecoration();
}

core::VoidResult Window::SetDecoration(WindowDecoration decoration)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->SetDecoration(decoration);
}

core::Result<WindowState> Window::GetState() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetState();
}

core::VoidResult Window::Minimize()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->Minimize();
}

core::VoidResult Window::Maximize()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->Maximize();
}

core::VoidResult Window::Restore()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->Restore();
}

core::Result<bool> Window::IsVisible() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsVisible();
}

core::Result<bool> Window::IsResizable() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsResizable();
}

core::VoidResult Window::SetResizable(bool resizable)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->SetResizable(resizable);
}

core::Result<bool> Window::IsFocused() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsFocused();
}

core::Result<bool> Window::IsAlwaysOnTop() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->IsAlwaysOnTop();
}

core::VoidResult Window::SetAlwaysOnTop(bool alwaysOnTop)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->SetAlwaysOnTop(alwaysOnTop);
}
} // namespace pond::platform
