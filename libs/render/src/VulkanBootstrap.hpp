#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/NativeWindow.hpp>
#include <ponder/render/Bootstrap.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <volk.h>

#include "VulkanQueueSynchronization.hpp"

namespace pond::render::draw2d
{
class Draw2DPacket;
struct Draw2DDrawRecord;
} // namespace pond::render::draw2d

namespace pond::render::detail
{
enum class VulkanWsiKind : std::uint8_t
{
    Win32 = 0,
    X11 = 1,
    Wayland = 2
};

struct VulkanDebugUtilityHooks final
{
    bool objectNames{};
    bool commandLabels{};
    bool timingMarkers{};
    bool captureRegions{};

    [[nodiscard]] constexpr bool IsActive() const noexcept
    {
        return objectNames || commandLabels || timingMarkers || captureRegions;
    }

    [[nodiscard]] friend constexpr bool operator==(
        const VulkanDebugUtilityHooks& lhs, const VulkanDebugUtilityHooks& rhs) noexcept = default;
};

struct VulkanInstanceInfo final
{
    std::uint32_t apiVersion{};
    VulkanWsiKind wsiKind{VulkanWsiKind::Win32};
    RenderValidationMode requestedValidationMode{RenderValidationMode::Default};
    RenderValidationMode enabledValidationMode{RenderValidationMode::Disabled};
    bool validationEnabled{};
    bool debugUtilsEnabled{};
    bool debugMessengerEnabled{};
    bool validationFeaturesEnabled{};
    bool surfaceMaintenance1Enabled{};
    bool khrSurfaceMaintenance1Enabled{};
    bool extSurfaceMaintenance1Enabled{};
    VulkanDebugUtilityHooks debugUtilityHooks{};

    [[nodiscard]] friend constexpr bool operator==(
        const VulkanInstanceInfo& lhs, const VulkanInstanceInfo& rhs) noexcept = default;
};

struct VulkanSurfaceInfo final
{
    VulkanWsiKind wsiKind{VulkanWsiKind::Win32};

    [[nodiscard]] friend constexpr bool operator==(const VulkanSurfaceInfo& lhs,
                                                   const VulkanSurfaceInfo& rhs) noexcept = default;
};

struct VulkanQueueFamilySnapshot final
{
    std::uint32_t familyIndex{};
    std::uint32_t queueCount{};
    bool supportsGraphics{};
    bool supportsPresentation{};

    [[nodiscard]] friend constexpr bool operator==(const VulkanQueueFamilySnapshot& lhs,
                                                   const VulkanQueueFamilySnapshot& rhs) noexcept =
        default;
};

struct VulkanDeviceQueuePlan final
{
    std::uint32_t graphicsQueueFamilyIndex{};
    std::uint32_t presentationQueueFamilyIndex{};
    std::vector<std::uint32_t> provisionedQueueFamilyIndices{};
    bool usesDistinctPresentationQueue{};

    [[nodiscard]] friend bool operator==(const VulkanDeviceQueuePlan& lhs,
                                         const VulkanDeviceQueuePlan& rhs) = default;
};

struct VulkanDeviceOptionalCapabilities final
{
    bool swapchainMaintenance1{};
    bool presentId{};
    bool presentWait{};
    bool vmaAllocator{};

    [[nodiscard]] friend constexpr bool operator==(
        const VulkanDeviceOptionalCapabilities& lhs,
        const VulkanDeviceOptionalCapabilities& rhs) noexcept = default;
};

[[nodiscard]] constexpr bool IsValidVulkanQueueFamilySnapshot(
    VulkanQueueFamilySnapshot value) noexcept
{
    return value.queueCount > 0U;
}

[[nodiscard]] constexpr bool IsValidVulkanDeviceOptionalCapabilities(
    VulkanDeviceOptionalCapabilities) noexcept
{
    return true;
}

[[nodiscard]] inline bool IsValidVulkanDeviceQueuePlan(const VulkanDeviceQueuePlan& value) noexcept
{
    if (value.provisionedQueueFamilyIndices.empty())
    {
        return false;
    }

    bool hasGraphicsQueue{};
    bool hasPresentationQueue{};
    for (std::size_t index = 0; index < value.provisionedQueueFamilyIndices.size(); ++index)
    {
        const std::uint32_t familyIndex = value.provisionedQueueFamilyIndices[index];
        hasGraphicsQueue = hasGraphicsQueue || familyIndex == value.graphicsQueueFamilyIndex;
        hasPresentationQueue =
            hasPresentationQueue || familyIndex == value.presentationQueueFamilyIndex;

        for (std::size_t laterIndex = index + 1U;
             laterIndex < value.provisionedQueueFamilyIndices.size(); ++laterIndex)
        {
            if (familyIndex == value.provisionedQueueFamilyIndices[laterIndex])
            {
                return false;
            }
        }
    }

    return hasGraphicsQueue && hasPresentationQueue &&
           value.usesDistinctPresentationQueue ==
               (value.graphicsQueueFamilyIndex != value.presentationQueueFamilyIndex);
}

struct VulkanDeviceInfo final
{
    RenderAdapterSnapshot selectedAdapter{};
    VulkanDeviceQueuePlan queuePlan{};
    VulkanDeviceOptionalCapabilities optionalCapabilities{};
    VkDeviceSize nonCoherentAtomSize{1U};
    std::array<std::uint32_t, 2U> maximumViewportDimensions{};

    [[nodiscard]] friend bool operator==(const VulkanDeviceInfo& lhs,
                                         const VulkanDeviceInfo& rhs) = default;
};

struct VulkanHostBufferAllocationRequest final
{
    VkMemoryPropertyFlags requiredMemoryProperties{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT};
    VkMemoryPropertyFlags preferredMemoryProperties{VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                                    VK_MEMORY_PROPERTY_HOST_CACHED_BIT};
};

struct VulkanHostBufferAllocationResult final
{
    VkMemoryPropertyFlags memoryProperties{};
    VkDeviceSize allocationSize{};
};

struct VulkanValidationMessageFilter final
{
    std::string messageIdName{};
    std::int32_t messageIdNumber{};

    [[nodiscard]] bool Matches(std::string_view inputName, std::int32_t inputNumber) const noexcept;
};

struct VulkanDebugMessengerContext final
{
    struct CapturedMessage final
    {
        std::atomic_bool ready{false};
        RenderValidationMessage message{};
    };

