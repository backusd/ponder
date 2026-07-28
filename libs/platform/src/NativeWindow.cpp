#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Window.hpp>

#include <type_traits>
#include <variant>

#include "WindowImpl.hpp"

namespace ponder::platform
{
namespace
{
constexpr ponder::core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr ponder::core::ErrorCode kUnsupportedCode = ToErrorCode(PlatformErrorCode::Unsupported);

[[nodiscard]] bool IsCompleteNativeWindowHandle(const NativeWindowHandle& handle) noexcept
{
    return std::visit(
        [](const auto& nativeWindow) noexcept
        {
            using NativeWindow = std::remove_cvref_t<decltype(nativeWindow)>;
            if constexpr (std::is_same_v<NativeWindow, NativeWin32Window>)
            {
                return nativeWindow.instance != nullptr && nativeWindow.window != nullptr;
            }
            else if constexpr (std::is_same_v<NativeWindow, NativeX11Window>)
            {
                return nativeWindow.display != nullptr && nativeWindow.window != 0;
            }
            else
            {
                return nativeWindow.display != nullptr && nativeWindow.surface != nullptr;
            }
        },
        handle);
}
} // namespace

namespace detail
{
ponder::core::Result<NativeWindowHandle> WindowImpl::GetNativeHandle() const
{
    VerifyUsable("native window handle query");
    switch (m_graphicsCompatibility)
    {
    case WindowGraphicsCompatibility::Vulkan:
        break;
    case WindowGraphicsCompatibility::Metal:
    case WindowGraphicsCompatibility::Default:
        return ponder::core::Result<NativeWindowHandle>::FromError(ponder::core::Error{kInvalidArgumentCode,
                                                                                       "Native window handles require Vulkan window graphics "
                                                                                       "compatibility."});
    default:
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Window graphics compatibility is invalid.");
    }

    ponder::core::Result<NativeWindowHandle> result = m_backend.GetNativeHandle(m_backendWindow);
    if (!result.HasValue())
    {
        if (result.GetError().GetCode() == kUnsupportedCode)
        {
            return result;
        }

        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Native window backend returned an unexpected recoverable error for {}: {}",
                                 GetErrorContext(), result.GetError().GetMessage());
    }
    if (!IsCompleteNativeWindowHandle(result.GetValue()))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Native window backend returned an incomplete native handle for {}.",
                                 GetErrorContext());
    }

    return result;
}
} // namespace detail

ponder::core::Result<NativeWindowHandle> Window::GetNativeHandle() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetNativeHandle();
}
} // namespace ponder::platform
