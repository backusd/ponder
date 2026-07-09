#include <ponder/platform/PlatformRuntime.hpp>

#include "PlatformRuntimeState.hpp"
#include "SdlError.hpp"
#include "StringValidation.hpp"

#include <ponder/core/Assert.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/platform/PlatformError.hpp>

#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace pond::platform
{
namespace
{
constexpr core::ErrorCode kInvalidArgumentCode =
    ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kBackendFailureCode =
    ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr core::ErrorCode kUnsupportedCode =
    ToErrorCode(PlatformErrorCode::Unsupported);
constexpr std::string_view kMissingSdlError{
    "SDL did not provide an error message"};

[[nodiscard]] core::Error MakeUnsupportedClipboardError(
    std::source_location location = std::source_location::current())
{
    return core::Error{
        kUnsupportedCode,
        "Text clipboard operations are unsupported by the active video driver.",
        location};
}

[[nodiscard]] core::Error MakeCapturedSdlFailure(
    std::string_view operation, std::string_view objectContext,
    std::string_view capturedError,
    std::source_location location = std::source_location::current())
{
    const std::string_view errorText =
        !capturedError.empty() ? capturedError : kMissingSdlError;
    std::string message;
    message.reserve(operation.size() + objectContext.size() + errorText.size() + 16);
    message.append(operation);
    message.append(" failed");
    if (!objectContext.empty())
    {
        message.append(" (");
        message.append(objectContext);
        message.push_back(')');
    }
    message.append(": ");
    message.append(errorText);
    return core::Error{kBackendFailureCode, std::move(message), location};
}

[[nodiscard]] core::VoidResult ValidateNullTerminatedUtf8(
    std::string_view text, std::string_view description)
{
    if (!detail::IsValidUtf8WithoutEmbeddedNull(text))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode,
            std::string{description} +
                " must be UTF-8 without embedded nulls."});
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
        return core::Result<std::string>::FromError(
            MakeUnsupportedClipboardError());
    }

    const BackendClipboardTextResult backendText =
        m_backend.getClipboardText(m_backend.context);
    if (backendText.text == nullptr)
    {
        return core::Result<std::string>::FromError(MakeCapturedSdlFailure(
            "SDL_GetClipboardText", "clipboard text", backendText.errorText));
    }

    auto freeText = core::MakeScopeExit(
        [this, text = backendText.text]() noexcept
        {
            m_backend.freeClipboardText(m_backend.context, text);
        });

    const std::string text{backendText.text};
    if (text.empty() && !backendText.errorText.empty())
    {
        return core::Result<std::string>::FromError(MakeCapturedSdlFailure(
            "SDL_GetClipboardText", "clipboard text", backendText.errorText));
    }

    return text;
}

core::VoidResult PlatformRuntimeState::SetClipboardText(std::string_view text)
{
    VerifyOwnerThread("clipboard text update");
    core::VoidResult validation =
        ValidateNullTerminatedUtf8(text, "Clipboard text");
    if (!validation.HasValue())
    {
        return validation;
    }
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
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode, "External URI must be non-empty."});
    }

    core::VoidResult validation =
        ValidateNullTerminatedUtf8(uri, "External URI");
    if (!validation.HasValue())
    {
        return validation;
    }

    const std::string ownedUri{uri};
    if (!m_backend.openExternalUri(m_backend.context, ownedUri.c_str()))
    {
        return core::VoidResult::FromError(detail::CaptureSdlFailure(
            kBackendFailureCode, "SDL_OpenURL", "external URI"));
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