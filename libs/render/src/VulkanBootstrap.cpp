#include "VulkanBootstrap.hpp"

#include "VulkanDiagnostics.hpp"

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <ponder/core/Log.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#ifndef PONDER_RENDER_ENABLE_VALIDATION
#define PONDER_RENDER_ENABLE_VALIDATION 0
#endif

#ifndef PONDER_RENDER_DEVELOPER_BUILD
#define PONDER_RENDER_DEVELOPER_BUILD 0
#endif

namespace pond::render::detail
{
namespace
{
inline constexpr char kSurfaceExtensionName[] = "VK_KHR_surface";
inline constexpr char kWin32SurfaceExtensionName[] = "VK_KHR_win32_surface";
inline constexpr char kX11SurfaceExtensionName[] = "VK_KHR_xlib_surface";
inline constexpr char kWaylandSurfaceExtensionName[] = "VK_KHR_wayland_surface";
inline constexpr char kSwapchainExtensionName[] = "VK_KHR_swapchain";
inline constexpr char kGetSurfaceCapabilities2ExtensionName[] =
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;
inline constexpr char kKhrSurfaceMaintenance1ExtensionName[] =
    VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME;
inline constexpr char kExtSurfaceMaintenance1ExtensionName[] =
    VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME;
inline constexpr char kKhrSwapchainMaintenance1ExtensionName[] =
    VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
inline constexpr char kExtSwapchainMaintenance1ExtensionName[] =
    VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
inline constexpr char kPresentIdExtensionName[] = VK_KHR_PRESENT_ID_EXTENSION_NAME;
inline constexpr char kPresentWaitExtensionName[] = VK_KHR_PRESENT_WAIT_EXTENSION_NAME;
inline constexpr char kValidationLayerName[] = "VK_LAYER_KHRONOS_validation";
inline constexpr std::string_view kValidationLogCategory{"render.vulkan.validation"};
inline constexpr std::string_view kDebugLogCategory{"render.vulkan.debug"};
template <typename... Handlers>
struct Overloaded : Handlers...
{
    using Handlers::operator()...;
};

template <typename... Handlers>
Overloaded(Handlers...) -> Overloaded<Handlers...>;

struct VulkanValidationSelection final
{
    RenderValidationMode requestedMode{RenderValidationMode::Default};
    RenderValidationMode enabledMode{RenderValidationMode::Disabled};
    bool validationEnabled{};
    bool debugUtilsEnabled{};
    bool validationFeaturesEnabled{};
    std::vector<const char*> enabledLayers{};
    std::vector<const char*> enabledExtensions{};
    std::vector<VkValidationFeatureEnableEXT> enabledFeatures{};
};

[[nodiscard]] core::Error MakeRenderError(RenderErrorCode code, std::string message)
{
    return core::Error{ToErrorCode(code), std::move(message)};
}

[[nodiscard]] const char* GetVkResultName(VkResult result) noexcept
{
    switch (result)
    {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    default:
        return "unknown VkResult";
    }
}

[[nodiscard]] std::string MakeVulkanFailureMessage(std::string_view operation, VkResult result)
{
    std::string message;
    message.reserve(operation.size() + 80U);
    message += "operation=";
    message += operation;
    message += " nativeCode=";
    message += std::to_string(static_cast<std::int64_t>(result));
    message += " symbolicName=";
    message += GetVkResultName(result);
    return message;
}

[[nodiscard]] core::Error MakeVulkanError(RenderErrorCode code, std::string_view operation,
                                          VkResult result)
{
    std::string message = MakeVulkanFailureMessage(operation, result);
    VulkanDiagnosticScope::Record(BackendDiagnostic{.backend = RenderBackendKind::Vulkan,
                                                    .renderCode = code,
                                                    .nativeCode = static_cast<std::int64_t>(result),
                                                    .symbolicName = GetVkResultName(result),
                                                    .operation = std::string{operation},
                                                    .validationContext = message});
    return MakeRenderError(code, std::move(message));
}

[[nodiscard]] std::string FormatVulkanVersion(std::uint32_t version)
{
    return std::to_string(VK_API_VERSION_MAJOR(version)) + "." +
           std::to_string(VK_API_VERSION_MINOR(version)) + "." +
           std::to_string(VK_API_VERSION_PATCH(version));
}

[[nodiscard]] RenderErrorCode MapInstanceCreateFailure(VkResult result) noexcept
{
    switch (result)
    {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return RenderErrorCode::OutOfMemory;
    case VK_ERROR_EXTENSION_NOT_PRESENT:
    case VK_ERROR_LAYER_NOT_PRESENT:
        return RenderErrorCode::UnsupportedCapability;
    default:
        return RenderErrorCode::BackendFailure;
    }
}

[[nodiscard]] RenderErrorCode MapSurfaceCreateFailure(VkResult result) noexcept
{
    switch (result)
    {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return RenderErrorCode::OutOfMemory;
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return RenderErrorCode::UnsupportedCapability;
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return RenderErrorCode::UnsupportedSurface;
    case VK_ERROR_SURFACE_LOST_KHR:
        return RenderErrorCode::SurfaceLost;
    default:
        return RenderErrorCode::BackendFailure;
    }
}

[[nodiscard]] RenderErrorCode MapDeviceFailure(VkResult result) noexcept
{
    switch (result)
    {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return RenderErrorCode::OutOfMemory;
    case VK_ERROR_EXTENSION_NOT_PRESENT:
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return RenderErrorCode::UnsupportedCapability;
    case VK_ERROR_DEVICE_LOST:
        return RenderErrorCode::DeviceLost;
    default:
        return RenderErrorCode::BackendFailure;
    }
}

[[nodiscard]] RenderErrorCode MapSwapchainFailure(VkResult result) noexcept
{
    switch (result)
    {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return RenderErrorCode::OutOfMemory;
    case VK_ERROR_DEVICE_LOST:
        return RenderErrorCode::DeviceLost;
    case VK_ERROR_SURFACE_LOST_KHR:
        return RenderErrorCode::SurfaceLost;
    default:
        return RenderErrorCode::BackendFailure;
    }
}

[[nodiscard]] RenderErrorCode MapFrameFailure(VkResult result) noexcept
{
    switch (result)
    {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return RenderErrorCode::OutOfMemory;
    case VK_ERROR_DEVICE_LOST:
        return RenderErrorCode::DeviceLost;
    case VK_ERROR_SURFACE_LOST_KHR:
        return RenderErrorCode::SurfaceLost;
    default:
        return RenderErrorCode::BackendFailure;
    }
}

[[nodiscard]] bool WasPresentationOperationEnqueued(VkResult result) noexcept
{
    switch (result)
    {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_ERROR_SURFACE_LOST_KHR:
    case VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT:
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool HasRequiredDispatch(const VulkanGlobalDispatch& dispatch) noexcept
{
    return dispatch.initialize != nullptr && dispatch.enumerateInstanceVersion != nullptr &&
           dispatch.enumerateInstanceExtensionProperties != nullptr &&
           dispatch.enumerateInstanceLayerProperties != nullptr &&
           dispatch.createInstance != nullptr && dispatch.destroyInstance != nullptr &&
           (dispatch.loadInstanceTable != nullptr || dispatch.loadInstanceOnly != nullptr);
}

[[nodiscard]] bool HasDebugMessengerDispatch(const VulkanGlobalDispatch& dispatch) noexcept
{
    return dispatch.createDebugUtilsMessenger != nullptr &&
           dispatch.destroyDebugUtilsMessenger != nullptr;
}

[[nodiscard]] bool HasSurfaceDispatch(const VulkanGlobalDispatch& dispatch,
                                      VulkanGlobalDispatch::CreateSurfaceFn createSurface) noexcept
{
    return createSurface != nullptr && dispatch.destroySurface != nullptr;
}

[[nodiscard]] bool HasAdapterDispatch(const VulkanGlobalDispatch& dispatch) noexcept
{
    return dispatch.enumeratePhysicalDevices != nullptr &&
           dispatch.getPhysicalDeviceProperties2 != nullptr &&
           dispatch.getPhysicalDeviceMemoryProperties != nullptr &&
           dispatch.getPhysicalDeviceQueueFamilyProperties != nullptr &&
           dispatch.getPhysicalDeviceSurfaceSupport != nullptr &&
           dispatch.getPhysicalDeviceSurfaceCapabilities != nullptr &&
           dispatch.getPhysicalDeviceSurfaceFormats != nullptr &&
           dispatch.getPhysicalDeviceSurfacePresentModes != nullptr &&
           dispatch.enumerateDeviceExtensionProperties != nullptr;
}

[[nodiscard]] bool HasDeviceDispatch(const VulkanGlobalDispatch& dispatch) noexcept
{
    return HasAdapterDispatch(dispatch) && dispatch.getPhysicalDeviceFeatures2 != nullptr &&
           dispatch.createDevice != nullptr && dispatch.destroyDevice != nullptr &&
           (dispatch.loadDeviceTable != nullptr || dispatch.loadDevice != nullptr) &&
           dispatch.getDeviceQueue != nullptr && dispatch.deviceWaitIdle != nullptr &&
           dispatch.queueWaitIdle != nullptr &&
           (dispatch.createOwnerLocalAllocator != nullptr || dispatch.createAllocator != nullptr) &&
           dispatch.destroyAllocator != nullptr;
}

[[nodiscard]] bool HasSwapchainDispatch(const VulkanGlobalDispatch& dispatch) noexcept
{
    return dispatch.getPhysicalDeviceSurfaceCapabilities != nullptr &&
           dispatch.getPhysicalDeviceSurfaceFormats != nullptr &&
           dispatch.getPhysicalDeviceSurfacePresentModes != nullptr &&
           dispatch.createSwapchain != nullptr && dispatch.destroySwapchain != nullptr &&
           dispatch.getSwapchainImages != nullptr && dispatch.createImageView != nullptr &&
           dispatch.destroyImageView != nullptr && dispatch.createRenderPass != nullptr &&
           dispatch.destroyRenderPass != nullptr && dispatch.createFramebuffer != nullptr &&
           dispatch.destroyFramebuffer != nullptr;
}

[[nodiscard]] bool HasFrameDispatch(const VulkanGlobalDispatch& dispatch) noexcept
{
    return dispatch.createCommandPool != nullptr && dispatch.destroyCommandPool != nullptr &&
           dispatch.allocateCommandBuffers != nullptr && dispatch.resetCommandBuffer != nullptr &&
           dispatch.beginCommandBuffer != nullptr && dispatch.endCommandBuffer != nullptr &&
           dispatch.cmdBeginRenderPass != nullptr && dispatch.cmdEndRenderPass != nullptr &&
           dispatch.createSemaphore != nullptr && dispatch.destroySemaphore != nullptr &&
           dispatch.createFence != nullptr && dispatch.destroyFence != nullptr &&
           dispatch.waitForFences != nullptr && dispatch.resetFences != nullptr &&
           dispatch.acquireNextImage != nullptr && dispatch.queueSubmit != nullptr &&
           dispatch.queuePresent != nullptr;
}

template <typename HandleType>
[[nodiscard]] std::uint64_t ToDebugObjectHandle(HandleType handle) noexcept
{
    if constexpr (std::is_pointer_v<HandleType>)
    {
        return reinterpret_cast<std::uint64_t>(handle);
    }
    else
    {
        return static_cast<std::uint64_t>(handle);
    }
}

template <typename HandleType>
void TryNameObject(const VulkanGlobalDispatch& dispatch, VkDevice device, HandleType handle,
                   VkObjectType objectType, const char* name) noexcept
{
    const std::uint64_t objectHandle = ToDebugObjectHandle(handle);
    if (dispatch.setDebugUtilsObjectName == nullptr || device == VK_NULL_HANDLE ||
        objectHandle == 0U || name == nullptr)
    {
        return;
    }

    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = objectHandle;
    nameInfo.pObjectName = name;
    [[maybe_unused]] const VkResult result = dispatch.setDebugUtilsObjectName(device, &nameInfo);
}

[[nodiscard]] std::string MakeTargetDebugObjectName(platform::WindowId windowId,
                                                    std::string_view resourceName)
{
    std::string name{"pond.render.target/window:"};
    name += std::to_string(windowId.GetValue());
    name += '/';
    name += resourceName;
    return name;
}

[[nodiscard]] std::string MakeTargetDebugObjectName(platform::WindowId windowId,
                                                    std::string_view resourceName,
                                                    std::size_t resourceIndex)
{
    std::string name = MakeTargetDebugObjectName(windowId, resourceName);
    name += '.';
    name += std::to_string(resourceIndex);
    return name;
}

void TryBeginLabel(const VulkanGlobalDispatch& dispatch, VkCommandBuffer commandBuffer,
                   const char* name) noexcept
{
    if (dispatch.cmdBeginDebugUtilsLabel == nullptr || commandBuffer == VK_NULL_HANDLE ||
        name == nullptr)
    {
        return;
    }

    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    label.color[0] = 0.2F;
    label.color[1] = 0.4F;
    label.color[2] = 0.9F;
    label.color[3] = 1.0F;
    dispatch.cmdBeginDebugUtilsLabel(commandBuffer, &label);
}

void TryEndLabel(const VulkanGlobalDispatch& dispatch, VkCommandBuffer commandBuffer) noexcept
{
    if (dispatch.cmdEndDebugUtilsLabel != nullptr && commandBuffer != VK_NULL_HANDLE)
    {
        dispatch.cmdEndDebugUtilsLabel(commandBuffer);
    }
}
[[nodiscard]] core::Result<std::vector<VkExtensionProperties>> EnumerateInstanceExtensions(
    const VulkanGlobalDispatch& dispatch)
{
    std::uint32_t extensionCount{};
    VkResult result =
        dispatch.enumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (result != VK_SUCCESS)
    {
        return core::Result<std::vector<VkExtensionProperties>>::FromError(
            MakeVulkanError(RenderErrorCode::BackendFailure,
                            "vkEnumerateInstanceExtensionProperties.count", result));
    }

    std::vector<VkExtensionProperties> extensions(extensionCount);
    if (extensionCount == 0U)
    {
        return core::Result<std::vector<VkExtensionProperties>>::FromValue(std::move(extensions));
    }

    result =
        dispatch.enumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    if (result != VK_SUCCESS && result != VK_INCOMPLETE)
    {
        return core::Result<std::vector<VkExtensionProperties>>::FromError(
            MakeVulkanError(RenderErrorCode::BackendFailure,
                            "vkEnumerateInstanceExtensionProperties.read", result));
    }

    extensions.resize(extensionCount);
    return core::Result<std::vector<VkExtensionProperties>>::FromValue(std::move(extensions));
}

[[nodiscard]] core::Result<std::vector<VkLayerProperties>> EnumerateInstanceLayers(
    const VulkanGlobalDispatch& dispatch)
{
    std::uint32_t layerCount{};
    VkResult result = dispatch.enumerateInstanceLayerProperties(&layerCount, nullptr);
    if (result != VK_SUCCESS)
    {
        return core::Result<std::vector<VkLayerProperties>>::FromError(MakeVulkanError(
            RenderErrorCode::BackendFailure, "vkEnumerateInstanceLayerProperties.count", result));
    }

    std::vector<VkLayerProperties> layers(layerCount);
    if (layerCount == 0U)
    {
        return core::Result<std::vector<VkLayerProperties>>::FromValue(std::move(layers));
    }

    result = dispatch.enumerateInstanceLayerProperties(&layerCount, layers.data());
    if (result != VK_SUCCESS && result != VK_INCOMPLETE)
    {
        return core::Result<std::vector<VkLayerProperties>>::FromError(MakeVulkanError(
            RenderErrorCode::BackendFailure, "vkEnumerateInstanceLayerProperties.read", result));
    }

    layers.resize(layerCount);
    return core::Result<std::vector<VkLayerProperties>>::FromValue(std::move(layers));
}

[[nodiscard]] bool ContainsExtension(const std::vector<VkExtensionProperties>& extensions,
                                     const char* requiredExtension) noexcept
{
    return std::ranges::any_of(extensions,
                               [requiredExtension](const VkExtensionProperties& extension) noexcept
                               {
                                   return std::strcmp(extension.extensionName, requiredExtension) ==
                                          0;
                               });
}

[[nodiscard]] bool ContainsLayer(const std::vector<VkLayerProperties>& layers,
                                 const char* requiredLayer) noexcept
{
    return std::ranges::any_of(layers,
                               [requiredLayer](const VkLayerProperties& layer) noexcept
                               {
                                   return std::strcmp(layer.layerName, requiredLayer) == 0;
                               });
}

[[nodiscard]] bool IsValidationFeatureMode(RenderValidationMode mode) noexcept
{
    return mode == RenderValidationMode::Synchronization ||
           mode == RenderValidationMode::BestPractices || mode == RenderValidationMode::GpuAssisted;
}

[[nodiscard]] VkValidationFeatureEnableEXT GetValidationFeature(RenderValidationMode mode) noexcept
{
    switch (mode)
    {
    case RenderValidationMode::Synchronization:
        return VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    case RenderValidationMode::BestPractices:
        return VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT;
    case RenderValidationMode::GpuAssisted:
        return VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT;
    case RenderValidationMode::Default:
    case RenderValidationMode::Disabled:
    case RenderValidationMode::Standard:
        break;
    }

    return VK_VALIDATION_FEATURE_ENABLE_MAX_ENUM_EXT;
}

[[nodiscard]] VulkanDebugUtilityHooks MakeDebugUtilityHooks(const VulkanGlobalDispatch& dispatch,
                                                            bool debugUtilsEnabled) noexcept
{
    return VulkanDebugUtilityHooks{
        .objectNames = debugUtilsEnabled && dispatch.setDebugUtilsObjectName != nullptr,
        .commandLabels = debugUtilsEnabled && dispatch.cmdBeginDebugUtilsLabel != nullptr &&
                         dispatch.cmdEndDebugUtilsLabel != nullptr,
        .timingMarkers = false,
        .captureRegions = false};
}
[[nodiscard]] core::Result<VulkanValidationSelection> SelectValidationPolicy(
    RenderValidationMode requestedMode, const std::vector<VkExtensionProperties>& extensions,
    const std::vector<VkLayerProperties>& layers)
{
    VulkanValidationSelection selection{.requestedMode = requestedMode};
    if (requestedMode == RenderValidationMode::Disabled)
    {
        return core::Result<VulkanValidationSelection>::FromValue(std::move(selection));
    }

    const bool hasValidationLayer = ContainsLayer(layers, kValidationLayerName);
    const bool hasDebugUtils = ContainsExtension(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    const bool hasValidationFeatures =
        ContainsExtension(extensions, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);

    RenderValidationMode effectiveMode = requestedMode;
    if (requestedMode == RenderValidationMode::Default)
    {
        if (!IsDefaultValidationEnabledForDeveloperBuild() || !hasValidationLayer || !hasDebugUtils)
        {
            return core::Result<VulkanValidationSelection>::FromValue(std::move(selection));
        }

        effectiveMode = RenderValidationMode::Standard;
    }

    if (!hasValidationLayer)
    {
        return core::Result<VulkanValidationSelection>::FromError(MakeRenderError(
            RenderErrorCode::UnsupportedCapability,
            std::string{"Explicit Vulkan validation requires missing instance layer "} +
                kValidationLayerName + "."));
    }

    if (!hasDebugUtils)
    {
        return core::Result<VulkanValidationSelection>::FromError(MakeRenderError(
            RenderErrorCode::UnsupportedCapability,
            std::string{"Explicit Vulkan validation requires missing instance extension "} +
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME + "."));
    }

    selection.enabledMode = effectiveMode;
    selection.validationEnabled = true;
    selection.debugUtilsEnabled = true;
    selection.enabledLayers.push_back(kValidationLayerName);
    selection.enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    if (IsValidationFeatureMode(effectiveMode))
    {
        if (!hasValidationFeatures)
        {
            return core::Result<VulkanValidationSelection>::FromError(MakeRenderError(
                RenderErrorCode::UnsupportedCapability,
                std::string{"Vulkan validation mode requires missing instance extension "} +
                    VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME + "."));
        }

        selection.validationFeaturesEnabled = true;
        selection.enabledExtensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        selection.enabledFeatures.push_back(GetValidationFeature(effectiveMode));
    }

    return core::Result<VulkanValidationSelection>::FromValue(std::move(selection));
}

[[nodiscard]] VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo(
    VulkanDebugMessengerContext* context) noexcept
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = &HandleVulkanDebugUtilsMessage;
    createInfo.pUserData = context;
    return createInfo;
}

[[nodiscard]] core::LogLevel MapDebugSeverity(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity) noexcept
{
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
    {
        return core::LogLevel::Error;
    }

    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
    {
        return core::LogLevel::Warning;
    }

    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0)
    {
        return core::LogLevel::Info;
    }

    return core::LogLevel::Debug;
}

[[nodiscard]] bool IsWarningOrError(VkDebugUtilsMessageSeverityFlagBitsEXT severity) noexcept
{
    return (severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) != 0;
}

[[nodiscard]] bool IsFiltered(const VulkanDebugMessengerContext& context,
                              const VkDebugUtilsMessengerCallbackDataEXT* callbackData) noexcept
{
    const std::string_view name = callbackData != nullptr && callbackData->pMessageIdName != nullptr
                                      ? std::string_view{callbackData->pMessageIdName}
                                      : std::string_view{};
    const std::int32_t number = callbackData != nullptr ? callbackData->messageIdNumber : 0;

    return std::ranges::any_of(context.exactMessageFilters,
                               [name, number](const VulkanValidationMessageFilter& filter) noexcept
                               {
                                   return filter.Matches(name, number);
                               });
}
void AppendCallbackMessage(std::string& output,
                           const VkDebugUtilsMessengerCallbackDataEXT* callbackData)
{
    const char* const idName = callbackData != nullptr && callbackData->pMessageIdName != nullptr
                                   ? callbackData->pMessageIdName
                                   : "unknown-message";
    const std::int32_t idNumber = callbackData != nullptr ? callbackData->messageIdNumber : 0;
    const char* const message = callbackData != nullptr && callbackData->pMessage != nullptr
                                    ? callbackData->pMessage
                                    : "no message";
    const std::uint32_t objectCount = callbackData != nullptr ? callbackData->objectCount : 0U;

    output += "messageId=";
    output += idName;
    output += " numericCode=";
    output += std::to_string(idNumber);
    output += " objects=";
    output += std::to_string(objectCount);

    if (callbackData != nullptr && callbackData->objectCount > 0U &&
        callbackData->pObjects != nullptr && callbackData->pObjects[0].pObjectName != nullptr)
    {
        output += " firstObject=";
        output += callbackData->pObjects[0].pObjectName;
    }

    output += ": ";
    output += message;
}

std::once_flag g_volkInitializeOnce;
VkResult g_volkInitializeResult{VK_ERROR_INITIALIZATION_FAILED};
std::mutex g_volkTableLoadMutex;

[[nodiscard]] VkResult VolkInitialize() noexcept
{
    try
    {
        std::call_once(g_volkInitializeOnce,
                       []() noexcept
                       {
                           g_volkInitializeResult = volkInitialize();
                       });
    }
    catch (...)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return g_volkInitializeResult;
}

void VolkLoadInstanceTable(VolkInstanceTable* table, VkInstance instance) noexcept
{
    const std::scoped_lock lock{g_volkTableLoadMutex};
    volkLoadInstanceTable(table, instance);
}

void VolkLoadDeviceTable(VolkDeviceTable* table, PFN_vkGetDeviceProcAddr getDeviceProcAddr,
                         VkDevice device) noexcept
{
    const std::scoped_lock lock{g_volkTableLoadMutex};
    vkGetDeviceProcAddr = getDeviceProcAddr;
    volkLoadDeviceTable(table, device);
}

[[nodiscard]] VkResult VolkEnumerateInstanceVersion(std::uint32_t* version) noexcept
{
    if (vkEnumerateInstanceVersion == nullptr)
    {
        *version = VK_API_VERSION_1_0;
        return VK_SUCCESS;
    }

    return vkEnumerateInstanceVersion(version);
}

[[nodiscard]] VkResult VolkEnumerateInstanceExtensionProperties(
    const char* layerName, std::uint32_t* propertyCount, VkExtensionProperties* properties) noexcept
{
    return vkEnumerateInstanceExtensionProperties(layerName, propertyCount, properties);
}

[[nodiscard]] VkResult VolkEnumerateInstanceLayerProperties(std::uint32_t* propertyCount,
                                                            VkLayerProperties* properties) noexcept
{
    return vkEnumerateInstanceLayerProperties(propertyCount, properties);
}

[[nodiscard]] VkResult VolkCreateInstance(const VkInstanceCreateInfo* createInfo,
                                          const VkAllocationCallbacks* allocator,
                                          VkInstance* instance) noexcept
{
    return vkCreateInstance(createInfo, allocator, instance);
}

void VolkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* allocator) noexcept
{
    vkDestroyInstance(instance, allocator);
}

[[nodiscard]] VkResult VolkCreateDebugUtilsMessenger(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* messenger) noexcept
{
    return vkCreateDebugUtilsMessengerEXT(instance, createInfo, allocator, messenger);
}

void VolkDestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                    const VkAllocationCallbacks* allocator) noexcept
{
    vkDestroyDebugUtilsMessengerEXT(instance, messenger, allocator);
}
[[nodiscard]] VkResult VolkCreateWin32Surface(VkInstance instance, const void* createInfo,
                                              const VkAllocationCallbacks* allocator,
                                              VkSurfaceKHR* surface) noexcept
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    const auto function = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
        vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR"));
    if (function == nullptr)
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    return function(instance, static_cast<const VkWin32SurfaceCreateInfoKHR*>(createInfo),
                    allocator, surface);
#else
    (void)instance;
    (void)createInfo;
    (void)allocator;
    (void)surface;
    return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif
}

[[nodiscard]] VkResult VolkCreateX11Surface(VkInstance instance, const void* createInfo,
                                            const VkAllocationCallbacks* allocator,
                                            VkSurfaceKHR* surface) noexcept
{
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    const auto function = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
        vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR"));
    if (function == nullptr)
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    return function(instance, static_cast<const VkXlibSurfaceCreateInfoKHR*>(createInfo), allocator,
                    surface);
#else
    (void)instance;
    (void)createInfo;
    (void)allocator;
    (void)surface;
    return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif
}

[[nodiscard]] VkResult VolkCreateWaylandSurface(VkInstance instance, const void* createInfo,
                                                const VkAllocationCallbacks* allocator,
                                                VkSurfaceKHR* surface) noexcept
{
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    const auto function = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
        vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR"));
    if (function == nullptr)
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    return function(instance, static_cast<const VkWaylandSurfaceCreateInfoKHR*>(createInfo),
                    allocator, surface);
#else
    (void)instance;
    (void)createInfo;
    (void)allocator;
    (void)surface;
    return VK_ERROR_EXTENSION_NOT_PRESENT;
#endif
}

void VolkDestroySurface(VkInstance instance, VkSurfaceKHR surface,
                        const VkAllocationCallbacks* allocator) noexcept
{
    const auto function = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
        vkGetInstanceProcAddr(instance, "vkDestroySurfaceKHR"));
    if (function != nullptr)
    {
        function(instance, surface, allocator);
    }
}

[[nodiscard]] VkResult VolkEnumeratePhysicalDevices(VkInstance instance,
                                                    std::uint32_t* physicalDeviceCount,
                                                    VkPhysicalDevice* physicalDevices) noexcept
{
    return vkEnumeratePhysicalDevices(instance, physicalDeviceCount, physicalDevices);
}

void VolkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                      VkPhysicalDeviceProperties2* properties) noexcept
{
    vkGetPhysicalDeviceProperties2(physicalDevice, properties);
}

void VolkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* memoryProperties) noexcept
{
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, memoryProperties);
}

void VolkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice,
                                                std::uint32_t* propertyCount,
                                                VkQueueFamilyProperties* properties) noexcept
{
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, propertyCount, properties);
}

[[nodiscard]] VkResult VolkGetPhysicalDeviceSurfaceSupport(VkPhysicalDevice physicalDevice,
                                                           std::uint32_t queueFamilyIndex,
                                                           VkSurfaceKHR surface,
                                                           VkBool32* supported) noexcept
{
    return vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface,
                                                supported);
}

