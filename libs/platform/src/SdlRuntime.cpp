#include "SdlRuntime.hpp"

#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/core/Log.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/core/String.hpp>
#include <ponder/core/Timing.hpp>
#include <ponder/io/Path.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Runtime.hpp>

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_misc.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "SdlCommon.hpp"
#include "SdlError.hpp"
#include "SdlEventTranslator.hpp"
#include "WindowImpl.hpp"

namespace ponder::platform
{
namespace
{
constexpr std::string_view kLogCategory{"platform"};
constexpr ponder::core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr ponder::core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr ponder::core::ErrorCode kNotFoundCode = ToErrorCode(PlatformErrorCode::NotFound);
constexpr ponder::core::ErrorCode kUnsupportedCode = ToErrorCode(PlatformErrorCode::Unsupported);

[[nodiscard]] const char* GetMetadataPropertyName(detail::ApplicationMetadataProperty property) noexcept
{
    using detail::ApplicationMetadataProperty;
    switch (property)
    {
    case ApplicationMetadataProperty::Name:
        return SDL_PROP_APP_METADATA_NAME_STRING;
    case ApplicationMetadataProperty::Version:
        return SDL_PROP_APP_METADATA_VERSION_STRING;
    case ApplicationMetadataProperty::Identifier:
        return SDL_PROP_APP_METADATA_IDENTIFIER_STRING;
    }

    return nullptr;
}

[[nodiscard]] SDL_SystemCursor ToSdlSystemCursor(SystemCursorShape shape)
{
    switch (shape)
    {
    case SystemCursorShape::Default:
        return SDL_SYSTEM_CURSOR_DEFAULT;
    case SystemCursorShape::TextInput:
        return SDL_SYSTEM_CURSOR_TEXT;
    case SystemCursorShape::Move:
        return SDL_SYSTEM_CURSOR_MOVE;
    case SystemCursorShape::ResizeNorthSouth:
        return SDL_SYSTEM_CURSOR_NS_RESIZE;
    case SystemCursorShape::ResizeEastWest:
        return SDL_SYSTEM_CURSOR_EW_RESIZE;
    case SystemCursorShape::ResizeNortheastSouthwest:
        return SDL_SYSTEM_CURSOR_NESW_RESIZE;
    case SystemCursorShape::ResizeNorthwestSoutheast:
        return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
    case SystemCursorShape::Pointer:
        return SDL_SYSTEM_CURSOR_POINTER;
    case SystemCursorShape::Wait:
        return SDL_SYSTEM_CURSOR_WAIT;
    case SystemCursorShape::Progress:
        return SDL_SYSTEM_CURSOR_PROGRESS;
    case SystemCursorShape::NotAllowed:
        return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    }

    throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "System cursor shape value {} is invalid.", static_cast<unsigned int>(shape));
}

[[nodiscard]] detail::CursorHandle ToCursorHandle(SDL_Cursor* cursor) noexcept
{
    return detail::CursorHandle{reinterpret_cast<detail::CursorHandle::ValueType>(cursor)};
}

[[nodiscard]] SDL_Cursor* ToSdlCursor(detail::CursorHandle cursor) noexcept
{
    return reinterpret_cast<SDL_Cursor*>(cursor.GetValue());
}

[[nodiscard]] detail::BackendEventKind GetBackendEventKind(const SDL_Event& event) noexcept
{
    using detail::BackendEventKind;
    switch (event.type)
    {
    case SDL_EVENT_DISPLAY_ADDED:
        return BackendEventKind::DisplayAdded;
    case SDL_EVENT_DISPLAY_REMOVED:
        return BackendEventKind::DisplayRemoved;
    case SDL_EVENT_DISPLAY_MOVED:
    case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED:
    case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
    case SDL_EVENT_DISPLAY_ORIENTATION:
    case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
    case SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED:
        return BackendEventKind::DisplayChanged;
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        return BackendEventKind::WindowDisplayChanged;
    case SDL_EVENT_WINDOW_SHOWN:
        return BackendEventKind::WindowShown;
    default:
        return BackendEventKind::Other;
    }
}

[[nodiscard]] bool SupportsGlobalMouse() noexcept
{
    const char* const driver = SDL_GetCurrentVideoDriver();
    if (driver == nullptr)
    {
        return false;
    }

    const std::string_view name{driver};
    return name == "windows" || name == "cocoa" || name == "x11";
}

[[nodiscard]] ponder::core::Error MakeDisplayNotFoundError(DisplayId id)
{
    return ponder::core::Error{kNotFoundCode, std::format("Display {} is not connected.", id)};
}

[[nodiscard]] std::string MakeDisplayContext(DisplayId id)
{
    return std::format("display {}", id);
}

[[nodiscard]] ScreenRectangle ConvertRectangle(const detail::BackendScreenRectangle& rectangle, std::string_view operation, std::string_view context)
{
    if (rectangle.width < 0 || rectangle.height < 0)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{} returned a negative extent for {}.", operation, context);
    }

    if (!std::in_range<std::int32_t>(rectangle.x) || !std::in_range<std::int32_t>(rectangle.y))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{} returned an out-of-range position for {}.", operation, context);
    }

    return ScreenRectangle{ScreenPosition{static_cast<std::int32_t>(rectangle.x), static_cast<std::int32_t>(rectangle.y)},
                           ScreenExtent{static_cast<std::uint32_t>(rectangle.width), static_cast<std::uint32_t>(rectangle.height)}};
}

[[nodiscard]] DisplayOrientation ConvertOrientation(detail::BackendDisplayOrientation orientation) noexcept
{
    switch (orientation)
    {
    case detail::BackendDisplayOrientation::Landscape:
        return DisplayOrientation::Landscape;
    case detail::BackendDisplayOrientation::LandscapeFlipped:
        return DisplayOrientation::LandscapeFlipped;
    case detail::BackendDisplayOrientation::Portrait:
        return DisplayOrientation::Portrait;
    case detail::BackendDisplayOrientation::PortraitFlipped:
        return DisplayOrientation::PortraitFlipped;
    case detail::BackendDisplayOrientation::Unknown:
        return DisplayOrientation::Unknown;
    }

    return DisplayOrientation::Unknown;
}

[[nodiscard]] float ValidateScale(float scale, std::string_view operation, std::string_view context)
{
    if (!std::isfinite(scale) || scale <= 0.0F)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{} returned an invalid scale for {}.", operation, context);
    }

    return scale;
}

[[nodiscard]] std::optional<std::size_t> GetCursorIndex(SystemCursorShape shape) noexcept
{
    switch (shape)
    {
    case SystemCursorShape::Default:
        return 0;
    case SystemCursorShape::TextInput:
        return 1;
    case SystemCursorShape::Move:
        return 2;
    case SystemCursorShape::ResizeNorthSouth:
        return 3;
    case SystemCursorShape::ResizeEastWest:
        return 4;
    case SystemCursorShape::ResizeNortheastSouthwest:
        return 5;
    case SystemCursorShape::ResizeNorthwestSoutheast:
        return 6;
    case SystemCursorShape::Pointer:
        return 7;
    case SystemCursorShape::Wait:
        return 8;
    case SystemCursorShape::Progress:
        return 9;
    case SystemCursorShape::NotAllowed:
        return 10;
    }

    return std::nullopt;
}

void QuitSdlNoThrow(std::string_view context) noexcept
{
    try
    {
        SDL_Quit();
    }
    catch (const ponder::core::Exception& exception)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "Platform runtime cleanup failed during {}: {}", context, exception.GetMessage());
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "Platform runtime cleanup failed during {}: {}", context, exception.what());
    }
    catch (...)
    {
        LOG_ERROR_CATEGORY(kLogCategory, "Platform runtime cleanup failed during {} with an unknown exception", context);
    }
}
} // namespace

namespace detail
{
class DisplayTopologyUpdate final
{
public:
    explicit DisplayTopologyUpdate(SdlRuntime& runtime) noexcept :
        m_runtime(&runtime),
        m_nextDisplayId(runtime.m_nextDisplayId)
    {
    }

    ~DisplayTopologyUpdate() noexcept
    {
        Rollback();
    }

    DisplayTopologyUpdate(const DisplayTopologyUpdate&) = delete;
    DisplayTopologyUpdate& operator=(const DisplayTopologyUpdate&) = delete;

    DisplayTopologyUpdate(DisplayTopologyUpdate&& other) noexcept :
        m_runtime(other.m_runtime),
        m_connections(std::move(other.m_connections)),
        m_lifecycleUpdates(std::move(other.m_lifecycleUpdates)),
        m_nextConnectedBackendIds(std::move(other.m_nextConnectedBackendIds)),
        m_nextDisplayId(other.m_nextDisplayId),
        m_commitNextDisplayId(other.m_commitNextDisplayId),
        m_markDisplayRefreshComplete(other.m_markDisplayRefreshComplete),
        m_committed(other.m_committed)
    {
        other.m_runtime = nullptr;
        other.m_committed = true;
    }

    DisplayTopologyUpdate& operator=(DisplayTopologyUpdate&&) = delete;

    void ReserveConnections(std::size_t count, std::size_t newBackendMappingCount)
    {
        m_connections.reserve(count);
        if (newBackendMappingCount != 0)
        {
            m_runtime->m_displaysByBackendId.reserve(m_runtime->m_displaysByBackendId.size() + newBackendMappingCount);
        }
        if (count != 0)
        {
            m_runtime->m_displaysById.reserve(m_runtime->m_displaysById.size() + count);
        }
    }

    void ReserveLifecycleUpdates(std::size_t count)
    {
        m_lifecycleUpdates.reserve(count);
    }

    void SetConnectedBackendIds(std::span<const std::uint32_t> backendDisplayIds)
    {
        PONDER_VERIFY(!m_nextConnectedBackendIds.has_value(), "A display topology update already has a connected-display order");
        m_nextConnectedBackendIds.emplace(backendDisplayIds.begin(), backendDisplayIds.end());
    }

    [[nodiscard]] DisplayId StageConnection(std::uint32_t backendDisplayId, bool queueAdditionEvent)
    {
        const auto current = m_runtime->m_displaysByBackendId.find(backendDisplayId);
        PONDER_VERIFY(current == m_runtime->m_displaysByBackendId.end() || !current->second.connected, "Backend display {} is already connected",
                      backendDisplayId);
        PONDER_VERIFY(m_nextDisplayId != 0, "Platform display ID space is exhausted");

        const DisplayId id{m_nextDisplayId};
        ++m_nextDisplayId;
        m_commitNextDisplayId = true;

        const bool needsBackendMapping = current == m_runtime->m_displaysByBackendId.end();
        std::deque<RuntimeDisplayLifecycleEvent> nextPendingLifecycleEvents;
        if (!needsBackendMapping)
        {
            nextPendingLifecycleEvents = current->second.pendingLifecycleEvents;
        }
        if (queueAdditionEvent)
        {
            nextPendingLifecycleEvents.emplace_back(RuntimeDisplayLifecycleEvent{RuntimeDisplayLifecycleEventKind::Added, id});
        }

        PendingDisplayConnection& connection = m_connections.emplace_back(
            PendingDisplayConnection{backendDisplayId, id, false, false, queueAdditionEvent, std::move(nextPendingLifecycleEvents)});

        const auto [projectIterator, projectInserted] = m_runtime->m_displaysById.emplace(id, RuntimeBackendDisplayRecord{backendDisplayId, false});
        static_cast<void>(projectIterator);
        connection.projectMappingInserted = projectInserted;
        PONDER_VERIFY(projectInserted, "Platform display ID {} is already registered", id.GetValue());

        if (needsBackendMapping)
        {
            const auto [backendIterator, backendInserted] =
                m_runtime->m_displaysByBackendId.emplace(backendDisplayId, RuntimeDisplayRecord{id, false, {}});
            static_cast<void>(backendIterator);
            connection.backendMappingInserted = backendInserted;
            PONDER_VERIFY(backendInserted, "Backend display {} is already registered", backendDisplayId);
        }
        return id;
    }

    void StageCurrentDisconnection(std::uint32_t backendDisplayId)
    {
        const auto backendMapping = m_runtime->m_displaysByBackendId.find(backendDisplayId);
        PONDER_VERIFY(backendMapping != m_runtime->m_displaysByBackendId.end() && backendMapping->second.connected,
                      "Connected backend display {} has no active mapping", backendDisplayId);
        const DisplayId id = backendMapping->second.id;
        const auto projectMapping = m_runtime->m_displaysById.find(id);
        PONDER_VERIFY(projectMapping != m_runtime->m_displaysById.end() && projectMapping->second.backendId == backendDisplayId &&
                          projectMapping->second.connected,
                      "Connected display {} has no active project mapping", id.GetValue());

        PendingDisplayLifecycleUpdate disconnection{backendDisplayId, id, true, false, backendMapping->second.pendingLifecycleEvents};
        disconnection.nextPendingLifecycleEvents.emplace_back(RuntimeDisplayLifecycleEvent{RuntimeDisplayLifecycleEventKind::Removed, id});
        m_lifecycleUpdates.emplace_back(std::move(disconnection));
    }

    void StageRemoval(std::uint32_t backendDisplayId, DisplayId id)
    {
        const auto backendMapping = m_runtime->m_displaysByBackendId.find(backendDisplayId);
        PONDER_VERIFY(backendMapping != m_runtime->m_displaysByBackendId.end(), "Backend display {} has no removal mapping", backendDisplayId);
        const auto projectMapping = m_runtime->m_displaysById.find(id);
        PONDER_VERIFY(projectMapping != m_runtime->m_displaysById.end() && projectMapping->second.backendId == backendDisplayId,
                      "Backend display {} has an inconsistent removal mapping", backendDisplayId);

        PendingDisplayLifecycleUpdate disconnection{backendDisplayId, id, false, true, backendMapping->second.pendingLifecycleEvents};
        if (!disconnection.nextPendingLifecycleEvents.empty())
        {
            PONDER_VERIFY(disconnection.nextPendingLifecycleEvents.front().kind == RuntimeDisplayLifecycleEventKind::Removed &&
                              disconnection.nextPendingLifecycleEvents.front().id == id,
                          "Backend display {} removal events are out of order", backendDisplayId);
            disconnection.nextPendingLifecycleEvents.pop_front();
        }
        else
        {
            PONDER_VERIFY(backendMapping->second.connected && backendMapping->second.id == id && projectMapping->second.connected,
                          "Backend display {} is not connected for removal", backendDisplayId);
            disconnection.disconnectCurrent = true;

            std::vector<std::uint32_t> nextConnectedBackendIds = m_runtime->m_connectedBackendDisplayIds;
            std::erase(nextConnectedBackendIds, backendDisplayId);
            SetConnectedBackendIds(nextConnectedBackendIds);
        }

        m_lifecycleUpdates.emplace_back(std::move(disconnection));
    }

