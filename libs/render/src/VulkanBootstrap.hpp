#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/NativeWindow.hpp>
#include <ponder/render/Bootstrap.hpp>

#include <volk.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

    [[nodiscard]] friend constexpr bool operator==(const VulkanDebugUtilityHooks& lhs,
                                                   const VulkanDebugUtilityHooks& rhs) noexcept =
        default;
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
    VulkanDebugUtilityHooks debugUtilityHooks{};

    [[nodiscard]] friend constexpr bool operator==(const VulkanInstanceInfo& lhs,
                                                   const VulkanInstanceInfo& rhs) noexcept =
        default;
};

struct VulkanSurfaceInfo final
{
    VulkanWsiKind wsiKind{VulkanWsiKind::Win32};

    [[nodiscard]] friend constexpr bool operator==(const VulkanSurfaceInfo& lhs,
                                                   const VulkanSurfaceInfo& rhs) noexcept = default;
};

struct VulkanDeviceInfo final
{
    RenderAdapterSnapshot selectedAdapter{};
    RenderDeviceQueuePlan queuePlan{};
    RenderDeviceOptionalCapabilities optionalCapabilities{};

    [[nodiscard]] friend bool operator==(const VulkanDeviceInfo& lhs,
                                         const VulkanDeviceInfo& rhs) = default;
};

struct VulkanValidationMessageFilter final
{
    std::string messageIdName{};
    std::int32_t messageIdNumber{};

    [[nodiscard]] bool Matches(std::string_view inputName, std::int32_t inputNumber) const noexcept;
};

struct VulkanDebugMessengerContext final
{
    std::string currentOperation{"vulkan-instance-bootstrap"};
    bool failOnWarningOrError{true};
    std::vector<VulkanValidationMessageFilter> exactMessageFilters{};
    std::atomic_bool warningOrErrorSeen{false};
};

