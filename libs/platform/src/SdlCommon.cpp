#include "SdlCommon.hpp"

#include <ponder/core/Assert.hpp>

#include <format>

namespace pond::platform::detail
{
SDL_Window* ToSdlWindow(BackendWindowHandle window) noexcept
{
    PONDER_VERIFY(window.IsValid(), "Cannot use an invalid backend window handle");
    return reinterpret_cast<SDL_Window*>(window.GetValue());
}

BackendWindowHandle ToBackendWindowHandle(SDL_Window* window) noexcept
{
    return BackendWindowHandle{reinterpret_cast<BackendWindowHandle::ValueType>(window)};
}

std::string GetBackendWindowContext(BackendWindowHandle window)
{
    return std::format("backend window {}", window);
}
} // namespace pond::platform::detail