[[nodiscard]] VkResult VolkGetPhysicalDeviceSurfaceFormats(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::uint32_t* surfaceFormatCount,
    VkSurfaceFormatKHR* surfaceFormats) noexcept
{
    return vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, surfaceFormatCount,
                                                surfaceFormats);
}

[[nodiscard]] VkResult VolkGetPhysicalDeviceSurfacePresentModes(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::uint32_t* presentModeCount,
    VkPresentModeKHR* presentModes) noexcept
{
    return vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, presentModeCount,
                                                     presentModes);
}

[[nodiscard]] VkResult VolkGetPhysicalDeviceSurfaceCapabilities(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    VkSurfaceCapabilitiesKHR* capabilities) noexcept
{
    return vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, capabilities);
}

[[nodiscard]] VkResult VolkEnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice, const char* layerName, std::uint32_t* propertyCount,
    VkExtensionProperties* properties) noexcept
{
    return vkEnumerateDeviceExtensionProperties(physicalDevice, layerName, propertyCount,
                                                properties);
}

void VolkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice,
                                    VkPhysicalDeviceFeatures2* features) noexcept
{
    vkGetPhysicalDeviceFeatures2(physicalDevice, features);
}

[[nodiscard]] VkResult VolkCreateDevice(VkPhysicalDevice physicalDevice,
                                        const VkDeviceCreateInfo* createInfo,
                                        const VkAllocationCallbacks* allocator,
                                        VkDevice* device) noexcept
{
    return vkCreateDevice(physicalDevice, createInfo, allocator, device);
}

void VolkDestroyDevice(VkDevice device, const VkAllocationCallbacks* allocator) noexcept
{
    vkDestroyDevice(device, allocator);
}

void VolkGetDeviceQueue(VkDevice device, std::uint32_t queueFamilyIndex, std::uint32_t queueIndex,
                        VkQueue* queue) noexcept
{
    vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, queue);
}

[[nodiscard]] VkResult VolkDeviceWaitIdle(VkDevice device) noexcept
{
    return vkDeviceWaitIdle(device);
}

[[nodiscard]] VkResult VolkQueueWaitIdle(VkQueue queue) noexcept
{
    return vkQueueWaitIdle(queue);
}

[[nodiscard]] VkResult VolkCreateCommandPool(VkDevice device,
                                             const VkCommandPoolCreateInfo* createInfo,
                                             const VkAllocationCallbacks* allocator,
                                             VkCommandPool* commandPool) noexcept
{
    return vkCreateCommandPool(device, createInfo, allocator, commandPool);
}

void VolkDestroyCommandPool(VkDevice device, VkCommandPool commandPool,
                            const VkAllocationCallbacks* allocator) noexcept
{
    vkDestroyCommandPool(device, commandPool, allocator);
}

[[nodiscard]] VkResult VolkAllocateCommandBuffers(VkDevice device,
                                                  const VkCommandBufferAllocateInfo* allocateInfo,
                                                  VkCommandBuffer* commandBuffers) noexcept
{
    return vkAllocateCommandBuffers(device, allocateInfo, commandBuffers);
}

[[nodiscard]] VkResult VolkResetCommandBuffer(VkCommandBuffer commandBuffer,
                                              VkCommandBufferResetFlags flags) noexcept
{
    return vkResetCommandBuffer(commandBuffer, flags);
}

[[nodiscard]] VkResult VolkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                              const VkCommandBufferBeginInfo* beginInfo) noexcept
{
    return vkBeginCommandBuffer(commandBuffer, beginInfo);
}

[[nodiscard]] VkResult VolkEndCommandBuffer(VkCommandBuffer commandBuffer) noexcept
{
    return vkEndCommandBuffer(commandBuffer);
}

void VolkCmdBeginRenderPass(VkCommandBuffer commandBuffer,
                            const VkRenderPassBeginInfo* renderPassBegin,
                            VkSubpassContents contents) noexcept
{
    vkCmdBeginRenderPass(commandBuffer, renderPassBegin, contents);
}

void VolkCmdEndRenderPass(VkCommandBuffer commandBuffer) noexcept
{
    vkCmdEndRenderPass(commandBuffer);
}

[[nodiscard]] VkResult VolkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* createInfo,
                                           const VkAllocationCallbacks* allocator,
                                           VkSemaphore* semaphore) noexcept
{
    return vkCreateSemaphore(device, createInfo, allocator, semaphore);
}

void VolkDestroySemaphore(VkDevice device, VkSemaphore semaphore,
                          const VkAllocationCallbacks* allocator) noexcept
{
    vkDestroySemaphore(device, semaphore, allocator);
}

[[nodiscard]] VkResult VolkCreateFence(VkDevice device, const VkFenceCreateInfo* createInfo,
                                       const VkAllocationCallbacks* allocator,
                                       VkFence* fence) noexcept
{
    return vkCreateFence(device, createInfo, allocator, fence);
}

void VolkDestroyFence(VkDevice device, VkFence fence,
                      const VkAllocationCallbacks* allocator) noexcept
{
    vkDestroyFence(device, fence, allocator);
}

[[nodiscard]] VkResult VolkWaitForFences(VkDevice device, std::uint32_t fenceCount,
                                         const VkFence* fences, VkBool32 waitAll,
                                         std::uint64_t timeout) noexcept
{
    return vkWaitForFences(device, fenceCount, fences, waitAll, timeout);
}

[[nodiscard]] VkResult VolkResetFences(VkDevice device, std::uint32_t fenceCount,
                                       const VkFence* fences) noexcept
{
    return vkResetFences(device, fenceCount, fences);
}

[[nodiscard]] VkResult VolkAcquireNextImage(VkDevice device, VkSwapchainKHR swapchain,
                                            std::uint64_t timeout, VkSemaphore semaphore,
                                            VkFence fence, std::uint32_t* imageIndex) noexcept
{
    return vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, imageIndex);
}

[[nodiscard]] VkResult VolkQueueSubmit(VkQueue queue, std::uint32_t submitCount,
                                       const VkSubmitInfo* submits, VkFence fence) noexcept
{
    return vkQueueSubmit(queue, submitCount, submits, fence);
}

[[nodiscard]] VkResult VolkQueuePresent(VkQueue queue, const VkPresentInfoKHR* presentInfo) noexcept
{
    return vkQueuePresentKHR(queue, presentInfo);
}

[[nodiscard]] VkResult VolkWaitForPresent(VkDevice device, VkSwapchainKHR swapchain,
                                          std::uint64_t presentId, std::uint64_t timeout) noexcept
{
    return vkWaitForPresentKHR(device, swapchain, presentId, timeout);
}

[[nodiscard]] VkResult VolkSetDebugUtilsObjectName(
    VkDevice device, const VkDebugUtilsObjectNameInfoEXT* nameInfo) noexcept
{
    if (vkSetDebugUtilsObjectNameEXT == nullptr)
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    return vkSetDebugUtilsObjectNameEXT(device, nameInfo);
}

void VolkCmdBeginDebugUtilsLabel(VkCommandBuffer commandBuffer,
                                 const VkDebugUtilsLabelEXT* labelInfo) noexcept
{
    if (vkCmdBeginDebugUtilsLabelEXT != nullptr)
    {
        vkCmdBeginDebugUtilsLabelEXT(commandBuffer, labelInfo);
    }
}

void VolkCmdEndDebugUtilsLabel(VkCommandBuffer commandBuffer) noexcept
{
    if (vkCmdEndDebugUtilsLabelEXT != nullptr)
    {
        vkCmdEndDebugUtilsLabelEXT(commandBuffer);
    }
}

[[nodiscard]] VkResult VolkCreateSwapchain(VkDevice device,
                                           const VkSwapchainCreateInfoKHR* createInfo,
                                           const VkAllocationCallbacks* allocator,
                                           VkSwapchainKHR* swapchain) noexcept
{
    return vkCreateSwapchainKHR(device, createInfo, allocator, swapchain);
}

void VolkDestroySwapchain(VkDevice device, VkSwapchainKHR swapchain,
                          const VkAllocationCallbacks* allocator) noexcept
{
    vkDestroySwapchainKHR(device, swapchain, allocator);
}

[[nodiscard]] VkResult VolkGetSwapchainImages(VkDevice device, VkSwapchainKHR swapchain,
                                              std::uint32_t* imageCount, VkImage* images) noexcept
{
    return vkGetSwapchainImagesKHR(device, swapchain, imageCount, images);
}

[[nodiscard]] VkResult VolkCreateImageView(VkDevice device, const VkImageViewCreateInfo* createInfo,
                                           const VkAllocationCallbacks* allocator,
                                           VkImageView* imageView) noexcept
{
    return vkCreateImageView(device, createInfo, allocator, imageView);
}

void VolkDestroyImageView(VkDevice device, VkImageView imageView,
                          const VkAllocationCallbacks* allocator) noexcept
{
    vkDestroyImageView(device, imageView, allocator);
}

[[nodiscard]] VkResult VolkCreateRenderPass(VkDevice device,
                                            const VkRenderPassCreateInfo* createInfo,
                                            const VkAllocationCallbacks* allocator,
                                            VkRenderPass* renderPass) noexcept
{
    return vkCreateRenderPass(device, createInfo, allocator, renderPass);
}

void VolkDestroyRenderPass(VkDevice device, VkRenderPass renderPass,
                           const VkAllocationCallbacks* allocator) noexcept
{
    vkDestroyRenderPass(device, renderPass, allocator);
}

[[nodiscard]] VkResult VolkCreateFramebuffer(VkDevice device,
                                             const VkFramebufferCreateInfo* createInfo,
                                             const VkAllocationCallbacks* allocator,
                                             VkFramebuffer* framebuffer) noexcept
{
    return vkCreateFramebuffer(device, createInfo, allocator, framebuffer);
}

void VolkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer,
                            const VkAllocationCallbacks* allocator) noexcept
{
    vkDestroyFramebuffer(device, framebuffer, allocator);
}

[[nodiscard]] VulkanGlobalDispatch BindInstanceDispatch(VulkanGlobalDispatch dispatch,
                                                        const VolkInstanceTable& table) noexcept
{
    if (dispatch.getInstanceProcAddr == nullptr)
    {
        dispatch.getInstanceProcAddr = vkGetInstanceProcAddr;
    }
    dispatch.destroyInstance = table.vkDestroyInstance;
    dispatch.loadInstanceOnly = nullptr;
    dispatch.createDebugUtilsMessenger = table.vkCreateDebugUtilsMessengerEXT;
    dispatch.destroyDebugUtilsMessenger = table.vkDestroyDebugUtilsMessengerEXT;
    dispatch.setDebugUtilsObjectName = table.vkSetDebugUtilsObjectNameEXT;
    dispatch.cmdBeginDebugUtilsLabel = table.vkCmdBeginDebugUtilsLabelEXT;
    dispatch.cmdEndDebugUtilsLabel = table.vkCmdEndDebugUtilsLabelEXT;
    dispatch.destroySurface = table.vkDestroySurfaceKHR;
    dispatch.enumeratePhysicalDevices = table.vkEnumeratePhysicalDevices;
    dispatch.getPhysicalDeviceProperties2 = table.vkGetPhysicalDeviceProperties2;
    dispatch.getPhysicalDeviceFeatures2 = table.vkGetPhysicalDeviceFeatures2;
    dispatch.getPhysicalDeviceMemoryProperties = table.vkGetPhysicalDeviceMemoryProperties;
    dispatch.getPhysicalDeviceQueueFamilyProperties =
        table.vkGetPhysicalDeviceQueueFamilyProperties;
    dispatch.getPhysicalDeviceSurfaceSupport = table.vkGetPhysicalDeviceSurfaceSupportKHR;
    dispatch.getPhysicalDeviceSurfaceFormats = table.vkGetPhysicalDeviceSurfaceFormatsKHR;
    dispatch.getPhysicalDeviceSurfacePresentModes = table.vkGetPhysicalDeviceSurfacePresentModesKHR;
    dispatch.getPhysicalDeviceSurfaceCapabilities = table.vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    dispatch.enumerateDeviceExtensionProperties = table.vkEnumerateDeviceExtensionProperties;
    dispatch.createDevice = table.vkCreateDevice;
    return dispatch;
}

