#include <ponder/core/Assert.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Window.hpp>

#include "PlatformRuntimeBackend.hpp"
#include "WindowImpl.hpp"

namespace pond::platform
{
namespace
{
constexpr core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kUnsupportedCode = ToErrorCode(PlatformErrorCode::Unsupported);
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
    case WindowGraphicsCompatibility::Metal:
        return core::Result<NativeWindowHandle>::FromError(core::Error{
            kUnsupportedCode,
            "Native window handles are not defined for Metal window graphics compatibility."});
    case WindowGraphicsCompatibility::Default:
        return core::Result<NativeWindowHandle>::FromError(
            core::Error{kInvalidArgumentCode,
                        "Native window handles require Vulkan window graphics compatibility."});
    default:
        return core::Result<NativeWindowHandle>::FromError(
            core::Error{kInvalidArgumentCode, "Window graphics compatibility is invalid."});
    }

    return m_backend.GetNativeHandle(m_backendWindow);
}
} // namespace detail

core::Result<NativeWindowHandle> Window::GetNativeHandle() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetNativeHandle();
}
} // namespace pond::platform