    [[nodiscard]] std::optional<DisplayId> FindConnectedDisplayId(std::uint32_t backendDisplayId) const
    {
        if (backendDisplayId == 0)
        {
            return std::nullopt;
        }

        const auto connection = std::ranges::find(m_connections, backendDisplayId, &PendingDisplayConnection::backendId);
        if (connection != m_connections.end())
        {
            return connection->id;
        }

        const auto disconnection = std::ranges::find(m_lifecycleUpdates, backendDisplayId, &PendingDisplayLifecycleUpdate::backendId);
        if (disconnection != m_lifecycleUpdates.end() && disconnection->disconnectCurrent)
        {
            return std::nullopt;
        }

        return m_runtime->FindConnectedDisplayId(backendDisplayId);
    }

    void MarkDisplayRefreshComplete() noexcept
    {
        m_markDisplayRefreshComplete = true;
    }

    void Commit()
    {
        PONDER_VERIFY(!m_committed, "A display topology update was committed more than once");

        for (const PendingDisplayLifecycleUpdate& disconnection : m_lifecycleUpdates)
        {
            const auto backendMapping = m_runtime->m_displaysByBackendId.find(disconnection.backendId);
            const auto projectMapping = m_runtime->m_displaysById.find(disconnection.id);
            PONDER_VERIFY(backendMapping != m_runtime->m_displaysByBackendId.end() && projectMapping != m_runtime->m_displaysById.end() &&
                              projectMapping->second.backendId == disconnection.backendId,
                          "Display {} could not commit its disconnection mapping", disconnection.id.GetValue());
            if (disconnection.disconnectCurrent)
            {
                PONDER_VERIFY(backendMapping->second.id == disconnection.id && backendMapping->second.connected && projectMapping->second.connected,
                              "Display {} changed before its disconnection was committed", disconnection.id.GetValue());
            }
        }

        for (const PendingDisplayConnection& connection : m_connections)
        {
            const auto backendMapping = m_runtime->m_displaysByBackendId.find(connection.backendId);
            const auto projectMapping = m_runtime->m_displaysById.find(connection.id);
            PONDER_VERIFY(backendMapping != m_runtime->m_displaysByBackendId.end() && projectMapping != m_runtime->m_displaysById.end() &&
                              projectMapping->second.backendId == connection.backendId && !backendMapping->second.connected &&
                              !projectMapping->second.connected,
                          "Display {} could not commit its connection mapping", connection.id.GetValue());
        }

        for (PendingDisplayLifecycleUpdate& disconnection : m_lifecycleUpdates)
        {
            auto backendMapping = m_runtime->m_displaysByBackendId.find(disconnection.backendId);
            auto projectMapping = m_runtime->m_displaysById.find(disconnection.id);
            backendMapping->second.pendingLifecycleEvents.swap(disconnection.nextPendingLifecycleEvents);
            if (disconnection.disconnectCurrent)
            {
                backendMapping->second.connected = false;
                backendMapping->second.unconfirmedRemovedEventId.reset();
                projectMapping->second.connected = false;
            }
            if (disconnection.recordRemovalEvent)
            {
                backendMapping->second.lastRemovedEventId = disconnection.id;
            }
        }

        for (PendingDisplayConnection& connection : m_connections)
        {
            auto backendMapping = m_runtime->m_displaysByBackendId.find(connection.backendId);
            auto projectMapping = m_runtime->m_displaysById.find(connection.id);
            if (connection.lifecycleEventsChanged)
            {
                backendMapping->second.pendingLifecycleEvents.swap(connection.nextPendingLifecycleEvents);
            }
            backendMapping->second.id = connection.id;
            backendMapping->second.connected = true;
            backendMapping->second.unconfirmedRemovedEventId.reset();
            projectMapping->second.connected = true;
        }

        if (m_nextConnectedBackendIds.has_value())
        {
            m_runtime->m_connectedBackendDisplayIds.swap(*m_nextConnectedBackendIds);
        }
        if (m_commitNextDisplayId)
        {
            m_runtime->m_nextDisplayId = m_nextDisplayId;
        }
        if (m_markDisplayRefreshComplete)
        {
            m_runtime->m_hasCompletedDisplayRefresh = true;
        }
        m_committed = true;
    }

private:
    struct PendingDisplayConnection final
    {
        std::uint32_t backendId{};
        DisplayId id;
        bool backendMappingInserted{};
        bool projectMappingInserted{};
        bool lifecycleEventsChanged{};
        std::deque<RuntimeDisplayLifecycleEvent> nextPendingLifecycleEvents;
    };

    struct PendingDisplayLifecycleUpdate final
    {
        std::uint32_t backendId{};
        DisplayId id;
        bool disconnectCurrent{};
        bool recordRemovalEvent{};
        std::deque<RuntimeDisplayLifecycleEvent> nextPendingLifecycleEvents;
    };

    void Rollback() noexcept
    {
        if (m_runtime == nullptr || m_committed)
        {
            return;
        }

        for (const PendingDisplayConnection& connection : m_connections)
        {
            if (connection.backendMappingInserted)
            {
                m_runtime->m_displaysByBackendId.erase(connection.backendId);
            }
            if (connection.projectMappingInserted)
            {
                m_runtime->m_displaysById.erase(connection.id);
            }
        }
        m_committed = true;
    }

    SdlRuntime* m_runtime{};
    std::vector<PendingDisplayConnection> m_connections;
    std::vector<PendingDisplayLifecycleUpdate> m_lifecycleUpdates;
    std::optional<std::vector<std::uint32_t>> m_nextConnectedBackendIds;
    DisplayId::ValueType m_nextDisplayId{};
    bool m_commitNextDisplayId{};
    bool m_markDisplayRefreshComplete{};
    bool m_committed{};
};

namespace
{
struct RuntimeEventTranslationContext final
{
    SdlRuntime* runtime{};
    const DisplayTopologyUpdate* topologyUpdate{};
    std::uint32_t routedBackendDisplayId{};
    std::optional<DisplayId> routedDisplayId;
};

[[nodiscard]] std::optional<WindowId> ResolveWindowId(void* context, std::uint32_t backendWindowId)
{
    auto& routing = *static_cast<RuntimeEventTranslationContext*>(context);
    return routing.runtime->FindWindowId(backendWindowId);
}

[[nodiscard]] std::optional<DisplayId> ResolveDisplayId(void* context, std::uint32_t backendDisplayId)
{
    auto& routing = *static_cast<RuntimeEventTranslationContext*>(context);
    if (routing.routedDisplayId.has_value() && routing.routedBackendDisplayId == backendDisplayId)
    {
        return routing.routedDisplayId;
    }

    if (routing.topologyUpdate != nullptr)
    {
        return routing.topologyUpdate->FindConnectedDisplayId(backendDisplayId);
    }

    return routing.runtime->FindConnectedDisplayId(backendDisplayId);
}
} // namespace

SdlRuntime::SdlRuntime() :
    m_windowRegistry(m_ownerThread)
{
    if (!SDL_IsMainThread())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::WrongThread, "Runtime must be created on SDL's main thread.");
    }
    if (SDL_WasInit(0) != 0)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Cannot create Runtime while SDL subsystems are already initialized.");
    }

    DialogInitialize();
}

void SdlRuntime::Initialize(const RuntimeDesc& desc)
{
    PONDER_VERIFY(!m_initialized, "Cannot initialize SdlRuntime more than once");
    m_ownerThread.Verify("runtime initialization");

    if (!SDL_IsMainThread())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::WrongThread, "Runtime must be created on SDL's main thread.");
    }

    if (SDL_WasInit(0) != 0)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Cannot create Runtime while SDL subsystems are already initialized.");
    }

    const auto setMetadata = [](ApplicationMetadataProperty property, const char* value)
    {
        const char* const propertyName = GetMetadataPropertyName(property);
        PONDER_VERIFY(propertyName != nullptr, "Application metadata property is invalid");
        if (!SDL_SetAppMetadataProperty(propertyName, value))
        {
            throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}",
                                     CaptureSdlFailureMessage("SDL_SetAppMetadataProperty", std::format("{}", property)));
        }
    };

    setMetadata(ApplicationMetadataProperty::Name, desc.applicationName.c_str());
    setMetadata(ApplicationMetadataProperty::Version, desc.applicationVersion.has_value() ? desc.applicationVersion->c_str() : nullptr);
    setMetadata(ApplicationMetadataProperty::Identifier, desc.applicationIdentifier.has_value() ? desc.applicationIdentifier->c_str() : nullptr);

    auto quitAfterFailedInitialization = ponder::core::MakeScopeExit(
        []() noexcept
        {
            QuitSdlNoThrow("failed runtime initialization");
        });
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", CaptureSdlFailureMessage("SDL_Init", "SDL_INIT_VIDEO"));
    }

    m_initialized = true;
    quitAfterFailedInitialization.Dismiss();

    LOG_INFO_CATEGORY(kLogCategory, "Platform runtime initialized");
}

SdlRuntime::~SdlRuntime() noexcept
{
    VerifyOwnerThreadForDestruction("Runtime");
    const std::size_t outstandingDialogCount = m_initialized ? DialogGetOutstandingRequestCount() : 0;
    PONDER_VERIFY(m_registry.IsEmpty() && outstandingDialogCount == 0, "Cannot destroy Runtime with {} children and {} requests",
                  m_registry.GetChildCount(), outstandingDialogCount);
    PONDER_VERIFY(m_windowRegistry.IsEmpty(), "Cannot destroy Runtime with {} registered windows", m_windowRegistry.GetWindowCount());

    if (m_initialized)
    {
        constexpr SDL_InitFlags kRuntimeSubsystems = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
        const SDL_InitFlags initializedSubsystems = SDL_WasInit(0);
        PONDER_VERIFY((initializedSubsystems & kRuntimeSubsystems) == kRuntimeSubsystems && (initializedSubsystems & ~kRuntimeSubsystems) == 0,
                      "SDL subsystem ownership changed while Runtime was active");

        LOG_INFO_CATEGORY(kLogCategory, "Platform runtime shutting down");
        DestroySystemCursors();
    }

    DialogShutdownForRuntimeDestruction();
    if (m_initialized)
    {
        QuitSdlNoThrow("runtime destruction");
        m_initialized = false;
        LOG_INFO_CATEGORY(kLogCategory, "Platform runtime shut down");
    }
    HintRestoreAll();
}

void SdlRuntime::VerifyOwnerThread(std::string_view operation) const
{
    m_ownerThread.Verify(operation);
    VerifyInitialized(operation);
}

void SdlRuntime::VerifyInitialized(std::string_view operation) const
{
    if (!m_initialized)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Cannot perform {} before Runtime initialization.", operation);
    }
}

void SdlRuntime::VerifyOwnerThreadForDestruction(std::string_view object) const noexcept
{
    m_ownerThread.VerifyForDestruction(object);
}

void SdlRuntime::RegisterChild(const void* child)
{
    VerifyOwnerThread("child registration");
    m_registry.RegisterChild(child);
}

void SdlRuntime::UnregisterChild(const void* child)
{
    VerifyOwnerThread("child unregistration");
    m_registry.UnregisterChild(child);
}

WindowId SdlRuntime::GetNextWindowIdForRegistration() const
{
    VerifyOwnerThread("window registration preparation");
    return m_windowRegistry.GetNextWindowId();
}

void SdlRuntime::RegisterWindow(WindowImpl& window, BackendWindowHandle backendWindow, std::uint32_t backendWindowId, WindowId id)
{
    VerifyOwnerThread("window registration");
    m_windowRegistry.Register(window, backendWindow, backendWindowId, id);

    try
    {
        m_registry.RegisterChild(std::addressof(window));
    }
    catch (...)
    {
        m_windowRegistry.RollbackRegistration(window, backendWindowId, id);
        throw;
    }

    window.CommitRegistration();
}

void SdlRuntime::BeginWindowDestruction(WindowImpl& window, std::uint32_t backendWindowId, WindowId id)
{
    VerifyOwnerThread("window destruction");
    m_windowRegistry.Unregister(window, backendWindowId, id);
}

void SdlRuntime::FinishWindowDestruction(WindowImpl& window)
{
    VerifyOwnerThread("window destruction");
    m_registry.UnregisterChild(std::addressof(window));
}

void SdlRuntime::RestoreWindowIdAfterFailedConstruction(WindowId id) noexcept
{
    m_windowRegistry.RestoreWindowIdAfterFailedConstruction(id);
}

std::optional<WindowId> SdlRuntime::FindWindowId(std::uint32_t backendWindowId) const
{
    return m_windowRegistry.FindWindowId(backendWindowId);
}

std::optional<DisplayId> SdlRuntime::FindConnectedDisplayId(std::uint32_t backendDisplayId) const
{
    VerifyOwnerThread("display lookup");
    if (backendDisplayId == 0)
    {
        return std::nullopt;
    }

    const auto mapping = m_displaysByBackendId.find(backendDisplayId);
    if (mapping == m_displaysByBackendId.end() || !mapping->second.connected)
    {
        return std::nullopt;
    }

    const auto projectMapping = m_displaysById.find(mapping->second.id);
    PONDER_VERIFY(projectMapping != m_displaysById.end() && projectMapping->second.backendId == backendDisplayId && projectMapping->second.connected,
                  "Connected backend display {} has an inconsistent project mapping", backendDisplayId);
    return mapping->second.id;
}

std::optional<DisplayId> SdlRuntime::FindDisplayIdForRemoval(std::uint32_t backendDisplayId) const
{
    VerifyOwnerThread("display removal lookup");
    if (backendDisplayId == 0)
    {
        return std::nullopt;
    }

    const auto mapping = m_displaysByBackendId.find(backendDisplayId);
    if (mapping == m_displaysByBackendId.end())
    {
        return std::nullopt;
    }

    if (!mapping->second.pendingLifecycleEvents.empty())
    {
        const RuntimeDisplayLifecycleEvent& pending = mapping->second.pendingLifecycleEvents.front();
        return pending.kind == RuntimeDisplayLifecycleEventKind::Removed ? std::optional<DisplayId>{pending.id} : std::nullopt;
    }

    if (!mapping->second.connected)
    {
        return std::nullopt;
    }

    const auto projectMapping = m_displaysById.find(mapping->second.id);
    PONDER_VERIFY(projectMapping != m_displaysById.end() && projectMapping->second.backendId == backendDisplayId,
                  "Backend display {} has an inconsistent removal mapping", backendDisplayId);
    return mapping->second.id;
}

ponder::core::Timestamp SdlRuntime::TimeNow() const
{
    VerifyOwnerThread("timestamp query");
    const std::uint64_t ticks = SDL_GetTicksNS();
    constexpr std::uint64_t kMaximumTicks = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    PONDER_VERIFY(ticks <= kMaximumTicks, "SDL monotonic timestamp exceeds Timestamp range");
    return ponder::core::Timestamp{std::chrono::nanoseconds{static_cast<std::int64_t>(ticks)}};
}

IPlatformWindowBackend& SdlRuntime::GetWindowBackend() noexcept
{
    return m_windowBackend;
}