[[nodiscard]] VulkanGlobalDispatch BindDeviceDispatch(VulkanGlobalDispatch dispatch,
                                                      const VolkDeviceTable& table) noexcept
{
    dispatch.destroyDevice = table.vkDestroyDevice;
    dispatch.loadDevice = nullptr;
    dispatch.getDeviceQueue = table.vkGetDeviceQueue;
    dispatch.deviceWaitIdle = table.vkDeviceWaitIdle;
    dispatch.queueWaitIdle = table.vkQueueWaitIdle;
    dispatch.createSwapchain = table.vkCreateSwapchainKHR;
    dispatch.destroySwapchain = table.vkDestroySwapchainKHR;
    dispatch.getSwapchainImages = table.vkGetSwapchainImagesKHR;
    dispatch.createImageView = table.vkCreateImageView;
    dispatch.destroyImageView = table.vkDestroyImageView;
    dispatch.createRenderPass = table.vkCreateRenderPass;
    dispatch.destroyRenderPass = table.vkDestroyRenderPass;
    dispatch.createFramebuffer = table.vkCreateFramebuffer;
    dispatch.destroyFramebuffer = table.vkDestroyFramebuffer;
    dispatch.createCommandPool = table.vkCreateCommandPool;
    dispatch.destroyCommandPool = table.vkDestroyCommandPool;
    dispatch.allocateCommandBuffers = table.vkAllocateCommandBuffers;
    dispatch.resetCommandBuffer = table.vkResetCommandBuffer;
    dispatch.beginCommandBuffer = table.vkBeginCommandBuffer;
    dispatch.endCommandBuffer = table.vkEndCommandBuffer;
    dispatch.cmdBeginRenderPass = table.vkCmdBeginRenderPass;
    dispatch.cmdEndRenderPass = table.vkCmdEndRenderPass;
    dispatch.createSemaphore = table.vkCreateSemaphore;
    dispatch.destroySemaphore = table.vkDestroySemaphore;
    dispatch.createFence = table.vkCreateFence;
    dispatch.destroyFence = table.vkDestroyFence;
    dispatch.waitForFences = table.vkWaitForFences;
    dispatch.resetFences = table.vkResetFences;
    dispatch.acquireNextImage = table.vkAcquireNextImageKHR;
    dispatch.queueSubmit = table.vkQueueSubmit;
    dispatch.queuePresent = table.vkQueuePresentKHR;
    dispatch.waitForPresent = table.vkWaitForPresentKHR;
    return dispatch;
}
[[nodiscard]] VkResult CreateOwnerLocalVmaAllocator(VkInstance instance,
                                                    VkPhysicalDevice physicalDevice,
                                                    VkDevice device, std::uint32_t apiVersion,
                                                    PFN_vkGetInstanceProcAddr getInstanceProcAddr,
                                                    PFN_vkGetDeviceProcAddr getDeviceProcAddr,
                                                    void** allocator) noexcept
{
    if (allocator == nullptr || getInstanceProcAddr == nullptr || getDeviceProcAddr == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr = getInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = getDeviceProcAddr;

    VmaAllocatorCreateInfo createInfo{};
    createInfo.physicalDevice = physicalDevice;
    createInfo.device = device;
    createInfo.instance = instance;
    createInfo.vulkanApiVersion = std::min(apiVersion, VK_API_VERSION_1_2);
    createInfo.pVulkanFunctions = &vulkanFunctions;

    VmaAllocator createdAllocator{};
    const VkResult result = vmaCreateAllocator(&createInfo, &createdAllocator);
    *allocator = result == VK_SUCCESS ? static_cast<void*>(createdAllocator) : nullptr;
    return result;
}

void DestroyVmaAllocator(void* allocator) noexcept
{
    if (allocator != nullptr)
    {
        vmaDestroyAllocator(static_cast<VmaAllocator>(allocator));
    }
}
struct VulkanAdapterCandidate final
{
    VkPhysicalDevice device{VK_NULL_HANDLE};
    RenderAdapterSnapshot snapshot{};
    std::vector<VulkanQueueFamilySnapshot> queueFamilies{};
    std::vector<VkSurfaceFormatKHR> surfaceFormats{};
    std::vector<VkPresentModeKHR> presentModes{};
    std::vector<std::string> rejectionReasons{};
    bool supportsSwapchain{};
    bool compatible{};
};

[[nodiscard]] RenderErrorCode MapAdapterQueryFailure(VkResult result) noexcept
{
    switch (result)
    {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return RenderErrorCode::OutOfMemory;
    case VK_ERROR_DEVICE_LOST:
        return RenderErrorCode::DeviceLost;
    case VK_ERROR_SURFACE_LOST_KHR:
        return RenderErrorCode::SurfaceLost;
    default:
        return RenderErrorCode::BackendFailure;
    }
}

[[nodiscard]] std::string CopyFixedString(const char* text, std::size_t capacity)
{
    std::size_t count{};
    while (count < capacity && text[count] != '\0')
    {
        ++count;
    }

    return std::string{text, count};
}

[[nodiscard]] RenderAdapterType MapPhysicalDeviceType(VkPhysicalDeviceType type) noexcept
{
    switch (type)
    {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return RenderAdapterType::IntegratedGpu;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return RenderAdapterType::DiscreteGpu;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return RenderAdapterType::VirtualGpu;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return RenderAdapterType::Cpu;
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    default:
        return RenderAdapterType::Unknown;
    }
}

[[nodiscard]] RenderAdapterId MakeRenderAdapterId(
    const VkPhysicalDeviceIDProperties& idProperties) noexcept
{
    RenderAdapterId id{};
    for (std::size_t index = 0; index < id.deviceUuid.size() && index < VK_UUID_SIZE; ++index)
    {
        id.deviceUuid[index] = idProperties.deviceUUID[index];
    }

    for (std::size_t index = 0; index < id.deviceLuid.size() && index < VK_LUID_SIZE; ++index)
    {
        id.deviceLuid[index] = idProperties.deviceLUID[index];
    }

    id.deviceNodeMask = idProperties.deviceNodeMask;
    id.deviceLuidValid = idProperties.deviceLUIDValid == VK_TRUE;
    return id;
}

[[nodiscard]] std::optional<PresentationPolicy> MapPresentMode(
    VkPresentModeKHR presentMode) noexcept
{
    switch (presentMode)
    {
    case VK_PRESENT_MODE_FIFO_KHR:
        return PresentationPolicy::VSync;
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return PresentationPolicy::LowLatencyVSync;
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return PresentationPolicy::Uncapped;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] VkPresentModeKHR ToVkPresentMode(PresentationPolicy policy) noexcept
{
    switch (policy)
    {
    case PresentationPolicy::VSync:
        return VK_PRESENT_MODE_FIFO_KHR;
    case PresentationPolicy::LowLatencyVSync:
        return VK_PRESENT_MODE_MAILBOX_KHR;
    case PresentationPolicy::Uncapped:
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

[[nodiscard]] bool ContainsVkPresentMode(const std::vector<VkPresentModeKHR>& presentModes,
                                         VkPresentModeKHR expected) noexcept
{
    return std::ranges::find(presentModes, expected) != presentModes.end();
}

[[nodiscard]] core::Result<VkSurfaceCapabilitiesKHR> QuerySurfaceCapabilities(
    const VulkanGlobalDispatch& dispatch, VkPhysicalDevice device, VkSurfaceKHR surface)
{
    VkSurfaceCapabilitiesKHR capabilities{};
    const VkResult result =
        dispatch.getPhysicalDeviceSurfaceCapabilities(device, surface, &capabilities);
    if (result != VK_SUCCESS)
    {
        return core::Result<VkSurfaceCapabilitiesKHR>::FromError(MakeVulkanError(
            MapAdapterQueryFailure(result), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR", result));
    }

    return core::Result<VkSurfaceCapabilitiesKHR>::FromValue(capabilities);
}

[[nodiscard]] core::Result<std::vector<VkSurfaceFormatKHR>> QuerySurfaceFormats(
    const VulkanGlobalDispatch& dispatch, VkPhysicalDevice device, VkSurfaceKHR surface)
{
    for (;;)
    {
        std::uint32_t formatCount{};
        VkResult result =
            dispatch.getPhysicalDeviceSurfaceFormats(device, surface, &formatCount, nullptr);
        if (result != VK_SUCCESS)
        {
            return core::Result<std::vector<VkSurfaceFormatKHR>>::FromError(
                MakeVulkanError(MapAdapterQueryFailure(result),
                                "vkGetPhysicalDeviceSurfaceFormatsKHR.count", result));
        }

        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        if (formatCount == 0U)
        {
            return core::Result<std::vector<VkSurfaceFormatKHR>>::FromValue(std::move(formats));
        }

        result =
            dispatch.getPhysicalDeviceSurfaceFormats(device, surface, &formatCount, formats.data());
        if (result == VK_INCOMPLETE)
        {
            continue;
        }
        if (result != VK_SUCCESS)
        {
            return core::Result<std::vector<VkSurfaceFormatKHR>>::FromError(
                MakeVulkanError(MapAdapterQueryFailure(result),
                                "vkGetPhysicalDeviceSurfaceFormatsKHR.read", result));
        }

        formats.resize(formatCount);
        return core::Result<std::vector<VkSurfaceFormatKHR>>::FromValue(std::move(formats));
    }
}

[[nodiscard]] core::Result<std::vector<VkPresentModeKHR>> QueryPresentModes(
    const VulkanGlobalDispatch& dispatch, VkPhysicalDevice device, VkSurfaceKHR surface)
{
    for (;;)
    {
        std::uint32_t presentModeCount{};
        VkResult result = dispatch.getPhysicalDeviceSurfacePresentModes(device, surface,
                                                                        &presentModeCount, nullptr);
        if (result != VK_SUCCESS)
        {
            return core::Result<std::vector<VkPresentModeKHR>>::FromError(
                MakeVulkanError(MapAdapterQueryFailure(result),
                                "vkGetPhysicalDeviceSurfacePresentModesKHR.count", result));
        }

        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        if (presentModeCount == 0U)
        {
            return core::Result<std::vector<VkPresentModeKHR>>::FromValue(std::move(presentModes));
        }

        result = dispatch.getPhysicalDeviceSurfacePresentModes(device, surface, &presentModeCount,
                                                               presentModes.data());
        if (result == VK_INCOMPLETE)
        {
            continue;
        }
        if (result != VK_SUCCESS)
        {
            return core::Result<std::vector<VkPresentModeKHR>>::FromError(
                MakeVulkanError(MapAdapterQueryFailure(result),
                                "vkGetPhysicalDeviceSurfacePresentModesKHR.read", result));
        }

        presentModes.resize(presentModeCount);
        return core::Result<std::vector<VkPresentModeKHR>>::FromValue(std::move(presentModes));
    }
}

[[nodiscard]] constexpr bool IsRequiredSdrSrgbSurfaceFormat(VkFormat format,
                                                            VkColorSpaceKHR colorSpace) noexcept
{
    return colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
           (format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB);
}

[[nodiscard]] std::optional<VkSurfaceFormatKHR> SelectSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats) noexcept
{
    constexpr VkFormat preferredFormats[] = {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB};
    for (const VkFormat preferredFormat : preferredFormats)
    {
        const auto format = std::ranges::find_if(
            formats,
            [preferredFormat](VkSurfaceFormatKHR candidate) noexcept
            {
                return candidate.format == preferredFormat &&
                       IsRequiredSdrSrgbSurfaceFormat(candidate.format, candidate.colorSpace);
            });
        if (format != formats.end())
        {
            return *format;
        }
    }

    return std::nullopt;
}

struct VulkanSurfaceOutputSupport final
{
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats{};
};

[[nodiscard]] core::Result<VulkanSurfaceOutputSupport> QuerySurfaceOutputSupport(
    const VulkanGlobalDispatch& dispatch, VkPhysicalDevice device, VkSurfaceKHR surface)
{
    core::Result<VkSurfaceCapabilitiesKHR> capabilities =
        QuerySurfaceCapabilities(dispatch, device, surface);
    if (!capabilities)
    {
        return core::Result<VulkanSurfaceOutputSupport>::FromError(
            std::move(capabilities).GetError());
    }

    core::Result<std::vector<VkSurfaceFormatKHR>> formats =
        QuerySurfaceFormats(dispatch, device, surface);
    if (!formats)
    {
        return core::Result<VulkanSurfaceOutputSupport>::FromError(std::move(formats).GetError());
    }

    return core::Result<VulkanSurfaceOutputSupport>::FromValue(VulkanSurfaceOutputSupport{
        .capabilities = capabilities.GetValue(), .formats = std::move(formats).GetValue()});
}

[[nodiscard]] core::VoidResult ValidateRequiredSdrSrgbOutput(
    const VulkanSurfaceOutputSupport& support)
{
    if (support.formats.empty())
    {
        return core::VoidResult::FromError(
            MakeRenderError(RenderErrorCode::UnsupportedSurface,
                            "Vulkan surface reported no supported swapchain formats."));
    }

    if (!SelectSurfaceFormat(support.formats).has_value())
    {
        return core::VoidResult::FromError(MakeRenderError(
            RenderErrorCode::UnsupportedSurface,
            "Vulkan surface does not support the required SDR sRGB presentation format."));
    }

    if ((support.capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) == 0U)
    {
        return core::VoidResult::FromError(
            MakeRenderError(RenderErrorCode::UnsupportedSurface,
                            "Vulkan surface does not support required opaque composition."));
    }

    if ((support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0U)
    {
        return core::VoidResult::FromError(
            MakeRenderError(RenderErrorCode::UnsupportedSurface,
                            "Vulkan surface does not support color-attachment swapchain images."));
    }

    return core::VoidResult::Success();
}

[[nodiscard]] core::Result<VkPresentModeKHR> SelectPresentMode(
    PresentationPolicyRequest request, const std::vector<VkPresentModeKHR>& presentModes,
    PresentationPolicyFallbackReason& fallbackReason)
{
    fallbackReason = PresentationPolicyFallbackReason::None;
    const auto requireMode =
        [&presentModes](VkPresentModeKHR mode) -> core::Result<VkPresentModeKHR>
    {
        if (!ContainsVkPresentMode(presentModes, mode))
        {
            return core::Result<VkPresentModeKHR>::FromError(MakeRenderError(
                RenderErrorCode::UnsupportedSurface,
                "The required presentation policy is unavailable for this target."));
        }

        return core::Result<VkPresentModeKHR>::FromValue(mode);
    };

    const auto selectFallback =
        [&fallbackReason](VkPresentModeKHR mode) -> core::Result<VkPresentModeKHR>
    {
        fallbackReason = PresentationPolicyFallbackReason::UnavailableForTarget;
        return core::Result<VkPresentModeKHR>::FromValue(mode);
    };

    switch (request.policy)
    {
    case PresentationPolicy::VSync:
        return requireMode(VK_PRESENT_MODE_FIFO_KHR);
    case PresentationPolicy::LowLatencyVSync:
        if (ContainsVkPresentMode(presentModes, VK_PRESENT_MODE_MAILBOX_KHR))
        {
            return core::Result<VkPresentModeKHR>::FromValue(VK_PRESENT_MODE_MAILBOX_KHR);
        }
        if (request.strength == RequirementStrength::Required)
        {
            return requireMode(VK_PRESENT_MODE_MAILBOX_KHR);
        }
        if (!ContainsVkPresentMode(presentModes, VK_PRESENT_MODE_FIFO_KHR))
        {
            return requireMode(VK_PRESENT_MODE_FIFO_KHR);
        }
        return selectFallback(VK_PRESENT_MODE_FIFO_KHR);
    case PresentationPolicy::Uncapped:
        if (ContainsVkPresentMode(presentModes, VK_PRESENT_MODE_IMMEDIATE_KHR))
        {
            return core::Result<VkPresentModeKHR>::FromValue(VK_PRESENT_MODE_IMMEDIATE_KHR);
        }
        if (request.strength == RequirementStrength::Required)
        {
            return requireMode(VK_PRESENT_MODE_IMMEDIATE_KHR);
        }
        if (ContainsVkPresentMode(presentModes, VK_PRESENT_MODE_MAILBOX_KHR))
        {
            return selectFallback(VK_PRESENT_MODE_MAILBOX_KHR);
        }
        if (!ContainsVkPresentMode(presentModes, VK_PRESENT_MODE_FIFO_KHR))
        {
            return requireMode(VK_PRESENT_MODE_FIFO_KHR);
        }
        return selectFallback(VK_PRESENT_MODE_FIFO_KHR);
    }

    return core::Result<VkPresentModeKHR>::FromError(
        MakeRenderError(RenderErrorCode::InvalidArgument, "Presentation policy is invalid."));
}

[[nodiscard]] VkExtent2D SelectSurfaceExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                             platform::PixelSize requestedExtent) noexcept
{
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    return VkExtent2D{.width = std::clamp(requestedExtent.width, capabilities.minImageExtent.width,
                                          capabilities.maxImageExtent.width),
                      .height =
                          std::clamp(requestedExtent.height, capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height)};
}

[[nodiscard]] std::uint32_t SelectSwapchainImageCount(
    const VkSurfaceCapabilitiesKHR& capabilities, QueuedFrameLatencyRequest request,
    QueuedFrameLatency& selectedLatency, QueuedFrameLatencyFallbackReason& fallbackReason)
{
    const QueuedFrameLatency requestedLatency = request.maximumQueuedFrames;
    std::uint32_t preferredImageCount =
        std::max(capabilities.minImageCount, requestedLatency.frameCount + 1U);
    if (capabilities.maxImageCount > 0U)
    {
        preferredImageCount = std::min(preferredImageCount, capabilities.maxImageCount);
    }

    selectedLatency = requestedLatency;
    if (capabilities.maxImageCount > 0U && selectedLatency.frameCount > preferredImageCount)
    {
        selectedLatency.frameCount = preferredImageCount;
    }

    fallbackReason = selectedLatency == requestedLatency
                         ? QueuedFrameLatencyFallbackReason::None
                         : QueuedFrameLatencyFallbackReason::TargetMaximumExceeded;
    return preferredImageCount;
}

[[nodiscard]] core::Result<std::vector<VkPhysicalDevice>> EnumeratePhysicalDevices(
    const VulkanGlobalDispatch& dispatch, VkInstance instance)
{
    std::uint32_t deviceCount{};
    VkResult result = dispatch.enumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS)
    {
        return core::Result<std::vector<VkPhysicalDevice>>::FromError(MakeVulkanError(
            MapAdapterQueryFailure(result), "vkEnumeratePhysicalDevices.count", result));
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    if (deviceCount == 0U)
    {
        return core::Result<std::vector<VkPhysicalDevice>>::FromValue(std::move(devices));
    }

    result = dispatch.enumeratePhysicalDevices(instance, &deviceCount, devices.data());
    if (result != VK_SUCCESS && result != VK_INCOMPLETE)
    {
        return core::Result<std::vector<VkPhysicalDevice>>::FromError(MakeVulkanError(
            MapAdapterQueryFailure(result), "vkEnumeratePhysicalDevices.read", result));
    }

    devices.resize(deviceCount);
    return core::Result<std::vector<VkPhysicalDevice>>::FromValue(std::move(devices));
}

[[nodiscard]] core::Result<std::vector<VkExtensionProperties>> EnumerateDeviceExtensions(
    const VulkanGlobalDispatch& dispatch, VkPhysicalDevice device)
{
    std::uint32_t extensionCount{};
    VkResult result =
        dispatch.enumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    if (result != VK_SUCCESS)
    {
        return core::Result<std::vector<VkExtensionProperties>>::FromError(MakeVulkanError(
            MapAdapterQueryFailure(result), "vkEnumerateDeviceExtensionProperties.count", result));
    }

    std::vector<VkExtensionProperties> extensions(extensionCount);
    if (extensionCount == 0U)
    {
        return core::Result<std::vector<VkExtensionProperties>>::FromValue(std::move(extensions));
    }

    result = dispatch.enumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                                         extensions.data());
    if (result != VK_SUCCESS && result != VK_INCOMPLETE)
    {
        return core::Result<std::vector<VkExtensionProperties>>::FromError(MakeVulkanError(
            MapAdapterQueryFailure(result), "vkEnumerateDeviceExtensionProperties.read", result));
    }

    extensions.resize(extensionCount);
    return core::Result<std::vector<VkExtensionProperties>>::FromValue(std::move(extensions));
}

[[nodiscard]] std::vector<RenderAdapterMemoryHeap> MakeMemoryHeapSnapshots(
    const VkPhysicalDeviceMemoryProperties& memoryProperties)
{
    std::vector<RenderAdapterMemoryHeap> heaps;
    heaps.reserve(memoryProperties.memoryHeapCount);
    for (std::uint32_t index = 0; index < memoryProperties.memoryHeapCount; ++index)
    {
        const VkMemoryHeap& heap = memoryProperties.memoryHeaps[index];
        if (heap.size == 0U)
        {
            continue;
        }

        heaps.push_back(RenderAdapterMemoryHeap{
            .sizeBytes = static_cast<std::uint64_t>(heap.size),
            .deviceLocal = (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0});
    }

    return heaps;
}

[[nodiscard]] core::Result<std::vector<VulkanQueueFamilySnapshot>> MakeQueueFamilySnapshots(
    const VulkanGlobalDispatch& dispatch, VkPhysicalDevice device, VkSurfaceKHR surface)
{
    std::uint32_t queueFamilyCount{};
    dispatch.getPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueProperties(queueFamilyCount);
    if (queueFamilyCount > 0U)
    {
        dispatch.getPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                                        queueProperties.data());
        queueProperties.resize(queueFamilyCount);
    }

    std::vector<VulkanQueueFamilySnapshot> queueFamilies;
    queueFamilies.reserve(queueProperties.size());
    for (std::uint32_t index = 0; index < queueProperties.size(); ++index)
    {
        VkBool32 presentationSupported{VK_FALSE};
        const VkResult result = dispatch.getPhysicalDeviceSurfaceSupport(device, index, surface,
                                                                         &presentationSupported);
        if (result != VK_SUCCESS)
        {
            return core::Result<std::vector<VulkanQueueFamilySnapshot>>::FromError(MakeVulkanError(
                MapAdapterQueryFailure(result), "vkGetPhysicalDeviceSurfaceSupportKHR", result));
        }

        queueFamilies.push_back(VulkanQueueFamilySnapshot{
            .familyIndex = index,
            .queueCount = queueProperties[index].queueCount,
            .supportsGraphics = (queueProperties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0,
            .supportsPresentation = presentationSupported == VK_TRUE});
    }

    return core::Result<std::vector<VulkanQueueFamilySnapshot>>::FromValue(
        std::move(queueFamilies));
}

[[nodiscard]] bool HasGraphicsQueue(
    const std::vector<VulkanQueueFamilySnapshot>& queueFamilies) noexcept
{
    return std::ranges::any_of(queueFamilies,
                               [](const VulkanQueueFamilySnapshot& queueFamily) noexcept
                               {
                                   return queueFamily.queueCount > 0U &&
                                          queueFamily.supportsGraphics;
                               });
}

[[nodiscard]] bool HasPresentationQueue(
    const std::vector<VulkanQueueFamilySnapshot>& queueFamilies) noexcept
{
    return std::ranges::any_of(queueFamilies,
                               [](const VulkanQueueFamilySnapshot& queueFamily) noexcept
                               {
                                   return queueFamily.queueCount > 0U &&
                                          queueFamily.supportsPresentation;
                               });
}

[[nodiscard]] std::vector<PresentationPolicy> MakeSupportedPresentationPolicies(
    const std::vector<VkPresentModeKHR>& presentModes)
{
    std::vector<PresentationPolicy> policies;
    constexpr std::array orderedPolicies{PresentationPolicy::VSync,
                                         PresentationPolicy::LowLatencyVSync,
                                         PresentationPolicy::Uncapped};
    policies.reserve(orderedPolicies.size());
    for (const PresentationPolicy policy : orderedPolicies)
    {
        if (ContainsVkPresentMode(presentModes, ToVkPresentMode(policy)))
        {
            policies.push_back(policy);
        }
    }
    return policies;
}

[[nodiscard]] QueuedFrameLatencyRange MakeQueuedLatencyRange(
    const VkSurfaceCapabilitiesKHR& capabilities) noexcept
{
    const std::uint32_t maximum =
        capabilities.maxImageCount == 0U
            ? QueuedFrameLatency::kMaximumFrames
            : std::clamp(capabilities.maxImageCount, QueuedFrameLatency::kMinimumFrames,
                         QueuedFrameLatency::kMaximumFrames);
    return QueuedFrameLatencyRange{.minimum =
                                       QueuedFrameLatency{QueuedFrameLatency::kMinimumFrames},
                                   .maximum = QueuedFrameLatency{maximum}};
}

[[nodiscard]] bool IsExplicitlyRequested(const RenderAdapterSelectionDesc& desc,
                                         const RenderAdapterId& adapterId) noexcept
{
    return desc.explicitAdapterId.has_value() && *desc.explicitAdapterId == adapterId;
}

[[nodiscard]] std::uint32_t GetAdapterPreferenceRank(RenderAdapterType type,
                                                     RenderAdapterPreference preference) noexcept
{
    switch (preference)
    {
    case RenderAdapterPreference::LowPower:
        switch (type)
        {
        case RenderAdapterType::IntegratedGpu:
            return 0U;
        case RenderAdapterType::DiscreteGpu:
            return 1U;
        case RenderAdapterType::VirtualGpu:
            return 2U;
        case RenderAdapterType::Unknown:
            return 3U;
        case RenderAdapterType::Cpu:
            return 4U;
        }
        break;
    case RenderAdapterPreference::Software:
        return type == RenderAdapterType::Cpu ? 0U : 1U;
    case RenderAdapterPreference::Default:
    case RenderAdapterPreference::HighPerformance:
        switch (type)
        {
        case RenderAdapterType::DiscreteGpu:
            return 0U;
        case RenderAdapterType::IntegratedGpu:
            return 1U;
        case RenderAdapterType::VirtualGpu:
            return 2U;
        case RenderAdapterType::Unknown:
            return 3U;
        case RenderAdapterType::Cpu:
            return 4U;
        }
        break;
    }

    return 5U;
}

[[nodiscard]] bool IsStableAdapterLess(const RenderAdapterSnapshot& lhs,
                                       const RenderAdapterSnapshot& rhs) noexcept
{
    if (lhs.identity.vendorId != rhs.identity.vendorId)
    {
        return lhs.identity.vendorId < rhs.identity.vendorId;
    }
    if (lhs.identity.deviceId != rhs.identity.deviceId)
    {
        return lhs.identity.deviceId < rhs.identity.deviceId;
    }
    if (lhs.identity.name != rhs.identity.name)
    {
        return lhs.identity.name < rhs.identity.name;
    }
    if (lhs.adapterId.deviceUuid != rhs.adapterId.deviceUuid)
    {
        return lhs.adapterId.deviceUuid < rhs.adapterId.deviceUuid;
    }
    if (lhs.adapterId.deviceLuidValid != rhs.adapterId.deviceLuidValid)
    {
        return lhs.adapterId.deviceLuidValid < rhs.adapterId.deviceLuidValid;
    }
    if (lhs.adapterId.deviceLuid != rhs.adapterId.deviceLuid)
    {
        return lhs.adapterId.deviceLuid < rhs.adapterId.deviceLuid;
    }

    return lhs.adapterId.deviceNodeMask < rhs.adapterId.deviceNodeMask;
}

[[nodiscard]] bool IsPreferredAdapterLess(const RenderAdapterSnapshot& lhs,
                                          const RenderAdapterSnapshot& rhs,
                                          RenderAdapterPreference preference) noexcept
{
    const std::uint32_t lhsRank = GetAdapterPreferenceRank(lhs.identity.adapterType, preference);
    const std::uint32_t rhsRank = GetAdapterPreferenceRank(rhs.identity.adapterType, preference);
    if (lhsRank != rhsRank)
    {
        return lhsRank < rhsRank;
    }

    return IsStableAdapterLess(lhs, rhs);
}

[[nodiscard]] core::Result<VulkanAdapterCandidate> BuildAdapterCandidate(
    const VulkanGlobalDispatch& dispatch, VkPhysicalDevice device, VkSurfaceKHR surface,
    const RenderAdapterSelectionDesc& desc)
{
    VkPhysicalDeviceDriverProperties driverProperties{};
    driverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;

    VkPhysicalDeviceIDProperties idProperties{};
    idProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
    idProperties.pNext = &driverProperties;

    VkPhysicalDeviceProperties2 properties2{};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &idProperties;
    dispatch.getPhysicalDeviceProperties2(device, &properties2);

    VkPhysicalDeviceMemoryProperties memoryProperties{};
    dispatch.getPhysicalDeviceMemoryProperties(device, &memoryProperties);

    core::Result<std::vector<VulkanQueueFamilySnapshot>> queueFamilies =
        MakeQueueFamilySnapshots(dispatch, device, surface);
    if (!queueFamilies)
    {
        return core::Result<VulkanAdapterCandidate>::FromError(std::move(queueFamilies).GetError());
    }

    core::Result<VkSurfaceCapabilitiesKHR> surfaceCapabilities =
        QuerySurfaceCapabilities(dispatch, device, surface);
    if (!surfaceCapabilities)
    {
        return core::Result<VulkanAdapterCandidate>::FromError(
            std::move(surfaceCapabilities).GetError());
    }

    core::Result<std::vector<VkSurfaceFormatKHR>> surfaceFormats =
        QuerySurfaceFormats(dispatch, device, surface);
    if (!surfaceFormats)
    {
        return core::Result<VulkanAdapterCandidate>::FromError(
            std::move(surfaceFormats).GetError());
    }

    core::Result<std::vector<VkPresentModeKHR>> presentModes =
        QueryPresentModes(dispatch, device, surface);
    if (!presentModes)
    {
        return core::Result<VulkanAdapterCandidate>::FromError(std::move(presentModes).GetError());
    }

    core::Result<std::vector<VkExtensionProperties>> deviceExtensions =
        EnumerateDeviceExtensions(dispatch, device);
    if (!deviceExtensions)
    {
        return core::Result<VulkanAdapterCandidate>::FromError(
            std::move(deviceExtensions).GetError());
    }

    const VkPhysicalDeviceProperties& properties = properties2.properties;
    const bool supportsSwapchain =
        ContainsExtension(deviceExtensions.GetValue(), kSwapchainExtensionName);
    const bool supportsPresentation = HasPresentationQueue(queueFamilies.GetValue());
    const bool supportsRequiredOutput =
        std::ranges::any_of(surfaceFormats.GetValue(),
                            [](const VkSurfaceFormatKHR& format) noexcept
                            {
                                return IsRequiredSdrSrgbSurfaceFormat(format.format,
                                                                      format.colorSpace);
                            }) &&
        (surfaceCapabilities.GetValue().supportedCompositeAlpha &
         VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0U &&
        (surfaceCapabilities.GetValue().supportedUsageFlags &
         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0U;
    std::vector<PresentationPolicy> supportedPolicies =
        MakeSupportedPresentationPolicies(presentModes.GetValue());
    const bool hasVSync =
        std::ranges::find(supportedPolicies, PresentationPolicy::VSync) != supportedPolicies.end();

    VulkanAdapterCandidate candidate{.device = device};
    candidate.snapshot = RenderAdapterSnapshot{
        .adapterId = MakeRenderAdapterId(idProperties),
        .backend = RenderBackendKind::Vulkan,
        .apiVersion = properties.apiVersion,
        .identity =
            RenderAdapterIdentity{
                .vendorId = properties.vendorID,
                .deviceId = properties.deviceID,
                .adapterType = MapPhysicalDeviceType(properties.deviceType),
                .name = CopyFixedString(properties.deviceName, sizeof(properties.deviceName))},
        .driver =
            RenderAdapterDriverIdentity{
                .driverVersion = properties.driverVersion,
                .driverName = CopyFixedString(driverProperties.driverName,
                                              sizeof(driverProperties.driverName)),
                .driverInfo = CopyFixedString(driverProperties.driverInfo,
                                              sizeof(driverProperties.driverInfo))},
        .limits =
            RenderAdapterLimits{.maxImageDimension2D = properties.limits.maxImageDimension2D,
                                .maxFramebufferWidth = properties.limits.maxFramebufferWidth,
                                .maxFramebufferHeight = properties.limits.maxFramebufferHeight,
                                .maxColorAttachments = properties.limits.maxColorAttachments},
        .memoryHeaps = MakeMemoryHeapSnapshots(memoryProperties),
        .presentation = RenderPresentationCapabilities{
            .supportedPolicies = std::move(supportedPolicies),
            .queuedLatency = MakeQueuedLatencyRange(surfaceCapabilities.GetValue()),
            .supportsWindowPresentation =
                supportsSwapchain && HasGraphicsQueue(queueFamilies.GetValue()) &&
                supportsPresentation && supportsRequiredOutput && hasVSync,
            .supportsOpaqueSdrSrgbOutput = supportsRequiredOutput}};
    candidate.queueFamilies = std::move(queueFamilies).GetValue();
    candidate.surfaceFormats = std::move(surfaceFormats).GetValue();
    candidate.presentModes = std::move(presentModes).GetValue();
    candidate.supportsSwapchain = supportsSwapchain;

    if (!pond::render::IsValid(candidate.snapshot.adapterId))
    {
        candidate.rejectionReasons.push_back(
            "Adapter does not expose a stable Vulkan device UUID or LUID.");
    }

    if (desc.explicitAdapterId.has_value() &&
        !IsExplicitlyRequested(desc, candidate.snapshot.adapterId))
    {
        candidate.rejectionReasons.push_back("Adapter does not match the explicit adapter ID.");
    }

    if (candidate.snapshot.apiVersion < GetMinimumVulkanInstanceVersion())
    {
        candidate.rejectionReasons.push_back(
            "Adapter reports Vulkan API " + FormatVulkanVersion(candidate.snapshot.apiVersion) +
            ", below the required Vulkan " +
            FormatVulkanVersion(GetMinimumVulkanInstanceVersion()) + ".");
    }

    if (!HasGraphicsQueue(candidate.queueFamilies))
    {
        candidate.rejectionReasons.push_back("Adapter has no graphics queue family.");
    }

    if (!supportsPresentation)
    {
        candidate.rejectionReasons.push_back(
            "Adapter cannot present to the first prepared surface.");
    }

    if (candidate.surfaceFormats.empty())
    {
        candidate.rejectionReasons.push_back(
            "Adapter exposes no formats for the first prepared surface.");
    }
    else if (!std::ranges::any_of(candidate.surfaceFormats,
                                  [](const VkSurfaceFormatKHR& format) noexcept
                                  {
                                      return IsRequiredSdrSrgbSurfaceFormat(format.format,
                                                                            format.colorSpace);
                                  }))
    {
        candidate.rejectionReasons.push_back(
            "Adapter cannot provide the required SDR sRGB format and color space for the first "
            "prepared surface.");
    }

    if ((surfaceCapabilities.GetValue().supportedCompositeAlpha &
         VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) == 0U)
    {
        candidate.rejectionReasons.push_back(
            "Adapter cannot provide required opaque composition for the first prepared surface.");
    }

    if ((surfaceCapabilities.GetValue().supportedUsageFlags &
         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0U)
    {
        candidate.rejectionReasons.push_back(
            "Adapter cannot use first-surface swapchain images as color attachments.");
    }

    if (!ContainsVkPresentMode(candidate.presentModes, VK_PRESENT_MODE_FIFO_KHR))
    {
        candidate.rejectionReasons.push_back(
            "Adapter does not expose required FIFO presentation for the first prepared surface.");
    }

    if (!candidate.supportsSwapchain)
    {
        candidate.rejectionReasons.push_back(
            std::string{"Adapter does not expose required device extension "} +
            kSwapchainExtensionName + ".");
    }

    if (!desc.explicitAdapterId.has_value())
    {
        if (desc.adapterPreference == RenderAdapterPreference::Software &&
            candidate.snapshot.identity.adapterType != RenderAdapterType::Cpu)
        {
            candidate.rejectionReasons.push_back(
                "Software adapter preference excludes hardware adapters.");
        }
        else if (desc.adapterPreference != RenderAdapterPreference::Software &&
                 candidate.snapshot.identity.adapterType == RenderAdapterType::Cpu)
        {
            candidate.rejectionReasons.push_back(
                "Software adapters require RenderAdapterPreference::Software or an explicit "
                "adapter "
                "ID.");
        }
    }

    candidate.compatible = candidate.rejectionReasons.empty();
    return core::Result<VulkanAdapterCandidate>::FromValue(std::move(candidate));
}

[[nodiscard]] RenderAdapterRejection MakeRejection(const VulkanAdapterCandidate& candidate)
{
    return RenderAdapterRejection{.adapterId = candidate.snapshot.adapterId,
                                  .identity = candidate.snapshot.identity,
                                  .reasons = candidate.rejectionReasons};
}

[[nodiscard]] std::string MakeNoCompatibleAdapterMessage(
    const std::vector<RenderAdapterRejection>& rejections)
{
    std::string message{"No Vulkan adapter satisfied the first-surface requirements."};
    for (const RenderAdapterRejection& rejection : rejections)
    {
        message += " ";
        message += rejection.identity.name.empty() ? "<unnamed adapter>" : rejection.identity.name;
        message += ": ";
        for (std::size_t index = 0; index < rejection.reasons.size(); ++index)
        {
            if (index > 0U)
            {
                message += "; ";
            }
            message += rejection.reasons[index];
        }
        message += ".";
    }

    return message;
}
struct VulkanDeviceFeatureSelection final
{
    VulkanDeviceOptionalCapabilities optionalCapabilities{};
    std::vector<const char*> enabledExtensions{};
};

[[nodiscard]] bool ContainsQueueFamilyIndex(const std::vector<std::uint32_t>& indices,
                                            std::uint32_t expected) noexcept
{
    return std::ranges::find(indices, expected) != indices.end();
}

[[nodiscard]] std::optional<std::uint32_t> FindGraphicsQueueFamily(
    const std::vector<VulkanQueueFamilySnapshot>& queueFamilies) noexcept
{
    for (const VulkanQueueFamilySnapshot& queueFamily : queueFamilies)
    {
        if (queueFamily.queueCount > 0U && queueFamily.supportsGraphics)
        {
            return queueFamily.familyIndex;
        }
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<std::uint32_t> FindPresentationQueueFamily(
    const std::vector<VulkanQueueFamilySnapshot>& queueFamilies,
    std::uint32_t preferredFamilyIndex) noexcept
{
    for (const VulkanQueueFamilySnapshot& queueFamily : queueFamilies)
    {
        if (queueFamily.queueCount > 0U && queueFamily.supportsPresentation &&
            queueFamily.familyIndex == preferredFamilyIndex)
        {
            return queueFamily.familyIndex;
        }
    }

    for (const VulkanQueueFamilySnapshot& queueFamily : queueFamilies)
    {
        if (queueFamily.queueCount > 0U && queueFamily.supportsPresentation)
        {
            return queueFamily.familyIndex;
        }
    }

    return std::nullopt;
}

[[nodiscard]] core::Result<VulkanDeviceQueuePlan> MakeDeviceQueuePlan(
    const std::vector<VulkanQueueFamilySnapshot>& queueFamilies)
{
    const std::optional<std::uint32_t> graphicsFamily = FindGraphicsQueueFamily(queueFamilies);
    if (!graphicsFamily.has_value())
    {
        return core::Result<VulkanDeviceQueuePlan>::FromError(
            MakeRenderError(RenderErrorCode::NoCompatibleAdapter,
                            "Selected Vulkan adapter no longer exposes a graphics queue family."));
    }

    const std::optional<std::uint32_t> presentationFamily =
        FindPresentationQueueFamily(queueFamilies, *graphicsFamily);
    if (!presentationFamily.has_value())
    {
        return core::Result<VulkanDeviceQueuePlan>::FromError(MakeRenderError(
            RenderErrorCode::UnsupportedSurface,
            "Selected Vulkan adapter no longer exposes a presentation queue for the surface."));
    }

    std::vector<std::uint32_t> provisionedFamilies;
    provisionedFamilies.reserve(queueFamilies.size());
    for (const VulkanQueueFamilySnapshot& queueFamily : queueFamilies)
    {
        if (queueFamily.queueCount > 0U &&
            !ContainsQueueFamilyIndex(provisionedFamilies, queueFamily.familyIndex))
        {
            provisionedFamilies.push_back(queueFamily.familyIndex);
        }
    }

    VulkanDeviceQueuePlan plan{.graphicsQueueFamilyIndex = *graphicsFamily,
                               .presentationQueueFamilyIndex = *presentationFamily,
                               .provisionedQueueFamilyIndices = std::move(provisionedFamilies),
                               .usesDistinctPresentationQueue =
                                   *graphicsFamily != *presentationFamily};
    if (!IsValidVulkanDeviceQueuePlan(plan))
    {
        return core::Result<VulkanDeviceQueuePlan>::FromError(
            MakeRenderError(RenderErrorCode::BackendFailure,
                            "Internal Vulkan logical-device queue plan failed validation."));
    }

    return core::Result<VulkanDeviceQueuePlan>::FromValue(std::move(plan));
}

void AppendFeatureStruct(void**& nextLink, void* featureStruct) noexcept
{
    *nextLink = featureStruct;
    nextLink =
        reinterpret_cast<void**>(&reinterpret_cast<VkBaseOutStructure*>(featureStruct)->pNext);
}

[[nodiscard]] VulkanDeviceFeatureSelection SelectOptionalDeviceFeatures(
    const VulkanGlobalDispatch& dispatch, VkPhysicalDevice device,
    const std::vector<VkExtensionProperties>& extensions, bool khrSurfaceMaintenance1Enabled,
    bool extSurfaceMaintenance1Enabled)
{
    const char* swapchainMaintenance1Extension = nullptr;
    if (khrSurfaceMaintenance1Enabled &&
        ContainsExtension(extensions, kKhrSwapchainMaintenance1ExtensionName))
    {
        swapchainMaintenance1Extension = kKhrSwapchainMaintenance1ExtensionName;
    }
    else if (extSurfaceMaintenance1Enabled &&
             ContainsExtension(extensions, kExtSwapchainMaintenance1ExtensionName))
    {
        swapchainMaintenance1Extension = kExtSwapchainMaintenance1ExtensionName;
    }

    const bool hasSwapchainMaintenance1 = swapchainMaintenance1Extension != nullptr;
    const bool hasPresentId = ContainsExtension(extensions, kPresentIdExtensionName);
    const bool hasPresentWait = ContainsExtension(extensions, kPresentWaitExtensionName);

    VkPhysicalDeviceFeatures2 availableFeatures{};
    availableFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR maintenanceFeatures{};
    maintenanceFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR;

    VkPhysicalDevicePresentIdFeaturesKHR presentIdFeatures{};
    presentIdFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;

    VkPhysicalDevicePresentWaitFeaturesKHR presentWaitFeatures{};
    presentWaitFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;

    void** queryNext = &availableFeatures.pNext;
    if (hasSwapchainMaintenance1)
    {
        AppendFeatureStruct(queryNext, &maintenanceFeatures);
    }
    if (hasPresentId)
    {
        AppendFeatureStruct(queryNext, &presentIdFeatures);
    }
    if (hasPresentWait)
    {
        AppendFeatureStruct(queryNext, &presentWaitFeatures);
    }

    dispatch.getPhysicalDeviceFeatures2(device, &availableFeatures);

    VulkanDeviceFeatureSelection selection;
    selection.enabledExtensions.push_back(kSwapchainExtensionName);

    if (hasSwapchainMaintenance1 && maintenanceFeatures.swapchainMaintenance1 == VK_TRUE)
    {
        selection.optionalCapabilities.swapchainMaintenance1 = true;
        selection.enabledExtensions.push_back(swapchainMaintenance1Extension);
    }

    const bool canEnablePresentCompletion = hasPresentId && hasPresentWait &&
                                            presentIdFeatures.presentId == VK_TRUE &&
                                            presentWaitFeatures.presentWait == VK_TRUE;
    if (canEnablePresentCompletion)
    {
        selection.optionalCapabilities.presentId = true;
        selection.optionalCapabilities.presentWait = true;
        selection.enabledExtensions.push_back(kPresentIdExtensionName);
        selection.enabledExtensions.push_back(kPresentWaitExtensionName);
    }

    return selection;
}

[[nodiscard]] core::Result<VulkanAdapterCandidate> FindSelectedAdapterCandidate(
    const VulkanGlobalDispatch& dispatch, VkInstance instance, VkSurfaceKHR surface,
    const RenderAdapterSelection& selection)
{
    core::Result<std::vector<VkPhysicalDevice>> physicalDevices =
        EnumeratePhysicalDevices(dispatch, instance);
    if (!physicalDevices)
    {
        return core::Result<VulkanAdapterCandidate>::FromError(
            std::move(physicalDevices).GetError());
    }

    for (VkPhysicalDevice physicalDevice : physicalDevices.GetValue())
    {
        core::Result<VulkanAdapterCandidate> candidate =
            BuildAdapterCandidate(dispatch, physicalDevice, surface, selection.request);
        if (!candidate)
        {
            return core::Result<VulkanAdapterCandidate>::FromError(std::move(candidate).GetError());
        }

        VulkanAdapterCandidate value = std::move(candidate).GetValue();
        if (value.snapshot.adapterId == selection.selectedAdapter.adapterId)
        {
            if (!value.compatible)
            {
                return core::Result<VulkanAdapterCandidate>::FromError(MakeRenderError(
                    RenderErrorCode::UnsupportedSurface,
                    "Selected Vulkan adapter no longer satisfies the first-surface requirements."));
            }

            return core::Result<VulkanAdapterCandidate>::FromValue(std::move(value));
        }
    }

    return core::Result<VulkanAdapterCandidate>::FromError(
        MakeRenderError(RenderErrorCode::NoCompatibleAdapter,
                        "Selected Vulkan adapter was not found during logical-device creation."));
}
} // namespace

bool VulkanValidationMessageFilter::Matches(std::string_view inputName,
                                            std::int32_t inputNumber) const noexcept
{
    return messageIdName == inputName && messageIdNumber == inputNumber;
}

namespace
{
void CaptureValidationMessage(VulkanDebugMessengerContext& context,
                              VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                              const VkDebugUtilsMessengerCallbackDataEXT* callbackData) noexcept
{
    const bool isError = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;
    if (isError)
    {
        context.errorCount.fetch_add(1U, std::memory_order_relaxed);
    }
    else
    {
        context.warningCount.fetch_add(1U, std::memory_order_relaxed);
    }

    const std::size_t index = context.reservedMessageCount.fetch_add(1U, std::memory_order_relaxed);
    if (index >= context.capturedMessages.size())
    {
        context.droppedMessageCount.fetch_add(1U, std::memory_order_relaxed);
        return;
    }

    VulkanDebugMessengerContext::CapturedMessage& captured = context.capturedMessages[index];
    captured.message.severity =
        isError ? RenderValidationMessageSeverity::Error : RenderValidationMessageSeverity::Warning;
    captured.message.messageIdNumber = callbackData != nullptr ? callbackData->messageIdNumber : 0;
    const char* const sourceName =
        callbackData != nullptr && callbackData->pMessageIdName != nullptr
            ? callbackData->pMessageIdName
            : "unknown-message";
    std::size_t characterIndex{};
    for (; characterIndex < RenderValidationMessage::kMaximumMessageIdNameLength &&
           sourceName[characterIndex] != '\0';
         ++characterIndex)
    {
        captured.message.messageIdName[characterIndex] = sourceName[characterIndex];
    }
    captured.message.messageIdName[characterIndex] = '\0';
    captured.ready.store(true, std::memory_order_release);
}
} // namespace

VulkanInstanceOwner::VulkanInstanceOwner() noexcept = default;

VulkanInstanceOwner::VulkanInstanceOwner(
    VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
    VulkanGlobalDispatch::DestroyInstanceFn destroyInstance,
    VulkanGlobalDispatch::DestroyDebugUtilsMessengerFn destroyDebugMessenger,
    VulkanGlobalDispatch dispatch, std::optional<VolkInstanceTable> volkTable,
    VulkanInstanceInfo info, std::unique_ptr<VulkanDebugMessengerContext> debugContext) noexcept
    : m_instance{instance}, m_debugMessenger{debugMessenger}, m_destroyInstance{destroyInstance},
      m_destroyDebugMessenger{destroyDebugMessenger}, m_dispatch{dispatch},
      m_volkTable{std::move(volkTable)}, m_info{info}, m_debugContext{std::move(debugContext)}
{
}

VulkanInstanceOwner::VulkanInstanceOwner(VulkanInstanceOwner&& other) noexcept
    : m_instance{other.m_instance}, m_debugMessenger{other.m_debugMessenger},
      m_destroyInstance{other.m_destroyInstance},
      m_destroyDebugMessenger{other.m_destroyDebugMessenger}, m_dispatch{other.m_dispatch},
      m_volkTable{std::move(other.m_volkTable)}, m_info{other.m_info},
      m_debugContext{std::move(other.m_debugContext)}
{
    other.m_instance = VK_NULL_HANDLE;
    other.m_debugMessenger = VK_NULL_HANDLE;
    other.m_destroyInstance = nullptr;
    other.m_destroyDebugMessenger = nullptr;
    other.m_dispatch = {};
    other.m_volkTable.reset();
    other.m_info = {};
}

VulkanInstanceOwner& VulkanInstanceOwner::operator=(VulkanInstanceOwner&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_instance = other.m_instance;
        m_debugMessenger = other.m_debugMessenger;
        m_destroyInstance = other.m_destroyInstance;
        m_destroyDebugMessenger = other.m_destroyDebugMessenger;
        m_dispatch = other.m_dispatch;
        m_volkTable = std::move(other.m_volkTable);
        m_info = other.m_info;
        m_debugContext = std::move(other.m_debugContext);
        other.m_instance = VK_NULL_HANDLE;
        other.m_debugMessenger = VK_NULL_HANDLE;
        other.m_destroyInstance = nullptr;
        other.m_destroyDebugMessenger = nullptr;
        other.m_dispatch = {};
        other.m_volkTable.reset();
        other.m_info = {};
    }

    return *this;
}

VulkanInstanceOwner::~VulkanInstanceOwner()
{
    Reset();
}

bool VulkanInstanceOwner::IsValid() const noexcept
{
    return m_instance != VK_NULL_HANDLE;
}

VkInstance VulkanInstanceOwner::GetHandle() const noexcept
{
    return m_instance;
}

VulkanInstanceInfo VulkanInstanceOwner::GetInfo() const noexcept
{
    return m_info;
}

RenderValidationReport VulkanInstanceOwner::GetValidationReport() const noexcept
{
    RenderValidationReport report{};
    if (m_debugContext == nullptr)
    {
        return report;
    }

    report.warningCount = m_debugContext->warningCount.load(std::memory_order_relaxed);
    report.errorCount = m_debugContext->errorCount.load(std::memory_order_relaxed);
    report.droppedMessageCount =
        m_debugContext->droppedMessageCount.load(std::memory_order_relaxed);
    const std::size_t reserved =
        std::min(m_debugContext->reservedMessageCount.load(std::memory_order_acquire),
                 m_debugContext->capturedMessages.size());
    for (std::size_t index = 0U; index < reserved; ++index)
    {
        const VulkanDebugMessengerContext::CapturedMessage& captured =
            m_debugContext->capturedMessages[index];
        if (!captured.ready.load(std::memory_order_acquire))
        {
            continue;
        }

        report.capturedMessages[report.capturedMessageCount] = captured.message;
        ++report.capturedMessageCount;
    }
    return report;
}

const VulkanGlobalDispatch& VulkanInstanceOwner::GetDispatch() const noexcept
{
    return m_dispatch;
}

PFN_vkGetDeviceProcAddr VulkanInstanceOwner::GetDeviceProcAddr() const noexcept
{
    return m_volkTable.has_value() ? m_volkTable->vkGetDeviceProcAddr : nullptr;
}

void VulkanInstanceOwner::Reset() noexcept
{
    if (m_debugMessenger != VK_NULL_HANDLE && m_destroyDebugMessenger != nullptr)
    {
        m_destroyDebugMessenger(m_instance, m_debugMessenger, nullptr);
    }

    if (m_instance != VK_NULL_HANDLE && m_destroyInstance != nullptr)
    {
        m_destroyInstance(m_instance, nullptr);
    }

    m_instance = VK_NULL_HANDLE;
    m_debugMessenger = VK_NULL_HANDLE;
    m_destroyInstance = nullptr;
    m_destroyDebugMessenger = nullptr;
    m_dispatch = {};
    m_volkTable.reset();
    m_info = {};
    m_debugContext.reset();
}
VulkanSurfaceOwner::VulkanSurfaceOwner() noexcept = default;

VulkanSurfaceOwner::VulkanSurfaceOwner(std::shared_ptr<VulkanInstanceOwner> instance,
                                       VkSurfaceKHR surface,
                                       VulkanGlobalDispatch::DestroySurfaceFn destroySurface,
                                       VulkanSurfaceInfo info) noexcept
    : m_instance{std::move(instance)}, m_surface{surface}, m_destroySurface{destroySurface},
      m_info{info}
{
}

VulkanSurfaceOwner::VulkanSurfaceOwner(VulkanSurfaceOwner&& other) noexcept
    : m_instance{std::move(other.m_instance)}, m_surface{other.m_surface},
      m_destroySurface{other.m_destroySurface}, m_info{other.m_info}
{
    other.m_surface = VK_NULL_HANDLE;
    other.m_destroySurface = nullptr;
    other.m_info = {};
}

VulkanSurfaceOwner& VulkanSurfaceOwner::operator=(VulkanSurfaceOwner&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_instance = std::move(other.m_instance);
        m_surface = other.m_surface;
        m_destroySurface = other.m_destroySurface;
        m_info = other.m_info;
        other.m_surface = VK_NULL_HANDLE;
        other.m_destroySurface = nullptr;
        other.m_info = {};
    }

    return *this;
}

VulkanSurfaceOwner::~VulkanSurfaceOwner()
{
    Reset();
}

bool VulkanSurfaceOwner::IsValid() const noexcept
{
    return m_surface != VK_NULL_HANDLE && m_instance != nullptr && m_instance->IsValid();
}

VkSurfaceKHR VulkanSurfaceOwner::GetHandle() const noexcept
{
    return m_surface;
}

VulkanSurfaceInfo VulkanSurfaceOwner::GetInfo() const noexcept
{
    return m_info;
}

std::shared_ptr<VulkanInstanceOwner> VulkanSurfaceOwner::GetInstanceOwner() const noexcept
{
    return m_instance;
}

void VulkanSurfaceOwner::Reset() noexcept
{
    if (m_surface != VK_NULL_HANDLE && m_destroySurface != nullptr && m_instance != nullptr &&
        m_instance->IsValid())
    {
        m_destroySurface(m_instance->GetHandle(), m_surface, nullptr);
    }

    m_instance.reset();
    m_surface = VK_NULL_HANDLE;
    m_destroySurface = nullptr;
    m_info = {};
}
VulkanDeviceOwner::VulkanDeviceOwner() noexcept = default;

VulkanDeviceOwner::VulkanDeviceOwner(
    std::shared_ptr<VulkanInstanceOwner> instance, VkPhysicalDevice physicalDevice, VkDevice device,
    void* allocator, std::vector<VkQueue> queues,
    std::unique_ptr<VulkanQueueSynchronization> queueSynchronization,
    VulkanGlobalDispatch::DestroyDeviceFn destroyDevice,
    VulkanGlobalDispatch::DeviceWaitIdleFn deviceWaitIdle,
    VulkanGlobalDispatch::QueueWaitIdleFn queueWaitIdle,
    VulkanGlobalDispatch::DestroyAllocatorFn destroyAllocator, VulkanGlobalDispatch dispatch,
    std::optional<VolkDeviceTable> volkTable, VulkanDeviceInfo info) noexcept
    : m_instance{std::move(instance)}, m_physicalDevice{physicalDevice}, m_device{device},
      m_allocator{allocator}, m_queues{std::move(queues)},
      m_queueSynchronization{std::move(queueSynchronization)}, m_destroyDevice{destroyDevice},
      m_deviceWaitIdle{deviceWaitIdle}, m_queueWaitIdle{queueWaitIdle},
      m_destroyAllocator{destroyAllocator}, m_dispatch{dispatch}, m_volkTable{std::move(volkTable)},
      m_info{std::move(info)}
{
}

VulkanDeviceOwner::VulkanDeviceOwner(VulkanDeviceOwner&& other) noexcept
    : m_instance{std::move(other.m_instance)}, m_physicalDevice{other.m_physicalDevice},
      m_device{other.m_device}, m_allocator{other.m_allocator}, m_queues{std::move(other.m_queues)},
      m_queueSynchronization{std::move(other.m_queueSynchronization)},
      m_destroyDevice{other.m_destroyDevice}, m_deviceWaitIdle{other.m_deviceWaitIdle},
      m_queueWaitIdle{other.m_queueWaitIdle}, m_destroyAllocator{other.m_destroyAllocator},
      m_dispatch{other.m_dispatch}, m_volkTable{std::move(other.m_volkTable)},
      m_info{std::move(other.m_info)}
{
    other.m_physicalDevice = VK_NULL_HANDLE;
    other.m_device = VK_NULL_HANDLE;
    other.m_allocator = nullptr;
    other.m_destroyDevice = nullptr;
    other.m_deviceWaitIdle = nullptr;
    other.m_queueWaitIdle = nullptr;
    other.m_destroyAllocator = nullptr;
    other.m_dispatch = {};
    other.m_volkTable.reset();
    other.m_info = {};
}

VulkanDeviceOwner& VulkanDeviceOwner::operator=(VulkanDeviceOwner&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_instance = std::move(other.m_instance);
        m_physicalDevice = other.m_physicalDevice;
        m_device = other.m_device;
        m_allocator = other.m_allocator;
        m_queues = std::move(other.m_queues);
        m_queueSynchronization = std::move(other.m_queueSynchronization);
        m_destroyDevice = other.m_destroyDevice;
        m_deviceWaitIdle = other.m_deviceWaitIdle;
        m_queueWaitIdle = other.m_queueWaitIdle;
        m_destroyAllocator = other.m_destroyAllocator;
        m_dispatch = other.m_dispatch;
        m_volkTable = std::move(other.m_volkTable);
        m_info = std::move(other.m_info);
        other.m_physicalDevice = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
        other.m_allocator = nullptr;
        other.m_destroyDevice = nullptr;
        other.m_deviceWaitIdle = nullptr;
        other.m_queueWaitIdle = nullptr;
        other.m_destroyAllocator = nullptr;
        other.m_dispatch = {};
        other.m_volkTable.reset();
        other.m_info = {};
    }

    return *this;
}

VulkanDeviceOwner::~VulkanDeviceOwner()
{
    Reset();
}

bool VulkanDeviceOwner::IsValid() const noexcept
{
    return m_device != VK_NULL_HANDLE && m_instance != nullptr && m_instance->IsValid();
}

VkDevice VulkanDeviceOwner::GetHandle() const noexcept
{
    return m_device;
}

VkPhysicalDevice VulkanDeviceOwner::GetPhysicalDevice() const noexcept
{
    return m_physicalDevice;
}

const VulkanDeviceInfo& VulkanDeviceOwner::GetInfo() const noexcept
{
    return m_info;
}

const VulkanGlobalDispatch& VulkanDeviceOwner::GetDispatch() const noexcept
{
    return m_dispatch;
}

VkQueue VulkanDeviceOwner::GetQueue(std::uint32_t queueFamilyIndex) const noexcept
{
    for (std::size_t index = 0U; index < m_info.queuePlan.provisionedQueueFamilyIndices.size();
         ++index)
    {
        if (m_info.queuePlan.provisionedQueueFamilyIndices[index] == queueFamilyIndex &&
            index < m_queues.size())
        {
            return m_queues[index];
        }
    }

    return VK_NULL_HANDLE;
}

VulkanQueueSynchronization::QueueOperationLock VulkanDeviceOwner::LockQueueOperation(
    VkQueue queue) const
{
    PONDER_VERIFY(m_queueSynchronization != nullptr,
                  "A live Vulkan device must own queue synchronization state");
    return m_queueSynchronization->LockQueue(queue);
}

core::VoidResult VulkanDeviceOwner::WaitIdle() const
{
    if (!IsValid() || m_deviceWaitIdle == nullptr || m_queueSynchronization == nullptr)
    {
        return core::VoidResult::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Vulkan device wait-idle requires a live device."));
    }

    [[maybe_unused]] auto deviceLock = m_queueSynchronization->LockDevice();
    const VkResult result = m_deviceWaitIdle(m_device);
    if (result != VK_SUCCESS)
    {
        return core::VoidResult::FromError(
            MakeVulkanError(MapDeviceFailure(result), "vkDeviceWaitIdle", result));
    }

    return core::VoidResult::Success();
}

core::VoidResult VulkanDeviceOwner::WaitQueueIdle(std::uint32_t queueFamilyIndex) const
{
    const VkQueue queue = GetQueue(queueFamilyIndex);
    if (!IsValid() || queue == VK_NULL_HANDLE || m_queueWaitIdle == nullptr ||
        m_queueSynchronization == nullptr)
    {
        return core::VoidResult::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Vulkan queue wait-idle requires a live queue."));
    }

    [[maybe_unused]] auto queueLock = m_queueSynchronization->LockQueue(queue);
    const VkResult result = m_queueWaitIdle(queue);
    if (result != VK_SUCCESS)
    {
        return core::VoidResult::FromError(
            MakeVulkanError(MapDeviceFailure(result), "vkQueueWaitIdle", result));
    }

    return core::VoidResult::Success();
}

void VulkanDeviceOwner::Reset() noexcept
{
    const auto destroyNativeResources = [this]() noexcept
    {
        if (m_allocator != nullptr && m_destroyAllocator != nullptr)
        {
            m_destroyAllocator(m_allocator);
        }

        if (m_device != VK_NULL_HANDLE && m_destroyDevice != nullptr)
        {
            m_destroyDevice(m_device, nullptr);
        }
    };

    if (m_queueSynchronization != nullptr)
    {
        [[maybe_unused]] auto deviceLock = m_queueSynchronization->LockDevice();
        destroyNativeResources();
    }
    else
    {
        destroyNativeResources();
    }

    m_instance.reset();
    m_physicalDevice = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_allocator = nullptr;
    m_queues.clear();
    m_queueSynchronization.reset();
    m_destroyDevice = nullptr;
    m_deviceWaitIdle = nullptr;
    m_queueWaitIdle = nullptr;
    m_destroyAllocator = nullptr;
    m_dispatch = {};
    m_volkTable.reset();
    m_info = {};
}
VulkanSwapchainOwner::VulkanSwapchainOwner() noexcept = default;

VulkanSwapchainOwner::VulkanSwapchainOwner(
    VkDevice device, VkSwapchainKHR swapchain, std::vector<VkImage> images,
    std::vector<VkImageView> imageViews, VkRenderPass renderPass,
    std::vector<VkFramebuffer> framebuffers,
    VulkanGlobalDispatch::DestroySwapchainFn destroySwapchain,
    VulkanGlobalDispatch::DestroyImageViewFn destroyImageView,
    VulkanGlobalDispatch::DestroyRenderPassFn destroyRenderPass,
    VulkanGlobalDispatch::DestroyFramebufferFn destroyFramebuffer,
    VulkanSwapchainConfig config) noexcept
    : m_device{device}, m_swapchain{swapchain}, m_images{std::move(images)},
      m_imageViews{std::move(imageViews)}, m_renderPass{renderPass},
      m_framebuffers{std::move(framebuffers)}, m_destroySwapchain{destroySwapchain},
      m_destroyImageView{destroyImageView}, m_destroyRenderPass{destroyRenderPass},
      m_destroyFramebuffer{destroyFramebuffer}, m_config{std::move(config)}
{
}

VulkanSwapchainOwner::VulkanSwapchainOwner(VulkanSwapchainOwner&& other) noexcept
    : m_device{other.m_device}, m_swapchain{other.m_swapchain}, m_images{std::move(other.m_images)},
      m_imageViews{std::move(other.m_imageViews)}, m_renderPass{other.m_renderPass},
      m_framebuffers{std::move(other.m_framebuffers)}, m_destroySwapchain{other.m_destroySwapchain},
      m_destroyImageView{other.m_destroyImageView}, m_destroyRenderPass{other.m_destroyRenderPass},
      m_destroyFramebuffer{other.m_destroyFramebuffer}, m_config{std::move(other.m_config)}
{
    other.m_device = VK_NULL_HANDLE;
    other.m_swapchain = VK_NULL_HANDLE;
    other.m_renderPass = VK_NULL_HANDLE;
    other.m_destroySwapchain = nullptr;
    other.m_destroyImageView = nullptr;
    other.m_destroyRenderPass = nullptr;
    other.m_destroyFramebuffer = nullptr;
    other.m_config = {};
}

VulkanSwapchainOwner& VulkanSwapchainOwner::operator=(VulkanSwapchainOwner&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_device = other.m_device;
        m_swapchain = other.m_swapchain;
        m_images = std::move(other.m_images);
        m_imageViews = std::move(other.m_imageViews);
        m_renderPass = other.m_renderPass;
        m_framebuffers = std::move(other.m_framebuffers);
        m_destroySwapchain = other.m_destroySwapchain;
        m_destroyImageView = other.m_destroyImageView;
        m_destroyRenderPass = other.m_destroyRenderPass;
        m_destroyFramebuffer = other.m_destroyFramebuffer;
        m_config = std::move(other.m_config);

        other.m_device = VK_NULL_HANDLE;
        other.m_swapchain = VK_NULL_HANDLE;
        other.m_renderPass = VK_NULL_HANDLE;
        other.m_destroySwapchain = nullptr;
        other.m_destroyImageView = nullptr;
        other.m_destroyRenderPass = nullptr;
        other.m_destroyFramebuffer = nullptr;
        other.m_config = {};
    }

    return *this;
}

VulkanSwapchainOwner::~VulkanSwapchainOwner()
{
    Reset();
}

bool VulkanSwapchainOwner::IsValid() const noexcept
{
    return m_device != VK_NULL_HANDLE && m_swapchain != VK_NULL_HANDLE &&
           m_renderPass != VK_NULL_HANDLE && !m_imageViews.empty() &&
           m_imageViews.size() == m_framebuffers.size();
}

VkSwapchainKHR VulkanSwapchainOwner::GetHandle() const noexcept
{
    return m_swapchain;
}

VkRenderPass VulkanSwapchainOwner::GetRenderPass() const noexcept
{
    return m_renderPass;
}

VkFramebuffer VulkanSwapchainOwner::GetFramebuffer(std::uint32_t imageIndex) const noexcept
{
    if (imageIndex >= m_framebuffers.size())
    {
        return VK_NULL_HANDLE;
    }

    return m_framebuffers[imageIndex];
}

const VulkanSwapchainConfig& VulkanSwapchainOwner::GetConfig() const noexcept
{
    return m_config;
}

std::uint32_t VulkanSwapchainOwner::GetFramebufferCount() const noexcept
{
    return static_cast<std::uint32_t>(m_framebuffers.size());
}

void VulkanSwapchainOwner::Reset() noexcept
{
    if (m_device != VK_NULL_HANDLE)
    {
        if (m_destroyFramebuffer != nullptr)
        {
            for (VkFramebuffer framebuffer : m_framebuffers)
            {
                if (framebuffer != VK_NULL_HANDLE)
                {
                    m_destroyFramebuffer(m_device, framebuffer, nullptr);
                }
            }
        }

        if (m_renderPass != VK_NULL_HANDLE && m_destroyRenderPass != nullptr)
        {
            m_destroyRenderPass(m_device, m_renderPass, nullptr);
        }

        if (m_destroyImageView != nullptr)
        {
            for (VkImageView imageView : m_imageViews)
            {
                if (imageView != VK_NULL_HANDLE)
                {
                    m_destroyImageView(m_device, imageView, nullptr);
                }
            }
        }

        if (m_swapchain != VK_NULL_HANDLE && m_destroySwapchain != nullptr)
        {
            m_destroySwapchain(m_device, m_swapchain, nullptr);
        }
    }

    m_device = VK_NULL_HANDLE;
    m_swapchain = VK_NULL_HANDLE;
    m_images.clear();
    m_imageViews.clear();
    m_renderPass = VK_NULL_HANDLE;
    m_framebuffers.clear();
    m_destroySwapchain = nullptr;
    m_destroyImageView = nullptr;
    m_destroyRenderPass = nullptr;
    m_destroyFramebuffer = nullptr;
    m_config = {};
}
VulkanFrameResourcesOwner::VulkanFrameResourcesOwner() noexcept = default;

VulkanFrameResourcesOwner::VulkanFrameResourcesOwner(
    VkDevice device, VkCommandPool commandPool, std::vector<VulkanFrameSlotResources> slots,
    std::vector<VkSemaphore> renderFinishedSemaphores,
    VulkanGlobalDispatch::DestroyCommandPoolFn destroyCommandPool,
    VulkanGlobalDispatch::DestroySemaphoreFn destroySemaphore,
    VulkanGlobalDispatch::DestroyFenceFn destroyFence) noexcept
    : m_device{device}, m_commandPool{commandPool}, m_slots{std::move(slots)},
      m_renderFinishedSemaphores{std::move(renderFinishedSemaphores)},
      m_destroyCommandPool{destroyCommandPool}, m_destroySemaphore{destroySemaphore},
      m_destroyFence{destroyFence}
{
}

VulkanFrameResourcesOwner::VulkanFrameResourcesOwner(VulkanFrameResourcesOwner&& other) noexcept
    : m_device{other.m_device}, m_commandPool{other.m_commandPool},
      m_slots{std::move(other.m_slots)},
      m_renderFinishedSemaphores{std::move(other.m_renderFinishedSemaphores)},
      m_completedSubmissionSlots{std::move(other.m_completedSubmissionSlots)},
      m_poisoned{other.m_poisoned}, m_destroyCommandPool{other.m_destroyCommandPool},
      m_destroySemaphore{other.m_destroySemaphore}, m_destroyFence{other.m_destroyFence}
{
    other.m_device = VK_NULL_HANDLE;
    other.m_commandPool = VK_NULL_HANDLE;
    other.m_poisoned = false;
    other.m_destroyCommandPool = nullptr;
    other.m_destroySemaphore = nullptr;
    other.m_destroyFence = nullptr;
}

VulkanFrameResourcesOwner& VulkanFrameResourcesOwner::operator=(
    VulkanFrameResourcesOwner&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_device = other.m_device;
        m_commandPool = other.m_commandPool;
        m_slots = std::move(other.m_slots);
        m_renderFinishedSemaphores = std::move(other.m_renderFinishedSemaphores);
        m_completedSubmissionSlots = std::move(other.m_completedSubmissionSlots);
        m_poisoned = other.m_poisoned;
        m_destroyCommandPool = other.m_destroyCommandPool;
        m_destroySemaphore = other.m_destroySemaphore;
        m_destroyFence = other.m_destroyFence;

        other.m_device = VK_NULL_HANDLE;
        other.m_commandPool = VK_NULL_HANDLE;
        other.m_poisoned = false;
        other.m_destroyCommandPool = nullptr;
        other.m_destroySemaphore = nullptr;
        other.m_destroyFence = nullptr;
    }

    return *this;
}

VulkanFrameResourcesOwner::~VulkanFrameResourcesOwner()
{
    Reset();
}

bool VulkanFrameResourcesOwner::IsValid() const noexcept
{
    if (m_device == VK_NULL_HANDLE || m_commandPool == VK_NULL_HANDLE || m_slots.empty() ||
        m_renderFinishedSemaphores.empty())
    {
        return false;
    }

    return std::ranges::all_of(m_slots,
                               [](VulkanFrameSlotResources slot) noexcept
                               {
                                   return slot.IsValid();
                               }) &&
           std::ranges::all_of(m_renderFinishedSemaphores,
                               [](VkSemaphore semaphore) noexcept
                               {
                                   return semaphore != VK_NULL_HANDLE;
                               });
}

bool VulkanFrameResourcesOwner::IsPoisoned() const noexcept
{
    return m_poisoned;
}

std::uint32_t VulkanFrameResourcesOwner::GetSlotCount() const noexcept
{
    return static_cast<std::uint32_t>(m_slots.size());
}

VulkanFrameSlotResources VulkanFrameResourcesOwner::GetSlot(std::uint32_t slotIndex) const noexcept
{
    if (slotIndex >= m_slots.size())
    {
        return {};
    }

    return m_slots[slotIndex];
}

VkSemaphore VulkanFrameResourcesOwner::GetRenderFinishedSemaphore(
    std::uint32_t imageIndex) const noexcept
{
    if (imageIndex >= m_renderFinishedSemaphores.size())
    {
        return VK_NULL_HANDLE;
    }

    return m_renderFinishedSemaphores[imageIndex];
}

core::Result<bool> VulkanFrameResourcesOwner::PrepareFrameSlot(const VulkanGlobalDispatch& dispatch,
                                                               std::uint32_t slotIndex,
                                                               std::uint64_t timeoutNanoseconds)
{
    if (!IsValid() || m_poisoned)
    {
        return core::Result<bool>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState,
            "Vulkan frame-slot preparation requires live, reusable frame resources."));
    }

    if (slotIndex >= m_slots.size() || dispatch.waitForFences == nullptr ||
        dispatch.resetFences == nullptr)
    {
        return core::Result<bool>::FromError(MakeRenderError(
            RenderErrorCode::BackendFailure,
            "Vulkan frame-slot preparation requires valid fence state and dispatch."));
    }

    VulkanFrameSlotResources& slot = m_slots[slotIndex];
    std::array<VkFence, 2U> pendingFences{};
    std::uint32_t pendingFenceCount{};
    if (slot.acquireFencePending)
    {
        pendingFences[pendingFenceCount++] = slot.imageAcquiredFence;
    }
    if (slot.submissionFencePending)
    {
        pendingFences[pendingFenceCount++] = slot.inFlightFence;
    }

    if (pendingFenceCount > 0U)
    {
        const VkResult waitResult = dispatch.waitForFences(
            m_device, pendingFenceCount, pendingFences.data(), VK_TRUE, timeoutNanoseconds);
        if (waitResult == VK_TIMEOUT || waitResult == VK_NOT_READY)
        {
            return core::Result<bool>::FromValue(false);
        }
        if (waitResult != VK_SUCCESS)
        {
            return core::Result<bool>::FromError(
                MakeVulkanError(MapFrameFailure(waitResult), "vkWaitForFences.frame", waitResult));
        }

        if (slot.acquireFencePending)
        {
            slot.acquireFencePending = false;
            slot.acquireFenceNeedsReset = true;
        }
        if (slot.submissionFencePending)
        {
            slot.submissionFencePending = false;
            m_completedSubmissionSlots.push_back(slotIndex);
        }
    }

    if (slot.acquireFenceNeedsReset)
    {
        const VkResult resetResult = dispatch.resetFences(m_device, 1U, &slot.imageAcquiredFence);
        if (resetResult != VK_SUCCESS)
        {
            return core::Result<bool>::FromError(MakeVulkanError(
                MapFrameFailure(resetResult), "vkResetFences.acquire", resetResult));
        }
        slot.acquireFenceNeedsReset = false;
    }

    return core::Result<bool>::FromValue(true);
}

void VulkanFrameResourcesOwner::RecordImageAcquired(std::uint32_t slotIndex) noexcept
{
    if (slotIndex < m_slots.size())
    {
        m_slots[slotIndex].acquireFencePending = true;
    }
}

void VulkanFrameResourcesOwner::RecordSubmissionQueued(std::uint32_t slotIndex) noexcept
{
    if (slotIndex < m_slots.size())
    {
        VulkanFrameSlotResources& slot = m_slots[slotIndex];
        slot.submissionFencePending = true;
    }
}

void VulkanFrameResourcesOwner::MarkPoisoned() noexcept
{
    m_poisoned = true;
}

std::vector<std::uint32_t> VulkanFrameResourcesOwner::ConsumeCompletedSubmissionSlots() noexcept
{
    return std::exchange(m_completedSubmissionSlots, {});
}

core::Result<bool> VulkanFrameResourcesOwner::AreAllFencesSignaled(
    const VulkanGlobalDispatch& dispatch, std::uint64_t timeoutNanoseconds)
{
    if (!IsValid())
    {
        return core::Result<bool>::FromValue(true);
    }

    if (dispatch.waitForFences == nullptr)
    {
        return core::Result<bool>::FromError(MakeRenderError(
            RenderErrorCode::BackendFailure, "Vulkan fence wait dispatch is unavailable."));
    }

    std::vector<VkFence> fences;
    fences.reserve(m_slots.size() * 2U);
    for (const VulkanFrameSlotResources& slot : m_slots)
    {
        if (slot.acquireFencePending)
        {
            fences.push_back(slot.imageAcquiredFence);
        }
        if (slot.submissionFencePending)
        {
            fences.push_back(slot.inFlightFence);
        }
    }

    if (fences.empty())
    {
        return core::Result<bool>::FromValue(true);
    }

    const VkResult result =
        dispatch.waitForFences(m_device, static_cast<std::uint32_t>(fences.size()), fences.data(),
                               VK_TRUE, timeoutNanoseconds);
    if (result == VK_SUCCESS)
    {
        for (std::uint32_t slotIndex = 0U; slotIndex < m_slots.size(); ++slotIndex)
        {
            VulkanFrameSlotResources& slot = m_slots[slotIndex];
            if (slot.acquireFencePending)
            {
                slot.acquireFencePending = false;
                slot.acquireFenceNeedsReset = true;
            }
            if (slot.submissionFencePending)
            {
                slot.submissionFencePending = false;
                m_completedSubmissionSlots.push_back(slotIndex);
            }
        }
        return core::Result<bool>::FromValue(true);
    }

    if (result == VK_TIMEOUT || result == VK_NOT_READY)
    {
        return core::Result<bool>::FromValue(false);
    }

    return core::Result<bool>::FromError(
        MakeVulkanError(MapFrameFailure(result), "vkWaitForFences.retire", result));
}

void VulkanFrameResourcesOwner::Reset() noexcept
{
    if (m_device != VK_NULL_HANDLE)
    {
        if (m_destroySemaphore != nullptr)
        {
            for (VulkanFrameSlotResources slot : m_slots)
            {
                if (slot.imageAvailableSemaphore != VK_NULL_HANDLE)
                {
                    m_destroySemaphore(m_device, slot.imageAvailableSemaphore, nullptr);
                }
            }

            for (VkSemaphore semaphore : m_renderFinishedSemaphores)
            {
                if (semaphore != VK_NULL_HANDLE)
                {
                    m_destroySemaphore(m_device, semaphore, nullptr);
                }
            }
        }

        if (m_destroyFence != nullptr)
        {
            for (VulkanFrameSlotResources slot : m_slots)
            {
                if (slot.imageAcquiredFence != VK_NULL_HANDLE)
                {
                    m_destroyFence(m_device, slot.imageAcquiredFence, nullptr);
                }
                if (slot.inFlightFence != VK_NULL_HANDLE)
                {
                    m_destroyFence(m_device, slot.inFlightFence, nullptr);
                }
            }
        }

        if (m_commandPool != VK_NULL_HANDLE && m_destroyCommandPool != nullptr)
        {
            m_destroyCommandPool(m_device, m_commandPool, nullptr);
        }
    }

    m_device = VK_NULL_HANDLE;
    m_commandPool = VK_NULL_HANDLE;
    m_slots.clear();
    m_renderFinishedSemaphores.clear();
    m_completedSubmissionSlots.clear();
    m_poisoned = false;
    m_destroyCommandPool = nullptr;
    m_destroySemaphore = nullptr;
    m_destroyFence = nullptr;
}
VulkanPresentationTrackerOwner::VulkanPresentationTrackerOwner() noexcept = default;

VulkanPresentationTrackerOwner::VulkanPresentationTrackerOwner(
    VkDevice device, VulkanPresentationCompletionPath completionPath, bool usesPresentIds,
    std::vector<std::uint64_t> imagePresentationSerials,
    std::vector<std::uint64_t> pendingAcquireSerials, std::vector<bool> acquireProofPending,
    std::vector<VulkanPresentFenceSlot> presentFenceSlots,
    VulkanGlobalDispatch::DestroyFenceFn destroyFence) noexcept
    : m_device{device}, m_completionPath{completionPath}, m_usesPresentIds{usesPresentIds},
      m_imagePresentationSerials{std::move(imagePresentationSerials)},
      m_pendingAcquireSerials{std::move(pendingAcquireSerials)},
      m_acquireProofPending{std::move(acquireProofPending)},
      m_presentFenceSlots{std::move(presentFenceSlots)}, m_destroyFence{destroyFence}
{
}

VulkanPresentationTrackerOwner::VulkanPresentationTrackerOwner(
    VulkanPresentationTrackerOwner&& other) noexcept
    : m_device{other.m_device}, m_completionPath{other.m_completionPath},
      m_usesPresentIds{other.m_usesPresentIds},
      m_hadQueuedPresentation{other.m_hadQueuedPresentation},
      m_completionSatisfied{other.m_completionSatisfied},
      m_usedExplicitCompletion{other.m_usedExplicitCompletion},
      m_nextPresentId{other.m_nextPresentId}, m_lastQueuedPresentId{other.m_lastQueuedPresentId},
      m_nextCorePresentationSerial{other.m_nextCorePresentationSerial},
      m_coreCompletionObserved{other.m_coreCompletionObserved},
      m_imagePresentationSerials{std::move(other.m_imagePresentationSerials)},
      m_pendingAcquireSerials{std::move(other.m_pendingAcquireSerials)},
      m_acquireProofPending{std::move(other.m_acquireProofPending)},
      m_presentFenceSlots{std::move(other.m_presentFenceSlots)},
      m_destroyFence{other.m_destroyFence}
{
    other.m_device = VK_NULL_HANDLE;
    other.m_usesPresentIds = false;
    other.m_hadQueuedPresentation = false;
    other.m_completionSatisfied = false;
    other.m_usedExplicitCompletion = false;
    other.m_nextPresentId = 1U;
    other.m_lastQueuedPresentId = 0U;
    other.m_nextCorePresentationSerial = 1U;
    other.m_coreCompletionObserved = false;
    other.m_destroyFence = nullptr;
}

VulkanPresentationTrackerOwner& VulkanPresentationTrackerOwner::operator=(
    VulkanPresentationTrackerOwner&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_device = other.m_device;
        m_completionPath = other.m_completionPath;
        m_usesPresentIds = other.m_usesPresentIds;
        m_hadQueuedPresentation = other.m_hadQueuedPresentation;
        m_completionSatisfied = other.m_completionSatisfied;
        m_usedExplicitCompletion = other.m_usedExplicitCompletion;
        m_nextPresentId = other.m_nextPresentId;
        m_lastQueuedPresentId = other.m_lastQueuedPresentId;
        m_nextCorePresentationSerial = other.m_nextCorePresentationSerial;
        m_coreCompletionObserved = other.m_coreCompletionObserved;
        m_imagePresentationSerials = std::move(other.m_imagePresentationSerials);
        m_pendingAcquireSerials = std::move(other.m_pendingAcquireSerials);
        m_acquireProofPending = std::move(other.m_acquireProofPending);
        m_presentFenceSlots = std::move(other.m_presentFenceSlots);
        m_destroyFence = other.m_destroyFence;

        other.m_device = VK_NULL_HANDLE;
        other.m_usesPresentIds = false;
        other.m_hadQueuedPresentation = false;
        other.m_completionSatisfied = false;
        other.m_usedExplicitCompletion = false;
        other.m_nextPresentId = 1U;
        other.m_lastQueuedPresentId = 0U;
        other.m_nextCorePresentationSerial = 1U;
        other.m_coreCompletionObserved = false;
        other.m_destroyFence = nullptr;
    }

    return *this;
}

VulkanPresentationTrackerOwner::~VulkanPresentationTrackerOwner()
{
    Reset();
}

bool VulkanPresentationTrackerOwner::IsValid() const noexcept
{
    if (m_device == VK_NULL_HANDLE || m_imagePresentationSerials.empty() ||
        m_pendingAcquireSerials.empty() ||
        m_pendingAcquireSerials.size() != m_acquireProofPending.size())
    {
        return false;
    }

    if (!UsesPresentFences())
    {
        return true;
    }

    return m_presentFenceSlots.size() == m_pendingAcquireSerials.size() &&
           std::ranges::all_of(m_presentFenceSlots,
                               [](VulkanPresentFenceSlot slot) noexcept
                               {
                                   return slot.fence != VK_NULL_HANDLE;
                               });
}

VulkanPresentationCompletionPath VulkanPresentationTrackerOwner::GetCompletionPath() const noexcept
{
    return m_completionPath;
}

bool VulkanPresentationTrackerOwner::UsesPresentFences() const noexcept
{
    return m_completionPath == VulkanPresentationCompletionPath::SwapchainMaintenanceFence;
}

bool VulkanPresentationTrackerOwner::UsesPresentIds() const noexcept
{
    return m_usesPresentIds;
}

bool VulkanPresentationTrackerOwner::HasQueuedPresentation() const noexcept
{
    return m_hadQueuedPresentation;
}

core::Result<bool> VulkanPresentationTrackerOwner::PrepareFrameSlot(
    const VulkanGlobalDispatch& dispatch, std::uint32_t frameSlotIndex,
    std::uint64_t timeoutNanoseconds)
{
    if (!IsValid())
    {
        return core::Result<bool>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Presentation tracking requires a live Vulkan device."));
    }

    if (!UsesPresentFences())
    {
        return core::Result<bool>::FromValue(true);
    }

    if (frameSlotIndex >= m_presentFenceSlots.size() || dispatch.waitForFences == nullptr ||
        dispatch.resetFences == nullptr)
    {
        return core::Result<bool>::FromError(MakeRenderError(
            RenderErrorCode::BackendFailure, "Vulkan present-fence dispatch is unavailable."));
    }

    VulkanPresentFenceSlot& slot = m_presentFenceSlots[frameSlotIndex];
    if (!slot.pending)
    {
        return core::Result<bool>::FromValue(true);
    }

    VkResult result =
        dispatch.waitForFences(m_device, 1U, &slot.fence, VK_TRUE, timeoutNanoseconds);
    if (result == VK_TIMEOUT || result == VK_NOT_READY)
    {
        return core::Result<bool>::FromValue(false);
    }
    if (result != VK_SUCCESS)
    {
        return core::Result<bool>::FromError(
            MakeVulkanError(MapFrameFailure(result), "vkWaitForFences.present", result));
    }

    result = dispatch.resetFences(m_device, 1U, &slot.fence);
    if (result != VK_SUCCESS)
    {
        return core::Result<bool>::FromError(
            MakeVulkanError(MapFrameFailure(result), "vkResetFences.present", result));
    }

    slot.pending = false;
    m_usedExplicitCompletion = true;
    return core::Result<bool>::FromValue(true);
}

VkFence VulkanPresentationTrackerOwner::GetPresentFence(std::uint32_t frameSlotIndex) const noexcept
{
    if (!UsesPresentFences() || frameSlotIndex >= m_presentFenceSlots.size())
    {
        return VK_NULL_HANDLE;
    }

    return m_presentFenceSlots[frameSlotIndex].fence;
}

std::uint64_t VulkanPresentationTrackerOwner::ReservePresentId() noexcept
{
    if (!m_usesPresentIds || m_nextPresentId == 0U)
    {
        return 0U;
    }

    const std::uint64_t presentId = m_nextPresentId;
    m_nextPresentId = presentId == std::numeric_limits<std::uint64_t>::max() ? 0U : presentId + 1U;
    return presentId;
}

void VulkanPresentationTrackerOwner::RecordFrameFenceCompletion(
    std::uint32_t frameSlotIndex) noexcept
{
    if (frameSlotIndex < m_acquireProofPending.size() && m_acquireProofPending[frameSlotIndex])
    {
        m_acquireProofPending[frameSlotIndex] = false;
        m_coreCompletionObserved = true;
    }
}

void VulkanPresentationTrackerOwner::RecordImageAcquired(std::uint32_t frameSlotIndex,
                                                         std::uint32_t imageIndex) noexcept
{
    if (frameSlotIndex >= m_pendingAcquireSerials.size() ||
        imageIndex >= m_imagePresentationSerials.size())
    {
        return;
    }

    m_pendingAcquireSerials[frameSlotIndex] = m_imagePresentationSerials[imageIndex];
}

void VulkanPresentationTrackerOwner::RecordAcquireSubmission(std::uint32_t frameSlotIndex) noexcept
{
    if (frameSlotIndex >= m_pendingAcquireSerials.size())
    {
        return;
    }

    m_acquireProofPending[frameSlotIndex] = m_pendingAcquireSerials[frameSlotIndex] != 0U;
    m_pendingAcquireSerials[frameSlotIndex] = 0U;
}

void VulkanPresentationTrackerOwner::AbandonAcquiredFrame(std::uint32_t frameSlotIndex) noexcept
{
    if (frameSlotIndex >= m_pendingAcquireSerials.size())
    {
        return;
    }

    m_pendingAcquireSerials[frameSlotIndex] = 0U;
    m_acquireProofPending[frameSlotIndex] = false;
}

void VulkanPresentationTrackerOwner::RecordPresentResult(std::uint32_t frameSlotIndex,
                                                         std::uint32_t imageIndex,
                                                         std::uint64_t presentId,
                                                         VkResult result) noexcept
{
    if (!WasPresentationOperationEnqueued(result))
    {
        return;
    }

    m_hadQueuedPresentation = true;
    m_completionSatisfied = false;
    if (UsesPresentFences() && frameSlotIndex < m_presentFenceSlots.size())
    {
        m_presentFenceSlots[frameSlotIndex].pending = true;
    }
    if (m_usesPresentIds && presentId != 0U)
    {
        m_lastQueuedPresentId = presentId;
    }
    if (imageIndex < m_imagePresentationSerials.size() && m_nextCorePresentationSerial != 0U)
    {
        m_imagePresentationSerials[imageIndex] = m_nextCorePresentationSerial;
        m_nextCorePresentationSerial =
            m_nextCorePresentationSerial == std::numeric_limits<std::uint64_t>::max()
                ? 0U
                : m_nextCorePresentationSerial + 1U;
    }
}

bool VulkanPresentationTrackerOwner::ConsumeCorePresentationCompletion() noexcept
{
    return std::exchange(m_coreCompletionObserved, false);
}

core::Result<VulkanPresentationCompletionResult> VulkanPresentationTrackerOwner::PollCompletion(
    const VulkanGlobalDispatch& dispatch, VkSwapchainKHR swapchain,
    std::uint64_t timeoutNanoseconds)
{
    if (!IsValid())
    {
        return core::Result<VulkanPresentationCompletionResult>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Presentation completion requires a live tracker."));
    }

    if (m_completionSatisfied || !m_hadQueuedPresentation)
    {
        return core::Result<VulkanPresentationCompletionResult>::FromValue(
            VulkanPresentationCompletionResult{.status =
                                                   VulkanPresentationCompletionStatus::Complete,
                                               .usedExplicitCompletion = m_usedExplicitCompletion});
    }

    if (UsesPresentFences())
    {
        if (dispatch.waitForFences == nullptr)
        {
            return core::Result<VulkanPresentationCompletionResult>::FromError(MakeRenderError(
                RenderErrorCode::BackendFailure, "Vulkan present-fence wait is unavailable."));
        }

        std::vector<VkFence> pendingFences;
        pendingFences.reserve(m_presentFenceSlots.size());
        for (VulkanPresentFenceSlot slot : m_presentFenceSlots)
        {
            if (slot.pending)
            {
                pendingFences.push_back(slot.fence);
            }
        }

        if (pendingFences.empty())
        {
            m_completionSatisfied = true;
            m_usedExplicitCompletion = true;
        }
        else
        {
            const VkResult result =
                dispatch.waitForFences(m_device, static_cast<std::uint32_t>(pendingFences.size()),
                                       pendingFences.data(), VK_TRUE, timeoutNanoseconds);
            if (result == VK_TIMEOUT || result == VK_NOT_READY)
            {
                return core::Result<VulkanPresentationCompletionResult>::FromValue(
                    VulkanPresentationCompletionResult{
                        .status = VulkanPresentationCompletionStatus::Pending});
            }
            if (result != VK_SUCCESS)
            {
                return core::Result<VulkanPresentationCompletionResult>::FromError(MakeVulkanError(
                    MapFrameFailure(result), "vkWaitForFences.present_retire", result));
            }

            for (VulkanPresentFenceSlot& slot : m_presentFenceSlots)
            {
                slot.pending = false;
            }
            m_completionSatisfied = true;
            m_usedExplicitCompletion = true;
        }

        return core::Result<VulkanPresentationCompletionResult>::FromValue(
            VulkanPresentationCompletionResult{.status =
                                                   VulkanPresentationCompletionStatus::Complete,
                                               .usedExplicitCompletion = true});
    }

    if (m_completionPath == VulkanPresentationCompletionPath::PresentWait)
    {
        if (swapchain == VK_NULL_HANDLE || m_lastQueuedPresentId == 0U ||
            dispatch.waitForPresent == nullptr)
        {
            return core::Result<VulkanPresentationCompletionResult>::FromError(MakeRenderError(
                RenderErrorCode::BackendFailure, "Vulkan present-wait dispatch is unavailable."));
        }

        const VkResult result =
            dispatch.waitForPresent(m_device, swapchain, m_lastQueuedPresentId, timeoutNanoseconds);
        if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
        {
            m_completionSatisfied = true;
            m_usedExplicitCompletion = true;
            return core::Result<VulkanPresentationCompletionResult>::FromValue(
                VulkanPresentationCompletionResult{.status =
                                                       VulkanPresentationCompletionStatus::Complete,
                                                   .usedExplicitCompletion = true});
        }
        if (result == VK_TIMEOUT || result == VK_NOT_READY)
        {
            return core::Result<VulkanPresentationCompletionResult>::FromValue(
                VulkanPresentationCompletionResult{
                    .status = VulkanPresentationCompletionStatus::Pending});
        }
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_SURFACE_LOST_KHR ||
            result == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
        {
            return core::Result<VulkanPresentationCompletionResult>::FromValue(
                VulkanPresentationCompletionResult{
                    .status = VulkanPresentationCompletionStatus::PracticalIdleFallbackRequired});
        }

        return core::Result<VulkanPresentationCompletionResult>::FromError(
            MakeVulkanError(MapFrameFailure(result), "vkWaitForPresentKHR", result));
    }

    return core::Result<VulkanPresentationCompletionResult>::FromValue(
        VulkanPresentationCompletionResult{.status = VulkanPresentationCompletionStatus::Pending});
}

void VulkanPresentationTrackerOwner::MarkAwaitingSuccessorPresentation() noexcept
{
    m_completionPath = VulkanPresentationCompletionPath::CoreAcquireHistory;
    m_completionSatisfied = false;
}

void VulkanPresentationTrackerOwner::MarkSuccessorPresentationComplete() noexcept
{
    m_completionSatisfied = true;
}

void VulkanPresentationTrackerOwner::Reset() noexcept
{
    if (m_device != VK_NULL_HANDLE && m_destroyFence != nullptr)
    {
        for (VulkanPresentFenceSlot slot : m_presentFenceSlots)
        {
            if (slot.fence != VK_NULL_HANDLE)
            {
                m_destroyFence(m_device, slot.fence, nullptr);
            }
        }
    }

    m_device = VK_NULL_HANDLE;
    m_completionPath = VulkanPresentationCompletionPath::CoreAcquireHistory;
    m_usesPresentIds = false;
    m_hadQueuedPresentation = false;
    m_completionSatisfied = false;
    m_usedExplicitCompletion = false;
    m_nextPresentId = 1U;
    m_lastQueuedPresentId = 0U;
    m_nextCorePresentationSerial = 1U;
    m_coreCompletionObserved = false;
    m_imagePresentationSerials.clear();
    m_pendingAcquireSerials.clear();
    m_acquireProofPending.clear();
    m_presentFenceSlots.clear();
    m_destroyFence = nullptr;
}
core::Result<VulkanInstanceInfo> VulkanInstanceBootstrap::EnsureInitialized(
    const VulkanGlobalDispatch& dispatch, VulkanWsiKind wsiKind,
    RenderValidationMode validationMode)
{
    if (m_owner != nullptr)
    {
        const VulkanInstanceInfo info = m_owner->GetInfo();
        if (info.wsiKind != wsiKind)
        {
            return core::Result<VulkanInstanceInfo>::FromError(MakeRenderError(
                RenderErrorCode::UnsupportedSurface,
                "RenderBootstrap already initialized Vulkan for " +
                    std::string{GetVulkanWsiKindName(info.wsiKind)} +
                    " WSI and cannot mix it with " + std::string{GetVulkanWsiKindName(wsiKind)} +
                    " in the same instance."));
        }

        if (info.requestedValidationMode != validationMode)
        {
            return core::Result<VulkanInstanceInfo>::FromError(MakeRenderError(
                RenderErrorCode::InvalidState,
                "RenderBootstrap already initialized Vulkan with a different validation policy."));
        }

        return core::Result<VulkanInstanceInfo>::FromValue(info);
    }

    core::Result<VulkanInstanceOwner> created =
        CreateVulkanInstanceForWsi(dispatch, wsiKind, validationMode);
    if (!created)
    {
        return core::Result<VulkanInstanceInfo>::FromError(std::move(created).GetError());
    }

    VulkanInstanceInfo info = created.GetValue().GetInfo();
    m_owner = std::make_shared<VulkanInstanceOwner>(std::move(created).GetValue());
    return core::Result<VulkanInstanceInfo>::FromValue(info);
}

bool VulkanInstanceBootstrap::IsInitialized() const noexcept
{
    return m_owner != nullptr && m_owner->IsValid();
}

std::optional<VulkanInstanceInfo> VulkanInstanceBootstrap::GetInfo() const noexcept
{
    if (!IsInitialized())
    {
        return std::nullopt;
    }

    return m_owner->GetInfo();
}

std::shared_ptr<VulkanInstanceOwner> VulkanInstanceBootstrap::GetOwner() const noexcept
{
    return m_owner;
}

RenderValidationReport VulkanInstanceBootstrap::GetValidationReport() const noexcept
{
    return m_owner == nullptr ? RenderValidationReport{} : m_owner->GetValidationReport();
}

void VulkanInstanceBootstrap::Reset() noexcept
{
    m_owner.reset();
}

VulkanGlobalDispatch CreateVolkGlobalDispatch() noexcept
{
    return VulkanGlobalDispatch{
        .initialize = &VolkInitialize,
        .enumerateInstanceVersion = &VolkEnumerateInstanceVersion,
        .enumerateInstanceExtensionProperties = &VolkEnumerateInstanceExtensionProperties,
        .enumerateInstanceLayerProperties = &VolkEnumerateInstanceLayerProperties,
        .createInstance = &VolkCreateInstance,
        .destroyInstance = &VolkDestroyInstance,
        .loadInstanceTable = &VolkLoadInstanceTable,
        .createDebugUtilsMessenger = &VolkCreateDebugUtilsMessenger,
        .destroyDebugUtilsMessenger = &VolkDestroyDebugUtilsMessenger,
        .createWin32Surface = &VolkCreateWin32Surface,
        .createX11Surface = &VolkCreateX11Surface,
        .createWaylandSurface = &VolkCreateWaylandSurface,
        .destroySurface = &VolkDestroySurface,
        .enumeratePhysicalDevices = &VolkEnumeratePhysicalDevices,
        .getPhysicalDeviceProperties2 = &VolkGetPhysicalDeviceProperties2,
        .getPhysicalDeviceFeatures2 = &VolkGetPhysicalDeviceFeatures2,
        .getPhysicalDeviceMemoryProperties = &VolkGetPhysicalDeviceMemoryProperties,
        .getPhysicalDeviceQueueFamilyProperties = &VolkGetPhysicalDeviceQueueFamilyProperties,
        .getPhysicalDeviceSurfaceSupport = &VolkGetPhysicalDeviceSurfaceSupport,
        .getPhysicalDeviceSurfaceFormats = &VolkGetPhysicalDeviceSurfaceFormats,
        .getPhysicalDeviceSurfacePresentModes = &VolkGetPhysicalDeviceSurfacePresentModes,
        .getPhysicalDeviceSurfaceCapabilities = &VolkGetPhysicalDeviceSurfaceCapabilities,
        .enumerateDeviceExtensionProperties = &VolkEnumerateDeviceExtensionProperties,
        .createDevice = &VolkCreateDevice,
        .destroyDevice = &VolkDestroyDevice,
        .loadDeviceTable = &VolkLoadDeviceTable,
        .getDeviceQueue = &VolkGetDeviceQueue,
        .deviceWaitIdle = &VolkDeviceWaitIdle,
        .queueWaitIdle = &VolkQueueWaitIdle,
        .createSwapchain = &VolkCreateSwapchain,
        .destroySwapchain = &VolkDestroySwapchain,
        .getSwapchainImages = &VolkGetSwapchainImages,
        .createImageView = &VolkCreateImageView,
        .destroyImageView = &VolkDestroyImageView,
        .createRenderPass = &VolkCreateRenderPass,
        .destroyRenderPass = &VolkDestroyRenderPass,
        .createFramebuffer = &VolkCreateFramebuffer,
        .destroyFramebuffer = &VolkDestroyFramebuffer,
        .createCommandPool = &VolkCreateCommandPool,
        .destroyCommandPool = &VolkDestroyCommandPool,
        .allocateCommandBuffers = &VolkAllocateCommandBuffers,
        .resetCommandBuffer = &VolkResetCommandBuffer,
        .beginCommandBuffer = &VolkBeginCommandBuffer,
        .endCommandBuffer = &VolkEndCommandBuffer,
        .cmdBeginRenderPass = &VolkCmdBeginRenderPass,
        .cmdEndRenderPass = &VolkCmdEndRenderPass,
        .createSemaphore = &VolkCreateSemaphore,
        .destroySemaphore = &VolkDestroySemaphore,
        .createFence = &VolkCreateFence,
        .destroyFence = &VolkDestroyFence,
        .waitForFences = &VolkWaitForFences,
        .resetFences = &VolkResetFences,
        .acquireNextImage = &VolkAcquireNextImage,
        .queueSubmit = &VolkQueueSubmit,
        .queuePresent = &VolkQueuePresent,
        .waitForPresent = &VolkWaitForPresent,
        .setDebugUtilsObjectName = &VolkSetDebugUtilsObjectName,
        .cmdBeginDebugUtilsLabel = &VolkCmdBeginDebugUtilsLabel,
        .cmdEndDebugUtilsLabel = &VolkCmdEndDebugUtilsLabel,
        .createOwnerLocalAllocator = &CreateOwnerLocalVmaAllocator,
        .destroyAllocator = &DestroyVmaAllocator};
}

core::Result<VulkanInstanceOwner> CreateVulkanInstanceForWsi(const VulkanGlobalDispatch& dispatch,
                                                             VulkanWsiKind wsiKind,
                                                             RenderValidationMode validationMode)
{
    if (!HasRequiredDispatch(dispatch))
    {
        return core::Result<VulkanInstanceOwner>::FromError(MakeRenderError(
            RenderErrorCode::LoaderUnavailable,
            "Vulkan global dispatch is incomplete; install or repair the system Vulkan loader."));
    }

    VkResult result = dispatch.initialize();
    if (result != VK_SUCCESS)
    {
        [[maybe_unused]] const core::Error nativeFailure =
            MakeVulkanError(RenderErrorCode::LoaderUnavailable, "volkInitialize", result);
        return core::Result<VulkanInstanceOwner>::FromError(
            MakeRenderError(RenderErrorCode::LoaderUnavailable,
                            "Could not open the system Vulkan loader. " +
                                MakeVulkanFailureMessage("volkInitialize", result)));
    }

    std::uint32_t loaderVersion{};
    result = dispatch.enumerateInstanceVersion(&loaderVersion);
    if (result != VK_SUCCESS)
    {
        return core::Result<VulkanInstanceOwner>::FromError(
            MakeVulkanError(RenderErrorCode::BackendFailure, "vkEnumerateInstanceVersion", result));
    }

    if (loaderVersion < GetMinimumVulkanInstanceVersion())
    {
        return core::Result<VulkanInstanceOwner>::FromError(MakeRenderError(
            RenderErrorCode::UnsupportedCapability,
            "The installed Vulkan loader reports API " + FormatVulkanVersion(loaderVersion) +
                ", but ponder_render requires at least Vulkan " +
                FormatVulkanVersion(GetMinimumVulkanInstanceVersion()) + "."));
    }

    core::Result<std::vector<VkExtensionProperties>> extensions =
        EnumerateInstanceExtensions(dispatch);
    if (!extensions)
    {
        return core::Result<VulkanInstanceOwner>::FromError(std::move(extensions).GetError());
    }

    core::Result<std::vector<VkLayerProperties>> layers = EnumerateInstanceLayers(dispatch);
    if (!layers)
    {
        return core::Result<VulkanInstanceOwner>::FromError(std::move(layers).GetError());
    }

    const char* const wsiExtension = GetVulkanWsiExtensionName(wsiKind);
    const std::array requiredSurfaceExtensions{kSurfaceExtensionName, wsiExtension};
    for (const char* const requiredExtension : requiredSurfaceExtensions)
    {
        if (!ContainsExtension(extensions.GetValue(), requiredExtension))
        {
            return core::Result<VulkanInstanceOwner>::FromError(
                MakeRenderError(RenderErrorCode::UnsupportedSurface,
                                "The Vulkan loader does not expose required instance extension " +
                                    std::string{requiredExtension} + " for " +
                                    std::string{GetVulkanWsiKindName(wsiKind)} + " WSI."));
        }
    }

    core::Result<VulkanValidationSelection> validation =
        SelectValidationPolicy(validationMode, extensions.GetValue(), layers.GetValue());
    if (!validation)
    {
        return core::Result<VulkanInstanceOwner>::FromError(std::move(validation).GetError());
    }

    const VulkanValidationSelection validationSelection = std::move(validation).GetValue();
    std::vector<const char*> enabledExtensions{requiredSurfaceExtensions.begin(),
                                               requiredSurfaceExtensions.end()};
    const bool getSurfaceCapabilities2Enabled =
        ContainsExtension(extensions.GetValue(), kGetSurfaceCapabilities2ExtensionName);
    const bool khrSurfaceMaintenance1Enabled =
        getSurfaceCapabilities2Enabled &&
        ContainsExtension(extensions.GetValue(), kKhrSurfaceMaintenance1ExtensionName);
    const bool extSurfaceMaintenance1Enabled =
        getSurfaceCapabilities2Enabled &&
        ContainsExtension(extensions.GetValue(), kExtSurfaceMaintenance1ExtensionName);
    const bool surfaceMaintenance1Enabled =
        khrSurfaceMaintenance1Enabled || extSurfaceMaintenance1Enabled;
    if (surfaceMaintenance1Enabled)
    {
        enabledExtensions.push_back(kGetSurfaceCapabilities2ExtensionName);
    }
    if (khrSurfaceMaintenance1Enabled)
    {
        enabledExtensions.push_back(kKhrSurfaceMaintenance1ExtensionName);
    }
    if (extSurfaceMaintenance1Enabled)
    {
        enabledExtensions.push_back(kExtSurfaceMaintenance1ExtensionName);
    }
    enabledExtensions.insert(enabledExtensions.end(), validationSelection.enabledExtensions.begin(),
                             validationSelection.enabledExtensions.end());

    const std::uint32_t selectedVersion =
        std::min(loaderVersion, GetMaximumHeaderVulkanInstanceVersion());

    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "ponder";
    applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    applicationInfo.pEngineName = "ponder";
    applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    applicationInfo.apiVersion = selectedVersion;

    std::unique_ptr<VulkanDebugMessengerContext> debugContext;
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (validationSelection.debugUtilsEnabled)
    {
        debugContext = std::make_unique<VulkanDebugMessengerContext>();
        debugContext->currentOperation = "vkCreateInstance";
        debugCreateInfo = MakeDebugMessengerCreateInfo(debugContext.get());
    }
    VkValidationFeaturesEXT validationFeatures{};
    if (validationSelection.validationFeaturesEnabled)
    {
        validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        validationFeatures.enabledValidationFeatureCount =
            static_cast<std::uint32_t>(validationSelection.enabledFeatures.size());
        validationFeatures.pEnabledValidationFeatures = validationSelection.enabledFeatures.data();
        validationFeatures.pNext =
            validationSelection.debugUtilsEnabled ? &debugCreateInfo : nullptr;
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext =
        validationSelection.validationFeaturesEnabled
            ? static_cast<const void*>(&validationFeatures)
            : static_cast<const void*>(validationSelection.debugUtilsEnabled ? &debugCreateInfo
                                                                             : nullptr);
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.enabledLayerCount =
        static_cast<std::uint32_t>(validationSelection.enabledLayers.size());
    createInfo.ppEnabledLayerNames = validationSelection.enabledLayers.data();
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(enabledExtensions.size());
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    VkInstance instance{VK_NULL_HANDLE};
    result = dispatch.createInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS)
    {
        return core::Result<VulkanInstanceOwner>::FromError(
            MakeVulkanError(MapInstanceCreateFailure(result), "vkCreateInstance", result));
    }

    std::optional<VolkInstanceTable> volkTable;
    VulkanGlobalDispatch instanceDispatch = dispatch;
    if (dispatch.loadInstanceTable != nullptr)
    {
        volkTable.emplace();
        dispatch.loadInstanceTable(&*volkTable, instance);
        instanceDispatch = BindInstanceDispatch(dispatch, *volkTable);
    }
    else
    {
        dispatch.loadInstanceOnly(instance);
    }

    VkDebugUtilsMessengerEXT debugMessenger{VK_NULL_HANDLE};
    if (validationSelection.debugUtilsEnabled)
    {
        if (!HasDebugMessengerDispatch(instanceDispatch))
        {
            instanceDispatch.destroyInstance(instance, nullptr);
            return core::Result<VulkanInstanceOwner>::FromError(MakeRenderError(
                RenderErrorCode::UnsupportedCapability,
                "VK_EXT_debug_utils was selected but debug messenger dispatch is unavailable."));
        }

        debugContext->currentOperation = "vkCreateDebugUtilsMessengerEXT";
        result = instanceDispatch.createDebugUtilsMessenger(instance, &debugCreateInfo, nullptr,
                                                            &debugMessenger);
        if (result != VK_SUCCESS)
        {
            instanceDispatch.destroyInstance(instance, nullptr);
            return core::Result<VulkanInstanceOwner>::FromError(MakeVulkanError(
                MapInstanceCreateFailure(result), "vkCreateDebugUtilsMessengerEXT", result));
        }

        debugContext->currentOperation = "vulkan-runtime-validation";
    }

    const VulkanInstanceInfo info{
        .apiVersion = selectedVersion,
        .wsiKind = wsiKind,
        .requestedValidationMode = validationMode,
        .enabledValidationMode = validationSelection.enabledMode,
        .validationEnabled = validationSelection.validationEnabled,
        .debugUtilsEnabled = validationSelection.debugUtilsEnabled,
        .debugMessengerEnabled = debugMessenger != VK_NULL_HANDLE,
        .validationFeaturesEnabled = validationSelection.validationFeaturesEnabled,
        .surfaceMaintenance1Enabled = surfaceMaintenance1Enabled,
        .khrSurfaceMaintenance1Enabled = khrSurfaceMaintenance1Enabled,
        .extSurfaceMaintenance1Enabled = extSurfaceMaintenance1Enabled,
        .debugUtilityHooks =
            MakeDebugUtilityHooks(instanceDispatch, validationSelection.debugUtilsEnabled)};

    return core::Result<VulkanInstanceOwner>::FromValue(
        VulkanInstanceOwner{instance, debugMessenger, instanceDispatch.destroyInstance,
                            instanceDispatch.destroyDebugUtilsMessenger, instanceDispatch,
                            std::move(volkTable), info, std::move(debugContext)});
}

core::Result<VulkanSurfaceOwner> CreateVulkanSurfaceForNativeWindow(
    const VulkanGlobalDispatch&, std::shared_ptr<VulkanInstanceOwner> instance,
    const platform::NativeWindowHandle& nativeWindowHandle)
{
    if (instance == nullptr || !instance->IsValid())
    {
        return core::Result<VulkanSurfaceOwner>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Vulkan surface creation requires a live Vulkan instance owner."));
    }

    const VulkanGlobalDispatch& dispatch = instance->GetDispatch();

    const VulkanWsiKind nativeWsiKind = GetVulkanWsiKind(nativeWindowHandle);
    if (instance->GetInfo().wsiKind != nativeWsiKind)
    {
        return core::Result<VulkanSurfaceOwner>::FromError(MakeRenderError(
            RenderErrorCode::UnsupportedSurface,
            "Native window WSI " + std::string{GetVulkanWsiKindName(nativeWsiKind)} +
                " does not match the initialized Vulkan instance WSI " +
                std::string{GetVulkanWsiKindName(instance->GetInfo().wsiKind)} + "."));
    }

    const auto createSurface = [&](VulkanWsiKind wsiKind, const void* createInfo,
                                   VulkanGlobalDispatch::CreateSurfaceFn createSurfaceFn,
                                   std::string_view operation) -> core::Result<VulkanSurfaceOwner>
    {
        if (!HasSurfaceDispatch(dispatch, createSurfaceFn))
        {
            return core::Result<VulkanSurfaceOwner>::FromError(
                MakeRenderError(RenderErrorCode::UnsupportedCapability,
                                std::string{operation} + " dispatch is unavailable."));
        }

        VkSurfaceKHR surface{VK_NULL_HANDLE};
        const VkResult result =
            createSurfaceFn(instance->GetHandle(), createInfo, nullptr, &surface);
        if (result != VK_SUCCESS)
        {
            return core::Result<VulkanSurfaceOwner>::FromError(
                MakeVulkanError(MapSurfaceCreateFailure(result), operation, result));
        }

        return core::Result<VulkanSurfaceOwner>::FromValue(VulkanSurfaceOwner{
            instance, surface, dispatch.destroySurface, VulkanSurfaceInfo{.wsiKind = wsiKind}});
    };

    return std::visit(
        Overloaded{
            [&](platform::NativeWin32Window native) -> core::Result<VulkanSurfaceOwner>
            {
                if (native.instance == nullptr || native.window == nullptr)
                {
                    return core::Result<VulkanSurfaceOwner>::FromError(MakeRenderError(
                        RenderErrorCode::InvalidArgument,
                        "Win32 Vulkan surface creation requires non-null HINSTANCE and HWND."));
                }

#if defined(VK_USE_PLATFORM_WIN32_KHR)
                VkWin32SurfaceCreateInfoKHR createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
                createInfo.hinstance = reinterpret_cast<HINSTANCE>(native.instance);
                createInfo.hwnd = reinterpret_cast<HWND>(native.window);
                return createSurface(VulkanWsiKind::Win32, &createInfo, dispatch.createWin32Surface,
                                     "vkCreateWin32SurfaceKHR");
#else
                return createSurface(VulkanWsiKind::Win32, nullptr, dispatch.createWin32Surface,
                                     "vkCreateWin32SurfaceKHR");
#endif
            },
            [&](platform::NativeX11Window native) -> core::Result<VulkanSurfaceOwner>
            {
                if (native.display == nullptr || native.window == 0U)
                {
                    return core::Result<VulkanSurfaceOwner>::FromError(MakeRenderError(
                        RenderErrorCode::InvalidArgument,
                        "X11 Vulkan surface creation requires non-null Display and Window."));
                }

#if defined(VK_USE_PLATFORM_XLIB_KHR)
                if (native.window > static_cast<std::uintptr_t>(std::numeric_limits<Window>::max()))
                {
                    return core::Result<VulkanSurfaceOwner>::FromError(MakeRenderError(
                        RenderErrorCode::InvalidArgument,
                        "X11 native Window value cannot be represented by this build."));
                }

                VkXlibSurfaceCreateInfoKHR createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
                createInfo.dpy = reinterpret_cast<Display*>(native.display);
                createInfo.window = static_cast<Window>(native.window);
                return createSurface(VulkanWsiKind::X11, &createInfo, dispatch.createX11Surface,
                                     "vkCreateXlibSurfaceKHR");
#else
                return createSurface(VulkanWsiKind::X11, nullptr, dispatch.createX11Surface,
                                     "vkCreateXlibSurfaceKHR");
#endif
            },
            [&](platform::NativeWaylandWindow native) -> core::Result<VulkanSurfaceOwner>
            {
                if (native.display == nullptr || native.surface == nullptr)
                {
                    return core::Result<VulkanSurfaceOwner>::FromError(MakeRenderError(
                        RenderErrorCode::InvalidArgument,
                        "Wayland Vulkan surface creation requires non-null display and surface."));
                }

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
                VkWaylandSurfaceCreateInfoKHR createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
                createInfo.display = reinterpret_cast<wl_display*>(native.display);
                createInfo.surface = reinterpret_cast<wl_surface*>(native.surface);
                return createSurface(VulkanWsiKind::Wayland, &createInfo,
                                     dispatch.createWaylandSurface, "vkCreateWaylandSurfaceKHR");
#else
                return createSurface(VulkanWsiKind::Wayland, nullptr, dispatch.createWaylandSurface,
                                     "vkCreateWaylandSurfaceKHR");
#endif
            }},
        nativeWindowHandle);
}
core::Result<RenderAdapterSelection> SelectVulkanAdapterForSurface(
    const VulkanGlobalDispatch&, std::shared_ptr<VulkanInstanceOwner> instance,
    VkSurfaceKHR surface, const RenderAdapterSelectionDesc& desc)
{
    if (!pond::render::IsValid(desc))
    {
        return core::Result<RenderAdapterSelection>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Render adapter selection descriptor is invalid."));
    }

    if (instance == nullptr || !instance->IsValid())
    {
        return core::Result<RenderAdapterSelection>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Vulkan adapter selection requires a live Vulkan instance owner."));
    }

    const VulkanGlobalDispatch& dispatch = instance->GetDispatch();

    if (surface == VK_NULL_HANDLE)
    {
        return core::Result<RenderAdapterSelection>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Vulkan adapter selection requires a live first prepared surface."));
    }

    if (!HasAdapterDispatch(dispatch))
    {
        return core::Result<RenderAdapterSelection>::FromError(
            MakeRenderError(RenderErrorCode::UnsupportedCapability,
                            "Vulkan physical-device dispatch is unavailable."));
    }

    core::Result<std::vector<VkPhysicalDevice>> physicalDevices =
        EnumeratePhysicalDevices(dispatch, instance->GetHandle());
    if (!physicalDevices)
    {
        return core::Result<RenderAdapterSelection>::FromError(
            std::move(physicalDevices).GetError());
    }

    if (physicalDevices.GetValue().empty())
    {
        return core::Result<RenderAdapterSelection>::FromError(
            MakeRenderError(RenderErrorCode::NoCompatibleAdapter,
                            "No Vulkan physical devices were reported by the selected instance."));
    }

    std::vector<RenderAdapterSnapshot> compatibleAdapters;
    std::vector<RenderAdapterRejection> rejectedAdapters;
    compatibleAdapters.reserve(physicalDevices.GetValue().size());
    rejectedAdapters.reserve(physicalDevices.GetValue().size());

    for (VkPhysicalDevice physicalDevice : physicalDevices.GetValue())
    {
        core::Result<VulkanAdapterCandidate> candidate =
            BuildAdapterCandidate(dispatch, physicalDevice, surface, desc);
        if (!candidate)
        {
            return core::Result<RenderAdapterSelection>::FromError(std::move(candidate).GetError());
        }

        VulkanAdapterCandidate candidateValue = std::move(candidate).GetValue();
        if (candidateValue.compatible)
        {
            compatibleAdapters.push_back(std::move(candidateValue.snapshot));
        }
        else
        {
            rejectedAdapters.push_back(MakeRejection(candidateValue));
        }
    }

    std::ranges::sort(
        compatibleAdapters,
        [preference = desc.adapterPreference](const RenderAdapterSnapshot& lhs,
                                              const RenderAdapterSnapshot& rhs) noexcept
        {
            return IsPreferredAdapterLess(lhs, rhs, preference);
        });

    std::ranges::sort(
        rejectedAdapters,
        [](const RenderAdapterRejection& lhs, const RenderAdapterRejection& rhs) noexcept
        {
            if (lhs.identity.vendorId != rhs.identity.vendorId)
            {
                return lhs.identity.vendorId < rhs.identity.vendorId;
            }
            if (lhs.identity.deviceId != rhs.identity.deviceId)
            {
                return lhs.identity.deviceId < rhs.identity.deviceId;
            }
            if (lhs.identity.name != rhs.identity.name)
            {
                return lhs.identity.name < rhs.identity.name;
            }

            return lhs.adapterId.deviceUuid < rhs.adapterId.deviceUuid;
        });

    if (compatibleAdapters.empty())
    {
        return core::Result<RenderAdapterSelection>::FromError(
            MakeRenderError(RenderErrorCode::NoCompatibleAdapter,
                            MakeNoCompatibleAdapterMessage(rejectedAdapters)));
    }

    const RenderAdapterSnapshot selectedAdapter = compatibleAdapters.front();
    const bool selectedByPreferenceFallback =
        !desc.explicitAdapterId.has_value() &&
        GetAdapterPreferenceRank(selectedAdapter.identity.adapterType, desc.adapterPreference) > 0U;

    return core::Result<RenderAdapterSelection>::FromValue(
        RenderAdapterSelection{.request = desc,
                               .selectedAdapter = selectedAdapter,
                               .compatibleAdapters = std::move(compatibleAdapters),
                               .rejectedAdapters = std::move(rejectedAdapters),
                               .selectedByPreferenceFallback = selectedByPreferenceFallback});
}
core::Result<VulkanDeviceOwner> CreateVulkanDeviceForAdapterSelection(
    const VulkanGlobalDispatch&, std::shared_ptr<VulkanInstanceOwner> instance,
    VkSurfaceKHR surface, const RenderAdapterSelection& selection, const RenderDeviceDesc& desc)
{
    if (!pond::render::IsValid(desc))
    {
        return core::Result<VulkanDeviceOwner>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Render device descriptor is invalid."));
    }

    if (!pond::render::IsValid(selection))
    {
        return core::Result<VulkanDeviceOwner>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Render adapter selection is invalid."));
    }

    if (instance == nullptr || !instance->IsValid())
    {
        return core::Result<VulkanDeviceOwner>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState,
            "Vulkan logical-device creation requires a live Vulkan instance owner."));
    }

    const VulkanGlobalDispatch& dispatch = instance->GetDispatch();

    if (surface == VK_NULL_HANDLE)
    {
        return core::Result<VulkanDeviceOwner>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState,
            "Vulkan logical-device creation requires a live first prepared surface."));
    }

    if (!HasDeviceDispatch(dispatch))
    {
        return core::Result<VulkanDeviceOwner>::FromError(
            MakeRenderError(RenderErrorCode::UnsupportedCapability,
                            "Vulkan logical-device dispatch is unavailable."));
    }

    core::Result<VulkanAdapterCandidate> selectedCandidate =
        FindSelectedAdapterCandidate(dispatch, instance->GetHandle(), surface, selection);
    if (!selectedCandidate)
    {
        return core::Result<VulkanDeviceOwner>::FromError(std::move(selectedCandidate).GetError());
    }

    VulkanAdapterCandidate candidate = std::move(selectedCandidate).GetValue();
    core::Result<VulkanDeviceQueuePlan> queuePlan = MakeDeviceQueuePlan(candidate.queueFamilies);
    if (!queuePlan)
    {
        return core::Result<VulkanDeviceOwner>::FromError(std::move(queuePlan).GetError());
    }

    core::Result<std::vector<VkExtensionProperties>> availableExtensions =
        EnumerateDeviceExtensions(dispatch, candidate.device);
    if (!availableExtensions)
    {
        return core::Result<VulkanDeviceOwner>::FromError(
            std::move(availableExtensions).GetError());
    }

    const VulkanInstanceInfo instanceInfo = instance->GetInfo();
    VulkanDeviceFeatureSelection featureSelection = SelectOptionalDeviceFeatures(
        dispatch, candidate.device, availableExtensions.GetValue(),
        instanceInfo.khrSurfaceMaintenance1Enabled, instanceInfo.extSurfaceMaintenance1Enabled);

    const float queuePriority = 1.0F;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(queuePlan.GetValue().provisionedQueueFamilyIndices.size());
    for (const std::uint32_t familyIndex : queuePlan.GetValue().provisionedQueueFamilyIndices)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = familyIndex;
        queueCreateInfo.queueCount = 1U;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures2 enabledFeatures{};
    enabledFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR enabledMaintenanceFeatures{};
    enabledMaintenanceFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR;

    VkPhysicalDevicePresentIdFeaturesKHR enabledPresentIdFeatures{};
    enabledPresentIdFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;

    VkPhysicalDevicePresentWaitFeaturesKHR enabledPresentWaitFeatures{};
    enabledPresentWaitFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;

    void** createNext = &enabledFeatures.pNext;
    if (featureSelection.optionalCapabilities.swapchainMaintenance1)
    {
        enabledMaintenanceFeatures.swapchainMaintenance1 = VK_TRUE;
        AppendFeatureStruct(createNext, &enabledMaintenanceFeatures);
    }
    if (featureSelection.optionalCapabilities.presentId)
    {
        enabledPresentIdFeatures.presentId = VK_TRUE;
        AppendFeatureStruct(createNext, &enabledPresentIdFeatures);
    }
    if (featureSelection.optionalCapabilities.presentWait)
    {
        enabledPresentWaitFeatures.presentWait = VK_TRUE;
        AppendFeatureStruct(createNext, &enabledPresentWaitFeatures);
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &enabledFeatures;
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount =
        static_cast<std::uint32_t>(featureSelection.enabledExtensions.size());
    createInfo.ppEnabledExtensionNames = featureSelection.enabledExtensions.data();

    VulkanDeviceInfo info{.selectedAdapter = std::move(candidate.snapshot),
                          .queuePlan = std::move(queuePlan).GetValue(),
                          .optionalCapabilities = featureSelection.optionalCapabilities};

    VkDevice device{VK_NULL_HANDLE};
    VkResult result = dispatch.createDevice(candidate.device, &createInfo, nullptr, &device);
    if (result != VK_SUCCESS)
    {
        return core::Result<VulkanDeviceOwner>::FromError(
            MakeVulkanError(MapDeviceFailure(result), "vkCreateDevice", result));
    }

    std::optional<VolkDeviceTable> volkTable;
    VulkanGlobalDispatch deviceDispatch = dispatch;
    if (dispatch.loadDeviceTable != nullptr)
    {
        const PFN_vkGetDeviceProcAddr getDeviceProcAddr = instance->GetDeviceProcAddr();
        if (dispatch.getInstanceProcAddr == nullptr || getDeviceProcAddr == nullptr)
        {
            dispatch.destroyDevice(device, nullptr);
            return core::Result<VulkanDeviceOwner>::FromError(
                MakeRenderError(RenderErrorCode::UnsupportedCapability,
                                "Owner-local Vulkan proc-address dispatch is unavailable."));
        }

        volkTable.emplace();
        dispatch.loadDeviceTable(&*volkTable, getDeviceProcAddr, device);
        deviceDispatch = BindDeviceDispatch(dispatch, *volkTable);
    }
    else
    {
        dispatch.loadDevice(device);
    }

    std::vector<VkQueue> queues;
    std::unique_ptr<VulkanQueueSynchronization> queueSynchronization;
    try
    {
        queues.reserve(info.queuePlan.provisionedQueueFamilyIndices.size());
        for (const std::uint32_t familyIndex : info.queuePlan.provisionedQueueFamilyIndices)
        {
            VkQueue queue{VK_NULL_HANDLE};
            deviceDispatch.getDeviceQueue(device, familyIndex, 0U, &queue);
            if (queue == VK_NULL_HANDLE)
            {
                deviceDispatch.destroyDevice(device, nullptr);
                return core::Result<VulkanDeviceOwner>::FromError(MakeRenderError(
                    RenderErrorCode::BackendFailure,
                    "vkGetDeviceQueue returned a null queue for a provisioned family."));
            }

            queues.push_back(queue);
        }

        queueSynchronization = std::make_unique<VulkanQueueSynchronization>(queues);
    }
    catch (const std::bad_alloc&)
    {
        deviceDispatch.destroyDevice(device, nullptr);
        return core::Result<VulkanDeviceOwner>::FromError(
            MakeRenderError(RenderErrorCode::OutOfMemory,
                            "Vulkan queue synchronization could not allocate host memory."));
    }

    void* allocator{};
    if (deviceDispatch.createOwnerLocalAllocator != nullptr)
    {
        result = deviceDispatch.createOwnerLocalAllocator(
            instance->GetHandle(), candidate.device, device, info.selectedAdapter.apiVersion,
            deviceDispatch.getInstanceProcAddr, instance->GetDeviceProcAddr(), &allocator);
    }
    else
    {
        result = deviceDispatch.createAllocator(instance->GetHandle(), candidate.device, device,
                                                info.selectedAdapter.apiVersion, &allocator);
    }
    if (result != VK_SUCCESS || allocator == nullptr)
    {
        deviceDispatch.destroyDevice(device, nullptr);
        return core::Result<VulkanDeviceOwner>::FromError(
            MakeVulkanError(MapDeviceFailure(result), "vmaCreateAllocator", result));
    }

    info.optionalCapabilities.vmaAllocator = true;

    return core::Result<VulkanDeviceOwner>::FromValue(VulkanDeviceOwner{
        std::move(instance), candidate.device, device, allocator, std::move(queues),
        std::move(queueSynchronization), deviceDispatch.destroyDevice,
        deviceDispatch.deviceWaitIdle, deviceDispatch.queueWaitIdle,
        deviceDispatch.destroyAllocator, deviceDispatch, std::move(volkTable), std::move(info)});
}
core::Result<VulkanDeviceQueuePlan> ValidateVulkanDeviceSurfaceCompatibility(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface)
{
    if (!device.IsValid() || device.GetPhysicalDevice() == VK_NULL_HANDLE)
    {
        return core::Result<VulkanDeviceQueuePlan>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Vulkan target compatibility requires a live logical device."));
    }

    if (surface == VK_NULL_HANDLE)
    {
        return core::Result<VulkanDeviceQueuePlan>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Vulkan target compatibility requires a live prepared surface."));
    }

    if (dispatch.getPhysicalDeviceSurfaceSupport == nullptr)
    {
        return core::Result<VulkanDeviceQueuePlan>::FromError(
            MakeRenderError(RenderErrorCode::UnsupportedCapability,
                            "Vulkan surface presentation support dispatch is unavailable."));
    }

    const VulkanDeviceInfo& info = device.GetInfo();
    if (!IsValidVulkanDeviceQueuePlan(info.queuePlan))
    {
        return core::Result<VulkanDeviceQueuePlan>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState, "Vulkan device queue plan is invalid."));
    }

    const auto queryPresentationSupport = [&](std::uint32_t queueFamilyIndex) -> core::Result<bool>
    {
        VkBool32 supported{VK_FALSE};
        const VkResult result = dispatch.getPhysicalDeviceSurfaceSupport(
            device.GetPhysicalDevice(), queueFamilyIndex, surface, &supported);
        if (result != VK_SUCCESS)
        {
            return core::Result<bool>::FromError(
                MakeVulkanError(MapAdapterQueryFailure(result),
                                "vkGetPhysicalDeviceSurfaceSupportKHR.target", result));
        }

        return core::Result<bool>::FromValue(supported == VK_TRUE);
    };

    VulkanDeviceQueuePlan queuePlan = info.queuePlan;
    core::Result<bool> currentPresentationSupport =
        queryPresentationSupport(queuePlan.presentationQueueFamilyIndex);
    if (!currentPresentationSupport)
    {
        return core::Result<VulkanDeviceQueuePlan>::FromError(
            std::move(currentPresentationSupport).GetError());
    }

    if (!currentPresentationSupport.GetValue())
    {
        bool foundPresentationQueue{};
        for (const std::uint32_t familyIndex : queuePlan.provisionedQueueFamilyIndices)
        {
            core::Result<bool> familySupport = queryPresentationSupport(familyIndex);
            if (!familySupport)
            {
                return core::Result<VulkanDeviceQueuePlan>::FromError(
                    std::move(familySupport).GetError());
            }

            if (familySupport.GetValue())
            {
                queuePlan.presentationQueueFamilyIndex = familyIndex;
                foundPresentationQueue = true;
                break;
            }
        }

        if (!foundPresentationQueue)
        {
            return core::Result<VulkanDeviceQueuePlan>::FromError(MakeRenderError(
                RenderErrorCode::UnsupportedSurface, "Selected Vulkan device has no pre-created "
                                                     "queue that can present to this surface."));
        }
    }

    queuePlan.usesDistinctPresentationQueue =
        queuePlan.graphicsQueueFamilyIndex != queuePlan.presentationQueueFamilyIndex;
    if (!IsValidVulkanDeviceQueuePlan(queuePlan))
    {
        return core::Result<VulkanDeviceQueuePlan>::FromError(
            MakeRenderError(RenderErrorCode::BackendFailure,
                            "Validated target queue plan is internally inconsistent."));
    }

    return core::Result<VulkanDeviceQueuePlan>::FromValue(std::move(queuePlan));
}

