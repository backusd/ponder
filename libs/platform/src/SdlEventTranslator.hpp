#pragma once

#include <ponder/platform/PlatformEvent.hpp>

#include <optional>

union SDL_Event;

namespace ponder::platform::detail
{
struct EventTranslationContext;

[[nodiscard]] std::optional<PlatformEvent> TranslateSdlEvent(const SDL_Event& event, ponder::core::Timestamp timestamp,
                                                             const EventTranslationContext& context);
} // namespace ponder::platform::detail
