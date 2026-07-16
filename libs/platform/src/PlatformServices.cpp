#include <ponder/core/Assert.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/core/String.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <source_location>
#include <string>
#include <string_view>

#include "PlatformRuntimeState.hpp"
#include "SdlError.hpp"

namespace pond::platform
{
namespace
{
constexpr core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr core::ErrorCode kUnsupportedCode = ToErrorCode(PlatformErrorCode::Unsupported);

[[nodiscard]] core::Error MakeUnsupportedClipboardError(
    std::source_location location = std::source_location::current())
{
    return core::Error{kUnsupportedCode,
                       "Text clipboard operations are unsupported by the active video driver.",
                       location};
}

[[nodiscard]] core::VoidResult ValidateNullTerminatedUtf8(std::string_view text,
                                                          std::string_view description)
{
    if (!core::IsValidUtf8WithoutEmbeddedNull(text))
    {
        return core::VoidResult::FromError(
            core::Error{kInvalidArgumentCode,
                        std::string{description} + " must be UTF-8 without embedded nulls."});
    }

    return core::VoidResult::Success();
}
} // namespace

namespace detail
{
core::Result<std::string> PlatformRuntimeState::GetClipboardText() const
{
    VerifyOwnerThread("clipboard text query");
    if (!m_backend.supportsClipboardText(m_backend.context))
    {
        return core::Result<std::string>::FromError(MakeUnsupportedClipboardError());
    }

    const BackendClipboardTextResult backendText = m_backend.getClipboardText(m_backend.context);
    if (backendText.text == nullptr)
    {
        return core::Result<std::string>::FromError(detail::CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetClipboardText", "clipboard text", backendText.errorText));
    }

    auto freeText = core::MakeScopeExit(
        [this, text = backendText.text]() noexcept
        {
            m_backend.freeClipboardText(m_backend.context, text);
        });

    const std::string text{backendText.text};
    if (text.empty() && !backendText.errorText.empty())
    {
        return core::Result<std::string>::FromError(detail::CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetClipboardText", "clipboard text", backendText.errorText));
    }

    return text;
}

core::VoidResult PlatformRuntimeState::SetClipboardText(std::string_view text)
{
    VerifyOwnerThread("clipboard text update");
    core::VoidResult validation = ValidateNullTerminatedUtf8(text, "Clipboard text");
    RETURN_ERROR_IF_FAILED(validation);
    if (!m_backend.supportsClipboardText(m_backend.context))
    {
        return core::VoidResult::FromError(MakeUnsupportedClipboardError());
    }

    const std::string ownedText{text};
    if (!m_backend.setClipboardText(m_backend.context, ownedText.c_str()))
    {
        return core::VoidResult::FromError(detail::CaptureSdlFailure(
            kBackendFailureCode, "SDL_SetClipboardText", "clipboard text"));
    }

    return core::VoidResult::Success();
}

core::VoidResult PlatformRuntimeState::OpenExternalUri(std::string_view uri)
{
    VerifyOwnerThread("external URI opening");
    if (uri.empty())
    {
        return core::VoidResult::FromError(
            core::Error{kInvalidArgumentCode, "External URI must be non-empty."});
    }

    core::VoidResult validation = ValidateNullTerminatedUtf8(uri, "External URI");
    RETURN_ERROR_IF_FAILED(validation);

    const std::string ownedUri{uri};
    if (!m_backend.openExternalUri(m_backend.context, ownedUri.c_str()))
    {
        return core::VoidResult::FromError(
            detail::CaptureSdlFailure(kBackendFailureCode, "SDL_OpenURL", "external URI"));
    }

    return core::VoidResult::Success();
}
} // namespace detail

core::Result<std::string> PlatformRuntime::GetClipboardText() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->GetClipboardText();
}

core::VoidResult PlatformRuntime::SetClipboardText(std::string_view text)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->SetClipboardText(text);
}

core::VoidResult PlatformRuntime::OpenExternalUri(std::string_view uri)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->OpenExternalUri(uri);
}
} // namespace pond::platform