    std::string currentOperation{"vulkan-instance-bootstrap"};
    bool failOnWarningOrError{true};
    std::vector<VulkanValidationMessageFilter> exactMessageFilters{};
    std::atomic_bool warningOrErrorSeen{false};
    std::atomic_uint64_t warningCount{};
    std::atomic_uint64_t errorCount{};
    std::atomic_uint64_t droppedMessageCount{};
    std::atomic_size_t reservedMessageCount{};
    std::array<CapturedMessage, RenderValidationReport::kMaximumCapturedMessages>
        capturedMessages{};
};

struct VulkanGlobalDispatch final
{
    using InitializeFn = VkResult (*)();
    using EnumerateInstanceVersionFn = VkResult (*)(std::uint32_t*);
    using EnumerateInstanceExtensionPropertiesFn = VkResult (*)(const char*, std::uint32_t*,
                                                                VkExtensionProperties*);
    using EnumerateInstanceLayerPropertiesFn = VkResult (*)(std::uint32_t*, VkLayerProperties*);
    using CreateInstanceFn = VkResult (*)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*,
                                          VkInstance*);
    using DestroyInstanceFn = void (*)(VkInstance, const VkAllocationCallbacks*);
    using LoadInstanceOnlyFn = void (*)(VkInstance);
    using LoadInstanceTableFn = void (*)(VolkInstanceTable*, VkInstance);
    using CreateDebugUtilsMessengerFn = VkResult (*)(VkInstance,
                                                     const VkDebugUtilsMessengerCreateInfoEXT*,
                                                     const VkAllocationCallbacks*,
                                                     VkDebugUtilsMessengerEXT*);
    using DestroyDebugUtilsMessengerFn = void (*)(VkInstance, VkDebugUtilsMessengerEXT,
                                                  const VkAllocationCallbacks*);
    using CreateSurfaceFn = VkResult (*)(VkInstance, const void*, const VkAllocationCallbacks*,
                                         VkSurfaceKHR*);
    using DestroySurfaceFn = void (*)(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks*);
    using EnumeratePhysicalDevicesFn = VkResult (*)(VkInstance, std::uint32_t*, VkPhysicalDevice*);
    using GetPhysicalDeviceProperties2Fn = void (*)(VkPhysicalDevice, VkPhysicalDeviceProperties2*);
    using GetPhysicalDeviceFeatures2Fn = void (*)(VkPhysicalDevice, VkPhysicalDeviceFeatures2*);
    using GetPhysicalDeviceMemoryPropertiesFn = void (*)(VkPhysicalDevice,
                                                         VkPhysicalDeviceMemoryProperties*);
    using GetPhysicalDeviceQueueFamilyPropertiesFn = void (*)(VkPhysicalDevice, std::uint32_t*,
                                                              VkQueueFamilyProperties*);
    using GetPhysicalDeviceSurfaceSupportFn = VkResult (*)(VkPhysicalDevice, std::uint32_t,
                                                           VkSurfaceKHR, VkBool32*);
    using GetPhysicalDeviceSurfaceFormatsFn = VkResult (*)(VkPhysicalDevice, VkSurfaceKHR,
                                                           std::uint32_t*, VkSurfaceFormatKHR*);
    using GetPhysicalDeviceSurfacePresentModesFn = VkResult (*)(VkPhysicalDevice, VkSurfaceKHR,
                                                                std::uint32_t*, VkPresentModeKHR*);
    using GetPhysicalDeviceSurfaceCapabilitiesFn = VkResult (*)(VkPhysicalDevice, VkSurfaceKHR,
                                                                VkSurfaceCapabilitiesKHR*);
    using EnumerateDeviceExtensionPropertiesFn = VkResult (*)(VkPhysicalDevice, const char*,
                                                              std::uint32_t*,
                                                              VkExtensionProperties*);
    using CreateDeviceFn = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo*,
                                        const VkAllocationCallbacks*, VkDevice*);
    using DestroyDeviceFn = void (*)(VkDevice, const VkAllocationCallbacks*);
    using LoadDeviceFn = void (*)(VkDevice);
    using LoadDeviceTableFn = void (*)(VolkDeviceTable*, PFN_vkGetDeviceProcAddr, VkDevice);
    using GetDeviceQueueFn = void (*)(VkDevice, std::uint32_t, std::uint32_t, VkQueue*);
    using DeviceWaitIdleFn = VkResult (*)(VkDevice);
    using QueueWaitIdleFn = VkResult (*)(VkQueue);
    using CreateSwapchainFn = VkResult (*)(VkDevice, const VkSwapchainCreateInfoKHR*,
                                           const VkAllocationCallbacks*, VkSwapchainKHR*);
    using DestroySwapchainFn = void (*)(VkDevice, VkSwapchainKHR, const VkAllocationCallbacks*);
    using GetSwapchainImagesFn = VkResult (*)(VkDevice, VkSwapchainKHR, std::uint32_t*, VkImage*);
    using CreateImageViewFn = VkResult (*)(VkDevice, const VkImageViewCreateInfo*,
                                           const VkAllocationCallbacks*, VkImageView*);
    using DestroyImageViewFn = void (*)(VkDevice, VkImageView, const VkAllocationCallbacks*);
    using CreateRenderPassFn = VkResult (*)(VkDevice, const VkRenderPassCreateInfo*,
                                            const VkAllocationCallbacks*, VkRenderPass*);
    using DestroyRenderPassFn = void (*)(VkDevice, VkRenderPass, const VkAllocationCallbacks*);
    using CreateFramebufferFn = VkResult (*)(VkDevice, const VkFramebufferCreateInfo*,
                                             const VkAllocationCallbacks*, VkFramebuffer*);
    using DestroyFramebufferFn = void (*)(VkDevice, VkFramebuffer, const VkAllocationCallbacks*);
    using CreateShaderModuleFn = VkResult (*)(VkDevice, const VkShaderModuleCreateInfo*,
                                              const VkAllocationCallbacks*, VkShaderModule*);
    using DestroyShaderModuleFn = void (*)(VkDevice, VkShaderModule, const VkAllocationCallbacks*);
    using CreatePipelineLayoutFn = VkResult (*)(VkDevice, const VkPipelineLayoutCreateInfo*,
                                                const VkAllocationCallbacks*, VkPipelineLayout*);
    using DestroyPipelineLayoutFn = void (*)(VkDevice, VkPipelineLayout,
                                             const VkAllocationCallbacks*);
    using CreateGraphicsPipelinesFn = VkResult (*)(VkDevice, VkPipelineCache, std::uint32_t,
                                                   const VkGraphicsPipelineCreateInfo*,
                                                   const VkAllocationCallbacks*, VkPipeline*);
    using DestroyPipelineFn = void (*)(VkDevice, VkPipeline, const VkAllocationCallbacks*);
    using CreateBufferFn = VkResult (*)(void*, const VkBufferCreateInfo*,
                                        const VulkanHostBufferAllocationRequest*, VkBuffer*, void**,
                                        VulkanHostBufferAllocationResult*);
    using DestroyBufferFn = void (*)(void*, VkBuffer, void*);
    using MapMemoryFn = VkResult (*)(void*, void*, void**);
    using UnmapMemoryFn = void (*)(void*, void*);
    using FlushAllocationFn = VkResult (*)(void*, void*, VkDeviceSize, VkDeviceSize);
    using CmdBindPipelineFn = void (*)(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
    using CmdSetViewportFn = void (*)(VkCommandBuffer, std::uint32_t, std::uint32_t,
                                      const VkViewport*);
    using CmdSetScissorFn = void (*)(VkCommandBuffer, std::uint32_t, std::uint32_t,
                                     const VkRect2D*);
    using CmdBindVertexBuffersFn = void (*)(VkCommandBuffer, std::uint32_t, std::uint32_t,
                                            const VkBuffer*, const VkDeviceSize*);
    using CmdBindIndexBufferFn = void (*)(VkCommandBuffer, VkBuffer, VkDeviceSize, VkIndexType);
    using CmdPushConstantsFn = void (*)(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags,
                                        std::uint32_t, std::uint32_t, const void*);
    using CmdDrawIndexedFn = void (*)(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t,
                                      std::int32_t, std::uint32_t);
    using CreateCommandPoolFn = VkResult (*)(VkDevice, const VkCommandPoolCreateInfo*,
                                             const VkAllocationCallbacks*, VkCommandPool*);
    using DestroyCommandPoolFn = void (*)(VkDevice, VkCommandPool, const VkAllocationCallbacks*);
    using AllocateCommandBuffersFn = VkResult (*)(VkDevice, const VkCommandBufferAllocateInfo*,
                                                  VkCommandBuffer*);
    using ResetCommandBufferFn = VkResult (*)(VkCommandBuffer, VkCommandBufferResetFlags);
    using BeginCommandBufferFn = VkResult (*)(VkCommandBuffer, const VkCommandBufferBeginInfo*);
    using EndCommandBufferFn = VkResult (*)(VkCommandBuffer);
    using CmdBeginRenderPassFn = void (*)(VkCommandBuffer, const VkRenderPassBeginInfo*,
                                          VkSubpassContents);
    using CmdEndRenderPassFn = void (*)(VkCommandBuffer);
    using CreateSemaphoreFn = VkResult (*)(VkDevice, const VkSemaphoreCreateInfo*,
                                           const VkAllocationCallbacks*, VkSemaphore*);
    using DestroySemaphoreFn = void (*)(VkDevice, VkSemaphore, const VkAllocationCallbacks*);
    using CreateFenceFn = VkResult (*)(VkDevice, const VkFenceCreateInfo*,
                                       const VkAllocationCallbacks*, VkFence*);
    using DestroyFenceFn = void (*)(VkDevice, VkFence, const VkAllocationCallbacks*);
    using WaitForFencesFn = VkResult (*)(VkDevice, std::uint32_t, const VkFence*, VkBool32,
                                         std::uint64_t);
    using ResetFencesFn = VkResult (*)(VkDevice, std::uint32_t, const VkFence*);
    using AcquireNextImageFn = VkResult (*)(VkDevice, VkSwapchainKHR, std::uint64_t, VkSemaphore,
                                            VkFence, std::uint32_t*);
    using QueueSubmitFn = VkResult (*)(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence);
    using QueuePresentFn = VkResult (*)(VkQueue, const VkPresentInfoKHR*);
    using WaitForPresentFn = VkResult (*)(VkDevice, VkSwapchainKHR, std::uint64_t, std::uint64_t);
    using SetDebugUtilsObjectNameFn = VkResult (*)(VkDevice, const VkDebugUtilsObjectNameInfoEXT*);
    using CmdBeginDebugUtilsLabelFn = void (*)(VkCommandBuffer, const VkDebugUtilsLabelEXT*);
    using CmdEndDebugUtilsLabelFn = void (*)(VkCommandBuffer);
    using CreateAllocatorFn = VkResult (*)(VkInstance, VkPhysicalDevice, VkDevice, std::uint32_t,
                                           void**);
    using CreateOwnerLocalAllocatorFn = VkResult (*)(VkInstance, VkPhysicalDevice, VkDevice,
                                                     std::uint32_t, PFN_vkGetInstanceProcAddr,
                                                     PFN_vkGetDeviceProcAddr, void**);
    using DestroyAllocatorFn = void (*)(void*);

    InitializeFn initialize{};
    PFN_vkGetInstanceProcAddr getInstanceProcAddr{};
    EnumerateInstanceVersionFn enumerateInstanceVersion{};
    EnumerateInstanceExtensionPropertiesFn enumerateInstanceExtensionProperties{};
    EnumerateInstanceLayerPropertiesFn enumerateInstanceLayerProperties{};
    CreateInstanceFn createInstance{};
    DestroyInstanceFn destroyInstance{};
    LoadInstanceOnlyFn loadInstanceOnly{};
    LoadInstanceTableFn loadInstanceTable{};
    CreateDebugUtilsMessengerFn createDebugUtilsMessenger{};
    DestroyDebugUtilsMessengerFn destroyDebugUtilsMessenger{};
    CreateSurfaceFn createWin32Surface{};
    CreateSurfaceFn createX11Surface{};
    CreateSurfaceFn createWaylandSurface{};
    DestroySurfaceFn destroySurface{};
    EnumeratePhysicalDevicesFn enumeratePhysicalDevices{};
    GetPhysicalDeviceProperties2Fn getPhysicalDeviceProperties2{};
    GetPhysicalDeviceFeatures2Fn getPhysicalDeviceFeatures2{};
    GetPhysicalDeviceMemoryPropertiesFn getPhysicalDeviceMemoryProperties{};
    GetPhysicalDeviceQueueFamilyPropertiesFn getPhysicalDeviceQueueFamilyProperties{};
    GetPhysicalDeviceSurfaceSupportFn getPhysicalDeviceSurfaceSupport{};
    GetPhysicalDeviceSurfaceFormatsFn getPhysicalDeviceSurfaceFormats{};
    GetPhysicalDeviceSurfacePresentModesFn getPhysicalDeviceSurfacePresentModes{};
    GetPhysicalDeviceSurfaceCapabilitiesFn getPhysicalDeviceSurfaceCapabilities{};
    EnumerateDeviceExtensionPropertiesFn enumerateDeviceExtensionProperties{};
    CreateDeviceFn createDevice{};
    DestroyDeviceFn destroyDevice{};
    LoadDeviceFn loadDevice{};
    LoadDeviceTableFn loadDeviceTable{};
    GetDeviceQueueFn getDeviceQueue{};
    DeviceWaitIdleFn deviceWaitIdle{};
    QueueWaitIdleFn queueWaitIdle{};
    CreateSwapchainFn createSwapchain{};
    DestroySwapchainFn destroySwapchain{};
    GetSwapchainImagesFn getSwapchainImages{};
    CreateImageViewFn createImageView{};
    DestroyImageViewFn destroyImageView{};
    CreateRenderPassFn createRenderPass{};
    DestroyRenderPassFn destroyRenderPass{};
    CreateFramebufferFn createFramebuffer{};
    DestroyFramebufferFn destroyFramebuffer{};
    CreateShaderModuleFn createShaderModule{};
    DestroyShaderModuleFn destroyShaderModule{};
    CreatePipelineLayoutFn createPipelineLayout{};
    DestroyPipelineLayoutFn destroyPipelineLayout{};
    CreateGraphicsPipelinesFn createGraphicsPipelines{};
    DestroyPipelineFn destroyPipeline{};
    CreateBufferFn createBuffer{};
    DestroyBufferFn destroyBuffer{};
    MapMemoryFn mapMemory{};
    UnmapMemoryFn unmapMemory{};
    FlushAllocationFn flushAllocation{};
    CmdBindPipelineFn cmdBindPipeline{};
    CmdSetViewportFn cmdSetViewport{};
    CmdSetScissorFn cmdSetScissor{};
    CmdBindVertexBuffersFn cmdBindVertexBuffers{};
    CmdBindIndexBufferFn cmdBindIndexBuffer{};
    CmdPushConstantsFn cmdPushConstants{};
    CmdDrawIndexedFn cmdDrawIndexed{};
    CreateCommandPoolFn createCommandPool{};
    DestroyCommandPoolFn destroyCommandPool{};
    AllocateCommandBuffersFn allocateCommandBuffers{};
    ResetCommandBufferFn resetCommandBuffer{};
    BeginCommandBufferFn beginCommandBuffer{};
    EndCommandBufferFn endCommandBuffer{};
    CmdBeginRenderPassFn cmdBeginRenderPass{};
    CmdEndRenderPassFn cmdEndRenderPass{};
    CreateSemaphoreFn createSemaphore{};
    DestroySemaphoreFn destroySemaphore{};
    CreateFenceFn createFence{};
    DestroyFenceFn destroyFence{};
    WaitForFencesFn waitForFences{};
    ResetFencesFn resetFences{};
    AcquireNextImageFn acquireNextImage{};
    QueueSubmitFn queueSubmit{};
    QueuePresentFn queuePresent{};
    WaitForPresentFn waitForPresent{};
    SetDebugUtilsObjectNameFn setDebugUtilsObjectName{};
    CmdBeginDebugUtilsLabelFn cmdBeginDebugUtilsLabel{};
    CmdEndDebugUtilsLabelFn cmdEndDebugUtilsLabel{};
    CreateAllocatorFn createAllocator{};
    CreateOwnerLocalAllocatorFn createOwnerLocalAllocator{};
    DestroyAllocatorFn destroyAllocator{};
};