void SdlRuntime::RecoverPendingDisplayLifecycleEvent()
{
    if (!m_pendingDisplayLifecycleRecovery.has_value())
    {
        return;
    }

    const RuntimePendingDisplayLifecycleRecovery recovery = *m_pendingDisplayLifecycleRecovery;
    const std::optional<RuntimeDisplayLifecycleEvent> pending = FindPendingDisplayLifecycleEvent(recovery.backendId);
    if (pending.has_value() && pending->kind == recovery.kind && (!recovery.id.has_value() || pending->id == recovery.id))
    {
        AcknowledgePendingDisplayLifecycleEvent(recovery.backendId, *pending);
        m_pendingDisplayLifecycleRecovery.reset();
        return;
    }
    if (recovery.kind == RuntimeDisplayLifecycleEventKind::Added && !recovery.id.has_value() && pending.has_value() &&
        pending->kind == RuntimeDisplayLifecycleEventKind::Removed)
    {
        m_pendingDisplayLifecycleRecovery.reset();
        return;
    }

    if (recovery.kind == RuntimeDisplayLifecycleEventKind::Added)
    {
        if (recovery.id.has_value())
        {
            DisplayTopologyUpdate disconnection = DisconnectDisplayFromEvent(recovery.backendId, *recovery.id);
            disconnection.Commit();
            m_pendingDisplayLifecycleRecovery->id.reset();

            const std::optional<RuntimeDisplayLifecycleEvent> pendingAddition = FindPendingDisplayLifecycleEvent(recovery.backendId);
            if (pendingAddition.has_value() && pendingAddition->kind == RuntimeDisplayLifecycleEventKind::Added)
            {
                m_pendingDisplayLifecycleRecovery.reset();
                return;
            }
        }

        if (recovery.validateAgainstLiveTopology)
        {
            const std::vector<std::uint32_t> connectedBackendDisplayIds = EnumerateBackendDisplays();
            if (std::ranges::find(connectedBackendDisplayIds, recovery.backendId) == connectedBackendDisplayIds.end())
            {
                m_pendingDisplayLifecycleRecovery.reset();
                return;
            }
            m_pendingDisplayLifecycleRecovery->validateAgainstLiveTopology = false;
        }

        DisplayTopologyUpdate update = ConnectDisplayFromEvent(recovery.backendId);
        update.Commit();
        m_pendingDisplayLifecycleRecovery.reset();
        return;
    }

    PONDER_VERIFY(recovery.id.has_value(), "A pending display removal recovery has no project identity");
    if (recovery.validateAgainstLiveTopology)
    {
        const std::vector<std::uint32_t> connectedBackendDisplayIds = EnumerateBackendDisplays();
        if (std::ranges::find(connectedBackendDisplayIds, recovery.backendId) != connectedBackendDisplayIds.end())
        {
            const auto current = m_displaysByBackendId.find(recovery.backendId);
            PONDER_VERIFY(current != m_displaysByBackendId.end() && current->second.connected && current->second.id == *recovery.id,
                          "Backend display {} changed before removal recovery", recovery.backendId);
            current->second.unconfirmedRemovedEventId = recovery.id;
            m_pendingDisplayLifecycleRecovery.reset();
            return;
        }
        m_pendingDisplayLifecycleRecovery->validateAgainstLiveTopology = false;
    }

    DisplayTopologyUpdate update = DisconnectDisplayFromEvent(recovery.backendId, *recovery.id);
    update.Commit();
    m_pendingDisplayLifecycleRecovery.reset();
}

bool SdlRuntime::PollBackendEvent(BackendEvent& event) noexcept
{
    static_assert(std::is_trivially_copyable_v<SDL_Event>);
    static_assert(sizeof(SDL_Event) <= BackendEvent::kStorageSize);

    SDL_Event sdlEvent{};
    if (!SDL_PollEvent(&sdlEvent))
    {
        return false;
    }

    event = BackendEvent{};
    std::memcpy(event.m_storage.data(), &sdlEvent, sizeof(sdlEvent));
    event.m_kind = GetBackendEventKind(sdlEvent);
    switch (event.m_kind)
    {
    case BackendEventKind::DisplayAdded:
    case BackendEventKind::DisplayRemoved:
    case BackendEventKind::DisplayChanged:
        event.m_backendDisplayId = static_cast<std::uint32_t>(sdlEvent.display.displayID);
        break;
    case BackendEventKind::WindowDisplayChanged:
        event.m_backendWindowId = static_cast<std::uint32_t>(sdlEvent.window.windowID);
        event.m_backendDisplayId = static_cast<std::uint32_t>(sdlEvent.window.data1);
        break;
    case BackendEventKind::WindowShown:
        event.m_backendWindowId = static_cast<std::uint32_t>(sdlEvent.window.windowID);
        break;
    case BackendEventKind::Other:
        break;
    }
    return true;
}

std::optional<PlatformEvent> SdlRuntime::TranslateBackendEvent(const BackendEvent& event, const EventTranslationContext& context) const
{
    SDL_Event sdlEvent{};
    std::memcpy(&sdlEvent, event.m_storage.data(), sizeof(sdlEvent));
    return TranslateSdlEvent(sdlEvent, context);
}

std::optional<PlatformEvent> SdlRuntime::EventPoll()
{
    VerifyOwnerThread("event polling");
    RecoverPendingDisplayLifecycleEvent();

    if (std::optional<DialogCompletedEvent> dialogCompletion = DialogPollCompletion(); dialogCompletion.has_value())
    {
        return PlatformEvent{std::move(*dialogCompletion)};
    }

    while (true)
    {
        BackendEvent event;
        if (!PollBackendEvent(event))
        {
            return std::nullopt;
        }

        RuntimeEventTranslationContext routing{this};
        std::optional<DisplayTopologyUpdate> topologyUpdate;
        bool commitsPendingLifecycleRecovery = false;
        if (event.GetKind() == BackendEventKind::DisplayAdded)
        {
            const std::uint32_t backendDisplayId = event.GetBackendDisplayId();
            if (backendDisplayId == 0)
            {
                continue;
            }

            const std::optional<RuntimeDisplayLifecycleEvent> pending = FindPendingDisplayLifecycleEvent(backendDisplayId);
            if (pending.has_value())
            {
                if (pending->kind != RuntimeDisplayLifecycleEventKind::Added)
                {
                    continue;
                }

                routing.routedBackendDisplayId = backendDisplayId;
                routing.routedDisplayId = pending->id;
                AcknowledgePendingDisplayLifecycleEvent(backendDisplayId, *pending);
                topologyUpdate.emplace(*this);
            }
            else
            {
                const auto current = m_displaysByBackendId.find(backendDisplayId);
                if (current != m_displaysByBackendId.end() && current->second.connected &&
                    current->second.unconfirmedRemovedEventId == current->second.id)
                {
                    PONDER_VERIFY(!m_pendingDisplayLifecycleRecovery.has_value(), "A display lifecycle recovery is already pending");
                    m_pendingDisplayLifecycleRecovery =
                        RuntimePendingDisplayLifecycleRecovery{RuntimeDisplayLifecycleEventKind::Added, backendDisplayId, current->second.id, true};
                    commitsPendingLifecycleRecovery = true;
                    current->second.unconfirmedRemovedEventId.reset();

                    DisplayTopologyUpdate confirmedRemoval = DisconnectDisplayFromEvent(backendDisplayId, current->second.id);
                    confirmedRemoval.Commit();
                    m_pendingDisplayLifecycleRecovery->id.reset();
                }
                else if (FindConnectedDisplayId(backendDisplayId).has_value())
                {
                    continue;
                }
                if (!commitsPendingLifecycleRecovery)
                {
                    PONDER_VERIFY(!m_pendingDisplayLifecycleRecovery.has_value(), "A display lifecycle recovery is already pending");
                    m_pendingDisplayLifecycleRecovery =
                        RuntimePendingDisplayLifecycleRecovery{RuntimeDisplayLifecycleEventKind::Added, backendDisplayId, std::nullopt, true};
                    commitsPendingLifecycleRecovery = true;
                }
                topologyUpdate.emplace(ConnectDisplayFromEvent(backendDisplayId));
            }
            routing.topologyUpdate = std::addressof(*topologyUpdate);
            if (!routing.routedDisplayId.has_value() && !topologyUpdate->FindConnectedDisplayId(backendDisplayId).has_value())
            {
                topologyUpdate->Commit();
                if (commitsPendingLifecycleRecovery)
                {
                    m_pendingDisplayLifecycleRecovery.reset();
                }
                continue;
            }
        }
        else if (event.GetKind() == BackendEventKind::DisplayRemoved)
        {
            routing.routedBackendDisplayId = event.GetBackendDisplayId();
            const std::optional<RuntimeDisplayLifecycleEvent> pending = FindPendingDisplayLifecycleEvent(routing.routedBackendDisplayId);
            if (pending.has_value() && pending->kind != RuntimeDisplayLifecycleEventKind::Removed)
            {
                continue;
            }

            routing.routedDisplayId =
                pending.has_value() ? std::optional<DisplayId>{pending->id} : FindDisplayIdForRemoval(routing.routedBackendDisplayId);
            if (!routing.routedDisplayId.has_value())
            {
                continue;
            }

            if (pending.has_value())
            {
                AcknowledgePendingDisplayLifecycleEvent(routing.routedBackendDisplayId, *pending);
                topologyUpdate.emplace(*this);
            }
            else
            {
                const auto current = m_displaysByBackendId.find(routing.routedBackendDisplayId);
                PONDER_VERIFY(current != m_displaysByBackendId.end() && current->second.connected && current->second.id == *routing.routedDisplayId,
                              "Backend display {} changed before removal reconciliation", routing.routedBackendDisplayId);
                const bool reusedAfterRemoval =
                    current->second.lastRemovedEventId.has_value() && current->second.lastRemovedEventId != routing.routedDisplayId;
                PONDER_VERIFY(!m_pendingDisplayLifecycleRecovery.has_value(), "A display lifecycle recovery is already pending");
                m_pendingDisplayLifecycleRecovery = RuntimePendingDisplayLifecycleRecovery{
                    RuntimeDisplayLifecycleEventKind::Removed, routing.routedBackendDisplayId, routing.routedDisplayId, reusedAfterRemoval};
                commitsPendingLifecycleRecovery = true;
                if (reusedAfterRemoval)
                {
                    const std::vector<std::uint32_t> connectedBackendDisplayIds = EnumerateBackendDisplays();
                    if (std::ranges::find(connectedBackendDisplayIds, routing.routedBackendDisplayId) != connectedBackendDisplayIds.end())
                    {
                        current->second.unconfirmedRemovedEventId = routing.routedDisplayId;
                        m_pendingDisplayLifecycleRecovery.reset();
                        continue;
                    }
                    m_pendingDisplayLifecycleRecovery->validateAgainstLiveTopology = false;
                }

                topologyUpdate.emplace(DisconnectDisplayFromEvent(routing.routedBackendDisplayId, *routing.routedDisplayId));
            }
            routing.topologyUpdate = std::addressof(*topologyUpdate);
        }
        else if (event.GetKind() == BackendEventKind::DisplayChanged)
        {
            const std::uint32_t backendDisplayId = event.GetBackendDisplayId();
            topologyUpdate.emplace(ReconcileDisplayFromEvent(backendDisplayId));
            routing.topologyUpdate = std::addressof(*topologyUpdate);
            if (!topologyUpdate->FindConnectedDisplayId(backendDisplayId).has_value())
            {
                topologyUpdate->Commit();
                continue;
            }
        }

        if (event.GetKind() == BackendEventKind::WindowDisplayChanged && !FindWindowId(event.GetBackendWindowId()).has_value())
        {
            continue;
        }

        if (event.GetKind() == BackendEventKind::WindowDisplayChanged)
        {
            topologyUpdate.emplace(ReconcileDisplayFromEvent(event.GetBackendDisplayId()));
            routing.topologyUpdate = std::addressof(*topologyUpdate);
        }

        if (event.GetKind() == BackendEventKind::WindowShown)
        {
            ObserveWindowShownEvent(event.GetBackendWindowId());
        }

        const EventTranslationContext translationContext{&routing, ResolveWindowId, ResolveDisplayId};
        std::optional<PlatformEvent> translated = TranslateBackendEvent(event, translationContext);

        if (topologyUpdate.has_value())
        {
            topologyUpdate->Commit();
            if (commitsPendingLifecycleRecovery)
            {
                m_pendingDisplayLifecycleRecovery.reset();
            }
        }

        if (translated.has_value())
        {
            return translated;
        }
    }
}

Window SdlRuntime::WindowCreate(const WindowDesc& desc)
{
    VerifyOwnerThread("window creation");
    return Window{WindowImpl::Create(*this, desc)};
}

std::vector<DisplayInfo> SdlRuntime::DisplayEnumerate()
{
    std::vector<std::uint32_t> backendDisplayIds = RefreshDisplays();

    std::vector<DisplayInfo> displays;
    displays.reserve(backendDisplayIds.size());
    for (const std::uint32_t backendDisplayId : backendDisplayIds)
    {
        const auto mapping = m_displaysByBackendId.find(backendDisplayId);
        PONDER_VERIFY(mapping != m_displaysByBackendId.end() && mapping->second.connected, "Connected backend display {} has no project mapping",
                      backendDisplayId);
        displays.emplace_back(QueryDisplayInfo(mapping->second.id, backendDisplayId));
    }

    return displays;
}

ponder::core::Result<DisplayInfo> SdlRuntime::DisplayGetInfo(DisplayId id)
{
    VerifyOwnerThread("display query");
    if (!id.IsValid())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Display ID must be valid.");
    }

    static_cast<void>(RefreshDisplays());

    const auto mapping = m_displaysById.find(id);
    if (mapping == m_displaysById.end() || !mapping->second.connected)
    {
        return ponder::core::Result<DisplayInfo>::FromError(MakeDisplayNotFoundError(id));
    }

    try
    {
        return QueryDisplayInfo(id, mapping->second.backendId);
    }
    catch (const ponder::core::Exception&)
    {
        const std::exception_ptr primaryFailure = std::current_exception();
        try
        {
            static_cast<void>(RefreshDisplays());
            const auto confirmedMapping = m_displaysById.find(id);
            if (confirmedMapping == m_displaysById.end() || !confirmedMapping->second.connected)
            {
                return ponder::core::Result<DisplayInfo>::FromError(MakeDisplayNotFoundError(id));
            }
        }
        catch (const ponder::core::Exception&)
        {
        }

        std::rethrow_exception(primaryFailure);
    }
}

ponder::core::Result<DisplayId> SdlRuntime::GetDisplayIdForWindow(BackendWindowHandle window, std::string_view windowContext)
{
    VerifyOwnerThread("window display query");
    const std::uint32_t backendDisplayId = m_displayBackend.GetForWindow(window);
    if (backendDisplayId == 0)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL_GetDisplayForWindow returned an invalid display ID for {}.", windowContext);
    }

    static_cast<void>(RefreshDisplays());

    const auto mapping = m_displaysByBackendId.find(backendDisplayId);
    if (mapping == m_displaysByBackendId.end() || !mapping->second.connected)
    {
        return ponder::core::Result<DisplayId>::FromError(ponder::core::Error{kNotFoundCode, "The window's display is not connected."});
    }
    return mapping->second.id;
}

