#include <ponder/core/Assert.hpp>
#include <ponder/core/Log.hpp>
#include <ponder/platform/Display.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/platform/Window.hpp>

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "PlatformRuntimeState.hpp"
#include "SdlError.hpp"
#include "WindowImpl.hpp"

namespace pond::platform
{
namespace
{
constexpr core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr core::ErrorCode kNotFoundCode = ToErrorCode(PlatformErrorCode::NotFound);

[[nodiscard]] core::Error MakeBackendDataError(std::string message)
{
    return core::Error{kBackendFailureCode, std::move(message)};
}

[[nodiscard]] core::Error MakeDisplayNotFoundError(DisplayId id)
{
    return core::Error{kNotFoundCode,
                       "Display " + std::to_string(id.GetValue()) + " is not connected."};
}

[[nodiscard]] std::string MakeDisplayContext(DisplayId id)
{
    return "display " + std::to_string(id.GetValue());
}

[[nodiscard]] core::Result<ScreenRectangle> ConvertRectangle(
    const detail::BackendScreenRectangle& rectangle, std::string_view operation,
    std::string_view context)
{
    if (rectangle.width < 0 || rectangle.height < 0)
    {
        return core::Result<ScreenRectangle>::FromError(
            MakeBackendDataError(std::string{operation} + " returned a negative extent for " +
                                 std::string{context} + "."));
    }

    if (!std::in_range<std::int32_t>(rectangle.x) || !std::in_range<std::int32_t>(rectangle.y))
    {
        return core::Result<ScreenRectangle>::FromError(MakeBackendDataError(
            std::string{operation} + " returned an out-of-range position for " +
            std::string{context} + "."));
    }

    return ScreenRectangle{ScreenPosition{static_cast<std::int32_t>(rectangle.x),
                                          static_cast<std::int32_t>(rectangle.y)},
                           ScreenExtent{static_cast<std::uint32_t>(rectangle.width),
                                        static_cast<std::uint32_t>(rectangle.height)}};
}

[[nodiscard]] DisplayOrientation ConvertOrientation(
    detail::BackendDisplayOrientation orientation) noexcept
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

[[nodiscard]] core::Result<float> ValidateScale(float scale, std::string_view operation,
                                                std::string_view context)
{
    if (!std::isfinite(scale) || scale <= 0.0F)
    {
        return core::Result<float>::FromError(
            MakeBackendDataError(std::string{operation} + " returned an invalid scale for " +
                                 std::string{context} + "."));
    }

    return scale;
}
} // namespace

namespace detail
{
namespace
{
struct PendingDisplayConnection final
{
    std::uint32_t backendId{};
    DisplayId id;
    bool needsBackendMapping{};
    bool backendMappingInserted{};
    bool projectMappingInserted{};
};
} // namespace

std::optional<DisplayId> PlatformRuntimeState::FindConnectedDisplayId(
    std::uint32_t backendDisplayId) const
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
    PONDER_VERIFY(projectMapping != m_displaysById.end() &&
                      projectMapping->second.backendId == backendDisplayId &&
                      projectMapping->second.connected,
                  "Connected backend display {} has an inconsistent project mapping",
                  backendDisplayId);
    return mapping->second.id;
}

std::optional<DisplayId> PlatformRuntimeState::FindDisplayIdForRemoval(
    std::uint32_t backendDisplayId) const
{
    VerifyOwnerThread("display removal lookup");
    if (backendDisplayId == 0)
    {
        return std::nullopt;
    }

    const auto mapping = m_displaysByBackendId.find(backendDisplayId);
    if (mapping == m_displaysByBackendId.end() ||
        (!mapping->second.connected && !mapping->second.removalEventPending))
    {
        return std::nullopt;
    }

    const auto projectMapping = m_displaysById.find(mapping->second.id);
    PONDER_VERIFY(projectMapping != m_displaysById.end() &&
                      projectMapping->second.backendId == backendDisplayId,
                  "Backend display {} has an inconsistent removal mapping", backendDisplayId);
    return mapping->second.id;
}

std::optional<DisplayId> PlatformRuntimeState::FindKnownDisplayId(
    std::uint32_t backendDisplayId) const
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
    PONDER_VERIFY(projectMapping != m_displaysById.end() &&
                      projectMapping->second.backendId == backendDisplayId,
                  "Backend display {} has an inconsistent project mapping", backendDisplayId);
    return mapping->second.id;
}

