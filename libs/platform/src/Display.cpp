#include <ponder/platform/Display.hpp>

#include "PlatformRuntimeState.hpp"
#include "SdlError.hpp"
#include "WindowImpl.hpp"

#include <ponder/core/Assert.hpp>
#include <ponder/core/Log.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/platform/Window.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pond::platform
{
namespace
{
constexpr core::ErrorCode kInvalidArgumentCode =
    ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kBackendFailureCode =
    ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr core::ErrorCode kNotFoundCode =
    ToErrorCode(PlatformErrorCode::NotFound);

[[nodiscard]] core::Error MakeBackendDataError(std::string message)
{
    return core::Error{kBackendFailureCode, std::move(message)};
}

[[nodiscard]] core::Error MakeDisplayNotFoundError(DisplayId id)
{
    return core::Error{
        kNotFoundCode,
        "Display " + std::to_string(id.GetValue()) + " is not connected."};
}

[[nodiscard]] std::string MakeDisplayContext(DisplayId id)
{
    return "display " + std::to_string(id.GetValue());
}

[[nodiscard]] std::string MakeWindowContext(WindowId id)
{
    return "window " + std::to_string(id.GetValue());
}

[[nodiscard]] core::Result<ScreenRectangle> ConvertRectangle(
    const detail::BackendScreenRectangle& rectangle, std::string_view operation,
    std::string_view context)
{
    if (rectangle.width < 0 || rectangle.height < 0)
    {
        return core::Result<ScreenRectangle>::FromError(MakeBackendDataError(
            std::string{operation} + " returned a negative extent for " +
            std::string{context} + "."));
    }

    constexpr int kMinimumPosition =
        static_cast<int>(std::numeric_limits<std::int32_t>::min());
    constexpr int kMaximumPosition =
        static_cast<int>(std::numeric_limits<std::int32_t>::max());
    if (rectangle.x < kMinimumPosition || rectangle.x > kMaximumPosition ||
        rectangle.y < kMinimumPosition || rectangle.y > kMaximumPosition)
    {
        return core::Result<ScreenRectangle>::FromError(MakeBackendDataError(
            std::string{operation} + " returned an out-of-range position for " +
            std::string{context} + "."));
    }

    return ScreenRectangle{
        ScreenPosition{static_cast<std::int32_t>(rectangle.x),
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

[[nodiscard]] core::Result<float> ValidateScale(float scale,
                                                std::string_view operation,
                                                std::string_view context)
{
    if (!std::isfinite(scale) || scale <= 0.0F)
    {
        return core::Result<float>::FromError(MakeBackendDataError(
            std::string{operation} + " returned an invalid scale for " +
            std::string{context} + "."));
    }

    return scale;
}
} // namespace

namespace detail
{
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
                  "Backend display {} has an inconsistent removal mapping",
                  backendDisplayId);
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
                  "Backend display {} has an inconsistent project mapping",
                  backendDisplayId);
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
    auto nextByBackend = m_displaysByBackendId;
    auto nextByProject = m_displaysById;

    nextByBackend[backendDisplayId] = RuntimeDisplayRecord{id, true, false};
    const auto [iterator, inserted] = nextByProject.emplace(
        id, RuntimeBackendDisplayRecord{backendDisplayId, true});
    static_cast<void>(iterator);
    PONDER_VERIFY(inserted, "Platform display ID {} is already registered",
                  id.GetValue());

    m_displaysByBackendId.swap(nextByBackend);
    m_displaysById.swap(nextByProject);
    ++m_nextDisplayId;
    return id;
}

void PlatformRuntimeState::DisconnectDisplayFromEvent(
    std::uint32_t backendDisplayId)
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
                  "Backend display {} has an inconsistent disconnection mapping",
                  backendDisplayId);
    mapping->second.connected = false;
    mapping->second.removalEventPending = false;
    projectMapping->second.connected = false;
}