float SdlRuntime::GetPixelDensityForWindow(BackendWindowHandle window, std::string_view windowContext) const
{
    VerifyOwnerThread("window pixel density query");
    return ValidateScale(m_displayBackend.GetWindowPixelDensity(window), "SDL_GetWindowPixelDensity", windowContext);
}

float SdlRuntime::GetDisplayScaleForWindow(BackendWindowHandle window, std::string_view windowContext) const
{
    VerifyOwnerThread("window display scale query");
    return ValidateScale(m_displayBackend.GetWindowDisplayScale(window), "SDL_GetWindowDisplayScale", windowContext);
}

ponder::core::VoidResult SdlRuntime::MouseSetCapture(bool enabled)
{
    VerifyOwnerThread("mouse capture update");
    if (!SupportsGlobalMouse())
    {
        if (!enabled)
        {
            return ponder::core::VoidResult::Success();
        }

        return ponder::core::VoidResult::FromError(
            ponder::core::Error{kUnsupportedCode, "Global mouse capture is unsupported by the active video driver."});
    }

    if (!SDL_CaptureMouse(enabled))
    {
        return ponder::core::VoidResult::FromError(CaptureSdlFailure(kBackendFailureCode, "SDL_CaptureMouse", enabled ? "enable" : "disable"));
    }
    return ponder::core::VoidResult::Success();
}

ponder::core::Result<LogicalPoint> SdlRuntime::MouseGetGlobalPosition() const
{
    VerifyOwnerThread("global mouse-position query");
    if (!SupportsGlobalMouse())
    {
        return ponder::core::Result<LogicalPoint>::FromError(
            ponder::core::Error{kUnsupportedCode, "Global mouse position is unsupported by the active video driver."});
    }

    LogicalPoint position;
    static_cast<void>(SDL_GetGlobalMouseState(&position.x, &position.y));
    if (!IsValid(position))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Global mouse-position backend returned non-finite coordinates.");
    }

    return position;
}

void SdlRuntime::MouseSetSystemCursor(SystemCursorShape shape)
{
    VerifyOwnerThread("system cursor update");
    const std::optional<std::size_t> cursorIndex = GetCursorIndex(shape);
    if (!cursorIndex.has_value())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "System cursor shape is invalid.");
    }

    CursorHandle& cursor = m_systemCursors[*cursorIndex];
    if (!cursor.IsValid())
    {
        SDL_Cursor* const sdlCursor = SDL_CreateSystemCursor(ToSdlSystemCursor(shape));
        if (sdlCursor == nullptr)
        {
            throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}",
                                     CaptureSdlFailureMessage("SDL_CreateSystemCursor", std::format("{}", shape)));
        }
        const CursorHandle createdCursor = ToCursorHandle(sdlCursor);
        if (!createdCursor.IsValid())
        {
            throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "System cursor backend returned an invalid cursor handle for {}.", shape);
        }
        cursor = createdCursor;
    }

    if (!SDL_SetCursor(ToSdlCursor(cursor)))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}",
                                 CaptureSdlFailureMessage("SDL_SetCursor", std::format("cursor {}", cursor)));
    }
}

void SdlRuntime::MouseShowCursor()
{
    VerifyOwnerThread("cursor visibility update");
    if (SDL_CursorVisible())
    {
        return;
    }

    if (!SDL_ShowCursor())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", CaptureSdlFailureMessage("SDL_ShowCursor"));
    }
}

void SdlRuntime::MouseHideCursor()
{
    VerifyOwnerThread("cursor visibility update");
    if (!SDL_CursorVisible())
    {
        return;
    }

    if (!SDL_HideCursor())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", CaptureSdlFailureMessage("SDL_HideCursor"));
    }
}

bool SdlRuntime::MouseIsCursorVisible() const
{
    VerifyOwnerThread("cursor visibility query");
    return SDL_CursorVisible();
}

ponder::core::VoidResult SdlRuntime::UriOpenExternal(std::string_view uri)
{
    VerifyOwnerThread("external URI opening");
    if (uri.empty())
    {
        return ponder::core::VoidResult::FromError(ponder::core::Error{kInvalidArgumentCode, "External URI must be non-empty."});
    }

    ponder::core::VoidResult validation = ValidateNullTerminatedUtf8(uri, "External URI");
    if (!validation)
    {
        return validation;
    }

    const std::string ownedUri{uri};
    if (!SDL_OpenURL(ownedUri.c_str()))
    {
        return ponder::core::VoidResult::FromError(CaptureSdlFailure(kBackendFailureCode, "SDL_OpenURL", "external URI"));
    }
    return ponder::core::VoidResult::Success();
}

ponder::core::Result<std::string> SdlRuntime::ClipboardGetText() const
{
    const std::scoped_lock lock{m_clipboardMutex};
    VerifyOwnerThread("clipboard text query");

    static_cast<void>(SDL_ClearError());
    char* const rawText = SDL_GetClipboardText();
    [[maybe_unused]] auto freeText = ponder::core::MakeScopeExit(
        [rawText]() noexcept
        {
            SDL_free(rawText);
        });

    const char* const rawError = SDL_GetError();
    const std::string errorText = rawError != nullptr ? std::string{rawError} : std::string{};
    if (rawText == nullptr || (rawText[0] == '\0' && !errorText.empty()))
    {
        return ponder::core::Result<std::string>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetClipboardText", "clipboard text", errorText));
    }

    std::string text{rawText};
    if (!ponder::core::IsValidUtf8WithoutEmbeddedNull(text))
    {
        return ponder::core::Result<std::string>::FromError(
            ponder::core::Error{kBackendFailureCode, "SDL_GetClipboardText returned text that is not valid null-free UTF-8."});
    }
    return text;
}

ponder::core::VoidResult SdlRuntime::ClipboardSetText(std::string_view text)
{
    const std::scoped_lock lock{m_clipboardMutex};
    VerifyOwnerThread("clipboard text update");
    if (!ponder::core::IsValidUtf8WithoutEmbeddedNull(text))
    {
        return ponder::core::VoidResult::FromError(ponder::core::Error{kInvalidArgumentCode, "Clipboard text must be null-free UTF-8."});
    }

    const std::string ownedText{text};
    if (!SDL_SetClipboardText(ownedText.c_str()))
    {
        return ponder::core::VoidResult::FromError(CaptureSdlFailure(kBackendFailureCode, "SDL_SetClipboardText", "clipboard text"));
    }
    return ponder::core::VoidResult::Success();
}

std::vector<std::uint32_t> SdlRuntime::EnumerateBackendDisplays() const
{
    VerifyOwnerThread("display refresh");

    std::vector<std::uint32_t> backendDisplayIds = m_displayBackend.Enumerate();

    std::unordered_set<std::uint32_t> uniqueIds;
    uniqueIds.reserve(backendDisplayIds.size());
    for (const std::uint32_t backendDisplayId : backendDisplayIds)
    {
        if (backendDisplayId == 0 || !uniqueIds.insert(backendDisplayId).second)
        {
            throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL_GetDisplays returned a zero or duplicate display ID.");
        }
    }

    return backendDisplayIds;
}

DisplayTopologyUpdate SdlRuntime::PrepareDisplayRefresh(std::span<const std::uint32_t> backendDisplayIds)
{
    VerifyOwnerThread("display refresh");

    std::unordered_set<std::uint32_t> uniqueIds{backendDisplayIds.begin(), backendDisplayIds.end()};
    PONDER_VERIFY(uniqueIds.size() == backendDisplayIds.size(), "Validated display refresh contains duplicate backend IDs");

    std::size_t pendingConnectionCount = 0;
    std::size_t newBackendMappingCount = 0;
    for (const std::uint32_t backendDisplayId : backendDisplayIds)
    {
        const auto current = m_displaysByBackendId.find(backendDisplayId);
        const bool wasConnected = current != m_displaysByBackendId.end() && current->second.connected;
        if (wasConnected)
        {
            const DisplayId id = current->second.id;
            const auto project = m_displaysById.find(id);
            PONDER_VERIFY(project != m_displaysById.end(), "Connected display {} has no project mapping", id.GetValue());
            continue;
        }

        ++pendingConnectionCount;
        newBackendMappingCount += current == m_displaysByBackendId.end() ? 1U : 0U;
    }

    DisplayTopologyUpdate update{*this};
    update.MarkDisplayRefreshComplete();
    update.SetConnectedBackendIds(backendDisplayIds);
    update.ReserveLifecycleUpdates(m_connectedBackendDisplayIds.size());
    update.ReserveConnections(pendingConnectionCount, newBackendMappingCount);

    for (const std::uint32_t backendDisplayId : m_connectedBackendDisplayIds)
    {
        if (uniqueIds.contains(backendDisplayId))
        {
            continue;
        }

        update.StageCurrentDisconnection(backendDisplayId);
    }

    for (const std::uint32_t backendDisplayId : backendDisplayIds)
    {
        const auto current = m_displaysByBackendId.find(backendDisplayId);
        if (current != m_displaysByBackendId.end() && current->second.connected)
        {
            continue;
        }

        const bool queueAdditionEvent = current != m_displaysByBackendId.end() || m_hasCompletedDisplayRefresh;
        static_cast<void>(update.StageConnection(backendDisplayId, queueAdditionEvent));
    }

    return update;
}

std::vector<std::uint32_t> SdlRuntime::RefreshDisplays()
{
    std::vector<std::uint32_t> backendDisplayIds = EnumerateBackendDisplays();
    DisplayTopologyUpdate update = PrepareDisplayRefresh(backendDisplayIds);
    update.Commit();
    return backendDisplayIds;
}

DisplayInfo SdlRuntime::QueryDisplayInfo(DisplayId id, std::uint32_t backendDisplayId) const
{
    const std::string context = MakeDisplayContext(id);

    SdlDisplayBackend& displayBackend = m_displayBackend;
    std::string name = displayBackend.GetName(backendDisplayId);

    const BackendScreenRectangle backendBounds = displayBackend.GetBounds(backendDisplayId);
    const ScreenRectangle bounds = ConvertRectangle(backendBounds, "SDL_GetDisplayBounds", context);

    const BackendScreenRectangle backendUsableBounds = displayBackend.GetUsableBounds(backendDisplayId);
    const ScreenRectangle usableBounds = ConvertRectangle(backendUsableBounds, "SDL_GetDisplayUsableBounds", context);

    const float refreshRateHertz = displayBackend.GetCurrentRefreshRate(backendDisplayId);
    if (!std::isfinite(refreshRateHertz) || refreshRateHertz < 0.0F)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL_GetCurrentDisplayMode returned an invalid refresh rate for {}.", context);
    }
    const std::optional<float> refreshRate = refreshRateHertz > 0.0F ? std::optional<float>{refreshRateHertz} : std::nullopt;

    const DisplayOrientation orientation = ConvertOrientation(displayBackend.GetCurrentOrientation(backendDisplayId));

    const float contentScale = ValidateScale(displayBackend.GetContentScale(backendDisplayId), "SDL_GetDisplayContentScale", context);

    return DisplayInfo{id, std::move(name), bounds, usableBounds, refreshRate, orientation, contentScale};
}

std::optional<DisplayId> SdlRuntime::FindKnownDisplayId(std::uint32_t backendDisplayId) const
{
    VerifyOwnerThread("known display lookup");
    if (backendDisplayId == 0)
    {
        return std::nullopt;
    }

    const auto mapping = m_displaysByBackendId.find(backendDisplayId);
    if (mapping == m_displaysByBackendId.end())
    {
        return std::nullopt;
    }

    const auto projectMapping = m_displaysById.find(mapping->second.id);
    PONDER_VERIFY(projectMapping != m_displaysById.end() && projectMapping->second.backendId == backendDisplayId,
                  "Backend display {} has an inconsistent project mapping", backendDisplayId);
    return mapping->second.id;
}

std::optional<RuntimeDisplayLifecycleEvent> SdlRuntime::FindPendingDisplayLifecycleEvent(std::uint32_t backendDisplayId) const
{
    VerifyOwnerThread("pending display lifecycle lookup");
    if (backendDisplayId == 0)
    {
        return std::nullopt;
    }

    const auto mapping = m_displaysByBackendId.find(backendDisplayId);
    if (mapping == m_displaysByBackendId.end() || mapping->second.pendingLifecycleEvents.empty())
    {
        return std::nullopt;
    }

    const RuntimeDisplayLifecycleEvent pending = mapping->second.pendingLifecycleEvents.front();
    const auto projectMapping = m_displaysById.find(pending.id);
    PONDER_VERIFY(projectMapping != m_displaysById.end() && projectMapping->second.backendId == backendDisplayId,
                  "Backend display {} has an inconsistent pending lifecycle mapping", backendDisplayId);
    return pending;
}

DisplayTopologyUpdate SdlRuntime::ConnectDisplayFromEvent(std::uint32_t backendDisplayId)
{
    VerifyOwnerThread("display connection event");
    if (backendDisplayId == 0)
    {
        return DisplayTopologyUpdate{*this};
    }

    const auto current = m_displaysByBackendId.find(backendDisplayId);
    if (current != m_displaysByBackendId.end() && current->second.connected)
    {
        return DisplayTopologyUpdate{*this};
    }

    std::vector<std::uint32_t> nextConnectedBackendIds = m_connectedBackendDisplayIds;
    PONDER_VERIFY(std::ranges::find(nextConnectedBackendIds, backendDisplayId) == nextConnectedBackendIds.end(),
                  "Disconnected backend display {} remained in the connected-display order", backendDisplayId);
    nextConnectedBackendIds.emplace_back(backendDisplayId);

    DisplayTopologyUpdate update{*this};
    update.SetConnectedBackendIds(nextConnectedBackendIds);
    update.ReserveConnections(1, current == m_displaysByBackendId.end() ? 1U : 0U);
    static_cast<void>(update.StageConnection(backendDisplayId, false));
    return update;
}

void SdlRuntime::AcknowledgePendingDisplayLifecycleEvent(std::uint32_t backendDisplayId, const RuntimeDisplayLifecycleEvent& pending)
{
    VerifyOwnerThread("pending display lifecycle acknowledgement");
    const auto mapping = m_displaysByBackendId.find(backendDisplayId);
    PONDER_VERIFY(mapping != m_displaysByBackendId.end() && !mapping->second.pendingLifecycleEvents.empty() &&
                      mapping->second.pendingLifecycleEvents.front().kind == pending.kind &&
                      mapping->second.pendingLifecycleEvents.front().id == pending.id,
                  "Backend display {} pending lifecycle event changed before acknowledgement", backendDisplayId);

    mapping->second.pendingLifecycleEvents.pop_front();
    if (pending.kind == RuntimeDisplayLifecycleEventKind::Removed)
    {
        mapping->second.lastRemovedEventId = pending.id;
        mapping->second.unconfirmedRemovedEventId.reset();
    }
}