core::VoidResult ValidateVulkanSurfaceOutputCompatibility(const VulkanGlobalDispatch& dispatch,
                                                          const VulkanDeviceOwner& device,
                                                          VkSurfaceKHR surface)
{
    if (!device.IsValid() || device.GetPhysicalDevice() == VK_NULL_HANDLE)
    {
        return core::VoidResult::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Vulkan output compatibility requires a live logical device."));
    }

    if (surface == VK_NULL_HANDLE)
    {
        return core::VoidResult::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Vulkan output compatibility requires a live prepared surface."));
    }

    if (dispatch.getPhysicalDeviceSurfaceCapabilities == nullptr ||
        dispatch.getPhysicalDeviceSurfaceFormats == nullptr)
    {
        return core::VoidResult::FromError(
            MakeRenderError(RenderErrorCode::UnsupportedCapability,
                            "Vulkan surface output query dispatch is unavailable."));
    }

    core::Result<VulkanSurfaceOutputSupport> outputSupport =
        QuerySurfaceOutputSupport(dispatch, device.GetPhysicalDevice(), surface);
    if (!outputSupport)
    {
        return core::VoidResult::FromError(std::move(outputSupport).GetError());
    }

    return ValidateRequiredSdrSrgbOutput(outputSupport.GetValue());
}