std::optional<DisplayId> PlatformRuntimeState::ConnectDisplayFromEvent(
    std::uint32_t backendDisplayId)
{
    VerifyOwnerThread("display connection event");
    if (backendDisplayId == 0)
    {
        return std::nullopt;
    }

    const auto current = m_displaysByBackendId.find(backendDisplayId);
    if (current != m_displaysByBackendId.end() && current->second.connected)
    {
        return current->second.id;
    }

    PONDER_VERIFY(m_nextDisplayId != 0, "Platform display ID space is exhausted");
    const DisplayId id{m_nextDisplayId};

    std::vector<std::uint32_t> nextConnectedBackendIds = m_connectedBackendDisplayIds;
    nextConnectedBackendIds.emplace_back(backendDisplayId);

    const bool needsBackendMapping = current == m_displaysByBackendId.end();
    m_displaysById.reserve(m_displaysById.size() + 1);
    if (needsBackendMapping)
    {
        m_displaysByBackendId.reserve(m_displaysByBackendId.size() + 1);
    }

    bool projectMappingInserted = false;
    bool backendMappingInserted = false;
    try
    {
        const auto [projectIterator, projectInserted] =
            m_displaysById.emplace(id, RuntimeBackendDisplayRecord{backendDisplayId, false});
        static_cast<void>(projectIterator);
        projectMappingInserted = projectInserted;
        PONDER_VERIFY(projectInserted, "Platform display ID {} is already registered",
                      id.GetValue());

        if (needsBackendMapping)
        {
            const auto [backendIterator, backendInserted] = m_displaysByBackendId.emplace(
                backendDisplayId, RuntimeDisplayRecord{id, false, false});
            static_cast<void>(backendIterator);
            backendMappingInserted = backendInserted;
            PONDER_VERIFY(backendInserted, "Backend display {} is already registered",
                          backendDisplayId);
        }
    }
    catch (...)
    {
        if (backendMappingInserted)
        {
            m_displaysByBackendId.erase(backendDisplayId);
        }
        if (projectMappingInserted)
        {
            m_displaysById.erase(id);
        }
        throw;
    }

    auto backendMapping = m_displaysByBackendId.find(backendDisplayId);
    auto projectMapping = m_displaysById.find(id);
    PONDER_VERIFY(backendMapping != m_displaysByBackendId.end() &&
                      projectMapping != m_displaysById.end(),
                  "Display {} could not commit its identity mapping", id.GetValue());
    backendMapping->second = RuntimeDisplayRecord{id, true, false};
    projectMapping->second.connected = true;
    m_connectedBackendDisplayIds.swap(nextConnectedBackendIds);
    ++m_nextDisplayId;
    return id;
}
void PlatformRuntimeState::DisconnectDisplayFromEvent(std::uint32_t backendDisplayId)
{
    VerifyOwnerThread("display disconnection event");
    const auto mapping = m_displaysByBackendId.find(backendDisplayId);
    if (mapping == m_displaysByBackendId.end())
    {
        return;
    }

    const auto projectMapping = m_displaysById.find(mapping->second.id);
    PONDER_VERIFY(projectMapping != m_displaysById.end() &&
                      projectMapping->second.backendId == backendDisplayId,
                  "Backend display {} has an inconsistent disconnection mapping", backendDisplayId);
    mapping->second.connected = false;
    mapping->second.removalEventPending = false;
    projectMapping->second.connected = false;
    std::erase(m_connectedBackendDisplayIds, backendDisplayId);
}

void PlatformRuntimeState::ReconcileDisplayFromEvent(std::uint32_t backendDisplayId)
{
    if (backendDisplayId == 0 || FindConnectedDisplayId(backendDisplayId).has_value() ||
        FindKnownDisplayId(backendDisplayId).has_value())
    {
        return;
    }

    auto refresh = RefreshDisplays();
    if (!refresh.HasValue())
    {
        LOG_ERROR_CATEGORY("platform",
                           "Failed to reconcile display identity while polling an event: {}",
                           refresh.GetError().GetMessage());
    }
}

