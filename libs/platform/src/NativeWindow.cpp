#include <ponder/core/Assert.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Window.hpp>

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

[[nodiscard]] core::Error MakeNativeHandleError(core::ErrorCode code, std::string_view message,
                                                std::string_view context)
{
    return core::Error{code, std::string{message} + " Context: " + std::string{context} + "."};
}
} // namespace

namespace detail
{
core::Result<NativeWindowHandle> WindowImpl::GetNativeHandle() const
{
    VerifyUsable("native window handle query");
    switch (m_graphicsCompatibility)
    {
    case WindowGraphicsCompatibility::Vulkan:
        break;
    case WindowGraphicsCompatibility::Default:
        return core::Result<NativeWindowHandle>::FromError(
            core::Error{kInvalidArgumentCode,
                        "Native window handles require Vulkan window graphics compatibility."});
    default:
        return core::Result<NativeWindowHandle>::FromError(
            core::Error{kInvalidArgumentCode, "Window graphics compatibility is invalid."});
    }

    NativeWindowHandle handle;
    const BackendNativeWindowHandleResult result =
        m_backend.getNativeHandle(m_backend.context, m_nativeWindow, &m_cocoaMetalView, &handle);
    switch (result.status)
    {
    case BackendNativeWindowHandleStatus::Succeeded:
        return core::Result<NativeWindowHandle>::FromValue(std::move(handle));
    case BackendNativeWindowHandleStatus::Unsupported:
        return core::Result<NativeWindowHandle>::FromError(MakeNativeHandleError(
            kUnsupportedCode,
            result.message != nullptr ? std::string_view{result.message}
                                      : std::string_view{"Native window handles are unsupported."},
            GetErrorContext()));
    case BackendNativeWindowHandleStatus::Failed:
        if (result.captureSdlError)
        {
            return core::Result<NativeWindowHandle>::FromError(CaptureSdlFailure(
                kBackendFailureCode,
                result.operation != nullptr ? result.operation : "SDL native window handle query",
                GetErrorContext()));
        }
        return core::Result<NativeWindowHandle>::FromError(MakeNativeHandleError(
            kBackendFailureCode,
            result.message != nullptr ? std::string_view{result.message}
                                      : std::string_view{"Native window handle query failed."},
            GetErrorContext()));
    }

    return core::Result<NativeWindowHandle>::FromError(
        core::Error{kBackendFailureCode,
                    "The backend returned an unknown result for native window handle query."});
}
} // namespace detail

core::Result<NativeWindowHandle> Window::GetNativeHandle() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetNativeHandle();
}
} // namespace pond::platform