core::Result<VulkanSwapchainConfig> SelectVulkanSwapchainConfig(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface,
    const VulkanDeviceQueuePlan& queuePlan, const RenderTargetDesc& desc)
{
    if (!device.IsValid() || device.GetPhysicalDevice() == VK_NULL_HANDLE)
    {
        return core::Result<VulkanSwapchainConfig>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Vulkan swapchain selection requires a live logical device."));
    }

    if (surface == VK_NULL_HANDLE)
    {
        return core::Result<VulkanSwapchainConfig>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Vulkan swapchain selection requires a live prepared surface."));
    }

    if (!IsValidVulkanDeviceQueuePlan(queuePlan) || !pond::render::IsValid(desc))
    {
        return core::Result<VulkanSwapchainConfig>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Vulkan swapchain selection inputs are invalid."));
    }

    const RenderTargetSnapshot snapshot = desc.targetSnapshot;
    const platform::PixelSize requestedExtent = snapshot.GetPixelSize();
    if (snapshot.GetWindowState() == platform::WindowState::Minimized || !snapshot.IsVisible() ||
        requestedExtent.width == 0U || requestedExtent.height == 0U)
    {
        return core::Result<VulkanSwapchainConfig>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Suspended render targets do not have Vulkan swapchains."));
    }

    if (!HasSwapchainDispatch(dispatch))
    {
        return core::Result<VulkanSwapchainConfig>::FromError(MakeRenderError(
            RenderErrorCode::UnsupportedCapability, "Vulkan swapchain dispatch is unavailable."));
    }

    core::Result<VulkanSurfaceOutputSupport> outputSupport =
        QuerySurfaceOutputSupport(dispatch, device.GetPhysicalDevice(), surface);
    if (!outputSupport)
    {
        return core::Result<VulkanSwapchainConfig>::FromError(std::move(outputSupport).GetError());
    }

    if (core::VoidResult validOutput = ValidateRequiredSdrSrgbOutput(outputSupport.GetValue());
        !validOutput)
    {
        return core::Result<VulkanSwapchainConfig>::FromError(std::move(validOutput).GetError());
    }

    const VkSurfaceCapabilitiesKHR& surfaceCapabilities = outputSupport.GetValue().capabilities;
    const std::vector<VkSurfaceFormatKHR>& surfaceFormats = outputSupport.GetValue().formats;

    core::Result<std::vector<VkPresentModeKHR>> presentModes =
        QueryPresentModes(dispatch, device.GetPhysicalDevice(), surface);
    if (!presentModes)
    {
        return core::Result<VulkanSwapchainConfig>::FromError(std::move(presentModes).GetError());
    }

    PresentationPolicyFallbackReason policyFallback{};
    core::Result<VkPresentModeKHR> selectedPresentMode =
        SelectPresentMode(desc.presentation, presentModes.GetValue(), policyFallback);
    if (!selectedPresentMode)
    {
        return core::Result<VulkanSwapchainConfig>::FromError(
            std::move(selectedPresentMode).GetError());
    }

    const std::optional<VkSurfaceFormatKHR> selectedFormat = SelectSurfaceFormat(surfaceFormats);
    if (!selectedFormat.has_value())
    {
        return core::Result<VulkanSwapchainConfig>::FromError(
            MakeRenderError(RenderErrorCode::BackendFailure,
                            "Validated Vulkan surface output support could not be selected."));
    }

    const VkExtent2D selectedExtent = SelectSurfaceExtent(surfaceCapabilities, requestedExtent);
    if (selectedExtent.width == 0U || selectedExtent.height == 0U)
    {
        return core::Result<VulkanSwapchainConfig>::FromError(
            MakeRenderError(RenderErrorCode::UnsupportedSurface,
                            "Vulkan surface reported a zero-sized swapchain extent."));
    }

    constexpr VkCompositeAlphaFlagBitsKHR selectedCompositeAlpha =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    constexpr VkSurfaceTransformFlagBitsKHR preferredTransforms[] = {
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR,
        VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR,
        VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR,
        VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR,
        VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR,
        VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR,
        VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR,
        VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR};
    VkSurfaceTransformFlagBitsKHR selectedTransform = surfaceCapabilities.currentTransform;
    if ((surfaceCapabilities.supportedTransforms & selectedTransform) == 0U)
    {
        selectedTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        for (const VkSurfaceTransformFlagBitsKHR candidate : preferredTransforms)
        {
            if ((surfaceCapabilities.supportedTransforms & candidate) != 0U)
            {
                selectedTransform = candidate;
                break;
            }
        }
    }

    QueuedFrameLatency selectedLatency{};
    QueuedFrameLatencyFallbackReason latencyFallback{};
    const std::uint32_t imageCount = SelectSwapchainImageCount(
        surfaceCapabilities, desc.queuedLatency, selectedLatency, latencyFallback);
    if (imageCount < surfaceCapabilities.minImageCount ||
        (surfaceCapabilities.maxImageCount > 0U && imageCount > surfaceCapabilities.maxImageCount))
    {
        return core::Result<VulkanSwapchainConfig>::FromError(
            MakeRenderError(RenderErrorCode::UnsupportedSurface,
                            "Vulkan surface image-count limits are inconsistent."));
    }
    if (desc.queuedLatency.strength == RequirementStrength::Required &&
        latencyFallback != QueuedFrameLatencyFallbackReason::None)
    {
        return core::Result<VulkanSwapchainConfig>::FromError(
            MakeRenderError(RenderErrorCode::UnsupportedSurface,
                            "The required queued-frame latency is unavailable for this target."));
    }

    VulkanSwapchainConfig config{};
    config.windowId = snapshot.GetWindowId();
    config.format = selectedFormat->format;
    config.colorSpace = selectedFormat->colorSpace;
    config.extent = selectedExtent;
    config.compositeAlpha = selectedCompositeAlpha;
    config.preTransform = selectedTransform;
    config.presentMode = selectedPresentMode.GetValue();
    config.imageCount = imageCount;
    config.usesSurfaceCurrentExtent =
        surfaceCapabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max();

    if (queuePlan.usesDistinctPresentationQueue)
    {
        config.sharingMode = VK_SHARING_MODE_CONCURRENT;
        config.sharingQueueFamilyIndices = {queuePlan.graphicsQueueFamilyIndex,
                                            queuePlan.presentationQueueFamilyIndex};
    }
    else
    {
        config.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    config.presentation = SelectedPresentationConfig{
        .requestedPolicy = desc.presentation,
        .actualPolicy = MapPresentMode(config.presentMode).value_or(PresentationPolicy::VSync),
        .policyFallback = policyFallback,
        .requestedQueuedLatency = desc.queuedLatency,
        .actualQueuedLatency = selectedLatency,
        .queuedLatencyFallback = latencyFallback,
        .output = PresentationOutput::OpaqueSdrSrgb,
        .pixelExtent = platform::PixelSize{selectedExtent.width, selectedExtent.height}};

    if (!pond::render::IsValid(config.presentation))
    {
        return core::Result<VulkanSwapchainConfig>::FromError(
            MakeRenderError(RenderErrorCode::BackendFailure,
                            "Selected Vulkan swapchain presentation config is invalid."));
    }

    return core::Result<VulkanSwapchainConfig>::FromValue(std::move(config));
}

core::Result<VulkanSwapchainOwner> CreateVulkanSwapchainForTarget(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface,
    const VulkanDeviceQueuePlan& queuePlan, const RenderTargetDesc& desc)
{
    core::Result<VulkanSwapchainConfig> selectedConfig =
        SelectVulkanSwapchainConfig(dispatch, device, surface, queuePlan, desc);
    if (!selectedConfig)
    {
        return core::Result<VulkanSwapchainOwner>::FromError(std::move(selectedConfig).GetError());
    }

    VulkanSwapchainCreationState creationState;
    return CreateVulkanSwapchainForSelectedConfig(
        dispatch, device, surface, selectedConfig.GetValue(), VK_NULL_HANDLE, creationState);
}

core::Result<VulkanSwapchainOwner> CreateVulkanSwapchainForSelectedConfig(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface,
    const VulkanSwapchainConfig& selectedConfig, VkSwapchainKHR oldSwapchain,
    VulkanSwapchainCreationState& creationState)
{
    creationState = {};
    if (!device.IsValid())
    {
        return core::Result<VulkanSwapchainOwner>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Vulkan swapchain creation requires a live logical device."));
    }

    if (!HasSwapchainDispatch(dispatch) || surface == VK_NULL_HANDLE)
    {
        return core::Result<VulkanSwapchainOwner>::FromError(MakeRenderError(
            RenderErrorCode::UnsupportedCapability, "Vulkan swapchain dispatch is unavailable."));
    }

    const std::optional<PresentationPolicy> selectedPolicy =
        MapPresentMode(selectedConfig.presentMode);
    const bool presentationMetadataMatchesNativeConfig =
        selectedPolicy.has_value() && selectedConfig.presentation.actualPolicy == *selectedPolicy &&
        selectedConfig.presentation.output == PresentationOutput::OpaqueSdrSrgb &&
        selectedConfig.presentation.pixelExtent ==
            platform::PixelSize{selectedConfig.extent.width, selectedConfig.extent.height};

    if (!IsValid(selectedConfig.presentation) || !presentationMetadataMatchesNativeConfig ||
        !IsRequiredSdrSrgbSurfaceFormat(selectedConfig.format, selectedConfig.colorSpace) ||
        selectedConfig.compositeAlpha != VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR ||
        !selectedConfig.windowId.IsValid() || selectedConfig.extent.width == 0U ||
        selectedConfig.extent.height == 0U || selectedConfig.imageCount == 0U)
    {
        return core::Result<VulkanSwapchainOwner>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Selected Vulkan swapchain config is invalid."));
    }

    VulkanSwapchainOwner swapchainOwner;
    try
    {
        VulkanSwapchainConfig config = selectedConfig;
        const std::uint32_t* queueFamilyIndices = config.sharingQueueFamilyIndices.empty()
                                                      ? nullptr
                                                      : config.sharingQueueFamilyIndices.data();

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = config.imageCount;
        createInfo.imageFormat = config.format;
        createInfo.imageColorSpace = config.colorSpace;
        createInfo.imageExtent = config.extent;
        createInfo.imageArrayLayers = 1U;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = config.sharingMode;
        createInfo.queueFamilyIndexCount =
            static_cast<std::uint32_t>(config.sharingQueueFamilyIndices.size());
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
        createInfo.preTransform = config.preTransform;
        createInfo.compositeAlpha = config.compositeAlpha;
        createInfo.presentMode = config.presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = oldSwapchain;

        VkSwapchainKHR swapchain{VK_NULL_HANDLE};
        creationState.nativeCallAttempted = true;
        creationState.oldSwapchainRetired = oldSwapchain != VK_NULL_HANDLE;
        VkResult result =
            dispatch.createSwapchain(device.GetHandle(), &createInfo, nullptr, &swapchain);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            creationState.outcome = VulkanSwapchainCreationOutcome::OutOfDate;
        }
        if (result != VK_SUCCESS)
        {
            return core::Result<VulkanSwapchainOwner>::FromError(
                MakeVulkanError(MapSwapchainFailure(result), "vkCreateSwapchainKHR", result));
        }
        creationState.outcome = VulkanSwapchainCreationOutcome::Created;

        swapchainOwner = VulkanSwapchainOwner{device.GetHandle(),
                                              swapchain,
                                              {},
                                              {},
                                              VK_NULL_HANDLE,
                                              {},
                                              dispatch.destroySwapchain,
                                              dispatch.destroyImageView,
                                              dispatch.destroyRenderPass,
                                              dispatch.destroyFramebuffer,
                                              std::move(config)};

        TryNameObject(
            dispatch, device.GetHandle(), swapchain, VK_OBJECT_TYPE_SWAPCHAIN_KHR,
            MakeTargetDebugObjectName(swapchainOwner.m_config.windowId, "swapchain").c_str());

        std::vector<VkImage>& images = swapchainOwner.m_images;
        for (;;)
        {
            std::uint32_t imageCount{};
            result =
                dispatch.getSwapchainImages(device.GetHandle(), swapchain, &imageCount, nullptr);
            if (result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                creationState.outcome = VulkanSwapchainCreationOutcome::OutOfDate;
            }
            if (result != VK_SUCCESS || imageCount == 0U)
            {
                return core::Result<VulkanSwapchainOwner>::FromError(
                    MakeVulkanError(result == VK_SUCCESS ? RenderErrorCode::BackendFailure
                                                         : MapSwapchainFailure(result),
                                    "vkGetSwapchainImagesKHR.count", result));
            }

            images.resize(imageCount);
            result = dispatch.getSwapchainImages(device.GetHandle(), swapchain, &imageCount,
                                                 images.data());
            if (result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                creationState.outcome = VulkanSwapchainCreationOutcome::OutOfDate;
            }
            if (result == VK_INCOMPLETE)
            {
                images.clear();
                continue;
            }
            if (result != VK_SUCCESS || imageCount == 0U)
            {
                return core::Result<VulkanSwapchainOwner>::FromError(
                    MakeVulkanError(result == VK_SUCCESS ? RenderErrorCode::BackendFailure
                                                         : MapSwapchainFailure(result),
                                    "vkGetSwapchainImagesKHR.read", result));
            }

            images.resize(imageCount);
            break;
        }

        std::vector<VkImageView>& imageViews = swapchainOwner.m_imageViews;
        imageViews.reserve(images.size());
        for (std::size_t imageIndex = 0U; imageIndex < images.size(); ++imageIndex)
        {
            const VkImage image = images[imageIndex];
            VkImageViewCreateInfo imageViewCreateInfo{};
            imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewCreateInfo.image = image;
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            imageViewCreateInfo.format = swapchainOwner.m_config.format;
            imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCreateInfo.subresourceRange.baseMipLevel = 0U;
            imageViewCreateInfo.subresourceRange.levelCount = 1U;
            imageViewCreateInfo.subresourceRange.baseArrayLayer = 0U;
            imageViewCreateInfo.subresourceRange.layerCount = 1U;

            VkImageView imageView{VK_NULL_HANDLE};
            result = dispatch.createImageView(device.GetHandle(), &imageViewCreateInfo, nullptr,
                                              &imageView);
            if (result != VK_SUCCESS)
            {
                return core::Result<VulkanSwapchainOwner>::FromError(
                    MakeVulkanError(MapSwapchainFailure(result), "vkCreateImageView", result));
            }

            imageViews.push_back(imageView);
            TryNameObject(dispatch, device.GetHandle(), image, VK_OBJECT_TYPE_IMAGE,
                          MakeTargetDebugObjectName(swapchainOwner.m_config.windowId,
                                                    "swapchain_image", imageIndex)
                              .c_str());
            TryNameObject(dispatch, device.GetHandle(), imageView, VK_OBJECT_TYPE_IMAGE_VIEW,
                          MakeTargetDebugObjectName(swapchainOwner.m_config.windowId,
                                                    "swapchain_image_view", imageIndex)
                              .c_str());
        }

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchainOwner.m_config.format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentReference{};
        colorAttachmentReference.attachment = 0U;
        colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1U;
        subpass.pColorAttachments = &colorAttachmentReference;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0U;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassCreateInfo{};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = 1U;
        renderPassCreateInfo.pAttachments = &colorAttachment;
        renderPassCreateInfo.subpassCount = 1U;
        renderPassCreateInfo.pSubpasses = &subpass;
        renderPassCreateInfo.dependencyCount = 1U;
        renderPassCreateInfo.pDependencies = &dependency;

        VkRenderPass renderPass{VK_NULL_HANDLE};
        result = dispatch.createRenderPass(device.GetHandle(), &renderPassCreateInfo, nullptr,
                                           &renderPass);
        if (result != VK_SUCCESS)
        {
            return core::Result<VulkanSwapchainOwner>::FromError(
                MakeVulkanError(MapSwapchainFailure(result), "vkCreateRenderPass", result));
        }
        swapchainOwner.m_renderPass = renderPass;

        TryNameObject(
            dispatch, device.GetHandle(), renderPass, VK_OBJECT_TYPE_RENDER_PASS,
            MakeTargetDebugObjectName(swapchainOwner.m_config.windowId, "clear_render_pass")
                .c_str());

        std::vector<VkFramebuffer>& framebuffers = swapchainOwner.m_framebuffers;
        framebuffers.reserve(imageViews.size());
        for (std::size_t imageIndex = 0U; imageIndex < imageViews.size(); ++imageIndex)
        {
            const VkImageView imageView = imageViews[imageIndex];
            VkFramebufferCreateInfo framebufferCreateInfo{};
            framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferCreateInfo.renderPass = renderPass;
            framebufferCreateInfo.attachmentCount = 1U;
            framebufferCreateInfo.pAttachments = &imageView;
            framebufferCreateInfo.width = swapchainOwner.m_config.extent.width;
            framebufferCreateInfo.height = swapchainOwner.m_config.extent.height;
            framebufferCreateInfo.layers = 1U;

            VkFramebuffer framebuffer{VK_NULL_HANDLE};
            result = dispatch.createFramebuffer(device.GetHandle(), &framebufferCreateInfo, nullptr,
                                                &framebuffer);
            if (result != VK_SUCCESS)
            {
                return core::Result<VulkanSwapchainOwner>::FromError(
                    MakeVulkanError(MapSwapchainFailure(result), "vkCreateFramebuffer", result));
            }

            framebuffers.push_back(framebuffer);
            TryNameObject(dispatch, device.GetHandle(), framebuffer, VK_OBJECT_TYPE_FRAMEBUFFER,
                          MakeTargetDebugObjectName(swapchainOwner.m_config.windowId, "framebuffer",
                                                    imageIndex)
                              .c_str());
        }

        return core::Result<VulkanSwapchainOwner>::FromValue(std::move(swapchainOwner));
    }
    catch (const std::bad_alloc&)
    {
        return core::Result<VulkanSwapchainOwner>::FromError(
            MakeRenderError(RenderErrorCode::OutOfMemory,
                            "Vulkan swapchain creation could not allocate host memory."));
    }
    catch (const std::length_error&)
    {
        return core::Result<VulkanSwapchainOwner>::FromError(MakeRenderError(
            RenderErrorCode::BackendFailure,
            "Vulkan swapchain creation received an unrepresentable resource count."));
    }
}