class VulkanInstanceOwner final
{
public:
    VulkanInstanceOwner() noexcept;
    VulkanInstanceOwner(const VulkanInstanceOwner&) = delete;
    VulkanInstanceOwner& operator=(const VulkanInstanceOwner&) = delete;
    VulkanInstanceOwner(VulkanInstanceOwner&& other) noexcept;
    VulkanInstanceOwner& operator=(VulkanInstanceOwner&& other) noexcept;
    ~VulkanInstanceOwner();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] VkInstance GetHandle() const noexcept;
    [[nodiscard]] VulkanInstanceInfo GetInfo() const noexcept;
    [[nodiscard]] RenderValidationReport GetValidationReport() const noexcept;
    [[nodiscard]] const VulkanGlobalDispatch& GetDispatch() const noexcept;
    [[nodiscard]] PFN_vkGetDeviceProcAddr GetDeviceProcAddr() const noexcept;

    void Reset() noexcept;

private:
    friend core::Result<VulkanInstanceOwner> CreateVulkanInstanceForWsi(
        const VulkanGlobalDispatch& dispatch, VulkanWsiKind wsiKind,
        RenderValidationMode validationMode);

    VulkanInstanceOwner(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                        VulkanGlobalDispatch::DestroyInstanceFn destroyInstance,
                        VulkanGlobalDispatch::DestroyDebugUtilsMessengerFn destroyDebugMessenger,
                        VulkanGlobalDispatch dispatch, std::optional<VolkInstanceTable> volkTable,
                        VulkanInstanceInfo info,
                        std::unique_ptr<VulkanDebugMessengerContext> debugContext) noexcept;

    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
    VulkanGlobalDispatch::DestroyInstanceFn m_destroyInstance{};
    VulkanGlobalDispatch::DestroyDebugUtilsMessengerFn m_destroyDebugMessenger{};
    VulkanGlobalDispatch m_dispatch{};
    std::optional<VolkInstanceTable> m_volkTable{};
    VulkanInstanceInfo m_info{};
    std::unique_ptr<VulkanDebugMessengerContext> m_debugContext;
};

class VulkanSurfaceOwner final
{
public:
    VulkanSurfaceOwner() noexcept;
    VulkanSurfaceOwner(const VulkanSurfaceOwner&) = delete;
    VulkanSurfaceOwner& operator=(const VulkanSurfaceOwner&) = delete;
    VulkanSurfaceOwner(VulkanSurfaceOwner&& other) noexcept;
    VulkanSurfaceOwner& operator=(VulkanSurfaceOwner&& other) noexcept;
    ~VulkanSurfaceOwner();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] VkSurfaceKHR GetHandle() const noexcept;
    [[nodiscard]] VulkanSurfaceInfo GetInfo() const noexcept;
    [[nodiscard]] std::shared_ptr<VulkanInstanceOwner> GetInstanceOwner() const noexcept;

    void Reset() noexcept;