void PlatformRuntimeState::ReconcileDisplayFromEvent(
    std::uint32_t backendDisplayId)
{
    if (backendDisplayId == 0 ||
        FindConnectedDisplayId(backendDisplayId).has_value() ||
        FindKnownDisplayId(backendDisplayId).has_value())
    {
        return;
    }

    auto refresh = RefreshDisplays();
    if (!refresh.HasValue())
    {
        LOG_ERROR_CATEGORY(
            "platform",
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
                MakeBackendDataError(
                    "SDL_GetDisplays returned a zero or duplicate display ID."));
        }
    }

    auto nextByBackend = m_displaysByBackendId;
    auto nextByProject = m_displaysById;
    for (auto& [backendId, record] : nextByBackend)
    {
        static_cast<void>(backendId);
        if (record.connected)
        {
            record.removalEventPending = true;
        }
        record.connected = false;
    }
    for (auto& [projectId, record] : nextByProject)
    {
        static_cast<void>(projectId);
        record.connected = false;
    }

    DisplayId::ValueType nextDisplayId = m_nextDisplayId;
    for (const std::uint32_t backendDisplayId : backendDisplayIds)
    {
        const auto current = m_displaysByBackendId.find(backendDisplayId);
        const bool wasConnected =
            current != m_displaysByBackendId.end() && current->second.connected;
        if (wasConnected)
        {
            const DisplayId id = current->second.id;
            nextByBackend[backendDisplayId] =
                RuntimeDisplayRecord{id, true, false};
            const auto project = nextByProject.find(id);
            PONDER_VERIFY(project != nextByProject.end(),
                          "Connected display {} has no project mapping",
                          id.GetValue());
            project->second.connected = true;
            continue;
        }

        PONDER_VERIFY(nextDisplayId != 0, "Platform display ID space is exhausted");
        const DisplayId id{nextDisplayId};
        ++nextDisplayId;
        nextByBackend[backendDisplayId] = RuntimeDisplayRecord{id, true, false};
        const auto [iterator, inserted] = nextByProject.emplace(
            id, RuntimeBackendDisplayRecord{backendDisplayId, true});
        static_cast<void>(iterator);
        PONDER_VERIFY(inserted, "Platform display ID {} is already registered",
                      id.GetValue());
    }

    m_displaysByBackendId.swap(nextByBackend);
    m_displaysById.swap(nextByProject);
    m_nextDisplayId = nextDisplayId;
    return core::Result<std::vector<std::uint32_t>>::FromValue(
        std::move(backendDisplayIds));
}

core::Result<DisplayInfo> PlatformRuntimeState::QueryDisplayInfo(
    DisplayId id, std::uint32_t backendDisplayId) const
{
    const std::string context = MakeDisplayContext(id);

    const char* const backendName =
        m_displayBackend.getName(m_displayBackend.context, backendDisplayId);
    if (backendName == nullptr)
    {
        return core::Result<DisplayInfo>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetDisplayName", context));
    }
    const std::string name{backendName};

    BackendScreenRectangle backendBounds;
    if (!m_displayBackend.getBounds(m_displayBackend.context, backendDisplayId,
                                    &backendBounds))
    {
        return core::Result<DisplayInfo>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetDisplayBounds", context));
    }
    auto bounds = ConvertRectangle(backendBounds, "SDL_GetDisplayBounds", context);
    if (!bounds.HasValue())
    {
        return core::Result<DisplayInfo>::FromError(
            std::move(bounds).GetError());
    }

    BackendScreenRectangle backendUsableBounds;
    if (!m_displayBackend.getUsableBounds(
            m_displayBackend.context, backendDisplayId, &backendUsableBounds))
    {
        return core::Result<DisplayInfo>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetDisplayUsableBounds", context));
    }
    auto usableBounds = ConvertRectangle(
        backendUsableBounds, "SDL_GetDisplayUsableBounds", context);
    if (!usableBounds.HasValue())
    {
        return core::Result<DisplayInfo>::FromError(
            std::move(usableBounds).GetError());
    }

    float refreshRateHertz{};
    if (!m_displayBackend.getCurrentRefreshRate(
            m_displayBackend.context, backendDisplayId, &refreshRateHertz))
    {
        return core::Result<DisplayInfo>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetCurrentDisplayMode", context));
    }
    if (!std::isfinite(refreshRateHertz) || refreshRateHertz < 0.0F)
    {
        return core::Result<DisplayInfo>::FromError(MakeBackendDataError(
            "SDL_GetCurrentDisplayMode returned an invalid refresh rate for " +
            context + "."));
    }
    const std::optional<float> refreshRate = refreshRateHertz > 0.0F
                                                 ? std::optional<float>{refreshRateHertz}
                                                 : std::nullopt;

    const DisplayOrientation orientation = ConvertOrientation(
        m_displayBackend.getCurrentOrientation(m_displayBackend.context,
                                               backendDisplayId));

    const float contentScale =
        m_displayBackend.getContentScale(m_displayBackend.context, backendDisplayId);
    if (contentScale == 0.0F)
    {
        return core::Result<DisplayInfo>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetDisplayContentScale", context));
    }
    auto validContentScale = ValidateScale(
        contentScale, "SDL_GetDisplayContentScale", context);
    if (!validContentScale.HasValue())
    {
        return core::Result<DisplayInfo>::FromError(
            std::move(validContentScale).GetError());
    }

    return DisplayInfo{
        id,
        name,
        std::move(bounds).GetValue(),
        std::move(usableBounds).GetValue(),
        refreshRate,
        orientation,
        validContentScale.GetValue()};
}