core::Result<VulkanFrameResourcesOwner> CreateVulkanFrameResourcesForTarget(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    platform::WindowId windowId, const VulkanDeviceQueuePlan& queuePlan,
    QueuedFrameLatency queuedLatency, std::uint32_t swapchainImageCount)
{
    if (!device.IsValid())
    {
        return core::Result<VulkanFrameResourcesOwner>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Frame resources require a live Vulkan device."));
    }

    if (!windowId.IsValid() || !IsValidVulkanDeviceQueuePlan(queuePlan) ||
        !pond::render::IsValid(queuedLatency) || swapchainImageCount == 0U)
    {
        return core::Result<VulkanFrameResourcesOwner>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument,
            "Frame resources require valid queue, latency, and swapchain-image state."));
    }

    if (!HasFrameDispatch(dispatch))
    {
        return core::Result<VulkanFrameResourcesOwner>::FromError(MakeRenderError(
            RenderErrorCode::BackendFailure, "Vulkan frame dispatch is incomplete."));
    }

    VulkanFrameResourcesOwner frameResourcesOwner;
    try
    {
        const std::uint32_t slotCount = queuedLatency.frameCount;
        std::vector<VulkanFrameSlotResources> slots(slotCount);
        std::vector<VkSemaphore> renderFinishedSemaphores(swapchainImageCount);
        std::vector<VkCommandBuffer> commandBuffers(slotCount);

        VkCommandPoolCreateInfo commandPoolCreateInfo{};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolCreateInfo.queueFamilyIndex = queuePlan.graphicsQueueFamilyIndex;

        VkCommandPool commandPool{VK_NULL_HANDLE};
        VkResult result = dispatch.createCommandPool(device.GetHandle(), &commandPoolCreateInfo,
                                                     nullptr, &commandPool);
        if (result != VK_SUCCESS)
        {
            return core::Result<VulkanFrameResourcesOwner>::FromError(
                MakeVulkanError(MapFrameFailure(result), "vkCreateCommandPool", result));
        }

        frameResourcesOwner = VulkanFrameResourcesOwner{device.GetHandle(),
                                                        commandPool,
                                                        std::move(slots),
                                                        std::move(renderFinishedSemaphores),
                                                        dispatch.destroyCommandPool,
                                                        dispatch.destroySemaphore,
                                                        dispatch.destroyFence};

        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = slotCount;
        result = dispatch.allocateCommandBuffers(device.GetHandle(), &allocateInfo,
                                                 commandBuffers.data());
        if (result != VK_SUCCESS)
        {
            return core::Result<VulkanFrameResourcesOwner>::FromError(
                MakeVulkanError(MapFrameFailure(result), "vkAllocateCommandBuffers", result));
        }

        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo acquireFenceCreateInfo{};
        acquireFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        VkFenceCreateInfo submissionFenceCreateInfo{};
        submissionFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        submissionFenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (std::uint32_t index = 0U; index < slotCount; ++index)
        {
            VulkanFrameSlotResources& slot = frameResourcesOwner.m_slots[index];
            slot.commandBuffer = commandBuffers[index];

            result = dispatch.createSemaphore(device.GetHandle(), &semaphoreCreateInfo, nullptr,
                                              &slot.imageAvailableSemaphore);
            if (result != VK_SUCCESS)
            {
                return core::Result<VulkanFrameResourcesOwner>::FromError(MakeVulkanError(
                    MapFrameFailure(result), "vkCreateSemaphore.imageAvailable", result));
            }

            result = dispatch.createFence(device.GetHandle(), &acquireFenceCreateInfo, nullptr,
                                          &slot.imageAcquiredFence);
            if (result != VK_SUCCESS)
            {
                return core::Result<VulkanFrameResourcesOwner>::FromError(MakeVulkanError(
                    MapFrameFailure(result), "vkCreateFence.imageAcquired", result));
            }

            result = dispatch.createFence(device.GetHandle(), &submissionFenceCreateInfo, nullptr,
                                          &slot.inFlightFence);
            if (result != VK_SUCCESS)
            {
                return core::Result<VulkanFrameResourcesOwner>::FromError(
                    MakeVulkanError(MapFrameFailure(result), "vkCreateFence.inFlight", result));
            }
        }

        for (VkSemaphore& semaphore : frameResourcesOwner.m_renderFinishedSemaphores)
        {
            result = dispatch.createSemaphore(device.GetHandle(), &semaphoreCreateInfo, nullptr,
                                              &semaphore);
            if (result != VK_SUCCESS)
            {
                return core::Result<VulkanFrameResourcesOwner>::FromError(MakeVulkanError(
                    MapFrameFailure(result), "vkCreateSemaphore.renderFinished", result));
            }
        }

        TryNameObject(dispatch, device.GetHandle(), commandPool, VK_OBJECT_TYPE_COMMAND_POOL,
                      MakeTargetDebugObjectName(windowId, "command_pool").c_str());
        for (std::uint32_t index = 0U; index < slotCount; ++index)
        {
            const VulkanFrameSlotResources& slot = frameResourcesOwner.m_slots[index];
            TryNameObject(dispatch, device.GetHandle(), slot.commandBuffer,
                          VK_OBJECT_TYPE_COMMAND_BUFFER,
                          MakeTargetDebugObjectName(windowId, "command_buffer", index).c_str());
            TryNameObject(dispatch, device.GetHandle(), slot.imageAvailableSemaphore,
                          VK_OBJECT_TYPE_SEMAPHORE,
                          MakeTargetDebugObjectName(windowId, "image_available", index).c_str());
            TryNameObject(dispatch, device.GetHandle(), slot.imageAcquiredFence,
                          VK_OBJECT_TYPE_FENCE,
                          MakeTargetDebugObjectName(windowId, "image_acquired", index).c_str());
            TryNameObject(dispatch, device.GetHandle(), slot.inFlightFence, VK_OBJECT_TYPE_FENCE,
                          MakeTargetDebugObjectName(windowId, "in_flight", index).c_str());
        }
        for (std::uint32_t imageIndex = 0U; imageIndex < swapchainImageCount; ++imageIndex)
        {
            TryNameObject(
                dispatch, device.GetHandle(),
                frameResourcesOwner.m_renderFinishedSemaphores[imageIndex],
                VK_OBJECT_TYPE_SEMAPHORE,
                MakeTargetDebugObjectName(windowId, "render_finished", imageIndex).c_str());
        }

        return core::Result<VulkanFrameResourcesOwner>::FromValue(std::move(frameResourcesOwner));
    }
    catch (const std::bad_alloc&)
    {
        return core::Result<VulkanFrameResourcesOwner>::FromError(
            MakeRenderError(RenderErrorCode::OutOfMemory,
                            "Vulkan frame-resource creation could not allocate host memory."));
    }
    catch (const std::length_error&)
    {
        return core::Result<VulkanFrameResourcesOwner>::FromError(MakeRenderError(
            RenderErrorCode::BackendFailure,
            "Vulkan frame-resource creation received an unrepresentable resource count."));
    }
}