private:
    friend core::Result<VulkanSurfaceOwner> CreateVulkanSurfaceForNativeWindow(
        const VulkanGlobalDispatch& dispatch, std::shared_ptr<VulkanInstanceOwner> instance,
        const platform::NativeWindowHandle& nativeWindowHandle);

    VulkanSurfaceOwner(std::shared_ptr<VulkanInstanceOwner> instance, VkSurfaceKHR surface,
                       VulkanGlobalDispatch::DestroySurfaceFn destroySurface,
                       VulkanSurfaceInfo info) noexcept;

    std::shared_ptr<VulkanInstanceOwner> m_instance;
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VulkanGlobalDispatch::DestroySurfaceFn m_destroySurface{};
    VulkanSurfaceInfo m_info{};
};

struct VulkanDeviceChildLifetime final
{
    std::atomic_bool active{true};
    std::atomic_uint32_t draw2DPipelineChildren{};
};

class VulkanDeviceOwner final
{
public:
    VulkanDeviceOwner() noexcept;
    VulkanDeviceOwner(const VulkanDeviceOwner&) = delete;
    VulkanDeviceOwner& operator=(const VulkanDeviceOwner&) = delete;
    VulkanDeviceOwner(VulkanDeviceOwner&& other) noexcept;
    VulkanDeviceOwner& operator=(VulkanDeviceOwner&& other) noexcept;
    ~VulkanDeviceOwner();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] VkDevice GetHandle() const noexcept;
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const noexcept;
    [[nodiscard]] void* GetAllocator() const noexcept;
    [[nodiscard]] VkDeviceSize GetNonCoherentAtomSize() const noexcept;
    [[nodiscard]] const std::shared_ptr<VulkanDeviceChildLifetime>& GetChildLifetime()
        const noexcept;
    [[nodiscard]] const VulkanDeviceInfo& GetInfo() const noexcept;
    [[nodiscard]] const VulkanGlobalDispatch& GetDispatch() const noexcept;
    [[nodiscard]] VkQueue GetQueue(std::uint32_t queueFamilyIndex) const noexcept;
    [[nodiscard]] VulkanQueueSynchronization::QueueOperationLock LockQueueOperation(
        VkQueue queue) const;
    [[nodiscard]] core::VoidResult WaitIdle() const;
    [[nodiscard]] core::VoidResult WaitQueueIdle(std::uint32_t queueFamilyIndex) const;

    void Reset() noexcept;

private:
    friend core::Result<VulkanDeviceOwner> CreateVulkanDeviceForAdapterSelection(
        const VulkanGlobalDispatch& dispatch, std::shared_ptr<VulkanInstanceOwner> instance,
        VkSurfaceKHR surface, const RenderAdapterSelection& selection,
        const RenderDeviceDesc& desc);

    VulkanDeviceOwner(std::shared_ptr<VulkanInstanceOwner> instance,
                      VkPhysicalDevice physicalDevice, VkDevice device, void* allocator,
                      std::vector<VkQueue> queues,
                      std::unique_ptr<VulkanQueueSynchronization> queueSynchronization,
                      VulkanGlobalDispatch::DestroyDeviceFn destroyDevice,
                      VulkanGlobalDispatch::DeviceWaitIdleFn deviceWaitIdle,
                      VulkanGlobalDispatch::QueueWaitIdleFn queueWaitIdle,
                      VulkanGlobalDispatch::DestroyAllocatorFn destroyAllocator,
                      VulkanGlobalDispatch dispatch, std::optional<VolkDeviceTable> volkTable,
                      VulkanDeviceInfo info,
                      std::shared_ptr<VulkanDeviceChildLifetime> childLifetime) noexcept;

    std::shared_ptr<VulkanInstanceOwner> m_instance;
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    void* m_allocator{};
    std::vector<VkQueue> m_queues{};
    std::unique_ptr<VulkanQueueSynchronization> m_queueSynchronization;
    VulkanGlobalDispatch::DestroyDeviceFn m_destroyDevice{};
    VulkanGlobalDispatch::DeviceWaitIdleFn m_deviceWaitIdle{};
    VulkanGlobalDispatch::QueueWaitIdleFn m_queueWaitIdle{};
    VulkanGlobalDispatch::DestroyAllocatorFn m_destroyAllocator{};
    VulkanGlobalDispatch m_dispatch{};
    std::optional<VolkDeviceTable> m_volkTable{};
    VulkanDeviceInfo m_info{};
    std::shared_ptr<VulkanDeviceChildLifetime> m_childLifetime;
};

struct VulkanSwapchainConfig final
{
    SelectedPresentationConfig presentation{};
    platform::WindowId windowId{};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkColorSpaceKHR colorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkExtent2D extent{};
    VkCompositeAlphaFlagBitsKHR compositeAlpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR};
    VkSurfaceTransformFlagBitsKHR preTransform{VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR};
    VkPresentModeKHR presentMode{VK_PRESENT_MODE_FIFO_KHR};
    std::uint32_t imageCount{};
    VkSharingMode sharingMode{VK_SHARING_MODE_EXCLUSIVE};
    std::vector<std::uint32_t> sharingQueueFamilyIndices{};
    bool usesSurfaceCurrentExtent{};

    [[nodiscard]] friend bool operator==(const VulkanSwapchainConfig& lhs,
                                         const VulkanSwapchainConfig& rhs) = default;
};

enum class VulkanSwapchainCreationOutcome : std::uint8_t
{
    NotAttempted = 0,
    Created = 1,
    OutOfDate = 2
};

struct VulkanSwapchainCreationState final
{
    VulkanSwapchainCreationOutcome outcome{VulkanSwapchainCreationOutcome::NotAttempted};
    bool nativeCallAttempted{};
    bool oldSwapchainRetired{};
};

class VulkanSwapchainOwner final
{
public:
    VulkanSwapchainOwner() noexcept;
    VulkanSwapchainOwner(const VulkanSwapchainOwner&) = delete;
    VulkanSwapchainOwner& operator=(const VulkanSwapchainOwner&) = delete;
    VulkanSwapchainOwner(VulkanSwapchainOwner&& other) noexcept;
    VulkanSwapchainOwner& operator=(VulkanSwapchainOwner&& other) noexcept;
    ~VulkanSwapchainOwner();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] VkSwapchainKHR GetHandle() const noexcept;
    [[nodiscard]] VkRenderPass GetRenderPass() const noexcept;
    [[nodiscard]] VkFramebuffer GetFramebuffer(std::uint32_t imageIndex) const noexcept;
    [[nodiscard]] const VulkanSwapchainConfig& GetConfig() const noexcept;
    [[nodiscard]] std::uint32_t GetFramebufferCount() const noexcept;

    void Reset() noexcept;

private:
    friend core::Result<VulkanSwapchainOwner> CreateVulkanSwapchainForTarget(
        const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface,
        const VulkanDeviceQueuePlan& queuePlan, const RenderTargetDesc& desc);
    friend core::Result<VulkanSwapchainOwner> CreateVulkanSwapchainForSelectedConfig(
        const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface,
        const VulkanSwapchainConfig& config, VkSwapchainKHR oldSwapchain,
        VulkanSwapchainCreationState& creationState);

    VulkanSwapchainOwner(VkDevice device, VkSwapchainKHR swapchain, std::vector<VkImage> images,
                         std::vector<VkImageView> imageViews, VkRenderPass renderPass,
                         std::vector<VkFramebuffer> framebuffers,
                         VulkanGlobalDispatch::DestroySwapchainFn destroySwapchain,
                         VulkanGlobalDispatch::DestroyImageViewFn destroyImageView,
                         VulkanGlobalDispatch::DestroyRenderPassFn destroyRenderPass,
                         VulkanGlobalDispatch::DestroyFramebufferFn destroyFramebuffer,
                         VulkanSwapchainConfig config) noexcept;

    VkDevice m_device{VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    std::vector<VkImage> m_images{};
    std::vector<VkImageView> m_imageViews{};
    VkRenderPass m_renderPass{VK_NULL_HANDLE};
    std::vector<VkFramebuffer> m_framebuffers{};
    VulkanGlobalDispatch::DestroySwapchainFn m_destroySwapchain{};
    VulkanGlobalDispatch::DestroyImageViewFn m_destroyImageView{};
    VulkanGlobalDispatch::DestroyRenderPassFn m_destroyRenderPass{};
    VulkanGlobalDispatch::DestroyFramebufferFn m_destroyFramebuffer{};
    VulkanSwapchainConfig m_config{};
};