struct VulkanGlobalDispatch final
{
    using InitializeFn = VkResult (*)();
    using EnumerateInstanceVersionFn = VkResult (*)(std::uint32_t*);
    using EnumerateInstanceExtensionPropertiesFn =
        VkResult (*)(const char*, std::uint32_t*, VkExtensionProperties*);
    using EnumerateInstanceLayerPropertiesFn = VkResult (*)(std::uint32_t*, VkLayerProperties*);
    using CreateInstanceFn =
        VkResult (*)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);
    using DestroyInstanceFn = void (*)(VkInstance, const VkAllocationCallbacks*);
    using LoadInstanceOnlyFn = void (*)(VkInstance);
    using CreateDebugUtilsMessengerFn = VkResult (*)(VkInstance,
                                                     const VkDebugUtilsMessengerCreateInfoEXT*,
                                                     const VkAllocationCallbacks*,
                                                     VkDebugUtilsMessengerEXT*);
    using DestroyDebugUtilsMessengerFn =
        void (*)(VkInstance, VkDebugUtilsMessengerEXT, const VkAllocationCallbacks*);
    using CreateSurfaceFn =
        VkResult (*)(VkInstance, const void*, const VkAllocationCallbacks*, VkSurfaceKHR*);
    using DestroySurfaceFn = void (*)(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks*);
    using EnumeratePhysicalDevicesFn = VkResult (*)(VkInstance, std::uint32_t*, VkPhysicalDevice*);
    using GetPhysicalDeviceProperties2Fn = void (*)(VkPhysicalDevice, VkPhysicalDeviceProperties2*);
    using GetPhysicalDeviceFeatures2Fn = void (*)(VkPhysicalDevice, VkPhysicalDeviceFeatures2*);
    using GetPhysicalDeviceMemoryPropertiesFn = void (*)(VkPhysicalDevice,
                                                         VkPhysicalDeviceMemoryProperties*);
    using GetPhysicalDeviceQueueFamilyPropertiesFn = void (*)(VkPhysicalDevice,
                                                              std::uint32_t*,
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
                                                              std::uint32_t*, VkExtensionProperties*);
    using CreateDeviceFn = VkResult (*)(VkPhysicalDevice, const VkDeviceCreateInfo*,
                                        const VkAllocationCallbacks*, VkDevice*);
    using DestroyDeviceFn = void (*)(VkDevice, const VkAllocationCallbacks*);
    using LoadDeviceFn = void (*)(VkDevice);
    using GetDeviceQueueFn = void (*)(VkDevice, std::uint32_t, std::uint32_t, VkQueue*);
    using DeviceWaitIdleFn = VkResult (*)(VkDevice);
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
    using SetDebugUtilsObjectNameFn = VkResult (*)(VkDevice,
                                                  const VkDebugUtilsObjectNameInfoEXT*);
    using CmdBeginDebugUtilsLabelFn = void (*)(VkCommandBuffer, const VkDebugUtilsLabelEXT*);
    using CmdEndDebugUtilsLabelFn = void (*)(VkCommandBuffer);
    using CreateAllocatorFn = VkResult (*)(VkInstance, VkPhysicalDevice, VkDevice, std::uint32_t,
                                           void**);
    using DestroyAllocatorFn = void (*)(void*);

    InitializeFn initialize{};
    EnumerateInstanceVersionFn enumerateInstanceVersion{};
    EnumerateInstanceExtensionPropertiesFn enumerateInstanceExtensionProperties{};
    EnumerateInstanceLayerPropertiesFn enumerateInstanceLayerProperties{};
    CreateInstanceFn createInstance{};
    DestroyInstanceFn destroyInstance{};
    LoadInstanceOnlyFn loadInstanceOnly{};
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
    GetDeviceQueueFn getDeviceQueue{};
    DeviceWaitIdleFn deviceWaitIdle{};
    CreateSwapchainFn createSwapchain{};
    DestroySwapchainFn destroySwapchain{};
    GetSwapchainImagesFn getSwapchainImages{};
    CreateImageViewFn createImageView{};
    DestroyImageViewFn destroyImageView{};
    CreateRenderPassFn createRenderPass{};
    DestroyRenderPassFn destroyRenderPass{};
    CreateFramebufferFn createFramebuffer{};
    DestroyFramebufferFn destroyFramebuffer{};
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
    SetDebugUtilsObjectNameFn setDebugUtilsObjectName{};
    CmdBeginDebugUtilsLabelFn cmdBeginDebugUtilsLabel{};
    CmdEndDebugUtilsLabelFn cmdEndDebugUtilsLabel{};
    CreateAllocatorFn createAllocator{};
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

    void Reset() noexcept;

private:
    friend core::Result<VulkanInstanceOwner> CreateVulkanInstanceForWsi(
        const VulkanGlobalDispatch& dispatch, VulkanWsiKind wsiKind,
        RenderValidationMode validationMode);

    VulkanInstanceOwner(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                        VulkanGlobalDispatch::DestroyInstanceFn destroyInstance,
                        VulkanGlobalDispatch::DestroyDebugUtilsMessengerFn destroyDebugMessenger,
                        VulkanInstanceInfo info,
                        std::unique_ptr<VulkanDebugMessengerContext> debugContext) noexcept;

    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
    VulkanGlobalDispatch::DestroyInstanceFn m_destroyInstance{};
    VulkanGlobalDispatch::DestroyDebugUtilsMessengerFn m_destroyDebugMessenger{};
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
    [[nodiscard]] VulkanDeviceInfo GetInfo() const noexcept;
    [[nodiscard]] VkQueue GetQueue(std::uint32_t queueFamilyIndex) const noexcept;
    [[nodiscard]] core::VoidResult WaitIdle() const;

    void Reset() noexcept;

private:
    friend core::Result<VulkanDeviceOwner> CreateVulkanDeviceForAdapterSelection(
        const VulkanGlobalDispatch& dispatch, std::shared_ptr<VulkanInstanceOwner> instance,
        VkSurfaceKHR surface, const RenderAdapterSelection& selection, const RenderDeviceDesc& desc);

    VulkanDeviceOwner(std::shared_ptr<VulkanInstanceOwner> instance, VkPhysicalDevice physicalDevice,
                      VkDevice device, void* allocator, std::vector<VkQueue> queues,
                      VulkanGlobalDispatch::DestroyDeviceFn destroyDevice,
                      VulkanGlobalDispatch::DeviceWaitIdleFn deviceWaitIdle,
                      VulkanGlobalDispatch::DestroyAllocatorFn destroyAllocator,
                      VulkanDeviceInfo info) noexcept;

    std::shared_ptr<VulkanInstanceOwner> m_instance;
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    void* m_allocator{};
    std::vector<VkQueue> m_queues{};
    VulkanGlobalDispatch::DestroyDeviceFn m_destroyDevice{};
    VulkanGlobalDispatch::DeviceWaitIdleFn m_deviceWaitIdle{};
    VulkanGlobalDispatch::DestroyAllocatorFn m_destroyAllocator{};
    VulkanDeviceInfo m_info{};
};