core::Result<VulkanPresentationTrackerOwner> CreateVulkanPresentationTrackerForTarget(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    platform::WindowId windowId, std::uint32_t frameSlotCount, std::uint32_t swapchainImageCount)
{
    if (!device.IsValid())
    {
        return core::Result<VulkanPresentationTrackerOwner>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Presentation tracking requires a live Vulkan device."));
    }

    if (!windowId.IsValid() || frameSlotCount == 0U || swapchainImageCount == 0U)
    {
        return core::Result<VulkanPresentationTrackerOwner>::FromError(
            MakeRenderError(RenderErrorCode::InvalidArgument,
                            "Presentation tracking requires frame slots and swapchain images."));
    }

    const VulkanDeviceOptionalCapabilities capabilities = device.GetInfo().optionalCapabilities;
    const bool usesPresentIds = capabilities.presentId && capabilities.presentWait;
    VulkanPresentationCompletionPath completionPath =
        VulkanPresentationCompletionPath::CoreAcquireHistory;
    if (capabilities.swapchainMaintenance1)
    {
        completionPath = VulkanPresentationCompletionPath::SwapchainMaintenanceFence;
    }
    else if (usesPresentIds)
    {
        completionPath = VulkanPresentationCompletionPath::PresentWait;
    }

    if (completionPath == VulkanPresentationCompletionPath::PresentWait &&
        dispatch.waitForPresent == nullptr)
    {
        return core::Result<VulkanPresentationTrackerOwner>::FromError(MakeRenderError(
            RenderErrorCode::UnsupportedCapability,
            "VK_KHR_present_wait was enabled but vkWaitForPresentKHR is unavailable."));
    }

    if (completionPath == VulkanPresentationCompletionPath::SwapchainMaintenanceFence &&
        (dispatch.createFence == nullptr || dispatch.destroyFence == nullptr ||
         dispatch.waitForFences == nullptr || dispatch.resetFences == nullptr))
    {
        return core::Result<VulkanPresentationTrackerOwner>::FromError(
            MakeRenderError(RenderErrorCode::UnsupportedCapability,
                            "swapchain-maintenance1 present-fence dispatch is unavailable."));
    }

    VulkanPresentationTrackerOwner presentationTrackerOwner;
    try
    {
        std::vector<std::uint64_t> imagePresentationSerials(swapchainImageCount);
        std::vector<std::uint64_t> pendingAcquireSerials(frameSlotCount);
        std::vector<bool> acquireProofPending(frameSlotCount);
        std::vector<VulkanPresentFenceSlot> presentFenceSlots;
        if (completionPath == VulkanPresentationCompletionPath::SwapchainMaintenanceFence)
        {
            presentFenceSlots.resize(frameSlotCount);
        }

        presentationTrackerOwner =
            VulkanPresentationTrackerOwner{device.GetHandle(),
                                           completionPath,
                                           usesPresentIds,
                                           std::move(imagePresentationSerials),
                                           std::move(pendingAcquireSerials),
                                           std::move(acquireProofPending),
                                           std::move(presentFenceSlots),
                                           dispatch.destroyFence};

        if (presentationTrackerOwner.UsesPresentFences())
        {
            VkFenceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            for (std::uint32_t index = 0U; index < frameSlotCount; ++index)
            {
                VulkanPresentFenceSlot& slot = presentationTrackerOwner.m_presentFenceSlots[index];
                const VkResult result =
                    dispatch.createFence(device.GetHandle(), &createInfo, nullptr, &slot.fence);
                if (result != VK_SUCCESS)
                {
                    return core::Result<VulkanPresentationTrackerOwner>::FromError(
                        MakeVulkanError(MapFrameFailure(result), "vkCreateFence.present", result));
                }

                TryNameObject(dispatch, device.GetHandle(), slot.fence, VK_OBJECT_TYPE_FENCE,
                              MakeTargetDebugObjectName(windowId, "present", index).c_str());
            }
        }

        return core::Result<VulkanPresentationTrackerOwner>::FromValue(
            std::move(presentationTrackerOwner));
    }
    catch (const std::bad_alloc&)
    {
        return core::Result<VulkanPresentationTrackerOwner>::FromError(
            MakeRenderError(RenderErrorCode::OutOfMemory,
                            "Vulkan presentation tracking could not allocate host memory."));
    }
    catch (const std::length_error&)
    {
        return core::Result<VulkanPresentationTrackerOwner>::FromError(MakeRenderError(
            RenderErrorCode::BackendFailure,
            "Vulkan presentation tracking received an unrepresentable resource count."));
    }
}