enum class VulkanDraw2DColorContract : std::uint8_t
{
    LinearPremultipliedIntoOpaqueSdrSrgb = 0
};

struct VulkanDraw2DPipelineCompatibilityKey final
{
    VkFormat colorAttachmentFormat{VK_FORMAT_UNDEFINED};
    VulkanDraw2DColorContract colorContract{
        VulkanDraw2DColorContract::LinearPremultipliedIntoOpaqueSdrSrgb};
    VkSampleCountFlagBits sampleCount{VK_SAMPLE_COUNT_1_BIT};
    std::uint64_t renderPassCompatibilitySignature{};
    std::uint32_t subpass{};
    std::uint64_t shaderSchemaFingerprint{};
    std::uint64_t fixedStateSignature{};

    [[nodiscard]] friend constexpr bool operator==(
        const VulkanDraw2DPipelineCompatibilityKey& lhs,
        const VulkanDraw2DPipelineCompatibilityKey& rhs) noexcept = default;
};

struct VulkanDraw2DCommandDraw final
{
    VkRect2D scissor{};
    std::uint32_t firstIndex{};
    std::uint32_t indexCount{};
    std::int32_t baseVertex{};

    [[nodiscard]] friend constexpr bool operator==(
        const VulkanDraw2DCommandDraw& lhs, const VulkanDraw2DCommandDraw& rhs) noexcept = default;
};

struct VulkanDraw2DPipelineCacheUpdate final
{
    bool cacheHit{};
    bool replaced{};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkPipelineLayout layout{VK_NULL_HANDLE};

    [[nodiscard]] friend constexpr bool operator==(
        const VulkanDraw2DPipelineCacheUpdate& lhs,
        const VulkanDraw2DPipelineCacheUpdate& rhs) noexcept = default;
};

class VulkanDraw2DPipelineOwner final
{
public:
    VulkanDraw2DPipelineOwner() noexcept;
    VulkanDraw2DPipelineOwner(const VulkanDraw2DPipelineOwner&) = delete;
    VulkanDraw2DPipelineOwner& operator=(const VulkanDraw2DPipelineOwner&) = delete;
    VulkanDraw2DPipelineOwner(VulkanDraw2DPipelineOwner&& other) noexcept;
    VulkanDraw2DPipelineOwner& operator=(VulkanDraw2DPipelineOwner&& other) noexcept;
    ~VulkanDraw2DPipelineOwner();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] VkPipeline GetPipeline() const noexcept;
    [[nodiscard]] VkPipelineLayout GetLayout() const noexcept;
    [[nodiscard]] VkShaderModule GetVertexShaderModule() const noexcept;
    [[nodiscard]] VkShaderModule GetFragmentShaderModule() const noexcept;
    [[nodiscard]] const VulkanDraw2DPipelineCompatibilityKey& GetKey() const noexcept;

    void Reset() noexcept;

private:
    friend core::Result<VulkanDraw2DPipelineOwner> CreateVulkanDraw2DPipeline(
        const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
        const VulkanSwapchainOwner& swapchain, const VulkanDraw2DPipelineCompatibilityKey& key);

    VulkanDraw2DPipelineOwner(VkDevice device, VkShaderModule vertexShader,
                              VkShaderModule fragmentShader, VkPipelineLayout layout,
                              VkPipeline pipeline,
                              VulkanGlobalDispatch::DestroyShaderModuleFn destroyShaderModule,
                              VulkanGlobalDispatch::DestroyPipelineLayoutFn destroyPipelineLayout,
                              VulkanGlobalDispatch::DestroyPipelineFn destroyPipeline,
                              VulkanDraw2DPipelineCompatibilityKey key,
                              std::shared_ptr<VulkanDeviceChildLifetime> childLifetime) noexcept;

    VkDevice m_device{VK_NULL_HANDLE};
    VkShaderModule m_vertexShader{VK_NULL_HANDLE};
    VkShaderModule m_fragmentShader{VK_NULL_HANDLE};
    VkPipelineLayout m_layout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VulkanGlobalDispatch::DestroyShaderModuleFn m_destroyShaderModule{};
    VulkanGlobalDispatch::DestroyPipelineLayoutFn m_destroyPipelineLayout{};
    VulkanGlobalDispatch::DestroyPipelineFn m_destroyPipeline{};
    VulkanDraw2DPipelineCompatibilityKey m_key{};
    std::shared_ptr<VulkanDeviceChildLifetime> m_childLifetime;
    std::thread::id m_ownerThread{};
};

class VulkanDraw2DPipelineCache final
{
public:
    VulkanDraw2DPipelineCache() noexcept;
    VulkanDraw2DPipelineCache(const VulkanDraw2DPipelineCache&) = delete;
    VulkanDraw2DPipelineCache& operator=(const VulkanDraw2DPipelineCache&) = delete;
    VulkanDraw2DPipelineCache(VulkanDraw2DPipelineCache&& other) noexcept;
    VulkanDraw2DPipelineCache& operator=(VulkanDraw2DPipelineCache&& other) noexcept;
    ~VulkanDraw2DPipelineCache();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] const VulkanDraw2DPipelineOwner& GetCurrentPipeline() const noexcept;
    [[nodiscard]] std::shared_ptr<const VulkanDraw2DPipelineOwner> AcquireCurrentPipeline()
        const noexcept;
    [[nodiscard]] core::Result<VulkanDraw2DPipelineCacheUpdate> GetOrCreate(
        const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
        const VulkanSwapchainOwner& swapchain, const VulkanDraw2DPipelineCompatibilityKey& key);

    void Reset() noexcept;

private:
    VkDevice m_device{VK_NULL_HANDLE};
    std::shared_ptr<VulkanDraw2DPipelineOwner> m_pipeline;
};

enum class VulkanDraw2DUploadSlotState : std::uint8_t
{
    Idle = 0,
    Reserved = 1,
    Submitted = 2
};

struct VulkanDraw2DUploadReservation final
{
    std::uint32_t slotIndex{};
    std::uint64_t generation{};
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceSize vertexOffset{};
    VkDeviceSize vertexBytes{};
    VkDeviceSize indexOffset{};
    VkDeviceSize indexBytes{};
    VkDeviceSize uploadBytes{};

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return generation > 0U && buffer != VK_NULL_HANDLE && uploadBytes > 0U;
    }
};

struct VulkanDraw2DUploadLayout final
{
    VkDeviceSize vertexBytes{};
    VkDeviceSize indexOffset{};
    VkDeviceSize indexBytes{};
    VkDeviceSize uploadBytes{};

    [[nodiscard]] friend constexpr bool operator==(const VulkanDraw2DUploadLayout& lhs,
                                                   const VulkanDraw2DUploadLayout& rhs) noexcept =
        default;
};

[[nodiscard]] core::Result<VulkanDraw2DUploadLayout> ComputeVulkanDraw2DUploadLayout(
    VkDeviceSize vertexBytes, VkDeviceSize indexBytes, VkDeviceSize maximumUploadBytes);

struct VulkanDraw2DUploadSlotSnapshot final
{
    VulkanDraw2DUploadSlotState state{VulkanDraw2DUploadSlotState::Idle};
    std::uint64_t generation{};
    VkDeviceSize capacityBytes{};
    VkDeviceSize reservedBytes{};
    bool coherent{};

    [[nodiscard]] friend constexpr bool operator==(
        const VulkanDraw2DUploadSlotSnapshot& lhs,
        const VulkanDraw2DUploadSlotSnapshot& rhs) noexcept = default;
};

struct VulkanDraw2DUploadStats final
{
    VkDeviceSize currentCapacityBytes{};
    VkDeviceSize reservedBytes{};
    VkDeviceSize uploadedBytes{};
    VkDeviceSize highWaterReservedBytes{};
    std::uint64_t allocationCount{};
    std::uint64_t growthCount{};
    std::uint64_t generationCount{};
    std::uint64_t flushCount{};
    std::uint64_t retirementCount{};

    [[nodiscard]] friend constexpr bool operator==(
        const VulkanDraw2DUploadStats& lhs, const VulkanDraw2DUploadStats& rhs) noexcept = default;
};

