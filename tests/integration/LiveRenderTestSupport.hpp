#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformEvent.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/RenderError.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>

#ifndef PONDER_RENDER_REQUIRE_LIVE_TESTS
#define PONDER_RENDER_REQUIRE_LIVE_TESTS 0
#endif

#ifndef PONDER_RENDER_LIVE_VALIDATION_MODE
#define PONDER_RENDER_LIVE_VALIDATION_MODE "Default"
#endif

namespace pond::render::test
{
[[nodiscard]] constexpr RenderValidationMode GetLiveRenderValidationMode() noexcept
{
    constexpr std::string_view kMode{PONDER_RENDER_LIVE_VALIDATION_MODE};
    if constexpr (kMode == "Disabled")
    {
        return RenderValidationMode::Disabled;
    }
    else if constexpr (kMode == "Standard")
    {
        return RenderValidationMode::Standard;
    }
    else if constexpr (kMode == "Synchronization")
    {
        return RenderValidationMode::Synchronization;
    }
    else if constexpr (kMode == "BestPractices")
    {
        return RenderValidationMode::BestPractices;
    }
    else if constexpr (kMode == "GpuAssisted")
    {
        return RenderValidationMode::GpuAssisted;
    }
    else
    {
        return RenderValidationMode::Default;
    }
}

[[nodiscard]] constexpr RenderBootstrapDesc MakeLiveRenderBootstrapDesc() noexcept
{
    return RenderBootstrapDesc{.validationMode = GetLiveRenderValidationMode()};
}

enum class LivePrerequisiteOperation : std::uint8_t
{
    PlatformRuntime = 0,
    PlatformWindow = 1,
    Bootstrap = 2,
    Surface = 3,
    Adapter = 4,
    Device = 5,
    Target = 6
};

enum class MissingLivePrerequisite : std::uint8_t
{
    Display = 0,
    VulkanLoader = 1,
    VulkanVersion = 2,
    InstanceLayerOrExtension = 3,
    PresentationSurface = 4,
    CompatibleAdapter = 5,
    DeviceCapability = 6
};

[[nodiscard]] constexpr std::string_view GetMissingLivePrerequisiteName(
    MissingLivePrerequisite prerequisite) noexcept
{
    switch (prerequisite)
    {
    case MissingLivePrerequisite::Display:
        return "display";
    case MissingLivePrerequisite::VulkanLoader:
        return "Vulkan loader";
    case MissingLivePrerequisite::VulkanVersion:
        return "required Vulkan API version";
    case MissingLivePrerequisite::InstanceLayerOrExtension:
        return "required Vulkan instance layer or extension";
    case MissingLivePrerequisite::PresentationSurface:
        return "Vulkan presentation surface capability";
    case MissingLivePrerequisite::CompatibleAdapter:
        return "compatible Vulkan adapter";
    case MissingLivePrerequisite::DeviceCapability:
        return "required Vulkan device capability";
    }

    return "unknown live prerequisite";
}

[[nodiscard]] constexpr bool ContainsLivePrerequisiteText(std::string_view text,
                                                          std::string_view expected) noexcept
{
    return text.find(expected) != std::string_view::npos;
}

[[nodiscard]] constexpr std::optional<MissingLivePrerequisite>
ClassifyOptionalMissingLivePrerequisite(core::ErrorCode code, LivePrerequisiteOperation operation,
                                        std::string_view message) noexcept
{
    if ((operation == LivePrerequisiteOperation::PlatformRuntime ||
         operation == LivePrerequisiteOperation::PlatformWindow) &&
        (code == platform::ToErrorCode(platform::PlatformErrorCode::NotFound) ||
         code == platform::ToErrorCode(platform::PlatformErrorCode::Unsupported)))
    {
        return MissingLivePrerequisite::Display;
    }

    if (code == ToErrorCode(RenderErrorCode::LoaderUnavailable))
    {
        return MissingLivePrerequisite::VulkanLoader;
    }
    if (code == ToErrorCode(RenderErrorCode::NoCompatibleAdapter) &&
        operation == LivePrerequisiteOperation::Adapter)
    {
        return MissingLivePrerequisite::CompatibleAdapter;
    }

    if (code == ToErrorCode(RenderErrorCode::UnsupportedSurface) &&
        (operation == LivePrerequisiteOperation::Surface ||
         operation == LivePrerequisiteOperation::Target))
    {
        constexpr std::array<std::string_view, 8U> kMissingPresentationCapabilities{
            "does not expose required instance extension",
            "surface reported no supported swapchain formats",
            "surface does not support the required SDR sRGB presentation format",
            "surface does not support required opaque composition",
            "surface does not support color-attachment swapchain images",
            "required presentation policy is unavailable",
            "no presentation queue for the surface",
            "queued-frame latency is unavailable"};
        for (const std::string_view capability : kMissingPresentationCapabilities)
        {
            if (ContainsLivePrerequisiteText(message, capability))
            {
                return MissingLivePrerequisite::PresentationSurface;
            }
        }
    }

    if (code != ToErrorCode(RenderErrorCode::UnsupportedCapability))
    {
        return std::nullopt;
    }

    if (operation == LivePrerequisiteOperation::Bootstrap ||
        operation == LivePrerequisiteOperation::Surface)
    {
        if (ContainsLivePrerequisiteText(message, "installed Vulkan loader reports API ") &&
            ContainsLivePrerequisiteText(message, "requires at least Vulkan "))
        {
            return MissingLivePrerequisite::VulkanVersion;
        }
        if (ContainsLivePrerequisiteText(message, "requires missing instance layer ") ||
            ContainsLivePrerequisiteText(message, "requires missing instance extension ") ||
            ContainsLivePrerequisiteText(message, "symbolicName=VK_ERROR_LAYER_NOT_PRESENT") ||
            ContainsLivePrerequisiteText(message, "symbolicName=VK_ERROR_EXTENSION_NOT_PRESENT"))
        {
            return MissingLivePrerequisite::InstanceLayerOrExtension;
        }
    }

    if ((operation == LivePrerequisiteOperation::Device ||
         operation == LivePrerequisiteOperation::Target) &&
        (ContainsLivePrerequisiteText(message, "symbolicName=VK_ERROR_EXTENSION_NOT_PRESENT") ||
         ContainsLivePrerequisiteText(message, "symbolicName=VK_ERROR_FEATURE_NOT_PRESENT")))
    {
        return MissingLivePrerequisite::DeviceCapability;
    }

    // Missing dispatch and generic backend failures are implementation failures, not prerequisites.
    return std::nullopt;
}

static_assert(ClassifyOptionalMissingLivePrerequisite(
                  ToErrorCode(RenderErrorCode::UnsupportedCapability),
                  LivePrerequisiteOperation::Surface,
                  "Explicit Vulkan validation requires missing instance layer "
                  "VK_LAYER_KHRONOS_validation.") ==
              MissingLivePrerequisite::InstanceLayerOrExtension);
static_assert(!ClassifyOptionalMissingLivePrerequisite(
    ToErrorCode(RenderErrorCode::UnsupportedCapability), LivePrerequisiteOperation::Surface,
    "vkCreateWin32SurfaceKHR dispatch is unavailable."));
static_assert(!ClassifyOptionalMissingLivePrerequisite(
    ToErrorCode(RenderErrorCode::BackendFailure), LivePrerequisiteOperation::Bootstrap,
    "A generic backend failure must never become a missing-capability skip."));

[[nodiscard]] inline std::optional<MissingLivePrerequisite> ClassifyMissingLivePrerequisite(
    const core::Error& error, LivePrerequisiteOperation operation) noexcept
{
#if PONDER_RENDER_REQUIRE_LIVE_TESTS != 0
    (void)error;
    (void)operation;
    return std::nullopt;
#else
    return ClassifyOptionalMissingLivePrerequisite(error.GetCode(), operation, error.GetMessage());
#endif
}

[[nodiscard]] inline std::optional<std::string> MakeOptionalLiveSkipReason(
    const core::Error& error, LivePrerequisiteOperation operation, std::string_view context)
{
    const std::optional<MissingLivePrerequisite> prerequisite =
        ClassifyMissingLivePrerequisite(error, operation);
    if (!prerequisite.has_value())
    {
        return std::nullopt;
    }

    std::string reason{context};
    reason += " cannot run because this optional live host has no ";
    reason += GetMissingLivePrerequisiteName(*prerequisite);
    reason += ": ";
    reason += error.GetMessage();
    return reason;
}

struct CoalescedWindowEvents final
{
    platform::WindowId windowId{};
    std::size_t logicalSizeChanges{};
    std::size_t pixelSizeChanges{};
    std::size_t visibilityChanges{};
    std::size_t stateChanges{};
    bool closeRequested{};