DisplayTopologyUpdate SdlRuntime::DisconnectDisplayFromEvent(std::uint32_t backendDisplayId, DisplayId id)
{
    VerifyOwnerThread("display disconnection event");
    DisplayTopologyUpdate update{*this};
    update.ReserveLifecycleUpdates(1);
    update.StageRemoval(backendDisplayId, id);
    return update;
}

DisplayTopologyUpdate SdlRuntime::ReconcileDisplayFromEvent(std::uint32_t backendDisplayId)
{
    VerifyOwnerThread("display event reconciliation");
    if (backendDisplayId == 0 || FindConnectedDisplayId(backendDisplayId).has_value() || FindKnownDisplayId(backendDisplayId).has_value())
    {
        return DisplayTopologyUpdate{*this};
    }

    std::vector<std::uint32_t> backendDisplayIds = EnumerateBackendDisplays();
    return PrepareDisplayRefresh(backendDisplayIds);
}

void SdlRuntime::ObserveWindowShownEvent(std::uint32_t backendWindowId)
{
    m_windowRegistry.ObserveWindowShownEvent(backendWindowId);
}

void SdlRuntime::DestroySystemCursors() noexcept
{
    for (CursorHandle& cursor : m_systemCursors)
    {
        if (cursor.IsValid())
        {
            try
            {
                SDL_DestroyCursor(ToSdlCursor(cursor));
            }
            catch (const ponder::core::Exception& exception)
            {
                LOG_ERROR_CATEGORY(kLogCategory, "Failed to destroy system cursor {}: {}", cursor, exception.GetMessage());
            }
            catch (const std::exception& exception)
            {
                LOG_ERROR_CATEGORY(kLogCategory, "Failed to destroy system cursor {}: {}", cursor, exception.what());
            }
            catch (...)
            {
                LOG_ERROR_CATEGORY(kLogCategory, "Failed to destroy system cursor {} with an unknown exception", cursor);
            }
            cursor = CursorHandle{};
        }
    }
}

namespace
{
[[nodiscard]] std::optional<SystemCursorShape> FromSdlSystemCursorValue(int value) noexcept
{
    switch (value)
    {
    case SDL_SYSTEM_CURSOR_DEFAULT:
        return SystemCursorShape::Default;
    case SDL_SYSTEM_CURSOR_TEXT:
        return SystemCursorShape::TextInput;
    case SDL_SYSTEM_CURSOR_MOVE:
        return SystemCursorShape::Move;
    case SDL_SYSTEM_CURSOR_NS_RESIZE:
        return SystemCursorShape::ResizeNorthSouth;
    case SDL_SYSTEM_CURSOR_EW_RESIZE:
        return SystemCursorShape::ResizeEastWest;
    case SDL_SYSTEM_CURSOR_NESW_RESIZE:
        return SystemCursorShape::ResizeNortheastSouthwest;
    case SDL_SYSTEM_CURSOR_NWSE_RESIZE:
        return SystemCursorShape::ResizeNorthwestSoutheast;
    case SDL_SYSTEM_CURSOR_POINTER:
        return SystemCursorShape::Pointer;
    case SDL_SYSTEM_CURSOR_WAIT:
        return SystemCursorShape::Wait;
    case SDL_SYSTEM_CURSOR_PROGRESS:
        return SystemCursorShape::Progress;
    case SDL_SYSTEM_CURSOR_NOT_ALLOWED:
        return SystemCursorShape::NotAllowed;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] constexpr bool IsValid(SystemCursorShape value) noexcept
{
    switch (value)
    {
    case SystemCursorShape::Default:
    case SystemCursorShape::TextInput:
    case SystemCursorShape::Move:
    case SystemCursorShape::ResizeNorthSouth:
    case SystemCursorShape::ResizeEastWest:
    case SystemCursorShape::ResizeNortheastSouthwest:
    case SystemCursorShape::ResizeNorthwestSoutheast:
    case SystemCursorShape::Pointer:
    case SystemCursorShape::Wait:
    case SystemCursorShape::Progress:
    case SystemCursorShape::NotAllowed:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(hints::EventLoggingLevel value) noexcept
{
    switch (value)
    {
    case hints::EventLoggingLevel::Disabled:
    case hints::EventLoggingLevel::Common:
    case hints::EventLoggingLevel::Verbose:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(hints::ImeUiCapabilities value) noexcept
{
    switch (value)
    {
    case hints::ImeUiCapabilities::None:
    case hints::ImeUiCapabilities::Composition:
    case hints::ImeUiCapabilities::Candidates:
    case hints::ImeUiCapabilities::CompositionAndCandidates:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsValid(hints::FullscreenFocusLossBehavior value) noexcept
{
    switch (value)
    {
    case hints::FullscreenFocusLossBehavior::Automatic:
    case hints::FullscreenFocusLossBehavior::Minimize:
    case hints::FullscreenFocusLossBehavior::KeepFullscreen:
        return true;
    }

    return false;
}

void SetHintOverride(const char* name, const char* value)
{
    if (!SDL_SetHintWithPriority(name, value, SDL_HINT_OVERRIDE))
    {
        const std::string message = CaptureSdlFailureMessage("SDL_SetHintWithPriority", name);
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", message);
    }
}

void ResetHint(const char* name)
{
    if (!SDL_ResetHint(name))
    {
        const std::string message = CaptureSdlFailureMessage("SDL_ResetHint", name);
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{}", message);
    }
}

static_assert(SDL_SYSTEM_CURSOR_DEFAULT == 0);
static_assert(SDL_SYSTEM_CURSOR_TEXT == 1);
static_assert(SDL_SYSTEM_CURSOR_WAIT == 2);
static_assert(SDL_SYSTEM_CURSOR_PROGRESS == 4);
static_assert(SDL_SYSTEM_CURSOR_NWSE_RESIZE == 5);
static_assert(SDL_SYSTEM_CURSOR_NESW_RESIZE == 6);
static_assert(SDL_SYSTEM_CURSOR_EW_RESIZE == 7);
static_assert(SDL_SYSTEM_CURSOR_NS_RESIZE == 8);
static_assert(SDL_SYSTEM_CURSOR_MOVE == 9);
static_assert(SDL_SYSTEM_CURSOR_NOT_ALLOWED == 10);
static_assert(SDL_SYSTEM_CURSOR_POINTER == 11);
} // namespace

void SdlRuntime::HintPushRaw(const char* name, bool beforeInitialization, bool pushOnce, std::string value)
{
    m_ownerThread.Verify("hint mutation");
    if (!SDL_IsMainThread())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::WrongThread, "Platform hint mutation must run on SDL's main thread.");
    }

    HintBeginMutation();
    auto endMutation = ponder::core::MakeScopeExit(
        [this]() noexcept
        {
            m_hintMutationActive = false;
        });

    HintValidateMutationPhase(name, beforeInitialization);

    const auto existingState = m_hintStates.find(name);
    const char* const currentValue = SDL_GetHint(name);
    const std::optional<std::string> effectiveValueBeforeMutation = currentValue != nullptr ? std::optional<std::string>{currentValue} : std::nullopt;
    const bool everPushed = existingState != m_hintStates.end() && existingState->second.everPushed;
    if (pushOnce && (everPushed || currentValue != nullptr))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "{} can only be set once and is already set.", name);
    }

    const bool firstValue = existingState == m_hintStates.end() || existingState->second.values.empty();
    std::optional<std::string> originalValue;
    if (firstValue)
    {
        originalValue = effectiveValueBeforeMutation;
    }

    auto [stateIterator, inserted] = m_hintStates.try_emplace(name);
    SdlHintValueState& state = stateIterator->second;
    bool activeHintAdded{};
    bool valueAdded{};
    try
    {
        if (firstValue)
        {
            state.originalValue = std::move(originalValue);
            state.originalCaptured = true;
            m_activeHintNames.emplace_back(name);
            activeHintAdded = true;
        }

        state.values.push_back(std::move(value));
        valueAdded = true;
        HintSetValue(name, state.values.back(), effectiveValueBeforeMutation);
    }
    catch (...)
    {
        if (valueAdded)
        {
            state.values.pop_back();
        }
        if (firstValue)
        {
            state.originalValue.reset();
            state.originalCaptured = false;
            if (activeHintAdded)
            {
                m_activeHintNames.pop_back();
            }
        }
        if (inserted)
        {
            m_hintStates.erase(stateIterator);
        }
        throw;
    }

    state.everPushed = true;
}

void SdlRuntime::HintPopRaw(const char* name, bool beforeInitialization)
{
    m_ownerThread.Verify("hint pop");
    if (!SDL_IsMainThread())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::WrongThread, "Platform hint pop must run on SDL's main thread.");
    }

    HintBeginMutation();
    auto endMutation = ponder::core::MakeScopeExit(
        [this]() noexcept
        {
            m_hintMutationActive = false;
        });

    HintValidateMutationPhase(name, beforeInitialization);

    const auto stateIterator = m_hintStates.find(name);
    if (stateIterator == m_hintStates.end() || stateIterator->second.values.empty())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::NotFound, "{} does not have a managed value to pop.", name);
    }

    SdlHintValueState& state = stateIterator->second;
    const auto activeIterator = state.values.size() == 1 ? HintFindActive(name) : m_activeHintNames.end();
    if (state.values.size() > 1)
    {
        const char* const currentValue = SDL_GetHint(name);
        const std::optional<std::string> effectiveValueBeforeMutation =
            currentValue != nullptr ? std::optional<std::string>{currentValue} : std::nullopt;
        HintSetValue(name, state.values[state.values.size() - 2], effectiveValueBeforeMutation);
    }
    else
    {
        HintRestoreOriginalValue(name, state);
    }

    state.values.pop_back();
    if (state.values.empty())
    {
        HintFinishActivation(activeIterator, state);
    }
}

void SdlRuntime::HintClearRaw(const char* name, bool beforeInitialization)
{
    m_ownerThread.Verify("hint clear");
    if (!SDL_IsMainThread())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::WrongThread, "Platform hint clear must run on SDL's main thread.");
    }

    HintBeginMutation();
    auto endMutation = ponder::core::MakeScopeExit(
        [this]() noexcept
        {
            m_hintMutationActive = false;
        });

    HintValidateMutationPhase(name, beforeInitialization);

    const auto stateIterator = m_hintStates.find(name);
    if (stateIterator == m_hintStates.end() || stateIterator->second.values.empty())
    {
        return;
    }

    SdlHintValueState& state = stateIterator->second;
    const auto activeIterator = HintFindActive(name);
    HintRestoreOriginalValue(name, state);

    state.values.clear();
    HintFinishActivation(activeIterator, state);
}

std::optional<std::string> SdlRuntime::HintGetRaw(const char* name) const
{
    m_ownerThread.Verify("hint query");
    if (!SDL_IsMainThread())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::WrongThread, "Platform hint query must run on SDL's main thread.");
    }

    const char* const value = SDL_GetHint(name);
    return value != nullptr ? std::optional<std::string>{value} : std::nullopt;
}

void SdlRuntime::HintBeginMutation()
{
    if (m_hintMutationActive)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Reentrant platform hint mutations are not supported.");
    }
    m_hintMutationActive = true;
}

void SdlRuntime::HintValidateMutationPhase(const char* name, bool beforeInitialization) const
{
    if (beforeInitialization && SDL_WasInit(0) != 0)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "{} can only be changed before SDL is initialized.", name);
    }
}

bool SdlRuntime::HintMatchesValue(const char* name, const std::optional<std::string>& value) const noexcept
{
    const char* const currentValue = SDL_GetHint(name);
    return value.has_value() ? currentValue != nullptr && std::string_view{currentValue} == *value : currentValue == nullptr;
}

void SdlRuntime::HintRestoreValue(const char* name, const std::optional<std::string>& value)
{
    if (HintMatchesValue(name, value))
    {
        return;
    }

    try
    {
        if (value.has_value())
        {
            SetHintOverride(name, value->c_str());
        }
        else
        {
            ResetHint(name);
        }
    }
    catch (...)
    {
        if (!HintMatchesValue(name, value))
        {
            throw;
        }
    }
}

void SdlRuntime::HintSetValue(const char* name, const std::string& value, const std::optional<std::string>& rollbackValue)
{
    try
    {
        SetHintOverride(name, value.c_str());
    }
    catch (...)
    {
        const std::exception_ptr failure = std::current_exception();
        HintRestoreValue(name, rollbackValue);
        std::rethrow_exception(failure);
    }
}

void SdlRuntime::HintRestoreOriginalValue(const char* name, const SdlHintValueState& state)
{
    PONDER_VERIFY(state.originalCaptured, "Cannot restore a platform hint without its original value");
    HintRestoreValue(name, state.originalValue);
}

auto SdlRuntime::HintFindActive(const char* name) -> std::vector<std::string>::iterator
{
    const auto activeIterator = std::ranges::find(m_activeHintNames, name);
    PONDER_VERIFY(activeIterator != m_activeHintNames.end(), "Active platform hint is missing from restoration order");
    return activeIterator;
}

void SdlRuntime::HintFinishActivation(std::vector<std::string>::iterator activeIterator, SdlHintValueState& state)
{
    m_activeHintNames.erase(activeIterator);
    state.originalValue.reset();
    state.originalCaptured = false;
}

void SdlRuntime::HintRestoreAll() noexcept
{
    m_hintMutationActive = true;
    auto endMutation = ponder::core::MakeScopeExit(
        [this]() noexcept
        {
            m_hintMutationActive = false;
        });

    for (auto iterator = m_activeHintNames.rbegin(); iterator != m_activeHintNames.rend(); ++iterator)
    {
        try
        {
            const auto stateIterator = m_hintStates.find(*iterator);
            if (stateIterator == m_hintStates.end())
            {
                LOG_ERROR_CATEGORY(kLogCategory, "Platform hint restoration bookkeeping is inconsistent");
                continue;
            }

            HintRestoreOriginalValue(iterator->c_str(), stateIterator->second);
        }
        catch (const ponder::core::Exception& exception)
        {
            LOG_ERROR_CATEGORY(kLogCategory, "Platform hint restoration failed: {}", exception.GetMessage());
        }
        catch (const std::exception& exception)
        {
            LOG_ERROR_CATEGORY(kLogCategory, "Platform hint restoration failed: {}", exception.what());
        }
        catch (...)
        {
            LOG_ERROR_CATEGORY(kLogCategory, "Platform hint restoration failed");
        }
    }

    m_activeHintNames.clear();
    for (auto& [name, state] : m_hintStates)
    {
        static_cast<void>(name);
        state.values.clear();
        state.originalValue.reset();
        state.originalCaptured = false;
    }
}