struct VulkanSwapchainConfig final
{
    SelectedPresentationConfig presentation{};
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
    [[nodiscard]] VulkanSwapchainConfig GetConfig() const noexcept;
    [[nodiscard]] std::uint32_t GetFramebufferCount() const noexcept;

    void Reset() noexcept;

private:
    friend core::Result<VulkanSwapchainOwner> CreateVulkanSwapchainForTarget(
        const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface,
        const RenderDeviceQueuePlan& queuePlan, const RenderTargetDesc& desc);

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

struct VulkanFrameSlotResources final
{
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkSemaphore imageAvailableSemaphore{VK_NULL_HANDLE};
    VkSemaphore renderFinishedSemaphore{VK_NULL_HANDLE};
    VkFence inFlightFence{VK_NULL_HANDLE};

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return commandBuffer != VK_NULL_HANDLE && imageAvailableSemaphore != VK_NULL_HANDLE &&
               renderFinishedSemaphore != VK_NULL_HANDLE && inFlightFence != VK_NULL_HANDLE;
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
    [[nodiscard]] std::uint32_t GetSlotCount() const noexcept;
    [[nodiscard]] VulkanFrameSlotResources GetSlot(std::uint32_t slotIndex) const noexcept;

    void Reset() noexcept;

private:
    friend core::Result<VulkanFrameResourcesOwner> CreateVulkanFrameResourcesForTarget(
        const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
        const RenderDeviceQueuePlan& queuePlan, QueuedFrameLatency queuedLatency);

    VulkanFrameResourcesOwner(VkDevice device, VkCommandPool commandPool,
                              std::vector<VulkanFrameSlotResources> slots,
                              VulkanGlobalDispatch::DestroyCommandPoolFn destroyCommandPool,
                              VulkanGlobalDispatch::DestroySemaphoreFn destroySemaphore,
                              VulkanGlobalDispatch::DestroyFenceFn destroyFence) noexcept;

    VkDevice m_device{VK_NULL_HANDLE};
    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    std::vector<VulkanFrameSlotResources> m_slots{};
    VulkanGlobalDispatch::DestroyCommandPoolFn m_destroyCommandPool{};
    VulkanGlobalDispatch::DestroySemaphoreFn m_destroySemaphore{};
    VulkanGlobalDispatch::DestroyFenceFn m_destroyFence{};
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
[[nodiscard]] core::Result<RenderDeviceQueuePlan> ValidateVulkanDeviceSurfaceCompatibility(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface);
[[nodiscard]] core::Result<VulkanSwapchainConfig> SelectVulkanSwapchainConfig(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface,
    const RenderDeviceQueuePlan& queuePlan, const RenderTargetDesc& desc);
[[nodiscard]] core::Result<VulkanSwapchainOwner> CreateVulkanSwapchainForTarget(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, VkSurfaceKHR surface,
    const RenderDeviceQueuePlan& queuePlan, const RenderTargetDesc& desc);
[[nodiscard]] core::Result<VulkanFrameResourcesOwner> CreateVulkanFrameResourcesForTarget(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    const RenderDeviceQueuePlan& queuePlan, QueuedFrameLatency queuedLatency);
[[nodiscard]] core::Result<VulkanFramePresentationResult> ClearSubmitAndPresentVulkanFrame(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    const VulkanSwapchainOwner& swapchain, VulkanFrameResourcesOwner& frameResources,
    const RenderDeviceQueuePlan& queuePlan, std::uint32_t frameSlotIndex, ClearColor clearColor);
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
