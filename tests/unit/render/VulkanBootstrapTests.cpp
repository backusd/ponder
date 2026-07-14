#include "VulkanBootstrap.hpp"

#include <ponder/core/Log.hpp>
#include <ponder/platform/NativeWindow.hpp>
#include <ponder/render/RenderError.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{
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

struct FakeVulkanState final{
    VkResult initializeResult{VK_SUCCESS};
    VkResult enumerateVersionResult{VK_SUCCESS};
    VkResult enumerateExtensionsResult{VK_SUCCESS};
    VkResult enumerateLayersResult{VK_SUCCESS};
    VkResult createInstanceResult{VK_SUCCESS};
    VkResult createDebugMessengerResult{VK_SUCCESS};
    VkResult createWin32SurfaceResult{VK_SUCCESS};
    VkResult createX11SurfaceResult{VK_SUCCESS};
    VkResult createWaylandSurfaceResult{VK_SUCCESS};
    VkResult enumeratePhysicalDevicesResult{VK_SUCCESS};
    VkResult getSurfaceSupportResult{VK_SUCCESS};
    VkResult getSurfaceFormatsResult{VK_SUCCESS};
    VkResult getPresentModesResult{VK_SUCCESS};
    VkResult getSurfaceCapabilitiesResult{VK_SUCCESS};
    VkResult enumerateDeviceExtensionsResult{VK_SUCCESS};
    VkResult createDeviceResult{VK_SUCCESS};
    VkResult deviceWaitIdleResult{VK_SUCCESS};
    VkResult createSwapchainResult{VK_SUCCESS};
    VkResult getSwapchainImagesResult{VK_SUCCESS};
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
    VkResult setDebugNameResult{VK_SUCCESS};
    VkResult createAllocatorResult{VK_SUCCESS};
    std::uint32_t loaderVersion{VK_API_VERSION_1_2};
    std::vector<std::string> extensions{
        "VK_KHR_surface",
        pond::render::detail::GetVulkanWsiExtensionName(pond::render::detail::VulkanWsiKind::Win32),
        pond::render::detail::GetVulkanWsiExtensionName(pond::render::detail::VulkanWsiKind::X11),
        pond::render::detail::GetVulkanWsiExtensionName(pond::render::detail::VulkanWsiKind::Wayland)};
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
    std::uint32_t getDeviceQueueCalls{};
    std::uint32_t deviceWaitIdleCalls{};
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
    std::uint32_t setDebugNameCalls{};
    std::uint32_t beginLabelCalls{};
    std::uint32_t endLabelCalls{};
    std::uint32_t createAllocatorCalls{};
    std::uint32_t destroyAllocatorCalls{};
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
    std::uint32_t acquiredImageIndex{};
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

void CopyStringToFixedArray(std::string_view source, char* destination, std::size_t capacity) noexcept
{
    if (capacity == 0U)
    {
        return;
    }

    const std::size_t copyCount = std::min(source.size(), capacity - 1U);
    std::copy_n(source.data(), copyCount, destination);
    destination[copyCount] = '\0';
}

[[nodiscard]] FakePhysicalDevice MakeCompatibleFakeDevice(
    std::uintptr_t handleValue, std::uint8_t uuidSeed, VkPhysicalDeviceType type,
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
    device.surfaceCapabilities.currentExtent = VkExtent2D{
        .width = std::numeric_limits<std::uint32_t>::max(),
        .height = std::numeric_limits<std::uint32_t>::max()};
    device.surfaceCapabilities.minImageExtent = VkExtent2D{.width = 1U, .height = 1U};
    device.surfaceCapabilities.maxImageExtent = VkExtent2D{.width = 4096U, .height = 4096U};
    device.surfaceCapabilities.supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    device.surfaceCapabilities.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    device.surfaceCapabilities.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    device.surfaceCapabilities.supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    device.surfaceFormats.push_back(VkSurfaceFormatKHR{.format = VK_FORMAT_B8G8R8A8_UNORM,
                                                       .colorSpace =
                                                           VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
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

[[nodiscard]] VkResult FakeEnumerateInstanceExtensions(
    const char* layerName, std::uint32_t* propertyCount, VkExtensionProperties* properties) noexcept
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

    const VkValidationFeatureEnableEXT* const begin =
        validationFeatures.pEnabledValidationFeatures;
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
            const auto* validationFeatures = reinterpret_cast<const VkValidationFeaturesEXT*>(current);
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

    *messenger = MakeFakeHandle<VkDebugUtilsMessengerEXT>(
        g_fakeVulkanState->nextDebugMessengerValue);
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

[[nodiscard]] VkResult FakeEnumeratePhysicalDevices(
    VkInstance instance, std::uint32_t* physicalDeviceCount,
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
        *physicalDeviceCount = static_cast<std::uint32_t>(g_fakeVulkanState->physicalDevices.size());
        return VK_SUCCESS;
    }

    const std::uint32_t writableCount = std::min(
        *physicalDeviceCount, static_cast<std::uint32_t>(g_fakeVulkanState->physicalDevices.size()));
    for (std::uint32_t index = 0; index < writableCount; ++index)
    {
        physicalDevices[index] = MakeFakeHandle<VkPhysicalDevice>(
            g_fakeVulkanState->physicalDevices[index].handleValue);
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
            std::copy(device->deviceUuid.begin(), device->deviceUuid.end(), idProperties->deviceUUID);
            std::copy(device->deviceLuid.begin(), device->deviceLuid.end(), idProperties->deviceLUID);
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
            auto* presentIdFeatures = reinterpret_cast<VkPhysicalDevicePresentIdFeaturesKHR*>(current);
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

    memoryProperties->memoryHeapCount = std::min(
        static_cast<std::uint32_t>(device->memoryHeaps.size()),
        static_cast<std::uint32_t>(VK_MAX_MEMORY_HEAPS));
    for (std::uint32_t index = 0; index < memoryProperties->memoryHeapCount; ++index)
    {
        memoryProperties->memoryHeaps[index] = device->memoryHeaps[index];
    }
}

void FakeGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice physicalDevice, std::uint32_t* propertyCount,
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

[[nodiscard]] VkResult FakeGetPhysicalDeviceSurfaceSupport(
    VkPhysicalDevice physicalDevice, std::uint32_t queueFamilyIndex, VkSurfaceKHR surface,
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

    const std::uint32_t writableCount =
        std::min(*surfaceFormatCount, static_cast<std::uint32_t>(device->surfaceFormats.size()));
    for (std::uint32_t index = 0; index < writableCount; ++index)
    {
        surfaceFormats[index] = device->surfaceFormats[index];
    }
    *surfaceFormatCount = writableCount;
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

    const std::uint32_t writableCount =
        std::min(*presentModeCount, static_cast<std::uint32_t>(device->presentModes.size()));
    for (std::uint32_t index = 0; index < writableCount; ++index)
    {
        presentModes[index] = device->presentModes[index];
    }
    *presentModeCount = writableCount;
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

void FakeGetDeviceQueue(VkDevice device, std::uint32_t queueFamilyIndex,
                        std::uint32_t queueIndex, VkQueue* queue) noexcept
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
                                              std::uint32_t* imageCount,
                                              VkImage* images) noexcept
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

    if (images == nullptr)
    {
        *imageCount = g_fakeVulkanState->swapchainImageCount;
        return VK_SUCCESS;
    }

    const std::uint32_t writableCount = std::min(*imageCount,
                                                 g_fakeVulkanState->swapchainImageCount);
    for (std::uint32_t index = 0; index < writableCount; ++index)
    {
        images[index] = MakeFakeHandle<VkImage>(g_fakeVulkanState->nextImageValue + index);
    }
    *imageCount = writableCount;
    g_fakeVulkanState->nextImageValue += writableCount;
    return writableCount == g_fakeVulkanState->swapchainImageCount ? VK_SUCCESS : VK_INCOMPLETE;
}

[[nodiscard]] VkResult FakeCreateImageView(VkDevice device,
                                           const VkImageViewCreateInfo* createInfo,
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

[[nodiscard]] VkResult FakeAllocateCommandBuffers(
    VkDevice device, const VkCommandBufferAllocateInfo* allocateInfo,
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
        commandBuffers[index] = MakeFakeHandle<VkCommandBuffer>(
            g_fakeVulkanState->nextCommandBufferValue++);
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

[[nodiscard]] VkResult FakeBeginCommandBuffer(
    VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* beginInfo) noexcept
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
    ++g_fakeVulkanState->cmdBeginRenderPassCalls;
}

void FakeCmdEndRenderPass(VkCommandBuffer commandBuffer) noexcept
{
    (void)commandBuffer;
    ++g_fakeVulkanState->cmdEndRenderPassCalls;
}

[[nodiscard]] VkResult FakeCreateSemaphore(VkDevice device,
                                           const VkSemaphoreCreateInfo* createInfo,
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
    (void)fenceCount;
    (void)fences;
    (void)waitAll;
    (void)timeout;
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
    return g_fakeVulkanState->resetFencesResult;
}

[[nodiscard]] VkResult FakeAcquireNextImage(VkDevice device, VkSwapchainKHR swapchain,
                                            std::uint64_t timeout, VkSemaphore semaphore,
                                            VkFence fence, std::uint32_t* imageIndex) noexcept
{
    (void)device;
    (void)swapchain;
    (void)timeout;
    (void)semaphore;
    (void)fence;
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
    (void)submitCount;
    (void)submits;
    (void)fence;
    ++g_fakeVulkanState->queueSubmitCalls;
    return g_fakeVulkanState->queueSubmitResult;
}

[[nodiscard]] VkResult FakeQueuePresent(VkQueue queue, const VkPresentInfoKHR* presentInfo) noexcept
{
    (void)queue;
    (void)presentInfo;
    ++g_fakeVulkanState->queuePresentCalls;
    return g_fakeVulkanState->queuePresentResult;
}

[[nodiscard]] VkResult FakeSetDebugUtilsObjectName(
    VkDevice device, const VkDebugUtilsObjectNameInfoEXT* nameInfo) noexcept
{
    (void)device;
    (void)nameInfo;
    ++g_fakeVulkanState->setDebugNameCalls;
    return g_fakeVulkanState->setDebugNameResult;
}

void FakeCmdBeginDebugUtilsLabel(VkCommandBuffer commandBuffer,
                                 const VkDebugUtilsLabelEXT* labelInfo) noexcept
{
    (void)commandBuffer;
    (void)labelInfo;
    ++g_fakeVulkanState->beginLabelCalls;
}

void FakeCmdEndDebugUtilsLabel(VkCommandBuffer commandBuffer) noexcept
{
    (void)commandBuffer;
    ++g_fakeVulkanState->endLabelCalls;
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
    pond::render::PresentationPolicy policy = pond::render::PresentationPolicy::Default,
    pond::render::QueuedFrameLatency latency = {},
    pond::platform::PixelSize pixelSize = pond::platform::PixelSize{800, 600},
    bool visible = true, std::uint64_t revision = 1)
{
    return pond::render::RenderTargetDesc{
        .targetSnapshot = pond::render::RenderTargetSnapshot{pond::platform::WindowId{42},
                                                             pixelSize,
                                                             visible,
                                                             false,
                                                             true,
                                                             revision},
        .presentationPolicy = policy,
        .queuedLatency = latency};
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

    const pond::core::Result<pond::render::detail::VulkanInstanceOwner> result =
        pond::render::detail::CreateVulkanInstanceForWsi(
            dispatch, pond::render::detail::VulkanWsiKind::Win32,
            pond::render::RenderValidationMode::Default);

    ExpectRenderError(result, pond::render::RenderErrorCode::LoaderUnavailable);
    EXPECT_NE(result.GetError().GetMessage().find("Vulkan loader"), std::string_view::npos);
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
        EXPECT_TRUE(info.debugUtilityHooks.timingMarkers);
        EXPECT_TRUE(info.debugUtilityHooks.captureRegions);
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
        EXPECT_TRUE(ContainsString(state.lastEnabledExtensions,
                                   VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME));
        EXPECT_TRUE(ContainsValidationFeature(state.lastEnabledValidationFeatures,
                                              featureCase.feature));
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
    EXPECT_NE(g_capturedLogEntries[0].GetMessage().find("VALIDATION-UNIT"),
              std::string_view::npos);
    EXPECT_NE(g_capturedLogEntries[0].GetMessage().find("numericCode=123"),
              std::string_view::npos);
    EXPECT_NE(g_capturedLogEntries[0].GetMessage().find("unit-object"),
              std::string_view::npos);
    EXPECT_NE(g_capturedLogEntries[0].GetMessage().find("synthetic warning"),
              std::string_view::npos);
}

TEST(RenderVulkanBootstrapTests, ExactDebugMessageFilterSuppressesFailureAndLog)
{
    g_capturedLogEntries.clear();
    const pond::core::ScopedMinimumLogLevel minimumLogLevel{pond::core::LogLevel::Trace};
    const pond::core::ScopedLogSinkHandler logSink{&CaptureLogEntry};

    pond::render::detail::VulkanDebugMessengerContext context;
    context.exactMessageFilters.push_back(
        pond::render::detail::VulkanValidationMessageFilter{.messageIdName = "FILTERED-ID",
                                                            .messageIdNumber = 456});

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
            pond::render::detail::VulkanSurfaceOwner surface =
                std::move(surfaceResult).GetValue();
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
        const auto info = bootstrap.EnsureInitialized(dispatch,
                                                      pond::render::detail::VulkanWsiKind::Win32,
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
    std::shared_ptr<pond::render::detail::VulkanInstanceOwner> instance =
        CreateSharedFakeInstance(dispatch, pond::render::detail::VulkanWsiKind::Win32);
    ASSERT_NE(instance, nullptr);
    dispatch.destroySurface = nullptr;

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

TEST(RenderVulkanBootstrapTests, SelectsDiscreteAdapterForDefaultPreferenceAndRejectsImplicitSoftware)
{
    FakeVulkanState state;
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, "Integrated GPU"));
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5002U, 2U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Discrete GPU"));
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5003U, 3U, VK_PHYSICAL_DEVICE_TYPE_CPU, "Software Rasterizer"));
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
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5003U, 3U, VK_PHYSICAL_DEVICE_TYPE_CPU, "Software Rasterizer"));
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
    EXPECT_NE(selectionResult.GetValue().rejectedAdapters[0].reasons[0].find(
                  "excludes hardware"),
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
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5002U, 2U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Discrete B"));
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Discrete A"));
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
    FakePhysicalDevice rejected = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, "Broken GPU");
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
    EXPECT_NE(std::string{selectionResult.GetError().GetMessage()}.find("No Vulkan physical devices"),
              std::string::npos);
}
TEST(RenderVulkanBootstrapTests, CreatesLogicalDeviceWithEveryNonemptyQueueFamilyAndVma)
{
    FakeVulkanState state;
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Queue Plan GPU");
    device.queueFamilies = {VkQueueFamilyProperties{.queueFlags = VK_QUEUE_GRAPHICS_BIT,
                                                     .queueCount = 1U},
                            VkQueueFamilyProperties{.queueFlags = VK_QUEUE_TRANSFER_BIT,
                                                     .queueCount = 2U},
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
            dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
        ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
        pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();

        EXPECT_TRUE(owner.IsValid());
        const pond::render::detail::VulkanDeviceInfo info = owner.GetInfo();
        EXPECT_TRUE(pond::render::IsValid(info.queuePlan));
        EXPECT_EQ(info.queuePlan.graphicsQueueFamilyIndex, 0U);
        EXPECT_EQ(info.queuePlan.presentationQueueFamilyIndex, 2U);
        EXPECT_TRUE(info.queuePlan.usesDistinctPresentationQueue);
        const std::vector<std::uint32_t> expectedFamilies{0U, 1U, 2U};
        EXPECT_EQ(info.queuePlan.provisionedQueueFamilyIndices, expectedFamilies);
        EXPECT_EQ(state.lastDeviceQueueFamilyIndices, expectedFamilies);
        EXPECT_TRUE(info.optionalCapabilities.vmaAllocator);
        EXPECT_TRUE(ContainsString(state.lastDeviceEnabledExtensions,
                                   VK_KHR_SWAPCHAIN_EXTENSION_NAME));
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

    const VkSurfaceKHR surface = MakeFakeHandle<VkSurfaceKHR>(0x6000U);
    auto selectionResult = pond::render::detail::SelectVulkanAdapterForSurface(
        dispatch, instance, surface, pond::render::RenderAdapterSelectionDesc{});
    ASSERT_TRUE(selectionResult) << selectionResult.GetError().GetMessage();

    auto deviceResult = pond::render::detail::CreateVulkanDeviceForAdapterSelection(
        dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
    ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
    const pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();
    const pond::render::RenderDeviceOptionalCapabilities capabilities =
        owner.GetInfo().optionalCapabilities;

    EXPECT_TRUE(capabilities.swapchainMaintenance1);
    EXPECT_TRUE(capabilities.presentId);
    EXPECT_TRUE(capabilities.presentWait);
    EXPECT_TRUE(ContainsString(state.lastDeviceEnabledExtensions,
                               VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME));
    EXPECT_TRUE(ContainsString(state.lastDeviceEnabledExtensions,
                               VK_KHR_PRESENT_ID_EXTENSION_NAME));
    EXPECT_TRUE(ContainsString(state.lastDeviceEnabledExtensions,
                               VK_KHR_PRESENT_WAIT_EXTENSION_NAME));
    EXPECT_TRUE(state.swapchainMaintenance1FeaturesChained);
    EXPECT_TRUE(state.presentIdFeaturesChained);
    EXPECT_TRUE(state.presentWaitFeaturesChained);
    EXPECT_TRUE(state.lastEnabledSwapchainMaintenance1Feature);
    EXPECT_TRUE(state.lastEnabledPresentIdFeature);
    EXPECT_TRUE(state.lastEnabledPresentWaitFeature);
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
    EXPECT_TRUE(ContainsString(state.lastDeviceEnabledExtensions,
                               VK_KHR_SWAPCHAIN_EXTENSION_NAME));
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
    device.queueFamilies = {VkQueueFamilyProperties{.queueFlags = VK_QUEUE_GRAPHICS_BIT,
                                                     .queueCount = 1U},
                            VkQueueFamilyProperties{.queueFlags = VK_QUEUE_TRANSFER_BIT,
                                                     .queueCount = 1U},
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
    device.surfaceCapabilities.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
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
        MakeTargetDesc(pond::render::PresentationPolicy::PreferMailbox,
                       pond::render::QueuedFrameLatency{3},
                       pond::platform::PixelSize{2000, 64}));

    ASSERT_TRUE(configResult) << configResult.GetError().GetMessage();
    const pond::render::detail::VulkanSwapchainConfig config = configResult.GetValue();
    EXPECT_EQ(config.format, VK_FORMAT_B8G8R8A8_SRGB);
    EXPECT_EQ(config.extent.width, 1024U);
    EXPECT_EQ(config.extent.height, 128U);
    EXPECT_EQ(config.presentMode, VK_PRESENT_MODE_FIFO_KHR);
    EXPECT_EQ(config.compositeAlpha, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR);
    EXPECT_EQ(config.preTransform, VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR);
    EXPECT_EQ(config.imageCount, 4U);
    EXPECT_EQ(config.sharingMode, VK_SHARING_MODE_EXCLUSIVE);
    EXPECT_EQ(config.presentation.selectedMode, pond::render::SelectedPresentMode::Fifo);
    EXPECT_EQ(config.presentation.compositeAlpha,
              pond::render::SelectedCompositeAlpha::PreMultiplied);
    EXPECT_TRUE(config.presentation.optionalPreferenceUnavailable);
    EXPECT_FALSE(config.presentation.queuedLatencyLimitedBySurface);
    EXPECT_EQ(config.presentation.queuedLatency.frameCount, 3U);
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
        MakeTargetDesc(pond::render::PresentationPolicy::Fifo,
                       pond::render::QueuedFrameLatency{2}));
    ASSERT_TRUE(configResult) << configResult.GetError().GetMessage();
    EXPECT_EQ(configResult.GetValue().imageCount, 5U);
    EXPECT_EQ(configResult.GetValue().presentation.queuedLatency.frameCount, 2U);
    EXPECT_FALSE(configResult.GetValue().presentation.queuedLatencyLimitedBySurface);

    state.physicalDevices[0].surfaceCapabilities.minImageCount = 1U;
    state.physicalDevices[0].surfaceCapabilities.maxImageCount = 2U;
    configResult = pond::render::detail::SelectVulkanSwapchainConfig(
        dispatch, owner, surface, owner.GetInfo().queuePlan,
        MakeTargetDesc(pond::render::PresentationPolicy::Fifo,
                       pond::render::QueuedFrameLatency{3}));
    ASSERT_TRUE(configResult) << configResult.GetError().GetMessage();
    EXPECT_EQ(configResult.GetValue().imageCount, 2U);
    EXPECT_EQ(configResult.GetValue().presentation.queuedLatency.frameCount, 2U);
    EXPECT_TRUE(configResult.GetValue().presentation.queuedLatencyLimitedBySurface);
}

TEST(RenderVulkanBootstrapTests, SwapchainConfigUsesConcurrentSharingForDistinctQueues)
{
    FakeVulkanState state;
    FakePhysicalDevice device = MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Distinct Queue Swapchain GPU");
    device.queueFamilies = {VkQueueFamilyProperties{.queueFlags = VK_QUEUE_GRAPHICS_BIT,
                                                     .queueCount = 1U},
                            VkQueueFamilyProperties{.queueFlags = VK_QUEUE_TRANSFER_BIT,
                                                     .queueCount = 1U},
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

    ExpectRenderError(pond::render::detail::SelectVulkanSwapchainConfig(
                          dispatch, owner, surface, owner.GetInfo().queuePlan,
                          MakeTargetDesc(pond::render::PresentationPolicy::Default,
                                         pond::render::QueuedFrameLatency{},
                                         pond::platform::PixelSize{})),
                      pond::render::RenderErrorCode::InvalidState);
    ExpectRenderError(pond::render::detail::SelectVulkanSwapchainConfig(
                          dispatch, owner, surface, owner.GetInfo().queuePlan,
                          MakeTargetDesc(pond::render::PresentationPolicy::Default,
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
            dispatch, instance, surface, selectionResult.GetValue(), pond::render::RenderDeviceDesc{});
        ASSERT_TRUE(deviceResult) << deviceResult.GetError().GetMessage();
        pond::render::detail::VulkanDeviceOwner owner = std::move(deviceResult).GetValue();
        pond::render::detail::VulkanDeviceOwner movedOwner{std::move(owner)};

        EXPECT_FALSE(owner.IsValid());
        EXPECT_TRUE(movedOwner.IsValid());
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
    state.physicalDevices.push_back(MakeCompatibleFakeDevice(
        0x5001U, 1U, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Frame GPU"));
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
            dispatch, owner, owner.GetInfo().queuePlan,
            swapchain.GetConfig().presentation.queuedLatency);
        ASSERT_TRUE(frameResourcesResult) << frameResourcesResult.GetError().GetMessage();
        pond::render::detail::VulkanFrameResourcesOwner frameResources =
            std::move(frameResourcesResult).GetValue();
        ASSERT_TRUE(frameResources.IsValid());
        EXPECT_EQ(frameResources.GetSlotCount(), swapchain.GetConfig().presentation.queuedLatency.frameCount);

        const auto frameResult = pond::render::detail::ClearSubmitAndPresentVulkanFrame(
            dispatch, owner, swapchain, frameResources, owner.GetInfo().queuePlan, 0U,
            pond::render::ClearColor{.red = 0.1F, .green = 0.2F, .blue = 0.3F, .alpha = 1.0F});
        ASSERT_TRUE(frameResult) << frameResult.GetError().GetMessage();
        EXPECT_EQ(frameResult.GetValue().status, pond::render::FrameStatus::Presented);
        EXPECT_TRUE(frameResult.GetValue().presented);
        EXPECT_FALSE(frameResult.GetValue().suboptimal);

        EXPECT_EQ(state.waitForFencesCalls, 1U);
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
    EXPECT_EQ(state.destroySemaphoreCalls, swapchain.GetConfig().presentation.queuedLatency.frameCount * 2U);
    EXPECT_EQ(state.destroyFenceCalls, swapchain.GetConfig().presentation.queuedLatency.frameCount);
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
        dispatch, owner, owner.GetInfo().queuePlan, swapchain.GetConfig().presentation.queuedLatency);
    ASSERT_TRUE(frameResourcesResult) << frameResourcesResult.GetError().GetMessage();
    pond::render::detail::VulkanFrameResourcesOwner frameResources =
        std::move(frameResourcesResult).GetValue();

    state.acquireNextImageResult = VK_TIMEOUT;
    const auto timeoutResult = pond::render::detail::ClearSubmitAndPresentVulkanFrame(
        dispatch, owner, swapchain, frameResources, owner.GetInfo().queuePlan, 0U,
        pond::render::ClearColor{});
    ASSERT_TRUE(timeoutResult) << timeoutResult.GetError().GetMessage();
    EXPECT_EQ(timeoutResult.GetValue().status, pond::render::FrameStatus::TimedOut);
    EXPECT_FALSE(timeoutResult.GetValue().presented);
    EXPECT_EQ(state.queueSubmitCalls, 0U);
    EXPECT_EQ(state.queuePresentCalls, 0U);

    state.acquireNextImageResult = VK_SUCCESS;
    state.queueSubmitResult = VK_ERROR_DEVICE_LOST;
    ExpectRenderError(pond::render::detail::ClearSubmitAndPresentVulkanFrame(
                          dispatch, owner, swapchain, frameResources, owner.GetInfo().queuePlan, 0U,
                          pond::render::ClearColor{}),
                      pond::render::RenderErrorCode::DeviceLost);
    EXPECT_EQ(state.queueSubmitCalls, 1U);
    EXPECT_EQ(state.queuePresentCalls, 0U);
}