#define PONDER_DEFINE_BOOLEAN_HINT(Type, Name, BeforeInitialization, PushOnce)                                                                       \
    template <>                                                                                                                                      \
    void SdlRuntime::HintPush<hints::Type>(const hints::Type& hint)                                                                                  \
    {                                                                                                                                                \
        HintPushRaw(Name, BeforeInitialization, PushOnce, std::format("{}", hint));                                                                  \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void SdlRuntime::HintPop<hints::Type>()                                                                                                          \
    {                                                                                                                                                \
        HintPopRaw(Name, BeforeInitialization);                                                                                                      \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void SdlRuntime::HintClear<hints::Type>()                                                                                                        \
    {                                                                                                                                                \
        HintClearRaw(Name, BeforeInitialization);                                                                                                    \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    std::optional<hints::Type> SdlRuntime::HintGet<hints::Type>() const                                                                              \
    {                                                                                                                                                \
        std::optional<std::string> rawValue = HintGetRaw(Name);                                                                                      \
        if (!rawValue.has_value())                                                                                                                   \
        {                                                                                                                                            \
            return std::nullopt;                                                                                                                     \
        }                                                                                                                                            \
        const std::string& value = *rawValue;                                                                                                        \
        if (value.empty())                                                                                                                           \
        {                                                                                                                                            \
            throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{} has an empty boolean value.", Name);                                     \
        }                                                                                                                                            \
        return hints::Type{value.front() != '0' && !ponder::core::EqualsCaseInsensitive(value, "false")};                                            \
    }

PONDER_DEFINE_BOOLEAN_HINT(AllowAltTabWhileGrabbed, SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED, false, false);
PONDER_DEFINE_BOOLEAN_HINT(PollSentinel, SDL_HINT_POLL_SENTINEL, false, false);
PONDER_DEFINE_BOOLEAN_HINT(QuitOnLastWindowClose, SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, false, false);

PONDER_DEFINE_BOOLEAN_HINT(VideoAllowScreensaver, SDL_HINT_VIDEO_ALLOW_SCREENSAVER, true, true);
PONDER_DEFINE_BOOLEAN_HINT(VideoDoubleBuffer, SDL_HINT_VIDEO_DOUBLE_BUFFER, true, false);
PONDER_DEFINE_BOOLEAN_HINT(VideoForceEgl, SDL_HINT_VIDEO_FORCE_EGL, true, false);
PONDER_DEFINE_BOOLEAN_HINT(VideoSyncWindowOperations, SDL_HINT_VIDEO_SYNC_WINDOW_OPERATIONS, false, false);

PONDER_DEFINE_BOOLEAN_HINT(WindowActivateWhenRaised, SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED, false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowActivateWhenShown, SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowAllowTopmost, SDL_HINT_WINDOW_ALLOW_TOPMOST, false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowFrameUsableWhileCursorHidden, SDL_HINT_WINDOW_FRAME_USABLE_WHILE_CURSOR_HIDDEN, false, false);

PONDER_DEFINE_BOOLEAN_HINT(MouseAutoCapture, SDL_HINT_MOUSE_AUTO_CAPTURE, false, false);
PONDER_DEFINE_BOOLEAN_HINT(MouseDpiScaleCursors, SDL_HINT_MOUSE_DPI_SCALE_CURSORS, true, false);
PONDER_DEFINE_BOOLEAN_HINT(MouseEmulateWarpWithRelative, SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE, false, false);
PONDER_DEFINE_BOOLEAN_HINT(MouseFocusClickThrough, SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, false, false);
PONDER_DEFINE_BOOLEAN_HINT(MouseRelativeCursorVisible, SDL_HINT_MOUSE_RELATIVE_CURSOR_VISIBLE, false, false);
PONDER_DEFINE_BOOLEAN_HINT(MouseRelativeModeCenter, SDL_HINT_MOUSE_RELATIVE_MODE_CENTER, false, false);
PONDER_DEFINE_BOOLEAN_HINT(MouseRelativeSystemScale, SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, false, false);
PONDER_DEFINE_BOOLEAN_HINT(MouseRelativeWarpMotion, SDL_HINT_MOUSE_RELATIVE_WARP_MOTION, false, false);
PONDER_DEFINE_BOOLEAN_HINT(MouseTouchEvents, SDL_HINT_MOUSE_TOUCH_EVENTS, false, false);
PONDER_DEFINE_BOOLEAN_HINT(PenMouseEvents, SDL_HINT_PEN_MOUSE_EVENTS, false, false);
PONDER_DEFINE_BOOLEAN_HINT(PenTouchEvents, SDL_HINT_PEN_TOUCH_EVENTS, false, false);
PONDER_DEFINE_BOOLEAN_HINT(TouchMouseEvents, SDL_HINT_TOUCH_MOUSE_EVENTS, false, false);
PONDER_DEFINE_BOOLEAN_HINT(TrackpadIsTouchOnly, SDL_HINT_TRACKPAD_IS_TOUCH_ONLY, true, false);

#if defined(__APPLE__)
PONDER_DEFINE_BOOLEAN_HINT(MacCtrlClickEmulatesRightClick, SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK, false, false);
PONDER_DEFINE_BOOLEAN_HINT(MacScrollMomentum, SDL_HINT_MAC_SCROLL_MOMENTUM, true, false);
PONDER_DEFINE_BOOLEAN_HINT(VideoMacFullscreenSpaces, SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, true, false);
#endif

#if defined(_WIN32)
PONDER_DEFINE_BOOLEAN_HINT(WindowsCloseOnAltF4, SDL_HINT_WINDOWS_CLOSE_ON_ALT_F4, false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowsEnableMenuMnemonics, SDL_HINT_WINDOWS_ENABLE_MENU_MNEMONICS, false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowsGameInput, SDL_HINT_WINDOWS_GAMEINPUT, true, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowsRawKeyboard, SDL_HINT_WINDOWS_RAW_KEYBOARD, false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowsRawKeyboardExcludeHotkeys, SDL_HINT_WINDOWS_RAW_KEYBOARD_EXCLUDE_HOTKEYS, false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowsRawKeyboardInputSink, SDL_HINT_WINDOWS_RAW_KEYBOARD_INPUTSINK, false, false);
PONDER_DEFINE_BOOLEAN_HINT(WindowsRawMouseNoLegacy, SDL_HINT_WINDOWS_RAW_MOUSE_NOLEGACY, false, false);
#endif

#if defined(__linux__)
PONDER_DEFINE_BOOLEAN_HINT(VideoWaylandAllowLibdecor, SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, true, false);
PONDER_DEFINE_BOOLEAN_HINT(VideoWaylandModeEmulation, SDL_HINT_VIDEO_WAYLAND_MODE_EMULATION, true, false);
PONDER_DEFINE_BOOLEAN_HINT(VideoWaylandPreferLibdecor, SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR, true, false);
PONDER_DEFINE_BOOLEAN_HINT(VideoWaylandScaleToDisplay, SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY, true, false);
PONDER_DEFINE_BOOLEAN_HINT(VideoX11NetWmBypassCompositor, SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, true, false);
PONDER_DEFINE_BOOLEAN_HINT(VideoX11Xrandr, SDL_HINT_VIDEO_X11_XRANDR, true, false);
#endif

#undef PONDER_DEFINE_BOOLEAN_HINT

#define PONDER_DEFINE_HINT_POP_AND_CLEAR(Type, Name, BeforeInitialization)                                                                           \
    template <>                                                                                                                                      \
    void SdlRuntime::HintPop<hints::Type>()                                                                                                          \
    {                                                                                                                                                \
        HintPopRaw(Name, BeforeInitialization);                                                                                                      \
    }                                                                                                                                                \
                                                                                                                                                     \
    template <>                                                                                                                                      \
    void SdlRuntime::HintClear<hints::Type>()                                                                                                        \
    {                                                                                                                                                \
        HintClearRaw(Name, BeforeInitialization);                                                                                                    \
    }

template <>
void SdlRuntime::HintPush<hints::EventLogging>(const hints::EventLogging& hint)
{
    if (!IsValid(hint.value))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform event logging level is invalid.");
    }

    HintPushRaw(SDL_HINT_EVENT_LOGGING, false, false, std::format("{}", hint));
}

PONDER_DEFINE_HINT_POP_AND_CLEAR(EventLogging, SDL_HINT_EVENT_LOGGING, false);

template <>
std::optional<hints::EventLogging> SdlRuntime::HintGet<hints::EventLogging>() const
{
    const std::optional<std::string> value = HintGetRaw(SDL_HINT_EVENT_LOGGING);
    if (!value.has_value())
    {
        return std::nullopt;
    }

    if (*value == "0")
    {
        return hints::EventLogging{hints::EventLoggingLevel::Disabled};
    }
    if (*value == "1")
    {
        return hints::EventLogging{hints::EventLoggingLevel::Common};
    }
    if (*value == "2")
    {
        return hints::EventLogging{hints::EventLoggingLevel::Verbose};
    }
    throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL returned an invalid event-logging platform hint.");
}

template <>
void SdlRuntime::HintPush<hints::ImeImplementedUi>(const hints::ImeImplementedUi& hint)
{
    if (!IsValid(hint.value))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform IME UI capabilities value is invalid.");
    }

    HintPushRaw(SDL_HINT_IME_IMPLEMENTED_UI, true, false, std::format("{}", hint));
}

PONDER_DEFINE_HINT_POP_AND_CLEAR(ImeImplementedUi, SDL_HINT_IME_IMPLEMENTED_UI, true);

template <>
std::optional<hints::ImeImplementedUi> SdlRuntime::HintGet<hints::ImeImplementedUi>() const
{
    const std::optional<std::string> value = HintGetRaw(SDL_HINT_IME_IMPLEMENTED_UI);
    if (!value.has_value())
    {
        return std::nullopt;
    }

    if (*value == "0" || *value == "none")
    {
        return hints::ImeImplementedUi{hints::ImeUiCapabilities::None};
    }
    if (*value == "composition")
    {
        return hints::ImeImplementedUi{hints::ImeUiCapabilities::Composition};
    }
    if (*value == "candidates")
    {
        return hints::ImeImplementedUi{hints::ImeUiCapabilities::Candidates};
    }
    if (*value == "composition,candidates" || *value == "candidates,composition")
    {
        return hints::ImeImplementedUi{hints::ImeUiCapabilities::CompositionAndCandidates};
    }
    throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL returned invalid IME UI capabilities.");
}

#define PONDER_DEFINE_STRING_HINT(Type, Name, BeforeInitialization)                                                                                  \
    template <>                                                                                                                                      \
    void SdlRuntime::HintPush<hints::Type>(const hints::Type& hint)                                                                                  \
    {                                                                                                                                                \
        if (hint.value.empty() || hint.value.find('\0') != std::string::npos)                                                                        \
        {                                                                                                                                            \
            throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument,                                                                             \
                                     "Platform hint string value must be non-empty and contain no embedded nulls.");                                 \
        }                                                                                                                                            \
        HintPushRaw(Name, BeforeInitialization, false, std::format("{}", hint));                                                                     \
    }                                                                                                                                                \
    PONDER_DEFINE_HINT_POP_AND_CLEAR(Type, Name, BeforeInitialization);                                                                              \
    template <>                                                                                                                                      \
    std::optional<hints::Type> SdlRuntime::HintGet<hints::Type>() const                                                                              \
    {                                                                                                                                                \
        std::optional<std::string> value = HintGetRaw(Name);                                                                                         \
        if (!value.has_value())                                                                                                                      \
        {                                                                                                                                            \
            return std::nullopt;                                                                                                                     \
        }                                                                                                                                            \
        if (value->empty())                                                                                                                          \
        {                                                                                                                                            \
            throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "{} has an empty string value.", Name);                                      \
        }                                                                                                                                            \
        return hints::Type{std::move(*value)};                                                                                                       \
    }

PONDER_DEFINE_STRING_HINT(VideoDriver, SDL_HINT_VIDEO_DRIVER, true);
#if defined(__linux__)
PONDER_DEFINE_STRING_HINT(VideoDisplayPriority, SDL_HINT_VIDEO_DISPLAY_PRIORITY, true);
#endif
#undef PONDER_DEFINE_STRING_HINT

template <>
void SdlRuntime::HintPush<hints::VideoMinimizeOnFocusLoss>(const hints::VideoMinimizeOnFocusLoss& hint)
{
    if (!IsValid(hint.value))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform fullscreen focus-loss behavior is invalid.");
    }

    HintPushRaw(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, false, false, std::format("{}", hint));
}

PONDER_DEFINE_HINT_POP_AND_CLEAR(VideoMinimizeOnFocusLoss, SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, false);

template <>
std::optional<hints::VideoMinimizeOnFocusLoss> SdlRuntime::HintGet<hints::VideoMinimizeOnFocusLoss>() const
{
    const std::optional<std::string> value = HintGetRaw(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS);
    if (!value.has_value())
    {
        return std::nullopt;
    }

    if (*value == "auto")
    {
        return hints::VideoMinimizeOnFocusLoss{hints::FullscreenFocusLossBehavior::Automatic};
    }
    if (*value == "1")
    {
        return hints::VideoMinimizeOnFocusLoss{hints::FullscreenFocusLossBehavior::Minimize};
    }
    if (*value == "0")
    {
        return hints::VideoMinimizeOnFocusLoss{hints::FullscreenFocusLossBehavior::KeepFullscreen};
    }
    throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL returned an invalid fullscreen focus-loss platform hint.");
}

template <>
void SdlRuntime::HintPush<hints::MouseDefaultSystemCursor>(const hints::MouseDefaultSystemCursor& hint)
{
    if (!IsValid(hint.value))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform hint system cursor value is invalid.");
    }

    HintPushRaw(SDL_HINT_MOUSE_DEFAULT_SYSTEM_CURSOR, true, false, std::format("{}", hint));
}

PONDER_DEFINE_HINT_POP_AND_CLEAR(MouseDefaultSystemCursor, SDL_HINT_MOUSE_DEFAULT_SYSTEM_CURSOR, true);

template <>
std::optional<hints::MouseDefaultSystemCursor> SdlRuntime::HintGet<hints::MouseDefaultSystemCursor>() const
{
    const std::optional<std::string> value = HintGetRaw(SDL_HINT_MOUSE_DEFAULT_SYSTEM_CURSOR);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    const std::optional<int> parsed = ponder::core::ParseNumber<int>(*value);
    if (!parsed.has_value())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL returned an invalid system-cursor platform hint.");
    }

    const std::optional<SystemCursorShape> cursor = FromSdlSystemCursorValue(*parsed);
    if (!cursor.has_value())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL returned an unsupported system-cursor platform hint.");
    }
    return hints::MouseDefaultSystemCursor{*cursor};
}

template <>
void SdlRuntime::HintPush<hints::MouseDoubleClickRadius>(const hints::MouseDoubleClickRadius& hint)
{
    if (hint.value > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform hint integer value exceeds SDL's supported range.");
    }

    HintPushRaw(SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS, false, false, std::format("{}", hint));
}

PONDER_DEFINE_HINT_POP_AND_CLEAR(MouseDoubleClickRadius, SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS, false);

