#pragma once

#include <string>

#include "IPlatformWindowBackend.hpp"

struct SDL_Window;

namespace ponder::platform::detail
{
[[nodiscard]] SDL_Window* ToSdlWindow(BackendWindowHandle window) noexcept;
[[nodiscard]] BackendWindowHandle ToBackendWindowHandle(SDL_Window* window) noexcept;
[[nodiscard]] std::string GetBackendWindowContext(BackendWindowHandle window);
} // namespace ponder::platform::detail