void AbandonVulkanFrame(VulkanFrameResourcesOwner& frameResources,
                        VulkanPresentationTrackerOwner& presentationTracker,
                        VulkanFrameRecordingState& recording) noexcept
{
    if (!recording.IsActive())
    {
        return;
    }

    presentationTracker.AbandonAcquiredFrame(recording.frameSlotIndex);
    frameResources.MarkPoisoned();
    recording.phase = VulkanFrameRecordingPhase::Terminal;
}

core::Result<VulkanFrameBeginResult> BeginVulkanFrame(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    const VulkanSwapchainOwner& swapchain, VulkanFrameResourcesOwner& frameResources,
    VulkanPresentationTrackerOwner& presentationTracker, const VulkanDeviceQueuePlan& queuePlan,
    std::uint32_t frameSlotIndex)
{
    constexpr std::uint64_t kFenceWaitTimeoutNanoseconds = 100'000'000ULL;
    constexpr std::uint64_t kAcquireTimeoutNanoseconds = 16'666'667ULL;

    if (!device.IsValid() || !swapchain.IsValid() || !frameResources.IsValid() ||
        frameResources.IsPoisoned() || !presentationTracker.IsValid())
    {
        return core::Result<VulkanFrameBeginResult>::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Frame begin requires live Vulkan target resources."));
    }

    if (!HasFrameDispatch(dispatch))
    {
        return core::Result<VulkanFrameBeginResult>::FromError(MakeRenderError(
            RenderErrorCode::BackendFailure, "Vulkan frame dispatch is incomplete."));
    }

    if (frameSlotIndex >= frameResources.GetSlotCount())
    {
        return core::Result<VulkanFrameBeginResult>::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Frame begin received an invalid frame slot."));
    }

    const VkQueue graphicsQueue = device.GetQueue(queuePlan.graphicsQueueFamilyIndex);
    const VkQueue presentationQueue = device.GetQueue(queuePlan.presentationQueueFamilyIndex);
    if (graphicsQueue == VK_NULL_HANDLE || presentationQueue == VK_NULL_HANDLE)
    {
        return core::Result<VulkanFrameBeginResult>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Frame begin requires live graphics and presentation queues."));
    }

    core::Result<bool> presentSlotReady = presentationTracker.PrepareFrameSlot(
        dispatch, frameSlotIndex, kFenceWaitTimeoutNanoseconds);
    if (!presentSlotReady)
    {
        return core::Result<VulkanFrameBeginResult>::FromError(
            std::move(presentSlotReady).GetError());
    }
    if (!presentSlotReady.GetValue())
    {
        return core::Result<VulkanFrameBeginResult>::FromValue(
            VulkanFrameBeginResult{.status = FrameStatus::TimedOut});
    }

    core::Result<bool> frameSlotReady =
        frameResources.PrepareFrameSlot(dispatch, frameSlotIndex, kFenceWaitTimeoutNanoseconds);
    if (!frameSlotReady)
    {
        return core::Result<VulkanFrameBeginResult>::FromError(
            std::move(frameSlotReady).GetError());
    }
    if (!frameSlotReady.GetValue())
    {
        return core::Result<VulkanFrameBeginResult>::FromValue(
            VulkanFrameBeginResult{.status = FrameStatus::TimedOut});
    }

    for (const std::uint32_t completedSlot : frameResources.ConsumeCompletedSubmissionSlots())
    {
        presentationTracker.RecordFrameFenceCompletion(completedSlot);
    }

    const VulkanFrameSlotResources slot = frameResources.GetSlot(frameSlotIndex);
    std::uint32_t imageIndex{};
    VkResult result = dispatch.acquireNextImage(
        device.GetHandle(), swapchain.GetHandle(), kAcquireTimeoutNanoseconds,
        slot.imageAvailableSemaphore, slot.imageAcquiredFence, &imageIndex);
    if (result == VK_TIMEOUT || result == VK_NOT_READY)
    {
        return core::Result<VulkanFrameBeginResult>::FromValue(
            VulkanFrameBeginResult{.status = FrameStatus::TimedOut});
    }
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return core::Result<VulkanFrameBeginResult>::FromValue(
            VulkanFrameBeginResult{.status = FrameStatus::RecreationPending});
    }

    const bool suboptimal = result == VK_SUBOPTIMAL_KHR;
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        return core::Result<VulkanFrameBeginResult>::FromError(
            MakeVulkanError(MapFrameFailure(result), "vkAcquireNextImageKHR", result));
    }

    VulkanFrameRecordingState recording{.phase = VulkanFrameRecordingPhase::Recording,
                                        .frameSlotIndex = frameSlotIndex,
                                        .imageIndex = imageIndex,
                                        .suboptimal = suboptimal};
    frameResources.RecordImageAcquired(frameSlotIndex);
    const auto failAcquiredFrame = [&](core::Error error) -> core::Result<VulkanFrameBeginResult>
    {
        AbandonVulkanFrame(frameResources, presentationTracker, recording);
        return core::Result<VulkanFrameBeginResult>::FromError(std::move(error));
    };

    if (imageIndex >= swapchain.GetFramebufferCount())
    {
        return failAcquiredFrame(
            MakeRenderError(RenderErrorCode::BackendFailure,
                            "vkAcquireNextImageKHR returned an invalid image index."));
    }

    if (frameResources.GetRenderFinishedSemaphore(imageIndex) == VK_NULL_HANDLE)
    {
        return failAcquiredFrame(
            MakeRenderError(RenderErrorCode::BackendFailure,
                            "The acquired swapchain image has no render-finished semaphore."));
    }

    presentationTracker.RecordImageAcquired(frameSlotIndex, imageIndex);

    result = dispatch.resetCommandBuffer(slot.commandBuffer, 0U);
    if (result != VK_SUCCESS)
    {
        return failAcquiredFrame(
            MakeVulkanError(MapFrameFailure(result), "vkResetCommandBuffer", result));
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = dispatch.beginCommandBuffer(slot.commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
    {
        return failAcquiredFrame(
            MakeVulkanError(MapFrameFailure(result), "vkBeginCommandBuffer", result));
    }

    return core::Result<VulkanFrameBeginResult>::FromValue(
        VulkanFrameBeginResult{.status = FrameStatus::Ready, .recording = recording});
}

core::VoidResult RecordVulkanFrameClear(const VulkanGlobalDispatch& dispatch,
                                        const VulkanSwapchainOwner& swapchain,
                                        const VulkanFrameResourcesOwner& frameResources,
                                        VulkanFrameRecordingState& recording, ClearColor clearColor)
{
    if (!swapchain.IsValid() || !frameResources.IsValid() || frameResources.IsPoisoned())
    {
        return core::VoidResult::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Frame clear requires live Vulkan target resources."));
    }

    if (!HasFrameDispatch(dispatch))
    {
        return core::VoidResult::FromError(MakeRenderError(RenderErrorCode::BackendFailure,
                                                           "Vulkan frame dispatch is incomplete."));
    }

    if (recording.phase != VulkanFrameRecordingPhase::Recording)
    {
        return core::VoidResult::FromError(MakeRenderError(
            RenderErrorCode::InvalidState,
            "Frame clear requires an acquired command buffer with no recorded clear."));
    }

    if (!IsValid(clearColor))
    {
        return core::VoidResult::FromError(MakeRenderError(
            RenderErrorCode::InvalidArgument, "Frame clear received an invalid clear color."));
    }

    if (recording.frameSlotIndex >= frameResources.GetSlotCount() ||
        recording.imageIndex >= swapchain.GetFramebufferCount())
    {
        return core::VoidResult::FromError(MakeRenderError(
            RenderErrorCode::InvalidState, "Frame clear recording state is invalid."));
    }

    const VulkanFrameSlotResources slot = frameResources.GetSlot(recording.frameSlotIndex);
    VkClearValue clearValue{};
    clearValue.color.float32[0] = clearColor.red;
    clearValue.color.float32[1] = clearColor.green;
    clearValue.color.float32[2] = clearColor.blue;
    clearValue.color.float32[3] = clearColor.alpha;

    const VulkanSwapchainConfig config = swapchain.GetConfig();
    VkRenderPassBeginInfo renderPassBegin{};
    renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBegin.renderPass = swapchain.GetRenderPass();
    renderPassBegin.framebuffer = swapchain.GetFramebuffer(recording.imageIndex);
    renderPassBegin.renderArea.offset = VkOffset2D{.x = 0, .y = 0};
    renderPassBegin.renderArea.extent = config.extent;
    renderPassBegin.clearValueCount = 1U;
    renderPassBegin.pClearValues = &clearValue;

    TryBeginLabel(dispatch, slot.commandBuffer, "pond.render.clear");
    dispatch.cmdBeginRenderPass(slot.commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
    TryEndLabel(dispatch, slot.commandBuffer);
    recording.phase = VulkanFrameRecordingPhase::ClearRecorded;
    return core::VoidResult::Success();
}

core::VoidResult RecordVulkanIntermediateStageMarker(
    const VulkanGlobalDispatch& dispatch, const VulkanFrameResourcesOwner& frameResources,
    VulkanFrameRecordingState& recording)
{
    if (!frameResources.IsValid() || frameResources.IsPoisoned())
    {
        return core::VoidResult::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Intermediate recording requires live Vulkan frame resources."));
    }

    if (recording.phase != VulkanFrameRecordingPhase::ClearRecorded ||
        recording.frameSlotIndex >= frameResources.GetSlotCount())
    {
        return core::VoidResult::FromError(MakeRenderError(
            RenderErrorCode::InvalidState,
            "The intermediate stage must be recorded exactly once after the clear stage."));
    }

    const VulkanFrameSlotResources slot = frameResources.GetSlot(recording.frameSlotIndex);
    TryBeginLabel(dispatch, slot.commandBuffer, "pond.render.intermediate");
    TryEndLabel(dispatch, slot.commandBuffer);
    recording.phase = VulkanFrameRecordingPhase::IntermediateRecorded;
    return core::VoidResult::Success();
}

core::Result<VulkanFramePresentationResult> FinishAndPresentVulkanFrame(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    const VulkanSwapchainOwner& swapchain, VulkanFrameResourcesOwner& frameResources,
    VulkanPresentationTrackerOwner& presentationTracker, const VulkanDeviceQueuePlan& queuePlan,
    VulkanFrameRecordingState& recording)
{
    if (recording.phase != VulkanFrameRecordingPhase::ClearRecorded &&
        recording.phase != VulkanFrameRecordingPhase::IntermediateRecorded)
    {
        return core::Result<VulkanFramePresentationResult>::FromError(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Frame finish requires a clear before submission and presentation."));
    }

    const auto failAcquiredFrame =
        [&](core::Error error) -> core::Result<VulkanFramePresentationResult>
    {
        AbandonVulkanFrame(frameResources, presentationTracker, recording);
        return core::Result<VulkanFramePresentationResult>::FromError(std::move(error));
    };

    if (!device.IsValid() || !swapchain.IsValid() || !frameResources.IsValid() ||
        frameResources.IsPoisoned() || !presentationTracker.IsValid())
    {
        return failAcquiredFrame(MakeRenderError(
            RenderErrorCode::InvalidState, "Frame finish requires live Vulkan target resources."));
    }

    if (!HasFrameDispatch(dispatch))
    {
        return failAcquiredFrame(MakeRenderError(RenderErrorCode::BackendFailure,
                                                 "Vulkan frame dispatch is incomplete."));
    }

    const VkQueue graphicsQueue = device.GetQueue(queuePlan.graphicsQueueFamilyIndex);
    const VkQueue presentationQueue = device.GetQueue(queuePlan.presentationQueueFamilyIndex);
    if (graphicsQueue == VK_NULL_HANDLE || presentationQueue == VK_NULL_HANDLE)
    {
        return failAcquiredFrame(
            MakeRenderError(RenderErrorCode::InvalidState,
                            "Frame finish requires live graphics and presentation queues."));
    }

    if (recording.frameSlotIndex >= frameResources.GetSlotCount() ||
        recording.imageIndex >= swapchain.GetFramebufferCount())
    {
        return failAcquiredFrame(MakeRenderError(RenderErrorCode::InvalidState,
                                                 "Frame finish recording state is invalid."));
    }

    const VulkanFrameSlotResources slot = frameResources.GetSlot(recording.frameSlotIndex);
    const VkSemaphore renderFinishedSemaphore =
        frameResources.GetRenderFinishedSemaphore(recording.imageIndex);
    if (renderFinishedSemaphore == VK_NULL_HANDLE)
    {
        return failAcquiredFrame(
            MakeRenderError(RenderErrorCode::BackendFailure,
                            "The acquired swapchain image has no render-finished semaphore."));
    }

    const std::uint64_t presentId = presentationTracker.ReservePresentId();
    if (presentationTracker.UsesPresentIds() && presentId == 0U)
    {
        return failAcquiredFrame(MakeRenderError(RenderErrorCode::BackendFailure,
                                                 "The Vulkan present-ID sequence was exhausted."));
    }

    dispatch.cmdEndRenderPass(slot.commandBuffer);
    VkResult result = dispatch.endCommandBuffer(slot.commandBuffer);
    if (result != VK_SUCCESS)
    {
        return failAcquiredFrame(
            MakeVulkanError(MapFrameFailure(result), "vkEndCommandBuffer", result));
    }

    result = dispatch.resetFences(device.GetHandle(), 1U, &slot.inFlightFence);
    if (result != VK_SUCCESS)
    {
        return failAcquiredFrame(
            MakeVulkanError(MapFrameFailure(result), "vkResetFences.frame", result));
    }

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1U;
    submitInfo.pWaitSemaphores = &slot.imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1U;
    submitInfo.pCommandBuffers = &slot.commandBuffer;
    submitInfo.signalSemaphoreCount = 1U;
    submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

    {
        [[maybe_unused]] auto graphicsQueueLock = device.LockQueueOperation(graphicsQueue);
        result = dispatch.queueSubmit(graphicsQueue, 1U, &submitInfo, slot.inFlightFence);
    }
    if (result != VK_SUCCESS)
    {
        return failAcquiredFrame(MakeVulkanError(MapFrameFailure(result), "vkQueueSubmit", result));
    }

    recording.phase = VulkanFrameRecordingPhase::Submitted;
    frameResources.RecordSubmissionQueued(recording.frameSlotIndex);
    presentationTracker.RecordAcquireSubmission(recording.frameSlotIndex);

    VkPresentIdKHR presentIdInfo{};
    if (presentId != 0U)
    {
        presentIdInfo.sType = VK_STRUCTURE_TYPE_PRESENT_ID_KHR;
        presentIdInfo.swapchainCount = 1U;
        presentIdInfo.pPresentIds = &presentId;
    }

    const VkFence presentFence = presentationTracker.GetPresentFence(recording.frameSlotIndex);
    VkSwapchainPresentFenceInfoKHR presentFenceInfo{};
    if (presentFence != VK_NULL_HANDLE)
    {
        presentFenceInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR;
        presentFenceInfo.pNext = presentId != 0U ? &presentIdInfo : nullptr;
        presentFenceInfo.swapchainCount = 1U;
        presentFenceInfo.pFences = &presentFence;
    }

    VkSwapchainKHR swapchainHandle = swapchain.GetHandle();
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = presentFence != VK_NULL_HANDLE
                            ? static_cast<const void*>(&presentFenceInfo)
                            : static_cast<const void*>(presentId != 0U ? &presentIdInfo : nullptr);
    presentInfo.waitSemaphoreCount = 1U;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
    presentInfo.swapchainCount = 1U;
    presentInfo.pSwapchains = &swapchainHandle;
    presentInfo.pImageIndices = &recording.imageIndex;

    {
        [[maybe_unused]] auto presentationQueueLock = device.LockQueueOperation(presentationQueue);
        result = dispatch.queuePresent(presentationQueue, &presentInfo);
    }
    presentationTracker.RecordPresentResult(recording.frameSlotIndex, recording.imageIndex,
                                            presentId, result);
    recording.phase = VulkanFrameRecordingPhase::Terminal;
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return core::Result<VulkanFramePresentationResult>::FromValue(
            VulkanFramePresentationResult{.status = FrameStatus::RecreationPending,
                                          .presented = false,
                                          .suboptimal = false,
                                          .imageIndex = recording.imageIndex});
    }

    bool suboptimal = recording.suboptimal;
    if (result == VK_SUBOPTIMAL_KHR)
    {
        suboptimal = true;
    }
    else if (result != VK_SUCCESS)
    {
        if (!WasPresentationOperationEnqueued(result))
        {
            frameResources.MarkPoisoned();
        }
        return core::Result<VulkanFramePresentationResult>::FromError(
            MakeVulkanError(MapFrameFailure(result), "vkQueuePresentKHR", result));
    }

    return core::Result<VulkanFramePresentationResult>::FromValue(VulkanFramePresentationResult{
        .status = suboptimal ? FrameStatus::Suboptimal : FrameStatus::Presented,
        .presented = true,
        .suboptimal = suboptimal,
        .imageIndex = recording.imageIndex});
}
const char* GetVulkanWsiExtensionName(VulkanWsiKind wsiKind) noexcept
{
    switch (wsiKind)
    {
    case VulkanWsiKind::Win32:
        return kWin32SurfaceExtensionName;
    case VulkanWsiKind::X11:
        return kX11SurfaceExtensionName;
    case VulkanWsiKind::Wayland:
        return kWaylandSurfaceExtensionName;
    }

    return "";
}

std::string_view GetVulkanWsiKindName(VulkanWsiKind wsiKind) noexcept
{
    switch (wsiKind)
    {
    case VulkanWsiKind::Win32:
        return "Win32";
    case VulkanWsiKind::X11:
        return "X11";
    case VulkanWsiKind::Wayland:
        return "Wayland";
    }

    return "unknown";
}

VulkanWsiKind GetVulkanWsiKind(const platform::NativeWindowHandle& nativeWindowHandle) noexcept
{
    return std::visit(Overloaded{[](platform::NativeWin32Window) noexcept
                                 {
                                     return VulkanWsiKind::Win32;
                                 },
                                 [](platform::NativeX11Window) noexcept
                                 {
                                     return VulkanWsiKind::X11;
                                 },
                                 [](platform::NativeWaylandWindow) noexcept
                                 {
                                     return VulkanWsiKind::Wayland;
                                 }},
                      nativeWindowHandle);
}
std::uint32_t GetMinimumVulkanInstanceVersion() noexcept
{
    return VK_API_VERSION_1_2;
}

std::uint32_t GetMaximumHeaderVulkanInstanceVersion() noexcept
{
#if defined(VK_HEADER_VERSION_COMPLETE)
    return VK_HEADER_VERSION_COMPLETE;
#else
    return VK_API_VERSION_1_2;
#endif
}

bool IsDefaultValidationEnabledForDeveloperBuild() noexcept
{
    return PONDER_RENDER_ENABLE_VALIDATION != 0 && PONDER_RENDER_DEVELOPER_BUILD != 0;
}

VKAPI_ATTR VkBool32 VKAPI_CALL HandleVulkanDebugUtilsMessage(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) noexcept
{
    (void)messageTypes;

    auto* context = static_cast<VulkanDebugMessengerContext*>(userData);
    if (context != nullptr && IsFiltered(*context, callbackData))
    {
        return VK_FALSE;
    }

    if (context != nullptr && context->failOnWarningOrError && IsWarningOrError(messageSeverity))
    {
        context->warningOrErrorSeen.store(true, std::memory_order_relaxed);
        CaptureValidationMessage(*context, messageSeverity, callbackData);
    }

    try
    {
        std::string message;
        message.reserve(256U);
        message += "operation=";
        message += context != nullptr ? context->currentOperation : "unknown";
        message += " ";
        AppendCallbackMessage(message, callbackData);

        const core::LogLevel level = MapDebugSeverity(messageSeverity);
        const std::string_view category =
            IsWarningOrError(messageSeverity) ? kValidationLogCategory : kDebugLogCategory;
        core::LogMessage(level, category, message);
    }
    catch (...)
    {
    }

    return VK_FALSE;
}
} // namespace pond::render::detail
