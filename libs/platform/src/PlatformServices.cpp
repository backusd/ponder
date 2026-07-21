#include <ponder/core/Assert.hpp>
#include <ponder/core/String.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <source_location>
#include <string>
#include <string_view>

#include "PlatformRuntimeState.hpp"

namespace pond::platform
{
namespace
{
constexpr core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
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
    if (!m_backend->SupportsClipboardText())
    {
        return core::Result<std::string>::FromError(MakeUnsupportedClipboardError());
    }

    return m_backend->GetClipboardText();
}

core::VoidResult PlatformRuntimeState::SetClipboardText(std::string_view text)
{
    VerifyOwnerThread("clipboard text update");
    core::VoidResult validation = ValidateNullTerminatedUtf8(text, "Clipboard text");
    RETURN_ERROR_IF_FAILED(validation);
    if (!m_backend->SupportsClipboardText())
    {
        return core::VoidResult::FromError(MakeUnsupportedClipboardError());
    }

    return m_backend->SetClipboardText(text);
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

    return m_backend->OpenExternalUri(uri);
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