template <>
std::optional<hints::MouseDoubleClickRadius> SdlRuntime::HintGet<hints::MouseDoubleClickRadius>() const
{
    const std::optional<std::string> value = HintGetRaw(SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    const std::optional<std::uint32_t> parsed = ponder::core::ParseNumber<std::uint32_t>(*value);
    if (!parsed.has_value() || *parsed > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL returned an invalid unsigned-integer platform hint.");
    }
    return hints::MouseDoubleClickRadius{*parsed};
}

template <>
void SdlRuntime::HintPush<hints::MouseDoubleClickTime>(const hints::MouseDoubleClickTime& hint)
{
    if (hint.value.count() < 0 || hint.value.count() > std::numeric_limits<int>::max())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform hint duration must fit in SDL's non-negative millisecond range.");
    }

    HintPushRaw(SDL_HINT_MOUSE_DOUBLE_CLICK_TIME, false, false, std::format("{}", hint));
}

PONDER_DEFINE_HINT_POP_AND_CLEAR(MouseDoubleClickTime, SDL_HINT_MOUSE_DOUBLE_CLICK_TIME, false);

template <>
std::optional<hints::MouseDoubleClickTime> SdlRuntime::HintGet<hints::MouseDoubleClickTime>() const
{
    const std::optional<std::string> value = HintGetRaw(SDL_HINT_MOUSE_DOUBLE_CLICK_TIME);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    const std::optional<std::int64_t> parsed = ponder::core::ParseNumber<std::int64_t>(*value);
    if (!parsed.has_value() || *parsed < 0 || *parsed > std::numeric_limits<int>::max())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL returned an invalid duration platform hint.");
    }
    return hints::MouseDoubleClickTime{std::chrono::milliseconds{*parsed}};
}

#define PONDER_DEFINE_FLOAT_HINT(Type, Name)                                                                                                         \
    template <>                                                                                                                                      \
    void SdlRuntime::HintPush<hints::Type>(const hints::Type& hint)                                                                                  \
    {                                                                                                                                                \
        if (!std::isfinite(hint.value))                                                                                                              \
        {                                                                                                                                            \
            throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform hint floating-point value must be finite.");                      \
        }                                                                                                                                            \
        HintPushRaw(Name, false, false, std::format("{}", hint));                                                                                    \
    }                                                                                                                                                \
    PONDER_DEFINE_HINT_POP_AND_CLEAR(Type, Name, false);                                                                                             \
    template <>                                                                                                                                      \
    std::optional<hints::Type> SdlRuntime::HintGet<hints::Type>() const                                                                              \
    {                                                                                                                                                \
        const std::optional<std::string> value = HintGetRaw(Name);                                                                                   \
        if (!value.has_value())                                                                                                                      \
        {                                                                                                                                            \
            return std::nullopt;                                                                                                                     \
        }                                                                                                                                            \
        const std::optional<float> parsed = ponder::core::ParseNumber<float>(*value);                                                                \
        if (!parsed.has_value() || !std::isfinite(*parsed))                                                                                          \
        {                                                                                                                                            \
            throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "SDL returned an invalid floating-point platform hint.");                    \
        }                                                                                                                                            \
        return hints::Type{*parsed};                                                                                                                 \
    }

PONDER_DEFINE_FLOAT_HINT(MouseNormalSpeedScale, SDL_HINT_MOUSE_NORMAL_SPEED_SCALE);
PONDER_DEFINE_FLOAT_HINT(MouseRelativeSpeedScale, SDL_HINT_MOUSE_RELATIVE_SPEED_SCALE);

#undef PONDER_DEFINE_FLOAT_HINT
#undef PONDER_DEFINE_HINT_POP_AND_CLEAR

namespace
{
[[nodiscard]] constexpr bool IsValidFilterPatternCharacter(char character) noexcept
{
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') ||
           character == '-' || character == '_' || character == '.';
}

[[nodiscard]] bool IsValidFilterPattern(std::string_view pattern) noexcept
{
    if (pattern == "*")
    {
        return true;
    }

    bool segmentHasCharacters{};
    for (const char character : pattern)
    {
        if (character == ';')
        {
            if (!segmentHasCharacters)
            {
                return false;
            }
            segmentHasCharacters = false;
            continue;
        }

        if (!IsValidFilterPatternCharacter(character))
        {
            return false;
        }
        segmentHasCharacters = true;
    }

    return segmentHasCharacters;
}

[[nodiscard]] std::optional<std::string> ValidateDefaultLocation(const std::optional<std::filesystem::path>& location)
{
    if (!location.has_value())
    {
        return std::optional<std::string>{};
    }

    std::string text = ::pond::io::PathToUtf8(*location);
    if (text.empty() || !ponder::core::IsValidUtf8WithoutEmbeddedNull(text))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument,
                                 "Dialog default location must be absent or non-empty UTF-8 without embedded nulls.");
    }

    return std::optional<std::string>{std::move(text)};
}

void ValidateFilter(const DialogFileFilter& filter, std::size_t index)
{
    if (filter.name.empty() || !ponder::core::IsValidUtf8WithoutEmbeddedNull(filter.name))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Dialog filter {} name must be non-empty UTF-8 without embedded nulls.", index);
    }

    if (filter.pattern.empty() || !ponder::core::IsValidUtf8WithoutEmbeddedNull(filter.pattern) || !IsValidFilterPattern(filter.pattern))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument,
                                 "Dialog filter {} pattern must be '*', or a semicolon-separated list of ASCII file "
                                 "extensions.",
                                 index);
    }
}

struct PreparedDialogRequest final
{
    DialogKind kind{DialogKind::OpenFile};
    std::optional<WindowId> parentWindowId;
    std::optional<std::string> defaultLocation;
    std::vector<DialogFileFilter> filters;
    bool allowMultipleSelection{};
};

[[nodiscard]] PreparedDialogRequest PrepareDialogRequest(DialogKind kind, std::optional<WindowId> parentWindowId,
                                                         const std::optional<std::filesystem::path>& defaultLocation,
                                                         std::span<const DialogFileFilter> filters, bool allowMultipleSelection)
{
    if (parentWindowId.has_value() && !parentWindowId->IsValid())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Dialog parent window ID must be absent or valid.");
    }

    if (!std::in_range<int>(filters.size()))
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Dialog filter count exceeds the backend representation range.");
    }

    PreparedDialogRequest prepared{.kind = kind,
                                   .parentWindowId = parentWindowId,
                                   .defaultLocation = ValidateDefaultLocation(defaultLocation),
                                   .filters = {},
                                   .allowMultipleSelection = allowMultipleSelection};
    prepared.filters.reserve(filters.size());

    for (std::size_t index = 0; index < filters.size(); ++index)
    {
        ValidateFilter(filters[index], index);
        prepared.filters.push_back(filters[index]);
    }
    return prepared;
}

[[nodiscard]] std::string_view GetDialogOperation(DialogKind kind) noexcept
{
    switch (kind)
    {
    case DialogKind::OpenFile:
        return "SDL_ShowOpenFileDialog";
    case DialogKind::SaveFile:
        return "SDL_ShowSaveFileDialog";
    case DialogKind::OpenFolder:
        return "SDL_ShowOpenFolderDialog";
    }

    return "SDL_ShowFileDialog";
}

[[nodiscard]] DialogFailure MakeDialogFailure(DialogKind kind, std::string_view message)
{
    const std::string text = message.empty() ? std::format("{} failed for dialog request.", GetDialogOperation(kind))
                                             : std::format("{} failed for dialog request: {}", GetDialogOperation(kind), message);
    return DialogFailure{ponder::core::Error{kBackendFailureCode, text}};
}
} // namespace

struct DialogCompletionRecord final
{
    ponder::core::Timestamp timestamp{};
    DialogOutcome outcome;
};

static_assert(std::is_nothrow_move_constructible_v<DialogOutcome>);
static_assert(std::is_nothrow_move_assignable_v<DialogOutcome>);
static_assert(std::is_nothrow_move_constructible_v<DialogCompletionRecord>);
static_assert(std::is_nothrow_move_assignable_v<DialogCompletionRecord>);

class DialogCallbackHandoff final
{
public:
    explicit DialogCallbackHandoff(SdlRuntime& runtime) noexcept :
        m_runtime(&runtime)
    {
    }

    void EnqueueCompletion(DialogRequestId id, ponder::core::Timestamp timestamp, DialogOutcome outcome);
    void MarkCallbackFailure(DialogRequestId id, ponder::core::Timestamp timestamp) noexcept;
    void Deactivate(SdlRuntime& runtime) noexcept;

private:
    std::mutex m_mutex;
    SdlRuntime* m_runtime;
};

class DialogRequestState final
{
public:
    void CompleteSelection(std::span<const std::string> paths, int selectedFilter) noexcept;
    void CompleteCancellation() noexcept;
    void CompleteFailure(std::string_view message) noexcept;
    void FailCallback() noexcept;

    std::shared_ptr<DialogCallbackHandoff> callbackHandoff;
    DialogRequestInfo info;
    std::optional<DialogParentLease> parentLease;
    std::optional<std::string> defaultLocation;
    std::vector<DialogFileFilter> filters;
    std::optional<DialogCompletionRecord> completion;
    std::optional<DialogCompletionRecord> callbackFailure;
    std::list<DialogRequestId> completionQueueNode;
    bool completionEnqueued{};
    bool completionIsCallbackFailure{};

private:
    void Complete(ponder::core::Timestamp timestamp, DialogOutcome outcome);
    void FailCallback(ponder::core::Timestamp timestamp) noexcept;
};

namespace
{
class SdlDialogContext final
{
public:
    explicit SdlDialogContext(const DialogBackendRequest& request) :
        completion(request.completion)
    {
        if (request.defaultLocation.has_value())
        {
            defaultLocation = *request.defaultLocation;
        }

        filters.assign(request.filters.begin(), request.filters.end());
        nativeFilters.reserve(filters.size());
        for (const DialogFileFilter& filter : filters)
        {
            nativeFilters.push_back(SDL_DialogFileFilter{.name = filter.name.c_str(), .pattern = filter.pattern.c_str()});
        }
    }

    DialogBackendCompletion completion;
    std::optional<std::string> defaultLocation;
    std::vector<DialogFileFilter> filters;
    std::vector<SDL_DialogFileFilter> nativeFilters;
};

[[nodiscard]] DialogOutcome MakeDialogSelection(const DialogRequestState& request, std::span<const std::string> selectedPaths, int selectedFilter)
{
    if (selectedPaths.empty())
    {
        return DialogCancellation{};
    }

    std::vector<std::filesystem::path> paths;
    paths.reserve(selectedPaths.size());
    for (const std::string& pathText : selectedPaths)
    {
        if (!ponder::core::IsValidUtf8WithoutEmbeddedNull(pathText))
        {
            return MakeDialogFailure(request.info.kind, "SDL returned a dialog path that was not valid UTF-8.");
        }
        paths.push_back(::pond::io::PathFromUtf8(pathText));
    }

    std::optional<std::size_t> selectedFilterIndex;
    if (selectedFilter >= 0)
    {
        selectedFilterIndex = static_cast<std::size_t>(selectedFilter);
    }

    if (selectedFilterIndex.has_value() && *selectedFilterIndex >= request.filters.size())
    {
        return MakeDialogFailure(request.info.kind, "SDL returned an out-of-range selected dialog filter index.");
    }

    return DialogSelection{.paths = std::move(paths), .selectedFilterIndex = selectedFilterIndex};
}

void SDLCALL OnSdlDialogCompleted(void* userdata, const char* const* fileList, int selectedFilter) noexcept
{
    std::unique_ptr<SdlDialogContext> context{static_cast<SdlDialogContext*>(userdata)};
    if (context == nullptr)
    {
        return;
    }

    try
    {
        if (fileList == nullptr)
        {
            const char* const rawError = SDL_GetError();
            context->completion.CompleteFailure(rawError != nullptr ? std::string_view{rawError} : std::string_view{});
            return;
        }

        if (fileList[0] == nullptr)
        {
            context->completion.CompleteCancellation();
            return;
        }

        std::vector<std::string> paths;
        for (std::size_t index = 0; fileList[index] != nullptr; ++index)
        {
            paths.emplace_back(fileList[index]);
        }
        context->completion.CompleteSelection(paths, selectedFilter);
    }
    catch (const ponder::core::Exception&)
    {
        context->completion.FailCallback();
    }
    catch (const std::exception&)
    {
        context->completion.FailCallback();
    }
    catch (...)
    {
        context->completion.FailCallback();
    }
}

void ShowSdlDialogImpl(const DialogBackendRequest& request)
{
    auto context = std::make_unique<SdlDialogContext>(request);
    SDL_Window* const parent = request.parentWindow.has_value() ? ToSdlWindow(*request.parentWindow) : nullptr;
    const SDL_DialogFileFilter* const filters = context->nativeFilters.empty() ? nullptr : context->nativeFilters.data();
    const int filterCount = static_cast<int>(context->nativeFilters.size());
    const char* const defaultLocation = context->defaultLocation.has_value() ? context->defaultLocation->c_str() : nullptr;

    switch (request.kind)
    {
    case DialogKind::OpenFile:
        SDL_ShowOpenFileDialog(OnSdlDialogCompleted, context.release(), parent, filters, filterCount, defaultLocation,
                               request.allowMultipleSelection);
        break;
    case DialogKind::SaveFile:
        SDL_ShowSaveFileDialog(OnSdlDialogCompleted, context.release(), parent, filters, filterCount, defaultLocation);
        break;
    case DialogKind::OpenFolder:
        SDL_ShowOpenFolderDialog(OnSdlDialogCompleted, context.release(), parent, defaultLocation, request.allowMultipleSelection);
        break;
    default:
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Cannot show a dialog with unrecognized kind {}.", request.kind);
    }
}

[[nodiscard]] DialogCompletionRecord TakeDialogCompletion(DialogRequestState& request)
{
    if (request.completionIsCallbackFailure)
    {
        PONDER_VERIFY(request.callbackFailure.has_value(), "Dialog request {} is missing its callback failure fallback", request.info.id.GetValue());
        return std::move(*request.callbackFailure);
    }

    PONDER_VERIFY(request.completion.has_value(), "Dialog request {} has no completion record", request.info.id.GetValue());
    return std::move(*request.completion);
}
} // namespace

void ShowSdlDialog(const DialogBackendRequest& request)
{
    ShowSdlDialogImpl(request);
}

void DialogCallbackHandoff::EnqueueCompletion(DialogRequestId id, ponder::core::Timestamp timestamp, DialogOutcome outcome)
{
    std::scoped_lock lock{m_mutex};
    if (m_runtime != nullptr)
    {
        m_runtime->DialogEnqueueCompletion(id, timestamp, std::move(outcome));
    }
}

void DialogCallbackHandoff::MarkCallbackFailure(DialogRequestId id, ponder::core::Timestamp timestamp) noexcept
{
    std::scoped_lock lock{m_mutex};
    if (m_runtime != nullptr)
    {
        m_runtime->DialogMarkCallbackFailure(id, timestamp);
    }
}

