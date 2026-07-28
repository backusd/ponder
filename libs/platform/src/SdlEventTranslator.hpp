#pragma once

#include <ponder/platform/PlatformEvent.hpp>

#include <optional>

#include "SdlRuntimeTypes.hpp"

union SDL_Event;

namespace ponder::platform::detail
{
[[nodiscard]] std::optional<PlatformEvent> TranslateSdlEvent(const SDL_Event& event, const EventTranslationContext& context);
} // namespace ponder::platform::detail