class VulkanDraw2DUploadBufferOwner final
{
public:
    VulkanDraw2DUploadBufferOwner() noexcept;
    VulkanDraw2DUploadBufferOwner(const VulkanDraw2DUploadBufferOwner&) = delete;
    VulkanDraw2DUploadBufferOwner& operator=(const VulkanDraw2DUploadBufferOwner&) = delete;
    VulkanDraw2DUploadBufferOwner(VulkanDraw2DUploadBufferOwner&& other) noexcept;
    VulkanDraw2DUploadBufferOwner& operator=(VulkanDraw2DUploadBufferOwner&& other) noexcept;
    ~VulkanDraw2DUploadBufferOwner();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] VkBuffer GetBuffer() const noexcept;
    [[nodiscard]] std::byte* GetMappedData() const noexcept;
    [[nodiscard]] VkDeviceSize GetCapacity() const noexcept;
    [[nodiscard]] bool IsCoherent() const noexcept;
    [[nodiscard]] std::uint64_t GetGeneration() const noexcept;

    void Reset() noexcept;

private:
    friend class VulkanDraw2DUploadArena;

    VulkanDraw2DUploadBufferOwner(void* allocator, VkBuffer buffer, void* allocation,
                                  std::byte* mappedData, VkDeviceSize capacity,
                                  VkDeviceSize allocationSize, bool coherent,
                                  std::uint64_t generation,
                                  VulkanGlobalDispatch::DestroyBufferFn destroyBuffer,
                                  VulkanGlobalDispatch::UnmapMemoryFn unmapMemory) noexcept;

    void* m_allocator{};
    VkBuffer m_buffer{VK_NULL_HANDLE};
    void* m_allocation{};
    std::byte* m_mappedData{};
    VkDeviceSize m_capacity{};
    VkDeviceSize m_allocationSize{};
    bool m_coherent{};
    std::uint64_t m_generation{};
    VulkanGlobalDispatch::DestroyBufferFn m_destroyBuffer{};
    VulkanGlobalDispatch::UnmapMemoryFn m_unmapMemory{};
    std::thread::id m_ownerThread{};
};

class VulkanDraw2DUploadArena final
{
public:
    VulkanDraw2DUploadArena() noexcept;
    VulkanDraw2DUploadArena(const VulkanDraw2DUploadArena&) = delete;
    VulkanDraw2DUploadArena& operator=(const VulkanDraw2DUploadArena&) = delete;
    VulkanDraw2DUploadArena(VulkanDraw2DUploadArena&& other) noexcept;
    VulkanDraw2DUploadArena& operator=(VulkanDraw2DUploadArena&& other) noexcept;
    ~VulkanDraw2DUploadArena();

    [[nodiscard]] static core::Result<VulkanDraw2DUploadArena> Create(
        const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
        std::uint32_t slotCount, VkDeviceSize maximumUploadBytes);

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] std::uint32_t GetSlotCount() const noexcept;
    [[nodiscard]] VulkanDraw2DUploadSlotSnapshot GetSlotSnapshot(
        std::uint32_t slotIndex) const noexcept;
    [[nodiscard]] VulkanDraw2DUploadStats GetStats() const noexcept;
    [[nodiscard]] core::Result<VulkanDraw2DUploadReservation> ReserveAndUpload(
        std::uint32_t slotIndex, std::span<const std::byte> vertices,
        std::span<const std::byte> indices);
    void MarkSubmitted(std::uint32_t slotIndex) noexcept;
    void Abandon(std::uint32_t slotIndex) noexcept;
    void Complete(std::uint32_t slotIndex) noexcept;
    void Reset() noexcept;

private:
    struct Slot final
    {
        VulkanDraw2DUploadBufferOwner buffer{};
        VulkanDraw2DUploadSlotState state{VulkanDraw2DUploadSlotState::Idle};
        VkDeviceSize reservedBytes{};
    };

    VulkanDraw2DUploadArena(VulkanGlobalDispatch dispatch, void* allocator,
                            VkDeviceSize nonCoherentAtomSize, VkDeviceSize maximumUploadBytes,
                            std::vector<Slot> slots) noexcept;

    [[nodiscard]] core::Result<VulkanDraw2DUploadBufferOwner> CreateBuffer(
        VkDeviceSize capacity, std::uint64_t generation);
    [[nodiscard]] core::VoidResult CopyAndFlush(VulkanDraw2DUploadBufferOwner& buffer,
                                                VkDeviceSize vertexOffset,
                                                std::span<const std::byte> vertices,
                                                VkDeviceSize indexOffset,
                                                std::span<const std::byte> indices);

    VulkanGlobalDispatch m_dispatch{};
    void* m_allocator{};
    VkDeviceSize m_nonCoherentAtomSize{1U};
    VkDeviceSize m_maximumUploadBytes{};
    std::vector<Slot> m_slots{};
    VulkanDraw2DUploadStats m_stats{};
    std::thread::id m_ownerThread{};
};

struct VulkanFrameSlotResources final
{
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkSemaphore imageAvailableSemaphore{VK_NULL_HANDLE};
    VkFence imageAcquiredFence{VK_NULL_HANDLE};
    VkFence inFlightFence{VK_NULL_HANDLE};
    bool acquireFencePending{};
    bool acquireFenceNeedsReset{};
    bool submissionFencePending{};

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return commandBuffer != VK_NULL_HANDLE && imageAvailableSemaphore != VK_NULL_HANDLE &&
               imageAcquiredFence != VK_NULL_HANDLE && inFlightFence != VK_NULL_HANDLE;
    }
};

struct VulkanCompletedSubmissionSlots final
{
    std::array<std::uint32_t, QueuedFrameLatency::kMaximumFrames> values{};
    std::uint32_t count{};

    [[nodiscard]] constexpr const std::uint32_t* begin() const noexcept
    {
        return values.data();
    }

    [[nodiscard]] constexpr const std::uint32_t* end() const noexcept
    {
        return values.data() + count;
    }
};

class VulkanFrameResourcesOwner final
{
public:
    VulkanFrameResourcesOwner() noexcept;
    VulkanFrameResourcesOwner(const VulkanFrameResourcesOwner&) = delete;
    VulkanFrameResourcesOwner& operator=(const VulkanFrameResourcesOwner&) = delete;
    VulkanFrameResourcesOwner(VulkanFrameResourcesOwner&& other) noexcept;
    VulkanFrameResourcesOwner& operator=(VulkanFrameResourcesOwner&& other) noexcept;
    ~VulkanFrameResourcesOwner();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] bool IsPoisoned() const noexcept;
    [[nodiscard]] std::uint32_t GetSlotCount() const noexcept;
    [[nodiscard]] VulkanFrameSlotResources GetSlot(std::uint32_t slotIndex) const noexcept;
    [[nodiscard]] VkSemaphore GetRenderFinishedSemaphore(std::uint32_t imageIndex) const noexcept;
    [[nodiscard]] core::Result<bool> PrepareFrameSlot(const VulkanGlobalDispatch& dispatch,
                                                      std::uint32_t slotIndex,
                                                      std::uint64_t timeoutNanoseconds);
    void RecordImageAcquired(std::uint32_t slotIndex) noexcept;
    void RecordSubmissionQueued(std::uint32_t slotIndex) noexcept;
    [[nodiscard]] VulkanDraw2DUploadArena& GetDraw2DUploadArena() noexcept;
    [[nodiscard]] const VulkanDraw2DUploadArena& GetDraw2DUploadArena() const noexcept;
    [[nodiscard]] core::VoidResult AttachDraw2DPipeline(
        std::uint32_t slotIndex, std::shared_ptr<const VulkanDraw2DPipelineOwner> pipeline);
    void AbandonDraw2D(std::uint32_t slotIndex) noexcept;
    void MarkPoisoned() noexcept;
    [[nodiscard]] VulkanCompletedSubmissionSlots ConsumeCompletedSubmissionSlots() noexcept;
    [[nodiscard]] core::Result<bool> AreAllFencesSignaled(const VulkanGlobalDispatch& dispatch,
                                                          std::uint64_t timeoutNanoseconds = 0U);

    void Reset() noexcept;

