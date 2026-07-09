#pragma once

#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/PlatformEvent.hpp>

#include <cstdint>
#include <optional>

union SDL_Event;

namespace pond::platform::detail
{
struct EventTranslationContext final
{
    void* context{};
    std::optional<WindowId> (*resolveWindowId)(void* context,
                                                std::uint32_t backendWindowId){};
    std::optional<DisplayId> (*resolveDisplayId)(void* context,
                                                 std::uint32_t backendDisplayId){};
};

[[nodiscard]] std::optional<PlatformEvent> TranslateSdlEvent(
    const SDL_Event& event, const EventTranslationContext& context);
} // namespace pond::platform::detail
