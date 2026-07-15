#include <ponder/core/Log.hpp>
#include <ponder/platform/NativeWindow.hpp>
#include <ponder/render/RenderError.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <semaphore>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "VulkanBootstrap.hpp"
#include "VulkanDiagnostics.hpp"

namespace
{
static_assert(std::is_same_v<
              decltype(std::declval<const pond::render::detail::VulkanDeviceOwner&>().GetInfo()),
              const pond::render::detail::VulkanDeviceInfo&>);
static_assert(
    std::is_same_v<
        decltype(std::declval<const pond::render::detail::VulkanSwapchainOwner&>().GetConfig()),
        const pond::render::detail::VulkanSwapchainConfig&>);

struct FakePhysicalDevice final
{
    std::uintptr_t handleValue{};
    std::uint32_t apiVersion{VK_API_VERSION_1_2};
    std::uint32_t vendorId{};
    std::uint32_t deviceId{};
    VkPhysicalDeviceType deviceType{VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU};
    std::string deviceName{};
    std::array<std::uint8_t, VK_UUID_SIZE> deviceUuid{};
    std::array<std::uint8_t, VK_LUID_SIZE> deviceLuid{};
    std::uint32_t deviceNodeMask{};
    bool deviceLuidValid{};
    std::uint32_t driverVersion{1U};
    std::string driverName{"fake-driver"};
    std::string driverInfo{"fake-driver-info"};
    std::vector<VkMemoryHeap> memoryHeaps{};
    std::vector<VkQueueFamilyProperties> queueFamilies{};
    std::vector<VkBool32> surfaceSupport{};
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    std::vector<VkSurfaceFormatKHR> surfaceFormats{};
    std::vector<VkPresentModeKHR> presentModes{};
    std::vector<std::string> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    bool swapchainMaintenance1Feature{};
    bool presentIdFeature{};
    bool presentWaitFeature{};
};

struct FakeVulkanState final
{
    VkResult initializeResult{VK_SUCCESS};
    VkResult enumerateVersionResult{VK_SUCCESS};
    VkResult enumerateExtensionsResult{VK_SUCCESS};
    VkResult enumerateExtensionsReadResult{VK_SUCCESS};
    VkResult enumerateLayersResult{VK_SUCCESS};
    VkResult enumerateLayersReadResult{VK_SUCCESS};
    VkResult createInstanceResult{VK_SUCCESS};
    VkResult createDebugMessengerResult{VK_SUCCESS};
    VkResult createWin32SurfaceResult{VK_SUCCESS};
    VkResult createX11SurfaceResult{VK_SUCCESS};
    VkResult createWaylandSurfaceResult{VK_SUCCESS};
    VkResult enumeratePhysicalDevicesResult{VK_SUCCESS};
    VkResult enumeratePhysicalDevicesReadResult{VK_SUCCESS};
    VkResult getSurfaceSupportResult{VK_SUCCESS};
    VkResult getSurfaceFormatsResult{VK_SUCCESS};
    VkResult getSurfaceFormatsReadResult{VK_SUCCESS};
    VkResult getPresentModesResult{VK_SUCCESS};
    VkResult getPresentModesReadResult{VK_SUCCESS};
    VkResult getSurfaceCapabilitiesResult{VK_SUCCESS};
    std::uint32_t surfaceFormatIncompleteReadsRemaining{};
    std::uint32_t presentModeIncompleteReadsRemaining{};
    VkResult enumerateDeviceExtensionsResult{VK_SUCCESS};
    VkResult enumerateDeviceExtensionsReadResult{VK_SUCCESS};
    VkResult createDeviceResult{VK_SUCCESS};
    VkResult deviceWaitIdleResult{VK_SUCCESS};
    VkResult queueWaitIdleResult{VK_SUCCESS};
    VkResult createSwapchainResult{VK_SUCCESS};
    VkResult getSwapchainImagesResult{VK_SUCCESS};
    VkResult getSwapchainImagesReadResult{VK_SUCCESS};
    bool throwBadAllocOnSwapchainImageRead{};
    VkResult createImageViewResult{VK_SUCCESS};
    VkResult createRenderPassResult{VK_SUCCESS};
    VkResult createFramebufferResult{VK_SUCCESS};
    VkResult createCommandPoolResult{VK_SUCCESS};
    VkResult allocateCommandBuffersResult{VK_SUCCESS};
    VkResult resetCommandBufferResult{VK_SUCCESS};
    VkResult beginCommandBufferResult{VK_SUCCESS};
    VkResult endCommandBufferResult{VK_SUCCESS};
    VkResult createSemaphoreResult{VK_SUCCESS};
    VkResult createFenceResult{VK_SUCCESS};
    VkResult waitForFencesResult{VK_SUCCESS};
    VkResult resetFencesResult{VK_SUCCESS};
    VkResult acquireNextImageResult{VK_SUCCESS};
    VkResult queueSubmitResult{VK_SUCCESS};
    VkResult queuePresentResult{VK_SUCCESS};
    VkResult waitForPresentResult{VK_SUCCESS};
    VkResult setDebugNameResult{VK_SUCCESS};
    VkResult createAllocatorResult{VK_SUCCESS};
    std::uint32_t loaderVersion{VK_API_VERSION_1_2};
    std::vector<std::string> extensions{
        "VK_KHR_surface",
        pond::render::detail::GetVulkanWsiExtensionName(pond::render::detail::VulkanWsiKind::Win32),
        pond::render::detail::GetVulkanWsiExtensionName(pond::render::detail::VulkanWsiKind::X11),
        pond::render::detail::GetVulkanWsiExtensionName(
            pond::render::detail::VulkanWsiKind::Wayland)};
    std::vector<std::string> layers;
    std::vector<std::string> lastEnabledExtensions;
    std::vector<std::string> lastEnabledLayers;
    std::vector<std::string> lastDeviceEnabledExtensions;
    std::vector<std::uint32_t> lastDeviceQueueFamilyIndices;
    std::vector<VkValidationFeatureEnableEXT> lastEnabledValidationFeatures;
    std::uint32_t lastRequestedApiVersion{};
    bool debugCreateInfoChained{};
    bool validationFeaturesChained{};
    bool swapchainMaintenance1FeaturesChained{};
    bool presentIdFeaturesChained{};
    bool presentWaitFeaturesChained{};
    bool lastEnabledSwapchainMaintenance1Feature{};
    bool lastEnabledPresentIdFeature{};
    bool lastEnabledPresentWaitFeature{};
    VkExtent2D lastSwapchainExtent{};
    VkFormat lastSwapchainFormat{VK_FORMAT_UNDEFINED};
    VkColorSpaceKHR lastSwapchainColorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkPresentModeKHR lastSwapchainPresentMode{VK_PRESENT_MODE_FIFO_KHR};
    VkCompositeAlphaFlagBitsKHR lastSwapchainCompositeAlpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR};
    VkSurfaceTransformFlagBitsKHR lastSwapchainPreTransform{VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR};
    VkSharingMode lastSwapchainSharingMode{VK_SHARING_MODE_EXCLUSIVE};
    std::uint32_t lastSwapchainMinImageCount{};
    std::vector<std::uint32_t> lastSwapchainQueueFamilyIndices{};
    VkSwapchainKHR lastOldSwapchain{VK_NULL_HANDLE};
    std::uint32_t lastWaitForFencesFenceCount{};
    VkBool32 lastWaitForFencesWaitAll{};
    std::uint64_t lastWaitForFencesTimeout{};
    std::vector<VkFence> lastWaitForFences{};
    VkQueue lastQueueWaitIdle{VK_NULL_HANDLE};
    VkDevice lastWaitForPresentDevice{VK_NULL_HANDLE};
    VkSwapchainKHR lastWaitForPresentSwapchain{VK_NULL_HANDLE};
    std::uint64_t lastWaitForPresentId{};
    std::uint64_t lastWaitForPresentTimeout{};
    std::vector<VkFence> presentedFences{};
    std::vector<std::uint64_t> presentedIds{};
    std::vector<VkSemaphore> acquiredSemaphores{};
    std::vector<VkFence> acquiredFences{};
    std::vector<VkFence> submittedFences{};
    std::vector<VkSemaphore> submittedWaitSemaphores{};
    std::vector<VkSemaphore> submittedSignalSemaphores{};
    std::vector<VkSemaphore> presentWaitSemaphores{};
    std::vector<VkQueue> presentationQueues{};
    std::vector<std::string> frameTrace{};
    std::vector<std::string> debugObjectNames{};
    VkDebugUtilsMessageSeverityFlagsEXT lastDebugMessageSeverity{};
    VkDebugUtilsMessageTypeFlagsEXT lastDebugMessageType{};
    PFN_vkDebugUtilsMessengerCallbackEXT lastCallback{};
    void* lastCallbackUserData{};
    std::uint32_t initializeCalls{};
    std::uint32_t enumerateVersionCalls{};
    std::uint32_t enumerateExtensionsCalls{};
    std::uint32_t enumerateLayersCalls{};
    std::uint32_t createInstanceCalls{};
    std::uint32_t destroyInstanceCalls{};
    std::uint32_t loadInstanceCalls{};
    std::uint32_t loadInstanceTableCalls{};
    std::uint32_t ownerLocalInstanceADestroyCalls{};
    std::uint32_t ownerLocalInstanceBDestroyCalls{};
    std::uint32_t createDebugMessengerCalls{};
    std::uint32_t destroyDebugMessengerCalls{};
    std::uint32_t createWin32SurfaceCalls{};
    std::uint32_t createX11SurfaceCalls{};
    std::uint32_t createWaylandSurfaceCalls{};
    std::uint32_t destroySurfaceCalls{};
    std::uint32_t enumeratePhysicalDevicesCalls{};
    std::uint32_t getPhysicalDeviceProperties2Calls{};
    std::uint32_t getPhysicalDeviceMemoryPropertiesCalls{};
    std::uint32_t getPhysicalDeviceQueueFamilyPropertiesCalls{};
    std::uint32_t getSurfaceSupportCalls{};
    std::uint32_t getSurfaceFormatsCalls{};
    std::uint32_t getPresentModesCalls{};
    std::uint32_t getSurfaceCapabilitiesCalls{};
    std::uint32_t enumerateDeviceExtensionsCalls{};
    std::uint32_t getPhysicalDeviceFeatures2Calls{};
    std::uint32_t createDeviceCalls{};
    std::uint32_t destroyDeviceCalls{};
    std::uint32_t loadDeviceCalls{};
    std::uint32_t loadDeviceTableCalls{};
    std::uint32_t ownerLocalDeviceADestroyCalls{};
    std::uint32_t ownerLocalDeviceBDestroyCalls{};
    std::uint32_t getDeviceQueueCalls{};
    std::uint32_t deviceWaitIdleCalls{};
    std::uint32_t queueWaitIdleCalls{};
    std::uint32_t createSwapchainCalls{};
    std::uint32_t destroySwapchainCalls{};
    std::uint32_t getSwapchainImagesCalls{};
    std::uint32_t createImageViewCalls{};
    std::uint32_t destroyImageViewCalls{};
    std::uint32_t createRenderPassCalls{};
    std::uint32_t destroyRenderPassCalls{};
    std::uint32_t createFramebufferCalls{};
    std::uint32_t destroyFramebufferCalls{};
    std::uint32_t createCommandPoolCalls{};
    std::uint32_t destroyCommandPoolCalls{};
    std::uint32_t allocateCommandBuffersCalls{};
    std::uint32_t resetCommandBufferCalls{};
    std::uint32_t beginCommandBufferCalls{};
    std::uint32_t endCommandBufferCalls{};
    std::uint32_t cmdBeginRenderPassCalls{};
    std::uint32_t cmdEndRenderPassCalls{};
    std::uint32_t createSemaphoreCalls{};
    std::uint32_t destroySemaphoreCalls{};
    std::uint32_t createFenceCalls{};
    std::uint32_t destroyFenceCalls{};
    std::uint32_t waitForFencesCalls{};
    std::uint32_t resetFencesCalls{};
    std::uint32_t acquireNextImageCalls{};
    std::uint32_t queueSubmitCalls{};
    std::uint32_t queuePresentCalls{};
    std::uint32_t waitForPresentCalls{};
    std::uint32_t setDebugNameCalls{};
    std::uint32_t beginLabelCalls{};
    std::uint32_t endLabelCalls{};
    std::uint32_t createAllocatorCalls{};
    std::uint32_t createOwnerLocalAllocatorCalls{};
    std::uint32_t destroyAllocatorCalls{};
    PFN_vkGetInstanceProcAddr lastAllocatorGetInstanceProcAddr{};
    PFN_vkGetDeviceProcAddr lastAllocatorGetDeviceProcAddr{};
    pond::render::detail::VulkanWsiKind lastCreatedSurfaceWsiKind{
        pond::render::detail::VulkanWsiKind::Win32};
    std::vector<FakePhysicalDevice> physicalDevices{};
    std::uintptr_t nextInstanceValue{0x1000U};
    std::uintptr_t nextDebugMessengerValue{0x2000U};
    std::uintptr_t nextSurfaceValue{0x3000U};
    std::uintptr_t nextDeviceValue{0x7000U};
    std::uintptr_t nextQueueValue{0x8000U};
    std::uintptr_t nextAllocatorValue{0x9000U};
    std::uintptr_t nextSwapchainValue{0xA000U};
    std::uintptr_t nextImageValue{0xB000U};
    std::uintptr_t nextImageViewValue{0xC000U};
    std::uintptr_t nextRenderPassValue{0xD000U};
    std::uintptr_t nextFramebufferValue{0xE000U};
    std::uintptr_t nextCommandPoolValue{0xF000U};
    std::uintptr_t nextCommandBufferValue{0x10000U};
    std::uintptr_t nextSemaphoreValue{0x11000U};
    std::uintptr_t nextFenceValue{0x12000U};
    std::uint32_t swapchainImageCount{3U};
    std::uint32_t swapchainImageCountAfterFirstQuery{};
    std::uint32_t acquiredImageIndex{};
    std::atomic<bool> notifyNextResetFences{};
    std::binary_semaphore resetFencesReached{0};
};

struct ValidationFeatureCase final
{
    pond::render::RenderValidationMode mode{pond::render::RenderValidationMode::Default};
    VkValidationFeatureEnableEXT feature{VK_VALIDATION_FEATURE_ENABLE_MAX_ENUM_EXT};
};

FakeVulkanState* g_fakeVulkanState{};
std::vector<pond::core::LogEntry> g_capturedLogEntries;

template <typename HandleType>
[[nodiscard]] HandleType MakeFakeHandle(std::uintptr_t value) noexcept
{
    if constexpr (std::is_pointer_v<HandleType>)
    {
        return reinterpret_cast<HandleType>(value);
    }
    else
    {
        return static_cast<HandleType>(value);
    }
}

void AddValidationSupport(FakeVulkanState& state)
{
    state.layers.push_back("VK_LAYER_KHRONOS_validation");
    state.extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
}

void AddValidationFeaturesSupport(FakeVulkanState& state)
{
    state.extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
}
void AddSurfaceMaintenanceSupport(FakeVulkanState& state)
{
    state.extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
    state.extensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
}

[[nodiscard]] bool ContainsString(const std::vector<std::string>& values,
                                  std::string_view expected) noexcept
{
    return std::ranges::any_of(values,
                               [expected](std::string_view value) noexcept
                               {
                                   return value == expected;
                               });
}

[[nodiscard]] bool ContainsValidationFeature(
    const std::vector<VkValidationFeatureEnableEXT>& values,
    VkValidationFeatureEnableEXT expected) noexcept
{
    return std::ranges::find(values, expected) != values.end();
}

void CaptureLogEntry(const pond::core::LogEntry& entry)
{
    g_capturedLogEntries.push_back(entry);
}

void CopyStringToFixedArray(std::string_view source, char* destination,
                            std::size_t capacity) noexcept
{
    if (capacity == 0U)
    {
        return;
    }

    const std::size_t copyCount = std::min(source.size(), capacity - 1U);
    std::copy_n(source.data(), copyCount, destination);
    destination[copyCount] = '\0';
}

[[nodiscard]] FakePhysicalDevice MakeCompatibleFakeDevice(std::uintptr_t handleValue,
                                                          std::uint8_t uuidSeed,
                                                          VkPhysicalDeviceType type,
                                                          std::string_view name)
{
    FakePhysicalDevice device;
    device.handleValue = handleValue;
    device.vendorId = 0x1000U + uuidSeed;
    device.deviceId = 0x2000U + uuidSeed;
    device.deviceType = type;
    device.deviceName = std::string{name};
    device.deviceUuid[0] = uuidSeed;
    device.deviceUuid[1] = static_cast<std::uint8_t>(uuidSeed + 1U);
    device.driverVersion = 0x3000U + uuidSeed;
    device.memoryHeaps.push_back(
        VkMemoryHeap{.size = 512U * 1024U * 1024U, .flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT});
    device.queueFamilies.push_back(
        VkQueueFamilyProperties{.queueFlags = VK_QUEUE_GRAPHICS_BIT, .queueCount = 1U});
    device.surfaceSupport.push_back(VK_TRUE);
    device.surfaceCapabilities.minImageCount = 2U;
    device.surfaceCapabilities.maxImageCount = 0U;
    device.surfaceCapabilities.currentExtent =
        VkExtent2D{.width = std::numeric_limits<std::uint32_t>::max(),
                   .height = std::numeric_limits<std::uint32_t>::max()};
    device.surfaceCapabilities.minImageExtent = VkExtent2D{.width = 1U, .height = 1U};
    device.surfaceCapabilities.maxImageExtent = VkExtent2D{.width = 4096U, .height = 4096U};
    device.surfaceCapabilities.supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    device.surfaceCapabilities.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    device.surfaceCapabilities.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    device.surfaceCapabilities.supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    device.surfaceFormats.push_back(VkSurfaceFormatKHR{
        .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
    device.presentModes.push_back(VK_PRESENT_MODE_FIFO_KHR);
    device.presentModes.push_back(VK_PRESENT_MODE_MAILBOX_KHR);
    return device;
}

[[nodiscard]] FakePhysicalDevice* FindFakePhysicalDevice(VkPhysicalDevice handle) noexcept
{
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(handle);
    for (FakePhysicalDevice& device : g_fakeVulkanState->physicalDevices)
    {
        if (device.handleValue == value)
        {
            return &device;
        }
    }

    return nullptr;
}

[[nodiscard]] VkResult FakeInitialize() noexcept
{
    ++g_fakeVulkanState->initializeCalls;
    return g_fakeVulkanState->initializeResult;
}

[[nodiscard]] VkResult FakeEnumerateInstanceVersion(std::uint32_t* version) noexcept
{
    ++g_fakeVulkanState->enumerateVersionCalls;
    if (version != nullptr)
    {
        *version = g_fakeVulkanState->loaderVersion;
    }

    return g_fakeVulkanState->enumerateVersionResult;
}

[[nodiscard]] VkResult FakeEnumerateInstanceExtensions(const char* layerName,
                                                       std::uint32_t* propertyCount,
                                                       VkExtensionProperties* properties) noexcept
{
    (void)layerName;

    ++g_fakeVulkanState->enumerateExtensionsCalls;
    if (propertyCount == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (g_fakeVulkanState->enumerateExtensionsResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->enumerateExtensionsResult;
    }

    if (properties == nullptr)
    {
        *propertyCount = static_cast<std::uint32_t>(g_fakeVulkanState->extensions.size());
        return VK_SUCCESS;
    }
    if (g_fakeVulkanState->enumerateExtensionsReadResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->enumerateExtensionsReadResult;
    }

    const std::uint32_t writableCount =
        std::min(*propertyCount, static_cast<std::uint32_t>(g_fakeVulkanState->extensions.size()));
    for (std::uint32_t index = 0; index < writableCount; ++index)
    {
        properties[index] = {};
        const std::string& extension = g_fakeVulkanState->extensions[index];
        const std::size_t copyCount =
            std::min(extension.size(), sizeof(properties[index].extensionName) - 1U);
        std::copy_n(extension.data(), copyCount, properties[index].extensionName);
        properties[index].extensionName[copyCount] = '\0';
    }

    *propertyCount = writableCount;
    return writableCount == g_fakeVulkanState->extensions.size() ? VK_SUCCESS : VK_INCOMPLETE;
}

[[nodiscard]] VkResult FakeEnumerateInstanceLayers(std::uint32_t* propertyCount,
                                                   VkLayerProperties* properties) noexcept
{
    ++g_fakeVulkanState->enumerateLayersCalls;
    if (propertyCount == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (g_fakeVulkanState->enumerateLayersResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->enumerateLayersResult;
    }

    if (properties == nullptr)
    {
        *propertyCount = static_cast<std::uint32_t>(g_fakeVulkanState->layers.size());
        return VK_SUCCESS;
    }
    if (g_fakeVulkanState->enumerateLayersReadResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->enumerateLayersReadResult;
    }

    const std::uint32_t writableCount =
        std::min(*propertyCount, static_cast<std::uint32_t>(g_fakeVulkanState->layers.size()));
    for (std::uint32_t index = 0; index < writableCount; ++index)
    {
        properties[index] = {};
        CopyStringToFixedArray(g_fakeVulkanState->layers[index], properties[index].layerName,
                               sizeof(properties[index].layerName));
    }

    *propertyCount = writableCount;
    return writableCount == g_fakeVulkanState->layers.size() ? VK_SUCCESS : VK_INCOMPLETE;
}

void CaptureDebugMessengerCreateInfo(const VkDebugUtilsMessengerCreateInfoEXT& createInfo) noexcept
{
    g_fakeVulkanState->lastDebugMessageSeverity = createInfo.messageSeverity;
    g_fakeVulkanState->lastDebugMessageType = createInfo.messageType;
    g_fakeVulkanState->lastCallback = createInfo.pfnUserCallback;
    g_fakeVulkanState->lastCallbackUserData = createInfo.pUserData;
}

void CaptureValidationFeatures(const VkValidationFeaturesEXT& validationFeatures)
{
    g_fakeVulkanState->lastEnabledValidationFeatures.clear();
    if (validationFeatures.enabledValidationFeatureCount == 0U ||
        validationFeatures.pEnabledValidationFeatures == nullptr)
    {
        return;
    }

    const VkValidationFeatureEnableEXT* const begin = validationFeatures.pEnabledValidationFeatures;
    const VkValidationFeatureEnableEXT* const end =
        begin + validationFeatures.enabledValidationFeatureCount;
    g_fakeVulkanState->lastEnabledValidationFeatures.assign(begin, end);
}

void CaptureCreateInfoChain(const VkInstanceCreateInfo& createInfo)
{
    const VkBaseInStructure* current = static_cast<const VkBaseInStructure*>(createInfo.pNext);
    while (current != nullptr)
    {
        switch (current->sType)
        {
        case VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT:
        {
            g_fakeVulkanState->debugCreateInfoChained = true;
            const auto* debugCreateInfo =
                reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(current);
            CaptureDebugMessengerCreateInfo(*debugCreateInfo);
            break;
        }
        case VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT:
        {
            g_fakeVulkanState->validationFeaturesChained = true;
            const auto* validationFeatures =
                reinterpret_cast<const VkValidationFeaturesEXT*>(current);
            CaptureValidationFeatures(*validationFeatures);
            break;
        }
        default:
            break;
        }

        current = current->pNext;
    }
}
[[nodiscard]] VkResult FakeCreateInstance(const VkInstanceCreateInfo* createInfo,
                                          const VkAllocationCallbacks* allocator,
                                          VkInstance* instance) noexcept
{
    (void)allocator;

    ++g_fakeVulkanState->createInstanceCalls;
    g_fakeVulkanState->lastEnabledExtensions.clear();
    g_fakeVulkanState->lastEnabledLayers.clear();
    g_fakeVulkanState->lastEnabledValidationFeatures.clear();
    g_fakeVulkanState->debugCreateInfoChained = false;
    g_fakeVulkanState->validationFeaturesChained = false;

    if (createInfo != nullptr && createInfo->pApplicationInfo != nullptr)
    {
        g_fakeVulkanState->lastRequestedApiVersion = createInfo->pApplicationInfo->apiVersion;
    }

    if (createInfo != nullptr && createInfo->ppEnabledExtensionNames != nullptr)
    {
        const char* const* const begin = createInfo->ppEnabledExtensionNames;
        const char* const* const end = begin + createInfo->enabledExtensionCount;
        g_fakeVulkanState->lastEnabledExtensions.assign(begin, end);
    }

    if (createInfo != nullptr && createInfo->ppEnabledLayerNames != nullptr)
    {
        const char* const* const begin = createInfo->ppEnabledLayerNames;
        const char* const* const end = begin + createInfo->enabledLayerCount;
        g_fakeVulkanState->lastEnabledLayers.assign(begin, end);
    }

    if (createInfo != nullptr)
    {
        CaptureCreateInfoChain(*createInfo);
    }

    if (g_fakeVulkanState->createInstanceResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createInstanceResult;
    }

    *instance = MakeFakeHandle<VkInstance>(g_fakeVulkanState->nextInstanceValue);
    ++g_fakeVulkanState->nextInstanceValue;
    return VK_SUCCESS;
}

void FakeDestroyInstance(VkInstance instance, const VkAllocationCallbacks* allocator) noexcept
{
    (void)instance;
    (void)allocator;
    ++g_fakeVulkanState->destroyInstanceCalls;
}

void FakeLoadInstanceOnly(VkInstance instance) noexcept
{
    (void)instance;
    ++g_fakeVulkanState->loadInstanceCalls;
}

[[nodiscard]] VkResult FakeCreateDebugUtilsMessenger(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* messenger) noexcept
{
    (void)instance;
    (void)allocator;

    ++g_fakeVulkanState->createDebugMessengerCalls;
    if (createInfo != nullptr)
    {
        CaptureDebugMessengerCreateInfo(*createInfo);
    }

    if (messenger == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (g_fakeVulkanState->createDebugMessengerResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createDebugMessengerResult;
    }

    *messenger =
        MakeFakeHandle<VkDebugUtilsMessengerEXT>(g_fakeVulkanState->nextDebugMessengerValue);
    ++g_fakeVulkanState->nextDebugMessengerValue;
    return VK_SUCCESS;
}

void FakeDestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                    const VkAllocationCallbacks* allocator) noexcept
{
    (void)instance;
    (void)messenger;
    (void)allocator;
    ++g_fakeVulkanState->destroyDebugMessengerCalls;
}

[[nodiscard]] VkResult FakeCreateWin32Surface(VkInstance instance, const void* createInfo,
                                              const VkAllocationCallbacks* allocator,
                                              VkSurfaceKHR* surface) noexcept
{
    (void)instance;
    (void)createInfo;
    (void)allocator;
    ++g_fakeVulkanState->createWin32SurfaceCalls;
    g_fakeVulkanState->lastCreatedSurfaceWsiKind = pond::render::detail::VulkanWsiKind::Win32;
    if (g_fakeVulkanState->createWin32SurfaceResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createWin32SurfaceResult;
    }

    *surface = MakeFakeHandle<VkSurfaceKHR>(g_fakeVulkanState->nextSurfaceValue);
    ++g_fakeVulkanState->nextSurfaceValue;
    return VK_SUCCESS;
}

[[nodiscard]] VkResult FakeCreateX11Surface(VkInstance instance, const void* createInfo,
                                            const VkAllocationCallbacks* allocator,
                                            VkSurfaceKHR* surface) noexcept
{
    (void)instance;
    (void)createInfo;
    (void)allocator;
    ++g_fakeVulkanState->createX11SurfaceCalls;
    g_fakeVulkanState->lastCreatedSurfaceWsiKind = pond::render::detail::VulkanWsiKind::X11;
    if (g_fakeVulkanState->createX11SurfaceResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createX11SurfaceResult;
    }

    *surface = MakeFakeHandle<VkSurfaceKHR>(g_fakeVulkanState->nextSurfaceValue);
    ++g_fakeVulkanState->nextSurfaceValue;
    return VK_SUCCESS;
}

[[nodiscard]] VkResult FakeCreateWaylandSurface(VkInstance instance, const void* createInfo,
                                                const VkAllocationCallbacks* allocator,
                                                VkSurfaceKHR* surface) noexcept
{
    (void)instance;
    (void)createInfo;
    (void)allocator;
    ++g_fakeVulkanState->createWaylandSurfaceCalls;
    g_fakeVulkanState->lastCreatedSurfaceWsiKind = pond::render::detail::VulkanWsiKind::Wayland;
    if (g_fakeVulkanState->createWaylandSurfaceResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createWaylandSurfaceResult;
    }

    *surface = MakeFakeHandle<VkSurfaceKHR>(g_fakeVulkanState->nextSurfaceValue);
    ++g_fakeVulkanState->nextSurfaceValue;
    return VK_SUCCESS;
}

void FakeDestroySurface(VkInstance instance, VkSurfaceKHR surface,
                        const VkAllocationCallbacks* allocator) noexcept
{
    (void)instance;
    (void)surface;
    (void)allocator;
    ++g_fakeVulkanState->destroySurfaceCalls;
}

[[nodiscard]] VkResult FakeEnumeratePhysicalDevices(VkInstance instance,
                                                    std::uint32_t* physicalDeviceCount,
                                                    VkPhysicalDevice* physicalDevices) noexcept
{
    (void)instance;
    ++g_fakeVulkanState->enumeratePhysicalDevicesCalls;
    if (physicalDeviceCount == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (g_fakeVulkanState->enumeratePhysicalDevicesResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->enumeratePhysicalDevicesResult;
    }

    if (physicalDevices == nullptr)
    {
        *physicalDeviceCount =
            static_cast<std::uint32_t>(g_fakeVulkanState->physicalDevices.size());
        return VK_SUCCESS;
    }
    if (g_fakeVulkanState->enumeratePhysicalDevicesReadResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->enumeratePhysicalDevicesReadResult;
    }

    const std::uint32_t writableCount =
        std::min(*physicalDeviceCount,
                 static_cast<std::uint32_t>(g_fakeVulkanState->physicalDevices.size()));
    for (std::uint32_t index = 0; index < writableCount; ++index)
    {
        physicalDevices[index] =
            MakeFakeHandle<VkPhysicalDevice>(g_fakeVulkanState->physicalDevices[index].handleValue);
    }

    *physicalDeviceCount = writableCount;
    return writableCount == g_fakeVulkanState->physicalDevices.size() ? VK_SUCCESS : VK_INCOMPLETE;
}

void FakeGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                      VkPhysicalDeviceProperties2* properties) noexcept
{
    ++g_fakeVulkanState->getPhysicalDeviceProperties2Calls;
    FakePhysicalDevice* device = FindFakePhysicalDevice(physicalDevice);
    if (device == nullptr || properties == nullptr)
    {
        return;
    }

    properties->properties.apiVersion = device->apiVersion;
    properties->properties.vendorID = device->vendorId;
    properties->properties.deviceID = device->deviceId;
    properties->properties.deviceType = device->deviceType;
    properties->properties.driverVersion = device->driverVersion;
    properties->properties.limits.maxImageDimension2D = 16384U;
    properties->properties.limits.maxFramebufferWidth = 16384U;
    properties->properties.limits.maxFramebufferHeight = 16384U;
    properties->properties.limits.maxColorAttachments = 8U;
    CopyStringToFixedArray(device->deviceName, properties->properties.deviceName,
                           sizeof(properties->properties.deviceName));

    VkBaseOutStructure* current = reinterpret_cast<VkBaseOutStructure*>(properties->pNext);
    while (current != nullptr)
    {
        switch (current->sType)
        {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES:
        {
            auto* idProperties = reinterpret_cast<VkPhysicalDeviceIDProperties*>(current);
            std::copy(device->deviceUuid.begin(), device->deviceUuid.end(),
                      idProperties->deviceUUID);
            std::copy(device->deviceLuid.begin(), device->deviceLuid.end(),
                      idProperties->deviceLUID);
            idProperties->deviceNodeMask = device->deviceNodeMask;
            idProperties->deviceLUIDValid = device->deviceLuidValid ? VK_TRUE : VK_FALSE;
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES:
        {
            auto* driverProperties = reinterpret_cast<VkPhysicalDeviceDriverProperties*>(current);
            CopyStringToFixedArray(device->driverName, driverProperties->driverName,
                                   sizeof(driverProperties->driverName));
            CopyStringToFixedArray(device->driverInfo, driverProperties->driverInfo,
                                   sizeof(driverProperties->driverInfo));
            break;
        }
        default:
            break;
        }

        current = current->pNext;
    }
}

void FakeGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice,
                                    VkPhysicalDeviceFeatures2* features) noexcept
{
    ++g_fakeVulkanState->getPhysicalDeviceFeatures2Calls;
    FakePhysicalDevice* device = FindFakePhysicalDevice(physicalDevice);
    if (device == nullptr || features == nullptr)
    {
        return;
    }

    VkBaseOutStructure* current = reinterpret_cast<VkBaseOutStructure*>(features->pNext);
    while (current != nullptr)
    {
        switch (current->sType)
        {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT:
        {
            auto* maintenanceFeatures =
                reinterpret_cast<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT*>(current);
            maintenanceFeatures->swapchainMaintenance1 =
                device->swapchainMaintenance1Feature ? VK_TRUE : VK_FALSE;
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR:
        {
            auto* presentIdFeatures =
                reinterpret_cast<VkPhysicalDevicePresentIdFeaturesKHR*>(current);
            presentIdFeatures->presentId = device->presentIdFeature ? VK_TRUE : VK_FALSE;
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR:
        {
            auto* presentWaitFeatures =
                reinterpret_cast<VkPhysicalDevicePresentWaitFeaturesKHR*>(current);
            presentWaitFeatures->presentWait = device->presentWaitFeature ? VK_TRUE : VK_FALSE;
            break;
        }
        default:
            break;
        }

        current = current->pNext;
    }
}
void FakeGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* memoryProperties) noexcept
{
    ++g_fakeVulkanState->getPhysicalDeviceMemoryPropertiesCalls;
    FakePhysicalDevice* device = FindFakePhysicalDevice(physicalDevice);
    if (device == nullptr || memoryProperties == nullptr)
    {
        return;
    }

    memoryProperties->memoryHeapCount =
        std::min(static_cast<std::uint32_t>(device->memoryHeaps.size()),
                 static_cast<std::uint32_t>(VK_MAX_MEMORY_HEAPS));
    for (std::uint32_t index = 0; index < memoryProperties->memoryHeapCount; ++index)
    {
        memoryProperties->memoryHeaps[index] = device->memoryHeaps[index];
    }
}

void FakeGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice,
                                                std::uint32_t* propertyCount,
                                                VkQueueFamilyProperties* properties) noexcept
{
    ++g_fakeVulkanState->getPhysicalDeviceQueueFamilyPropertiesCalls;
    FakePhysicalDevice* device = FindFakePhysicalDevice(physicalDevice);
    if (device == nullptr || propertyCount == nullptr)
    {
        return;
    }

    if (properties == nullptr)
    {
        *propertyCount = static_cast<std::uint32_t>(device->queueFamilies.size());
        return;
    }

    const std::uint32_t writableCount =
        std::min(*propertyCount, static_cast<std::uint32_t>(device->queueFamilies.size()));
    for (std::uint32_t index = 0; index < writableCount; ++index)
    {
        properties[index] = device->queueFamilies[index];
    }
    *propertyCount = writableCount;
}

[[nodiscard]] VkResult FakeGetPhysicalDeviceSurfaceSupport(VkPhysicalDevice physicalDevice,
                                                           std::uint32_t queueFamilyIndex,
                                                           VkSurfaceKHR surface,
                                                           VkBool32* supported) noexcept
{
    (void)surface;
    ++g_fakeVulkanState->getSurfaceSupportCalls;
    if (g_fakeVulkanState->getSurfaceSupportResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->getSurfaceSupportResult;
    }

    FakePhysicalDevice* device = FindFakePhysicalDevice(physicalDevice);
    if (device == nullptr || supported == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    *supported = queueFamilyIndex < device->surfaceSupport.size()
                     ? device->surfaceSupport[queueFamilyIndex]
                     : VK_FALSE;
    return VK_SUCCESS;
}

[[nodiscard]] VkResult FakeGetPhysicalDeviceSurfaceFormats(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::uint32_t* surfaceFormatCount,
    VkSurfaceFormatKHR* surfaceFormats) noexcept
{
    (void)surface;
    ++g_fakeVulkanState->getSurfaceFormatsCalls;
    if (surfaceFormatCount == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (g_fakeVulkanState->getSurfaceFormatsResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->getSurfaceFormatsResult;
    }

    FakePhysicalDevice* device = FindFakePhysicalDevice(physicalDevice);
    if (device == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (surfaceFormats == nullptr)
    {
        *surfaceFormatCount = static_cast<std::uint32_t>(device->surfaceFormats.size());
        return VK_SUCCESS;
    }
    if (g_fakeVulkanState->getSurfaceFormatsReadResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->getSurfaceFormatsReadResult;
    }

    const std::uint32_t writableCount =
        std::min(*surfaceFormatCount, static_cast<std::uint32_t>(device->surfaceFormats.size()));
    for (std::uint32_t index = 0; index < writableCount; ++index)
    {
        surfaceFormats[index] = device->surfaceFormats[index];
    }
    *surfaceFormatCount = writableCount;
    if (g_fakeVulkanState->surfaceFormatIncompleteReadsRemaining > 0U)
    {
        --g_fakeVulkanState->surfaceFormatIncompleteReadsRemaining;
        return VK_INCOMPLETE;
    }

    return writableCount == device->surfaceFormats.size() ? VK_SUCCESS : VK_INCOMPLETE;
}

[[nodiscard]] VkResult FakeGetPhysicalDeviceSurfacePresentModes(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, std::uint32_t* presentModeCount,
    VkPresentModeKHR* presentModes) noexcept
{
    (void)surface;
    ++g_fakeVulkanState->getPresentModesCalls;
    if (presentModeCount == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (g_fakeVulkanState->getPresentModesResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->getPresentModesResult;
    }

    FakePhysicalDevice* device = FindFakePhysicalDevice(physicalDevice);
    if (device == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (presentModes == nullptr)
    {
        *presentModeCount = static_cast<std::uint32_t>(device->presentModes.size());
        return VK_SUCCESS;
    }
    if (g_fakeVulkanState->getPresentModesReadResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->getPresentModesReadResult;
    }

    const std::uint32_t writableCount =
        std::min(*presentModeCount, static_cast<std::uint32_t>(device->presentModes.size()));
    for (std::uint32_t index = 0; index < writableCount; ++index)
    {
        presentModes[index] = device->presentModes[index];
    }
    *presentModeCount = writableCount;
    if (g_fakeVulkanState->presentModeIncompleteReadsRemaining > 0U)
    {
        --g_fakeVulkanState->presentModeIncompleteReadsRemaining;
        return VK_INCOMPLETE;
    }

    return writableCount == device->presentModes.size() ? VK_SUCCESS : VK_INCOMPLETE;
}

[[nodiscard]] VkResult FakeGetPhysicalDeviceSurfaceCapabilities(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
    VkSurfaceCapabilitiesKHR* capabilities) noexcept
{
    (void)surface;
    ++g_fakeVulkanState->getSurfaceCapabilitiesCalls;
    if (capabilities == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (g_fakeVulkanState->getSurfaceCapabilitiesResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->getSurfaceCapabilitiesResult;
    }

    FakePhysicalDevice* device = FindFakePhysicalDevice(physicalDevice);
    if (device == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    *capabilities = device->surfaceCapabilities;
    return VK_SUCCESS;
}
[[nodiscard]] VkResult FakeEnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice, const char* layerName, std::uint32_t* propertyCount,
    VkExtensionProperties* properties) noexcept
{
    (void)layerName;
    ++g_fakeVulkanState->enumerateDeviceExtensionsCalls;
    if (propertyCount == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (g_fakeVulkanState->enumerateDeviceExtensionsResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->enumerateDeviceExtensionsResult;
    }

    FakePhysicalDevice* device = FindFakePhysicalDevice(physicalDevice);
    if (device == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (properties == nullptr)
    {
        *propertyCount = static_cast<std::uint32_t>(device->deviceExtensions.size());
        return VK_SUCCESS;
    }
    if (g_fakeVulkanState->enumerateDeviceExtensionsReadResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->enumerateDeviceExtensionsReadResult;
    }

    const std::uint32_t writableCount =
        std::min(*propertyCount, static_cast<std::uint32_t>(device->deviceExtensions.size()));
    for (std::uint32_t index = 0; index < writableCount; ++index)
    {
        properties[index] = {};
        CopyStringToFixedArray(device->deviceExtensions[index], properties[index].extensionName,
                               sizeof(properties[index].extensionName));
    }
    *propertyCount = writableCount;
    return writableCount == device->deviceExtensions.size() ? VK_SUCCESS : VK_INCOMPLETE;
}
void CaptureDeviceFeatureChain(const VkDeviceCreateInfo& createInfo)
{
    g_fakeVulkanState->swapchainMaintenance1FeaturesChained = false;
    g_fakeVulkanState->presentIdFeaturesChained = false;
    g_fakeVulkanState->presentWaitFeaturesChained = false;
    g_fakeVulkanState->lastEnabledSwapchainMaintenance1Feature = false;
    g_fakeVulkanState->lastEnabledPresentIdFeature = false;
    g_fakeVulkanState->lastEnabledPresentWaitFeature = false;

    const VkBaseInStructure* current = static_cast<const VkBaseInStructure*>(createInfo.pNext);
    while (current != nullptr)
    {
        switch (current->sType)
        {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT:
        {
            g_fakeVulkanState->swapchainMaintenance1FeaturesChained = true;
            const auto* maintenanceFeatures =
                reinterpret_cast<const VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT*>(current);
            g_fakeVulkanState->lastEnabledSwapchainMaintenance1Feature =
                maintenanceFeatures->swapchainMaintenance1 == VK_TRUE;
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR:
        {
            g_fakeVulkanState->presentIdFeaturesChained = true;
            const auto* presentIdFeatures =
                reinterpret_cast<const VkPhysicalDevicePresentIdFeaturesKHR*>(current);
            g_fakeVulkanState->lastEnabledPresentIdFeature =
                presentIdFeatures->presentId == VK_TRUE;
            break;
        }
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR:
        {
            g_fakeVulkanState->presentWaitFeaturesChained = true;
            const auto* presentWaitFeatures =
                reinterpret_cast<const VkPhysicalDevicePresentWaitFeaturesKHR*>(current);
            g_fakeVulkanState->lastEnabledPresentWaitFeature =
                presentWaitFeatures->presentWait == VK_TRUE;
            break;
        }
        default:
            break;
        }

        current = current->pNext;
    }
}

void CaptureDeviceCreateInfo(const VkDeviceCreateInfo& createInfo)
{
    g_fakeVulkanState->lastDeviceEnabledExtensions.clear();
    g_fakeVulkanState->lastDeviceQueueFamilyIndices.clear();

    if (createInfo.ppEnabledExtensionNames != nullptr)
    {
        const char* const* const begin = createInfo.ppEnabledExtensionNames;
        const char* const* const end = begin + createInfo.enabledExtensionCount;
        g_fakeVulkanState->lastDeviceEnabledExtensions.assign(begin, end);
    }

    if (createInfo.pQueueCreateInfos != nullptr)
    {
        for (std::uint32_t index = 0; index < createInfo.queueCreateInfoCount; ++index)
        {
            g_fakeVulkanState->lastDeviceQueueFamilyIndices.push_back(
                createInfo.pQueueCreateInfos[index].queueFamilyIndex);
        }
    }

    CaptureDeviceFeatureChain(createInfo);
}

[[nodiscard]] VkResult FakeCreateDevice(VkPhysicalDevice physicalDevice,
                                        const VkDeviceCreateInfo* createInfo,
                                        const VkAllocationCallbacks* allocator,
                                        VkDevice* device) noexcept
{
    (void)physicalDevice;
    (void)allocator;
    ++g_fakeVulkanState->createDeviceCalls;
    if (createInfo != nullptr)
    {
        CaptureDeviceCreateInfo(*createInfo);
    }

    if (device == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (g_fakeVulkanState->createDeviceResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createDeviceResult;
    }

    *device = MakeFakeHandle<VkDevice>(g_fakeVulkanState->nextDeviceValue);
    ++g_fakeVulkanState->nextDeviceValue;
    return VK_SUCCESS;
}

void FakeDestroyDevice(VkDevice device, const VkAllocationCallbacks* allocator) noexcept
{
    (void)device;
    (void)allocator;
    ++g_fakeVulkanState->destroyDeviceCalls;
}

void FakeLoadDevice(VkDevice device) noexcept
{
    (void)device;
    ++g_fakeVulkanState->loadDeviceCalls;
}

void FakeGetDeviceQueue(VkDevice device, std::uint32_t queueFamilyIndex, std::uint32_t queueIndex,
                        VkQueue* queue) noexcept
{
    (void)device;
    (void)queueFamilyIndex;
    (void)queueIndex;
    ++g_fakeVulkanState->getDeviceQueueCalls;
    if (queue != nullptr)
    {
        *queue = MakeFakeHandle<VkQueue>(g_fakeVulkanState->nextQueueValue);
        ++g_fakeVulkanState->nextQueueValue;
    }
}

[[nodiscard]] VkResult FakeDeviceWaitIdle(VkDevice device) noexcept
{
    (void)device;
    ++g_fakeVulkanState->deviceWaitIdleCalls;
    return g_fakeVulkanState->deviceWaitIdleResult;
}

[[nodiscard]] VkResult FakeQueueWaitIdle(VkQueue queue) noexcept
{
    g_fakeVulkanState->lastQueueWaitIdle = queue;
    ++g_fakeVulkanState->queueWaitIdleCalls;
    return g_fakeVulkanState->queueWaitIdleResult;
}

[[nodiscard]] VkResult FakeCreateAllocator(VkInstance instance, VkPhysicalDevice physicalDevice,
                                           VkDevice device, std::uint32_t apiVersion,
                                           void** allocator) noexcept
{
    (void)instance;
    (void)physicalDevice;
    (void)device;
    (void)apiVersion;
    ++g_fakeVulkanState->createAllocatorCalls;
    if (allocator == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (g_fakeVulkanState->createAllocatorResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createAllocatorResult;
    }

    *allocator = reinterpret_cast<void*>(g_fakeVulkanState->nextAllocatorValue);
    ++g_fakeVulkanState->nextAllocatorValue;
    return VK_SUCCESS;
}

[[nodiscard]] VkResult FakeCreateOwnerLocalAllocator(VkInstance instance,
                                                     VkPhysicalDevice physicalDevice,
                                                     VkDevice device, std::uint32_t apiVersion,
                                                     PFN_vkGetInstanceProcAddr getInstanceProcAddr,
                                                     PFN_vkGetDeviceProcAddr getDeviceProcAddr,
                                                     void** allocator) noexcept
{
    (void)instance;
    (void)physicalDevice;
    (void)device;
    (void)apiVersion;
    ++g_fakeVulkanState->createOwnerLocalAllocatorCalls;
    g_fakeVulkanState->lastAllocatorGetInstanceProcAddr = getInstanceProcAddr;
    g_fakeVulkanState->lastAllocatorGetDeviceProcAddr = getDeviceProcAddr;
    if (allocator == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    *allocator = reinterpret_cast<void*>(g_fakeVulkanState->nextAllocatorValue);
    ++g_fakeVulkanState->nextAllocatorValue;
    return VK_SUCCESS;
}
void FakeDestroyAllocator(void* allocator) noexcept
{
    (void)allocator;
    ++g_fakeVulkanState->destroyAllocatorCalls;
}

[[nodiscard]] VkResult FakeCreateSwapchain(VkDevice device,
                                           const VkSwapchainCreateInfoKHR* createInfo,
                                           const VkAllocationCallbacks* allocator,
                                           VkSwapchainKHR* swapchain) noexcept
{
    (void)device;
    (void)allocator;
    ++g_fakeVulkanState->createSwapchainCalls;
    if (createInfo != nullptr)
    {
        g_fakeVulkanState->lastSwapchainExtent = createInfo->imageExtent;
        g_fakeVulkanState->lastSwapchainFormat = createInfo->imageFormat;
        g_fakeVulkanState->lastSwapchainColorSpace = createInfo->imageColorSpace;
        g_fakeVulkanState->lastSwapchainPresentMode = createInfo->presentMode;
        g_fakeVulkanState->lastSwapchainCompositeAlpha = createInfo->compositeAlpha;
        g_fakeVulkanState->lastSwapchainPreTransform = createInfo->preTransform;
        g_fakeVulkanState->lastSwapchainSharingMode = createInfo->imageSharingMode;
        g_fakeVulkanState->lastSwapchainMinImageCount = createInfo->minImageCount;
        g_fakeVulkanState->lastOldSwapchain = createInfo->oldSwapchain;
        g_fakeVulkanState->lastSwapchainQueueFamilyIndices.clear();
        if (createInfo->pQueueFamilyIndices != nullptr)
        {
            const std::uint32_t* begin = createInfo->pQueueFamilyIndices;
            const std::uint32_t* end = begin + createInfo->queueFamilyIndexCount;
            g_fakeVulkanState->lastSwapchainQueueFamilyIndices.assign(begin, end);
        }
    }

    if (swapchain == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (g_fakeVulkanState->createSwapchainResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createSwapchainResult;
    }

    *swapchain = MakeFakeHandle<VkSwapchainKHR>(g_fakeVulkanState->nextSwapchainValue);
    ++g_fakeVulkanState->nextSwapchainValue;
    return VK_SUCCESS;
}

void FakeDestroySwapchain(VkDevice device, VkSwapchainKHR swapchain,
                          const VkAllocationCallbacks* allocator) noexcept
{
    (void)device;
    (void)swapchain;
    (void)allocator;
    ++g_fakeVulkanState->destroySwapchainCalls;
}

[[nodiscard]] VkResult FakeGetSwapchainImages(VkDevice device, VkSwapchainKHR swapchain,
                                              std::uint32_t* imageCount, VkImage* images)
{
    (void)device;
    (void)swapchain;
    ++g_fakeVulkanState->getSwapchainImagesCalls;
    if (imageCount == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (g_fakeVulkanState->getSwapchainImagesResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->getSwapchainImagesResult;
    }

    if (images != nullptr && g_fakeVulkanState->throwBadAllocOnSwapchainImageRead)
    {
        g_fakeVulkanState->throwBadAllocOnSwapchainImageRead = false;
        throw std::bad_alloc{};
    }

    if (images == nullptr)
    {
        *imageCount = g_fakeVulkanState->swapchainImageCount;
        if (g_fakeVulkanState->swapchainImageCountAfterFirstQuery != 0U)
        {
            g_fakeVulkanState->swapchainImageCount =
                g_fakeVulkanState->swapchainImageCountAfterFirstQuery;
            g_fakeVulkanState->swapchainImageCountAfterFirstQuery = 0U;
        }
        return VK_SUCCESS;
    }
    if (g_fakeVulkanState->getSwapchainImagesReadResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->getSwapchainImagesReadResult;
    }

    const std::uint32_t writableCount =
        std::min(*imageCount, g_fakeVulkanState->swapchainImageCount);
    for (std::uint32_t index = 0; index < writableCount; ++index)
    {
        images[index] = MakeFakeHandle<VkImage>(g_fakeVulkanState->nextImageValue + index);
    }
    *imageCount = writableCount;
    g_fakeVulkanState->nextImageValue += writableCount;
    return writableCount == g_fakeVulkanState->swapchainImageCount ? VK_SUCCESS : VK_INCOMPLETE;
}

[[nodiscard]] VkResult FakeCreateImageView(VkDevice device, const VkImageViewCreateInfo* createInfo,
                                           const VkAllocationCallbacks* allocator,
                                           VkImageView* imageView) noexcept
{
    (void)device;
    (void)createInfo;
    (void)allocator;
    ++g_fakeVulkanState->createImageViewCalls;
    if (imageView == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (g_fakeVulkanState->createImageViewResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createImageViewResult;
    }

    *imageView = MakeFakeHandle<VkImageView>(g_fakeVulkanState->nextImageViewValue);
    ++g_fakeVulkanState->nextImageViewValue;
    return VK_SUCCESS;
}

void FakeDestroyImageView(VkDevice device, VkImageView imageView,
                          const VkAllocationCallbacks* allocator) noexcept
{
    (void)device;
    (void)imageView;
    (void)allocator;
    ++g_fakeVulkanState->destroyImageViewCalls;
}

[[nodiscard]] VkResult FakeCreateRenderPass(VkDevice device,
                                            const VkRenderPassCreateInfo* createInfo,
                                            const VkAllocationCallbacks* allocator,
                                            VkRenderPass* renderPass) noexcept
{
    (void)device;
    (void)createInfo;
    (void)allocator;
    ++g_fakeVulkanState->createRenderPassCalls;
    if (renderPass == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (g_fakeVulkanState->createRenderPassResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createRenderPassResult;
    }

    *renderPass = MakeFakeHandle<VkRenderPass>(g_fakeVulkanState->nextRenderPassValue);
    ++g_fakeVulkanState->nextRenderPassValue;
    return VK_SUCCESS;
}

void FakeDestroyRenderPass(VkDevice device, VkRenderPass renderPass,
                           const VkAllocationCallbacks* allocator) noexcept
{
    (void)device;
    (void)renderPass;
    (void)allocator;
    ++g_fakeVulkanState->destroyRenderPassCalls;
}

[[nodiscard]] VkResult FakeCreateFramebuffer(VkDevice device,
                                             const VkFramebufferCreateInfo* createInfo,
                                             const VkAllocationCallbacks* allocator,
                                             VkFramebuffer* framebuffer) noexcept
{
    (void)device;
    (void)createInfo;
    (void)allocator;
    ++g_fakeVulkanState->createFramebufferCalls;
    if (framebuffer == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (g_fakeVulkanState->createFramebufferResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createFramebufferResult;
    }

    *framebuffer = MakeFakeHandle<VkFramebuffer>(g_fakeVulkanState->nextFramebufferValue);
    ++g_fakeVulkanState->nextFramebufferValue;
    return VK_SUCCESS;
}

void FakeDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer,
                            const VkAllocationCallbacks* allocator) noexcept
{
    (void)device;
    (void)framebuffer;
    (void)allocator;
    ++g_fakeVulkanState->destroyFramebufferCalls;
}

[[nodiscard]] VkResult FakeCreateCommandPool(VkDevice device,
                                             const VkCommandPoolCreateInfo* createInfo,
                                             const VkAllocationCallbacks* allocator,
                                             VkCommandPool* commandPool) noexcept
{
    (void)device;
    (void)allocator;
    ++g_fakeVulkanState->createCommandPoolCalls;
    if (g_fakeVulkanState->createCommandPoolResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createCommandPoolResult;
    }

    if (createInfo == nullptr || commandPool == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    *commandPool = MakeFakeHandle<VkCommandPool>(g_fakeVulkanState->nextCommandPoolValue++);
    return VK_SUCCESS;
}

void FakeDestroyCommandPool(VkDevice device, VkCommandPool commandPool,
                            const VkAllocationCallbacks* allocator) noexcept
{
    (void)device;
    (void)commandPool;
    (void)allocator;
    ++g_fakeVulkanState->destroyCommandPoolCalls;
}

[[nodiscard]] VkResult FakeAllocateCommandBuffers(VkDevice device,
                                                  const VkCommandBufferAllocateInfo* allocateInfo,
                                                  VkCommandBuffer* commandBuffers) noexcept
{
    (void)device;
    ++g_fakeVulkanState->allocateCommandBuffersCalls;
    if (g_fakeVulkanState->allocateCommandBuffersResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->allocateCommandBuffersResult;
    }

    if (allocateInfo == nullptr || commandBuffers == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    for (std::uint32_t index = 0U; index < allocateInfo->commandBufferCount; ++index)
    {
        commandBuffers[index] =
            MakeFakeHandle<VkCommandBuffer>(g_fakeVulkanState->nextCommandBufferValue++);
    }

    return VK_SUCCESS;
}

[[nodiscard]] VkResult FakeResetCommandBuffer(VkCommandBuffer commandBuffer,
                                              VkCommandBufferResetFlags flags) noexcept
{
    (void)commandBuffer;
    (void)flags;
    ++g_fakeVulkanState->resetCommandBufferCalls;
    return g_fakeVulkanState->resetCommandBufferResult;
}

[[nodiscard]] VkResult FakeBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                              const VkCommandBufferBeginInfo* beginInfo) noexcept
{
    (void)commandBuffer;
    (void)beginInfo;
    ++g_fakeVulkanState->beginCommandBufferCalls;
    return g_fakeVulkanState->beginCommandBufferResult;
}

[[nodiscard]] VkResult FakeEndCommandBuffer(VkCommandBuffer commandBuffer) noexcept
{
    (void)commandBuffer;
    ++g_fakeVulkanState->endCommandBufferCalls;
    return g_fakeVulkanState->endCommandBufferResult;
}

void FakeCmdBeginRenderPass(VkCommandBuffer commandBuffer,
                            const VkRenderPassBeginInfo* renderPassBegin,
                            VkSubpassContents contents) noexcept
{
    (void)commandBuffer;
    (void)renderPassBegin;
    (void)contents;
    g_fakeVulkanState->frameTrace.emplace_back("clear");
    ++g_fakeVulkanState->cmdBeginRenderPassCalls;
}

void FakeCmdEndRenderPass(VkCommandBuffer commandBuffer) noexcept
{
    (void)commandBuffer;
    ++g_fakeVulkanState->cmdEndRenderPassCalls;
}

[[nodiscard]] VkResult FakeCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* createInfo,
                                           const VkAllocationCallbacks* allocator,
                                           VkSemaphore* semaphore) noexcept
{
    (void)device;
    (void)allocator;
    ++g_fakeVulkanState->createSemaphoreCalls;
    if (g_fakeVulkanState->createSemaphoreResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createSemaphoreResult;
    }

    if (createInfo == nullptr || semaphore == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    *semaphore = MakeFakeHandle<VkSemaphore>(g_fakeVulkanState->nextSemaphoreValue++);
    return VK_SUCCESS;
}

void FakeDestroySemaphore(VkDevice device, VkSemaphore semaphore,
                          const VkAllocationCallbacks* allocator) noexcept
{
    (void)device;
    (void)semaphore;
    (void)allocator;
    ++g_fakeVulkanState->destroySemaphoreCalls;
}

[[nodiscard]] VkResult FakeCreateFence(VkDevice device, const VkFenceCreateInfo* createInfo,
                                       const VkAllocationCallbacks* allocator,
                                       VkFence* fence) noexcept
{
    (void)device;
    (void)allocator;
    ++g_fakeVulkanState->createFenceCalls;
    if (g_fakeVulkanState->createFenceResult != VK_SUCCESS)
    {
        return g_fakeVulkanState->createFenceResult;
    }

    if (createInfo == nullptr || fence == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    *fence = MakeFakeHandle<VkFence>(g_fakeVulkanState->nextFenceValue++);
    return VK_SUCCESS;
}

void FakeDestroyFence(VkDevice device, VkFence fence,
                      const VkAllocationCallbacks* allocator) noexcept
{
    (void)device;
    (void)fence;
    (void)allocator;
    ++g_fakeVulkanState->destroyFenceCalls;
}

[[nodiscard]] VkResult FakeWaitForFences(VkDevice device, std::uint32_t fenceCount,
                                         const VkFence* fences, VkBool32 waitAll,
                                         std::uint64_t timeout) noexcept
{
    (void)device;
    g_fakeVulkanState->lastWaitForFences.clear();
    if (fences != nullptr)
    {
        g_fakeVulkanState->lastWaitForFences.assign(fences, fences + fenceCount);
    }
    g_fakeVulkanState->lastWaitForFencesFenceCount = fenceCount;
    g_fakeVulkanState->lastWaitForFencesWaitAll = waitAll;
    g_fakeVulkanState->lastWaitForFencesTimeout = timeout;
    ++g_fakeVulkanState->waitForFencesCalls;
    return g_fakeVulkanState->waitForFencesResult;
}

[[nodiscard]] VkResult FakeResetFences(VkDevice device, std::uint32_t fenceCount,
                                       const VkFence* fences) noexcept
{
    (void)device;
    (void)fenceCount;
    (void)fences;
    ++g_fakeVulkanState->resetFencesCalls;
    if (g_fakeVulkanState->notifyNextResetFences.exchange(false))
    {
        g_fakeVulkanState->resetFencesReached.release();
    }
    return g_fakeVulkanState->resetFencesResult;
}

[[nodiscard]] VkResult FakeAcquireNextImage(VkDevice device, VkSwapchainKHR swapchain,
                                            std::uint64_t timeout, VkSemaphore semaphore,
                                            VkFence fence, std::uint32_t* imageIndex) noexcept
{
    (void)device;
    (void)swapchain;
    (void)timeout;
    g_fakeVulkanState->acquiredSemaphores.push_back(semaphore);
    g_fakeVulkanState->acquiredFences.push_back(fence);
    g_fakeVulkanState->frameTrace.emplace_back("acquire");
    ++g_fakeVulkanState->acquireNextImageCalls;
    if (g_fakeVulkanState->acquireNextImageResult != VK_SUCCESS &&
        g_fakeVulkanState->acquireNextImageResult != VK_SUBOPTIMAL_KHR)
    {
        return g_fakeVulkanState->acquireNextImageResult;
    }

    if (imageIndex == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    *imageIndex = g_fakeVulkanState->acquiredImageIndex;
    return g_fakeVulkanState->acquireNextImageResult;
}

[[nodiscard]] VkResult FakeQueueSubmit(VkQueue queue, std::uint32_t submitCount,
                                       const VkSubmitInfo* submits, VkFence fence) noexcept
{
    (void)queue;
    g_fakeVulkanState->submittedFences.push_back(fence);
    if (submitCount > 0U && submits != nullptr && submits[0].waitSemaphoreCount > 0U &&
        submits[0].pWaitSemaphores != nullptr)
    {
        g_fakeVulkanState->submittedWaitSemaphores.push_back(submits[0].pWaitSemaphores[0]);
    }
    if (submitCount > 0U && submits != nullptr && submits[0].signalSemaphoreCount > 0U &&
        submits[0].pSignalSemaphores != nullptr)
    {
        g_fakeVulkanState->submittedSignalSemaphores.push_back(submits[0].pSignalSemaphores[0]);
    }
    g_fakeVulkanState->frameTrace.emplace_back("submit");
    ++g_fakeVulkanState->queueSubmitCalls;
    return g_fakeVulkanState->queueSubmitResult;
}

[[nodiscard]] VkResult FakeQueuePresent(VkQueue queue, const VkPresentInfoKHR* presentInfo) noexcept
{
    g_fakeVulkanState->presentationQueues.push_back(queue);
    VkFence presentFence{VK_NULL_HANDLE};
    std::uint64_t presentId{};
    if (presentInfo != nullptr)
    {
        if (presentInfo->waitSemaphoreCount > 0U && presentInfo->pWaitSemaphores != nullptr)
        {
            g_fakeVulkanState->presentWaitSemaphores.push_back(presentInfo->pWaitSemaphores[0]);
        }

        const VkBaseInStructure* current =
            static_cast<const VkBaseInStructure*>(presentInfo->pNext);
        while (current != nullptr)
        {
            if (current->sType == VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT)
            {
                const auto* fenceInfo =
                    reinterpret_cast<const VkSwapchainPresentFenceInfoEXT*>(current);
                if (fenceInfo->swapchainCount > 0U && fenceInfo->pFences != nullptr)
                {
                    presentFence = fenceInfo->pFences[0];
                }
            }
            else if (current->sType == VK_STRUCTURE_TYPE_PRESENT_ID_KHR)
            {
                const auto* idInfo = reinterpret_cast<const VkPresentIdKHR*>(current);
                if (idInfo->swapchainCount > 0U && idInfo->pPresentIds != nullptr)
                {
                    presentId = idInfo->pPresentIds[0];
                }
            }

            current = current->pNext;
        }
    }

    g_fakeVulkanState->presentedFences.push_back(presentFence);
    g_fakeVulkanState->presentedIds.push_back(presentId);
    g_fakeVulkanState->frameTrace.emplace_back("present");
    ++g_fakeVulkanState->queuePresentCalls;
    return g_fakeVulkanState->queuePresentResult;
}

[[nodiscard]] VkResult FakeWaitForPresent(VkDevice device, VkSwapchainKHR swapchain,
                                          std::uint64_t presentId, std::uint64_t timeout) noexcept
{
    g_fakeVulkanState->lastWaitForPresentDevice = device;
    g_fakeVulkanState->lastWaitForPresentSwapchain = swapchain;
    g_fakeVulkanState->lastWaitForPresentId = presentId;
    g_fakeVulkanState->lastWaitForPresentTimeout = timeout;
    ++g_fakeVulkanState->waitForPresentCalls;
    return g_fakeVulkanState->waitForPresentResult;
}

[[nodiscard]] VkResult FakeSetDebugUtilsObjectName(
    VkDevice device, const VkDebugUtilsObjectNameInfoEXT* nameInfo) noexcept
{
    (void)device;
    if (nameInfo != nullptr && nameInfo->pObjectName != nullptr)
    {
        g_fakeVulkanState->debugObjectNames.emplace_back(nameInfo->pObjectName);
    }
    ++g_fakeVulkanState->setDebugNameCalls;
    return g_fakeVulkanState->setDebugNameResult;
}

void FakeCmdBeginDebugUtilsLabel(VkCommandBuffer commandBuffer,
                                 const VkDebugUtilsLabelEXT* labelInfo) noexcept
{
    (void)commandBuffer;
    if (labelInfo != nullptr && labelInfo->pLabelName != nullptr &&
        std::string_view{labelInfo->pLabelName} == "pond.render.intermediate")
    {
        g_fakeVulkanState->frameTrace.emplace_back("intermediate");
    }
    ++g_fakeVulkanState->beginLabelCalls;
}

void FakeCmdEndDebugUtilsLabel(VkCommandBuffer commandBuffer) noexcept
{
    (void)commandBuffer;
    ++g_fakeVulkanState->endLabelCalls;
}
[[nodiscard]] PFN_vkVoidFunction FakeGetInstanceProcAddr(VkInstance instance,
                                                         const char* name) noexcept
{
    (void)instance;
    (void)name;
    return nullptr;
}

[[nodiscard]] PFN_vkVoidFunction FakeGetDeviceProcAddr(VkDevice device, const char* name) noexcept
{
    (void)device;
    (void)name;
    return nullptr;
}

void FakeDestroyOwnerLocalInstanceA(VkInstance instance,
                                    const VkAllocationCallbacks* allocator) noexcept
{
    FakeDestroyInstance(instance, allocator);
    ++g_fakeVulkanState->ownerLocalInstanceADestroyCalls;
}

void FakeDestroyOwnerLocalInstanceB(VkInstance instance,
                                    const VkAllocationCallbacks* allocator) noexcept
{
    FakeDestroyInstance(instance, allocator);
    ++g_fakeVulkanState->ownerLocalInstanceBDestroyCalls;
}

void FakeLoadOwnerLocalInstanceTable(VolkInstanceTable* table, VkInstance instance) noexcept
{
    (void)instance;
    ++g_fakeVulkanState->loadInstanceTableCalls;
    *table = {};
    table->vkDestroyInstance = g_fakeVulkanState->loadInstanceTableCalls % 2U == 1U
                                   ? &FakeDestroyOwnerLocalInstanceA
                                   : &FakeDestroyOwnerLocalInstanceB;
    table->vkEnumeratePhysicalDevices = &FakeEnumeratePhysicalDevices;
    table->vkGetDeviceProcAddr = &FakeGetDeviceProcAddr;
    table->vkGetPhysicalDeviceProperties2 = &FakeGetPhysicalDeviceProperties2;
    table->vkGetPhysicalDeviceFeatures2 = &FakeGetPhysicalDeviceFeatures2;
    table->vkGetPhysicalDeviceMemoryProperties = &FakeGetPhysicalDeviceMemoryProperties;
    table->vkGetPhysicalDeviceQueueFamilyProperties = &FakeGetPhysicalDeviceQueueFamilyProperties;
    table->vkGetPhysicalDeviceSurfaceSupportKHR = &FakeGetPhysicalDeviceSurfaceSupport;
    table->vkGetPhysicalDeviceSurfaceFormatsKHR = &FakeGetPhysicalDeviceSurfaceFormats;
    table->vkGetPhysicalDeviceSurfacePresentModesKHR = &FakeGetPhysicalDeviceSurfacePresentModes;
    table->vkGetPhysicalDeviceSurfaceCapabilitiesKHR = &FakeGetPhysicalDeviceSurfaceCapabilities;
    table->vkEnumerateDeviceExtensionProperties = &FakeEnumerateDeviceExtensionProperties;
    table->vkCreateDevice = &FakeCreateDevice;
    table->vkCreateDebugUtilsMessengerEXT = &FakeCreateDebugUtilsMessenger;
    table->vkDestroyDebugUtilsMessengerEXT = &FakeDestroyDebugUtilsMessenger;
    table->vkDestroySurfaceKHR = &FakeDestroySurface;
    table->vkSetDebugUtilsObjectNameEXT = &FakeSetDebugUtilsObjectName;
    table->vkCmdBeginDebugUtilsLabelEXT = &FakeCmdBeginDebugUtilsLabel;
    table->vkCmdEndDebugUtilsLabelEXT = &FakeCmdEndDebugUtilsLabel;
}

void FakeLoadOwnerLocalInstanceTableWithoutDebugMessenger(VolkInstanceTable* table,
                                                          VkInstance instance) noexcept
{
    FakeLoadOwnerLocalInstanceTable(table, instance);
    table->vkCreateDebugUtilsMessengerEXT = nullptr;
}

void FakeLoadOwnerLocalInstanceTableWithoutDebugHooks(VolkInstanceTable* table,
                                                      VkInstance instance) noexcept
{
    FakeLoadOwnerLocalInstanceTable(table, instance);
    table->vkSetDebugUtilsObjectNameEXT = nullptr;
    table->vkCmdBeginDebugUtilsLabelEXT = nullptr;
    table->vkCmdEndDebugUtilsLabelEXT = nullptr;
}

void FakeDestroyOwnerLocalDeviceA(VkDevice device, const VkAllocationCallbacks* allocator) noexcept
{
    FakeDestroyDevice(device, allocator);
    ++g_fakeVulkanState->ownerLocalDeviceADestroyCalls;
}

void FakeDestroyOwnerLocalDeviceB(VkDevice device, const VkAllocationCallbacks* allocator) noexcept
{
    FakeDestroyDevice(device, allocator);
    ++g_fakeVulkanState->ownerLocalDeviceBDestroyCalls;
}

void FakeLoadOwnerLocalDeviceTable(VolkDeviceTable* table,
                                   PFN_vkGetDeviceProcAddr getDeviceProcAddr,
                                   VkDevice device) noexcept
{
    (void)getDeviceProcAddr;
    (void)device;
    ++g_fakeVulkanState->loadDeviceTableCalls;
    *table = {};
    table->vkDestroyDevice = g_fakeVulkanState->loadDeviceTableCalls % 2U == 1U
                                 ? &FakeDestroyOwnerLocalDeviceA
                                 : &FakeDestroyOwnerLocalDeviceB;
    table->vkGetDeviceQueue = &FakeGetDeviceQueue;
    table->vkDeviceWaitIdle = &FakeDeviceWaitIdle;
    table->vkQueueWaitIdle = &FakeQueueWaitIdle;
    table->vkCreateSwapchainKHR = &FakeCreateSwapchain;
    table->vkDestroySwapchainKHR = &FakeDestroySwapchain;
    table->vkGetSwapchainImagesKHR = &FakeGetSwapchainImages;
    table->vkCreateImageView = &FakeCreateImageView;
    table->vkDestroyImageView = &FakeDestroyImageView;
    table->vkCreateRenderPass = &FakeCreateRenderPass;
    table->vkDestroyRenderPass = &FakeDestroyRenderPass;
    table->vkCreateFramebuffer = &FakeCreateFramebuffer;
    table->vkDestroyFramebuffer = &FakeDestroyFramebuffer;
    table->vkCreateCommandPool = &FakeCreateCommandPool;
    table->vkDestroyCommandPool = &FakeDestroyCommandPool;
    table->vkAllocateCommandBuffers = &FakeAllocateCommandBuffers;
    table->vkResetCommandBuffer = &FakeResetCommandBuffer;
    table->vkBeginCommandBuffer = &FakeBeginCommandBuffer;
    table->vkEndCommandBuffer = &FakeEndCommandBuffer;
    table->vkCmdBeginRenderPass = &FakeCmdBeginRenderPass;
    table->vkCmdEndRenderPass = &FakeCmdEndRenderPass;
    table->vkCreateSemaphore = &FakeCreateSemaphore;
    table->vkDestroySemaphore = &FakeDestroySemaphore;
    table->vkCreateFence = &FakeCreateFence;
    table->vkDestroyFence = &FakeDestroyFence;
    table->vkWaitForFences = &FakeWaitForFences;
    table->vkResetFences = &FakeResetFences;
    table->vkAcquireNextImageKHR = &FakeAcquireNextImage;
    table->vkQueueSubmit = &FakeQueueSubmit;
    table->vkQueuePresentKHR = &FakeQueuePresent;
    table->vkWaitForPresentKHR = &FakeWaitForPresent;
}
[[nodiscard]] pond::render::detail::VulkanGlobalDispatch MakeFakeDispatch(
    FakeVulkanState& state) noexcept
{
    g_fakeVulkanState = &state;
    return pond::render::detail::VulkanGlobalDispatch{
        .initialize = &FakeInitialize,
        .enumerateInstanceVersion = &FakeEnumerateInstanceVersion,
        .enumerateInstanceExtensionProperties = &FakeEnumerateInstanceExtensions,
        .enumerateInstanceLayerProperties = &FakeEnumerateInstanceLayers,
        .createInstance = &FakeCreateInstance,
        .destroyInstance = &FakeDestroyInstance,
        .loadInstanceOnly = &FakeLoadInstanceOnly,
        .createDebugUtilsMessenger = &FakeCreateDebugUtilsMessenger,
        .destroyDebugUtilsMessenger = &FakeDestroyDebugUtilsMessenger,
        .createWin32Surface = &FakeCreateWin32Surface,
        .createX11Surface = &FakeCreateX11Surface,
        .createWaylandSurface = &FakeCreateWaylandSurface,
        .destroySurface = &FakeDestroySurface,
        .enumeratePhysicalDevices = &FakeEnumeratePhysicalDevices,
        .getPhysicalDeviceProperties2 = &FakeGetPhysicalDeviceProperties2,
        .getPhysicalDeviceFeatures2 = &FakeGetPhysicalDeviceFeatures2,
        .getPhysicalDeviceMemoryProperties = &FakeGetPhysicalDeviceMemoryProperties,
        .getPhysicalDeviceQueueFamilyProperties = &FakeGetPhysicalDeviceQueueFamilyProperties,
        .getPhysicalDeviceSurfaceSupport = &FakeGetPhysicalDeviceSurfaceSupport,
        .getPhysicalDeviceSurfaceFormats = &FakeGetPhysicalDeviceSurfaceFormats,
        .getPhysicalDeviceSurfacePresentModes = &FakeGetPhysicalDeviceSurfacePresentModes,
        .getPhysicalDeviceSurfaceCapabilities = &FakeGetPhysicalDeviceSurfaceCapabilities,
        .enumerateDeviceExtensionProperties = &FakeEnumerateDeviceExtensionProperties,
        .createDevice = &FakeCreateDevice,
        .destroyDevice = &FakeDestroyDevice,
        .loadDevice = &FakeLoadDevice,
        .getDeviceQueue = &FakeGetDeviceQueue,
        .deviceWaitIdle = &FakeDeviceWaitIdle,
        .queueWaitIdle = &FakeQueueWaitIdle,
        .createSwapchain = &FakeCreateSwapchain,
        .destroySwapchain = &FakeDestroySwapchain,
        .getSwapchainImages = &FakeGetSwapchainImages,
        .createImageView = &FakeCreateImageView,
        .destroyImageView = &FakeDestroyImageView,
        .createRenderPass = &FakeCreateRenderPass,
        .destroyRenderPass = &FakeDestroyRenderPass,
        .createFramebuffer = &FakeCreateFramebuffer,
        .destroyFramebuffer = &FakeDestroyFramebuffer,
        .createCommandPool = &FakeCreateCommandPool,
        .destroyCommandPool = &FakeDestroyCommandPool,
        .allocateCommandBuffers = &FakeAllocateCommandBuffers,
        .resetCommandBuffer = &FakeResetCommandBuffer,
        .beginCommandBuffer = &FakeBeginCommandBuffer,
        .endCommandBuffer = &FakeEndCommandBuffer,
        .cmdBeginRenderPass = &FakeCmdBeginRenderPass,
        .cmdEndRenderPass = &FakeCmdEndRenderPass,
        .createSemaphore = &FakeCreateSemaphore,
        .destroySemaphore = &FakeDestroySemaphore,
        .createFence = &FakeCreateFence,
        .destroyFence = &FakeDestroyFence,
        .waitForFences = &FakeWaitForFences,
        .resetFences = &FakeResetFences,
        .acquireNextImage = &FakeAcquireNextImage,
        .queueSubmit = &FakeQueueSubmit,
        .queuePresent = &FakeQueuePresent,
        .waitForPresent = &FakeWaitForPresent,
        .setDebugUtilsObjectName = &FakeSetDebugUtilsObjectName,
        .cmdBeginDebugUtilsLabel = &FakeCmdBeginDebugUtilsLabel,
        .cmdEndDebugUtilsLabel = &FakeCmdEndDebugUtilsLabel,
        .createAllocator = &FakeCreateAllocator,
        .destroyAllocator = &FakeDestroyAllocator};
}

[[nodiscard]] pond::platform::NativeWindowHandle MakeNativeWindowHandle(
    pond::render::detail::VulkanWsiKind wsiKind) noexcept
{
    switch (wsiKind)
    {
    case pond::render::detail::VulkanWsiKind::Win32:
        return pond::platform::NativeWin32Window{.instance = reinterpret_cast<void*>(0x1111),
                                                 .window = reinterpret_cast<void*>(0x2222)};
    case pond::render::detail::VulkanWsiKind::X11:
        return pond::platform::NativeX11Window{.display = reinterpret_cast<void*>(0x3333),
                                               .window = 0x4444U};
    case pond::render::detail::VulkanWsiKind::Wayland:
        return pond::platform::NativeWaylandWindow{.display = reinterpret_cast<void*>(0x5555),
                                                   .surface = reinterpret_cast<void*>(0x6666)};
    }

    return pond::platform::NativeWin32Window{};
}

[[nodiscard]] pond::platform::NativeWindowHandle MakeInvalidNativeWindowHandle(
    pond::render::detail::VulkanWsiKind wsiKind) noexcept
{
    switch (wsiKind)
    {
    case pond::render::detail::VulkanWsiKind::Win32:
        return pond::platform::NativeWin32Window{.instance = nullptr,
                                                 .window = reinterpret_cast<void*>(0x2222)};
    case pond::render::detail::VulkanWsiKind::X11:
        return pond::platform::NativeX11Window{.display = reinterpret_cast<void*>(0x3333),
                                               .window = 0U};
    case pond::render::detail::VulkanWsiKind::Wayland:
        return pond::platform::NativeWaylandWindow{.display = nullptr,
                                                   .surface = reinterpret_cast<void*>(0x6666)};
    }

    return pond::platform::NativeWin32Window{};
}

[[nodiscard]] pond::render::RenderTargetDesc MakeTargetDesc(
    pond::render::PresentationPolicy policy = pond::render::PresentationPolicy::VSync,
    pond::render::QueuedFrameLatency latency = {},
    pond::platform::PixelSize pixelSize = pond::platform::PixelSize{800, 600}, bool visible = true,
    std::uint64_t revision = 1,
    pond::render::RequirementStrength policyStrength = pond::render::RequirementStrength::Preferred,
    pond::render::RequirementStrength latencyStrength =
        pond::render::RequirementStrength::Preferred)
{
    return pond::render::RenderTargetDesc{
        .targetSnapshot =
            pond::render::RenderTargetSnapshot{pond::platform::WindowId{42}, pixelSize, visible,
                                               pond::platform::WindowState::Normal,
                                               pond::render::PresentationEnvironmentRevision{1},
                                               revision},
        .presentation = {.policy = policy, .strength = policyStrength},
        .queuedLatency = {.maximumQueuedFrames = latency, .strength = latencyStrength}};
}
[[nodiscard]] std::shared_ptr<pond::render::detail::VulkanInstanceOwner> CreateSharedFakeInstance(
    const pond::render::detail::VulkanGlobalDispatch& dispatch,
    pond::render::detail::VulkanWsiKind wsiKind)
{
    auto ownerResult = pond::render::detail::CreateVulkanInstanceForWsi(
        dispatch, wsiKind, pond::render::RenderValidationMode::Default);
    if (!ownerResult)
    {
        return {};
    }

    return std::make_shared<pond::render::detail::VulkanInstanceOwner>(
        std::move(ownerResult).GetValue());
}

struct FakeFrameTestOwners final
{
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance;
    pond::render::detail::VulkanDeviceOwner device;
    pond::render::detail::VulkanSwapchainOwner swapchain;
    pond::render::detail::VulkanFrameResourcesOwner frameResources;
    pond::render::detail::VulkanPresentationTrackerOwner presentationTracker;
};

[[nodiscard]] std::unique_ptr<FakeFrameTestOwners> CreateFakeFrameTestOwners(
    const pond::render::detail::VulkanGlobalDispatch& dispatch)
{
    auto owners = std::make_unique<FakeFrameTestOwners>();
    owners->instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    if (owners->instance == nullptr)
    {
        return {};
    }

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selection = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, owners->instance, surface, pond::render::RenderAdapterSelectionDesc{});
    if (!selection)
    {
        return {};
    }

    auto device = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, owners->instance, surface, selection.GetValue(),
        pond::render::RenderDeviceDesc{});
    if (!device)
    {
        return {};
    }
    owners->device = std::move(device).GetValue();

    auto swapchain = pond::render::detail::CreateVulkanSwapchainForTarget(
        dispatch, owners->device, surface, owners->device.GetInfo().queuePlan, MakeTargetDesc());
    if (!swapchain)
    {
        return {};
    }
    owners->swapchain = std::move(swapchain).GetValue();

    auto frameResources = pond::render::detail::CreateVulkanFrameResourcesForTarget(
        dispatch, owners->device, owners->swapchain.GetConfig().windowId,
        owners->device.GetInfo().queuePlan,
        owners->swapchain.GetConfig().presentation.actualQueuedLatency,
        owners->swapchain.GetConfig().imageCount);
    if (!frameResources)
    {
        return {};
    }
    owners->frameResources = std::move(frameResources).GetValue();

    auto presentationTracker = pond::render::detail::CreateVulkanPresentationTrackerForTarget(
        dispatch, owners->device, owners->swapchain.GetConfig().windowId,
        owners->frameResources.GetSlotCount(), owners->swapchain.GetConfig().imageCount);
    if (!presentationTracker)
    {
        return {};
    }
    owners->presentationTracker = std::move(presentationTracker).GetValue();
    return owners;
}

struct FakePublicTarget final
{
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance;
    pond::render::RenderTarget target;
    VkSwapchainKHR initialSwapchain{VK_NULL_HANDLE};
};

[[nodiscard]] std::unique_ptr<FakePublicTarget> CreateFakePublicTarget(
    const pond::render::detail::VulkanGlobalDispatch& dispatch)
{
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    if (instance == nullptr)
    {
        return {};
    }

    auto surfaceResult = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
        dispatch, instance, MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::Win32));
    if (!surfaceResult)
    {
        return {};
    }
    pond::render::detail::VulkanSurfaceOwner surface = std::move(surfaceResult).GetValue();

    auto selection = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface.GetHandle(), pond::render::RenderAdapterSelectionDesc{});
    if (!selection)
    {
        return {};
    }

    auto device = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface.GetHandle(), selection.GetValue(),
        pond::render::RenderDeviceDesc{});
    if (!device)
    {
        return {};
    }
    pond::render::detail::VulkanDeviceOwner deviceOwner = std::move(device).GetValue();

    auto swapchain = pond::render::detail::CreateVulkanSwapchainForTarget(
        dispatch, deviceOwner, surface.GetHandle(), deviceOwner.GetInfo().queuePlan,
        MakeTargetDesc());
    if (!swapchain)
    {
        return {};
    }
    pond::render::detail::VulkanSwapchainOwner swapchainOwner = std::move(swapchain).GetValue();

    auto frameResources = pond::render::detail::CreateVulkanFrameResourcesForTarget(
        dispatch, deviceOwner, swapchainOwner.GetConfig().windowId, deviceOwner.GetInfo().queuePlan,
        swapchainOwner.GetConfig().presentation.actualQueuedLatency,
        swapchainOwner.GetFramebufferCount());
    if (!frameResources)
    {
        return {};
    }
    pond::render::detail::VulkanFrameResourcesOwner frameResourcesOwner =
        std::move(frameResources).GetValue();

    auto presentationTracker = pond::render::detail::CreateVulkanPresentationTrackerForTarget(
        dispatch, deviceOwner, swapchainOwner.GetConfig().windowId,
        frameResourcesOwner.GetSlotCount(), swapchainOwner.GetFramebufferCount());
    if (!presentationTracker)
    {
        return {};
    }

    const VkSwapchainKHR initialSwapchain = swapchainOwner.GetHandle();
    auto target = pond::render::detail::RenderBackendTestAccess::CreateTarget(
        dispatch, std::move(deviceOwner), std::move(surface), std::move(swapchainOwner),
        std::move(frameResourcesOwner), std::move(presentationTracker).GetValue(),
        MakeTargetDesc());
    if (!target)
    {
        return {};
    }

    auto owners = std::make_unique<FakePublicTarget>();
    owners->instance = std::move(instance);
    owners->target = std::move(target).GetValue();
    owners->initialSwapchain = initialSwapchain;
    return owners;
}

struct FakeTargetResourceSet final
{
    pond::render::detail::VulkanSurfaceOwner surface;
    pond::render::detail::VulkanSwapchainOwner swapchain;
    pond::render::detail::VulkanFrameResourcesOwner frameResources;
    pond::render::detail::VulkanPresentationTrackerOwner presentationTracker;
};

[[nodiscard]] std::unique_ptr<FakeTargetResourceSet> CreateFakeTargetResourceSet(
    const pond::render::detail::VulkanGlobalDispatch& dispatch,
    const std::shared_ptr<pond::render::detail::VulkanInstanceOwner>& instance,
    const pond::render::detail::VulkanDeviceOwner& device,
    const pond::render::RenderTargetDesc& desc)
{
    auto surfaceResult = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
        dispatch, instance, MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::Win32));
    if (!surfaceResult)
    {
        return {};
    }

    auto resources = std::make_unique<FakeTargetResourceSet>();
    resources->surface = std::move(surfaceResult).GetValue();
    auto swapchain = pond::render::detail::CreateVulkanSwapchainForTarget(
        dispatch, device, resources->surface.GetHandle(), device.GetInfo().queuePlan, desc);
    if (!swapchain)
    {
        return {};
    }
    resources->swapchain = std::move(swapchain).GetValue();

    auto frameResources = pond::render::detail::CreateVulkanFrameResourcesForTarget(
        dispatch, device, desc.targetSnapshot.GetWindowId(), device.GetInfo().queuePlan,
        resources->swapchain.GetConfig().presentation.actualQueuedLatency,
        resources->swapchain.GetFramebufferCount());
    if (!frameResources)
    {
        return {};
    }
    resources->frameResources = std::move(frameResources).GetValue();

    auto presentationTracker = pond::render::detail::CreateVulkanPresentationTrackerForTarget(
        dispatch, device, desc.targetSnapshot.GetWindowId(),
        resources->frameResources.GetSlotCount(), resources->swapchain.GetFramebufferCount());
    if (!presentationTracker)
    {
        return {};
    }
    resources->presentationTracker = std::move(presentationTracker).GetValue();
    return resources;
}

struct FakePublicLifecycle final
{
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance;
    pond::render::detail::RenderPublicLifecycleTestOwners owners;
    std::optional<pond::render::RenderTarget> additionalTarget;
};

[[nodiscard]] std::unique_ptr<FakePublicLifecycle> CreateFakePublicLifecycle(
    const pond::render::detail::VulkanGlobalDispatch& dispatch,
    std::optional<pond::render::RenderTargetDesc> additionalTargetDesc = std::nullopt)
{
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    if (instance == nullptr)
    {
        return {};
    }

    auto initialSurface = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
        dispatch, instance, MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::Win32));
    if (!initialSurface)
    {
        return {};
    }

    auto selection = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, initialSurface.GetValue().GetHandle(),
        pond::render::RenderAdapterSelectionDesc{});
    if (!selection)
    {
        return {};
    }

    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, initialSurface.GetValue().GetHandle(), selection.GetValue(),
        pond::render::RenderDeviceDesc{});
    if (!deviceResult)
    {
        return {};
    }
    pond::render::detail::VulkanDeviceOwner device = std::move(deviceResult).GetValue();

    const pond::render::RenderTargetDesc initialDesc = MakeTargetDesc();
    auto initialResources = std::make_unique<FakeTargetResourceSet>();
    initialResources->surface = std::move(initialSurface).GetValue();
    auto initialSwapchain = pond::render::detail::CreateVulkanSwapchainForTarget(
        dispatch, device, initialResources->surface.GetHandle(), device.GetInfo().queuePlan,
        initialDesc);
    if (!initialSwapchain)
    {
        return {};
    }
    initialResources->swapchain = std::move(initialSwapchain).GetValue();
    auto initialFrameResources = pond::render::detail::CreateVulkanFrameResourcesForTarget(
        dispatch, device, initialDesc.targetSnapshot.GetWindowId(), device.GetInfo().queuePlan,
        initialResources->swapchain.GetConfig().presentation.actualQueuedLatency,
        initialResources->swapchain.GetFramebufferCount());
    if (!initialFrameResources)
    {
        return {};
    }
    initialResources->frameResources = std::move(initialFrameResources).GetValue();
    auto initialPresentationTracker =
        pond::render::detail::CreateVulkanPresentationTrackerForTarget(
            dispatch, device, initialDesc.targetSnapshot.GetWindowId(),
            initialResources->frameResources.GetSlotCount(),
            initialResources->swapchain.GetFramebufferCount());
    if (!initialPresentationTracker)
    {
        return {};
    }
    initialResources->presentationTracker = std::move(initialPresentationTracker).GetValue();

    std::unique_ptr<FakeTargetResourceSet> additionalResources;
    if (additionalTargetDesc.has_value())
    {
        additionalResources =
            CreateFakeTargetResourceSet(dispatch, instance, device, *additionalTargetDesc);
        if (additionalResources == nullptr)
        {
            return {};
        }
    }

    auto ownersResult = pond::render::detail::RenderBackendTestAccess::CreatePublicLifecycleOwners(
        std::move(device), std::move(initialResources->surface),
        std::move(initialResources->swapchain), std::move(initialResources->frameResources),
        std::move(initialResources->presentationTracker), initialDesc);
    if (!ownersResult)
    {
        return {};
    }

    auto result = std::make_unique<FakePublicLifecycle>();
    result->instance = std::move(instance);
    result->owners = std::move(ownersResult).GetValue();
    if (additionalResources != nullptr)
    {
        auto additionalTarget =
            pond::render::detail::RenderBackendTestAccess::CreateAdditionalTarget(
                result->owners.device, std::move(additionalResources->surface),
                std::move(additionalResources->swapchain),
                std::move(additionalResources->frameResources),
                std::move(additionalResources->presentationTracker), *additionalTargetDesc);
        if (!additionalTarget)
        {
            return {};
        }
        result->additionalTarget = std::move(additionalTarget).GetValue();
    }
    return result;
}

struct FakeLiveResourceCounts final
{
    std::uint64_t instances{};
    std::uint64_t debugMessengers{};
    std::uint64_t surfaces{};
    std::uint64_t devices{};
    std::uint64_t allocators{};
    std::uint64_t swapchains{};
    std::uint64_t imageViews{};
    std::uint64_t renderPasses{};
    std::uint64_t framebuffers{};
    std::uint64_t commandPools{};
    std::uint64_t semaphores{};
    std::uint64_t fences{};

    [[nodiscard]] friend constexpr bool operator==(
        const FakeLiveResourceCounts& lhs, const FakeLiveResourceCounts& rhs) noexcept = default;
};

[[nodiscard]] FakeLiveResourceCounts GetLiveResourceCounts(const FakeVulkanState& state) noexcept
{
    return FakeLiveResourceCounts{
        .instances = state.nextInstanceValue - 0x1000U - state.destroyInstanceCalls,
        .debugMessengers =
            state.nextDebugMessengerValue - 0x2000U - state.destroyDebugMessengerCalls,
        .surfaces = state.nextSurfaceValue - 0x3000U - state.destroySurfaceCalls,
        .devices = state.nextDeviceValue - 0x7000U - state.destroyDeviceCalls,
        .allocators = state.nextAllocatorValue - 0x9000U - state.destroyAllocatorCalls,
        .swapchains = state.nextSwapchainValue - 0xA000U - state.destroySwapchainCalls,
        .imageViews = state.nextImageViewValue - 0xC000U - state.destroyImageViewCalls,
        .renderPasses = state.nextRenderPassValue - 0xD000U - state.destroyRenderPassCalls,
        .framebuffers = state.nextFramebufferValue - 0xE000U - state.destroyFramebufferCalls,
        .commandPools = state.nextCommandPoolValue - 0xF000U - state.destroyCommandPoolCalls,
        .semaphores = state.nextSemaphoreValue - 0x11000U - state.destroySemaphoreCalls,
        .fences = state.nextFenceValue - 0x12000U - state.destroyFenceCalls};
}

void ExpectNoLiveResources(const FakeVulkanState& state)
{
    EXPECT_EQ(GetLiveResourceCounts(state), FakeLiveResourceCounts{});
}

[[nodiscard]] std::uint32_t GetSurfaceCreateCallCount(
    const FakeVulkanState& state, pond::render::detail::VulkanWsiKind wsiKind) noexcept
{
    switch (wsiKind)
    {
    case pond::render::detail::VulkanWsiKind::Win32:
        return state.createWin32SurfaceCalls;
    case pond::render::detail::VulkanWsiKind::X11:
        return state.createX11SurfaceCalls;
    case pond::render::detail::VulkanWsiKind::Wayland:
        return state.createWaylandSurfaceCalls;
    }

    return 0U;
}
void ExpectRenderError(const pond::core::VoidResult& result,
                       pond::render::RenderErrorCode expectedCode)
{
    ASSERT_FALSE(result);
    EXPECT_EQ(result.GetError().GetCode(), pond::render::ToErrorCode(expectedCode));
}
template <typename ValueType>
void ExpectRenderError(const pond::core::Result<ValueType>& result,
                       pond::render::RenderErrorCode expectedCode)
{
    ASSERT_FALSE(result);
    EXPECT_EQ(result.GetError().GetCode(), pond::render::ToErrorCode(expectedCode));
}

TEST(RenderVulkanBootstrapTests, DoesNotOpenLoaderBeforeExplicitInitialization)
{
    FakeVulkanState state;
    [[maybe_unused]] const pond::render::detail::VulkanGlobalDispatch dispatch =
        MakeFakeDispatch(state);
    const pond::render::detail::VulkanInstanceBootstrap bootstrap;

    EXPECT_FALSE(bootstrap.IsInitialized());
    EXPECT_EQ(state.initializeCalls, 0U);
    EXPECT_EQ(state.createInstanceCalls, 0U);
}

TEST(RenderVulkanBootstrapTests, CreatesInstanceForWsiWithoutSurface)
{
    FakeVulkanState state;
    state.loaderVersion = VK_API_VERSION_1_3;
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);

    {
        pond::render::detail::VulkanInstanceBootstrap bootstrap;
        const pond::core::Result<pond::render::detail::VulkanInstanceInfo> info =
            bootstrap.EnsureInitialized(dispatch, pond::render::detail::VulkanWsiKind::Win32,
                                        pond::render::RenderValidationMode::Default);

        ASSERT_TRUE(info) << info.GetError().GetMessage();
        EXPECT_TRUE(bootstrap.IsInitialized());
        ASSERT_TRUE(bootstrap.GetInfo().has_value());
        EXPECT_EQ(info.GetValue().wsiKind, pond::render::detail::VulkanWsiKind::Win32);
        EXPECT_EQ(info.GetValue().apiVersion,
                  std::min(state.loaderVersion,
                           pond::render::detail::GetMaximumHeaderVulkanInstanceVersion()));
        EXPECT_EQ(state.lastRequestedApiVersion, info.GetValue().apiVersion);
        EXPECT_EQ(state.initializeCalls, 1U);
        EXPECT_EQ(state.enumerateVersionCalls, 1U);
        EXPECT_EQ(state.createInstanceCalls, 1U);
        EXPECT_EQ(state.loadInstanceCalls, 1U);
        ASSERT_EQ(state.lastEnabledExtensions.size(), 2U);
        EXPECT_EQ(state.lastEnabledExtensions[0], "VK_KHR_surface");
        EXPECT_EQ(state.lastEnabledExtensions[1], "VK_KHR_win32_surface");
        EXPECT_EQ(state.destroyInstanceCalls, 0U);
    }

    EXPECT_EQ(state.destroyInstanceCalls, 1U);
}

TEST(RenderVulkanBootstrapTests, ReportsMissingLoaderWithActionableError)
{
    FakeVulkanState state;
    state.initializeResult = VK_ERROR_INITIALIZATION_FAILED;
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    pond::render::detail::VulkanDiagnosticScope diagnosticScope;

    const pond::core::Result<pond::render::detail::VulkanInstanceOwner> result =
        pond::render::detail::CreateVulkanInstanceForWsi(
            dispatch, pond::render::detail::VulkanWsiKind::Win32,
            pond::render::RenderValidationMode::Default);

    ExpectRenderError(result, pond::render::RenderErrorCode::LoaderUnavailable);
    EXPECT_NE(result.GetError().GetMessage().find("Vulkan loader"), std::string_view::npos);
    const pond::render::OptionalBackendDiagnostic diagnostic = diagnosticScope.TakeLastFailure();
    ASSERT_TRUE(diagnostic.has_value());
    EXPECT_EQ(diagnostic->backend, pond::render::RenderBackendKind::Vulkan);
    EXPECT_EQ(diagnostic->renderCode, pond::render::RenderErrorCode::LoaderUnavailable);
    EXPECT_EQ(diagnostic->nativeCode, static_cast<std::int64_t>(VK_ERROR_INITIALIZATION_FAILED));
    EXPECT_EQ(diagnostic->symbolicName, "VK_ERROR_INITIALIZATION_FAILED");
    EXPECT_EQ(diagnostic->operation, "volkInitialize");
    EXPECT_EQ(state.createInstanceCalls, 0U);
}

TEST(RenderVulkanBootstrapTests, RejectsLoaderBelowVulkanTwelve)
{
    FakeVulkanState state;
    state.loaderVersion = VK_API_VERSION_1_1;
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);

    const pond::core::Result<pond::render::detail::VulkanInstanceOwner> result =
        pond::render::detail::CreateVulkanInstanceForWsi(
            dispatch, pond::render::detail::VulkanWsiKind::Win32,
            pond::render::RenderValidationMode::Default);

    ExpectRenderError(result, pond::render::RenderErrorCode::UnsupportedCapability);
    EXPECT_EQ(state.createInstanceCalls, 0U);
}

TEST(RenderVulkanBootstrapTests, RejectsMissingRequiredWsiExtension)
{
    FakeVulkanState state;
    state.extensions = {"VK_KHR_surface"};
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);

    const pond::core::Result<pond::render::detail::VulkanInstanceOwner> result =
        pond::render::detail::CreateVulkanInstanceForWsi(
            dispatch, pond::render::detail::VulkanWsiKind::Wayland,
            pond::render::RenderValidationMode::Default);

    ExpectRenderError(result, pond::render::RenderErrorCode::UnsupportedSurface);
    EXPECT_NE(result.GetError().GetMessage().find("VK_KHR_wayland_surface"),
              std::string_view::npos);
    EXPECT_EQ(state.createInstanceCalls, 0U);
}

TEST(RenderVulkanBootstrapTests, RollsBackAfterInstanceCreationFailure)
{
    FakeVulkanState state;
    state.createInstanceResult = VK_ERROR_INITIALIZATION_FAILED;
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    pond::render::detail::VulkanInstanceBootstrap bootstrap;

    const pond::core::Result<pond::render::detail::VulkanInstanceInfo> failed =
        bootstrap.EnsureInitialized(dispatch, pond::render::detail::VulkanWsiKind::X11,
                                    pond::render::RenderValidationMode::Default);

    ExpectRenderError(failed, pond::render::RenderErrorCode::BackendFailure);
    EXPECT_FALSE(bootstrap.IsInitialized());
    EXPECT_EQ(state.destroyInstanceCalls, 0U);

    state.createInstanceResult = VK_SUCCESS;
    const pond::core::Result<pond::render::detail::VulkanInstanceInfo> recovered =
        bootstrap.EnsureInitialized(dispatch, pond::render::detail::VulkanWsiKind::X11,
                                    pond::render::RenderValidationMode::Default);

    ASSERT_TRUE(recovered) << recovered.GetError().GetMessage();
    EXPECT_TRUE(bootstrap.IsInitialized());
    EXPECT_EQ(state.createInstanceCalls, 2U);
    bootstrap.Reset();
    EXPECT_EQ(state.destroyInstanceCalls, 1U);
}

TEST(RenderVulkanBootstrapTests, ReusesMatchingWsiAndRejectsMismatchedWsi)
{
    FakeVulkanState state;
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    pond::render::detail::VulkanInstanceBootstrap bootstrap;

    const pond::core::Result<pond::render::detail::VulkanInstanceInfo> first =
        bootstrap.EnsureInitialized(dispatch, pond::render::detail::VulkanWsiKind::Wayland,
                                    pond::render::RenderValidationMode::Default);
    ASSERT_TRUE(first) << first.GetError().GetMessage();

    const pond::core::Result<pond::render::detail::VulkanInstanceInfo> second =
        bootstrap.EnsureInitialized(dispatch, pond::render::detail::VulkanWsiKind::Wayland,
                                    pond::render::RenderValidationMode::Default);
    ASSERT_TRUE(second) << second.GetError().GetMessage();
    EXPECT_EQ(second.GetValue(), first.GetValue());
    EXPECT_EQ(state.createInstanceCalls, 1U);

    const pond::core::Result<pond::render::detail::VulkanInstanceInfo> mismatch =
        bootstrap.EnsureInitialized(dispatch, pond::render::detail::VulkanWsiKind::X11,
                                    pond::render::RenderValidationMode::Default);

    ExpectRenderError(mismatch, pond::render::RenderErrorCode::UnsupportedSurface);
    EXPECT_EQ(state.createInstanceCalls, 1U);
    bootstrap.Reset();
    EXPECT_EQ(state.destroyInstanceCalls, 1U);
}
TEST(RenderVulkanBootstrapTests, RejectsMismatchedValidationPolicyAfterInitialization)
{
    FakeVulkanState state;
    AddValidationSupport(state);
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    pond::render::detail::VulkanInstanceBootstrap bootstrap;

    const pond::core::Result<pond::render::detail::VulkanInstanceInfo> first =
        bootstrap.EnsureInitialized(dispatch, pond::render::detail::VulkanWsiKind::Win32,
                                    pond::render::RenderValidationMode::Disabled);
    ASSERT_TRUE(first) << first.GetError().GetMessage();

    const pond::core::Result<pond::render::detail::VulkanInstanceInfo> mismatch =
        bootstrap.EnsureInitialized(dispatch, pond::render::detail::VulkanWsiKind::Win32,
                                    pond::render::RenderValidationMode::Standard);

    ExpectRenderError(mismatch, pond::render::RenderErrorCode::InvalidState);
    EXPECT_EQ(state.createInstanceCalls, 1U);
}

TEST(RenderVulkanBootstrapTests, ExplicitDisabledDoesNotEnableValidationWhenSupportExists)
{
    FakeVulkanState state;
    AddValidationSupport(state);
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);

    const pond::core::Result<pond::render::detail::VulkanInstanceOwner> result =
        pond::render::detail::CreateVulkanInstanceForWsi(
            dispatch, pond::render::detail::VulkanWsiKind::Win32,
            pond::render::RenderValidationMode::Disabled);

    ASSERT_TRUE(result) << result.GetError().GetMessage();
    const pond::render::detail::VulkanInstanceInfo info = result.GetValue().GetInfo();
    EXPECT_EQ(info.requestedValidationMode, pond::render::RenderValidationMode::Disabled);
    EXPECT_EQ(info.enabledValidationMode, pond::render::RenderValidationMode::Disabled);
    EXPECT_FALSE(info.validationEnabled);
    EXPECT_FALSE(info.debugUtilsEnabled);
    EXPECT_FALSE(info.debugMessengerEnabled);
    EXPECT_FALSE(info.debugUtilityHooks.IsActive());
    EXPECT_TRUE(state.lastEnabledLayers.empty());
    EXPECT_FALSE(ContainsString(state.lastEnabledExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME));
    EXPECT_EQ(state.createDebugMessengerCalls, 0U);
}

TEST(RenderVulkanBootstrapTests, ExplicitStandardEnablesLayerDebugUtilsMessengerAndHooks)
{
    FakeVulkanState state;
    AddValidationSupport(state);
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);

    {
        const pond::core::Result<pond::render::detail::VulkanInstanceOwner> result =
            pond::render::detail::CreateVulkanInstanceForWsi(
                dispatch, pond::render::detail::VulkanWsiKind::Win32,
                pond::render::RenderValidationMode::Standard);

        ASSERT_TRUE(result) << result.GetError().GetMessage();
        const pond::render::detail::VulkanInstanceInfo info = result.GetValue().GetInfo();
        EXPECT_EQ(info.enabledValidationMode, pond::render::RenderValidationMode::Standard);
        EXPECT_TRUE(info.validationEnabled);
        EXPECT_TRUE(info.debugUtilsEnabled);
        EXPECT_TRUE(info.debugMessengerEnabled);
        EXPECT_FALSE(info.validationFeaturesEnabled);
        EXPECT_TRUE(info.debugUtilityHooks.objectNames);
        EXPECT_TRUE(info.debugUtilityHooks.commandLabels);
        EXPECT_FALSE(info.debugUtilityHooks.timingMarkers);
        EXPECT_FALSE(info.debugUtilityHooks.captureRegions);
        EXPECT_TRUE(info.debugUtilityHooks.IsActive());
        EXPECT_TRUE(ContainsString(state.lastEnabledLayers, "VK_LAYER_KHRONOS_validation"));
        EXPECT_TRUE(ContainsString(state.lastEnabledExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME));
        EXPECT_TRUE(state.debugCreateInfoChained);
        EXPECT_FALSE(state.validationFeaturesChained);
        EXPECT_NE(state.lastCallback, nullptr);
        EXPECT_NE(state.lastCallbackUserData, nullptr);
        EXPECT_EQ(state.createDebugMessengerCalls, 1U);
        EXPECT_EQ(state.destroyDebugMessengerCalls, 0U);
    }

    EXPECT_EQ(state.destroyDebugMessengerCalls, 1U);
    EXPECT_EQ(state.destroyInstanceCalls, 1U);
}

TEST(RenderVulkanBootstrapTests, RejectsMissingOwnerLocalDebugMessengerDispatch)
{
    FakeVulkanState state;
    AddValidationSupport(state);
    pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    ASSERT_NE(dispatch.createDebugUtilsMessenger, nullptr);
    dispatch.loadInstanceTable = &FakeLoadOwnerLocalInstanceTableWithoutDebugMessenger;

    const auto result = pond::render::detail::CreateVulkanInstanceForWsi(
        dispatch, pond::render::detail::VulkanWsiKind::Win32,
        pond::render::RenderValidationMode::Standard);

    ExpectRenderError(result, pond::render::RenderErrorCode::UnsupportedCapability);
    EXPECT_EQ(state.createDebugMessengerCalls, 0U);
    EXPECT_EQ(state.ownerLocalInstanceADestroyCalls, 1U);
    EXPECT_EQ(state.destroyInstanceCalls, 1U);
}

TEST(RenderVulkanBootstrapTests, ValidationReportCapturesBoundedUnexpectedMessageIds)
{
    FakeVulkanState state;
    AddValidationSupport(state);
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    auto result = pond::render::detail::CreateVulkanInstanceForWsi(
        dispatch, pond::render::detail::VulkanWsiKind::Win32,
        pond::render::RenderValidationMode::Standard);
    ASSERT_TRUE(result) << result.GetError().GetMessage();
    pond::render::detail::VulkanInstanceOwner owner = std::move(result).GetValue();
    ASSERT_NE(state.lastCallback, nullptr);
    ASSERT_NE(state.lastCallbackUserData, nullptr);

    const pond::core::ScopedMinimumLogLevel minimumLogLevel{pond::core::LogLevel::Fatal};
    VkDebugUtilsMessengerCallbackDataEXT callbackData{};
    callbackData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    callbackData.pMessageIdName = "BOUNDED-VALIDATION-ID";
    callbackData.pMessage = "synthetic bounded validation warning";
    constexpr std::size_t kMessageCount =
        pond::render::RenderValidationReport::kMaximumCapturedMessages + 2U;
    for (std::size_t index = 0U; index < kMessageCount; ++index)
    {
        callbackData.messageIdNumber = static_cast<std::int32_t>(index + 1U);
        EXPECT_EQ(state.lastCallback(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callbackData,
                                     state.lastCallbackUserData),
                  VK_FALSE);
    }

    const pond::render::RenderValidationReport report = owner.GetValidationReport();
    EXPECT_FALSE(report.IsClean());
    EXPECT_EQ(report.warningCount, kMessageCount);
    EXPECT_EQ(report.errorCount, 0U);
    EXPECT_EQ(report.capturedMessageCount,
              pond::render::RenderValidationReport::kMaximumCapturedMessages);
    EXPECT_EQ(report.droppedMessageCount, 2U);
    EXPECT_EQ(report.capturedMessages.front().severity,
              pond::render::RenderValidationMessageSeverity::Warning);
    EXPECT_EQ(report.capturedMessages.front().messageIdNumber, 1);
    EXPECT_EQ(report.capturedMessages.front().GetMessageIdName(), "BOUNDED-VALIDATION-ID");
    EXPECT_EQ(
        report.capturedMessages[report.capturedMessageCount - 1U].messageIdNumber,
        static_cast<std::int32_t>(pond::render::RenderValidationReport::kMaximumCapturedMessages));
}

TEST(RenderVulkanBootstrapTests, ReportsOnlyDebugHooksAvailableFromOwnerLocalDispatch)
{
    FakeVulkanState state;
    AddValidationSupport(state);
    pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    dispatch.loadInstanceTable = &FakeLoadOwnerLocalInstanceTableWithoutDebugHooks;

    const auto result = pond::render::detail::CreateVulkanInstanceForWsi(
        dispatch, pond::render::detail::VulkanWsiKind::Win32,
        pond::render::RenderValidationMode::Standard);

    ASSERT_TRUE(result) << result.GetError().GetMessage();
    const pond::render::detail::VulkanInstanceInfo info = result.GetValue().GetInfo();
    EXPECT_TRUE(info.debugUtilsEnabled);
    EXPECT_TRUE(info.debugMessengerEnabled);
    EXPECT_FALSE(info.debugUtilityHooks.objectNames);
    EXPECT_FALSE(info.debugUtilityHooks.commandLabels);
    EXPECT_FALSE(info.debugUtilityHooks.timingMarkers);
    EXPECT_FALSE(info.debugUtilityHooks.captureRegions);
    EXPECT_FALSE(info.debugUtilityHooks.IsActive());
}

TEST(RenderVulkanBootstrapTests, DefaultAutoEnablesStandardValidationInDeveloperBuildWhenPresent)
{
    FakeVulkanState state;
    AddValidationSupport(state);
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);

    const pond::core::Result<pond::render::detail::VulkanInstanceOwner> result =
        pond::render::detail::CreateVulkanInstanceForWsi(
            dispatch, pond::render::detail::VulkanWsiKind::Win32,
            pond::render::RenderValidationMode::Default);

    ASSERT_TRUE(result) << result.GetError().GetMessage();
    const pond::render::detail::VulkanInstanceInfo info = result.GetValue().GetInfo();
    EXPECT_EQ(info.requestedValidationMode, pond::render::RenderValidationMode::Default);
    if (pond::render::detail::IsDefaultValidationEnabledForDeveloperBuild())
    {
        EXPECT_EQ(info.enabledValidationMode, pond::render::RenderValidationMode::Standard);
        EXPECT_TRUE(info.validationEnabled);
        EXPECT_TRUE(info.debugMessengerEnabled);
        EXPECT_EQ(state.createDebugMessengerCalls, 1U);
    }
    else
    {
        EXPECT_EQ(info.enabledValidationMode, pond::render::RenderValidationMode::Disabled);
        EXPECT_FALSE(info.validationEnabled);
        EXPECT_FALSE(info.debugMessengerEnabled);
        EXPECT_EQ(state.createDebugMessengerCalls, 0U);
    }
}

TEST(RenderVulkanBootstrapTests, DefaultDoesNotFailWhenValidationSupportIsUnavailable)
{
    FakeVulkanState state;
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);

    const pond::core::Result<pond::render::detail::VulkanInstanceOwner> result =
        pond::render::detail::CreateVulkanInstanceForWsi(
            dispatch, pond::render::detail::VulkanWsiKind::Win32,
            pond::render::RenderValidationMode::Default);

    ASSERT_TRUE(result) << result.GetError().GetMessage();
    EXPECT_EQ(result.GetValue().GetInfo().enabledValidationMode,
              pond::render::RenderValidationMode::Disabled);
    EXPECT_FALSE(result.GetValue().GetInfo().validationEnabled);
    EXPECT_EQ(state.createDebugMessengerCalls, 0U);
}

TEST(RenderVulkanBootstrapTests, ExplicitStandardFailsWhenValidationLayerIsMissing)
{
    FakeVulkanState state;
    state.extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);

    const pond::core::Result<pond::render::detail::VulkanInstanceOwner> result =
        pond::render::detail::CreateVulkanInstanceForWsi(
            dispatch, pond::render::detail::VulkanWsiKind::Win32,
            pond::render::RenderValidationMode::Standard);

    ExpectRenderError(result, pond::render::RenderErrorCode::UnsupportedCapability);
    EXPECT_EQ(state.createInstanceCalls, 0U);
}

TEST(RenderVulkanBootstrapTests, ExplicitStandardFailsWhenDebugUtilsIsMissing)
{
    FakeVulkanState state;
    state.layers.push_back("VK_LAYER_KHRONOS_validation");
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);

    const pond::core::Result<pond::render::detail::VulkanInstanceOwner> result =
        pond::render::detail::CreateVulkanInstanceForWsi(
            dispatch, pond::render::detail::VulkanWsiKind::Win32,
            pond::render::RenderValidationMode::Standard);

    ExpectRenderError(result, pond::render::RenderErrorCode::UnsupportedCapability);
    EXPECT_EQ(state.createInstanceCalls, 0U);
}
TEST(RenderVulkanBootstrapTests, FeatureModesEnableExpectedValidationFeatures)
{
    constexpr std::array kFeatureCases{
        ValidationFeatureCase{pond::render::RenderValidationMode::Synchronization,
                              VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT},
        ValidationFeatureCase{pond::render::RenderValidationMode::BestPractices,
                              VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT},
        ValidationFeatureCase{pond::render::RenderValidationMode::GpuAssisted,
                              VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT}};

    for (const ValidationFeatureCase& featureCase : kFeatureCases)
    {
        FakeVulkanState state;
        AddValidationSupport(state);
        AddValidationFeaturesSupport(state);
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);

        const pond::core::Result<pond::render::detail::VulkanInstanceOwner> result =
            pond::render::detail::CreateVulkanInstanceForWsi(
                dispatch, pond::render::detail::VulkanWsiKind::Win32, featureCase.mode);

        ASSERT_TRUE(result) << result.GetError().GetMessage();
        const pond::render::detail::VulkanInstanceInfo info = result.GetValue().GetInfo();
        EXPECT_EQ(info.enabledValidationMode, featureCase.mode);
        EXPECT_TRUE(info.validationEnabled);
        EXPECT_TRUE(info.validationFeaturesEnabled);
        EXPECT_TRUE(state.validationFeaturesChained);
        EXPECT_TRUE(
            ContainsString(state.lastEnabledExtensions, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME));
        EXPECT_TRUE(
            ContainsValidationFeature(state.lastEnabledValidationFeatures, featureCase.feature));
    }
}

TEST(RenderVulkanBootstrapTests, FeatureModeFailsWhenValidationFeaturesExtensionIsMissing)
{
    FakeVulkanState state;
    AddValidationSupport(state);
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);

    const pond::core::Result<pond::render::detail::VulkanInstanceOwner> result =
        pond::render::detail::CreateVulkanInstanceForWsi(
            dispatch, pond::render::detail::VulkanWsiKind::Win32,
            pond::render::RenderValidationMode::Synchronization);

    ExpectRenderError(result, pond::render::RenderErrorCode::UnsupportedCapability);
    EXPECT_EQ(state.createInstanceCalls, 0U);
}

TEST(RenderVulkanBootstrapTests, DebugMessengerCreationFailureRollsBackInstance)
{
    FakeVulkanState state;
    AddValidationSupport(state);
    state.createDebugMessengerResult = VK_ERROR_INITIALIZATION_FAILED;
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);

    const pond::core::Result<pond::render::detail::VulkanInstanceOwner> result =
        pond::render::detail::CreateVulkanInstanceForWsi(
            dispatch, pond::render::detail::VulkanWsiKind::Win32,
            pond::render::RenderValidationMode::Standard);

    ExpectRenderError(result, pond::render::RenderErrorCode::BackendFailure);
    EXPECT_EQ(state.createInstanceCalls, 1U);
    EXPECT_EQ(state.loadInstanceCalls, 1U);
    EXPECT_EQ(state.createDebugMessengerCalls, 1U);
    EXPECT_EQ(state.destroyDebugMessengerCalls, 0U);
    EXPECT_EQ(state.destroyInstanceCalls, 1U);
}

TEST(RenderVulkanBootstrapTests, DebugCallbackRoutesWarningWithContextAndMarksFailure)
{
    g_capturedLogEntries.clear();
    const pond::core::ScopedMinimumLogLevel minimumLogLevel{pond::core::LogLevel::Trace};
    const pond::core::ScopedLogSinkHandler logSink{&CaptureLogEntry};

    pond::render::detail::VulkanDebugMessengerContext context;
    context.currentOperation = "unit-test-operation";

    VkDebugUtilsObjectNameInfoEXT objectInfo{};
    objectInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    objectInfo.objectType = VK_OBJECT_TYPE_INSTANCE;
    objectInfo.objectHandle = 0x1234U;
    objectInfo.pObjectName = "unit-object";

    VkDebugUtilsMessengerCallbackDataEXT callbackData{};
    callbackData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    callbackData.pMessageIdName = "VALIDATION-UNIT";
    callbackData.messageIdNumber = 123;
    callbackData.pMessage = "synthetic warning";
    callbackData.objectCount = 1U;
    callbackData.pObjects = &objectInfo;

    const VkBool32 result = pond::render::detail::HandleVulkanDebugUtilsMessage(
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callbackData, &context);

    EXPECT_EQ(result, VK_FALSE);
    pond::core::FlushLog();
    EXPECT_TRUE(context.warningOrErrorSeen.load(std::memory_order_relaxed));
    ASSERT_EQ(g_capturedLogEntries.size(), 1U);
    EXPECT_EQ(g_capturedLogEntries[0].GetLevel(), pond::core::LogLevel::Warning);
    EXPECT_EQ(g_capturedLogEntries[0].GetCategory(), "render.vulkan.validation");
    EXPECT_NE(g_capturedLogEntries[0].GetMessage().find("unit-test-operation"),
              std::string_view::npos);
    EXPECT_NE(g_capturedLogEntries[0].GetMessage().find("VALIDATION-UNIT"), std::string_view::npos);
    EXPECT_NE(g_capturedLogEntries[0].GetMessage().find("numericCode=123"), std::string_view::npos);
    EXPECT_NE(g_capturedLogEntries[0].GetMessage().find("unit-object"), std::string_view::npos);
    EXPECT_NE(g_capturedLogEntries[0].GetMessage().find("synthetic warning"),
              std::string_view::npos);
}

TEST(RenderVulkanBootstrapTests, ExactDebugMessageFilterSuppressesFailureAndLog)
{
    g_capturedLogEntries.clear();
    const pond::core::ScopedMinimumLogLevel minimumLogLevel{pond::core::LogLevel::Trace};
    const pond::core::ScopedLogSinkHandler logSink{&CaptureLogEntry};

    pond::render::detail::VulkanDebugMessengerContext context;
    context.exactMessageFilters.push_back(pond::render::detail::VulkanValidationMessageFilter{
        .messageIdName = "FILTERED-ID", .messageIdNumber = 456});

    VkDebugUtilsMessengerCallbackDataEXT callbackData{};
    callbackData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    callbackData.pMessageIdName = "FILTERED-ID";
    callbackData.messageIdNumber = 456;
    callbackData.pMessage = "synthetic filtered error";

    const VkBool32 result = pond::render::detail::HandleVulkanDebugUtilsMessage(
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT, &callbackData, &context);

    EXPECT_EQ(result, VK_FALSE);
    pond::core::FlushLog();
    EXPECT_FALSE(context.warningOrErrorSeen.load(std::memory_order_relaxed));
    EXPECT_EQ(context.warningCount.load(std::memory_order_relaxed), 0U);
    EXPECT_EQ(context.errorCount.load(std::memory_order_relaxed), 0U);
    EXPECT_EQ(context.reservedMessageCount.load(std::memory_order_relaxed), 0U);
    EXPECT_TRUE(g_capturedLogEntries.empty());
}
TEST(RenderVulkanBootstrapTests, CreatesAndDestroysSurfaceForEachNativeAlternative)
{
    constexpr std::array kWsiKinds{pond::render::detail::VulkanWsiKind::Win32,
                                   pond::render::detail::VulkanWsiKind::X11,
                                   pond::render::detail::VulkanWsiKind::Wayland};

    for (const pond::render::detail::VulkanWsiKind wsiKind : kWsiKinds)
    {
        FakeVulkanState state;
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
        std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
            CreateSharedFakeInstance(dispatch, wsiKind);
        ASSERT_NE(instance, nullptr);

        {
            auto surfaceResult = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
                dispatch, instance, MakeNativeWindowHandle(wsiKind));

            ASSERT_TRUE(surfaceResult) << surfaceResult.GetError().GetMessage();
            pond::render::detail::VulkanSurfaceOwner surface = std::move(surfaceResult).GetValue();
            EXPECT_TRUE(surface.IsValid());
            EXPECT_EQ(surface.GetInfo().wsiKind, wsiKind);
            EXPECT_EQ(state.lastCreatedSurfaceWsiKind, wsiKind);
            EXPECT_EQ(GetSurfaceCreateCallCount(state, wsiKind), 1U);
            EXPECT_EQ(state.destroySurfaceCalls, 0U);
        }

        EXPECT_EQ(state.destroySurfaceCalls, 1U);
        instance.reset();
        EXPECT_EQ(state.destroyInstanceCalls, 1U);
    }
}

TEST(RenderVulkanBootstrapTests, RejectsInvalidNativePayloadBeforeNativeSurfaceCall)
{
    constexpr std::array kWsiKinds{pond::render::detail::VulkanWsiKind::Win32,
                                   pond::render::detail::VulkanWsiKind::X11,
                                   pond::render::detail::VulkanWsiKind::Wayland};

    for (const pond::render::detail::VulkanWsiKind wsiKind : kWsiKinds)
    {
        FakeVulkanState state;
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
        std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
            CreateSharedFakeInstance(dispatch, wsiKind);
        ASSERT_NE(instance, nullptr);

        const auto surfaceResult = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
            dispatch, instance, MakeInvalidNativeWindowHandle(wsiKind));

        ExpectRenderError(surfaceResult, pond::render::RenderErrorCode::InvalidArgument);
        EXPECT_EQ(GetSurfaceCreateCallCount(state, wsiKind), 0U);
        EXPECT_EQ(state.destroySurfaceCalls, 0U);
    }
}

TEST(RenderVulkanBootstrapTests, RejectsWrongWsiBeforeNativeSurfaceCall)
{
    FakeVulkanState state;
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const auto surfaceResult = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
        dispatch, instance, MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::X11));

    ExpectRenderError(surfaceResult, pond::render::RenderErrorCode::UnsupportedSurface);
    EXPECT_EQ(state.createWin32SurfaceCalls, 0U);
    EXPECT_EQ(state.createX11SurfaceCalls, 0U);
    EXPECT_EQ(state.createWaylandSurfaceCalls, 0U);
    EXPECT_EQ(state.destroySurfaceCalls, 0U);
}

TEST(RenderVulkanBootstrapTests, SurfaceCreationFailureDoesNotLeakSurface)
{
    FakeVulkanState state;
    state.createWin32SurfaceResult = VK_ERROR_NATIVE_WINDOW_IN_USE_KHR;
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const auto surfaceResult = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
        dispatch, instance, MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::Win32));

    ExpectRenderError(surfaceResult, pond::render::RenderErrorCode::UnsupportedSurface);
    EXPECT_EQ(state.createWin32SurfaceCalls, 1U);
    EXPECT_EQ(state.destroySurfaceCalls, 0U);
}

TEST(RenderVulkanBootstrapTests, SurfaceOwnerKeepsSharedInstanceAliveUntilSurfaceDestruction)
{
    FakeVulkanState state;
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    pond::render::detail::VulkanSurfaceOwner retainedSurface;

    {
        pond::render::detail::VulkanInstanceBootstrap bootstrap;
        const auto info =
            bootstrap.EnsureInitialized(dispatch, pond::render::detail::VulkanWsiKind::Win32,
                                        pond::render::RenderValidationMode::Default);
        ASSERT_TRUE(info) << info.GetError().GetMessage();

        std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance = bootstrap.GetOwner();
        ASSERT_NE(instance, nullptr);
        auto surfaceResult = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
            dispatch, instance, MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::Win32));
        ASSERT_TRUE(surfaceResult) << surfaceResult.GetError().GetMessage();
        retainedSurface = std::move(surfaceResult).GetValue();

        bootstrap.Reset();
        instance.reset();
        EXPECT_TRUE(retainedSurface.IsValid());
        EXPECT_EQ(state.destroySurfaceCalls, 0U);
        EXPECT_EQ(state.destroyInstanceCalls, 0U);
    }

    EXPECT_TRUE(retainedSurface.IsValid());
    EXPECT_EQ(state.destroyInstanceCalls, 0U);
    retainedSurface.Reset();
    EXPECT_EQ(state.destroySurfaceCalls, 1U);
    EXPECT_EQ(state.destroyInstanceCalls, 1U);
}

TEST(RenderVulkanBootstrapTests, SurfaceCreationFailsWhenDestroyDispatchIsMissing)
{
    FakeVulkanState state;
    pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    dispatch.destroySurface = nullptr;
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const auto surfaceResult = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
        dispatch, instance, MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::Win32));

    ExpectRenderError(surfaceResult, pond::render::RenderErrorCode::UnsupportedCapability);
    EXPECT_EQ(state.createWin32SurfaceCalls, 0U);
    EXPECT_EQ(state.destroySurfaceCalls, 0U);
}

[[nodiscard]] pond::render::RenderAdapterId MakeFakeAdapterId(std::uint8_t uuidSeed) noexcept
{
    pond::render::RenderAdapterId id{};
    id.deviceUuid[0] = uuidSeed;
    id.deviceUuid[1] = static_cast<std::uint8_t>(uuidSeed + 1U);
    return id;
}

TEST(RenderVulkanBootstrapTests,
     SelectsDiscreteAdapterForDefaultPreferenceAndRejectsImplicitSoftware)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, "Integrated GPU"));
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5002U, 2U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Discrete GPU"));
    state.physicalDevices.push_back(
        MakeCompatibleFakeDevice(0x5003U, 3U, VK_PHYSICAL_DEVICE_TYPE_CPU, "Software Rasterizer"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, MakeFakeHandle<VkSurfaceKHR>(0x6000U),
        pond::render::RenderAdapterSelectionDesc{});

    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    const pond::render::RenderAdapterSelection selection = selectionResult.GetValue();
    EXPECT_TRUE(pond::render::IsValid(selection));
    EXPECT_EQ(selection.selectedAdapter.identity.adapterType,
              pond::render::RenderAdapterType::DiscreteGpu);
    EXPECT_EQ(selection.selectedAdapter.identity.name, "Discrete GPU");
    ASSERT_EQ(selection.compatibleAdapters.size(), 2U);
    EXPECT_EQ(selection.compatibleAdapters[0].identity.name, "Discrete GPU");
    EXPECT_EQ(selection.compatibleAdapters[1].identity.name, "Integrated GPU");
    ASSERT_EQ(selection.rejectedAdapters.size(), 1U);
    EXPECT_EQ(selection.rejectedAdapters[0].identity.adapterType,
              pond::render::RenderAdapterType::Cpu);
    ASSERT_FALSE(selection.rejectedAdapters[0].reasons.empty());
    EXPECT_NE(selection.rejectedAdapters[0].reasons[0].find("Software adapters require"),
              std::string::npos);
    EXPECT_FALSE(selection.selectedByPreferenceFallback);
    EXPECT_EQ(state.enumeratePhysicalDevicesCalls, 2U);
    EXPECT_GT(state.getSurfaceSupportCalls, 0U);
}

TEST(RenderVulkanBootstrapTests, SelectsIntegratedAdapterForLowPowerPreference)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5002U, 2U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Discrete GPU"));
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, "Integrated GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, MakeFakeHandle<VkSurfaceKHR>(0x6000U),
        pond::render::RenderAdapterSelectionDesc{
            .adapterPreference = pond::render::RenderAdapterPreference::LowPower});

    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    EXPECT_EQ(selectionResult.GetValue().selectedAdapter.identity.adapterType,
              pond::render::RenderAdapterType::IntegratedGpu);
    EXPECT_EQ(selectionResult.GetValue().selectedAdapter.identity.name, "Integrated GPU");
    EXPECT_FALSE(selectionResult.GetValue().selectedByPreferenceFallback);
}

TEST(RenderVulkanBootstrapTests, SurfaceEnumerationRetriesIncompleteReads)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Enumeration Retry GPU"));
    state.surfaceFormatIncompleteReadsRemaining = 1U;
    state.presentModeIncompleteReadsRemaining = 1U;

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, MakeFakeHandle<VkSurfaceKHR>(0x6000U),
        pond::render::RenderAdapterSelectionDesc{});

    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    EXPECT_EQ(state.getSurfaceFormatsCalls, 4U);
    EXPECT_EQ(state.getPresentModesCalls, 4U);
    ASSERT_EQ(selectionResult.GetValue().compatibleAdapters.size(), 1U);
    const pond::render::RenderPresentationCapabilities& presentation =
        selectionResult.GetValue().compatibleAdapters[0].presentation;
    EXPECT_TRUE(presentation.supportsWindowPresentation);
    EXPECT_TRUE(presentation.supportsOpaqueSdrSrgbOutput);
    EXPECT_EQ(presentation.supportedPolicies,
              (std::vector<pond::render::PresentationPolicy>{
                  pond::render::PresentationPolicy::VSync,
                  pond::render::PresentationPolicy::LowLatencyVSync}));
}

TEST(RenderVulkanBootstrapTests, AdapterSelectionFiltersOutputContractBeforePreferenceRanking)
{
    FakeVulkanState state;
    FakePhysicalDevice incompatibleDiscrete = MakeCompatibleFakeDevice(
        0x5002U, 2U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Incompatible Discrete GPU");
    incompatibleDiscrete.surfaceFormats = {VkSurfaceFormatKHR{
        .format = VK_FORMAT_B8G8R8A8_UNORM, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    incompatibleDiscrete.surfaceCapabilities.supportedCompositeAlpha =
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    state.physicalDevices.push_back(std::move(incompatibleDiscrete));
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, "Compatible Integrated GPU"));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, MakeFakeHandle<VkSurfaceKHR>(0x6000U),
        pond::render::RenderAdapterSelectionDesc{
            .adapterPreference = pond::render::RenderAdapterPreference::HighPerformance});

    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    EXPECT_EQ(selectionResult.GetValue().selectedAdapter.identity.name,
              "Compatible Integrated GPU");
    EXPECT_TRUE(selectionResult.GetValue().selectedByPreferenceFallback);
    ASSERT_EQ(selectionResult.GetValue().compatibleAdapters.size(), 1U);
    ASSERT_EQ(selectionResult.GetValue().rejectedAdapters.size(), 1U);
    EXPECT_TRUE(std::ranges::any_of(selectionResult.GetValue().rejectedAdapters[0].reasons,
                                    [](const std::string& reason)
                                    {
                                        return reason.find("SDR sRGB") != std::string::npos;
                                    }));
    EXPECT_TRUE(std::ranges::any_of(selectionResult.GetValue().rejectedAdapters[0].reasons,
                                    [](const std::string& reason)
                                    {
                                        return reason.find("opaque composition") !=
                                               std::string::npos;
                                    }));
}
TEST(RenderVulkanBootstrapTests, RecordsFallbackWhenHighPerformanceUsesIntegratedAdapter)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, "Integrated GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, MakeFakeHandle<VkSurfaceKHR>(0x6000U),
        pond::render::RenderAdapterSelectionDesc{
            .adapterPreference = pond::render::RenderAdapterPreference::HighPerformance});

    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    EXPECT_EQ(selectionResult.GetValue().selectedAdapter.identity.adapterType,
              pond::render::RenderAdapterType::IntegratedGpu);
    EXPECT_TRUE(selectionResult.GetValue().selectedByPreferenceFallback);
}

TEST(RenderVulkanBootstrapTests, SelectsSoftwareAdapterOnlyWhenRequested)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, "Integrated GPU"));
    state.physicalDevices.push_back(
        MakeCompatibleFakeDevice(0x5003U, 3U, VK_PHYSICAL_DEVICE_TYPE_CPU, "Software Rasterizer"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, MakeFakeHandle<VkSurfaceKHR>(0x6000U),
        pond::render::RenderAdapterSelectionDesc{
            .adapterPreference = pond::render::RenderAdapterPreference::Software});

    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    EXPECT_EQ(selectionResult.GetValue().selectedAdapter.identity.adapterType,
              pond::render::RenderAdapterType::Cpu);
    ASSERT_EQ(selectionResult.GetValue().rejectedAdapters.size(), 1U);
    EXPECT_EQ(selectionResult.GetValue().rejectedAdapters[0].identity.adapterType,
              pond::render::RenderAdapterType::IntegratedGpu);
    EXPECT_NE(selectionResult.GetValue().rejectedAdapters[0].reasons[0].find("excludes hardware"),
              std::string::npos);
}

TEST(RenderVulkanBootstrapTests, SelectsExplicitAdapterIdWithoutRelyingOnEnumerationOrder)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5002U, 2U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Discrete GPU"));
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, "Integrated GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, MakeFakeHandle<VkSurfaceKHR>(0x6000U),
        pond::render::RenderAdapterSelectionDesc{
            .adapterPreference = pond::render::RenderAdapterPreference::HighPerformance,
            .explicitAdapterId = MakeFakeAdapterId(1U)});

    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    EXPECT_EQ(selectionResult.GetValue().selectedAdapter.identity.name, "Integrated GPU");
    ASSERT_EQ(selectionResult.GetValue().compatibleAdapters.size(), 1U);
    ASSERT_EQ(selectionResult.GetValue().rejectedAdapters.size(), 1U);
    EXPECT_NE(selectionResult.GetValue().rejectedAdapters[0].reasons[0].find("explicit adapter ID"),
              std::string::npos);
}

TEST(RenderVulkanBootstrapTests, OrdersEqualRankAdaptersDeterministically)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(
        MakeCompatibleFakeDevice(0x5002U, 2U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Discrete B"));
    state.physicalDevices.push_back(
        MakeCompatibleFakeDevice(0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Discrete A"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, MakeFakeHandle<VkSurfaceKHR>(0x6000U),
        pond::render::RenderAdapterSelectionDesc{});

    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    ASSERT_EQ(selectionResult.GetValue().compatibleAdapters.size(), 2U);
    EXPECT_EQ(selectionResult.GetValue().compatibleAdapters[0].identity.name, "Discrete A");
    EXPECT_EQ(selectionResult.GetValue().compatibleAdapters[1].identity.name, "Discrete B");
}

TEST(RenderVulkanBootstrapTests, RejectsAdapterBelowApiFloorAndMissingSurfaceRequirements)
{
    FakeVulkanState state;
    FakePhysicalDevice rejected =
        MakeCompatibleFakeDevice(0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, "Broken GPU");
    rejected.apiVersion = VK_API_VERSION_1_1;
    rejected.surfaceSupport[0] = VK_FALSE;
    rejected.deviceExtensions.clear();
    state.physicalDevices.push_back(std::move(rejected));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, MakeFakeHandle<VkSurfaceKHR>(0x6000U),
        pond::render::RenderAdapterSelectionDesc{});

    ExpectRenderError(selectionResult, pond::render::RenderErrorCode::NoCompatibleAdapter);
    const std::string message = std::string{selectionResult.GetError().GetMessage()};
    EXPECT_NE(message.find("below the required Vulkan"), std::string::npos);
    EXPECT_NE(message.find("cannot present"), std::string::npos);
    EXPECT_NE(message.find(VK_KHR_SWAPCHAIN_EXTENSION_NAME), std::string::npos);
}

TEST(RenderVulkanBootstrapTests, ReportsNoCompatibleAdapterWhenEnumerationIsEmpty)
{
    FakeVulkanState state;
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, MakeFakeHandle<VkSurfaceKHR>(0x6000U),
        pond::render::RenderAdapterSelectionDesc{});

    ExpectRenderError(selectionResult, pond::render::RenderErrorCode::NoCompatibleAdapter);
    EXPECT_NE(
        std::string{selectionResult.GetError().GetMessage()}.find("No Vulkan physical devices"),
        std::string::npos);
}
TEST(RenderVulkanBootstrapTests, CreatesLogicalDeviceWithEveryNonemptyQueueFamilyAndVma)
{
    FakeVulkanState state;
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Queue Plan GPU");
    device.queueFamilies = {
        VkQueueFamilyProperties{.queueFlags = VK_QUEUE_GRAPHICS_BIT, .queueCount = 1U},
        VkQueueFamilyProperties{.queueFlags = VK_QUEUE_TRANSFER_BIT, .queueCount = 2U},
        VkQueueFamilyProperties{.queueFlags = {}, .queueCount = 1U}};
    device.surfaceSupport = {VK_FALSE, VK_FALSE, VK_TRUE};
    state.physicalDevices.push_back(std::move(device));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();

    {
        auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
            dispatch, instance, surface, selectionResult.GetValue(),
            pond::render::RenderDeviceDesc{});
        ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
        pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

        EXPECT_TRUE(owner.IsValid());
        const pond::render::detail::VulkanDeviceInfo info = owner.GetInfo();
        EXPECT_TRUE(pond::render::detail::IsValidVulkanDeviceQueuePlan(info.queuePlan));
        EXPECT_EQ(info.queuePlan.graphicsQueueFamilyIndex, 0U);
        EXPECT_EQ(info.queuePlan.presentationQueueFamilyIndex, 2U);
        EXPECT_TRUE(info.queuePlan.usesDistinctPresentationQueue);
        const std::vector<std::uint32_t> expectedFamilies{0U, 1U, 2U};
        EXPECT_EQ(info.queuePlan.provisionedQueueFamilyIndices, expectedFamilies);
        EXPECT_EQ(state.lastDeviceQueueFamilyIndices, expectedFamilies);
        EXPECT_TRUE(info.optionalCapabilities.vmaAllocator);
        EXPECT_TRUE(
            ContainsString(state.lastDeviceEnabledExtensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME));
        EXPECT_EQ(state.createDeviceCalls, 1U);
        EXPECT_EQ(state.loadDeviceCalls, 1U);
        EXPECT_EQ(state.getDeviceQueueCalls, 3U);
        EXPECT_EQ(state.createAllocatorCalls, 1U);
    }

    EXPECT_EQ(state.destroyAllocatorCalls, 1U);
    EXPECT_EQ(state.destroyDeviceCalls, 1U);
}

TEST(RenderVulkanBootstrapTests, EnablesOptionalPresentCompletionOnlyWhenExtensionsAndFeaturesMatch)
{
    FakeVulkanState state;
    AddSurfaceMaintenanceSupport(state);
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Optional Feature GPU");
    device.deviceExtensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    device.deviceExtensions.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
    device.deviceExtensions.push_back(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
    device.swapchainMaintenance1Feature = true;
    device.presentIdFeature = true;
    device.presentWaitFeature = true;
    state.physicalDevices.push_back(std::move(device));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    EXPECT_TRUE(instance->GetInfo().surfaceMaintenance1Enabled);
    EXPECT_FALSE(instance->GetInfo().khrSurfaceMaintenance1Enabled);
    EXPECT_TRUE(instance->GetInfo().extSurfaceMaintenance1Enabled);
    EXPECT_TRUE(ContainsString(state.lastEnabledExtensions,
                               VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME));
    EXPECT_TRUE(
        ContainsString(state.lastEnabledExtensions, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME));
    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();

    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();
    const pond::render::detail::VulkanDeviceOptionalCapabilities capabilities =
        owner.GetInfo().optionalCapabilities;

    EXPECT_TRUE(capabilities.swapchainMaintenance1);
    EXPECT_TRUE(capabilities.presentId);
    EXPECT_TRUE(capabilities.presentWait);
    EXPECT_FALSE(ContainsString(state.lastDeviceEnabledExtensions,
                                VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME));
    EXPECT_TRUE(ContainsString(state.lastDeviceEnabledExtensions,
                               VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME));
    EXPECT_TRUE(
        ContainsString(state.lastDeviceEnabledExtensions, VK_KHR_PRESENT_ID_EXTENSION_NAME));
    EXPECT_TRUE(
        ContainsString(state.lastDeviceEnabledExtensions, VK_KHR_PRESENT_WAIT_EXTENSION_NAME));
    EXPECT_TRUE(state.swapchainMaintenance1FeaturesChained);
    EXPECT_TRUE(state.presentIdFeaturesChained);
    EXPECT_TRUE(state.presentWaitFeaturesChained);
    EXPECT_TRUE(state.lastEnabledSwapchainMaintenance1Feature);
    EXPECT_TRUE(state.lastEnabledPresentIdFeature);
    EXPECT_TRUE(state.lastEnabledPresentWaitFeature);
}

TEST(RenderVulkanBootstrapTests, PrefersKhrSwapchainMaintenanceWhenBothMaintenancePathsAreAvailable)
{
    FakeVulkanState state;
    AddSurfaceMaintenanceSupport(state);
    state.extensions.push_back(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Promoted Maintenance GPU");
    device.deviceExtensions.push_back(VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    device.deviceExtensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    device.swapchainMaintenance1Feature = true;
    state.physicalDevices.push_back(std::move(device));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const pond::render::detail::VulkanInstanceInfo instanceInfo = instance->GetInfo();
    EXPECT_TRUE(instanceInfo.surfaceMaintenance1Enabled);
    EXPECT_TRUE(instanceInfo.khrSurfaceMaintenance1Enabled);
    EXPECT_TRUE(instanceInfo.extSurfaceMaintenance1Enabled);
    EXPECT_TRUE(ContainsString(state.lastEnabledExtensions,
                               VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME));
    EXPECT_TRUE(
        ContainsString(state.lastEnabledExtensions, VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME));
    EXPECT_TRUE(
        ContainsString(state.lastEnabledExtensions, VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME));

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    EXPECT_TRUE(owner.GetInfo().optionalCapabilities.swapchainMaintenance1);
    EXPECT_TRUE(ContainsString(state.lastDeviceEnabledExtensions,
                               VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME));
    EXPECT_FALSE(ContainsString(state.lastDeviceEnabledExtensions,
                                VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME));
    EXPECT_TRUE(state.swapchainMaintenance1FeaturesChained);
    EXPECT_TRUE(state.lastEnabledSwapchainMaintenance1Feature);
}

TEST(RenderVulkanBootstrapTests, SwapchainMaintenanceRequiresBothInstancePrerequisites)
{
    struct PrerequisiteCase final
    {
        bool getSurfaceCapabilities2{};
        bool surfaceMaintenance1{};
    };

    constexpr std::array<PrerequisiteCase, 3> cases{
        PrerequisiteCase{}, PrerequisiteCase{.getSurfaceCapabilities2 = true},
        PrerequisiteCase{.surfaceMaintenance1 = true}};

    for (const PrerequisiteCase prerequisite : cases)
    {
        SCOPED_TRACE(::testing::Message()
                     << "getSurfaceCapabilities2=" << prerequisite.getSurfaceCapabilities2
                     << " surfaceMaintenance1=" << prerequisite.surfaceMaintenance1);
        FakeVulkanState state;
        if (prerequisite.getSurfaceCapabilities2)
        {
            state.extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
        }
        if (prerequisite.surfaceMaintenance1)
        {
            state.extensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
        }

        FakePhysicalDevice device = MakeCompatibleFakeDevice(
            0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Maintenance Prerequisite GPU");
        device.deviceExtensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
        device.swapchainMaintenance1Feature = true;
        state.physicalDevices.push_back(std::move(device));

        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
        std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
            CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
        ASSERT_NE(instance, nullptr);
        EXPECT_FALSE(instance->GetInfo().khrSurfaceMaintenance1Enabled);
        EXPECT_FALSE(instance->GetInfo().extSurfaceMaintenance1Enabled);
        EXPECT_FALSE(instance->GetInfo().surfaceMaintenance1Enabled);

        const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
        auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
            dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
        ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
        auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
            dispatch, instance, surface, selectionResult.GetValue(),
            pond::render::RenderDeviceDesc{});
        ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
        const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

        EXPECT_FALSE(owner.GetInfo().optionalCapabilities.swapchainMaintenance1);
        EXPECT_FALSE(ContainsString(state.lastDeviceEnabledExtensions,
                                    VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME));
        EXPECT_FALSE(state.swapchainMaintenance1FeaturesChained);
    }
}

TEST(RenderVulkanBootstrapTests, OptionalDeviceFeatureAbsenceDoesNotRejectVulkanTwelveDevice)
{
    FakeVulkanState state;
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, "Vulkan 1.2 GPU");
    state.physicalDevices.push_back(std::move(device));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();

    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    EXPECT_TRUE(owner.IsValid());
    EXPECT_FALSE(owner.GetInfo().optionalCapabilities.swapchainMaintenance1);
    EXPECT_FALSE(owner.GetInfo().optionalCapabilities.presentId);
    EXPECT_FALSE(owner.GetInfo().optionalCapabilities.presentWait);
    EXPECT_TRUE(ContainsString(state.lastDeviceEnabledExtensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME));
    EXPECT_FALSE(ContainsString(state.lastDeviceEnabledExtensions,
                                VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME));
    EXPECT_FALSE(state.swapchainMaintenance1FeaturesChained);
    EXPECT_FALSE(state.presentIdFeaturesChained);
    EXPECT_FALSE(state.presentWaitFeaturesChained);
}

TEST(RenderVulkanBootstrapTests, DeviceCreationFailureDoesNotCreateAllocatorOrLeakDevice)
{
    FakeVulkanState state;
    state.createDeviceResult = VK_ERROR_FEATURE_NOT_PRESENT;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Creation Failure GPU"));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();

    const auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});

    ExpectRenderError(deviceResult, pond::render::RenderErrorCode::UnsupportedCapability);
    const std::string message{deviceResult.GetError().GetMessage()};
    EXPECT_NE(message.find("operation=vkCreateDevice"), std::string::npos);
    EXPECT_NE(message.find("nativeCode=" +
                           std::to_string(static_cast<std::int64_t>(VK_ERROR_FEATURE_NOT_PRESENT))),
              std::string::npos);
    EXPECT_NE(message.find("symbolicName=VK_ERROR_FEATURE_NOT_PRESENT"), std::string::npos);
    EXPECT_EQ(state.createDeviceCalls, 1U);
    EXPECT_EQ(state.loadDeviceCalls, 0U);
    EXPECT_EQ(state.createAllocatorCalls, 0U);
    EXPECT_EQ(state.destroyDeviceCalls, 0U);
}

TEST(RenderVulkanBootstrapTests, AllocatorFailureRollsBackLogicalDevice)
{
    FakeVulkanState state;
    state.createAllocatorResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Allocator Failure GPU"));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();

    const auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});

    ExpectRenderError(deviceResult, pond::render::RenderErrorCode::OutOfMemory);
    EXPECT_EQ(state.createDeviceCalls, 1U);
    EXPECT_EQ(state.loadDeviceCalls, 1U);
    EXPECT_EQ(state.getDeviceQueueCalls, 1U);
    EXPECT_EQ(state.createAllocatorCalls, 1U);
    EXPECT_EQ(state.destroyAllocatorCalls, 0U);
    EXPECT_EQ(state.destroyDeviceCalls, 1U);
}

TEST(RenderVulkanBootstrapTests, TargetSurfaceCompatibilityUsesAlreadySelectedPresentationQueue)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Target Surface GPU"));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();

    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    const auto compatibility = pond::render::detail::ValidateVulkanDeviceSurfaceCompatibility(
        dispatch, owner, MakeFakeHandle<VkSurfaceKHR>(0x6001U));

    ASSERT_TRUE(compatibility) << compatibility.GetError().GetMessage();
    EXPECT_EQ(compatibility.GetValue().graphicsQueueFamilyIndex, 0U);
    EXPECT_EQ(compatibility.GetValue().presentationQueueFamilyIndex, 0U);
    EXPECT_FALSE(compatibility.GetValue().usesDistinctPresentationQueue);
    EXPECT_TRUE(owner.IsValid());
}

TEST(RenderVulkanBootstrapTests, TargetSurfaceCompatibilityCanFallbackToPrecreatedQueue)
{
    FakeVulkanState state;
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Multi Queue Target GPU");
    device.queueFamilies = {
        VkQueueFamilyProperties{.queueFlags = VK_QUEUE_GRAPHICS_BIT, .queueCount = 1U},
        VkQueueFamilyProperties{.queueFlags = VK_QUEUE_TRANSFER_BIT, .queueCount = 1U},
        VkQueueFamilyProperties{.queueFlags = {}, .queueCount = 1U}};
    device.surfaceSupport = {VK_TRUE, VK_FALSE, VK_TRUE};
    state.physicalDevices.push_back(std::move(device));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR firstSurface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, firstSurface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();

    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, firstSurface, selectionResult.GetValue(),
        pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    state.physicalDevices[0].surfaceSupport = {VK_FALSE, VK_FALSE, VK_TRUE};
    const auto compatibility = pond::render::detail::ValidateVulkanDeviceSurfaceCompatibility(
        dispatch, owner, MakeFakeHandle<VkSurfaceKHR>(0x6001U));

    ASSERT_TRUE(compatibility) << compatibility.GetError().GetMessage();
    EXPECT_EQ(compatibility.GetValue().graphicsQueueFamilyIndex, 0U);
    EXPECT_EQ(compatibility.GetValue().presentationQueueFamilyIndex, 2U);
    EXPECT_TRUE(compatibility.GetValue().usesDistinctPresentationQueue);
    EXPECT_TRUE(owner.IsValid());
}

TEST(RenderVulkanBootstrapTests, TargetSurfaceCompatibilityRejectsIncompatibleSurfaceAtomically)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Incompatible Target GPU"));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR firstSurface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, firstSurface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();

    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, firstSurface, selectionResult.GetValue(),
        pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    state.physicalDevices[0].surfaceSupport = {VK_FALSE};
    const auto compatibility = pond::render::detail::ValidateVulkanDeviceSurfaceCompatibility(
        dispatch, owner, MakeFakeHandle<VkSurfaceKHR>(0x6001U));

    ExpectRenderError(compatibility, pond::render::RenderErrorCode::UnsupportedSurface);
    EXPECT_TRUE(owner.IsValid());
}
TEST(RenderVulkanBootstrapTests, SelectsSwapchainConfigWithFallbacksAndClampedExtent)
{
    FakeVulkanState state;
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Swapchain Selection GPU");
    device.surfaceCapabilities.minImageExtent = VkExtent2D{.width = 128U, .height = 128U};
    device.surfaceCapabilities.maxImageExtent = VkExtent2D{.width = 1024U, .height = 768U};
    device.surfaceCapabilities.supportedCompositeAlpha =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR | VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    device.surfaceCapabilities.supportedTransforms = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    device.surfaceCapabilities.currentTransform = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    device.surfaceFormats = {VkSurfaceFormatKHR{.format = VK_FORMAT_R8G8B8A8_UNORM,
                                                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
                             VkSurfaceFormatKHR{.format = VK_FORMAT_B8G8R8A8_SRGB,
                                                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    device.presentModes = {VK_PRESENT_MODE_FIFO_KHR};
    state.physicalDevices.push_back(std::move(device));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    const auto configResult = pond::render::detail::SelectVulkanSwapchainConfig(
        dispatch, owner, surface, owner.GetInfo().queuePlan,
        MakeTargetDesc(pond::render::PresentationPolicy::LowLatencyVSync,
                       pond::render::QueuedFrameLatency{3}, pond::platform::PixelSize{2000, 64}));

    ASSERT_TRUE(configResult) << configResult.GetError().GetMessage();
    const pond::render::detail::VulkanSwapchainConfig config = configResult.GetValue();
    EXPECT_EQ(config.format, VK_FORMAT_B8G8R8A8_SRGB);
    EXPECT_EQ(config.colorSpace, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    EXPECT_EQ(config.extent.width, 1024U);
    EXPECT_EQ(config.extent.height, 128U);
    EXPECT_EQ(config.presentMode, VK_PRESENT_MODE_FIFO_KHR);
    EXPECT_EQ(config.compositeAlpha, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
    EXPECT_EQ(config.preTransform, VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR);
    EXPECT_EQ(config.imageCount, 4U);
    EXPECT_EQ(config.sharingMode, VK_SHARING_MODE_EXCLUSIVE);
    EXPECT_EQ(config.presentation.actualPolicy, pond::render::PresentationPolicy::VSync);
    EXPECT_EQ(config.presentation.policyFallback,
              pond::render::PresentationPolicyFallbackReason::UnavailableForTarget);
    EXPECT_EQ(config.presentation.output, pond::render::PresentationOutput::OpaqueSdrSrgb);
    EXPECT_EQ(config.presentation.queuedLatencyFallback,
              pond::render::QueuedFrameLatencyFallbackReason::None);
    EXPECT_EQ(config.presentation.actualQueuedLatency.frameCount, 3U);

    ExpectRenderError(pond::render::detail::SelectVulkanSwapchainConfig(
                          dispatch, owner, surface, owner.GetInfo().queuePlan,
                          MakeTargetDesc(pond::render::PresentationPolicy::LowLatencyVSync, {},
                                         pond::platform::PixelSize{800, 600}, true, 1U,
                                         pond::render::RequirementStrength::Required)),
                      pond::render::RenderErrorCode::UnsupportedSurface);

    state.physicalDevices[0].presentModes = {VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR};
    const auto lowLatencyExact = pond::render::detail::SelectVulkanSwapchainConfig(
        dispatch, owner, surface, owner.GetInfo().queuePlan,
        MakeTargetDesc(pond::render::PresentationPolicy::LowLatencyVSync));
    ASSERT_TRUE(lowLatencyExact) << lowLatencyExact.GetError().GetMessage();
    EXPECT_EQ(lowLatencyExact.GetValue().presentation.actualPolicy,
              pond::render::PresentationPolicy::LowLatencyVSync);
    EXPECT_EQ(lowLatencyExact.GetValue().presentation.policyFallback,
              pond::render::PresentationPolicyFallbackReason::None);

    const auto uncappedFallback = pond::render::detail::SelectVulkanSwapchainConfig(
        dispatch, owner, surface, owner.GetInfo().queuePlan,
        MakeTargetDesc(pond::render::PresentationPolicy::Uncapped));
    ASSERT_TRUE(uncappedFallback) << uncappedFallback.GetError().GetMessage();
    EXPECT_EQ(uncappedFallback.GetValue().presentation.actualPolicy,
              pond::render::PresentationPolicy::LowLatencyVSync);
    EXPECT_EQ(uncappedFallback.GetValue().presentation.policyFallback,
              pond::render::PresentationPolicyFallbackReason::UnavailableForTarget);

    state.physicalDevices[0].presentModes = {VK_PRESENT_MODE_FIFO_KHR};
    const auto uncappedVSyncFallback = pond::render::detail::SelectVulkanSwapchainConfig(
        dispatch, owner, surface, owner.GetInfo().queuePlan,
        MakeTargetDesc(pond::render::PresentationPolicy::Uncapped));
    ASSERT_TRUE(uncappedVSyncFallback) << uncappedVSyncFallback.GetError().GetMessage();
    EXPECT_EQ(uncappedVSyncFallback.GetValue().presentation.actualPolicy,
              pond::render::PresentationPolicy::VSync);
    EXPECT_EQ(uncappedVSyncFallback.GetValue().presentation.policyFallback,
              pond::render::PresentationPolicyFallbackReason::UnavailableForTarget);
    ExpectRenderError(pond::render::detail::SelectVulkanSwapchainConfig(
                          dispatch, owner, surface, owner.GetInfo().queuePlan,
                          MakeTargetDesc(pond::render::PresentationPolicy::Uncapped, {},
                                         pond::platform::PixelSize{800, 600}, true, 1U,
                                         pond::render::RequirementStrength::Required)),
                      pond::render::RenderErrorCode::UnsupportedSurface);

    state.physicalDevices[0].presentModes = {VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR,
                                             VK_PRESENT_MODE_IMMEDIATE_KHR};
    const auto uncappedExact = pond::render::detail::SelectVulkanSwapchainConfig(
        dispatch, owner, surface, owner.GetInfo().queuePlan,
        MakeTargetDesc(pond::render::PresentationPolicy::Uncapped, {},
                       pond::platform::PixelSize{800, 600}, true, 1U,
                       pond::render::RequirementStrength::Required));
    ASSERT_TRUE(uncappedExact) << uncappedExact.GetError().GetMessage();
    EXPECT_EQ(uncappedExact.GetValue().presentation.actualPolicy,
              pond::render::PresentationPolicy::Uncapped);
    EXPECT_EQ(uncappedExact.GetValue().presentation.policyFallback,
              pond::render::PresentationPolicyFallbackReason::None);
}

TEST(RenderVulkanBootstrapTests, SwapchainSelectionRequiresOpaqueSdrSrgbOutput)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Strict Output Contract GPU"));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    const auto selectConfig = [&]()
    {
        return pond::render::detail::SelectVulkanSwapchainConfig(
            dispatch, owner, surface, owner.GetInfo().queuePlan, MakeTargetDesc());
    };

    state.physicalDevices[0].surfaceFormats = {
        VkSurfaceFormatKHR{.format = VK_FORMAT_R8G8B8A8_SRGB,
                           .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        VkSurfaceFormatKHR{.format = VK_FORMAT_B8G8R8A8_SRGB,
                           .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    auto configResult = selectConfig();
    ASSERT_TRUE(configResult) << configResult.GetError().GetMessage();
    EXPECT_EQ(configResult.GetValue().format, VK_FORMAT_B8G8R8A8_SRGB);
    EXPECT_EQ(configResult.GetValue().presentation.output,
              pond::render::PresentationOutput::OpaqueSdrSrgb);

    state.physicalDevices[0].surfaceFormats = {VkSurfaceFormatKHR{
        .format = VK_FORMAT_R8G8B8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    configResult = selectConfig();
    ASSERT_TRUE(configResult) << configResult.GetError().GetMessage();
    EXPECT_EQ(configResult.GetValue().format, VK_FORMAT_R8G8B8A8_SRGB);
    EXPECT_EQ(configResult.GetValue().colorSpace, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    EXPECT_EQ(configResult.GetValue().compositeAlpha, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);

    state.physicalDevices[0].surfaceFormats = {VkSurfaceFormatKHR{
        .format = VK_FORMAT_UNDEFINED, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    ExpectRenderError(selectConfig(), pond::render::RenderErrorCode::UnsupportedSurface);

    state.physicalDevices[0].surfaceFormats = {VkSurfaceFormatKHR{
        .format = VK_FORMAT_B8G8R8A8_UNORM, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    ExpectRenderError(selectConfig(), pond::render::RenderErrorCode::UnsupportedSurface);

    state.physicalDevices[0].surfaceFormats = {VkSurfaceFormatKHR{
        .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT}};
    ExpectRenderError(selectConfig(), pond::render::RenderErrorCode::UnsupportedSurface);

    state.physicalDevices[0].surfaceFormats = {VkSurfaceFormatKHR{
        .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    state.physicalDevices[0].surfaceCapabilities.supportedCompositeAlpha =
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR | VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR |
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    ExpectRenderError(selectConfig(), pond::render::RenderErrorCode::UnsupportedSurface);
}

TEST(RenderVulkanBootstrapTests, SelectedPresentationMetadataMustMatchNativeSwapchainConfig)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Presentation Metadata GPU"));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selection = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selection) << selection.GetError().GetMessage();
    auto device = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selection.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(device) << device.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(device).GetValue();

    auto selectedConfig = pond::render::detail::SelectVulkanSwapchainConfig(
        dispatch, owner, surface, owner.GetInfo().queuePlan, MakeTargetDesc());
    ASSERT_TRUE(selectedConfig) << selectedConfig.GetError().GetMessage();

    const auto expectRejectedBeforeNativeCreate =
        [&](pond::render::detail::VulkanSwapchainConfig config)
    {
        const std::uint32_t createCallsBefore = state.createSwapchainCalls;
        pond::render::detail::VulkanSwapchainCreationState creationState;
        ExpectRenderError(pond::render::detail::CreateVulkanSwapchainForSelectedConfig(
                              dispatch, owner, surface, config, VK_NULL_HANDLE, creationState),
                          pond::render::RenderErrorCode::InvalidArgument);
        EXPECT_FALSE(creationState.nativeCallAttempted);
        EXPECT_EQ(state.createSwapchainCalls, createCallsBefore);
    };

    pond::render::detail::VulkanSwapchainConfig mismatched = selectedConfig.GetValue();
    mismatched.presentation.actualPolicy = pond::render::PresentationPolicy::Uncapped;
    expectRejectedBeforeNativeCreate(mismatched);

    mismatched = selectedConfig.GetValue();
    mismatched.presentation.output = static_cast<pond::render::PresentationOutput>(255);
    expectRejectedBeforeNativeCreate(mismatched);

    mismatched = selectedConfig.GetValue();
    ++mismatched.presentation.pixelExtent.width;
    expectRejectedBeforeNativeCreate(mismatched);
}
TEST(RenderVulkanBootstrapTests, SwapchainImageCountDoesNotRaiseSemanticLatency)
{
    FakeVulkanState state;
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Latency Selection GPU");
    device.surfaceCapabilities.minImageCount = 5U;
    device.surfaceCapabilities.maxImageCount = 0U;
    state.physicalDevices.push_back(std::move(device));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    auto configResult = pond::render::detail::SelectVulkanSwapchainConfig(
        dispatch, owner, surface, owner.GetInfo().queuePlan,
        MakeTargetDesc(pond::render::PresentationPolicy::VSync,
                       pond::render::QueuedFrameLatency{2}));
    ASSERT_TRUE(configResult) << configResult.GetError().GetMessage();
    EXPECT_EQ(configResult.GetValue().imageCount, 5U);
    EXPECT_EQ(configResult.GetValue().presentation.actualQueuedLatency.frameCount, 2U);
    EXPECT_EQ(configResult.GetValue().presentation.queuedLatencyFallback,
              pond::render::QueuedFrameLatencyFallbackReason::None);

    state.physicalDevices[0].surfaceCapabilities.minImageCount = 1U;
    state.physicalDevices[0].surfaceCapabilities.maxImageCount = 2U;
    configResult = pond::render::detail::SelectVulkanSwapchainConfig(
        dispatch, owner, surface, owner.GetInfo().queuePlan,
        MakeTargetDesc(pond::render::PresentationPolicy::VSync,
                       pond::render::QueuedFrameLatency{3}));
    ASSERT_TRUE(configResult) << configResult.GetError().GetMessage();
    EXPECT_EQ(configResult.GetValue().imageCount, 2U);
    EXPECT_EQ(configResult.GetValue().presentation.actualQueuedLatency.frameCount, 2U);
    EXPECT_EQ(configResult.GetValue().presentation.queuedLatencyFallback,
              pond::render::QueuedFrameLatencyFallbackReason::TargetMaximumExceeded);

    ExpectRenderError(
        pond::render::detail::SelectVulkanSwapchainConfig(
            dispatch, owner, surface, owner.GetInfo().queuePlan,
            MakeTargetDesc(pond::render::PresentationPolicy::VSync,
                           pond::render::QueuedFrameLatency{3}, pond::platform::PixelSize{800, 600},
                           true, 1U, pond::render::RequirementStrength::Preferred,
                           pond::render::RequirementStrength::Required)),
        pond::render::RenderErrorCode::UnsupportedSurface);
}

TEST(RenderVulkanBootstrapTests, SwapchainConfigUsesConcurrentSharingForDistinctQueues)
{
    FakeVulkanState state;
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Distinct Queue Swapchain GPU");
    device.queueFamilies = {
        VkQueueFamilyProperties{.queueFlags = VK_QUEUE_GRAPHICS_BIT, .queueCount = 1U},
        VkQueueFamilyProperties{.queueFlags = VK_QUEUE_TRANSFER_BIT, .queueCount = 1U},
        VkQueueFamilyProperties{.queueFlags = {}, .queueCount = 1U}};
    device.surfaceSupport = {VK_FALSE, VK_FALSE, VK_TRUE};
    state.physicalDevices.push_back(std::move(device));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    const auto configResult = pond::render::detail::SelectVulkanSwapchainConfig(
        dispatch, owner, surface, owner.GetInfo().queuePlan, MakeTargetDesc());

    ASSERT_TRUE(configResult) << configResult.GetError().GetMessage();
    EXPECT_EQ(configResult.GetValue().sharingMode, VK_SHARING_MODE_CONCURRENT);
    const std::vector<std::uint32_t> expectedFamilies{0U, 2U};
    EXPECT_EQ(configResult.GetValue().sharingQueueFamilyIndices, expectedFamilies);
}

TEST(RenderVulkanBootstrapTests, SwapchainSelectionRejectsSuspendedTargetState)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Suspended Swapchain GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    ExpectRenderError(
        pond::render::detail::SelectVulkanSwapchainConfig(
            dispatch, owner, surface, owner.GetInfo().queuePlan,
            MakeTargetDesc(pond::render::PresentationPolicy::VSync,
                           pond::render::QueuedFrameLatency{}, pond::platform::PixelSize{})),
        pond::render::RenderErrorCode::InvalidState);
    ExpectRenderError(pond::render::detail::SelectVulkanSwapchainConfig(
                          dispatch, owner, surface, owner.GetInfo().queuePlan,
                          MakeTargetDesc(pond::render::PresentationPolicy::VSync,
                                         pond::render::QueuedFrameLatency{},
                                         pond::platform::PixelSize{800, 600}, false)),
                      pond::render::RenderErrorCode::InvalidState);
}

TEST(RenderVulkanBootstrapTests, CreatesSwapchainResourcesAndRollsBackPartialFailures)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Swapchain Creation GPU"));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    {
        auto swapchainResult = pond::render::detail::CreateVulkanSwapchainForTarget(
            dispatch, owner, surface, owner.GetInfo().queuePlan, MakeTargetDesc());
        ASSERT_TRUE(swapchainResult) << swapchainResult.GetError().GetMessage();
        pond::render::detail::VulkanSwapchainOwner swapchain =
            std::move(swapchainResult).GetValue();
        EXPECT_TRUE(swapchain.IsValid());
        EXPECT_EQ(state.lastSwapchainFormat, VK_FORMAT_B8G8R8A8_SRGB);
        EXPECT_EQ(state.lastSwapchainColorSpace, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
        EXPECT_EQ(state.lastSwapchainCompositeAlpha, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
        EXPECT_EQ(swapchain.GetFramebufferCount(), state.swapchainImageCount);
        ASSERT_TRUE(pond::render::IsValid(swapchain.GetConfig().presentation));
    }
    EXPECT_EQ(state.destroyFramebufferCalls, state.swapchainImageCount);
    EXPECT_EQ(state.destroyRenderPassCalls, 1U);
    EXPECT_EQ(state.destroyImageViewCalls, state.swapchainImageCount);
    EXPECT_EQ(state.destroySwapchainCalls, 1U);

    state.createFramebufferResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const auto failingSwapchain = pond::render::detail::CreateVulkanSwapchainForTarget(
        dispatch, owner, surface, owner.GetInfo().queuePlan, MakeTargetDesc());

    ExpectRenderError(failingSwapchain, pond::render::RenderErrorCode::OutOfMemory);
    EXPECT_EQ(state.destroySwapchainCalls, 2U);
    EXPECT_EQ(state.destroyRenderPassCalls, 2U);
    EXPECT_EQ(state.destroyImageViewCalls, state.swapchainImageCount * 2U);
}

TEST(RenderVulkanBootstrapTests, RetriesSwapchainImageEnumerationAfterIncomplete)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Growing Swapchain GPU"));
    state.swapchainImageCount = 2U;
    state.swapchainImageCountAfterFirstQuery = 3U;

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    auto swapchainResult = pond::render::detail::CreateVulkanSwapchainForTarget(
        dispatch, owner, surface, owner.GetInfo().queuePlan, MakeTargetDesc());

    ASSERT_TRUE(swapchainResult) << swapchainResult.GetError().GetMessage();
    EXPECT_EQ(swapchainResult.GetValue().GetFramebufferCount(), 3U);
    EXPECT_EQ(state.getSwapchainImagesCalls, 4U);
    EXPECT_EQ(state.createImageViewCalls, 3U);
    EXPECT_EQ(state.createFramebufferCalls, 3U);
}
TEST(RenderVulkanBootstrapTests, TracksOldSwapchainRetirementAtNativeCreationBoundary)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Replacement Swapchain GPU"));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    auto configResult = pond::render::detail::SelectVulkanSwapchainConfig(
        dispatch, owner, surface, owner.GetInfo().queuePlan, MakeTargetDesc());
    ASSERT_TRUE(configResult) << configResult.GetError().GetMessage();

    const VkSwapchainKHR oldSwapchain = MakeFakeHandle<VkSwapchainKHR>(0xA777U);
    pond::render::detail::VulkanSwapchainCreationState creationState;
    auto swapchainResult = pond::render::detail::CreateVulkanSwapchainForSelectedConfig(
        dispatch, owner, surface, configResult.GetValue(), oldSwapchain, creationState);

    ASSERT_TRUE(swapchainResult) << swapchainResult.GetError().GetMessage();
    EXPECT_TRUE(creationState.nativeCallAttempted);
    EXPECT_TRUE(creationState.oldSwapchainRetired);
    EXPECT_EQ(state.lastOldSwapchain, oldSwapchain);
}

TEST(RenderVulkanBootstrapTests, PublicFrameExposesOneIntermediateStageAfterClear)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Intermediate Stage GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);
    state.frameTrace.clear();

    auto frameResult = owners->target.AcquireFrame();
    ASSERT_TRUE(frameResult) << frameResult.GetError().GetMessage();
    pond::render::RenderFrame frame = std::move(frameResult).GetValue();
    EXPECT_EQ(state.frameTrace, std::vector<std::string>{"acquire"});

    ExpectRenderError(pond::render::detail::RenderBackendTestAccess::RecordIntermediateStage(frame),
                      pond::render::RenderErrorCode::InvalidState);
    ASSERT_TRUE(frame.Clear());
    EXPECT_EQ(state.frameTrace, (std::vector<std::string>{"acquire", "clear"}));

    ASSERT_TRUE(pond::render::detail::RenderBackendTestAccess::RecordIntermediateStage(frame));
    ExpectRenderError(pond::render::detail::RenderBackendTestAccess::RecordIntermediateStage(frame),
                      pond::render::RenderErrorCode::InvalidState);
    EXPECT_EQ(state.frameTrace, (std::vector<std::string>{"acquire", "clear", "intermediate"}));

    const auto finish = frame.FinishAndPresent();
    ASSERT_TRUE(finish) << finish.GetError().GetMessage();
    EXPECT_EQ(state.frameTrace,
              (std::vector<std::string>{"acquire", "clear", "intermediate", "submit", "present"}));
}

TEST(RenderVulkanBootstrapTests,
     PublicAcquireOutOfDateStaysActiveThenCompletedRebuildReportsRecreated)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Out Of Date GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);

    state.acquireNextImageResult = VK_ERROR_OUT_OF_DATE_KHR;
    auto pendingFrameResult = owners->target.AcquireFrame();

    ASSERT_TRUE(pendingFrameResult) << pendingFrameResult.GetError().GetMessage();
    pond::render::RenderFrame pendingFrame = std::move(pendingFrameResult).GetValue();
    EXPECT_EQ(pendingFrame.GetStatus(), pond::render::FrameStatus::RecreationPending);
    EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::Active);
    EXPECT_FALSE(owners->target.IsSurfaceLost());
    EXPECT_TRUE(owners->target.HasPendingRecreation());
    ASSERT_TRUE(owners->target.GetPendingRecreationInfo().has_value());
    EXPECT_EQ(owners->target.GetPendingRecreationInfo()->reason,
              pond::render::TargetRecreationReason::PresentationChanged);
    EXPECT_FALSE(owners->target.GetPendingRecreationInfo()->previousRevision.has_value());
    EXPECT_FALSE(owners->target.GetPendingRecreationInfo()->currentRevision.has_value());

    const auto pendingFinish = pendingFrame.FinishAndPresent();
    ASSERT_TRUE(pendingFinish) << pendingFinish.GetError().GetMessage();
    EXPECT_EQ(pendingFinish.GetValue().status, pond::render::FrameStatus::RecreationPending);
    EXPECT_FALSE(pendingFinish.GetValue().presented);

    state.acquireNextImageResult = VK_SUBOPTIMAL_KHR;
    auto recreatedFrameResult = owners->target.AcquireFrame();

    ASSERT_TRUE(recreatedFrameResult) << recreatedFrameResult.GetError().GetMessage();
    EXPECT_EQ(owners->target.GetSwapchainGeneration(), 2U);
    pond::render::RenderFrame recreatedFrame = std::move(recreatedFrameResult).GetValue();
    EXPECT_EQ(recreatedFrame.GetStatus(), pond::render::FrameStatus::Recreated);
    ASSERT_TRUE(recreatedFrame.Clear());
    const auto recreatedFinish = recreatedFrame.FinishAndPresent();
    ASSERT_TRUE(recreatedFinish) << recreatedFinish.GetError().GetMessage();
    EXPECT_EQ(recreatedFinish.GetValue().status, pond::render::FrameStatus::Recreated);
    EXPECT_TRUE(recreatedFinish.GetValue().presented);
    EXPECT_TRUE(recreatedFinish.GetValue().suboptimal);
    EXPECT_TRUE(owners->target.HasPendingRecreation());
}

TEST(RenderVulkanBootstrapTests, PublicAcquireSurfaceLossBecomesStickyUntilRecovery)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Surface Loss GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);

    state.acquireNextImageResult = VK_ERROR_SURFACE_LOST_KHR;
    const auto lostFrame = owners->target.AcquireFrame();

    ExpectRenderError(lostFrame, pond::render::RenderErrorCode::SurfaceLost);
    EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::SurfaceLost);
    EXPECT_TRUE(owners->target.IsSurfaceLost());
    EXPECT_FALSE(owners->target.HasSwapchain());
    EXPECT_FALSE(owners->target.HasPendingRecreation());
    EXPECT_FALSE(owners->target.GetPendingRecreationInfo().has_value());

    const std::uint32_t acquireCallsAfterLoss = state.acquireNextImageCalls;
    ExpectRenderError(owners->target.AcquireFrame(), pond::render::RenderErrorCode::SurfaceLost);
    EXPECT_EQ(state.acquireNextImageCalls, acquireCallsAfterLoss);
    EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::SurfaceLost);

    state.acquireNextImageResult = VK_SUCCESS;
    auto surfaceResult = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
        dispatch, owners->instance,
        MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::Win32));
    ASSERT_TRUE(surfaceResult) << surfaceResult.GetError().GetMessage();
    auto preparedResult = pond::render::detail::RenderBackendTestAccess::CreateRecoverySurface(
        owners->target, std::move(surfaceResult).GetValue(), owners->target.GetTargetSnapshot());
    ASSERT_TRUE(preparedResult) << preparedResult.GetError().GetMessage();

    ASSERT_TRUE(owners->target.RecoverSurface(std::move(preparedResult).GetValue()));
    EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::Active);
    EXPECT_FALSE(owners->target.IsSurfaceLost());
    EXPECT_TRUE(owners->target.HasPendingRecreation());
    ASSERT_TRUE(owners->target.GetPendingRecreationInfo().has_value());
    EXPECT_EQ(owners->target.GetPendingRecreationInfo()->reason,
              pond::render::TargetRecreationReason::SurfaceLost);
    EXPECT_FALSE(owners->target.GetPendingRecreationInfo()->previousRevision.has_value());
    EXPECT_FALSE(owners->target.GetPendingRecreationInfo()->currentRevision.has_value());

    auto recoveredFrameResult = owners->target.AcquireFrame();
    ASSERT_TRUE(recoveredFrameResult) << recoveredFrameResult.GetError().GetMessage();
    pond::render::RenderFrame recoveredFrame = std::move(recoveredFrameResult).GetValue();
    EXPECT_EQ(recoveredFrame.GetStatus(), pond::render::FrameStatus::Recreated);
    ASSERT_TRUE(recoveredFrame.Clear());
    const auto recoveredFinish = recoveredFrame.FinishAndPresent();
    ASSERT_TRUE(recoveredFinish) << recoveredFinish.GetError().GetMessage();
    EXPECT_EQ(recoveredFinish.GetValue().status, pond::render::FrameStatus::Recreated);
    EXPECT_TRUE(recoveredFinish.GetValue().presented);
}

TEST(RenderVulkanBootstrapTests, IncompatibleRecoverySurfaceIsNotConsumed)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Recovery Output Contract GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);

    state.acquireNextImageResult = VK_ERROR_SURFACE_LOST_KHR;
    ExpectRenderError(owners->target.AcquireFrame(), pond::render::RenderErrorCode::SurfaceLost);
    ASSERT_TRUE(owners->target.IsSurfaceLost());
    ASSERT_FALSE(owners->target.HasSwapchain());

    state.acquireNextImageResult = VK_SUCCESS;
    auto surfaceResult = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
        dispatch, owners->instance,
        MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::Win32));
    ASSERT_TRUE(surfaceResult) << surfaceResult.GetError().GetMessage();
    auto preparedResult = pond::render::detail::RenderBackendTestAccess::CreateRecoverySurface(
        owners->target, std::move(surfaceResult).GetValue(), owners->target.GetTargetSnapshot());
    ASSERT_TRUE(preparedResult) << preparedResult.GetError().GetMessage();
    pond::render::PreparedSurface replacement = std::move(preparedResult).GetValue();

    state.physicalDevices[0].surfaceFormats = {VkSurfaceFormatKHR{
        .format = VK_FORMAT_B8G8R8A8_UNORM, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    const std::uint32_t createSwapchainCallsBefore = state.createSwapchainCalls;
    const std::uint32_t destroySurfaceCallsBefore = state.destroySurfaceCalls;

    ExpectRenderError(owners->target.RecoverSurface(std::move(replacement)),
                      pond::render::RenderErrorCode::UnsupportedSurface);
    EXPECT_TRUE(replacement.IsValid());
    EXPECT_TRUE(owners->target.IsSurfaceLost());
    EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::SurfaceLost);
    EXPECT_FALSE(owners->target.HasPendingRecreation());
    EXPECT_EQ(state.createSwapchainCalls, createSwapchainCallsBefore);
    EXPECT_EQ(state.destroySurfaceCalls, destroySurfaceCallsBefore);

    state.physicalDevices[0].surfaceFormats = {VkSurfaceFormatKHR{
        .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    ASSERT_TRUE(owners->target.RecoverSurface(std::move(replacement)));
    EXPECT_FALSE(replacement.IsValid());
    EXPECT_FALSE(owners->target.IsSurfaceLost());
    EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::Active);
    EXPECT_TRUE(owners->target.HasPendingRecreation());
    EXPECT_EQ(state.createSwapchainCalls, createSwapchainCallsBefore);
}
TEST(RenderVulkanBootstrapTests, PublicDeviceLossIsStickyAndDoesNotMasqueradeAsSurfaceLoss)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Device Loss GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);

    state.acquireNextImageResult = VK_ERROR_DEVICE_LOST;
    ExpectRenderError(owners->target.AcquireFrame(), pond::render::RenderErrorCode::DeviceLost);
    EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::DeviceLost);
    EXPECT_TRUE(owners->target.IsDeviceLost());
    EXPECT_FALSE(owners->target.IsSurfaceLost());
    EXPECT_FALSE(owners->target.HasPendingRecreation());

    const std::uint32_t acquireCallsAfterLoss = state.acquireNextImageCalls;
    ExpectRenderError(owners->target.AcquireFrame(), pond::render::RenderErrorCode::DeviceLost);
    EXPECT_EQ(state.acquireNextImageCalls, acquireCallsAfterLoss);
    EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::DeviceLost);
}

TEST(RenderVulkanBootstrapTests, PublicPresentOutOfDateSchedulesACompletedRebuild)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Present Out Of Date GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);

    auto frameResult = owners->target.AcquireFrame();
    ASSERT_TRUE(frameResult) << frameResult.GetError().GetMessage();
    pond::render::RenderFrame frame = std::move(frameResult).GetValue();
    ASSERT_TRUE(frame.Clear());

    state.queuePresentResult = VK_ERROR_OUT_OF_DATE_KHR;
    const auto pendingFinish = frame.FinishAndPresent();
    ASSERT_TRUE(pendingFinish) << pendingFinish.GetError().GetMessage();
    EXPECT_EQ(pendingFinish.GetValue().status, pond::render::FrameStatus::RecreationPending);
    EXPECT_FALSE(pendingFinish.GetValue().presented);
    EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::Active);
    EXPECT_FALSE(owners->target.IsSurfaceLost());
    ASSERT_TRUE(owners->target.GetPendingRecreationInfo().has_value());
    EXPECT_EQ(owners->target.GetPendingRecreationInfo()->reason,
              pond::render::TargetRecreationReason::PresentationChanged);
    EXPECT_FALSE(owners->target.GetPendingRecreationInfo()->previousRevision.has_value());
    EXPECT_FALSE(owners->target.GetPendingRecreationInfo()->currentRevision.has_value());

    state.queuePresentResult = VK_SUCCESS;
    auto recreatedFrameResult = owners->target.AcquireFrame();
    ASSERT_TRUE(recreatedFrameResult) << recreatedFrameResult.GetError().GetMessage();
    pond::render::RenderFrame recreatedFrame = std::move(recreatedFrameResult).GetValue();
    EXPECT_EQ(recreatedFrame.GetStatus(), pond::render::FrameStatus::Recreated);
    ASSERT_TRUE(recreatedFrame.Clear());
    const auto recreatedFinish = recreatedFrame.FinishAndPresent();
    ASSERT_TRUE(recreatedFinish) << recreatedFinish.GetError().GetMessage();
    EXPECT_EQ(recreatedFinish.GetValue().status, pond::render::FrameStatus::Recreated);
    EXPECT_TRUE(recreatedFinish.GetValue().presented);
}

TEST(RenderVulkanBootstrapTests, PublicTargetRebuildsPoisonedGenerationOnNextAcquire)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Poison Recovery GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);

    auto frameResult = owners->target.AcquireFrame();
    ASSERT_TRUE(frameResult) << frameResult.GetError().GetMessage();
    pond::render::RenderFrame frame = std::move(frameResult).GetValue();
    ASSERT_TRUE(frame.Clear());

    state.endCommandBufferResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    const auto failedPresent = frame.FinishAndPresent();

    ExpectRenderError(failedPresent, pond::render::RenderErrorCode::OutOfMemory);
    EXPECT_TRUE(owners->target.IsValid());
    EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::Active);
    EXPECT_FALSE(owners->target.HasSwapchain());
    EXPECT_EQ(owners->target.GetSwapchainGeneration(), 0U);
    EXPECT_TRUE(owners->target.HasPendingRecreation());
    EXPECT_FALSE(owners->target.GetSelectedPresentationConfig().has_value());

    state.endCommandBufferResult = VK_SUCCESS;
    auto recoveredFrameResult = owners->target.AcquireFrame();

    ASSERT_TRUE(recoveredFrameResult) << recoveredFrameResult.GetError().GetMessage();
    EXPECT_EQ(state.lastOldSwapchain, owners->initialSwapchain);
    EXPECT_TRUE(owners->target.HasSwapchain());
    EXPECT_EQ(owners->target.GetSwapchainGeneration(), 2U);
    EXPECT_FALSE(owners->target.HasPendingRecreation());
    EXPECT_EQ(owners->target.GetDiagnostics().recreationCount, 1U);

    pond::render::RenderFrame recoveredFrame = std::move(recoveredFrameResult).GetValue();
    ASSERT_TRUE(recoveredFrame.Clear());
    const auto recoveredPresent = recoveredFrame.FinishAndPresent();
    ASSERT_TRUE(recoveredPresent) << recoveredPresent.GetError().GetMessage();
    EXPECT_TRUE(recoveredPresent.GetValue().presented);
}

TEST(RenderVulkanBootstrapTests, PublicTargetPoisonedGenerationWaitsForProofDuringShutdown)
{
    struct FailureCase final
    {
        std::string_view name;
        VkResult FakeVulkanState::* resultField;
        bool submissionQueued{};
    };

    constexpr std::array failureCases{
        FailureCase{"pre-submit failure", &FakeVulkanState::endCommandBufferResult},
        FailureCase{"non-enqueued present failure", &FakeVulkanState::queuePresentResult, true},
    };

    for (const FailureCase& failureCase : failureCases)
    {
        SCOPED_TRACE(std::string{failureCase.name});

        FakeVulkanState state;
        state.physicalDevices.push_back(MakeCompatibleFakeDevice(
            0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Poison Shutdown GPU"));
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
        std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
        ASSERT_NE(owners, nullptr);

        auto frameResult = owners->target.AcquireFrame();
        ASSERT_TRUE(frameResult) << frameResult.GetError().GetMessage();
        pond::render::RenderFrame frame = std::move(frameResult).GetValue();
        ASSERT_TRUE(frame.Clear());

        state.*(failureCase.resultField) = VK_ERROR_OUT_OF_HOST_MEMORY;
        ExpectRenderError(frame.FinishAndPresent(), pond::render::RenderErrorCode::OutOfMemory);
        ASSERT_FALSE(owners->target.HasSwapchain());
        ASSERT_FALSE(state.acquiredFences.empty());
        std::vector<VkFence> expectedCompletionFences{state.acquiredFences.back()};
        if (failureCase.submissionQueued)
        {
            ASSERT_FALSE(state.submittedFences.empty());
            expectedCompletionFences.push_back(state.submittedFences.back());
        }

        state.lastWaitForFences.clear();
        const std::uint32_t waitsBeforeShutdown = state.waitForFencesCalls;
        owners.reset();

        EXPECT_GT(state.waitForFencesCalls, waitsBeforeShutdown);
        EXPECT_EQ(state.lastWaitForFences, expectedCompletionFences);
        EXPECT_EQ(state.destroyDeviceCalls, 1U);
    }
}

TEST(RenderVulkanBootstrapTests, SameSizePresentationEnvironmentChangesCoalesceIntoOneRecreation)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Presentation Environment GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);

    const pond::render::RenderTargetSnapshot initialSnapshot = owners->target.GetTargetSnapshot();
    const std::uint32_t createSwapchainCallsBefore = state.createSwapchainCalls;
    ASSERT_EQ(initialSnapshot.GetPresentationEnvironmentRevision(),
              pond::render::PresentationEnvironmentRevision{1U});

    ASSERT_TRUE(owners->target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
        initialSnapshot.GetWindowId(), initialSnapshot.GetPixelSize(), initialSnapshot.IsVisible(),
        initialSnapshot.GetWindowState(), pond::render::PresentationEnvironmentRevision{2U}, 2U}));
    ASSERT_TRUE(owners->target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
        initialSnapshot.GetWindowId(), initialSnapshot.GetPixelSize(), initialSnapshot.IsVisible(),
        initialSnapshot.GetWindowState(), pond::render::PresentationEnvironmentRevision{3U}, 3U}));

    EXPECT_EQ(state.createSwapchainCalls, createSwapchainCallsBefore);
    ASSERT_TRUE(owners->target.GetPendingRecreationInfo().has_value());
    const pond::render::TargetRecreationInfo pending = *owners->target.GetPendingRecreationInfo();
    EXPECT_TRUE(pond::render::IsValid(pending));
    EXPECT_EQ(pending.reason, pond::render::TargetRecreationReason::PresentationChanged);
    EXPECT_EQ(pending.previousRevision, 1U);
    EXPECT_EQ(pending.currentRevision, 3U);

    auto frameResult = owners->target.AcquireFrame();
    ASSERT_TRUE(frameResult) << frameResult.GetError().GetMessage();
    EXPECT_EQ(state.createSwapchainCalls, createSwapchainCallsBefore + 1U);
    EXPECT_EQ(owners->target.GetSwapchainGeneration(), 2U);
    EXPECT_FALSE(owners->target.HasPendingRecreation());
    ASSERT_TRUE(owners->target.GetSelectedPresentationConfig().has_value());
    EXPECT_EQ(owners->target.GetSelectedPresentationConfig()->pixelExtent,
              initialSnapshot.GetPixelSize());

    pond::render::RenderFrame frame = std::move(frameResult).GetValue();
    EXPECT_EQ(frame.GetStatus(), pond::render::FrameStatus::Recreated);
    ASSERT_TRUE(frame.Clear());
    const auto present = frame.FinishAndPresent();
    ASSERT_TRUE(present) << present.GetError().GetMessage();
    EXPECT_TRUE(present.GetValue().presented);
}

TEST(RenderVulkanBootstrapTests, PublicTargetPreflightFailurePreservesCommittedSwapchain)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Preflight Transaction GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);

    const pond::platform::PixelSize initialExtent{800, 600};
    const pond::platform::PixelSize resizedExtent{801, 601};
    ASSERT_TRUE(owners->target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
        pond::platform::WindowId{42}, resizedExtent, true, pond::platform::WindowState::Normal,
        pond::render::PresentationEnvironmentRevision{1U}, 2U}));
    ASSERT_TRUE(owners->target.HasPendingRecreation());

    state.getSurfaceCapabilitiesResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    const std::uint32_t createSwapchainCallsBefore = state.createSwapchainCalls;
    const auto failedFrame = owners->target.AcquireFrame();

    ExpectRenderError(failedFrame, pond::render::RenderErrorCode::OutOfMemory);
    EXPECT_EQ(state.createSwapchainCalls, createSwapchainCallsBefore);
    EXPECT_TRUE(owners->target.HasSwapchain());
    EXPECT_EQ(owners->target.GetSwapchainGeneration(), 1U);
    EXPECT_TRUE(owners->target.HasPendingRecreation());
    const pond::render::RenderTargetDiagnostics failedDiagnostics = owners->target.GetDiagnostics();
    EXPECT_EQ(failedDiagnostics.recreationCount, 0U);
    ASSERT_TRUE(failedDiagnostics.lastFailure.has_value());
    EXPECT_EQ(failedDiagnostics.lastFailure->renderCode,
              pond::render::RenderErrorCode::OutOfMemory);
    EXPECT_EQ(failedDiagnostics.lastFailure->nativeCode,
              static_cast<std::int64_t>(VK_ERROR_OUT_OF_HOST_MEMORY));
    EXPECT_EQ(failedDiagnostics.lastFailure->symbolicName, "VK_ERROR_OUT_OF_HOST_MEMORY");
    EXPECT_EQ(failedDiagnostics.lastFailure->operation,
              "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    EXPECT_EQ(failedDiagnostics.lastFailure->windowId, pond::platform::WindowId{42});
    EXPECT_EQ(failedDiagnostics.lastFailure->targetLabel, "target/window:42");
    const std::optional<pond::render::SelectedPresentationConfig> committedConfig =
        owners->target.GetSelectedPresentationConfig();
    ASSERT_TRUE(committedConfig.has_value());
    EXPECT_EQ(committedConfig->pixelExtent, initialExtent);

    state.getSurfaceCapabilitiesResult = VK_SUCCESS;
    auto replacementFrameResult = owners->target.AcquireFrame();

    ASSERT_TRUE(replacementFrameResult) << replacementFrameResult.GetError().GetMessage();
    EXPECT_EQ(state.lastOldSwapchain, owners->initialSwapchain);
    EXPECT_TRUE(owners->target.HasSwapchain());
    EXPECT_EQ(owners->target.GetSwapchainGeneration(), 2U);
    EXPECT_FALSE(owners->target.HasPendingRecreation());
    EXPECT_EQ(owners->target.GetDiagnostics().recreationCount, 1U);
    const std::optional<pond::render::SelectedPresentationConfig> replacementConfig =
        owners->target.GetSelectedPresentationConfig();
    ASSERT_TRUE(replacementConfig.has_value());
    EXPECT_EQ(replacementConfig->pixelExtent, resizedExtent);

    pond::render::RenderFrame replacementFrame = std::move(replacementFrameResult).GetValue();
    ASSERT_TRUE(replacementFrame.Clear());
    const auto replacementPresent = replacementFrame.FinishAndPresent();
    ASSERT_TRUE(replacementPresent) << replacementPresent.GetError().GetMessage();
    EXPECT_TRUE(replacementPresent.GetValue().presented);
}

TEST(RenderVulkanBootstrapTests, PublicTargetOutputContractFailurePreservesCommittedSwapchain)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Output Transaction GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);

    const std::optional<pond::render::SelectedPresentationConfig> initialConfig =
        owners->target.GetSelectedPresentationConfig();
    ASSERT_TRUE(initialConfig.has_value());
    ASSERT_TRUE(owners->target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
        pond::platform::WindowId{42}, pond::platform::PixelSize{801, 601}, true,
        pond::platform::WindowState::Normal, pond::render::PresentationEnvironmentRevision{1U},
        2U}));

    state.physicalDevices[0].surfaceFormats = {VkSurfaceFormatKHR{
        .format = VK_FORMAT_B8G8R8A8_UNORM, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    const std::uint32_t createSwapchainCallsBefore = state.createSwapchainCalls;

    ExpectRenderError(owners->target.AcquireFrame(),
                      pond::render::RenderErrorCode::UnsupportedSurface);
    EXPECT_EQ(state.createSwapchainCalls, createSwapchainCallsBefore);
    EXPECT_TRUE(owners->target.HasSwapchain());
    EXPECT_EQ(owners->target.GetSwapchainGeneration(), 1U);
    EXPECT_TRUE(owners->target.HasPendingRecreation());
    EXPECT_EQ(owners->target.GetDiagnostics().recreationCount, 0U);
    EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::Active);
    EXPECT_FALSE(owners->target.IsSurfaceLost());
    EXPECT_FALSE(owners->target.IsDeviceLost());
    EXPECT_EQ(owners->target.GetSelectedPresentationConfig(), initialConfig);

    state.physicalDevices[0].surfaceFormats = {VkSurfaceFormatKHR{
        .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    auto replacementFrameResult = owners->target.AcquireFrame();

    ASSERT_TRUE(replacementFrameResult) << replacementFrameResult.GetError().GetMessage();
    EXPECT_EQ(owners->target.GetSwapchainGeneration(), 2U);
    EXPECT_FALSE(owners->target.HasPendingRecreation());
    EXPECT_EQ(owners->target.GetDiagnostics().recreationCount, 1U);

    pond::render::RenderFrame replacementFrame = std::move(replacementFrameResult).GetValue();
    ASSERT_TRUE(replacementFrame.Clear());
    const auto replacementPresent = replacementFrame.FinishAndPresent();
    ASSERT_TRUE(replacementPresent) << replacementPresent.GetError().GetMessage();
    EXPECT_TRUE(replacementPresent.GetValue().presented);
}

TEST(RenderVulkanBootstrapTests, PublicTargetOpaqueAlphaFailurePreservesCommittedSwapchain)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Opaque Alpha Transaction GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);

    const std::optional<pond::render::SelectedPresentationConfig> initialConfig =
        owners->target.GetSelectedPresentationConfig();
    ASSERT_TRUE(initialConfig.has_value());
    ASSERT_TRUE(owners->target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
        pond::platform::WindowId{42}, pond::platform::PixelSize{801, 601}, true,
        pond::platform::WindowState::Normal, pond::render::PresentationEnvironmentRevision{1U},
        2U}));

    state.physicalDevices[0].surfaceCapabilities.supportedCompositeAlpha =
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR | VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR |
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    const std::uint32_t createCallsBefore = state.createSwapchainCalls;

    ExpectRenderError(owners->target.AcquireFrame(),
                      pond::render::RenderErrorCode::UnsupportedSurface);
    EXPECT_EQ(state.createSwapchainCalls, createCallsBefore);
    EXPECT_TRUE(owners->target.HasSwapchain());
    EXPECT_EQ(owners->target.GetSwapchainGeneration(), 1U);
    EXPECT_TRUE(owners->target.HasPendingRecreation());
    EXPECT_EQ(owners->target.GetSelectedPresentationConfig(), initialConfig);

    state.physicalDevices[0].surfaceCapabilities.supportedCompositeAlpha =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    auto replacementFrame = owners->target.AcquireFrame();
    ASSERT_TRUE(replacementFrame) << replacementFrame.GetError().GetMessage();
    EXPECT_EQ(owners->target.GetSwapchainGeneration(), 2U);
    EXPECT_FALSE(owners->target.HasPendingRecreation());

    pond::render::RenderFrame frame = std::move(replacementFrame).GetValue();
    ASSERT_TRUE(frame.Clear());
    const auto present = frame.FinishAndPresent();
    ASSERT_TRUE(present) << present.GetError().GetMessage();
    EXPECT_EQ(present.GetValue().status, pond::render::FrameStatus::Recreated);
}
TEST(RenderVulkanBootstrapTests, PublicSurfaceQueryLossTransitionsToSurfaceRecovery)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Surface Query Loss GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);

    ASSERT_TRUE(owners->target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
        pond::platform::WindowId{42}, pond::platform::PixelSize{801, 601}, true,
        pond::platform::WindowState::Normal, pond::render::PresentationEnvironmentRevision{1U},
        2U}));
    state.getSurfaceCapabilitiesResult = VK_ERROR_SURFACE_LOST_KHR;

    ExpectRenderError(owners->target.AcquireFrame(), pond::render::RenderErrorCode::SurfaceLost);
    EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::SurfaceLost);
    EXPECT_TRUE(owners->target.IsSurfaceLost());
    EXPECT_FALSE(owners->target.HasSwapchain());
    EXPECT_FALSE(owners->target.HasPendingRecreation());
}
TEST(RenderVulkanBootstrapTests, PublicTargetReplacementFailureRetiresOldSwapchainBeforeRetry)
{
    struct FailureCase final
    {
        std::string_view name;
        VkResult FakeVulkanState::* resultField;
    };

    constexpr std::array failureCases{
        FailureCase{"create swapchain", &FakeVulkanState::createSwapchainResult},
        FailureCase{"get swapchain images", &FakeVulkanState::getSwapchainImagesResult},
        FailureCase{"create image view", &FakeVulkanState::createImageViewResult},
        FailureCase{"create render pass", &FakeVulkanState::createRenderPassResult},
        FailureCase{"create framebuffer", &FakeVulkanState::createFramebufferResult},
        FailureCase{"create command pool", &FakeVulkanState::createCommandPoolResult},
        FailureCase{"allocate command buffers", &FakeVulkanState::allocateCommandBuffersResult},
        FailureCase{"create semaphore", &FakeVulkanState::createSemaphoreResult},
        FailureCase{"create fence", &FakeVulkanState::createFenceResult},
    };

    for (const FailureCase& failureCase : failureCases)
    {
        SCOPED_TRACE(std::string{failureCase.name});

        FakeVulkanState state;
        state.physicalDevices.push_back(MakeCompatibleFakeDevice(
            0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Replacement Transaction GPU"));
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
        std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
        ASSERT_NE(owners, nullptr);

        ASSERT_TRUE(owners->target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
            pond::platform::WindowId{42}, pond::platform::PixelSize{801, 601}, true,
            pond::platform::WindowState::Normal, pond::render::PresentationEnvironmentRevision{1U},
            2U}));
        state.*(failureCase.resultField) = VK_ERROR_OUT_OF_HOST_MEMORY;
        const auto failedFrame = owners->target.AcquireFrame();

        ExpectRenderError(failedFrame, pond::render::RenderErrorCode::OutOfMemory);
        EXPECT_EQ(state.lastOldSwapchain, owners->initialSwapchain);
        EXPECT_TRUE(owners->target.IsValid());
        EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::Active);
        EXPECT_FALSE(owners->target.HasSwapchain());
        EXPECT_EQ(owners->target.GetSwapchainGeneration(), 0U);
        EXPECT_TRUE(owners->target.HasPendingRecreation());
        EXPECT_FALSE(owners->target.GetSelectedPresentationConfig().has_value());
        EXPECT_EQ(owners->target.GetDiagnostics().recreationCount, 0U);

        state.*(failureCase.resultField) = VK_SUCCESS;
        auto replacementFrameResult = owners->target.AcquireFrame();

        ASSERT_TRUE(replacementFrameResult) << replacementFrameResult.GetError().GetMessage();
        EXPECT_EQ(state.lastOldSwapchain, VK_NULL_HANDLE);
        EXPECT_TRUE(owners->target.HasSwapchain());
        EXPECT_EQ(owners->target.GetSwapchainGeneration(), 2U);
        EXPECT_FALSE(owners->target.HasPendingRecreation());
        EXPECT_EQ(owners->target.GetDiagnostics().recreationCount, 1U);

        pond::render::RenderFrame replacementFrame = std::move(replacementFrameResult).GetValue();
        ASSERT_TRUE(replacementFrame.Clear());
        const auto replacementPresent = replacementFrame.FinishAndPresent();
        ASSERT_TRUE(replacementPresent) << replacementPresent.GetError().GetMessage();
        EXPECT_TRUE(replacementPresent.GetValue().presented);
    }
}

TEST(RenderVulkanBootstrapTests, PublicTargetHostAllocationFailureRetiresOldSwapchain)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Replacement Host Allocation GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);

    ASSERT_TRUE(owners->target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
        pond::platform::WindowId{42}, pond::platform::PixelSize{801, 601}, true,
        pond::platform::WindowState::Normal, pond::render::PresentationEnvironmentRevision{1U},
        2U}));
    state.throwBadAllocOnSwapchainImageRead = true;
    const auto failedFrame = owners->target.AcquireFrame();

    ExpectRenderError(failedFrame, pond::render::RenderErrorCode::OutOfMemory);
    EXPECT_EQ(state.lastOldSwapchain, owners->initialSwapchain);
    EXPECT_EQ(state.destroySwapchainCalls, 1U);
    EXPECT_TRUE(owners->target.IsValid());
    EXPECT_EQ(owners->target.GetStatus(), pond::render::TargetStatus::Active);
    EXPECT_FALSE(owners->target.HasSwapchain());
    EXPECT_EQ(owners->target.GetSwapchainGeneration(), 0U);
    EXPECT_TRUE(owners->target.HasPendingRecreation());
    EXPECT_FALSE(owners->target.GetSelectedPresentationConfig().has_value());

    auto replacementFrameResult = owners->target.AcquireFrame();

    ASSERT_TRUE(replacementFrameResult) << replacementFrameResult.GetError().GetMessage();
    EXPECT_EQ(state.lastOldSwapchain, VK_NULL_HANDLE);
    EXPECT_TRUE(owners->target.HasSwapchain());
    EXPECT_EQ(owners->target.GetSwapchainGeneration(), 2U);
    EXPECT_FALSE(owners->target.HasPendingRecreation());
}

TEST(RenderVulkanBootstrapTests, PublicTargetReplacementFailureLeavesDestructibleState)
{
    struct FailureCase final
    {
        std::string_view name;
        VkResult FakeVulkanState::* resultField;
        std::uint32_t expectedDestroyedSwapchains{};
    };

    constexpr std::array failureCases{
        FailureCase{"native creation failure", &FakeVulkanState::createSwapchainResult, 1U},
        FailureCase{"dependent framebuffer failure", &FakeVulkanState::createFramebufferResult, 2U},
    };

    for (const FailureCase& failureCase : failureCases)
    {
        SCOPED_TRACE(std::string{failureCase.name});

        FakeVulkanState state;
        state.physicalDevices.push_back(MakeCompatibleFakeDevice(
            0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Replacement Shutdown GPU"));
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
        std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
        ASSERT_NE(owners, nullptr);

        ASSERT_TRUE(owners->target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
            pond::platform::WindowId{42}, pond::platform::PixelSize{801, 601}, true,
            pond::platform::WindowState::Normal, pond::render::PresentationEnvironmentRevision{1U},
            2U}));
        state.*(failureCase.resultField) = VK_ERROR_OUT_OF_HOST_MEMORY;
        ExpectRenderError(owners->target.AcquireFrame(),
                          pond::render::RenderErrorCode::OutOfMemory);
        ASSERT_TRUE(owners->target.IsValid());
        ASSERT_FALSE(owners->target.HasSwapchain());
        ASSERT_TRUE(owners->target.HasPendingRecreation());

        owners.reset();

        EXPECT_EQ(state.destroySwapchainCalls, failureCase.expectedDestroyedSwapchains);
        EXPECT_EQ(state.destroySurfaceCalls, 1U);
        EXPECT_EQ(state.destroyDeviceCalls, 1U);
    }
}
TEST(RenderVulkanBootstrapTests, PublicTargetStateAllocationFailureRollsBackAndCanRetry)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Target State Allocation GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    auto surfaceResult = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
        dispatch, instance, MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::Win32));
    ASSERT_TRUE(surfaceResult) << surfaceResult.GetError().GetMessage();
    pond::render::detail::VulkanSurfaceOwner surface = std::move(surfaceResult).GetValue();

    auto selection = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface.GetHandle(), pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selection) << selection.GetError().GetMessage();
    auto device = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface.GetHandle(), selection.GetValue(),
        pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(device) << device.GetError().GetMessage();

    const pond::render::RenderTargetDesc targetDesc = MakeTargetDesc();
    auto ownersResult =
        pond::render::detail::RenderBackendTestAccess::CreateDeviceAndPreparedSurface(
            dispatch, std::move(device).GetValue(), std::move(surface),
            pond::render::SurfacePreparationDesc{
                .targetSnapshot = targetDesc.targetSnapshot,
                .reason = pond::render::SurfacePreparationReason::Initial});
    ASSERT_TRUE(ownersResult) << ownersResult.GetError().GetMessage();
    auto owners = std::move(ownersResult).GetValue();

    pond::render::detail::RenderBackendTestAccess::FailNextTargetStateAllocation();
    const auto failedTarget =
        owners.device.CreateRenderTarget(std::move(owners.surface), targetDesc);

    ExpectRenderError(failedTarget, pond::render::RenderErrorCode::OutOfMemory);
    EXPECT_TRUE(owners.surface.IsValid());
    EXPECT_EQ(owners.device.GetActiveTargetCount(), 0U);
    EXPECT_EQ(pond::render::detail::RenderBackendTestAccess::GetBootstrapTargetCount(owners.device),
              0U);
    EXPECT_EQ(state.destroySurfaceCalls, 0U);
    EXPECT_EQ(owners.device.GetDiagnostics().targetCreateAttempts, 1U);
    EXPECT_EQ(owners.device.GetDiagnostics().targetCreateSuccesses, 0U);
    EXPECT_EQ(owners.device.GetDiagnostics().targetCreateFailures, 1U);

    auto retry = owners.device.CreateRenderTarget(std::move(owners.surface), targetDesc);
    ASSERT_TRUE(retry) << retry.GetError().GetMessage();
    EXPECT_FALSE(owners.surface.IsValid());
    EXPECT_EQ(owners.device.GetActiveTargetCount(), 1U);
    EXPECT_EQ(pond::render::detail::RenderBackendTestAccess::GetBootstrapTargetCount(owners.device),
              1U);
    EXPECT_EQ(owners.device.GetDiagnostics().targetCreateSuccesses, 1U);

    pond::render::RenderTarget target = std::move(retry).GetValue();
    target = pond::render::RenderTarget{};
    EXPECT_EQ(owners.device.GetActiveTargetCount(), 0U);
    EXPECT_EQ(pond::render::detail::RenderBackendTestAccess::GetBootstrapTargetCount(owners.device),
              0U);
}

TEST(RenderVulkanBootstrapTests,
     PublicSuspensionRetirementAllocationFailurePreservesTargetAndCanRetry)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Suspension Retirement Allocation GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicLifecycle> lifecycle = CreateFakePublicLifecycle(dispatch);
    ASSERT_NE(lifecycle, nullptr);

    pond::render::RenderTarget& target = lifecycle->owners.target;
    const pond::render::RenderTargetSnapshot initialSnapshot = target.GetTargetSnapshot();
    const pond::render::RenderTargetSnapshot minimizedSnapshot{
        initialSnapshot.GetWindowId(),
        pond::platform::PixelSize{},
        true,
        pond::platform::WindowState::Minimized,
        initialSnapshot.GetPresentationEnvironmentRevision(),
        2U};

    pond::render::detail::RenderBackendTestAccess::FailNextRetirementAllocation();
    const auto failedSuspension = target.UpdateTargetSnapshot(minimizedSnapshot);

    ExpectRenderError(failedSuspension, pond::render::RenderErrorCode::OutOfMemory);
    EXPECT_EQ(target.GetTargetSnapshot(), initialSnapshot);
    EXPECT_EQ(target.GetStatus(), pond::render::TargetStatus::Active);
    EXPECT_TRUE(target.HasSwapchain());
    EXPECT_EQ(target.GetDiagnostics().suspensionCount, 0U);
    EXPECT_EQ(target.GetPresentationRetirementStats().pendingResourceSets, 0U);
    EXPECT_EQ(state.deviceWaitIdleCalls, 0U);
    EXPECT_EQ(state.queueWaitIdleCalls, 0U);

    ASSERT_TRUE(target.UpdateTargetSnapshot(minimizedSnapshot));
    EXPECT_TRUE(target.IsSuspended());
    EXPECT_FALSE(target.HasSwapchain());
    EXPECT_EQ(target.GetDiagnostics().suspensionCount, 1U);
    const pond::render::PresentationRetirementStats retirement =
        target.GetPresentationRetirementStats();
    EXPECT_GE(retirement.pendingResourceSets + retirement.retiredResourceSets, 1U);
    EXPECT_EQ(state.deviceWaitIdleCalls, 0U);
}

TEST(RenderVulkanBootstrapTests,
     PublicTargetDestructionAllocationFailureUsesOnlyTerminalDeviceIdleFallback)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Destructor Retirement Allocation GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicLifecycle> lifecycle = CreateFakePublicLifecycle(dispatch);
    ASSERT_NE(lifecycle, nullptr);

    auto frameResult = lifecycle->owners.target.AcquireFrame();
    ASSERT_TRUE(frameResult) << frameResult.GetError().GetMessage();
    pond::render::RenderFrame frame = std::move(frameResult).GetValue();
    ASSERT_TRUE(frame.Clear());
    ASSERT_TRUE(frame.FinishAndPresent());

    pond::render::detail::RenderBackendTestAccess::FailNextRetirementAllocation();
    lifecycle->owners.target = pond::render::RenderTarget{};

    EXPECT_EQ(lifecycle->owners.device.GetActiveTargetCount(), 0U);
    EXPECT_EQ(lifecycle->owners.device.GetDiagnostics().practicalWaitFallbacks, 1U);
    EXPECT_EQ(state.deviceWaitIdleCalls, 1U);
    EXPECT_EQ(state.queueWaitIdleCalls, 0U);
    EXPECT_EQ(state.destroySwapchainCalls, 1U);
    EXPECT_EQ(state.destroySurfaceCalls, 1U);

    ASSERT_TRUE(pond::render::detail::RenderBackendTestAccess::DrainOrphanedPresentationResources(
        lifecycle->owners.device));
    EXPECT_EQ(state.deviceWaitIdleCalls, 1U);

    lifecycle.reset();
    EXPECT_EQ(state.destroyDeviceCalls, 1U);
}

TEST(RenderVulkanBootstrapTests, PublicFrameStateAllocationFailurePoisonsAndRecreates)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Frame State Allocation GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
    ASSERT_NE(owners, nullptr);

    pond::render::detail::RenderBackendTestAccess::FailNextFrameStateAllocation();
    const auto failedFrame = owners->target.AcquireFrame();

    ExpectRenderError(failedFrame, pond::render::RenderErrorCode::OutOfMemory);
    EXPECT_TRUE(owners->target.IsValid());
    EXPECT_TRUE(owners->target.HasPendingRecreation());
    EXPECT_EQ(owners->target.GetDiagnostics().frameAcquireFailures, 1U);
    EXPECT_EQ(owners->target.GetDiagnostics().frameFailures, 1U);
    EXPECT_EQ(state.queueSubmitCalls, 0U);
    EXPECT_EQ(state.queuePresentCalls, 0U);

    auto retry = owners->target.AcquireFrame();
    ASSERT_TRUE(retry) << retry.GetError().GetMessage();
    pond::render::RenderFrame frame = std::move(retry).GetValue();
    EXPECT_EQ(frame.GetStatus(), pond::render::FrameStatus::Recreated);
    ASSERT_TRUE(frame.Clear());
    const auto finish = frame.FinishAndPresent();
    ASSERT_TRUE(finish) << finish.GetError().GetMessage();
    EXPECT_TRUE(finish.GetValue().presented);
    EXPECT_EQ(finish.GetValue().status, pond::render::FrameStatus::Recreated);
}

TEST(RenderVulkanBootstrapTests, PublicNonRecordingFrameTokensBlockDuplicatesUntilReleased)
{
    {
        auto ownersResult =
            pond::render::detail::RenderBackendTestAccess::CreateLifetimeContractOwners(
                MakeTargetDesc(pond::render::PresentationPolicy::VSync, {},
                               pond::platform::PixelSize{800, 600}, false));
        ASSERT_TRUE(ownersResult) << ownersResult.GetError().GetMessage();
        auto owners = std::move(ownersResult).GetValue();

        auto skippedResult = owners.target.AcquireFrame();
        ASSERT_TRUE(skippedResult) << skippedResult.GetError().GetMessage();
        pond::render::RenderFrame skipped = std::move(skippedResult).GetValue();
        EXPECT_EQ(skipped.GetStatus(), pond::render::FrameStatus::SkippedSuspended);
        ExpectRenderError(owners.target.AcquireFrame(),
                          pond::render::RenderErrorCode::InvalidState);

        skipped = pond::render::RenderFrame{};
        auto retry = owners.target.AcquireFrame();
        ASSERT_TRUE(retry) << retry.GetError().GetMessage();
        ASSERT_TRUE(std::move(retry).GetValue().FinishAndPresent());
    }

    {
        FakeVulkanState state;
        state.physicalDevices.push_back(MakeCompatibleFakeDevice(
            0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Timed Out Token GPU"));
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
        std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
        ASSERT_NE(owners, nullptr);

        state.acquireNextImageResult = VK_TIMEOUT;
        auto timeoutResult = owners->target.AcquireFrame();
        ASSERT_TRUE(timeoutResult) << timeoutResult.GetError().GetMessage();
        pond::render::RenderFrame timeout = std::move(timeoutResult).GetValue();
        EXPECT_EQ(timeout.GetStatus(), pond::render::FrameStatus::TimedOut);
        ExpectRenderError(owners->target.AcquireFrame(),
                          pond::render::RenderErrorCode::InvalidState);
        ASSERT_TRUE(timeout.FinishAndPresent());

        state.acquireNextImageResult = VK_SUCCESS;
        auto retry = owners->target.AcquireFrame();
        ASSERT_TRUE(retry) << retry.GetError().GetMessage();
        pond::render::RenderFrame frame = std::move(retry).GetValue();
        ASSERT_TRUE(frame.Clear());
        ASSERT_TRUE(frame.FinishAndPresent());
    }

    {
        FakeVulkanState state;
        state.physicalDevices.push_back(MakeCompatibleFakeDevice(
            0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Recreation Pending Token GPU"));
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
        std::unique_ptr<FakePublicTarget> owners = CreateFakePublicTarget(dispatch);
        ASSERT_NE(owners, nullptr);

        state.acquireNextImageResult = VK_ERROR_OUT_OF_DATE_KHR;
        auto pendingResult = owners->target.AcquireFrame();
        ASSERT_TRUE(pendingResult) << pendingResult.GetError().GetMessage();
        pond::render::RenderFrame pending = std::move(pendingResult).GetValue();
        EXPECT_EQ(pending.GetStatus(), pond::render::FrameStatus::RecreationPending);
        ExpectRenderError(owners->target.AcquireFrame(),
                          pond::render::RenderErrorCode::InvalidState);

        pending = pond::render::RenderFrame{};
        state.acquireNextImageResult = VK_SUCCESS;
        auto retry = owners->target.AcquireFrame();
        ASSERT_TRUE(retry) << retry.GetError().GetMessage();
        pond::render::RenderFrame frame = std::move(retry).GetValue();
        EXPECT_EQ(frame.GetStatus(), pond::render::FrameStatus::Recreated);
        ASSERT_TRUE(frame.Clear());
        ASSERT_TRUE(frame.FinishAndPresent());
    }
}
TEST(RenderVulkanBootstrapTests, FrameResourcesPollAllFencesForRetirement)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Retirement Fence GPU"));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    auto frameResourcesResult = pond::render::detail::CreateVulkanFrameResourcesForTarget(
        dispatch, owner, pond::platform::WindowId{42}, owner.GetInfo().queuePlan,
        pond::render::QueuedFrameLatency{2}, state.swapchainImageCount);
    ASSERT_TRUE(frameResourcesResult) << frameResourcesResult.GetError().GetMessage();
    pond::render::detail::VulkanFrameResourcesOwner frameResources =
        std::move(frameResourcesResult).GetValue();

    const auto initiallyComplete = frameResources.AreAllFencesSignaled(dispatch);
    ASSERT_TRUE(initiallyComplete) << initiallyComplete.GetError().GetMessage();
    EXPECT_TRUE(initiallyComplete.GetValue());
    EXPECT_EQ(state.waitForFencesCalls, 0U);

    const pond::render::detail::VulkanFrameSlotResources firstSlot = frameResources.GetSlot(0U);
    frameResources.RecordImageAcquired(0U);
    state.waitForFencesResult = VK_TIMEOUT;
    const auto acquirePending = frameResources.AreAllFencesSignaled(dispatch);
    ASSERT_TRUE(acquirePending) << acquirePending.GetError().GetMessage();
    EXPECT_FALSE(acquirePending.GetValue());
    EXPECT_EQ(state.waitForFencesCalls, 1U);
    ASSERT_EQ(state.lastWaitForFences.size(), 1U);
    EXPECT_EQ(state.lastWaitForFences.front(), firstSlot.imageAcquiredFence);
    EXPECT_EQ(state.lastWaitForFencesWaitAll, VK_TRUE);
    EXPECT_EQ(state.lastWaitForFencesTimeout, 0U);

    state.waitForFencesResult = VK_SUCCESS;
    const auto acquireComplete = frameResources.AreAllFencesSignaled(dispatch);
    ASSERT_TRUE(acquireComplete) << acquireComplete.GetError().GetMessage();
    EXPECT_TRUE(acquireComplete.GetValue());
    EXPECT_EQ(state.waitForFencesCalls, 2U);

    frameResources.RecordSubmissionQueued(0U);
    const auto submissionComplete = frameResources.AreAllFencesSignaled(dispatch);
    ASSERT_TRUE(submissionComplete) << submissionComplete.GetError().GetMessage();
    EXPECT_TRUE(submissionComplete.GetValue());
    EXPECT_EQ(state.waitForFencesCalls, 3U);
    ASSERT_EQ(state.lastWaitForFences.size(), 1U);
    EXPECT_EQ(state.lastWaitForFences.front(), firstSlot.inFlightFence);

    const pond::render::detail::VulkanFrameSlotResources secondSlot = frameResources.GetSlot(1U);
    frameResources.RecordImageAcquired(1U);
    state.waitForFencesResult = VK_ERROR_DEVICE_LOST;
    ExpectRenderError(frameResources.AreAllFencesSignaled(dispatch),
                      pond::render::RenderErrorCode::DeviceLost);
    EXPECT_EQ(state.waitForFencesCalls, 4U);
    ASSERT_EQ(state.lastWaitForFences.size(), 1U);
    EXPECT_EQ(state.lastWaitForFences.front(), secondSlot.imageAcquiredFence);
}

TEST(RenderVulkanBootstrapTests, DeviceOwnerMoveKeepsWaitIdleDispatchAndDestroysOnce)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Movable Device GPU"));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();

    {
        auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
            dispatch, instance, surface, selectionResult.GetValue(),
            pond::render::RenderDeviceDesc{});
        ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
        pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();
        pond::render::detail::VulkanDeviceOwner movedOwner{std::move(owner)};

        EXPECT_FALSE(owner.IsValid());
        EXPECT_TRUE(movedOwner.IsValid());
        const std::uint32_t presentationQueueFamilyIndex =
            movedOwner.GetInfo().queuePlan.presentationQueueFamilyIndex;
        const VkQueue presentationQueue = movedOwner.GetQueue(presentationQueueFamilyIndex);
        ASSERT_NE(presentationQueue, VK_NULL_HANDLE);
        EXPECT_TRUE(movedOwner.WaitQueueIdle(presentationQueueFamilyIndex));
        EXPECT_EQ(state.lastQueueWaitIdle, presentationQueue);
        state.queueWaitIdleResult = VK_ERROR_DEVICE_LOST;
        ExpectRenderError(movedOwner.WaitQueueIdle(presentationQueueFamilyIndex),
                          pond::render::RenderErrorCode::DeviceLost);
        EXPECT_EQ(state.queueWaitIdleCalls, 2U);

        EXPECT_TRUE(movedOwner.WaitIdle());
        state.deviceWaitIdleResult = VK_ERROR_DEVICE_LOST;
        ExpectRenderError(movedOwner.WaitIdle(), pond::render::RenderErrorCode::DeviceLost);
        EXPECT_EQ(state.deviceWaitIdleCalls, 2U);
    }

    EXPECT_EQ(state.destroyAllocatorCalls, 1U);
    EXPECT_EQ(state.destroyDeviceCalls, 1U);
}
} // namespace

TEST(RenderVulkanBootstrapTests, CreatesFrameResourcesAndClearsSubmitsAndPresents)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(
        MakeCompatibleFakeDevice(0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Frame GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    auto swapchainResult = pond::render::detail::CreateVulkanSwapchainForTarget(
        dispatch, owner, surface, owner.GetInfo().queuePlan, MakeTargetDesc());
    ASSERT_TRUE(swapchainResult) << swapchainResult.GetError().GetMessage();
    pond::render::detail::VulkanSwapchainOwner swapchain = std::move(swapchainResult).GetValue();

    {
        auto frameResourcesResult = pond::render::detail::CreateVulkanFrameResourcesForTarget(
            dispatch, owner, swapchain.GetConfig().windowId, owner.GetInfo().queuePlan,
            swapchain.GetConfig().presentation.actualQueuedLatency,
            swapchain.GetConfig().imageCount);
        ASSERT_TRUE(frameResourcesResult) << frameResourcesResult.GetError().GetMessage();
        pond::render::detail::VulkanFrameResourcesOwner frameResources =
            std::move(frameResourcesResult).GetValue();
        auto presentationTrackerResult =
            pond::render::detail::CreateVulkanPresentationTrackerForTarget(
                dispatch, owner, swapchain.GetConfig().windowId, frameResources.GetSlotCount(),
                swapchain.GetConfig().imageCount);
        ASSERT_TRUE(presentationTrackerResult) << presentationTrackerResult.GetError().GetMessage();
        pond::render::detail::VulkanPresentationTrackerOwner presentationTracker =
            std::move(presentationTrackerResult).GetValue();
        ASSERT_TRUE(frameResources.IsValid());
        EXPECT_EQ(frameResources.GetSlotCount(),
                  swapchain.GetConfig().presentation.actualQueuedLatency.frameCount);

        auto beginResult = pond::render::detail::BeginVulkanFrame(
            dispatch, owner, swapchain, frameResources, presentationTracker,
            owner.GetInfo().queuePlan, 0U);
        ASSERT_TRUE(beginResult) << beginResult.GetError().GetMessage();
        ASSERT_EQ(beginResult.GetValue().status, pond::render::FrameStatus::Ready);
        pond::render::detail::VulkanFrameRecordingState recording =
            std::move(beginResult).GetValue().recording;

        const auto clearResult = pond::render::detail::RecordVulkanFrameClear(
            dispatch, swapchain, frameResources, recording,
            pond::render::ClearColor{.red = 0.1F, .green = 0.2F, .blue = 0.3F, .alpha = 1.0F});
        ASSERT_TRUE(clearResult) << clearResult.GetError().GetMessage();

        const auto frameResult = pond::render::detail::FinishAndPresentVulkanFrame(
            dispatch, owner, swapchain, frameResources, presentationTracker,
            owner.GetInfo().queuePlan, recording);
        ASSERT_TRUE(frameResult) << frameResult.GetError().GetMessage();
        EXPECT_EQ(frameResult.GetValue().status, pond::render::FrameStatus::Presented);
        EXPECT_TRUE(frameResult.GetValue().presented);
        EXPECT_FALSE(frameResult.GetValue().suboptimal);

        EXPECT_EQ(state.waitForFencesCalls, 0U);
        EXPECT_EQ(state.acquireNextImageCalls, 1U);
        EXPECT_EQ(state.resetCommandBufferCalls, 1U);
        EXPECT_EQ(state.beginCommandBufferCalls, 1U);
        EXPECT_EQ(state.cmdBeginRenderPassCalls, 1U);
        EXPECT_EQ(state.cmdEndRenderPassCalls, 1U);
        EXPECT_EQ(state.endCommandBufferCalls, 1U);
        EXPECT_EQ(state.resetFencesCalls, 1U);
        EXPECT_EQ(state.queueSubmitCalls, 1U);
        EXPECT_EQ(state.queuePresentCalls, 1U);
        EXPECT_EQ(state.beginLabelCalls, 1U);
        EXPECT_EQ(state.endLabelCalls, 1U);
        EXPECT_GT(state.setDebugNameCalls, 0U);
    }

    EXPECT_EQ(state.destroyCommandPoolCalls, 1U);
    EXPECT_EQ(state.destroySemaphoreCalls,
              swapchain.GetConfig().presentation.actualQueuedLatency.frameCount +
                  swapchain.GetConfig().imageCount);
    EXPECT_EQ(state.destroyFenceCalls,
              swapchain.GetConfig().presentation.actualQueuedLatency.frameCount * 2U);
}

TEST(RenderVulkanBootstrapTests, TargetLocalFinishPreparationCompletesBeforeTheExactQueueLock)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Queue Lock GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakeFrameTestOwners> owners = CreateFakeFrameTestOwners(dispatch);
    ASSERT_NE(owners, nullptr);

    auto begin = pond::render::detail::BeginVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, 0U);
    ASSERT_TRUE(begin) << begin.GetError().GetMessage();
    ASSERT_EQ(begin.GetValue().status, pond::render::FrameStatus::Ready);
    pond::render::detail::VulkanFrameRecordingState recording =
        std::move(begin).GetValue().recording;

    const auto clear = pond::render::detail::RecordVulkanFrameClear(
        dispatch, owners->swapchain, owners->frameResources, recording, pond::render::ClearColor{});
    ASSERT_TRUE(clear) << clear.GetError().GetMessage();

    const pond::render::detail::VulkanDeviceQueuePlan queuePlan =
        owners->device.GetInfo().queuePlan;
    const VkQueue graphicsQueue = owners->device.GetQueue(queuePlan.graphicsQueueFamilyIndex);
    ASSERT_NE(graphicsQueue, VK_NULL_HANDLE);

    std::binary_semaphore queueLockHeld{0};
    std::binary_semaphore releaseQueueLock{0};
    std::jthread blockedTarget{[&]
                               {
                                   [[maybe_unused]] auto queueLock =
                                       owners->device.LockQueueOperation(graphicsQueue);
                                   queueLockHeld.release();
                                   releaseQueueLock.acquire();
                               }};
    queueLockHeld.acquire();

    state.notifyNextResetFences.store(true);
    std::atomic<bool> finishSucceeded{};
    std::jthread independentTarget{
        [&]
        {
            const auto finish = pond::render::detail::FinishAndPresentVulkanFrame(
                dispatch, owners->device, owners->swapchain, owners->frameResources,
                owners->presentationTracker, queuePlan, recording);
            finishSucceeded.store(static_cast<bool>(finish));
        }};

    const bool preparationCompleted =
        state.resetFencesReached.try_acquire_for(std::chrono::seconds{1});
    if (preparationCompleted)
    {
        EXPECT_EQ(state.endCommandBufferCalls, 1U);
        EXPECT_EQ(state.resetFencesCalls, 1U);
        EXPECT_EQ(state.queueSubmitCalls, 0U);
    }

    releaseQueueLock.release();
    blockedTarget.join();
    independentTarget.join();

    EXPECT_TRUE(preparationCompleted)
        << "Target-local finish preparation stalled behind the Vulkan queue lock.";
    EXPECT_TRUE(finishSucceeded.load());
    EXPECT_EQ(state.queueSubmitCalls, 1U);
    EXPECT_EQ(state.queuePresentCalls, 1U);
}

TEST(RenderVulkanBootstrapTests, RecordsAcquireClearIntermediateSubmitAndPresentInExactOrder)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Frame Phase GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakeFrameTestOwners> owners = CreateFakeFrameTestOwners(dispatch);
    ASSERT_NE(owners, nullptr);
    state.frameTrace.clear();

    auto begin = pond::render::detail::BeginVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, 0U);
    ASSERT_TRUE(begin) << begin.GetError().GetMessage();
    ASSERT_EQ(begin.GetValue().status, pond::render::FrameStatus::Ready);
    pond::render::detail::VulkanFrameRecordingState recording =
        std::move(begin).GetValue().recording;
    EXPECT_EQ(recording.phase, pond::render::detail::VulkanFrameRecordingPhase::Recording);
    EXPECT_EQ(state.frameTrace, std::vector<std::string>{"acquire"});
    EXPECT_EQ(state.queueSubmitCalls, 0U);
    EXPECT_EQ(state.queuePresentCalls, 0U);

    const auto clear = pond::render::detail::RecordVulkanFrameClear(
        dispatch, owners->swapchain, owners->frameResources, recording, pond::render::ClearColor{});
    ASSERT_TRUE(clear) << clear.GetError().GetMessage();
    EXPECT_EQ(recording.phase, pond::render::detail::VulkanFrameRecordingPhase::ClearRecorded);
    EXPECT_EQ(state.frameTrace, (std::vector<std::string>{"acquire", "clear"}));
    EXPECT_EQ(state.queueSubmitCalls, 0U);
    EXPECT_EQ(state.queuePresentCalls, 0U);

    const auto intermediate = pond::render::detail::RecordVulkanIntermediateStageMarker(
        dispatch, owners->frameResources, recording);
    ASSERT_TRUE(intermediate) << intermediate.GetError().GetMessage();
    EXPECT_EQ(recording.phase,
              pond::render::detail::VulkanFrameRecordingPhase::IntermediateRecorded);
    EXPECT_EQ(state.frameTrace, (std::vector<std::string>{"acquire", "clear", "intermediate"}));
    EXPECT_EQ(state.queueSubmitCalls, 0U);
    EXPECT_EQ(state.queuePresentCalls, 0U);

    const auto finish = pond::render::detail::FinishAndPresentVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, recording);
    ASSERT_TRUE(finish) << finish.GetError().GetMessage();
    EXPECT_EQ(finish.GetValue().status, pond::render::FrameStatus::Presented);
    EXPECT_EQ(recording.phase, pond::render::detail::VulkanFrameRecordingPhase::Terminal);
    EXPECT_EQ(state.frameTrace,
              (std::vector<std::string>{"acquire", "clear", "intermediate", "submit", "present"}));
}

TEST(RenderVulkanBootstrapTests, MapsFrameAcquireTimeoutAndSubmitFailure)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Frame Failure GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    auto swapchainResult = pond::render::detail::CreateVulkanSwapchainForTarget(
        dispatch, owner, surface, owner.GetInfo().queuePlan, MakeTargetDesc());
    ASSERT_TRUE(swapchainResult) << swapchainResult.GetError().GetMessage();
    pond::render::detail::VulkanSwapchainOwner swapchain = std::move(swapchainResult).GetValue();

    auto frameResourcesResult = pond::render::detail::CreateVulkanFrameResourcesForTarget(
        dispatch, owner, swapchain.GetConfig().windowId, owner.GetInfo().queuePlan,
        swapchain.GetConfig().presentation.actualQueuedLatency, swapchain.GetConfig().imageCount);
    ASSERT_TRUE(frameResourcesResult) << frameResourcesResult.GetError().GetMessage();
    pond::render::detail::VulkanFrameResourcesOwner frameResources =
        std::move(frameResourcesResult).GetValue();
    auto presentationTrackerResult = pond::render::detail::CreateVulkanPresentationTrackerForTarget(
        dispatch, owner, swapchain.GetConfig().windowId, frameResources.GetSlotCount(),
        swapchain.GetConfig().imageCount);
    ASSERT_TRUE(presentationTrackerResult) << presentationTrackerResult.GetError().GetMessage();
    pond::render::detail::VulkanPresentationTrackerOwner presentationTracker =
        std::move(presentationTrackerResult).GetValue();

    state.acquireNextImageResult = VK_TIMEOUT;
    const auto timeoutResult =
        pond::render::detail::BeginVulkanFrame(dispatch, owner, swapchain, frameResources,
                                               presentationTracker, owner.GetInfo().queuePlan, 0U);
    ASSERT_TRUE(timeoutResult) << timeoutResult.GetError().GetMessage();
    EXPECT_EQ(timeoutResult.GetValue().status, pond::render::FrameStatus::TimedOut);
    EXPECT_EQ(state.queueSubmitCalls, 0U);
    EXPECT_EQ(state.queuePresentCalls, 0U);

    state.acquireNextImageResult = VK_SUCCESS;
    auto beginResult =
        pond::render::detail::BeginVulkanFrame(dispatch, owner, swapchain, frameResources,
                                               presentationTracker, owner.GetInfo().queuePlan, 0U);
    ASSERT_TRUE(beginResult) << beginResult.GetError().GetMessage();
    ASSERT_EQ(beginResult.GetValue().status, pond::render::FrameStatus::Ready);
    pond::render::detail::VulkanFrameRecordingState recording =
        std::move(beginResult).GetValue().recording;
    const auto clearResult = pond::render::detail::RecordVulkanFrameClear(
        dispatch, swapchain, frameResources, recording, pond::render::ClearColor{});
    ASSERT_TRUE(clearResult) << clearResult.GetError().GetMessage();

    state.queueSubmitResult = VK_ERROR_DEVICE_LOST;
    ExpectRenderError(pond::render::detail::FinishAndPresentVulkanFrame(
                          dispatch, owner, swapchain, frameResources, presentationTracker,
                          owner.GetInfo().queuePlan, recording),
                      pond::render::RenderErrorCode::DeviceLost);
    EXPECT_EQ(state.queueSubmitCalls, 1U);
    EXPECT_EQ(state.queuePresentCalls, 0U);
}

TEST(RenderVulkanBootstrapTests, PostAcquireFailuresPoisonAndRetireAgainstTheCorrectFence)
{
    struct FailureCase final
    {
        std::string_view name;
        VkResult FakeVulkanState::* resultField;
        bool reachesQueueSubmit{};
        bool queuesSubmission{};
    };

    constexpr std::array failureCases{
        FailureCase{"reset command buffer", &FakeVulkanState::resetCommandBufferResult},
        FailureCase{"begin command buffer", &FakeVulkanState::beginCommandBufferResult},
        FailureCase{"end command buffer", &FakeVulkanState::endCommandBufferResult},
        FailureCase{"reset frame fence", &FakeVulkanState::resetFencesResult},
        FailureCase{"queue submit", &FakeVulkanState::queueSubmitResult, true, false},
        FailureCase{"non-enqueued queue present", &FakeVulkanState::queuePresentResult, true, true},
    };

    for (const FailureCase& failureCase : failureCases)
    {
        SCOPED_TRACE(std::string{failureCase.name});

        FakeVulkanState state;
        state.physicalDevices.push_back(MakeCompatibleFakeDevice(
            0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Poison Recovery GPU"));
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
        std::unique_ptr<FakeFrameTestOwners> owners = CreateFakeFrameTestOwners(dispatch);
        ASSERT_NE(owners, nullptr);

        constexpr std::uint32_t kFrameSlot = 0U;
        const pond::render::detail::VulkanFrameSlotResources failedSlot =
            owners->frameResources.GetSlot(kFrameSlot);
        ASSERT_TRUE(failedSlot.IsValid());
        ASSERT_FALSE(owners->frameResources.IsPoisoned());

        const bool failsDuringBegin =
            failureCase.resultField == &FakeVulkanState::resetCommandBufferResult ||
            failureCase.resultField == &FakeVulkanState::beginCommandBufferResult;
        if (failsDuringBegin)
        {
            state.*(failureCase.resultField) = VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        auto beginResult = pond::render::detail::BeginVulkanFrame(
            dispatch, owners->device, owners->swapchain, owners->frameResources,
            owners->presentationTracker, owners->device.GetInfo().queuePlan, kFrameSlot);
        if (failsDuringBegin)
        {
            ExpectRenderError(beginResult, pond::render::RenderErrorCode::OutOfMemory);
        }
        else
        {
            ASSERT_TRUE(beginResult) << beginResult.GetError().GetMessage();
            ASSERT_EQ(beginResult.GetValue().status, pond::render::FrameStatus::Ready);
            pond::render::detail::VulkanFrameRecordingState recording =
                std::move(beginResult).GetValue().recording;
            const auto clearResult = pond::render::detail::RecordVulkanFrameClear(
                dispatch, owners->swapchain, owners->frameResources, recording,
                pond::render::ClearColor{});
            ASSERT_TRUE(clearResult) << clearResult.GetError().GetMessage();

            state.*(failureCase.resultField) = VK_ERROR_OUT_OF_HOST_MEMORY;
            const auto finishResult = pond::render::detail::FinishAndPresentVulkanFrame(
                dispatch, owners->device, owners->swapchain, owners->frameResources,
                owners->presentationTracker, owners->device.GetInfo().queuePlan, recording);
            ExpectRenderError(finishResult, pond::render::RenderErrorCode::OutOfMemory);
        }
        EXPECT_TRUE(owners->frameResources.IsPoisoned());
        ASSERT_EQ(state.acquiredFences.size(), 1U);
        EXPECT_EQ(state.acquiredFences.back(), failedSlot.imageAcquiredFence);
        EXPECT_NE(state.acquiredFences.back(), VK_NULL_HANDLE);
        EXPECT_EQ(state.queueSubmitCalls, failureCase.reachesQueueSubmit ? 1U : 0U);
        EXPECT_EQ(state.queuePresentCalls, failureCase.queuesSubmission ? 1U : 0U);
        EXPECT_FALSE(owners->presentationTracker.HasQueuedPresentation());

        if (failureCase.reachesQueueSubmit)
        {
            ASSERT_EQ(state.submittedFences.size(), 1U);
            EXPECT_EQ(state.submittedFences.back(), failedSlot.inFlightFence);
            ASSERT_EQ(state.submittedWaitSemaphores.size(), 1U);
            EXPECT_EQ(state.submittedWaitSemaphores.back(), failedSlot.imageAvailableSemaphore);
        }
        else
        {
            EXPECT_TRUE(state.submittedFences.empty());
            EXPECT_TRUE(state.submittedWaitSemaphores.empty());
        }

        const pond::render::detail::VulkanFrameSlotResources poisonedSlot =
            owners->frameResources.GetSlot(kFrameSlot);
        EXPECT_TRUE(poisonedSlot.acquireFencePending);
        EXPECT_EQ(poisonedSlot.submissionFencePending, failureCase.queuesSubmission);

        const auto retirementComplete = owners->frameResources.AreAllFencesSignaled(dispatch, 73U);
        ASSERT_TRUE(retirementComplete) << retirementComplete.GetError().GetMessage();
        EXPECT_TRUE(retirementComplete.GetValue());
        ASSERT_EQ(state.lastWaitForFences.size(), failureCase.queuesSubmission ? 2U : 1U);
        EXPECT_EQ(state.lastWaitForFences.front(), failedSlot.imageAcquiredFence);
        if (failureCase.queuesSubmission)
        {
            EXPECT_EQ(state.lastWaitForFences.back(), failedSlot.inFlightFence);
        }
        EXPECT_EQ(state.lastWaitForFencesTimeout, 73U);

        const pond::render::detail::VulkanFrameSlotResources retiredSlot =
            owners->frameResources.GetSlot(kFrameSlot);
        EXPECT_FALSE(retiredSlot.acquireFencePending);
        EXPECT_FALSE(retiredSlot.submissionFencePending);

        const std::uint32_t slotCount = owners->frameResources.GetSlotCount();
        const std::uint32_t imageCount = owners->swapchain.GetConfig().imageCount;
        const std::uint32_t destroyedCommandPoolsBefore = state.destroyCommandPoolCalls;
        const std::uint32_t destroyedSemaphoresBefore = state.destroySemaphoreCalls;
        const std::uint32_t destroyedFencesBefore = state.destroyFenceCalls;
        owners.reset();
        EXPECT_EQ(state.destroyCommandPoolCalls - destroyedCommandPoolsBefore, 1U);
        EXPECT_EQ(state.destroySemaphoreCalls - destroyedSemaphoresBefore, slotCount + imageCount);
        EXPECT_EQ(state.destroyFenceCalls - destroyedFencesBefore, slotCount * 2U);

        state.*(failureCase.resultField) = VK_SUCCESS;
        std::unique_ptr<FakeFrameTestOwners> replacementOwners =
            CreateFakeFrameTestOwners(dispatch);
        ASSERT_NE(replacementOwners, nullptr);
        EXPECT_FALSE(replacementOwners->frameResources.IsPoisoned());

        auto recoveredBegin = pond::render::detail::BeginVulkanFrame(
            dispatch, replacementOwners->device, replacementOwners->swapchain,
            replacementOwners->frameResources, replacementOwners->presentationTracker,
            replacementOwners->device.GetInfo().queuePlan, kFrameSlot);
        ASSERT_TRUE(recoveredBegin) << recoveredBegin.GetError().GetMessage();
        ASSERT_EQ(recoveredBegin.GetValue().status, pond::render::FrameStatus::Ready);
        pond::render::detail::VulkanFrameRecordingState recoveredRecording =
            std::move(recoveredBegin).GetValue().recording;
        const auto recoveredClear = pond::render::detail::RecordVulkanFrameClear(
            dispatch, replacementOwners->swapchain, replacementOwners->frameResources,
            recoveredRecording, pond::render::ClearColor{});
        ASSERT_TRUE(recoveredClear) << recoveredClear.GetError().GetMessage();
        const auto recoveredFrame = pond::render::detail::FinishAndPresentVulkanFrame(
            dispatch, replacementOwners->device, replacementOwners->swapchain,
            replacementOwners->frameResources, replacementOwners->presentationTracker,
            replacementOwners->device.GetInfo().queuePlan, recoveredRecording);
        ASSERT_TRUE(recoveredFrame) << recoveredFrame.GetError().GetMessage();
        EXPECT_EQ(recoveredFrame.GetValue().status, pond::render::FrameStatus::Presented);
        EXPECT_TRUE(recoveredFrame.GetValue().presented);
    }
}

TEST(RenderVulkanBootstrapTests, InvalidAcquiredImagePoisonsAndRetiresAgainstAcquireFence)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Invalid Image GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakeFrameTestOwners> owners = CreateFakeFrameTestOwners(dispatch);
    ASSERT_NE(owners, nullptr);

    constexpr std::uint32_t kFrameSlot = 0U;
    const pond::render::detail::VulkanFrameSlotResources slot =
        owners->frameResources.GetSlot(kFrameSlot);
    state.acquiredImageIndex = owners->swapchain.GetFramebufferCount();

    const auto failedFrame = pond::render::detail::BeginVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, kFrameSlot);

    ExpectRenderError(failedFrame, pond::render::RenderErrorCode::BackendFailure);
    EXPECT_TRUE(owners->frameResources.IsPoisoned());
    EXPECT_EQ(state.queueSubmitCalls, 0U);
    EXPECT_TRUE(owners->frameResources.GetSlot(kFrameSlot).acquireFencePending);

    const auto retirementComplete = owners->frameResources.AreAllFencesSignaled(dispatch, 79U);
    ASSERT_TRUE(retirementComplete) << retirementComplete.GetError().GetMessage();
    EXPECT_TRUE(retirementComplete.GetValue());
    ASSERT_EQ(state.lastWaitForFences.size(), 1U);
    EXPECT_EQ(state.lastWaitForFences.front(), slot.imageAcquiredFence);
}

TEST(RenderVulkanBootstrapTests,
     ReusesFrameAcquireSemaphoreOnlyAfterAcquireAndSubmissionFencesComplete)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Acquire Reuse GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakeFrameTestOwners> owners = CreateFakeFrameTestOwners(dispatch);
    ASSERT_NE(owners, nullptr);

    constexpr std::uint32_t kFrameSlot = 0U;
    const pond::render::detail::VulkanFrameSlotResources slot =
        owners->frameResources.GetSlot(kFrameSlot);
    auto firstBegin = pond::render::detail::BeginVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, kFrameSlot);
    ASSERT_TRUE(firstBegin) << firstBegin.GetError().GetMessage();
    ASSERT_EQ(firstBegin.GetValue().status, pond::render::FrameStatus::Ready);
    pond::render::detail::VulkanFrameRecordingState firstRecording =
        std::move(firstBegin).GetValue().recording;
    ASSERT_TRUE(pond::render::detail::RecordVulkanFrameClear(dispatch, owners->swapchain,
                                                             owners->frameResources, firstRecording,
                                                             pond::render::ClearColor{}));
    const auto firstFrame = pond::render::detail::FinishAndPresentVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, firstRecording);
    ASSERT_TRUE(firstFrame) << firstFrame.GetError().GetMessage();
    EXPECT_EQ(state.waitForFencesCalls, 0U);

    auto secondBegin = pond::render::detail::BeginVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, kFrameSlot);
    ASSERT_TRUE(secondBegin) << secondBegin.GetError().GetMessage();
    ASSERT_EQ(secondBegin.GetValue().status, pond::render::FrameStatus::Ready);
    pond::render::detail::VulkanFrameRecordingState secondRecording =
        std::move(secondBegin).GetValue().recording;
    ASSERT_TRUE(pond::render::detail::RecordVulkanFrameClear(
        dispatch, owners->swapchain, owners->frameResources, secondRecording,
        pond::render::ClearColor{}));
    const auto secondFrame = pond::render::detail::FinishAndPresentVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, secondRecording);
    ASSERT_TRUE(secondFrame) << secondFrame.GetError().GetMessage();

    ASSERT_EQ(state.acquiredSemaphores.size(), 2U);
    EXPECT_EQ(state.acquiredSemaphores[0], slot.imageAvailableSemaphore);
    EXPECT_EQ(state.acquiredSemaphores[1], slot.imageAvailableSemaphore);
    ASSERT_EQ(state.acquiredFences.size(), 2U);
    EXPECT_EQ(state.acquiredFences[0], slot.imageAcquiredFence);
    EXPECT_EQ(state.acquiredFences[1], slot.imageAcquiredFence);
    ASSERT_EQ(state.lastWaitForFences.size(), 2U);
    EXPECT_EQ(state.lastWaitForFences.front(), slot.imageAcquiredFence);
    EXPECT_EQ(state.lastWaitForFences.back(), slot.inFlightFence);
    EXPECT_EQ(state.waitForFencesCalls, 1U);
    EXPECT_EQ(state.resetFencesCalls, 3U);
}

TEST(RenderVulkanBootstrapTests, MapsOutOfDateAndSuboptimalPresentationStates)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Frame Recreation GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();
    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

    auto swapchainResult = pond::render::detail::CreateVulkanSwapchainForTarget(
        dispatch, owner, surface, owner.GetInfo().queuePlan, MakeTargetDesc());
    ASSERT_TRUE(swapchainResult) << swapchainResult.GetError().GetMessage();
    pond::render::detail::VulkanSwapchainOwner swapchain = std::move(swapchainResult).GetValue();

    auto frameResourcesResult = pond::render::detail::CreateVulkanFrameResourcesForTarget(
        dispatch, owner, swapchain.GetConfig().windowId, owner.GetInfo().queuePlan,
        swapchain.GetConfig().presentation.actualQueuedLatency, swapchain.GetConfig().imageCount);
    ASSERT_TRUE(frameResourcesResult) << frameResourcesResult.GetError().GetMessage();
    pond::render::detail::VulkanFrameResourcesOwner frameResources =
        std::move(frameResourcesResult).GetValue();

    auto presentationTrackerResult = pond::render::detail::CreateVulkanPresentationTrackerForTarget(
        dispatch, owner, swapchain.GetConfig().windowId, frameResources.GetSlotCount(),
        swapchain.GetConfig().imageCount);
    ASSERT_TRUE(presentationTrackerResult) << presentationTrackerResult.GetError().GetMessage();
    pond::render::detail::VulkanPresentationTrackerOwner presentationTracker =
        std::move(presentationTrackerResult).GetValue();
    state.acquireNextImageResult = VK_ERROR_OUT_OF_DATE_KHR;
    const auto outOfDateAcquireResult =
        pond::render::detail::BeginVulkanFrame(dispatch, owner, swapchain, frameResources,
                                               presentationTracker, owner.GetInfo().queuePlan, 0U);
    ASSERT_TRUE(outOfDateAcquireResult) << outOfDateAcquireResult.GetError().GetMessage();
    EXPECT_EQ(outOfDateAcquireResult.GetValue().status,
              pond::render::FrameStatus::RecreationPending);
    EXPECT_EQ(state.queueSubmitCalls, 0U);
    EXPECT_EQ(state.queuePresentCalls, 0U);

    state.acquireNextImageResult = VK_SUBOPTIMAL_KHR;
    auto suboptimalBegin =
        pond::render::detail::BeginVulkanFrame(dispatch, owner, swapchain, frameResources,
                                               presentationTracker, owner.GetInfo().queuePlan, 0U);
    ASSERT_TRUE(suboptimalBegin) << suboptimalBegin.GetError().GetMessage();
    ASSERT_EQ(suboptimalBegin.GetValue().status, pond::render::FrameStatus::Ready);
    pond::render::detail::VulkanFrameRecordingState suboptimalRecording =
        std::move(suboptimalBegin).GetValue().recording;
    EXPECT_TRUE(suboptimalRecording.suboptimal);
    ASSERT_TRUE(pond::render::detail::RecordVulkanFrameClear(
        dispatch, swapchain, frameResources, suboptimalRecording, pond::render::ClearColor{}));
    const auto suboptimalAcquireResult = pond::render::detail::FinishAndPresentVulkanFrame(
        dispatch, owner, swapchain, frameResources, presentationTracker, owner.GetInfo().queuePlan,
        suboptimalRecording);
    ASSERT_TRUE(suboptimalAcquireResult) << suboptimalAcquireResult.GetError().GetMessage();
    EXPECT_EQ(suboptimalAcquireResult.GetValue().status, pond::render::FrameStatus::Suboptimal);
    EXPECT_TRUE(suboptimalAcquireResult.GetValue().presented);
    EXPECT_TRUE(suboptimalAcquireResult.GetValue().suboptimal);
    EXPECT_EQ(state.queueSubmitCalls, 1U);
    EXPECT_EQ(state.queuePresentCalls, 1U);

    state.acquireNextImageResult = VK_SUCCESS;
    auto outOfDatePresentBegin =
        pond::render::detail::BeginVulkanFrame(dispatch, owner, swapchain, frameResources,
                                               presentationTracker, owner.GetInfo().queuePlan, 0U);
    ASSERT_TRUE(outOfDatePresentBegin) << outOfDatePresentBegin.GetError().GetMessage();
    ASSERT_EQ(outOfDatePresentBegin.GetValue().status, pond::render::FrameStatus::Ready);
    pond::render::detail::VulkanFrameRecordingState outOfDatePresentRecording =
        std::move(outOfDatePresentBegin).GetValue().recording;
    ASSERT_TRUE(pond::render::detail::RecordVulkanFrameClear(dispatch, swapchain, frameResources,
                                                             outOfDatePresentRecording,
                                                             pond::render::ClearColor{}));

    state.queuePresentResult = VK_ERROR_OUT_OF_DATE_KHR;
    const auto outOfDatePresentResult = pond::render::detail::FinishAndPresentVulkanFrame(
        dispatch, owner, swapchain, frameResources, presentationTracker, owner.GetInfo().queuePlan,
        outOfDatePresentRecording);
    ASSERT_TRUE(outOfDatePresentResult) << outOfDatePresentResult.GetError().GetMessage();
    EXPECT_EQ(outOfDatePresentResult.GetValue().status,
              pond::render::FrameStatus::RecreationPending);
    EXPECT_FALSE(outOfDatePresentResult.GetValue().presented);
    EXPECT_FALSE(outOfDatePresentResult.GetValue().suboptimal);
    EXPECT_EQ(state.queueSubmitCalls, 2U);
    EXPECT_EQ(state.queuePresentCalls, 2U);
}
TEST(RenderVulkanBootstrapTests, CoreAcquireHistoryRequiresMatchingAcquireSubmissionFenceCompletion)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Core History GPU"));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakeFrameTestOwners> owners = CreateFakeFrameTestOwners(dispatch);
    ASSERT_NE(owners, nullptr);
    ASSERT_GE(owners->frameResources.GetSlotCount(), 2U);
    ASSERT_GE(owners->swapchain.GetConfig().imageCount, 2U);
    EXPECT_EQ(owners->presentationTracker.GetCompletionPath(),
              pond::render::detail::VulkanPresentationCompletionPath::CoreAcquireHistory);

    owners->presentationTracker.RecordPresentResult(0U, 0U, 0U, VK_SUCCESS);
    const auto pending =
        owners->presentationTracker.PollCompletion(dispatch, owners->swapchain.GetHandle(), 0U);
    ASSERT_TRUE(pending) << pending.GetError().GetMessage();
    EXPECT_EQ(pending.GetValue().status,
              pond::render::detail::VulkanPresentationCompletionStatus::Pending);
    EXPECT_FALSE(owners->presentationTracker.ConsumeCorePresentationCompletion());

    owners->presentationTracker.RecordImageAcquired(0U, 1U);
    owners->presentationTracker.RecordAcquireSubmission(0U);
    owners->presentationTracker.RecordFrameFenceCompletion(0U);
    EXPECT_FALSE(owners->presentationTracker.ConsumeCorePresentationCompletion());

    owners->presentationTracker.RecordImageAcquired(1U, 0U);
    owners->presentationTracker.RecordFrameFenceCompletion(1U);
    EXPECT_FALSE(owners->presentationTracker.ConsumeCorePresentationCompletion());

    owners->presentationTracker.RecordAcquireSubmission(1U);
    EXPECT_FALSE(owners->presentationTracker.ConsumeCorePresentationCompletion());

    owners->presentationTracker.RecordFrameFenceCompletion(0U);
    EXPECT_FALSE(owners->presentationTracker.ConsumeCorePresentationCompletion());

    owners->presentationTracker.RecordFrameFenceCompletion(1U);
    EXPECT_TRUE(owners->presentationTracker.ConsumeCorePresentationCompletion());
    EXPECT_FALSE(owners->presentationTracker.ConsumeCorePresentationCompletion());
}

TEST(RenderVulkanBootstrapTests, RenderFinishedSemaphoresFollowAcquiredSwapchainImages)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Image Semaphore GPU"));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakeFrameTestOwners> owners = CreateFakeFrameTestOwners(dispatch);
    ASSERT_NE(owners, nullptr);
    ASSERT_GE(owners->frameResources.GetSlotCount(), 2U);
    ASSERT_GE(owners->swapchain.GetConfig().imageCount, 2U);

    state.acquiredImageIndex = 0U;
    auto firstBegin = pond::render::detail::BeginVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, 0U);
    ASSERT_TRUE(firstBegin) << firstBegin.GetError().GetMessage();
    ASSERT_EQ(firstBegin.GetValue().status, pond::render::FrameStatus::Ready);
    pond::render::detail::VulkanFrameRecordingState firstRecording =
        std::move(firstBegin).GetValue().recording;
    ASSERT_TRUE(pond::render::detail::RecordVulkanFrameClear(dispatch, owners->swapchain,
                                                             owners->frameResources, firstRecording,
                                                             pond::render::ClearColor{}));
    const auto firstFrame = pond::render::detail::FinishAndPresentVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, firstRecording);
    ASSERT_TRUE(firstFrame) << firstFrame.GetError().GetMessage();

    state.acquiredImageIndex = 1U;
    auto secondBegin = pond::render::detail::BeginVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, 1U);
    ASSERT_TRUE(secondBegin) << secondBegin.GetError().GetMessage();
    ASSERT_EQ(secondBegin.GetValue().status, pond::render::FrameStatus::Ready);
    pond::render::detail::VulkanFrameRecordingState secondRecording =
        std::move(secondBegin).GetValue().recording;
    ASSERT_TRUE(pond::render::detail::RecordVulkanFrameClear(
        dispatch, owners->swapchain, owners->frameResources, secondRecording,
        pond::render::ClearColor{}));
    const auto secondFrame = pond::render::detail::FinishAndPresentVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, secondRecording);
    ASSERT_TRUE(secondFrame) << secondFrame.GetError().GetMessage();

    ASSERT_EQ(state.submittedSignalSemaphores.size(), 2U);
    ASSERT_EQ(state.presentWaitSemaphores.size(), 2U);
    for (std::uint32_t imageIndex = 0U; imageIndex < 2U; ++imageIndex)
    {
        EXPECT_EQ(state.submittedSignalSemaphores[imageIndex],
                  owners->frameResources.GetRenderFinishedSemaphore(imageIndex));
        EXPECT_EQ(state.presentWaitSemaphores[imageIndex],
                  state.submittedSignalSemaphores[imageIndex]);
    }
    EXPECT_NE(state.submittedSignalSemaphores[0], state.submittedSignalSemaphores[1]);
}

TEST(RenderVulkanBootstrapTests, MaintenancePresentFencePollsIndependentlyFromGraphicsFence)
{
    FakeVulkanState state;
    AddSurfaceMaintenanceSupport(state);
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Present Fence GPU");
    device.deviceExtensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    device.swapchainMaintenance1Feature = true;
    state.physicalDevices.push_back(std::move(device));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakeFrameTestOwners> owners = CreateFakeFrameTestOwners(dispatch);
    ASSERT_NE(owners, nullptr);

    ASSERT_FALSE(state.debugObjectNames.empty());
    for (const std::string& objectName : state.debugObjectNames)
    {
        EXPECT_NE(objectName.find("target/window:"), std::string::npos) << objectName;
        EXPECT_NE(objectName.find("target/window:42"), std::string::npos) << objectName;
    }
    const auto hasNamedResource = [&state](std::string_view resourceName)
    {
        return std::ranges::any_of(state.debugObjectNames,
                                   [resourceName](const std::string& objectName)
                                   {
                                       return objectName.find(resourceName) != std::string::npos;
                                   });
    };
    EXPECT_TRUE(hasNamedResource("/swapchain"));
    EXPECT_TRUE(hasNamedResource("/command_pool"));
    EXPECT_TRUE(hasNamedResource("/present.0"));

    EXPECT_EQ(owners->presentationTracker.GetCompletionPath(),
              pond::render::detail::VulkanPresentationCompletionPath::SwapchainMaintenanceFence);
    EXPECT_TRUE(owners->presentationTracker.UsesPresentFences());
    EXPECT_FALSE(owners->presentationTracker.UsesPresentIds());

    const pond::render::detail::VulkanFrameSlotResources frameSlot =
        owners->frameResources.GetSlot(0U);
    const VkFence acquireFence = frameSlot.imageAcquiredFence;
    const VkFence graphicsFence = frameSlot.inFlightFence;
    auto begin = pond::render::detail::BeginVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, 0U);
    ASSERT_TRUE(begin) << begin.GetError().GetMessage();
    ASSERT_EQ(begin.GetValue().status, pond::render::FrameStatus::Ready);
    pond::render::detail::VulkanFrameRecordingState recording =
        std::move(begin).GetValue().recording;
    ASSERT_TRUE(pond::render::detail::RecordVulkanFrameClear(dispatch, owners->swapchain,
                                                             owners->frameResources, recording,
                                                             pond::render::ClearColor{}));
    const auto frame = pond::render::detail::FinishAndPresentVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, recording);
    ASSERT_TRUE(frame) << frame.GetError().GetMessage();
    ASSERT_EQ(state.presentedFences.size(), 1U);
    EXPECT_NE(state.presentedFences[0], VK_NULL_HANDLE);
    ASSERT_EQ(state.presentedIds.size(), 1U);
    EXPECT_EQ(state.presentedIds[0], 0U);
    EXPECT_TRUE(state.lastWaitForFences.empty());

    state.waitForFencesResult = VK_TIMEOUT;
    const auto graphicsPending = owners->frameResources.AreAllFencesSignaled(dispatch);
    ASSERT_TRUE(graphicsPending) << graphicsPending.GetError().GetMessage();
    EXPECT_FALSE(graphicsPending.GetValue());
    ASSERT_EQ(state.lastWaitForFences.size(), 2U);
    EXPECT_EQ(state.lastWaitForFences[0], acquireFence);
    EXPECT_EQ(state.lastWaitForFences[1], graphicsFence);

    const auto pending =
        owners->presentationTracker.PollCompletion(dispatch, owners->swapchain.GetHandle(), 0U);
    ASSERT_TRUE(pending) << pending.GetError().GetMessage();
    EXPECT_EQ(pending.GetValue().status,
              pond::render::detail::VulkanPresentationCompletionStatus::Pending);
    ASSERT_EQ(state.lastWaitForFences.size(), 1U);
    EXPECT_EQ(state.lastWaitForFences[0], state.presentedFences[0]);
    EXPECT_NE(state.lastWaitForFences[0], graphicsFence);

    state.waitForFencesResult = VK_SUCCESS;
    const auto complete =
        owners->presentationTracker.PollCompletion(dispatch, owners->swapchain.GetHandle(), 0U);
    ASSERT_TRUE(complete) << complete.GetError().GetMessage();
    EXPECT_EQ(complete.GetValue().status,
              pond::render::detail::VulkanPresentationCompletionStatus::Complete);
    EXPECT_TRUE(complete.GetValue().usedExplicitCompletion);
    EXPECT_EQ(state.waitForFencesCalls, 3U);
}

TEST(RenderVulkanBootstrapTests, PresentIdsAreIncreasingAndPresentWaitCanBePolled)
{
    FakeVulkanState state;
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Present Wait GPU");
    device.deviceExtensions.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
    device.deviceExtensions.push_back(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
    device.presentIdFeature = true;
    device.presentWaitFeature = true;
    state.physicalDevices.push_back(std::move(device));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakeFrameTestOwners> owners = CreateFakeFrameTestOwners(dispatch);
    ASSERT_NE(owners, nullptr);
    EXPECT_EQ(owners->presentationTracker.GetCompletionPath(),
              pond::render::detail::VulkanPresentationCompletionPath::PresentWait);
    EXPECT_FALSE(owners->presentationTracker.UsesPresentFences());
    EXPECT_TRUE(owners->presentationTracker.UsesPresentIds());

    for (std::uint32_t frameSlot = 0U; frameSlot < 2U; ++frameSlot)
    {
        auto begin = pond::render::detail::BeginVulkanFrame(
            dispatch, owners->device, owners->swapchain, owners->frameResources,
            owners->presentationTracker, owners->device.GetInfo().queuePlan, frameSlot);
        ASSERT_TRUE(begin) << begin.GetError().GetMessage();
        ASSERT_EQ(begin.GetValue().status, pond::render::FrameStatus::Ready);
        pond::render::detail::VulkanFrameRecordingState recording =
            std::move(begin).GetValue().recording;
        ASSERT_TRUE(pond::render::detail::RecordVulkanFrameClear(dispatch, owners->swapchain,
                                                                 owners->frameResources, recording,
                                                                 pond::render::ClearColor{}));
        const auto frame = pond::render::detail::FinishAndPresentVulkanFrame(
            dispatch, owners->device, owners->swapchain, owners->frameResources,
            owners->presentationTracker, owners->device.GetInfo().queuePlan, recording);
        ASSERT_TRUE(frame) << frame.GetError().GetMessage();
    }

    ASSERT_EQ(state.presentedIds.size(), 2U);
    EXPECT_NE(state.presentedIds[0], 0U);
    EXPECT_LT(state.presentedIds[0], state.presentedIds[1]);
    ASSERT_EQ(state.presentedFences.size(), 2U);
    EXPECT_EQ(state.presentedFences[0], VK_NULL_HANDLE);
    EXPECT_EQ(state.presentedFences[1], VK_NULL_HANDLE);

    state.waitForPresentResult = VK_TIMEOUT;
    const auto pending =
        owners->presentationTracker.PollCompletion(dispatch, owners->swapchain.GetHandle(), 17U);
    ASSERT_TRUE(pending) << pending.GetError().GetMessage();
    EXPECT_EQ(pending.GetValue().status,
              pond::render::detail::VulkanPresentationCompletionStatus::Pending);
    EXPECT_EQ(state.lastWaitForPresentDevice, owners->device.GetHandle());
    EXPECT_EQ(state.lastWaitForPresentSwapchain, owners->swapchain.GetHandle());
    EXPECT_EQ(state.lastWaitForPresentId, state.presentedIds[1]);
    EXPECT_EQ(state.lastWaitForPresentTimeout, 17U);

    state.waitForPresentResult = VK_SUCCESS;
    const auto complete =
        owners->presentationTracker.PollCompletion(dispatch, owners->swapchain.GetHandle(), 29U);
    ASSERT_TRUE(complete) << complete.GetError().GetMessage();
    EXPECT_EQ(complete.GetValue().status,
              pond::render::detail::VulkanPresentationCompletionStatus::Complete);
    EXPECT_TRUE(complete.GetValue().usedExplicitCompletion);
    EXPECT_EQ(state.lastWaitForPresentId, state.presentedIds[1]);
    EXPECT_EQ(state.lastWaitForPresentTimeout, 29U);
    EXPECT_EQ(state.waitForPresentCalls, 2U);
}

TEST(RenderVulkanBootstrapTests, PresentWaitTimeoutDemotesToCoreSuccessorCompletion)
{
    FakeVulkanState state;
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Present Wait Demotion GPU");
    device.deviceExtensions.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
    device.deviceExtensions.push_back(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
    device.presentIdFeature = true;
    device.presentWaitFeature = true;
    state.physicalDevices.push_back(std::move(device));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakeFrameTestOwners> owners = CreateFakeFrameTestOwners(dispatch);
    ASSERT_NE(owners, nullptr);
    ASSERT_EQ(owners->presentationTracker.GetCompletionPath(),
              pond::render::detail::VulkanPresentationCompletionPath::PresentWait);

    const std::uint64_t presentId = owners->presentationTracker.ReservePresentId();
    ASSERT_EQ(presentId, 1U);
    owners->presentationTracker.RecordPresentResult(0U, 0U, presentId, VK_SUCCESS);
    ASSERT_TRUE(owners->presentationTracker.HasQueuedPresentation());

    state.waitForPresentResult = VK_TIMEOUT;
    const auto waitPending =
        owners->presentationTracker.PollCompletion(dispatch, owners->swapchain.GetHandle(), 11U);
    ASSERT_TRUE(waitPending) << waitPending.GetError().GetMessage();
    EXPECT_EQ(waitPending.GetValue().status,
              pond::render::detail::VulkanPresentationCompletionStatus::Pending);
    EXPECT_FALSE(waitPending.GetValue().usedExplicitCompletion);
    EXPECT_EQ(state.waitForPresentCalls, 1U);

    owners->presentationTracker.MarkAwaitingSuccessorPresentation();
    EXPECT_EQ(owners->presentationTracker.GetCompletionPath(),
              pond::render::detail::VulkanPresentationCompletionPath::CoreAcquireHistory);
    const auto corePending =
        owners->presentationTracker.PollCompletion(dispatch, owners->swapchain.GetHandle(), 13U);
    ASSERT_TRUE(corePending) << corePending.GetError().GetMessage();
    EXPECT_EQ(corePending.GetValue().status,
              pond::render::detail::VulkanPresentationCompletionStatus::Pending);
    EXPECT_EQ(state.waitForPresentCalls, 1U);

    owners->presentationTracker.MarkSuccessorPresentationComplete();
    const auto complete =
        owners->presentationTracker.PollCompletion(dispatch, owners->swapchain.GetHandle(), 17U);
    ASSERT_TRUE(complete) << complete.GetError().GetMessage();
    EXPECT_EQ(complete.GetValue().status,
              pond::render::detail::VulkanPresentationCompletionStatus::Complete);
    EXPECT_FALSE(complete.GetValue().usedExplicitCompletion);
    EXPECT_EQ(state.waitForPresentCalls, 1U);
}

TEST(RenderVulkanBootstrapTests, PresentErrorsTrackOnlyEnqueuedMaintenanceFences)
{
    FakeVulkanState state;
    AddSurfaceMaintenanceSupport(state);
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Present Error GPU");
    device.deviceExtensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
    device.swapchainMaintenance1Feature = true;
    state.physicalDevices.push_back(std::move(device));

    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakeFrameTestOwners> owners = CreateFakeFrameTestOwners(dispatch);
    ASSERT_NE(owners, nullptr);

    auto outOfDateBegin = pond::render::detail::BeginVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, 0U);
    ASSERT_TRUE(outOfDateBegin) << outOfDateBegin.GetError().GetMessage();
    ASSERT_EQ(outOfDateBegin.GetValue().status, pond::render::FrameStatus::Ready);
    pond::render::detail::VulkanFrameRecordingState outOfDateRecording =
        std::move(outOfDateBegin).GetValue().recording;
    ASSERT_TRUE(pond::render::detail::RecordVulkanFrameClear(
        dispatch, owners->swapchain, owners->frameResources, outOfDateRecording,
        pond::render::ClearColor{}));

    state.queuePresentResult = VK_ERROR_OUT_OF_DATE_KHR;
    const auto outOfDate = pond::render::detail::FinishAndPresentVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources,
        owners->presentationTracker, owners->device.GetInfo().queuePlan, outOfDateRecording);
    ASSERT_TRUE(outOfDate) << outOfDate.GetError().GetMessage();
    EXPECT_EQ(outOfDate.GetValue().status, pond::render::FrameStatus::RecreationPending);
    EXPECT_TRUE(owners->presentationTracker.HasQueuedPresentation());

    state.waitForFencesResult = VK_TIMEOUT;
    const auto outOfDatePending =
        owners->presentationTracker.PollCompletion(dispatch, owners->swapchain.GetHandle(), 0U);
    ASSERT_TRUE(outOfDatePending) << outOfDatePending.GetError().GetMessage();
    EXPECT_EQ(outOfDatePending.GetValue().status,
              pond::render::detail::VulkanPresentationCompletionStatus::Pending);

    auto oomTrackerResult = pond::render::detail::CreateVulkanPresentationTrackerForTarget(
        dispatch, owners->device, owners->swapchain.GetConfig().windowId,
        owners->frameResources.GetSlotCount(), owners->swapchain.GetConfig().imageCount);
    ASSERT_TRUE(oomTrackerResult) << oomTrackerResult.GetError().GetMessage();
    pond::render::detail::VulkanPresentationTrackerOwner oomTracker =
        std::move(oomTrackerResult).GetValue();
    state.waitForFencesResult = VK_SUCCESS;
    state.queuePresentResult = VK_SUCCESS;
    auto oomBegin = pond::render::detail::BeginVulkanFrame(
        dispatch, owners->device, owners->swapchain, owners->frameResources, oomTracker,
        owners->device.GetInfo().queuePlan, 1U);
    ASSERT_TRUE(oomBegin) << oomBegin.GetError().GetMessage();
    ASSERT_EQ(oomBegin.GetValue().status, pond::render::FrameStatus::Ready);
    pond::render::detail::VulkanFrameRecordingState oomRecording =
        std::move(oomBegin).GetValue().recording;
    ASSERT_TRUE(pond::render::detail::RecordVulkanFrameClear(dispatch, owners->swapchain,
                                                             owners->frameResources, oomRecording,
                                                             pond::render::ClearColor{}));

    state.queuePresentResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    ExpectRenderError(pond::render::detail::FinishAndPresentVulkanFrame(
                          dispatch, owners->device, owners->swapchain, owners->frameResources,
                          oomTracker, owners->device.GetInfo().queuePlan, oomRecording),
                      pond::render::RenderErrorCode::OutOfMemory);
    EXPECT_FALSE(oomTracker.HasQueuedPresentation());

    const std::uint32_t waitsBeforeOomPoll = state.waitForFencesCalls;
    const auto oomComplete = oomTracker.PollCompletion(dispatch, owners->swapchain.GetHandle(), 0U);
    ASSERT_TRUE(oomComplete) << oomComplete.GetError().GetMessage();
    EXPECT_EQ(oomComplete.GetValue().status,
              pond::render::detail::VulkanPresentationCompletionStatus::Complete);
    EXPECT_EQ(state.waitForFencesCalls, waitsBeforeOomPoll);
}

void ExpectPublicLifecycleShutdown(FakePublicLifecycle& fixture);

TEST(RenderVulkanBootstrapTests,
     CreationRollbackMatrixReleasesEveryNativeOwnerAndPreservesStructuredFailure)
{
    enum class FailureStage : std::uint8_t
    {
        Loader,
        InstanceEnumeration,
        Instance,
        DebugMessenger,
        Surface,
        Adapter,
        Device,
        Allocator,
        Swapchain,
        SwapchainImages,
        ImageView,
        RenderPass,
        Framebuffer,
        CommandPool,
        CommandBuffers,
        Semaphore,
        Fence
    };
    struct FailureCase final
    {
        std::string_view name;
        FailureStage stage;
        VkResult FakeVulkanState::* resultField;
        std::string_view operation;
        pond::render::RenderErrorCode renderCode{pond::render::RenderErrorCode::OutOfMemory};
    };

    constexpr std::array kCases{
        FailureCase{"loader", FailureStage::Loader, &FakeVulkanState::initializeResult,
                    "volkInitialize", pond::render::RenderErrorCode::LoaderUnavailable},
        FailureCase{"instance version", FailureStage::InstanceEnumeration,
                    &FakeVulkanState::enumerateVersionResult, "vkEnumerateInstanceVersion",
                    pond::render::RenderErrorCode::BackendFailure},
        FailureCase{"instance extension count", FailureStage::InstanceEnumeration,
                    &FakeVulkanState::enumerateExtensionsResult,
                    "vkEnumerateInstanceExtensionProperties.count",
                    pond::render::RenderErrorCode::BackendFailure},
        FailureCase{"instance extension read", FailureStage::InstanceEnumeration,
                    &FakeVulkanState::enumerateExtensionsReadResult,
                    "vkEnumerateInstanceExtensionProperties.read",
                    pond::render::RenderErrorCode::BackendFailure},
        FailureCase{"instance layer count", FailureStage::InstanceEnumeration,
                    &FakeVulkanState::enumerateLayersResult,
                    "vkEnumerateInstanceLayerProperties.count",
                    pond::render::RenderErrorCode::BackendFailure},
        FailureCase{"instance layer read", FailureStage::InstanceEnumeration,
                    &FakeVulkanState::enumerateLayersReadResult,
                    "vkEnumerateInstanceLayerProperties.read",
                    pond::render::RenderErrorCode::BackendFailure},
        FailureCase{"instance", FailureStage::Instance, &FakeVulkanState::createInstanceResult,
                    "vkCreateInstance"},
        FailureCase{"debug messenger", FailureStage::DebugMessenger,
                    &FakeVulkanState::createDebugMessengerResult, "vkCreateDebugUtilsMessengerEXT"},
        FailureCase{"surface", FailureStage::Surface, &FakeVulkanState::createWin32SurfaceResult,
                    "vkCreateWin32SurfaceKHR"},
        FailureCase{"physical device count", FailureStage::Adapter,
                    &FakeVulkanState::enumeratePhysicalDevicesResult,
                    "vkEnumeratePhysicalDevices.count"},
        FailureCase{"physical device read", FailureStage::Adapter,
                    &FakeVulkanState::enumeratePhysicalDevicesReadResult,
                    "vkEnumeratePhysicalDevices.read"},
        FailureCase{"surface support", FailureStage::Adapter,
                    &FakeVulkanState::getSurfaceSupportResult,
                    "vkGetPhysicalDeviceSurfaceSupportKHR"},
        FailureCase{"surface format count", FailureStage::Adapter,
                    &FakeVulkanState::getSurfaceFormatsResult,
                    "vkGetPhysicalDeviceSurfaceFormatsKHR.count"},
        FailureCase{"surface format read", FailureStage::Adapter,
                    &FakeVulkanState::getSurfaceFormatsReadResult,
                    "vkGetPhysicalDeviceSurfaceFormatsKHR.read"},
        FailureCase{"present mode count", FailureStage::Adapter,
                    &FakeVulkanState::getPresentModesResult,
                    "vkGetPhysicalDeviceSurfacePresentModesKHR.count"},
        FailureCase{"present mode read", FailureStage::Adapter,
                    &FakeVulkanState::getPresentModesReadResult,
                    "vkGetPhysicalDeviceSurfacePresentModesKHR.read"},
        FailureCase{"surface capabilities", FailureStage::Adapter,
                    &FakeVulkanState::getSurfaceCapabilitiesResult,
                    "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"},
        FailureCase{"device extension count", FailureStage::Adapter,
                    &FakeVulkanState::enumerateDeviceExtensionsResult,
                    "vkEnumerateDeviceExtensionProperties.count"},
        FailureCase{"device extension read", FailureStage::Adapter,
                    &FakeVulkanState::enumerateDeviceExtensionsReadResult,
                    "vkEnumerateDeviceExtensionProperties.read"},
        FailureCase{"device", FailureStage::Device, &FakeVulkanState::createDeviceResult,
                    "vkCreateDevice"},
        FailureCase{"allocator", FailureStage::Allocator, &FakeVulkanState::createAllocatorResult,
                    "vmaCreateAllocator"},
        FailureCase{"swapchain", FailureStage::Swapchain, &FakeVulkanState::createSwapchainResult,
                    "vkCreateSwapchainKHR"},
        FailureCase{"swapchain images", FailureStage::SwapchainImages,
                    &FakeVulkanState::getSwapchainImagesResult, "vkGetSwapchainImagesKHR.count"},
        FailureCase{"swapchain image read", FailureStage::SwapchainImages,
                    &FakeVulkanState::getSwapchainImagesReadResult, "vkGetSwapchainImagesKHR.read"},
        FailureCase{"image view", FailureStage::ImageView, &FakeVulkanState::createImageViewResult,
                    "vkCreateImageView"},
        FailureCase{"render pass", FailureStage::RenderPass,
                    &FakeVulkanState::createRenderPassResult, "vkCreateRenderPass"},
        FailureCase{"framebuffer", FailureStage::Framebuffer,
                    &FakeVulkanState::createFramebufferResult, "vkCreateFramebuffer"},
        FailureCase{"command pool", FailureStage::CommandPool,
                    &FakeVulkanState::createCommandPoolResult, "vkCreateCommandPool"},
        FailureCase{"command buffers", FailureStage::CommandBuffers,
                    &FakeVulkanState::allocateCommandBuffersResult, "vkAllocateCommandBuffers"},
        FailureCase{"semaphore", FailureStage::Semaphore, &FakeVulkanState::createSemaphoreResult,
                    "vkCreateSemaphore.imageAvailable"},
        FailureCase{"fence", FailureStage::Fence, &FakeVulkanState::createFenceResult,
                    "vkCreateFence.imageAcquired"}};

    for (const FailureCase& failureCase : kCases)
    {
        SCOPED_TRACE(std::string{failureCase.name});
        FakeVulkanState state;
        state.physicalDevices.push_back(MakeCompatibleFakeDevice(
            0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Creation Rollback Matrix GPU"));
        if (failureCase.resultField == &FakeVulkanState::enumerateLayersReadResult)
        {
            state.layers.emplace_back("VK_LAYER_FAKE_read_path");
        }
        if (failureCase.stage == FailureStage::DebugMessenger)
        {
            AddValidationSupport(state);
        }
        state.*(failureCase.resultField) = VK_ERROR_OUT_OF_HOST_MEMORY;
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);

        bool observedFailure{};
        const auto verifyFailure =
            [&](const auto& result, pond::render::detail::VulkanDiagnosticScope& diagnostics)
        {
            observedFailure = true;
            ExpectRenderError(result, failureCase.renderCode);
            const pond::render::OptionalBackendDiagnostic nativeFailure =
                diagnostics.TakeLastFailure();
            ASSERT_TRUE(nativeFailure.has_value());
            EXPECT_EQ(nativeFailure->nativeCode,
                      static_cast<std::int64_t>(VK_ERROR_OUT_OF_HOST_MEMORY));
            EXPECT_EQ(nativeFailure->symbolicName, "VK_ERROR_OUT_OF_HOST_MEMORY");
            EXPECT_EQ(nativeFailure->operation, failureCase.operation);
        };

        const auto exercise = [&]
        {
            pond::render::detail::VulkanDiagnosticScope diagnostics;
            auto instanceResult = pond::render::detail::CreateVulkanInstanceForWsi(
                dispatch, pond::render::detail::VulkanWsiKind::Win32,
                failureCase.stage == FailureStage::DebugMessenger
                    ? pond::render::RenderValidationMode::Standard
                    : pond::render::RenderValidationMode::Disabled);
            if (failureCase.stage == FailureStage::Loader ||
                failureCase.stage == FailureStage::InstanceEnumeration ||
                failureCase.stage == FailureStage::Instance ||
                failureCase.stage == FailureStage::DebugMessenger)
            {
                verifyFailure(instanceResult, diagnostics);
                return;
            }
            ASSERT_TRUE(instanceResult) << instanceResult.GetError().GetMessage();
            auto instance = std::make_shared<pond::render::detail::VulkanInstanceOwner>(
                std::move(instanceResult).GetValue());

            auto surfaceResult = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
                dispatch, instance,
                MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::Win32));
            if (failureCase.stage == FailureStage::Surface)
            {
                verifyFailure(surfaceResult, diagnostics);
                return;
            }
            ASSERT_TRUE(surfaceResult) << surfaceResult.GetError().GetMessage();
            pond::render::detail::VulkanSurfaceOwner surface = std::move(surfaceResult).GetValue();

            auto selection = pond::render::detail::SelectVulkanAdapterForSurface(
                dispatch, instance, surface.GetHandle(),
                pond::render::RenderAdapterSelectionDesc{});
            if (failureCase.stage == FailureStage::Adapter)
            {
                verifyFailure(selection, diagnostics);
                return;
            }
            ASSERT_TRUE(selection) << selection.GetError().GetMessage();
            auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
                dispatch, instance, surface.GetHandle(), selection.GetValue(),
                pond::render::RenderDeviceDesc{});
            if (failureCase.stage == FailureStage::Device ||
                failureCase.stage == FailureStage::Allocator)
            {
                verifyFailure(deviceResult, diagnostics);
                return;
            }
            ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
            pond::render::detail::VulkanDeviceOwner device = std::move(deviceResult).GetValue();

            auto swapchainResult = pond::render::detail::CreateVulkanSwapchainForTarget(
                dispatch, device, surface.GetHandle(), device.GetInfo().queuePlan,
                MakeTargetDesc());
            if (failureCase.stage >= FailureStage::Swapchain &&
                failureCase.stage <= FailureStage::Framebuffer)
            {
                verifyFailure(swapchainResult, diagnostics);
                return;
            }
            ASSERT_TRUE(swapchainResult) << swapchainResult.GetError().GetMessage();
            pond::render::detail::VulkanSwapchainOwner swapchain =
                std::move(swapchainResult).GetValue();

            auto frameResources = pond::render::detail::CreateVulkanFrameResourcesForTarget(
                dispatch, device, pond::platform::WindowId{42}, device.GetInfo().queuePlan,
                swapchain.GetConfig().presentation.actualQueuedLatency,
                swapchain.GetFramebufferCount());
            verifyFailure(frameResources, diagnostics);
        };

        exercise();
        EXPECT_TRUE(observedFailure);
        ExpectNoLiveResources(state);
    }
}

TEST(RenderVulkanBootstrapTests,
     PublicFrameRollbackMatrixRecoversOrShutsDownAfterEveryFallibleOperation)
{
    enum class FailureStage : std::uint8_t
    {
        Wait,
        ResetAcquireFence,
        ResetCommandBuffer,
        BeginCommandBuffer,
        Acquire,
        EndCommandBuffer,
        ResetFence,
        Submit,
        Present
    };
    struct FailureCase final
    {
        std::string_view name;
        FailureStage stage;
        VkResult FakeVulkanState::* resultField;
        std::string_view operation;
        bool expectsRecoverableNoSwapchain{};
    };

    constexpr std::array kCases{
        FailureCase{"wait", FailureStage::Wait, &FakeVulkanState::waitForFencesResult,
                    "vkWaitForFences.frame"},
        FailureCase{"reset acquire fence", FailureStage::ResetAcquireFence,
                    &FakeVulkanState::resetFencesResult, "vkResetFences.acquire"},
        FailureCase{"reset command buffer", FailureStage::ResetCommandBuffer,
                    &FakeVulkanState::resetCommandBufferResult, "vkResetCommandBuffer", true},
        FailureCase{"begin command buffer", FailureStage::BeginCommandBuffer,
                    &FakeVulkanState::beginCommandBufferResult, "vkBeginCommandBuffer", true},
        FailureCase{"acquire", FailureStage::Acquire, &FakeVulkanState::acquireNextImageResult,
                    "vkAcquireNextImageKHR"},
        FailureCase{"end command buffer", FailureStage::EndCommandBuffer,
                    &FakeVulkanState::endCommandBufferResult, "vkEndCommandBuffer", true},
        FailureCase{"reset fence", FailureStage::ResetFence, &FakeVulkanState::resetFencesResult,
                    "vkResetFences.frame", true},
        FailureCase{"submit", FailureStage::Submit, &FakeVulkanState::queueSubmitResult,
                    "vkQueueSubmit", true},
        FailureCase{"present", FailureStage::Present, &FakeVulkanState::queuePresentResult,
                    "vkQueuePresentKHR", true}};

    for (const FailureCase& failureCase : kCases)
    {
        SCOPED_TRACE(std::string{failureCase.name});
        FakeVulkanState state;
        state.physicalDevices.push_back(MakeCompatibleFakeDevice(
            0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Frame Rollback Matrix GPU"));
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
        std::unique_ptr<FakePublicLifecycle> fixture = CreateFakePublicLifecycle(dispatch);
        ASSERT_NE(fixture, nullptr);

        if (failureCase.stage == FailureStage::Wait ||
            failureCase.stage == FailureStage::ResetAcquireFence)
        {
            for (std::uint32_t frameIndex = 0U; frameIndex < 2U; ++frameIndex)
            {
                auto warmup = fixture->owners.target.AcquireFrame();
                ASSERT_TRUE(warmup) << warmup.GetError().GetMessage();
                pond::render::RenderFrame warmupFrame = std::move(warmup).GetValue();
                ASSERT_TRUE(warmupFrame.Clear());
                ASSERT_TRUE(warmupFrame.FinishAndPresent());
            }
        }

        const FakeLiveResourceCounts committedResources = GetLiveResourceCounts(state);
        state.*(failureCase.resultField) = VK_ERROR_OUT_OF_HOST_MEMORY;
        const bool failsDuringAcquire = failureCase.stage == FailureStage::Wait ||
                                        failureCase.stage == FailureStage::ResetAcquireFence ||
                                        failureCase.stage == FailureStage::ResetCommandBuffer ||
                                        failureCase.stage == FailureStage::BeginCommandBuffer ||
                                        failureCase.stage == FailureStage::Acquire;

        if (failsDuringAcquire)
        {
            ExpectRenderError(fixture->owners.target.AcquireFrame(),
                              pond::render::RenderErrorCode::OutOfMemory);
        }
        else
        {
            auto acquired = fixture->owners.target.AcquireFrame();
            ASSERT_TRUE(acquired) << acquired.GetError().GetMessage();
            pond::render::RenderFrame frame = std::move(acquired).GetValue();
            ASSERT_TRUE(frame.Clear());
            ExpectRenderError(frame.FinishAndPresent(), pond::render::RenderErrorCode::OutOfMemory);
        }

        const pond::render::RenderTargetDiagnostics targetDiagnostics =
            fixture->owners.target.GetDiagnostics();
        ASSERT_TRUE(targetDiagnostics.lastFailure.has_value());
        EXPECT_EQ(targetDiagnostics.lastFailure->renderCode,
                  pond::render::RenderErrorCode::OutOfMemory);
        EXPECT_EQ(targetDiagnostics.lastFailure->nativeCode,
                  static_cast<std::int64_t>(VK_ERROR_OUT_OF_HOST_MEMORY));
        EXPECT_EQ(targetDiagnostics.lastFailure->symbolicName, "VK_ERROR_OUT_OF_HOST_MEMORY");
        EXPECT_EQ(targetDiagnostics.lastFailure->operation, failureCase.operation);
        EXPECT_TRUE(fixture->owners.target.IsValid());
        EXPECT_FALSE(fixture->owners.target.IsDeviceLost());

        const FakeLiveResourceCounts resourcesAfterFailure = GetLiveResourceCounts(state);
        EXPECT_EQ(resourcesAfterFailure, committedResources);
        EXPECT_EQ(fixture->owners.target.HasSwapchain(),
                  !failureCase.expectsRecoverableNoSwapchain);
        EXPECT_EQ(fixture->owners.target.HasPendingRecreation(),
                  failureCase.expectsRecoverableNoSwapchain);
        EXPECT_EQ(fixture->owners.target.GetStatus(), pond::render::TargetStatus::Active);
        if (failureCase.expectsRecoverableNoSwapchain)
        {
            EXPECT_EQ(fixture->owners.target.GetSwapchainGeneration(), 0U);
            ASSERT_TRUE(fixture->owners.target.GetPendingRecreationInfo().has_value());
            EXPECT_EQ(fixture->owners.target.GetPendingRecreationInfo()->reason,
                      pond::render::TargetRecreationReason::PresentationChanged);
        }
        else
        {
            EXPECT_EQ(fixture->owners.target.GetSwapchainGeneration(), 1U);
            EXPECT_FALSE(fixture->owners.target.GetPendingRecreationInfo().has_value());
        }

        state.*(failureCase.resultField) = VK_SUCCESS;
        auto retry = fixture->owners.target.AcquireFrame();
        ASSERT_TRUE(retry) << retry.GetError().GetMessage();
        pond::render::RenderFrame retryFrame = std::move(retry).GetValue();
        ASSERT_TRUE(retryFrame.Clear());
        const auto presented = retryFrame.FinishAndPresent();
        ASSERT_TRUE(presented) << presented.GetError().GetMessage();
        EXPECT_TRUE(presented.GetValue().presented);

        ExpectPublicLifecycleShutdown(*fixture);
        fixture.reset();
        ExpectNoLiveResources(state);
    }
}

TEST(RenderVulkanBootstrapTests, DeviceAndQueueIdleFailureMatrixPreservesOwnersAndStructuredContext)
{
    enum class WaitStage : std::uint8_t
    {
        Device,
        PresentationQueue
    };
    struct WaitFailureCase final
    {
        std::string_view name;
        WaitStage stage;
        VkResult FakeVulkanState::* resultField;
        std::string_view operation;
    };
    constexpr std::array kCases{
        WaitFailureCase{"device idle", WaitStage::Device, &FakeVulkanState::deviceWaitIdleResult,
                        "vkDeviceWaitIdle"},
        WaitFailureCase{"presentation queue idle", WaitStage::PresentationQueue,
                        &FakeVulkanState::queueWaitIdleResult, "vkQueueWaitIdle"}};

    for (const WaitFailureCase& failureCase : kCases)
    {
        SCOPED_TRACE(std::string{failureCase.name});
        FakeVulkanState state;
        state.physicalDevices.push_back(MakeCompatibleFakeDevice(
            0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Idle Failure Matrix GPU"));
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
        std::unique_ptr<FakePublicLifecycle> fixture = CreateFakePublicLifecycle(dispatch);
        ASSERT_NE(fixture, nullptr);
        const FakeLiveResourceCounts committedResources = GetLiveResourceCounts(state);

        state.*(failureCase.resultField) = VK_ERROR_OUT_OF_HOST_MEMORY;
        pond::render::OptionalBackendDiagnostic nativeFailure;
        if (failureCase.stage == WaitStage::Device)
        {
            ExpectRenderError(fixture->owners.device.WaitIdle(),
                              pond::render::RenderErrorCode::OutOfMemory);
            nativeFailure = fixture->owners.device.GetDiagnostics().lastFailure;
        }
        else
        {
            pond::render::detail::VulkanDiagnosticScope diagnostics;
            ExpectRenderError(
                pond::render::detail::RenderBackendTestAccess::WaitPresentationQueueIdle(
                    fixture->owners.device),
                pond::render::RenderErrorCode::OutOfMemory);
            nativeFailure = diagnostics.TakeLastFailure();
        }

        ASSERT_TRUE(nativeFailure.has_value());
        EXPECT_EQ(nativeFailure->nativeCode,
                  static_cast<std::int64_t>(VK_ERROR_OUT_OF_HOST_MEMORY));
        EXPECT_EQ(nativeFailure->symbolicName, "VK_ERROR_OUT_OF_HOST_MEMORY");
        EXPECT_EQ(nativeFailure->operation, failureCase.operation);
        EXPECT_EQ(GetLiveResourceCounts(state), committedResources);
        EXPECT_TRUE(fixture->owners.device.IsValid());
        EXPECT_FALSE(fixture->owners.device.IsDeviceLost());
        EXPECT_TRUE(fixture->owners.target.HasSwapchain());
        EXPECT_FALSE(fixture->owners.target.HasPendingRecreation());
        EXPECT_EQ(fixture->owners.target.GetSwapchainGeneration(), 1U);

        state.*(failureCase.resultField) = VK_SUCCESS;
        if (failureCase.stage == WaitStage::Device)
        {
            ASSERT_TRUE(fixture->owners.device.WaitIdle());
        }
        else
        {
            ASSERT_TRUE(pond::render::detail::RenderBackendTestAccess::WaitPresentationQueueIdle(
                fixture->owners.device));
        }
        EXPECT_EQ(GetLiveResourceCounts(state), committedResources);

        ExpectPublicLifecycleShutdown(*fixture);
        fixture.reset();
        ExpectNoLiveResources(state);
    }
}

void ExpectPublicLifecycleShutdown(FakePublicLifecycle& fixture)
{
    ExpectRenderError(fixture.owners.bootstrap.Shutdown(),
                      pond::render::RenderErrorCode::InvalidState);
    fixture.additionalTarget.reset();
    fixture.owners.target = pond::render::RenderTarget{};
    EXPECT_EQ(fixture.owners.bootstrap.GetActiveTargetCount(), 0U);
    fixture.owners.device = pond::render::RenderDevice{};
    EXPECT_EQ(fixture.owners.bootstrap.GetActiveDeviceCount(), 0U);
    EXPECT_TRUE(fixture.owners.bootstrap.Shutdown());
    EXPECT_TRUE(fixture.owners.bootstrap.IsShutdown());
    fixture.instance.reset();
}

TEST(RenderVulkanBootstrapTests,
     PublicLifecycleInjectionCoversFrameMisuseAbandonmentAndReacquisition)
{
    enum class FrameCase : std::uint8_t
    {
        FinishWithoutClear,
        InvalidClear,
        Abandon,
        MoveAndAbandon
    };

    constexpr std::array kCases{FrameCase::FinishWithoutClear, FrameCase::InvalidClear,
                                FrameCase::Abandon, FrameCase::MoveAndAbandon};

    for (const FrameCase frameCase : kCases)
    {
        SCOPED_TRACE(static_cast<std::uint32_t>(frameCase));
        FakeVulkanState state;
        state.physicalDevices.push_back(MakeCompatibleFakeDevice(
            0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Lifecycle Injection GPU"));
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
        std::unique_ptr<FakePublicLifecycle> fixture = CreateFakePublicLifecycle(dispatch);
        ASSERT_NE(fixture, nullptr);
        EXPECT_EQ(fixture->owners.bootstrap.GetActiveDeviceCount(), 1U);
        EXPECT_EQ(fixture->owners.bootstrap.GetActiveTargetCount(), 1U);

        auto acquired = fixture->owners.target.AcquireFrame();
        ASSERT_TRUE(acquired) << acquired.GetError().GetMessage();
        pond::render::RenderFrame frame = std::move(acquired).GetValue();
        ASSERT_EQ(frame.GetStatus(), pond::render::FrameStatus::Ready);

        switch (frameCase)
        {
        case FrameCase::FinishWithoutClear:
            ExpectRenderError(frame.FinishAndPresent(),
                              pond::render::RenderErrorCode::InvalidState);
            ASSERT_TRUE(fixture->owners.target.GetDiagnostics().lastFailure.has_value());
            EXPECT_EQ(fixture->owners.target.GetDiagnostics().lastFailure->operation,
                      "FinishAndPresent");
            frame = pond::render::RenderFrame{};
            break;
        case FrameCase::InvalidClear:
            ExpectRenderError(frame.Clear(pond::render::ClearColor{
                                  .red = std::numeric_limits<float>::infinity()}),
                              pond::render::RenderErrorCode::InvalidArgument);
            ASSERT_TRUE(fixture->owners.target.GetDiagnostics().lastFailure.has_value());
            EXPECT_EQ(fixture->owners.target.GetDiagnostics().lastFailure->operation, "Clear");
            frame = pond::render::RenderFrame{};
            break;
        case FrameCase::Abandon:
            frame = pond::render::RenderFrame{};
            break;
        case FrameCase::MoveAndAbandon:
        {
            pond::render::RenderFrame movedFrame{std::move(frame)};
            EXPECT_FALSE(frame.IsValid());
            movedFrame = pond::render::RenderFrame{};
            break;
        }
        }

        EXPECT_FALSE(fixture->owners.target.GetDiagnostics().lastFailure.has_value() &&
                     fixture->owners.target.GetDiagnostics().lastFailure->renderCode ==
                         pond::render::RenderErrorCode::DeviceLost);
        auto retry = fixture->owners.target.AcquireFrame();
        ASSERT_TRUE(retry) << retry.GetError().GetMessage();
        pond::render::RenderFrame retryFrame = std::move(retry).GetValue();
        EXPECT_EQ(retryFrame.GetStatus(), pond::render::FrameStatus::Recreated);
        ASSERT_TRUE(retryFrame.Clear());
        const auto presented = retryFrame.FinishAndPresent();
        ASSERT_TRUE(presented) << presented.GetError().GetMessage();
        EXPECT_TRUE(presented.GetValue().presented);

        ExpectPublicLifecycleShutdown(*fixture);
        fixture.reset();
        ExpectNoLiveResources(state);
    }
}

TEST(RenderVulkanBootstrapTests,
     PublicLifecycleInjectionCoversSuspensionActiveFrameAndWrongThreadUse)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Lifecycle State GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicLifecycle> fixture = CreateFakePublicLifecycle(dispatch);
    ASSERT_NE(fixture, nullptr);

    const pond::render::RenderTargetSnapshot hidden{
        pond::platform::WindowId{42},
        pond::platform::PixelSize{800, 600},
        false,
        pond::platform::WindowState::Normal,
        pond::render::PresentationEnvironmentRevision{1U},
        2U};
    ASSERT_TRUE(fixture->owners.target.UpdateTargetSnapshot(hidden));
    auto skipped = fixture->owners.target.AcquireFrame();
    ASSERT_TRUE(skipped) << skipped.GetError().GetMessage();
    pond::render::RenderFrame skippedFrame = std::move(skipped).GetValue();
    EXPECT_TRUE(skippedFrame.IsSkipped());
    const auto skippedFinish = skippedFrame.FinishAndPresent();
    ASSERT_TRUE(skippedFinish) << skippedFinish.GetError().GetMessage();
    EXPECT_FALSE(skippedFinish.GetValue().presented);

    const pond::render::RenderTargetSnapshot restored{
        pond::platform::WindowId{42},
        pond::platform::PixelSize{800, 600},
        true,
        pond::platform::WindowState::Normal,
        pond::render::PresentationEnvironmentRevision{1U},
        3U};
    ASSERT_TRUE(fixture->owners.target.UpdateTargetSnapshot(restored));
    auto acquired = fixture->owners.target.AcquireFrame();
    ASSERT_TRUE(acquired) << acquired.GetError().GetMessage();
    pond::render::RenderFrame frame = std::move(acquired).GetValue();

    const pond::render::RenderTargetSnapshot activeUpdate{
        pond::platform::WindowId{42},
        pond::platform::PixelSize{801, 600},
        true,
        pond::platform::WindowState::Normal,
        pond::render::PresentationEnvironmentRevision{1U},
        4U};
    ExpectRenderError(fixture->owners.target.UpdateTargetSnapshot(activeUpdate),
                      pond::render::RenderErrorCode::InvalidState);
    EXPECT_EQ(fixture->owners.target.GetTargetSnapshot(), restored);

    auto recoverySurfaceOwner = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
        dispatch, fixture->instance,
        MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::Win32));
    ASSERT_TRUE(recoverySurfaceOwner) << recoverySurfaceOwner.GetError().GetMessage();
    auto recoverySurface = pond::render::detail::RenderBackendTestAccess::CreateRecoverySurface(
        fixture->owners.target, std::move(recoverySurfaceOwner).GetValue(), activeUpdate);
    ASSERT_TRUE(recoverySurface) << recoverySurface.GetError().GetMessage();
    pond::render::PreparedSurface preparedRecovery = std::move(recoverySurface).GetValue();
    ExpectRenderError(fixture->owners.target.RecoverSurface(std::move(preparedRecovery)),
                      pond::render::RenderErrorCode::InvalidState);
    EXPECT_TRUE(preparedRecovery.IsValid());
    preparedRecovery = pond::render::PreparedSurface{};

    std::atomic_bool targetWrongThreadRejected{};
    std::atomic_bool deviceWrongThreadRejected{};
    std::atomic_bool frameWrongThreadRejected{};
    std::jthread wrongThread{
        [&]
        {
            const auto targetResult = fixture->owners.target.AcquireFrame();
            targetWrongThreadRejected.store(
                !targetResult &&
                targetResult.GetError().GetCode() ==
                    pond::render::ToErrorCode(pond::render::RenderErrorCode::InvalidState));
            const auto deviceResult = fixture->owners.device.WaitIdle();
            deviceWrongThreadRejected.store(
                !deviceResult &&
                deviceResult.GetError().GetCode() ==
                    pond::render::ToErrorCode(pond::render::RenderErrorCode::InvalidState));
            const auto frameResult = frame.Clear();
            frameWrongThreadRejected.store(
                !frameResult &&
                frameResult.GetError().GetCode() ==
                    pond::render::ToErrorCode(pond::render::RenderErrorCode::InvalidState));
        }};
    wrongThread.join();
    EXPECT_TRUE(targetWrongThreadRejected.load());
    EXPECT_TRUE(deviceWrongThreadRejected.load());
    EXPECT_TRUE(frameWrongThreadRejected.load());

    ASSERT_TRUE(frame.Clear());
    const auto presented = frame.FinishAndPresent();
    ASSERT_TRUE(presented) << presented.GetError().GetMessage();
    EXPECT_TRUE(presented.GetValue().presented);
    ASSERT_TRUE(fixture->owners.target.UpdateTargetSnapshot(activeUpdate));

    ExpectPublicLifecycleShutdown(*fixture);
    fixture.reset();
    ExpectNoLiveResources(state);
}

TEST(RenderVulkanBootstrapTests, PublicLifecycleInjectionCoversSurfaceReplacementAndTargetIsolation)
{
    pond::render::RenderTargetDesc additionalDesc = MakeTargetDesc();
    additionalDesc.targetSnapshot =
        pond::render::RenderTargetSnapshot{pond::platform::WindowId{43},
                                           pond::platform::PixelSize{640, 480},
                                           true,
                                           pond::platform::WindowState::Normal,
                                           pond::render::PresentationEnvironmentRevision{1U},
                                           1U};

    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Multi Target GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicLifecycle> fixture =
        CreateFakePublicLifecycle(dispatch, additionalDesc);
    ASSERT_NE(fixture, nullptr);
    ASSERT_TRUE(fixture->additionalTarget.has_value());
    EXPECT_EQ(fixture->owners.bootstrap.GetActiveTargetCount(), 2U);
    EXPECT_EQ(fixture->owners.device.GetActiveTargetCount(), 2U);

    state.acquireNextImageResult = VK_ERROR_SURFACE_LOST_KHR;
    ExpectRenderError(fixture->owners.target.AcquireFrame(),
                      pond::render::RenderErrorCode::SurfaceLost);
    EXPECT_TRUE(fixture->owners.target.IsSurfaceLost());
    state.acquireNextImageResult = VK_SUCCESS;

    auto unaffectedFrameResult = fixture->additionalTarget->AcquireFrame();
    ASSERT_TRUE(unaffectedFrameResult) << unaffectedFrameResult.GetError().GetMessage();
    pond::render::RenderFrame unaffectedFrame = std::move(unaffectedFrameResult).GetValue();
    ASSERT_TRUE(unaffectedFrame.Clear());
    const auto unaffectedPresent = unaffectedFrame.FinishAndPresent();
    ASSERT_TRUE(unaffectedPresent) << unaffectedPresent.GetError().GetMessage();
    EXPECT_TRUE(unaffectedPresent.GetValue().presented);

    auto replacementOwner = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
        dispatch, fixture->instance,
        MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::Win32));
    ASSERT_TRUE(replacementOwner) << replacementOwner.GetError().GetMessage();
    auto replacement = pond::render::detail::RenderBackendTestAccess::CreateRecoverySurface(
        fixture->owners.target, std::move(replacementOwner).GetValue(),
        fixture->owners.target.GetTargetSnapshot());
    ASSERT_TRUE(replacement) << replacement.GetError().GetMessage();
    ASSERT_TRUE(fixture->owners.target.RecoverSurface(std::move(replacement).GetValue()));

    auto recoveredFrameResult = fixture->owners.target.AcquireFrame();
    ASSERT_TRUE(recoveredFrameResult) << recoveredFrameResult.GetError().GetMessage();
    pond::render::RenderFrame recoveredFrame = std::move(recoveredFrameResult).GetValue();
    EXPECT_EQ(recoveredFrame.GetStatus(), pond::render::FrameStatus::Recreated);
    ASSERT_TRUE(recoveredFrame.Clear());
    const auto recoveredPresent = recoveredFrame.FinishAndPresent();
    ASSERT_TRUE(recoveredPresent) << recoveredPresent.GetError().GetMessage();
    EXPECT_TRUE(recoveredPresent.GetValue().presented);

    ExpectPublicLifecycleShutdown(*fixture);
    fixture.reset();
    ExpectNoLiveResources(state);
}

TEST(RenderVulkanBootstrapTests, PublicLifecycleInjectionMakesDeviceLossStickyAcrossTargets)
{
    pond::render::RenderTargetDesc additionalDesc = MakeTargetDesc();
    additionalDesc.targetSnapshot =
        pond::render::RenderTargetSnapshot{pond::platform::WindowId{43},
                                           pond::platform::PixelSize{640, 480},
                                           true,
                                           pond::platform::WindowState::Normal,
                                           pond::render::PresentationEnvironmentRevision{1U},
                                           1U};

    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Sticky Loss GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicLifecycle> fixture =
        CreateFakePublicLifecycle(dispatch, additionalDesc);
    ASSERT_NE(fixture, nullptr);
    ASSERT_TRUE(fixture->additionalTarget.has_value());

    state.acquireNextImageResult = VK_ERROR_DEVICE_LOST;
    ExpectRenderError(fixture->owners.target.AcquireFrame(),
                      pond::render::RenderErrorCode::DeviceLost);
    ASSERT_TRUE(fixture->owners.device.IsDeviceLost());
    ASSERT_TRUE(fixture->owners.target.IsDeviceLost());
    ASSERT_TRUE(fixture->additionalTarget->IsDeviceLost());

    const std::uint32_t acquireCalls = state.acquireNextImageCalls;
    state.acquireNextImageResult = VK_SUCCESS;
    ExpectRenderError(fixture->owners.target.AcquireFrame(),
                      pond::render::RenderErrorCode::DeviceLost);
    ExpectRenderError(fixture->additionalTarget->AcquireFrame(),
                      pond::render::RenderErrorCode::DeviceLost);
    ExpectRenderError(fixture->owners.device.WaitIdle(), pond::render::RenderErrorCode::DeviceLost);
    EXPECT_EQ(state.acquireNextImageCalls, acquireCalls);
    EXPECT_EQ(state.deviceWaitIdleCalls, 0U);

    ExpectPublicLifecycleShutdown(*fixture);
    fixture.reset();
    ExpectNoLiveResources(state);
}

TEST(RenderVulkanBootstrapTests, OverlappingMovedInstancesRetainTheirOwnerLocalTables)
{
    FakeVulkanState state;
    pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    dispatch.loadInstanceTable = &FakeLoadOwnerLocalInstanceTable;

    auto firstResult = pond::render::detail::CreateVulkanInstanceForWsi(
        dispatch, pond::render::detail::VulkanWsiKind::Win32,
        pond::render::RenderValidationMode::Disabled);
    ASSERT_TRUE(firstResult) << firstResult.GetError().GetMessage();
    auto secondResult = pond::render::detail::CreateVulkanInstanceForWsi(
        dispatch, pond::render::detail::VulkanWsiKind::Win32,
        pond::render::RenderValidationMode::Disabled);
    ASSERT_TRUE(secondResult) << secondResult.GetError().GetMessage();

    pond::render::detail::VulkanInstanceOwner first = std::move(firstResult).GetValue();
    pond::render::detail::VulkanInstanceOwner second = std::move(secondResult).GetValue();
    EXPECT_EQ(state.loadInstanceTableCalls, 2U);
    EXPECT_EQ(state.loadInstanceCalls, 0U);
    EXPECT_EQ(first.GetDispatch().destroyInstance, &FakeDestroyOwnerLocalInstanceA);
    EXPECT_EQ(second.GetDispatch().destroyInstance, &FakeDestroyOwnerLocalInstanceB);

    pond::render::detail::VulkanInstanceOwner movedFirst{std::move(first)};
    EXPECT_FALSE(first.IsValid());
    second.Reset();
    movedFirst.Reset();

    EXPECT_EQ(state.ownerLocalInstanceADestroyCalls, 1U);
    EXPECT_EQ(state.ownerLocalInstanceBDestroyCalls, 1U);
    EXPECT_EQ(state.destroyInstanceCalls, 2U);
}

TEST(RenderVulkanBootstrapTests, OverlappingMovedDevicesRetainTheirOwnerLocalTables)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0xD15A7CU, 71U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Owner Local Dispatch GPU"));

    pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    dispatch.getInstanceProcAddr = &FakeGetInstanceProcAddr;
    dispatch.loadInstanceTable = &FakeLoadOwnerLocalInstanceTable;
    dispatch.loadDeviceTable = &FakeLoadOwnerLocalDeviceTable;
    dispatch.createOwnerLocalAllocator = &FakeCreateOwnerLocalAllocator;

    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0xD15A7CU);
    auto selection = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selection) << selection.GetError().GetMessage();

    auto firstResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selection.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(firstResult) << firstResult.GetError().GetMessage();
    auto secondResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selection.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(secondResult) << secondResult.GetError().GetMessage();

    pond::render::detail::VulkanDeviceOwner first = std::move(firstResult).GetValue();
    pond::render::detail::VulkanDeviceOwner second = std::move(secondResult).GetValue();
    EXPECT_EQ(state.loadDeviceTableCalls, 2U);
    EXPECT_EQ(state.loadDeviceCalls, 0U);
    EXPECT_EQ(state.createOwnerLocalAllocatorCalls, 2U);
    EXPECT_EQ(state.createAllocatorCalls, 0U);
    EXPECT_EQ(state.lastAllocatorGetInstanceProcAddr, &FakeGetInstanceProcAddr);
    EXPECT_EQ(state.lastAllocatorGetDeviceProcAddr, &FakeGetDeviceProcAddr);
    EXPECT_EQ(first.GetDispatch().destroyDevice, &FakeDestroyOwnerLocalDeviceA);
    EXPECT_EQ(second.GetDispatch().destroyDevice, &FakeDestroyOwnerLocalDeviceB);

    pond::render::detail::VulkanDeviceOwner movedFirst{std::move(first)};
    EXPECT_FALSE(first.IsValid());
    second.Reset();
    movedFirst.Reset();

    EXPECT_EQ(state.ownerLocalDeviceADestroyCalls, 1U);
    EXPECT_EQ(state.ownerLocalDeviceBDestroyCalls, 1U);
    EXPECT_EQ(state.destroyDeviceCalls, 2U);
    EXPECT_EQ(state.destroyAllocatorCalls, 2U);
}

namespace
{
enum class PublicRetirementExtensionPath : std::uint8_t
{
    SwapchainMaintenanceFence,
    PresentIdAndWait
};

struct PublicRetirementExtensionCase final
{
    std::string_view name;
    PublicRetirementExtensionPath path{PublicRetirementExtensionPath::SwapchainMaintenanceFence};
};

[[nodiscard]] pond::render::RenderTargetDesc MakeTargetDescForWindow(
    pond::platform::WindowId windowId, pond::platform::PixelSize pixelSize,
    pond::platform::WindowState windowState, std::uint64_t revision)
{
    return pond::render::RenderTargetDesc{
        .targetSnapshot =
            pond::render::RenderTargetSnapshot{windowId, pixelSize, true, windowState,
                                               pond::render::PresentationEnvironmentRevision{1U},
                                               revision},
        .presentation = {.policy = pond::render::PresentationPolicy::VSync,
                         .strength = pond::render::RequirementStrength::Preferred},
        .queuedLatency = {.maximumQueuedFrames = {},
                          .strength = pond::render::RequirementStrength::Preferred}};
}

void AddRetirementExtensionDevice(FakeVulkanState& state, PublicRetirementExtensionPath path)
{
    FakePhysicalDevice device =
        MakeCompatibleFakeDevice(0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
                                 path == PublicRetirementExtensionPath::SwapchainMaintenanceFence
                                     ? "Public Maintenance Retirement GPU"
                                     : "Public Present Wait Retirement GPU");
    if (path == PublicRetirementExtensionPath::SwapchainMaintenanceFence)
    {
        AddSurfaceMaintenanceSupport(state);
        device.deviceExtensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
        device.swapchainMaintenance1Feature = true;
    }
    else
    {
        device.deviceExtensions.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
        device.deviceExtensions.push_back(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
        device.presentIdFeature = true;
        device.presentWaitFeature = true;
    }
    state.physicalDevices.push_back(std::move(device));
}

void PresentPublicFrame(pond::render::RenderTarget& target)
{
    auto frameResult = target.AcquireFrame();
    ASSERT_TRUE(frameResult) << frameResult.GetError().GetMessage();
    pond::render::RenderFrame frame = std::move(frameResult).GetValue();
    ASSERT_TRUE(frame.GetStatus() == pond::render::FrameStatus::Ready ||
                frame.GetStatus() == pond::render::FrameStatus::Recreated ||
                frame.GetStatus() == pond::render::FrameStatus::Suboptimal);
    ASSERT_TRUE(frame.Clear());
    auto present = frame.FinishAndPresent();
    ASSERT_TRUE(present) << present.GetError().GetMessage();
    ASSERT_TRUE(present.GetValue().presented);
}

void PresentFramesUntilRetirementCompletes(pond::render::RenderTarget& target,
                                           std::uint64_t minimumRetiredResourceSets)
{
    constexpr std::uint32_t kMaximumProofFrames = 8U;
    for (std::uint32_t index = 0U;
         index < kMaximumProofFrames &&
         (target.GetPresentationRetirementStats().pendingResourceSets > 0U ||
          target.GetPresentationRetirementStats().retiredResourceSets < minimumRetiredResourceSets);
         ++index)
    {
        ASSERT_NO_FATAL_FAILURE(PresentPublicFrame(target));
    }
}
} // namespace

TEST(RenderVulkanBootstrapTests,
     PublicResizeRetirementUsesEachExtensionCompletionPathWithoutGlobalIdle)
{
    constexpr std::array extensionCases{
        PublicRetirementExtensionCase{"swapchain maintenance present fence",
                                      PublicRetirementExtensionPath::SwapchainMaintenanceFence},
        PublicRetirementExtensionCase{"present ID and wait",
                                      PublicRetirementExtensionPath::PresentIdAndWait},
    };

    for (const PublicRetirementExtensionCase& extensionCase : extensionCases)
    {
        SCOPED_TRACE(std::string{extensionCase.name});
        FakeVulkanState state;
        AddRetirementExtensionDevice(state, extensionCase.path);
        const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
        std::unique_ptr<FakePublicLifecycle> lifecycle = CreateFakePublicLifecycle(dispatch);
        ASSERT_NE(lifecycle, nullptr);

        ASSERT_NO_FATAL_FAILURE(PresentPublicFrame(lifecycle->owners.target));
        ASSERT_TRUE(
            lifecycle->owners.target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
                pond::platform::WindowId{42}, pond::platform::PixelSize{801, 601}, true,
                pond::platform::WindowState::Normal,
                pond::render::PresentationEnvironmentRevision{1U}, 2U}));

        auto replacementResult = lifecycle->owners.target.AcquireFrame();
        ASSERT_TRUE(replacementResult) << replacementResult.GetError().GetMessage();
        pond::render::RenderFrame replacement = std::move(replacementResult).GetValue();
        EXPECT_EQ(replacement.GetStatus(), pond::render::FrameStatus::Recreated);
        ASSERT_TRUE(replacement.Clear());
        auto replacementPresent = replacement.FinishAndPresent();
        ASSERT_TRUE(replacementPresent) << replacementPresent.GetError().GetMessage();
        ASSERT_TRUE(replacementPresent.GetValue().presented);

        const pond::render::PresentationRetirementStats retirement =
            lifecycle->owners.target.GetPresentationRetirementStats();
        EXPECT_EQ(retirement.pendingResourceSets, 0U);
        EXPECT_EQ(retirement.retiredResourceSets, 1U);
        EXPECT_EQ(retirement.practicalWaitFallbacks, 0U);
        EXPECT_TRUE(retirement.usedExplicitPresentationCompletion);
        EXPECT_FALSE(retirement.usedCoreAcquireHistory);
        EXPECT_EQ(lifecycle->owners.device.GetDiagnostics().practicalWaitFallbacks, 0U);
        EXPECT_EQ(state.deviceWaitIdleCalls, 0U);
        EXPECT_EQ(state.queueWaitIdleCalls, 0U);

        if (extensionCase.path == PublicRetirementExtensionPath::SwapchainMaintenanceFence)
        {
            ASSERT_FALSE(state.presentedFences.empty());
            EXPECT_NE(state.presentedFences.front(), VK_NULL_HANDLE);
            EXPECT_EQ(state.waitForPresentCalls, 0U);
        }
        else
        {
            ASSERT_FALSE(state.presentedIds.empty());
            EXPECT_NE(state.presentedIds.front(), 0U);
            EXPECT_GT(state.waitForPresentCalls, 0U);
        }

        lifecycle.reset();
        EXPECT_EQ(state.deviceWaitIdleCalls, 0U);
        EXPECT_EQ(state.queueWaitIdleCalls, 0U);
        EXPECT_EQ(state.destroyDeviceCalls, 1U);
    }
}

TEST(RenderVulkanBootstrapTests,
     PublicVulkan12RetirementUsesAcquireHistoryUntilFinalShutdownFallback)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Core Retirement GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    std::unique_ptr<FakePublicLifecycle> lifecycle = CreateFakePublicLifecycle(dispatch);
    ASSERT_NE(lifecycle, nullptr);

    ASSERT_NO_FATAL_FAILURE(PresentPublicFrame(lifecycle->owners.target));
    ASSERT_TRUE(lifecycle->owners.target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
        pond::platform::WindowId{42}, pond::platform::PixelSize{801, 601}, true,
        pond::platform::WindowState::Normal, pond::render::PresentationEnvironmentRevision{1U},
        2U}));
    ASSERT_NO_FATAL_FAILURE(PresentPublicFrame(lifecycle->owners.target));
    ASSERT_NO_FATAL_FAILURE(PresentFramesUntilRetirementCompletes(lifecycle->owners.target, 1U));

    pond::render::PresentationRetirementStats retirement =
        lifecycle->owners.target.GetPresentationRetirementStats();
    EXPECT_EQ(retirement.pendingResourceSets, 0U);
    EXPECT_EQ(retirement.retiredResourceSets, 1U);
    EXPECT_EQ(retirement.practicalWaitFallbacks, 0U);
    EXPECT_FALSE(retirement.usedExplicitPresentationCompletion);
    EXPECT_TRUE(retirement.usedCoreAcquireHistory);
    EXPECT_EQ(state.deviceWaitIdleCalls, 0U);
    EXPECT_EQ(state.queueWaitIdleCalls, 0U);

    ASSERT_TRUE(lifecycle->owners.target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
        pond::platform::WindowId{42}, pond::platform::PixelSize{}, true,
        pond::platform::WindowState::Minimized, pond::render::PresentationEnvironmentRevision{1U},
        3U}));
    EXPECT_TRUE(lifecycle->owners.target.IsSuspended());
    EXPECT_FALSE(lifecycle->owners.target.HasSwapchain());
    EXPECT_GT(lifecycle->owners.target.GetPresentationRetirementStats().pendingResourceSets, 0U);
    EXPECT_EQ(state.deviceWaitIdleCalls, 0U);
    EXPECT_EQ(state.queueWaitIdleCalls, 0U);

    ASSERT_TRUE(lifecycle->owners.target.UpdateTargetSnapshot(pond::render::RenderTargetSnapshot{
        pond::platform::WindowId{42}, pond::platform::PixelSize{801, 601}, true,
        pond::platform::WindowState::Normal, pond::render::PresentationEnvironmentRevision{1U},
        4U}));
    ASSERT_NO_FATAL_FAILURE(PresentPublicFrame(lifecycle->owners.target));
    ASSERT_NO_FATAL_FAILURE(PresentFramesUntilRetirementCompletes(lifecycle->owners.target, 2U));

    retirement = lifecycle->owners.target.GetPresentationRetirementStats();
    EXPECT_EQ(retirement.pendingResourceSets, 0U);
    EXPECT_EQ(retirement.retiredResourceSets, 2U);
    EXPECT_EQ(retirement.practicalWaitFallbacks, 0U);
    EXPECT_FALSE(retirement.usedExplicitPresentationCompletion);
    EXPECT_TRUE(retirement.usedCoreAcquireHistory);
    EXPECT_EQ(lifecycle->owners.device.GetDiagnostics().practicalWaitFallbacks, 0U);
    EXPECT_EQ(state.deviceWaitIdleCalls, 0U);
    EXPECT_EQ(state.queueWaitIdleCalls, 0U);

    lifecycle->owners.target = pond::render::RenderTarget{};
    EXPECT_EQ(lifecycle->owners.device.GetActiveTargetCount(), 0U);
    EXPECT_EQ(lifecycle->owners.device.GetDiagnostics().practicalWaitFallbacks, 0U);
    EXPECT_EQ(state.deviceWaitIdleCalls, 0U);
    ASSERT_TRUE(pond::render::detail::RenderBackendTestAccess::DrainOrphanedPresentationResources(
        lifecycle->owners.device));
    EXPECT_EQ(lifecycle->owners.device.GetDiagnostics().practicalWaitFallbacks, 1U);
    EXPECT_EQ(state.deviceWaitIdleCalls, 1U);
    EXPECT_EQ(state.queueWaitIdleCalls, 0U);

    lifecycle.reset();
    EXPECT_EQ(state.deviceWaitIdleCalls, 1U);
    EXPECT_EQ(state.destroyDeviceCalls, 1U);
}

TEST(RenderVulkanBootstrapTests,
     PublicSurfaceLossStallsOnlyTheSharedPresentationQueueAndKeepsOtherTargetUsable)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Public Shared Queue Recovery GPU"));
    const pond::render::detail::VulkanGlobalDispatch dispatch = MakeFakeDispatch(state);
    const pond::render::RenderTargetDesc secondDesc =
        MakeTargetDescForWindow(pond::platform::WindowId{84}, pond::platform::PixelSize{640, 480},
                                pond::platform::WindowState::Normal, 1U);
    std::unique_ptr<FakePublicLifecycle> lifecycle =
        CreateFakePublicLifecycle(dispatch, secondDesc);
    ASSERT_NE(lifecycle, nullptr);
    ASSERT_TRUE(lifecycle->additionalTarget.has_value());

    pond::render::RenderTarget& firstTarget = lifecycle->owners.target;
    pond::render::RenderTarget& secondTarget = *lifecycle->additionalTarget;
    ASSERT_NO_FATAL_FAILURE(PresentPublicFrame(firstTarget));
    ASSERT_NO_FATAL_FAILURE(PresentPublicFrame(secondTarget));
    ASSERT_EQ(state.presentationQueues.size(), 2U);
    ASSERT_NE(state.presentationQueues[0], VK_NULL_HANDLE);
    EXPECT_EQ(state.presentationQueues[0], state.presentationQueues[1]);

    state.acquireNextImageResult = VK_ERROR_SURFACE_LOST_KHR;
    ExpectRenderError(firstTarget.AcquireFrame(), pond::render::RenderErrorCode::SurfaceLost);
    EXPECT_TRUE(firstTarget.IsSurfaceLost());
    EXPECT_FALSE(firstTarget.HasSwapchain());
    EXPECT_EQ(firstTarget.GetPresentationRetirementStats().practicalWaitFallbacks, 1U);
    EXPECT_EQ(lifecycle->owners.device.GetDiagnostics().practicalWaitFallbacks, 1U);
    EXPECT_EQ(state.queueWaitIdleCalls, 1U);
    EXPECT_EQ(state.lastQueueWaitIdle, state.presentationQueues[0]);
    EXPECT_EQ(state.deviceWaitIdleCalls, 0U);

    EXPECT_EQ(secondTarget.GetStatus(), pond::render::TargetStatus::Active);
    EXPECT_TRUE(secondTarget.HasSwapchain());
    EXPECT_EQ(secondTarget.GetPresentationRetirementStats().practicalWaitFallbacks, 0U);
    const std::uint64_t secondFramesBeforeRecovery = secondTarget.GetDiagnostics().framesPresented;
    state.acquireNextImageResult = VK_SUCCESS;

    auto replacementSurface = pond::render::detail::CreateVulkanSurfaceForNativeWindow(
        dispatch, lifecycle->instance,
        MakeNativeWindowHandle(pond::render::detail::VulkanWsiKind::Win32));
    ASSERT_TRUE(replacementSurface) << replacementSurface.GetError().GetMessage();
    auto preparedResult = pond::render::detail::RenderBackendTestAccess::CreateRecoverySurface(
        firstTarget, std::move(replacementSurface).GetValue(), firstTarget.GetTargetSnapshot());
    ASSERT_TRUE(preparedResult) << preparedResult.GetError().GetMessage();
    pond::render::PreparedSurface replacement = std::move(preparedResult).GetValue();

    state.physicalDevices[0].surfaceFormats = {VkSurfaceFormatKHR{
        .format = VK_FORMAT_B8G8R8A8_UNORM, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    ExpectRenderError(firstTarget.RecoverSurface(std::move(replacement)),
                      pond::render::RenderErrorCode::UnsupportedSurface);
    EXPECT_TRUE(replacement.IsValid());
    EXPECT_TRUE(firstTarget.IsSurfaceLost());
    EXPECT_EQ(state.queueWaitIdleCalls, 1U);
    EXPECT_EQ(state.deviceWaitIdleCalls, 0U);

    ASSERT_NO_FATAL_FAILURE(PresentPublicFrame(secondTarget));
    EXPECT_EQ(secondTarget.GetDiagnostics().framesPresented, secondFramesBeforeRecovery + 1U);
    EXPECT_EQ(secondTarget.GetStatus(), pond::render::TargetStatus::Active);
    EXPECT_TRUE(secondTarget.HasSwapchain());

    state.physicalDevices[0].surfaceFormats = {VkSurfaceFormatKHR{
        .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    ASSERT_TRUE(firstTarget.RecoverSurface(std::move(replacement)));
    EXPECT_FALSE(replacement.IsValid());
    EXPECT_FALSE(firstTarget.IsSurfaceLost());
    EXPECT_TRUE(firstTarget.HasPendingRecreation());
    ASSERT_NO_FATAL_FAILURE(PresentPublicFrame(firstTarget));
    ASSERT_NO_FATAL_FAILURE(PresentPublicFrame(secondTarget));
    EXPECT_EQ(state.queueWaitIdleCalls, 1U);
    EXPECT_EQ(state.deviceWaitIdleCalls, 0U);
    EXPECT_EQ(lifecycle->owners.device.GetDiagnostics().practicalWaitFallbacks, 1U);
    EXPECT_EQ(secondTarget.GetPresentationRetirementStats().practicalWaitFallbacks, 0U);

    lifecycle.reset();
    EXPECT_EQ(state.queueWaitIdleCalls, 1U);
    EXPECT_EQ(state.deviceWaitIdleCalls, 1U);
    EXPECT_EQ(state.destroyDeviceCalls, 1U);
}