private:
    friend core::Result<VulkanFrameResourcesOwner> CreateVulkanFrameResourcesForTarget(
        const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
        platform::WindowId windowId, const VulkanDeviceQueuePlan& queuePlan,
        QueuedFrameLatency queuedLatency, std::uint32_t swapchainImageCount);

    VulkanFrameResourcesOwner(
        VkDevice device, VkCommandPool commandPool, std::vector<VulkanFrameSlotResources> slots,
        std::vector<VkSemaphore> renderFinishedSemaphores,
        VulkanDraw2DUploadArena&& draw2DUploadArena,
        std::vector<std::shared_ptr<const VulkanDraw2DPipelineOwner>> draw2DSubmissionPipelines,
        VulkanGlobalDispatch::DestroyCommandPoolFn destroyCommandPool,
        VulkanGlobalDispatch::DestroySemaphoreFn destroySemaphore,
        VulkanGlobalDispatch::DestroyFenceFn destroyFence) noexcept;

    VkDevice m_device{VK_NULL_HANDLE};
    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    std::vector<VulkanFrameSlotResources> m_slots{};
    std::vector<VkSemaphore> m_renderFinishedSemaphores{};
    VulkanCompletedSubmissionSlots m_completedSubmissionSlots{};
    VulkanDraw2DUploadArena m_draw2DUploadArena{};
    std::vector<std::shared_ptr<const VulkanDraw2DPipelineOwner>> m_draw2DSubmissionPipelines{};
    bool m_poisoned{};
    VulkanGlobalDispatch::DestroyCommandPoolFn m_destroyCommandPool{};
    VulkanGlobalDispatch::DestroySemaphoreFn m_destroySemaphore{};
    VulkanGlobalDispatch::DestroyFenceFn m_destroyFence{};
};

enum class VulkanPresentationCompletionPath : std::uint8_t
{
    SwapchainMaintenanceFence = 0,
    PresentWait = 1,
    CoreAcquireHistory = 2
};

enum class VulkanPresentationCompletionStatus : std::uint8_t
{
    Complete = 0,
    Pending = 1,
    PracticalIdleFallbackRequired = 2
};

struct VulkanPresentationCompletionResult final
{
    VulkanPresentationCompletionStatus status{VulkanPresentationCompletionStatus::Complete};
    bool usedExplicitCompletion{};
};

struct VulkanPresentFenceSlot final
{
    VkFence fence{VK_NULL_HANDLE};
    bool pending{};
};

class VulkanPresentationTrackerOwner final
{
public:
    VulkanPresentationTrackerOwner() noexcept;
    VulkanPresentationTrackerOwner(const VulkanPresentationTrackerOwner&) = delete;
    VulkanPresentationTrackerOwner& operator=(const VulkanPresentationTrackerOwner&) = delete;
    VulkanPresentationTrackerOwner(VulkanPresentationTrackerOwner&& other) noexcept;
    VulkanPresentationTrackerOwner& operator=(VulkanPresentationTrackerOwner&& other) noexcept;
    ~VulkanPresentationTrackerOwner();

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] VulkanPresentationCompletionPath GetCompletionPath() const noexcept;
    [[nodiscard]] bool UsesPresentFences() const noexcept;
    [[nodiscard]] bool UsesPresentIds() const noexcept;
    [[nodiscard]] bool HasQueuedPresentation() const noexcept;
    [[nodiscard]] core::Result<bool> PrepareFrameSlot(const VulkanGlobalDispatch& dispatch,
                                                      std::uint32_t frameSlotIndex,
                                                      std::uint64_t timeoutNanoseconds);
    [[nodiscard]] VkFence GetPresentFence(std::uint32_t frameSlotIndex) const noexcept;
    [[nodiscard]] std::uint64_t ReservePresentId() noexcept;
    void RecordFrameFenceCompletion(std::uint32_t frameSlotIndex) noexcept;
    void RecordImageAcquired(std::uint32_t frameSlotIndex, std::uint32_t imageIndex) noexcept;
    void RecordAcquireSubmission(std::uint32_t frameSlotIndex) noexcept;
    void AbandonAcquiredFrame(std::uint32_t frameSlotIndex) noexcept;
    void RecordPresentResult(std::uint32_t frameSlotIndex, std::uint32_t imageIndex,
                             std::uint64_t presentId, VkResult result) noexcept;
    [[nodiscard]] bool ConsumeCorePresentationCompletion() noexcept;
    [[nodiscard]] core::Result<VulkanPresentationCompletionResult> PollCompletion(
        const VulkanGlobalDispatch& dispatch, VkSwapchainKHR swapchain,
        std::uint64_t timeoutNanoseconds);
    void MarkAwaitingSuccessorPresentation() noexcept;
    void MarkSuccessorPresentationComplete() noexcept;

    void Reset() noexcept;

private:
    friend core::Result<VulkanPresentationTrackerOwner> CreateVulkanPresentationTrackerForTarget(
        const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
        platform::WindowId windowId, std::uint32_t frameSlotCount,
        std::uint32_t swapchainImageCount);

    VulkanPresentationTrackerOwner(VkDevice device, VulkanPresentationCompletionPath completionPath,
                                   bool usesPresentIds,
                                   std::vector<std::uint64_t> imagePresentationSerials,
                                   std::vector<std::uint64_t> pendingAcquireSerials,
                                   std::vector<bool> acquireProofPending,
                                   std::vector<VulkanPresentFenceSlot> presentFenceSlots,
                                   VulkanGlobalDispatch::DestroyFenceFn destroyFence) noexcept;

    VkDevice m_device{VK_NULL_HANDLE};
    VulkanPresentationCompletionPath m_completionPath{
        VulkanPresentationCompletionPath::CoreAcquireHistory};
    bool m_usesPresentIds{};
    bool m_hadQueuedPresentation{};
    bool m_completionSatisfied{};
    bool m_usedExplicitCompletion{};
    std::uint64_t m_nextPresentId{1U};
    std::uint64_t m_lastQueuedPresentId{};
    std::uint64_t m_nextCorePresentationSerial{1U};
    bool m_coreCompletionObserved{};
    std::vector<std::uint64_t> m_imagePresentationSerials{};
    std::vector<std::uint64_t> m_pendingAcquireSerials{};
    std::vector<bool> m_acquireProofPending{};
    std::vector<VulkanPresentFenceSlot> m_presentFenceSlots{};
    VulkanGlobalDispatch::DestroyFenceFn m_destroyFence{};
};

struct RenderLifetimeContractOwners final
{
    RenderDevice device;
    RenderTarget target;
};

struct RenderDeviceSurfaceTestOwners final
{
    RenderDevice device;
    PreparedSurface surface;
};

struct RenderPublicLifecycleTestOwners final
{
    RenderBootstrap bootstrap;
    RenderDevice device;
    RenderTarget target;
};

class RenderBackendTestAccess final
{
public:
    [[nodiscard]] static core::Result<RenderLifetimeContractOwners> CreateLifetimeContractOwners(
        const RenderTargetDesc& desc);
    [[nodiscard]] static core::Result<RenderDeviceSurfaceTestOwners> CreateDeviceAndPreparedSurface(
        VulkanGlobalDispatch dispatch, VulkanDeviceOwner&& device, VulkanSurfaceOwner&& surface,
        const SurfacePreparationDesc& desc);
    [[nodiscard]] static core::Result<RenderPublicLifecycleTestOwners> CreatePublicLifecycleOwners(
        VulkanDeviceOwner&& device, VulkanSurfaceOwner&& surface, VulkanSwapchainOwner&& swapchain,
        VulkanFrameResourcesOwner&& frameResources,
        VulkanPresentationTrackerOwner&& presentationTracker, const RenderTargetDesc& desc);
    [[nodiscard]] static core::Result<RenderTarget> CreateAdditionalTarget(
        RenderDevice& device, VulkanSurfaceOwner&& surface, VulkanSwapchainOwner&& swapchain,
        VulkanFrameResourcesOwner&& frameResources,
        VulkanPresentationTrackerOwner&& presentationTracker, const RenderTargetDesc& desc);
    static void FailNextTargetStateAllocation() noexcept;
    static void FailNextFrameStateAllocation() noexcept;
    static void FailNextRetirementAllocation() noexcept;
    [[nodiscard]] static std::uint32_t GetBootstrapTargetCount(const RenderDevice& device) noexcept;
    [[nodiscard]] static core::VoidResult DrainOrphanedPresentationResources(RenderDevice& device);
    [[nodiscard]] static core::VoidResult WaitPresentationQueueIdle(RenderDevice& device);
    [[nodiscard]] static core::Result<RenderTarget> CreateTarget(
        VulkanGlobalDispatch dispatch, VulkanDeviceOwner&& device, VulkanSurfaceOwner&& surface,
        VulkanSwapchainOwner&& swapchain, VulkanFrameResourcesOwner&& frameResources,
        VulkanPresentationTrackerOwner&& presentationTracker, const RenderTargetDesc& desc);
    [[nodiscard]] static core::Result<PreparedSurface> CreateRecoverySurface(
        RenderTarget& target, VulkanSurfaceOwner&& surface, RenderTargetSnapshot snapshot);
};

