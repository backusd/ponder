#include <ponder/core/Assert.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <SDL3/SDL_events.h>
#include <cstdint>
#include <optional>

#include "PlatformRuntimeState.hpp"
#include "SdlEventTranslator.hpp"
#include "WindowImpl.hpp"

namespace pond::platform
{
namespace detail
{
namespace
{
struct RuntimeEventTranslationContext final
{
    PlatformRuntimeState* runtime{};
    std::uint32_t removedBackendDisplayId{};
    std::optional<DisplayId> removedDisplayId;
};

[[nodiscard]] std::optional<WindowId> ResolveWindowId(void* context, std::uint32_t backendWindowId)
{
    auto& routing = *static_cast<RuntimeEventTranslationContext*>(context);
    return routing.runtime->FindWindowId(backendWindowId);
}

[[nodiscard]] std::optional<DisplayId> ResolveDisplayId(void* context,
                                                        std::uint32_t backendDisplayId)
{
    auto& routing = *static_cast<RuntimeEventTranslationContext*>(context);
    if (routing.removedDisplayId.has_value() && routing.removedBackendDisplayId == backendDisplayId)
    {
        return routing.removedDisplayId;
    }

    return routing.runtime->FindConnectedDisplayId(backendDisplayId);
}
} // namespace

void PlatformRuntimeState::ObserveWindowShownEvent(std::uint32_t backendWindowId)
{
    const auto window = m_windowsByBackendId.find(backendWindowId);
    if (window != m_windowsByBackendId.end())
    {
        PONDER_VERIFY(window->second.window != nullptr,
                      "Registered backend window {} has no owning window", backendWindowId);
        window->second.window->ObserveShownEvent();
    }
}

std::optional<PlatformEvent> PlatformRuntimeState::PollEvent()
{
    VerifyOwnerThread("event polling");

    if (std::optional<PlatformEvent> dialogCompletion = PollDialogCompletion();
        dialogCompletion.has_value())
    {
        return dialogCompletion;
    }

    while (true)
    {
        SDL_Event event{};
        if (!m_backend.pollEvent(m_backend.context, &event))
        {
            return std::nullopt;
        }

        RuntimeEventTranslationContext routing{this};
        if (event.type == SDL_EVENT_DISPLAY_ADDED)
        {
            static_cast<void>(
                ConnectDisplayFromEvent(static_cast<std::uint32_t>(event.display.displayID)));
        }
        else if (event.type == SDL_EVENT_DISPLAY_REMOVED)
        {
            routing.removedBackendDisplayId = static_cast<std::uint32_t>(event.display.displayID);
            routing.removedDisplayId = FindDisplayIdForRemoval(routing.removedBackendDisplayId);
        }
        else if (event.type == SDL_EVENT_DISPLAY_MOVED ||
                 event.type == SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED ||
                 event.type == SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED ||
                 event.type == SDL_EVENT_DISPLAY_ORIENTATION ||
                 event.type == SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED ||
                 event.type == SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED)
        {
            ReconcileDisplayFromEvent(static_cast<std::uint32_t>(event.display.displayID));
        }

        if (event.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED)
        {
            ReconcileDisplayFromEvent(static_cast<std::uint32_t>(event.window.data1));
        }

        if (event.type == SDL_EVENT_WINDOW_SHOWN)
        {
            ObserveWindowShownEvent(static_cast<std::uint32_t>(event.window.windowID));
        }

        const EventTranslationContext translationContext{&routing, ResolveWindowId,
                                                         ResolveDisplayId};
        std::optional<PlatformEvent> translated = TranslateSdlEvent(event, translationContext);

        if (event.type == SDL_EVENT_DISPLAY_REMOVED && routing.removedDisplayId.has_value())
        {
            DisconnectDisplayFromEvent(routing.removedBackendDisplayId);
        }

        if (translated.has_value())
        {
            return translated;
        }
    }
}
} // namespace detail

std::optional<PlatformEvent> PlatformRuntime::PollEvent()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->PollEvent();
}
} // namespace pond::platform