    [[nodiscard]] constexpr std::size_t GetResizeEventCount() const noexcept
    {
        return logicalSizeChanges + pixelSizeChanges;
    }
};

inline void ObserveWindowEvent(CoalescedWindowEvents& events,
                               const platform::PlatformEvent& event) noexcept
{
    std::visit(
        [&events](const auto& payload) noexcept
        {
            using Event = std::remove_cvref_t<decltype(payload)>;
            if constexpr (requires { payload.windowId; })
            {
                if (payload.windowId != events.windowId)
                {
                    return;
                }

                if constexpr (std::is_same_v<Event, platform::WindowLogicalSizeChangedEvent>)
                {
                    ++events.logicalSizeChanges;
                }
                else if constexpr (std::is_same_v<Event, platform::WindowPixelSizeChangedEvent>)
                {
                    ++events.pixelSizeChanges;
                }
                else if constexpr (std::is_same_v<Event, platform::WindowVisibilityChangedEvent>)
                {
                    ++events.visibilityChanges;
                }
                else if constexpr (std::is_same_v<Event, platform::WindowStateChangedEvent>)
                {
                    ++events.stateChanges;
                }
                else if constexpr (std::is_same_v<Event, platform::WindowCloseRequestedEvent>)
                {
                    events.closeRequested = true;
                }
            }
        },
        event);
}

inline void DrainPlatformEvents(platform::PlatformRuntime& runtime, CoalescedWindowEvents& events)
{
    while (std::optional<platform::PlatformEvent> event = runtime.PollEvent())
    {
        ObserveWindowEvent(events, *event);
    }
}

template <typename Predicate>
[[nodiscard]] bool PumpPlatformEventsUntil(platform::PlatformRuntime& runtime,
                                           CoalescedWindowEvents& events, Predicate&& predicate,
                                           std::chrono::milliseconds timeout = std::chrono::seconds{
                                               3})
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do
    {
        DrainPlatformEvents(runtime, events);
        if (std::forward<Predicate>(predicate)())
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    } while (std::chrono::steady_clock::now() < deadline);