core::Result<std::vector<std::uint32_t>> PlatformRuntimeState::RefreshDisplays()
{
    VerifyOwnerThread("display refresh");

    std::vector<std::uint32_t> backendDisplayIds;
    if (!m_displayBackend.enumerate(m_displayBackend.context, backendDisplayIds))
    {
        return core::Result<std::vector<std::uint32_t>>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetDisplays", "displays"));
    }

    std::unordered_set<std::uint32_t> uniqueIds;
    uniqueIds.reserve(backendDisplayIds.size());
    for (const std::uint32_t backendDisplayId : backendDisplayIds)
    {
        if (backendDisplayId == 0 || !uniqueIds.insert(backendDisplayId).second)
        {
            return core::Result<std::vector<std::uint32_t>>::FromError(
                MakeBackendDataError("SDL_GetDisplays returned a zero or duplicate display ID."));
        }
    }

    std::vector<std::uint32_t> nextConnectedBackendIds = backendDisplayIds;
    std::vector<PendingDisplayConnection> pendingConnections;
    pendingConnections.reserve(backendDisplayIds.size());
    DisplayId::ValueType nextDisplayId = m_nextDisplayId;
    for (const std::uint32_t backendDisplayId : backendDisplayIds)
    {
        const auto current = m_displaysByBackendId.find(backendDisplayId);
        const bool wasConnected =
            current != m_displaysByBackendId.end() && current->second.connected;
        if (wasConnected)
        {
            const DisplayId id = current->second.id;
            const auto project = m_displaysById.find(id);
            PONDER_VERIFY(project != m_displaysById.end(),
                          "Connected display {} has no project mapping", id.GetValue());
            continue;
        }

        PONDER_VERIFY(nextDisplayId != 0, "Platform display ID space is exhausted");
        const DisplayId id{nextDisplayId};
        ++nextDisplayId;
        pendingConnections.emplace_back(
            PendingDisplayConnection{backendDisplayId, id, current == m_displaysByBackendId.end()});
    }

    std::size_t newBackendMappingCount = 0;
    for (const PendingDisplayConnection& connection : pendingConnections)
    {
        newBackendMappingCount += connection.needsBackendMapping ? 1U : 0U;
    }
    if (newBackendMappingCount != 0)
    {
        m_displaysByBackendId.reserve(m_displaysByBackendId.size() + newBackendMappingCount);
    }
    if (!pendingConnections.empty())
    {
        m_displaysById.reserve(m_displaysById.size() + pendingConnections.size());
    }

    try
    {
        for (PendingDisplayConnection& connection : pendingConnections)
        {
            const auto [projectIterator, projectInserted] = m_displaysById.emplace(
                connection.id, RuntimeBackendDisplayRecord{connection.backendId, false});
            static_cast<void>(projectIterator);
            connection.projectMappingInserted = projectInserted;
            PONDER_VERIFY(projectInserted, "Platform display ID {} is already registered",
                          connection.id.GetValue());

            if (connection.needsBackendMapping)
            {
                const auto [backendIterator, backendInserted] = m_displaysByBackendId.emplace(
                    connection.backendId, RuntimeDisplayRecord{connection.id, false, false});
                static_cast<void>(backendIterator);
                connection.backendMappingInserted = backendInserted;
                PONDER_VERIFY(backendInserted, "Backend display {} is already registered",
                              connection.backendId);
            }
        }
    }
    catch (...)
    {
        for (const PendingDisplayConnection& connection : pendingConnections)
        {
            if (connection.backendMappingInserted)
            {
                m_displaysByBackendId.erase(connection.backendId);
            }
            if (connection.projectMappingInserted)
            {
                m_displaysById.erase(connection.id);
            }
        }
        throw;
    }

    for (const std::uint32_t backendDisplayId : m_connectedBackendDisplayIds)
    {
        if (uniqueIds.contains(backendDisplayId))
        {
            continue;
        }

        const auto backendMapping = m_displaysByBackendId.find(backendDisplayId);
        PONDER_VERIFY(backendMapping != m_displaysByBackendId.end() &&
                          backendMapping->second.connected,
                      "Connected backend display {} has no active mapping", backendDisplayId);
        const auto projectMapping = m_displaysById.find(backendMapping->second.id);
        PONDER_VERIFY(projectMapping != m_displaysById.end() && projectMapping->second.connected,
                      "Connected display {} has no active project mapping",
                      backendMapping->second.id.GetValue());
        backendMapping->second.connected = false;
        backendMapping->second.removalEventPending = true;
        projectMapping->second.connected = false;
    }

    for (const PendingDisplayConnection& connection : pendingConnections)
    {
        const auto backendMapping = m_displaysByBackendId.find(connection.backendId);
        const auto projectMapping = m_displaysById.find(connection.id);
        PONDER_VERIFY(backendMapping != m_displaysByBackendId.end() &&
                          projectMapping != m_displaysById.end(),
                      "Display {} could not commit its identity mapping", connection.id.GetValue());
        backendMapping->second = RuntimeDisplayRecord{connection.id, true, false};
        projectMapping->second.connected = true;
    }

    m_connectedBackendDisplayIds.swap(nextConnectedBackendIds);
    m_nextDisplayId = nextDisplayId;
    return core::Result<std::vector<std::uint32_t>>::FromValue(std::move(backendDisplayIds));
}
core::Result<DisplayInfo> PlatformRuntimeState::QueryDisplayInfo(
    DisplayId id, std::uint32_t backendDisplayId) const
{
    const std::string context = MakeDisplayContext(id);

    const char* const backendName =
        m_displayBackend.getName(m_displayBackend.context, backendDisplayId);
    if (backendName == nullptr)
    {
        return core::Result<DisplayInfo>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetDisplayName", context));
    }
    const std::string name{backendName};

    BackendScreenRectangle backendBounds;
    if (!m_displayBackend.getBounds(m_displayBackend.context, backendDisplayId, &backendBounds))
    {
        return core::Result<DisplayInfo>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetDisplayBounds", context));
    }
    auto bounds = ConvertRectangle(backendBounds, "SDL_GetDisplayBounds", context);
    if (!bounds.HasValue())
    {
        return core::Result<DisplayInfo>::FromError(std::move(bounds).GetError());
    }

    BackendScreenRectangle backendUsableBounds;
    if (!m_displayBackend.getUsableBounds(m_displayBackend.context, backendDisplayId,
                                          &backendUsableBounds))
    {
        return core::Result<DisplayInfo>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetDisplayUsableBounds", context));
    }
    auto usableBounds =
        ConvertRectangle(backendUsableBounds, "SDL_GetDisplayUsableBounds", context);
    if (!usableBounds.HasValue())
    {
        return core::Result<DisplayInfo>::FromError(std::move(usableBounds).GetError());
    }

    float refreshRateHertz{};
    if (!m_displayBackend.getCurrentRefreshRate(m_displayBackend.context, backendDisplayId,
                                                &refreshRateHertz))
    {
        return core::Result<DisplayInfo>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetCurrentDisplayMode", context));
    }
    if (!std::isfinite(refreshRateHertz) || refreshRateHertz < 0.0F)
    {
        return core::Result<DisplayInfo>::FromError(MakeBackendDataError(
            "SDL_GetCurrentDisplayMode returned an invalid refresh rate for " + context + "."));
    }
    const std::optional<float> refreshRate =
        refreshRateHertz > 0.0F ? std::optional<float>{refreshRateHertz} : std::nullopt;

    const DisplayOrientation orientation = ConvertOrientation(
        m_displayBackend.getCurrentOrientation(m_displayBackend.context, backendDisplayId));

    const float contentScale =
        m_displayBackend.getContentScale(m_displayBackend.context, backendDisplayId);
    if (contentScale == 0.0F)
    {
        return core::Result<DisplayInfo>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetDisplayContentScale", context));
    }
    auto validContentScale = ValidateScale(contentScale, "SDL_GetDisplayContentScale", context);
    if (!validContentScale.HasValue())
    {
        return core::Result<DisplayInfo>::FromError(std::move(validContentScale).GetError());
    }

    return DisplayInfo{
        id,          name,        std::move(bounds).GetValue(), std::move(usableBounds).GetValue(),
        refreshRate, orientation, validContentScale.GetValue()};
}