core::Result<std::vector<DisplayInfo>> PlatformRuntimeState::EnumerateDisplays()
{
    auto refresh = RefreshDisplays();
    if (!refresh.HasValue())
    {
        return core::Result<std::vector<DisplayInfo>>::FromError(
            std::move(refresh).GetError());
    }

    std::vector<DisplayInfo> displays;
    displays.reserve(refresh.GetValue().size());
    for (const std::uint32_t backendDisplayId : refresh.GetValue())
    {
        const auto mapping = m_displaysByBackendId.find(backendDisplayId);
        PONDER_VERIFY(mapping != m_displaysByBackendId.end() &&
                          mapping->second.connected,
                      "Connected backend display {} has no project mapping",
                      backendDisplayId);
        auto info = QueryDisplayInfo(mapping->second.id, backendDisplayId);
        if (!info.HasValue())
        {
            return core::Result<std::vector<DisplayInfo>>::FromError(
                std::move(info).GetError());
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
        return core::Result<DisplayInfo>::FromError(core::Error{
            kInvalidArgumentCode, "Display ID must be valid."});
    }

    auto refresh = RefreshDisplays();
    if (!refresh.HasValue())
    {
        return core::Result<DisplayInfo>::FromError(
            std::move(refresh).GetError());
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
        if (confirmedMapping == m_displaysById.end() ||
            !confirmedMapping->second.connected)
        {
            return core::Result<DisplayInfo>::FromError(
                MakeDisplayNotFoundError(id));
        }
    }
    return core::Result<DisplayInfo>::FromError(std::move(primaryError));
}

core::Result<DisplayId> PlatformRuntimeState::GetDisplayIdForWindow(
    void* nativeWindow, WindowId windowId)
{
    VerifyOwnerThread("window display query");
    const std::string context = MakeWindowContext(windowId);
    const std::uint32_t backendDisplayId =
        m_displayBackend.getForWindow(m_displayBackend.context, nativeWindow);
    if (backendDisplayId == 0)
    {
        return core::Result<DisplayId>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetDisplayForWindow", context));
    }

    auto refresh = RefreshDisplays();
    if (!refresh.HasValue())
    {
        return core::Result<DisplayId>::FromError(
            std::move(refresh).GetError());
    }

    const auto mapping = m_displaysByBackendId.find(backendDisplayId);
    if (mapping == m_displaysByBackendId.end() || !mapping->second.connected)
    {
        return core::Result<DisplayId>::FromError(core::Error{
            kNotFoundCode, "The window's display is not connected."});
    }
    return mapping->second.id;
}

core::Result<float> PlatformRuntimeState::GetPixelDensityForWindow(
    void* nativeWindow, WindowId windowId) const
{
    VerifyOwnerThread("window pixel density query");
    const std::string context = MakeWindowContext(windowId);
    const float density = m_displayBackend.getWindowPixelDensity(
        m_displayBackend.context, nativeWindow);
    if (density == 0.0F)
    {
        return core::Result<float>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetWindowPixelDensity", context));
    }
    return ValidateScale(density, "SDL_GetWindowPixelDensity", context);
}

core::Result<float> PlatformRuntimeState::GetDisplayScaleForWindow(
    void* nativeWindow, WindowId windowId) const
{
    VerifyOwnerThread("window display scale query");
    const std::string context = MakeWindowContext(windowId);
    const float scale = m_displayBackend.getWindowDisplayScale(
        m_displayBackend.context, nativeWindow);
    if (scale == 0.0F)
    {
        return core::Result<float>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetWindowDisplayScale", context));
    }
    return ValidateScale(scale, "SDL_GetWindowDisplayScale", context);
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
    return m_runtime->GetDisplayIdForWindow(m_nativeWindow, m_id);
}

core::Result<float> WindowImpl::GetPixelDensity() const
{
    VerifyUsable("pixel density query");
    return m_runtime->GetPixelDensityForWindow(m_nativeWindow, m_id);
}

core::Result<float> WindowImpl::GetDisplayScale() const
{
    VerifyUsable("display scale query");
    return m_runtime->GetDisplayScaleForWindow(m_nativeWindow, m_id);
}
} // namespace detail
} // namespace pond::platform
