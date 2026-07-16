#include <ponder/core/Assert.hpp>
#include <ponder/render/draw2d/Draw2DPacket.hpp>
#include <ponder/render/draw2d/Draw2DRectangleShaderInterface.hpp>

#include <Draw2DRectangleFragmentShader.hpp>
#include <Draw2DRectangleVertexShader.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <utility>

#include "VulkanBootstrap.hpp"
#include "VulkanDiagnostics.hpp"

namespace pond::render::detail
{
namespace
{
inline constexpr std::uint64_t kFnvOffset{0xcbf29ce484222325ULL};
inline constexpr std::uint64_t kFnvPrime{0x100000001b3ULL};
inline constexpr std::uint64_t kDraw2DFixedStateSignature{0x9d7f81ec2c9b6a41ULL};
inline constexpr std::string_view kDraw2DPipelineLabel{"pond.render.draw2d"};

[[nodiscard]] core::Error MakePipelineRenderError(RenderErrorCode code, std::string message)
{
    return core::Error{ToErrorCode(code), std::move(message)};
}

[[nodiscard]] const char* GetPipelineVkResultName(VkResult result) noexcept
{
    switch (result)
    {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_INVALID_SHADER_NV:
        return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_UNKNOWN:
        return "VK_ERROR_UNKNOWN";
    default:
        return "unknown VkResult";
    }
}

[[nodiscard]] RenderErrorCode MapPipelineFailure(VkResult result) noexcept
{
    switch (result)
    {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return RenderErrorCode::OutOfMemory;
    case VK_ERROR_DEVICE_LOST:
        return RenderErrorCode::DeviceLost;
    default:
        return RenderErrorCode::BackendFailure;
    }
}

[[nodiscard]] core::Error MakePipelineVulkanError(std::string_view operation, VkResult result)
{
    std::string message;
    message.reserve(operation.size() + 80U);
    message += "operation=";
    message += operation;
    message += " nativeCode=";
    message += std::to_string(static_cast<std::int64_t>(result));
    message += " symbolicName=";
    message += GetPipelineVkResultName(result);

    const RenderErrorCode code = MapPipelineFailure(result);
    VulkanDiagnosticScope::Record(BackendDiagnostic{.backend = RenderBackendKind::Vulkan,
                                                    .renderCode = code,
                                                    .nativeCode = static_cast<std::int64_t>(result),
                                                    .symbolicName = GetPipelineVkResultName(result),
                                                    .operation = std::string{operation},
                                                    .validationContext = message});
    return MakePipelineRenderError(code, std::move(message));
}

[[nodiscard]] constexpr std::uint64_t HashAppend(std::uint64_t hash, std::uint64_t value) noexcept
{
    for (std::uint32_t index = 0U; index < 8U; ++index)
    {
        hash ^= static_cast<std::uint8_t>((value >> (index * 8U)) & 0xffU);
        hash *= kFnvPrime;
    }

    return hash;
}

[[nodiscard]] bool HasPipelineCreateDispatch(const VulkanGlobalDispatch& dispatch) noexcept
{
    return dispatch.createShaderModule != nullptr && dispatch.destroyShaderModule != nullptr &&
           dispatch.createPipelineLayout != nullptr && dispatch.destroyPipelineLayout != nullptr &&
           dispatch.createGraphicsPipelines != nullptr && dispatch.destroyPipeline != nullptr;
}

[[nodiscard]] bool HasPipelineCommandDispatch(const VulkanGlobalDispatch& dispatch) noexcept
{
    return dispatch.cmdBindPipeline != nullptr && dispatch.cmdSetViewport != nullptr &&
           dispatch.cmdSetScissor != nullptr && dispatch.cmdBindVertexBuffers != nullptr &&
           dispatch.cmdBindIndexBuffer != nullptr && dispatch.cmdPushConstants != nullptr &&
           dispatch.cmdDrawIndexed != nullptr;
}

template <typename HandleType>
[[nodiscard]] std::uint64_t ToDebugHandle(HandleType handle) noexcept
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
void TryNamePipelineObject(const VulkanGlobalDispatch& dispatch, VkDevice device, HandleType handle,
                           VkObjectType objectType, const char* name) noexcept
{
    const std::uint64_t objectHandle = ToDebugHandle(handle);
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

void TryBeginPipelineLabel(const VulkanGlobalDispatch& dispatch,
                           VkCommandBuffer commandBuffer) noexcept
{
    if (dispatch.cmdBeginDebugUtilsLabel == nullptr || dispatch.cmdEndDebugUtilsLabel == nullptr ||
        commandBuffer == VK_NULL_HANDLE)
    {
        return;
    }

    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = kDraw2DPipelineLabel.data();
    label.color[0] = 0.2F;
    label.color[1] = 0.4F;
    label.color[2] = 0.9F;
    label.color[3] = 1.0F;
    dispatch.cmdBeginDebugUtilsLabel(commandBuffer, &label);
}

void TryEndPipelineLabel(const VulkanGlobalDispatch& dispatch,
                         VkCommandBuffer commandBuffer) noexcept
{
    if (dispatch.cmdBeginDebugUtilsLabel != nullptr && dispatch.cmdEndDebugUtilsLabel != nullptr &&
        commandBuffer != VK_NULL_HANDLE)
    {
        dispatch.cmdEndDebugUtilsLabel(commandBuffer);
    }
}

[[nodiscard]] VkFormat ToVkFormat(draw2d::Draw2DRectangleVertexAttributeFormat format) noexcept
{
    switch (format)
    {
    case draw2d::Draw2DRectangleVertexAttributeFormat::Float32x2:
        return VK_FORMAT_R32G32_SFLOAT;
    case draw2d::Draw2DRectangleVertexAttributeFormat::Unorm8x4:
        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    return VK_FORMAT_UNDEFINED;
}

[[nodiscard]] core::Result<VkShaderModule> CreateShaderModule(
    const VulkanGlobalDispatch& dispatch, VkDevice device,
    std::span<const std::uint32_t> spirvWords, std::string_view operation)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvWords.size_bytes();
    createInfo.pCode = spirvWords.data();

    VkShaderModule shaderModule{VK_NULL_HANDLE};
    const VkResult result =
        dispatch.createShaderModule(device, &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS)
    {
        return core::Result<VkShaderModule>::FromError(MakePipelineVulkanError(operation, result));
    }

    return core::Result<VkShaderModule>::FromValue(shaderModule);
}

[[nodiscard]] bool IsSupportedKey(const VulkanDraw2DPipelineCompatibilityKey& key) noexcept
{
    const bool supportedFormat = key.colorAttachmentFormat == VK_FORMAT_B8G8R8A8_SRGB ||
                                 key.colorAttachmentFormat == VK_FORMAT_R8G8B8A8_SRGB;
    const std::uint64_t expectedRenderPassSignature =
        ComputeVulkanDraw2DRenderPassCompatibilitySignature(
            key.colorAttachmentFormat, key.sampleCount, key.colorContract, key.subpass);
    return supportedFormat &&
           key.colorContract == VulkanDraw2DColorContract::LinearPremultipliedIntoOpaqueSdrSrgb &&
           key.sampleCount == VK_SAMPLE_COUNT_1_BIT &&
           key.renderPassCompatibilitySignature == expectedRenderPassSignature &&
           key.shaderSchemaFingerprint == draw2d::kDraw2DSchemaFingerprint &&
           key.fixedStateSignature == kDraw2DFixedStateSignature;
}
} // namespace

std::uint64_t GetVulkanDraw2DFixedStateSignature() noexcept
{
    return kDraw2DFixedStateSignature;
}

std::uint64_t ComputeVulkanDraw2DRenderPassCompatibilitySignature(
    VkFormat colorAttachmentFormat, VkSampleCountFlagBits sampleCount,
    VulkanDraw2DColorContract colorContract, std::uint32_t subpass) noexcept
{
    std::uint64_t hash{kFnvOffset};
    hash = HashAppend(hash, static_cast<std::uint64_t>(colorAttachmentFormat));
    hash = HashAppend(hash, static_cast<std::uint64_t>(sampleCount));
    hash = HashAppend(hash, static_cast<std::uint64_t>(colorContract));
    hash = HashAppend(hash, static_cast<std::uint64_t>(subpass));
    hash = HashAppend(hash, static_cast<std::uint64_t>(VK_ATTACHMENT_LOAD_OP_CLEAR));
    hash = HashAppend(hash, static_cast<std::uint64_t>(VK_ATTACHMENT_STORE_OP_STORE));
    hash = HashAppend(hash, static_cast<std::uint64_t>(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
    return hash;
}

VulkanDraw2DPipelineCompatibilityKey MakeVulkanDraw2DPipelineCompatibilityKey(
    const VulkanSwapchainConfig& config, std::uint32_t subpass) noexcept
{
    return VulkanDraw2DPipelineCompatibilityKey{
        .colorAttachmentFormat = config.format,
        .colorContract = VulkanDraw2DColorContract::LinearPremultipliedIntoOpaqueSdrSrgb,
        .sampleCount = VK_SAMPLE_COUNT_1_BIT,
        .renderPassCompatibilitySignature = ComputeVulkanDraw2DRenderPassCompatibilitySignature(
            config.format, VK_SAMPLE_COUNT_1_BIT,
            VulkanDraw2DColorContract::LinearPremultipliedIntoOpaqueSdrSrgb, subpass),
        .subpass = subpass,
        .shaderSchemaFingerprint = draw2d::kDraw2DSchemaFingerprint,
        .fixedStateSignature = kDraw2DFixedStateSignature};
}

VulkanDraw2DPipelineOwner::VulkanDraw2DPipelineOwner() noexcept = default;

VulkanDraw2DPipelineOwner::VulkanDraw2DPipelineOwner(
    VkDevice device, VkShaderModule vertexShader, VkShaderModule fragmentShader,
    VkPipelineLayout layout, VkPipeline pipeline,
    VulkanGlobalDispatch::DestroyShaderModuleFn destroyShaderModule,
    VulkanGlobalDispatch::DestroyPipelineLayoutFn destroyPipelineLayout,
    VulkanGlobalDispatch::DestroyPipelineFn destroyPipeline,
    VulkanDraw2DPipelineCompatibilityKey key,
    std::shared_ptr<VulkanDeviceChildLifetime> childLifetime) noexcept
    : m_device{device}, m_vertexShader{vertexShader}, m_fragmentShader{fragmentShader},
      m_layout{layout}, m_pipeline{pipeline}, m_destroyShaderModule{destroyShaderModule},
      m_destroyPipelineLayout{destroyPipelineLayout}, m_destroyPipeline{destroyPipeline},
      m_key{key}, m_childLifetime{std::move(childLifetime)},
      m_ownerThread{std::this_thread::get_id()}
{
}

VulkanDraw2DPipelineOwner::VulkanDraw2DPipelineOwner(VulkanDraw2DPipelineOwner&& other) noexcept
    : m_device{std::exchange(other.m_device, VK_NULL_HANDLE)},
      m_vertexShader{std::exchange(other.m_vertexShader, VK_NULL_HANDLE)},
      m_fragmentShader{std::exchange(other.m_fragmentShader, VK_NULL_HANDLE)},
      m_layout{std::exchange(other.m_layout, VK_NULL_HANDLE)},
      m_pipeline{std::exchange(other.m_pipeline, VK_NULL_HANDLE)},
      m_destroyShaderModule{std::exchange(other.m_destroyShaderModule, nullptr)},
      m_destroyPipelineLayout{std::exchange(other.m_destroyPipelineLayout, nullptr)},
      m_destroyPipeline{std::exchange(other.m_destroyPipeline, nullptr)}, m_key{other.m_key},
      m_childLifetime{std::move(other.m_childLifetime)},
      m_ownerThread{std::exchange(other.m_ownerThread, {})}
{
}

VulkanDraw2DPipelineOwner& VulkanDraw2DPipelineOwner::operator=(
    VulkanDraw2DPipelineOwner&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_vertexShader = std::exchange(other.m_vertexShader, VK_NULL_HANDLE);
        m_fragmentShader = std::exchange(other.m_fragmentShader, VK_NULL_HANDLE);
        m_layout = std::exchange(other.m_layout, VK_NULL_HANDLE);
        m_pipeline = std::exchange(other.m_pipeline, VK_NULL_HANDLE);
        m_destroyShaderModule = std::exchange(other.m_destroyShaderModule, nullptr);
        m_destroyPipelineLayout = std::exchange(other.m_destroyPipelineLayout, nullptr);
        m_destroyPipeline = std::exchange(other.m_destroyPipeline, nullptr);
        m_key = other.m_key;
        m_childLifetime = std::move(other.m_childLifetime);
        m_ownerThread = std::exchange(other.m_ownerThread, {});
    }

    return *this;
}

VulkanDraw2DPipelineOwner::~VulkanDraw2DPipelineOwner()
{
    Reset();
}

bool VulkanDraw2DPipelineOwner::IsValid() const noexcept
{
    return m_device != VK_NULL_HANDLE && m_vertexShader != VK_NULL_HANDLE &&
           m_fragmentShader != VK_NULL_HANDLE && m_layout != VK_NULL_HANDLE &&
           m_pipeline != VK_NULL_HANDLE && m_childLifetime != nullptr &&
           m_childLifetime->active.load(std::memory_order_acquire);
}

VkPipeline VulkanDraw2DPipelineOwner::GetPipeline() const noexcept
{
    return m_pipeline;
}

VkPipelineLayout VulkanDraw2DPipelineOwner::GetLayout() const noexcept
{
    return m_layout;
}

VkShaderModule VulkanDraw2DPipelineOwner::GetVertexShaderModule() const noexcept
{
    return m_vertexShader;
}

VkShaderModule VulkanDraw2DPipelineOwner::GetFragmentShaderModule() const noexcept
{
    return m_fragmentShader;
}

const VulkanDraw2DPipelineCompatibilityKey& VulkanDraw2DPipelineOwner::GetKey() const noexcept
{
    return m_key;
}

void VulkanDraw2DPipelineOwner::Reset() noexcept
{
    if (m_device == VK_NULL_HANDLE)
    {
        return;
    }

    PONDER_VERIFY(std::this_thread::get_id() == m_ownerThread,
                  "VulkanDraw2DPipelineOwner destruction must occur on its owner thread");
    PONDER_VERIFY(m_childLifetime != nullptr &&
                      m_childLifetime->active.load(std::memory_order_acquire),
                  "VulkanDraw2DPipelineOwner must be destroyed before its parent Vulkan device");

    if (m_pipeline != VK_NULL_HANDLE && m_destroyPipeline != nullptr)
    {
        m_destroyPipeline(m_device, m_pipeline, nullptr);
    }
    if (m_layout != VK_NULL_HANDLE && m_destroyPipelineLayout != nullptr)
    {
        m_destroyPipelineLayout(m_device, m_layout, nullptr);
    }
    if (m_fragmentShader != VK_NULL_HANDLE && m_destroyShaderModule != nullptr)
    {
        m_destroyShaderModule(m_device, m_fragmentShader, nullptr);
    }
    if (m_vertexShader != VK_NULL_HANDLE && m_destroyShaderModule != nullptr)
    {
        m_destroyShaderModule(m_device, m_vertexShader, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_vertexShader = VK_NULL_HANDLE;
    m_fragmentShader = VK_NULL_HANDLE;
    m_layout = VK_NULL_HANDLE;
    m_pipeline = VK_NULL_HANDLE;
    m_destroyShaderModule = nullptr;
    m_destroyPipelineLayout = nullptr;
    m_destroyPipeline = nullptr;
    m_key = {};
    const std::uint32_t previousChildren =
        m_childLifetime->draw2DPipelineChildren.fetch_sub(1U, std::memory_order_acq_rel);
    PONDER_VERIFY(previousChildren > 0U, "Vulkan Draw2D pipeline child lifetime count underflowed");
    m_childLifetime.reset();
    m_ownerThread = {};
}

VulkanDraw2DPipelineCache::VulkanDraw2DPipelineCache() noexcept = default;
VulkanDraw2DPipelineCache::VulkanDraw2DPipelineCache(VulkanDraw2DPipelineCache&& other) noexcept
    : m_device{std::exchange(other.m_device, VK_NULL_HANDLE)},
      m_pipeline{std::move(other.m_pipeline)}
{
}

VulkanDraw2DPipelineCache& VulkanDraw2DPipelineCache::operator=(
    VulkanDraw2DPipelineCache&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_pipeline = std::move(other.m_pipeline);
    }
    return *this;
}
VulkanDraw2DPipelineCache::~VulkanDraw2DPipelineCache() = default;

bool VulkanDraw2DPipelineCache::IsValid() const noexcept
{
    return m_pipeline != nullptr && m_pipeline->IsValid();
}

const VulkanDraw2DPipelineOwner& VulkanDraw2DPipelineCache::GetCurrentPipeline() const noexcept
{
    PONDER_VERIFY(IsValid(), "Draw2D pipeline cache access requires a live pipeline");
    return *m_pipeline;
}

std::shared_ptr<const VulkanDraw2DPipelineOwner> VulkanDraw2DPipelineCache::AcquireCurrentPipeline()
    const noexcept
{
    return m_pipeline;
}

core::Result<VulkanDraw2DPipelineCacheUpdate> VulkanDraw2DPipelineCache::GetOrCreate(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    const VulkanSwapchainOwner& swapchain, const VulkanDraw2DPipelineCompatibilityKey& key)
{
    if (m_device != VK_NULL_HANDLE && m_device != device.GetHandle())
    {
        return core::Result<VulkanDraw2DPipelineCacheUpdate>::FromError(MakePipelineRenderError(
            RenderErrorCode::InvalidState,
            "Draw2D pipeline cache cannot be shared across Vulkan devices."));
    }
    if (!swapchain.IsValid() ||
        key != MakeVulkanDraw2DPipelineCompatibilityKey(swapchain.GetConfig()))
    {
        return core::Result<VulkanDraw2DPipelineCacheUpdate>::FromError(
            MakePipelineRenderError(RenderErrorCode::InvalidArgument,
                                    "Draw2D pipeline key does not match its Vulkan swapchain."));
    }
    if (IsValid() && m_pipeline->GetKey() == key)
    {
        return core::Result<VulkanDraw2DPipelineCacheUpdate>::FromValue(
            VulkanDraw2DPipelineCacheUpdate{.cacheHit = true,
                                            .replaced = false,
                                            .pipeline = m_pipeline->GetPipeline(),
                                            .layout = m_pipeline->GetLayout()});
    }

    core::Result<VulkanDraw2DPipelineOwner> candidate =
        CreateVulkanDraw2DPipeline(dispatch, device, swapchain, key);
    if (!candidate)
    {
        return core::Result<VulkanDraw2DPipelineCacheUpdate>::FromError(
            std::move(candidate).GetError());
    }

    const bool replaced = IsValid();
    try
    {
        m_pipeline = std::make_shared<VulkanDraw2DPipelineOwner>(std::move(candidate).GetValue());
    }
    catch (const std::bad_alloc&)
    {
        return core::Result<VulkanDraw2DPipelineCacheUpdate>::FromError(MakePipelineRenderError(
            RenderErrorCode::OutOfMemory,
            "Draw2D pipeline cache could not allocate pipeline ownership state."));
    }
    m_device = device.GetHandle();
    return core::Result<VulkanDraw2DPipelineCacheUpdate>::FromValue(
        VulkanDraw2DPipelineCacheUpdate{.cacheHit = false,
                                        .replaced = replaced,
                                        .pipeline = m_pipeline->GetPipeline(),
                                        .layout = m_pipeline->GetLayout()});
}

void VulkanDraw2DPipelineCache::Reset() noexcept
{
    m_pipeline.reset();
    m_device = VK_NULL_HANDLE;
}

core::Result<VulkanDraw2DPipelineOwner> CreateVulkanDraw2DPipeline(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device,
    const VulkanSwapchainOwner& swapchain, const VulkanDraw2DPipelineCompatibilityKey& key)
{
    if (!device.IsValid())
    {
        return core::Result<VulkanDraw2DPipelineOwner>::FromError(
            MakePipelineRenderError(RenderErrorCode::InvalidState,
                                    "Draw2D pipeline creation requires a live Vulkan device."));
    }
    if (!swapchain.IsValid())
    {
        return core::Result<VulkanDraw2DPipelineOwner>::FromError(MakePipelineRenderError(
            RenderErrorCode::InvalidState, "Draw2D pipeline creation requires a live swapchain."));
    }
    if (!HasPipelineCreateDispatch(dispatch))
    {
        return core::Result<VulkanDraw2DPipelineOwner>::FromError(
            MakePipelineRenderError(RenderErrorCode::UnsupportedCapability,
                                    "Vulkan Draw2D pipeline dispatch is unavailable."));
    }
    if (!IsSupportedKey(key) ||
        key != MakeVulkanDraw2DPipelineCompatibilityKey(swapchain.GetConfig()))
    {
        return core::Result<VulkanDraw2DPipelineOwner>::FromError(
            MakePipelineRenderError(key.shaderSchemaFingerprint == draw2d::kDraw2DSchemaFingerprint
                                        ? RenderErrorCode::InvalidArgument
                                        : RenderErrorCode::InvalidState,
                                    "Draw2D pipeline compatibility key does not match the "
                                    "reflected packet/shader contract."));
    }

    const VkDevice vkDevice = device.GetHandle();
    const VkRenderPass renderPass = swapchain.GetRenderPass();
    VkShaderModule vertexShader{VK_NULL_HANDLE};
    VkShaderModule fragmentShader{VK_NULL_HANDLE};
    VkPipelineLayout layout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    const auto rollback = [&]() noexcept
    {
        if (pipeline != VK_NULL_HANDLE)
        {
            dispatch.destroyPipeline(vkDevice, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        if (layout != VK_NULL_HANDLE)
        {
            dispatch.destroyPipelineLayout(vkDevice, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
        if (fragmentShader != VK_NULL_HANDLE)
        {
            dispatch.destroyShaderModule(vkDevice, fragmentShader, nullptr);
            fragmentShader = VK_NULL_HANDLE;
        }
        if (vertexShader != VK_NULL_HANDLE)
        {
            dispatch.destroyShaderModule(vkDevice, vertexShader, nullptr);
            vertexShader = VK_NULL_HANDLE;
        }
    };

    core::Result<VkShaderModule> vertexShaderResult =
        CreateShaderModule(dispatch, vkDevice, shaders::kDraw2DRectangleVertexShaderSpirvWords,
                           "vkCreateShaderModule.Draw2DRectangleVertex");
    if (!vertexShaderResult)
    {
        return core::Result<VulkanDraw2DPipelineOwner>::FromError(
            std::move(vertexShaderResult).GetError());
    }
    vertexShader = vertexShaderResult.GetValue();

    core::Result<VkShaderModule> fragmentShaderResult =
        CreateShaderModule(dispatch, vkDevice, shaders::kDraw2DRectangleFragmentShaderSpirvWords,
                           "vkCreateShaderModule.Draw2DRectangleFragment");
    if (!fragmentShaderResult)
    {
        rollback();
        return core::Result<VulkanDraw2DPipelineOwner>::FromError(
            std::move(fragmentShaderResult).GetError());
    }
    fragmentShader = fragmentShaderResult.GetValue();

    const VkPushConstantRange pushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                                .offset = draw2d::kDraw2DRectangleConstantsOffset,
                                                .size = draw2d::kDraw2DRectangleConstantsSize};
    VkPipelineLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.setLayoutCount = 0U;
    layoutCreateInfo.pSetLayouts = nullptr;
    layoutCreateInfo.pushConstantRangeCount = 1U;
    layoutCreateInfo.pPushConstantRanges = &pushConstantRange;

    VkResult result = dispatch.createPipelineLayout(vkDevice, &layoutCreateInfo, nullptr, &layout);
    if (result != VK_SUCCESS)
    {
        rollback();
        return core::Result<VulkanDraw2DPipelineOwner>::FromError(
            MakePipelineVulkanError("vkCreatePipelineLayout.Draw2D", result));
    }

    const std::array<VkPipelineShaderStageCreateInfo, 2U> stages{
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexShader,
            .pName = shaders::kDraw2DRectangleVertexShaderEntryPoint.data()},
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentShader,
            .pName = shaders::kDraw2DRectangleFragmentShaderEntryPoint.data()}};

    const VkVertexInputBindingDescription binding{.binding = draw2d::kDraw2DRectangleVertexBinding,
                                                  .stride = draw2d::kDraw2DRectangleVertexStride,
                                                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    const std::array<VkVertexInputAttributeDescription, 2U> attributes{
        VkVertexInputAttributeDescription{
            .location = draw2d::kDraw2DRectanglePositionAttribute.location,
            .binding = draw2d::kDraw2DRectanglePositionAttribute.binding,
            .format = ToVkFormat(draw2d::kDraw2DRectanglePositionAttribute.format),
            .offset = draw2d::kDraw2DRectanglePositionAttribute.offset},
        VkVertexInputAttributeDescription{
            .location = draw2d::kDraw2DRectangleColorAttribute.location,
            .binding = draw2d::kDraw2DRectangleColorAttribute.binding,
            .format = ToVkFormat(draw2d::kDraw2DRectangleColorAttribute.format),
            .offset = draw2d::kDraw2DRectangleColorAttribute.offset}};
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1U;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1U;
    viewportState.scissorCount = 1U;

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.depthClampEnable = VK_FALSE;
    rasterization.rasterizerDiscardEnable = VK_FALSE;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.depthBiasEnable = VK_FALSE;
    rasterization.lineWidth = 1.0F;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    const VkPipelineColorBlendAttachmentState blendAttachment{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = VK_FALSE;
    colorBlend.attachmentCount = 1U;
    colorBlend.pAttachments = &blendAttachment;

    const std::array<VkDynamicState, 2U> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT,
                                                       VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = static_cast<std::uint32_t>(stages.size());
    pipelineCreateInfo.pStages = stages.data();
    pipelineCreateInfo.pVertexInputState = &vertexInput;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterization;
    pipelineCreateInfo.pMultisampleState = &multisample;
    pipelineCreateInfo.pDepthStencilState = &depthStencil;
    pipelineCreateInfo.pColorBlendState = &colorBlend;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.layout = layout;
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.subpass = key.subpass;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = -1;

    result = dispatch.createGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1U, &pipelineCreateInfo,
                                              nullptr, &pipeline);
    if (result != VK_SUCCESS)
    {
        rollback();
        return core::Result<VulkanDraw2DPipelineOwner>::FromError(
            MakePipelineVulkanError("vkCreateGraphicsPipelines.Draw2D", result));
    }

    TryNamePipelineObject(dispatch, vkDevice, vertexShader, VK_OBJECT_TYPE_SHADER_MODULE,
                          "pond.render.draw2d.rectangle.vertexShader");
    TryNamePipelineObject(dispatch, vkDevice, fragmentShader, VK_OBJECT_TYPE_SHADER_MODULE,
                          "pond.render.draw2d.rectangle.fragmentShader");
    TryNamePipelineObject(dispatch, vkDevice, layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                          "pond.render.draw2d.pipelineLayout");
    TryNamePipelineObject(dispatch, vkDevice, pipeline, VK_OBJECT_TYPE_PIPELINE,
                          "pond.render.draw2d.pipeline");

    std::shared_ptr<VulkanDeviceChildLifetime> childLifetime = device.GetChildLifetime();
    if (childLifetime == nullptr || !childLifetime->active.load(std::memory_order_acquire))
    {
        rollback();
        return core::Result<VulkanDraw2DPipelineOwner>::FromError(MakePipelineRenderError(
            RenderErrorCode::InvalidState, "Draw2D pipeline parent device is no longer live."));
    }
    const std::uint32_t previousChildren =
        childLifetime->draw2DPipelineChildren.fetch_add(1U, std::memory_order_acq_rel);
    if (previousChildren == std::numeric_limits<std::uint32_t>::max())
    {
        childLifetime->draw2DPipelineChildren.fetch_sub(1U, std::memory_order_acq_rel);
        rollback();
        return core::Result<VulkanDraw2DPipelineOwner>::FromError(MakePipelineRenderError(
            RenderErrorCode::InvalidState, "Draw2D pipeline child lifetime count is exhausted."));
    }

    return core::Result<VulkanDraw2DPipelineOwner>::FromValue(VulkanDraw2DPipelineOwner{
        vkDevice, std::exchange(vertexShader, VK_NULL_HANDLE),
        std::exchange(fragmentShader, VK_NULL_HANDLE), std::exchange(layout, VK_NULL_HANDLE),
        std::exchange(pipeline, VK_NULL_HANDLE), dispatch.destroyShaderModule,
        dispatch.destroyPipelineLayout, dispatch.destroyPipeline, key, std::move(childLifetime)});
}

core::VoidResult RecordVulkanDraw2DPipelineCommands(
    const VulkanGlobalDispatch& dispatch, VkCommandBuffer commandBuffer,
    const VulkanDraw2DPipelineOwner& pipeline, VkBuffer vertexBuffer, VkDeviceSize vertexOffset,
    VkBuffer indexBuffer, VkDeviceSize indexOffset, VkExtent2D extent,
    VkExtent2D maximumViewportExtent, std::span<const draw2d::Draw2DDrawRecord> draws)
{
    if (!pipeline.IsValid())
    {
        return core::VoidResult::FromError(MakePipelineRenderError(
            RenderErrorCode::InvalidState, "Draw2D command recording requires a live pipeline."));
    }
    if (commandBuffer == VK_NULL_HANDLE || vertexBuffer == VK_NULL_HANDLE ||
        indexBuffer == VK_NULL_HANDLE || extent.width == 0U || extent.height == 0U)
    {
        return core::VoidResult::FromError(MakePipelineRenderError(
            RenderErrorCode::InvalidArgument, "Draw2D command recording arguments are invalid."));
    }
    if (!HasPipelineCommandDispatch(dispatch))
    {
        return core::VoidResult::FromError(
            MakePipelineRenderError(RenderErrorCode::UnsupportedCapability,
                                    "Vulkan Draw2D command dispatch is unavailable."));
    }
    if (maximumViewportExtent.width == 0U || maximumViewportExtent.height == 0U ||
        extent.width > maximumViewportExtent.width || extent.height > maximumViewportExtent.height)
    {
        return core::VoidResult::FromError(MakePipelineRenderError(
            RenderErrorCode::UnsupportedCapability,
            "Draw2D framebuffer extent exceeds the Vulkan device viewport limits."));
    }
    if (draws.empty() ||
        std::ranges::any_of(
            draws,
            [extent](const draw2d::Draw2DDrawRecord& draw) noexcept
            {
                constexpr std::uint32_t kMaximumVulkanScissorOffset{
                    static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())};
                return draw.scissor.left > kMaximumVulkanScissorOffset ||
                       draw.scissor.top > kMaximumVulkanScissorOffset ||
                       draw.scissor.left >= draw.scissor.right ||
                       draw.scissor.top >= draw.scissor.bottom ||
                       draw.scissor.right > extent.width || draw.scissor.bottom > extent.height;
            }))
    {
        return core::VoidResult::FromError(
            MakePipelineRenderError(RenderErrorCode::InvalidArgument,
                                    "Draw2D command scissors are not representable by Vulkan."));
    }

    TryBeginPipelineLabel(dispatch, commandBuffer);
    dispatch.cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             pipeline.GetPipeline());

    const VkViewport viewport{.x = 0.0F,
                              .y = 0.0F,
                              .width = static_cast<float>(extent.width),
                              .height = static_cast<float>(extent.height),
                              .minDepth = 0.0F,
                              .maxDepth = 1.0F};
    dispatch.cmdSetViewport(commandBuffer, 0U, 1U, &viewport);
    dispatch.cmdBindVertexBuffers(commandBuffer, draw2d::kDraw2DRectangleVertexBinding, 1U,
                                  &vertexBuffer, &vertexOffset);
    dispatch.cmdBindIndexBuffer(commandBuffer, indexBuffer, indexOffset, VK_INDEX_TYPE_UINT32);

    const std::array<std::uint32_t, 2U> constants{extent.width, extent.height};
    dispatch.cmdPushConstants(commandBuffer, pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                              draw2d::kDraw2DRectangleConstantsOffset,
                              draw2d::kDraw2DRectangleConstantsSize, constants.data());

    for (const draw2d::Draw2DDrawRecord& draw : draws)
    {
        const VkRect2D scissor{
            .offset = VkOffset2D{.x = static_cast<std::int32_t>(draw.scissor.left),
                                 .y = static_cast<std::int32_t>(draw.scissor.top)},
            .extent = VkExtent2D{.width = draw.scissor.right - draw.scissor.left,
                                 .height = draw.scissor.bottom - draw.scissor.top}};
        dispatch.cmdSetScissor(commandBuffer, 0U, 1U, &scissor);
        dispatch.cmdDrawIndexed(commandBuffer, draw.indexCount, 1U, draw.firstIndex,
                                draw.baseVertex, 0U);
    }

    TryEndPipelineLabel(dispatch, commandBuffer);
    return core::VoidResult::Success();
}
} // namespace pond::render::detail