    DrainPlatformEvents(runtime, events);
    return std::forward<Predicate>(predicate)();
}

[[nodiscard]] inline core::Result<RenderTargetSnapshot> CaptureWindowSnapshot(
    const platform::Window& window, std::uint64_t revision,
    PresentationEnvironmentRevision presentationRevision = PresentationEnvironmentRevision{1U})
{
    auto pixelSize = window.GetPixelSize();
    if (!pixelSize)
    {
        return core::Result<RenderTargetSnapshot>::FromError(std::move(pixelSize).GetError());
    }

    auto logicalSize = window.GetLogicalSize();
    if (!logicalSize)
    {
        return core::Result<RenderTargetSnapshot>::FromError(std::move(logicalSize).GetError());
    }

    auto visible = window.IsVisible();
    if (!visible)
    {
        return core::Result<RenderTargetSnapshot>::FromError(std::move(visible).GetError());
    }

    auto state = window.GetState();
    if (!state)
    {
        return core::Result<RenderTargetSnapshot>::FromError(std::move(state).GetError());
    }

    return core::Result<RenderTargetSnapshot>::FromValue(
        RenderTargetSnapshot{window.GetId(), pixelSize.GetValue(), logicalSize.GetValue(),
                             visible.GetValue(), state.GetValue(), presentationRevision, revision});
}

[[nodiscard]] inline std::string FormatValidationReport(const RenderValidationReport& report)
{
    std::ostringstream output;
    output << "warnings=" << report.warningCount << " errors=" << report.errorCount
           << " dropped=" << report.droppedMessageCount << " messageIds=[";
    for (std::size_t index = 0U; index < report.capturedMessageCount; ++index)
    {
        if (index != 0U)
        {
            output << ", ";
        }
        const RenderValidationMessage& message = report.capturedMessages[index];
        output << message.GetMessageIdName() << '(' << message.messageIdNumber
               << ") operation=" << message.GetOperationContext()
               << " message=" << message.GetMessageText();
        if (message.operationContextTruncated || message.messageTextTruncated)
        {
            output << " truncated=[operation=" << message.operationContextTruncated
                   << ", message=" << message.messageTextTruncated << ']';
        }
    }
    output << ']';
    return output.str();
}
} // namespace pond::render::test