core::Result<std::vector<DisplayInfo>> PlatformRuntimeState::EnumerateDisplays()
{
    auto refresh = RefreshDisplays();
    if (!refresh.HasValue())
    {
        return core::Result<std::vector<DisplayInfo>>::FromError(std::move(refresh).GetError());
    }

    std::vector<DisplayInfo> displays;
    displays.reserve(refresh.GetValue().size());
    for (const std::uint32_t backendDisplayId : refresh.GetValue())
    {
        const auto mapping = m_displaysByBackendId.find(backendDisplayId);
        PONDER_VERIFY(mapping != m_displaysByBackendId.end() && mapping->second.connected,
                      "Connected backend display {} has no project mapping", backendDisplayId);
        auto info = QueryDisplayInfo(mapping->second.id, backendDisplayId);
        if (!info.HasValue())
        {
            return core::Result<std::vector<DisplayInfo>>::FromError(std::move(info).GetError());
        }
        displays.emplace_back(std::move(info).GetValue());
    }

    return core::Result<std::vector<DisplayInfo>>::FromValue(std::move(displays));
}

core::Result<DisplayInfo> PlatformRuntimeState::GetDisplayInfo(DisplayId id)
{
    VerifyOwnerThread("display query");
    if (!id.IsValid())
    {
        return core::Result<DisplayInfo>::FromError(
            core::Error{kInvalidArgumentCode, "Display ID must be valid."});
    }

    auto refresh = RefreshDisplays();
    if (!refresh.HasValue())
    {
        return core::Result<DisplayInfo>::FromError(std::move(refresh).GetError());
    }

    const auto mapping = m_displaysById.find(id);
    if (mapping == m_displaysById.end() || !mapping->second.connected)
    {
        return core::Result<DisplayInfo>::FromError(MakeDisplayNotFoundError(id));
    }

    auto info = QueryDisplayInfo(id, mapping->second.backendId);
    if (info.HasValue())
    {
        return info;
    }

    core::Error primaryError = std::move(info).GetError();
    auto confirmation = RefreshDisplays();
    if (confirmation.HasValue())
    {
        const auto confirmedMapping = m_displaysById.find(id);
        if (confirmedMapping == m_displaysById.end() || !confirmedMapping->second.connected)
        {
            return core::Result<DisplayInfo>::FromError(MakeDisplayNotFoundError(id));
        }
    }
    return core::Result<DisplayInfo>::FromError(std::move(primaryError));
}