enum class VulkanFrameRecordingPhase : std::uint8_t
{
    Inactive = 0,
    Recording = 1,
    ClearRecorded = 2,
    Draw2DRecorded = 3,
    Submitted = 4,
    Terminal = 5
};

struct VulkanFrameRecordingState final
{
    VulkanFrameRecordingPhase phase{VulkanFrameRecordingPhase::Inactive};
    std::uint32_t frameSlotIndex{};
    std::uint32_t imageIndex{};
    bool suboptimal{};

    [[nodiscard]] constexpr bool IsActive() const noexcept
    {
        return phase == VulkanFrameRecordingPhase::Recording ||
               phase == VulkanFrameRecordingPhase::ClearRecorded ||
               phase == VulkanFrameRecordingPhase::Draw2DRecorded;
    }
};

struct VulkanFrameBeginResult final
{
    FrameStatus status{FrameStatus::Ready};
    VulkanFrameRecordingState recording{};
};

struct VulkanFramePresentationResult final
{
    FrameStatus status{FrameStatus::Presented};
    bool presented{};
    bool suboptimal{};
    std::uint32_t imageIndex{};
};

class VulkanInstanceBootstrap final
{
public:
    [[nodiscard]] core::Result<VulkanInstanceInfo> EnsureInitialized(
        const VulkanGlobalDispatch& dispatch, VulkanWsiKind wsiKind,
        RenderValidationMode validationMode);

    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] std::optional<VulkanInstanceInfo> GetInfo() const noexcept;
    [[nodiscard]] std::shared_ptr<VulkanInstanceOwner> GetOwner() const noexcept;
    [[nodiscard]] RenderValidationReport GetValidationReport() const noexcept;

    void Reset() noexcept;

private:
    std::shared_ptr<VulkanInstanceOwner> m_owner;
};

[[nodiscard]] VulkanGlobalDispatch CreateVolkGlobalDispatch() noexcept;
[[nodiscard]] core::Result<VulkanInstanceOwner> CreateVulkanInstanceForWsi(
    const VulkanGlobalDispatch& dispatch, VulkanWsiKind wsiKind,
    RenderValidationMode validationMode);
[[nodiscard]] core::Result<VulkanSurfaceOwner> CreateVulkanSurfaceForNativeWindow(
    const VulkanGlobalDispatch& dispatch, std::shared_ptr<VulkanInstanceOwner> instance,
    const platform::NativeWindowHandle& nativeWindowHandle);
[[nodiscard]] core::Result<RenderAdapterSelection> SelectVulkanAdapterForSurface(
    const VulkanGlobalDispatch& dispatch, std::shared_ptr<VulkanInstanceOwner> instance,
    VkSurfaceKHR surface, const RenderAdapterSelectionDesc& desc);
[[nodiscard]] core::Result<VulkanDeviceOwner> CreateVulkanDeviceForAdapterSelection(
    const VulkanGlobalDispatch& dispatch, std::shared_ptr<VulkanInstanceOwner> instance,
    VkSurfaceKHR surface, const RenderAdapterSelection& selection, const RenderDeviceDesc& desc);
[[nodiscard]] core::Result<VulkanDeviceQueuePlan> ValidateVulkanDeviceSurfaceCompatibility(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface);
[[nodiscard]] core::VoidResult ValidateVulkanSurfaceOutputCompatibility(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface);
[[nodiscard]] core::Result<VulkanSwapchainConfig> SelectVulkanSwapchainConfig(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface,
    const VulkanDeviceQueuePlan& queuePlan, const RenderTargetDesc& desc);
[[nodiscard]] core::Result<VulkanSwapchainOwner> CreateVulkanSwapchainForTarget(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface,
    const VulkanDeviceQueuePlan& queuePlan, const RenderTargetDesc& desc);
[[nodiscard]] core::Result<VulkanSwapchainOwner> CreateVulkanSwapchainForSelectedConfig(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface,
    const VulkanSwapchainConfig& config, VkSwapchainKHR oldSwapchain,
    VulkanSwapchainCreationState& creationState);
[[nodiscard]] core::Result<VulkanFrameResourcesOwner> CreateVulkanFrameResourcesForTarget(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    platform::WindowId windowId, const VulkanDeviceQueuePlan& queuePlan,
    QueuedFrameLatency queuedLatency, std::uint32_t swapchainImageCount);
[[nodiscard]] core::Result<VulkanPresentationTrackerOwner> CreateVulkanPresentationTrackerForTarget(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    platform::WindowId windowId, std::uint32_t frameSlotCount, std::uint32_t swapchainImageCount);
[[nodiscard]] core::Result<VulkanFrameBeginResult> BeginVulkanFrame(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    const VulkanSwapchainOwner& swapchain, VulkanFrameResourcesOwner& frameResources,
    VulkanPresentationTrackerOwner& presentationTracker, const VulkanDeviceQueuePlan& queuePlan,
    std::uint32_t frameSlotIndex);
[[nodiscard]] core::VoidResult RecordVulkanFrameClear(
    const VulkanGlobalDispatch& dispatch, const VulkanSwapchainOwner& swapchain,
    const VulkanFrameResourcesOwner& frameResources, VulkanFrameRecordingState& recording,
    ClearColor clearColor);
[[nodiscard]] core::VoidResult RecordVulkanDraw2DStage(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    const VulkanSwapchainOwner& swapchain, VulkanFrameResourcesOwner& frameResources,
    VulkanDraw2DPipelineCache& pipelineCache, VulkanFrameRecordingState& recording,
    const ::pond::render::draw2d::Draw2DPacket& packet);
[[nodiscard]] std::uint64_t GetVulkanDraw2DFixedStateSignature() noexcept;
[[nodiscard]] std::uint64_t ComputeVulkanDraw2DRenderPassCompatibilitySignature(
    VkFormat colorAttachmentFormat, VkSampleCountFlagBits sampleCount,
    VulkanDraw2DColorContract colorContract, std::uint32_t subpass) noexcept;
[[nodiscard]] VulkanDraw2DPipelineCompatibilityKey MakeVulkanDraw2DPipelineCompatibilityKey(
    const VulkanSwapchainConfig& config, std::uint32_t subpass = 0U) noexcept;
[[nodiscard]] core::Result<VulkanDraw2DPipelineOwner> CreateVulkanDraw2DPipeline(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    const VulkanSwapchainOwner& swapchain, const VulkanDraw2DPipelineCompatibilityKey& key);
[[nodiscard]] core::VoidResult RecordVulkanDraw2DPipelineCommands(
    const VulkanGlobalDispatch& dispatch, VkCommandBuffer commandBuffer,
    const VulkanDraw2DPipelineOwner& pipeline, VkBuffer vertexBuffer, VkDeviceSize vertexOffset,
    VkBuffer indexBuffer, VkDeviceSize indexOffset, VkExtent2D extent,
    VkExtent2D maximumViewportExtent,
    std::span<const ::pond::render::draw2d::Draw2DDrawRecord> draws);
[[nodiscard]] core::Result<VulkanFramePresentationResult> FinishAndPresentVulkanFrame(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    const VulkanSwapchainOwner& swapchain, VulkanFrameResourcesOwner& frameResources,
    VulkanPresentationTrackerOwner& presentationTracker, const VulkanDeviceQueuePlan& queuePlan,
    VulkanFrameRecordingState& recording);
void AbandonVulkanFrame(VulkanFrameResourcesOwner& frameResources,
                        VulkanPresentationTrackerOwner& presentationTracker,
                        VulkanFrameRecordingState& recording) noexcept;
[[nodiscard]] const char* GetVulkanWsiExtensionName(VulkanWsiKind wsiKind) noexcept;
[[nodiscard]] std::string_view GetVulkanWsiKindName(VulkanWsiKind wsiKind) noexcept;
[[nodiscard]] VulkanWsiKind GetVulkanWsiKind(
    const platform::NativeWindowHandle& nativeWindowHandle) noexcept;
[[nodiscard]] std::uint32_t GetMinimumVulkanInstanceVersion() noexcept;
[[nodiscard]] std::uint32_t GetMaximumHeaderVulkanInstanceVersion() noexcept;
[[nodiscard]] bool IsDefaultValidationEnabledForDeveloperBuild() noexcept;

VKAPI_ATTR VkBool32 VKAPI_CALL HandleVulkanDebugUtilsMessage(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) noexcept;
} // namespace pond::render::detail
