#pragma once

#include <string>

#include "IPlatformWindowBackend.hpp"

struct SDL_Window;

namespace pond::platform::detail
{
[[nodiscard]] SDL_Window* ToSdlWindow(BackendWindowHandle window) noexcept;
[[nodiscard]] BackendWindowHandle ToBackendWindowHandle(SDL_Window* window) noexcept;
[[nodiscard]] std::string GetBackendWindowContext(BackendWindowHandle window);
} // namespace pond::platform::detail