core::Result<DisplayId> PlatformRuntimeState::GetDisplayIdForWindow(void* nativeWindow,
                                                                    std::string_view windowContext)
{
    VerifyOwnerThread("window display query");
    const std::uint32_t backendDisplayId =
        m_displayBackend.getForWindow(m_displayBackend.context, nativeWindow);
    if (backendDisplayId == 0)
    {
        return core::Result<DisplayId>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetDisplayForWindow", windowContext));
    }

    auto refresh = RefreshDisplays();
    if (!refresh.HasValue())
    {
        return core::Result<DisplayId>::FromError(std::move(refresh).GetError());
    }

    const auto mapping = m_displaysByBackendId.find(backendDisplayId);
    if (mapping == m_displaysByBackendId.end() || !mapping->second.connected)
    {
        return core::Result<DisplayId>::FromError(
            core::Error{kNotFoundCode, "The window's display is not connected."});
    }
    return mapping->second.id;
}

core::Result<float> PlatformRuntimeState::GetPixelDensityForWindow(
    void* nativeWindow, std::string_view windowContext) const
{
    VerifyOwnerThread("window pixel density query");
    const float density =
        m_displayBackend.getWindowPixelDensity(m_displayBackend.context, nativeWindow);
    if (density == 0.0F)
    {
        return core::Result<float>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetWindowPixelDensity", windowContext));
    }
    return ValidateScale(density, "SDL_GetWindowPixelDensity", windowContext);
}

core::Result<float> PlatformRuntimeState::GetDisplayScaleForWindow(
    void* nativeWindow, std::string_view windowContext) const
{
    VerifyOwnerThread("window display scale query");
    const float scale =
        m_displayBackend.getWindowDisplayScale(m_displayBackend.context, nativeWindow);
    if (scale == 0.0F)
    {
        return core::Result<float>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetWindowDisplayScale", windowContext));
    }
    return ValidateScale(scale, "SDL_GetWindowDisplayScale", windowContext);
}
} // namespace detail

core::Result<std::vector<DisplayInfo>> PlatformRuntime::EnumerateDisplays()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->EnumerateDisplays();
}

core::Result<DisplayInfo> PlatformRuntime::GetDisplayInfo(DisplayId id)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->GetDisplayInfo(id);
}

core::Result<DisplayId> Window::GetDisplayId() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetDisplayId();
}

core::Result<float> Window::GetPixelDensity() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetPixelDensity();
}

core::Result<float> Window::GetDisplayScale() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetDisplayScale();
}

namespace detail
{
core::Result<DisplayId> WindowImpl::GetDisplayId() const
{
    VerifyUsable("display query");
    return m_runtime->GetDisplayIdForWindow(m_nativeWindow, GetErrorContext());
}

core::Result<float> WindowImpl::GetPixelDensity() const
{
    VerifyUsable("pixel density query");
    return m_runtime->GetPixelDensityForWindow(m_nativeWindow, GetErrorContext());
}

core::Result<float> WindowImpl::GetDisplayScale() const
{
    VerifyUsable("display scale query");
    return m_runtime->GetDisplayScaleForWindow(m_nativeWindow, GetErrorContext());
}
} // namespace detail
} // namespace pond::platform