void DialogCallbackHandoff::Deactivate(SdlRuntime& runtime) noexcept
{
    std::scoped_lock lock{m_mutex};
    PONDER_VERIFY(m_runtime == &runtime, "Dialog callback handoff has an unexpected runtime");
    m_runtime = nullptr;
}

void DialogRequestState::Complete(ponder::core::Timestamp timestamp, DialogOutcome outcome)
{
    if (callbackHandoff == nullptr)
    {
        return;
    }

    callbackHandoff->EnqueueCompletion(info.id, timestamp, std::move(outcome));
}

void DialogRequestState::CompleteSelection(std::span<const std::string> paths, int selectedFilter) noexcept
{
    const ponder::core::Timestamp timestamp = ponder::core::Timestamp::Now();
    try
    {
        Complete(timestamp, MakeDialogSelection(*this, paths, selectedFilter));
    }
    catch (const ponder::core::Exception&)
    {
        FailCallback(timestamp);
    }
    catch (const std::exception&)
    {
        FailCallback(timestamp);
    }
    catch (...)
    {
        FailCallback(timestamp);
    }
}

void DialogRequestState::CompleteCancellation() noexcept
{
    const ponder::core::Timestamp timestamp = ponder::core::Timestamp::Now();
    try
    {
        Complete(timestamp, DialogCancellation{});
    }
    catch (const ponder::core::Exception&)
    {
        FailCallback(timestamp);
    }
    catch (const std::exception&)
    {
        FailCallback(timestamp);
    }
    catch (...)
    {
        FailCallback(timestamp);
    }
}

void DialogRequestState::CompleteFailure(std::string_view message) noexcept
{
    const ponder::core::Timestamp timestamp = ponder::core::Timestamp::Now();
    try
    {
        Complete(timestamp, MakeDialogFailure(info.kind, message));
    }
    catch (const ponder::core::Exception&)
    {
        FailCallback(timestamp);
    }
    catch (const std::exception&)
    {
        FailCallback(timestamp);
    }
    catch (...)
    {
        FailCallback(timestamp);
    }
}

void DialogRequestState::FailCallback() noexcept
{
    FailCallback(ponder::core::Timestamp::Now());
}

void DialogRequestState::FailCallback(ponder::core::Timestamp timestamp) noexcept
{
    if (callbackHandoff != nullptr)
    {
        callbackHandoff->MarkCallbackFailure(info.id, timestamp);
    }
}

DialogBackendCompletion::DialogBackendCompletion(const std::shared_ptr<DialogRequestState>& request) noexcept :
    m_request(request)
{
}

void DialogBackendCompletion::CompleteSelection(std::span<const std::string> paths, int selectedFilter) const noexcept
{
    if (const std::shared_ptr<DialogRequestState> request = m_request.lock())
    {
        request->CompleteSelection(paths, selectedFilter);
    }
}

void DialogBackendCompletion::CompleteCancellation() const noexcept
{
    if (const std::shared_ptr<DialogRequestState> request = m_request.lock())
    {
        request->CompleteCancellation();
    }
}

void DialogBackendCompletion::CompleteFailure(std::string_view message) const noexcept
{
    if (const std::shared_ptr<DialogRequestState> request = m_request.lock())
    {
        request->CompleteFailure(message);
    }
}

void DialogBackendCompletion::FailCallback() const noexcept
{
    if (const std::shared_ptr<DialogRequestState> request = m_request.lock())
    {
        request->FailCallback();
    }
}

DialogRequestId SdlRuntime::DialogShow(DialogKind kind, std::optional<WindowId> parentWindowId,
                                       const std::optional<std::filesystem::path>& defaultLocation, std::span<const DialogFileFilter> filters,
                                       bool allowMultipleSelection)
{
    VerifyOwnerThread("dialog request");

    if (m_dialogShutdown)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Cannot show a dialog after dialog services shutdown.");
    }
    if (m_dialogCallbackHandoff == nullptr)
    {
        m_dialogCallbackHandoff = std::make_shared<DialogCallbackHandoff>(*this);
    }

    PreparedDialogRequest prepared = PrepareDialogRequest(kind, parentWindowId, defaultLocation, filters, allowMultipleSelection);

    std::optional<DialogParentLease> parentLease;
    if (prepared.parentWindowId.has_value())
    {
        parentLease.emplace(m_windowRegistry.AcquireDialogLease(*prepared.parentWindowId));
    }
    std::optional<BackendWindowHandle> parentBackendWindow;
    if (parentLease.has_value())
    {
        parentBackendWindow = parentLease->GetBackendWindow();
    }

    if (m_nextDialogRequestId == 0)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Platform dialog request ID space is exhausted.");
    }
    const DialogRequestId id{m_nextDialogRequestId};
    ++m_nextDialogRequestId;

    const ponder::core::Timestamp requestedAt = ponder::core::Timestamp::Now();

    auto request = std::make_shared<DialogRequestState>();
    request->callbackHandoff = m_dialogCallbackHandoff;
    request->info = DialogRequestInfo{.id = id,
                                      .kind = prepared.kind,
                                      .requestedAt = requestedAt,
                                      .parentWindowId = prepared.parentWindowId,
                                      .filterCount = prepared.filters.size(),
                                      .allowMultipleSelection = prepared.allowMultipleSelection};
    request->parentLease = std::move(parentLease);
    request->defaultLocation = std::move(prepared.defaultLocation);
    request->filters = std::move(prepared.filters);
    request->callbackFailure.emplace(DialogCompletionRecord{
        .timestamp = {},
        .outcome = MakeDialogFailure(request->info.kind, "Dialog callback failed before a completion could be safely enqueued.")});
    request->completionQueueNode.push_back(id);

    {
        std::scoped_lock lock{m_dialogMutex};
        const auto [iterator, inserted] = m_dialogRequests.emplace(id, request);
        static_cast<void>(iterator);
        if (!inserted)
        {
            throw PLATFORM_EXCEPTION(PlatformErrorCode::BackendFailure, "Dialog request ID {} is already registered.", id.GetValue());
        }
    }

    auto rollbackRequest = ponder::core::MakeScopeExit(
        [this, id]() noexcept
        {
            DialogRollbackRequest(id);
        });
    ShowSdlDialog(DialogBackendRequest{
        .kind = request->info.kind,
        .parentWindow = parentBackendWindow,
        .filters = request->filters,
        .defaultLocation = request->defaultLocation.has_value() ? std::optional<std::string_view>{*request->defaultLocation} : std::nullopt,
        .allowMultipleSelection = request->info.allowMultipleSelection,
        .completion = DialogBackendCompletion{request}});
    rollbackRequest.Dismiss();
    return id;
}

void SdlRuntime::DialogRollbackRequest(DialogRequestId id) noexcept
{
    std::optional<DialogParentLease> parentLease;
    {
        std::scoped_lock lock{m_dialogMutex};
        const auto request = m_dialogRequests.find(id);
        PONDER_VERIFY(request != m_dialogRequests.end(), "Cannot roll back unregistered dialog request {}", id.GetValue());

        DialogRequestState& requestState = *request->second;
        if (requestState.completionEnqueued)
        {
            const auto completion = std::ranges::find(m_completedDialogRequests, id);
            PONDER_VERIFY(completion != m_completedDialogRequests.end(), "Cannot roll back dialog request {} without its queued completion",
                          id.GetValue());
            PONDER_VERIFY(requestState.completionQueueNode.empty(), "Queued dialog request {} still owns its completion node", id.GetValue());
            requestState.completionQueueNode.splice(requestState.completionQueueNode.end(), m_completedDialogRequests, completion);
        }

        parentLease = std::move(requestState.parentLease);
        m_dialogRequests.erase(request);
    }

    parentLease.reset();
}

DialogRequestId SdlRuntime::DialogShowOpenFile(const OpenFileDialogDesc& desc)
{
    return DialogShow(DialogKind::OpenFile, desc.parentWindowId, desc.defaultLocation, desc.filters, desc.allowMultipleSelection);
}

DialogRequestId SdlRuntime::DialogShowSaveFile(const SaveFileDialogDesc& desc)
{
    return DialogShow(DialogKind::SaveFile, desc.parentWindowId, desc.defaultLocation, desc.filters, false);
}

DialogRequestId SdlRuntime::DialogShowOpenFolder(const OpenFolderDialogDesc& desc)
{
    return DialogShow(DialogKind::OpenFolder, desc.parentWindowId, desc.defaultLocation, {}, desc.allowMultipleSelection);
}

std::size_t SdlRuntime::DialogGetPendingCount() const
{
    VerifyOwnerThread("dialog pending-count query");
    std::scoped_lock lock{m_dialogMutex};
    return m_dialogRequests.size();
}

bool SdlRuntime::DialogHasPending() const
{
    VerifyOwnerThread("dialog pending-state query");
    std::scoped_lock lock{m_dialogMutex};
    return !m_dialogRequests.empty();
}

std::vector<DialogRequestInfo> SdlRuntime::DialogGetPending() const
{
    VerifyOwnerThread("dialog pending-list query");

    std::vector<DialogRequestInfo> pendingDialogs;
    {
        std::scoped_lock lock{m_dialogMutex};
        pendingDialogs.reserve(m_dialogRequests.size());
        for (const auto& entry : m_dialogRequests)
        {
            pendingDialogs.push_back(entry.second->info);
        }
    }

    std::ranges::sort(pendingDialogs, {}, &DialogRequestInfo::id);
    return pendingDialogs;
}

void SdlRuntime::DialogEnqueueCompletion(DialogRequestId id, ponder::core::Timestamp timestamp, DialogOutcome outcome)
{
    std::scoped_lock lock{m_dialogMutex};
    const auto request = m_dialogRequests.find(id);
    if (request == m_dialogRequests.end() || request->second->completionEnqueued)
    {
        return;
    }

    DialogRequestState& completedRequest = *request->second;
    PONDER_VERIFY(completedRequest.completionQueueNode.size() == 1, "Dialog request {} does not own exactly one completion node", id.GetValue());
    completedRequest.completion.emplace(DialogCompletionRecord{.timestamp = timestamp, .outcome = std::move(outcome)});
    completedRequest.completionIsCallbackFailure = false;
    m_completedDialogRequests.splice(m_completedDialogRequests.end(), completedRequest.completionQueueNode);
    completedRequest.completionEnqueued = true;
}

void SdlRuntime::DialogMarkCallbackFailure(DialogRequestId id, ponder::core::Timestamp timestamp) noexcept
{
    std::scoped_lock lock{m_dialogMutex};
    const auto request = m_dialogRequests.find(id);
    if (request == m_dialogRequests.end() || request->second->completionEnqueued)
    {
        return;
    }

    DialogRequestState& completedRequest = *request->second;
    PONDER_VERIFY(completedRequest.callbackFailure.has_value(), "Dialog request {} has no callback failure fallback", id.GetValue());
    PONDER_VERIFY(completedRequest.completionQueueNode.size() == 1, "Dialog request {} does not own exactly one completion node", id.GetValue());
    completedRequest.callbackFailure->timestamp = timestamp;
    completedRequest.completionIsCallbackFailure = true;
    m_completedDialogRequests.splice(m_completedDialogRequests.end(), completedRequest.completionQueueNode);
    completedRequest.completionEnqueued = true;
}

std::optional<DialogCompletedEvent> SdlRuntime::DialogPollCompletion()
{
    VerifyOwnerThread("dialog completion polling");

    std::optional<DialogCompletionRecord> completion;
    std::optional<DialogParentLease> parentLease;
    std::shared_ptr<DialogRequestState> request;
    {
        std::scoped_lock lock{m_dialogMutex};
        if (m_completedDialogRequests.empty())
        {
            return std::nullopt;
        }

        const DialogRequestId completedRequestId = m_completedDialogRequests.front();
        const auto completionIterator = m_dialogRequests.find(completedRequestId);
        PONDER_VERIFY(completionIterator != m_dialogRequests.end(), "Completed dialog request {} is not registered", completedRequestId.GetValue());
        request = completionIterator->second;
        completion = TakeDialogCompletion(*request);
        m_completedDialogRequests.pop_front();
        parentLease = std::move(request->parentLease);
        m_dialogRequests.erase(completionIterator);
    }

    parentLease.reset();

    return DialogCompletedEvent{.timestamp = completion->timestamp, .request = request->info, .outcome = std::move(completion->outcome)};
}

void SdlRuntime::DialogInitialize()
{
    PONDER_VERIFY(m_dialogCallbackHandoff == nullptr, "Cannot initialize runtime dialog services more than once");
    PONDER_VERIFY(!m_dialogShutdown, "Cannot initialize runtime dialog services after shutdown");
    m_dialogCallbackHandoff = std::make_shared<DialogCallbackHandoff>(*this);
}

std::size_t SdlRuntime::DialogGetOutstandingRequestCount() const
{
    return DialogGetPendingCount();
}

void SdlRuntime::DialogShutdown()
{
    VerifyOwnerThread("dialog services shutdown");
    if (m_dialogShutdown)
    {
        return;
    }

    const std::size_t outstandingRequestCount = DialogGetPendingCount();
    if (outstandingRequestCount != 0)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Cannot shut down runtime dialog services with {} outstanding requests.",
                                 outstandingRequestCount);
    }

    if (m_dialogCallbackHandoff != nullptr)
    {
        m_dialogCallbackHandoff->Deactivate(*this);
        m_dialogCallbackHandoff.reset();
    }

    {
        std::scoped_lock lock{m_dialogMutex};
        PONDER_VERIFY(m_dialogRequests.empty(), "Cannot shut down runtime dialog services with {} registered requests", m_dialogRequests.size());
        PONDER_VERIFY(m_completedDialogRequests.empty(), "Cannot shut down runtime dialog services with queued completions");
    }
    m_dialogShutdown = true;
}

void SdlRuntime::DialogShutdownForRuntimeDestruction() noexcept
{
    if (!m_initialized)
    {
        if (m_dialogCallbackHandoff != nullptr)
        {
            m_dialogCallbackHandoff->Deactivate(*this);
            m_dialogCallbackHandoff.reset();
        }
        m_dialogShutdown = true;
        return;
    }

    try
    {
        DialogShutdown();
        return;
    }
    catch (const ponder::core::Exception& exception)
    {
        LOG_ERROR_CATEGORY("platform", "Dialog cleanup failed during runtime destruction: {}", exception.GetMessage());
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR_CATEGORY("platform", "Dialog cleanup failed during runtime destruction: {}", exception.what());
    }
    catch (...)
    {
        LOG_ERROR_CATEGORY("platform", "Dialog cleanup failed during runtime destruction with an unknown exception");
    }

    if (m_dialogCallbackHandoff != nullptr)
    {
        m_dialogCallbackHandoff->Deactivate(*this);
        m_dialogCallbackHandoff.reset();
    }
    m_dialogShutdown = true;
}
} // namespace detail
} // namespace ponder::platform
