#include <ponder/core/Assert.hpp>
#include <ponder/render/RenderError.hpp>

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

#include "VulkanBootstrap.hpp"

namespace pond::render::detail
{
namespace
{
constexpr VkDeviceSize kInitialUploadCapacity{4U * 1024U};
constexpr VkDeviceSize kIndexAlignment{sizeof(std::uint32_t)};

[[nodiscard]] core::Error MakeUploadError(
    RenderErrorCode code, std::string message,
    std::source_location location = std::source_location::current())
{
    return core::Error{ToErrorCode(code), std::move(message), location};
}

[[nodiscard]] RenderErrorCode MapUploadFailure(VkResult result) noexcept
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

[[nodiscard]] core::Error MakeUploadVulkanError(std::string_view operation, VkResult result)
{
    return MakeUploadError(MapUploadFailure(result),
                           std::string{operation} + " failed with VkResult " +
                               std::to_string(static_cast<std::int32_t>(result)) + ".");
}

[[nodiscard]] bool CheckedAdd(VkDeviceSize lhs, VkDeviceSize rhs, VkDeviceSize& result) noexcept
{
    if (rhs > std::numeric_limits<VkDeviceSize>::max() - lhs)
    {
        return false;
    }
    result = lhs + rhs;
    return true;
}

[[nodiscard]] bool AlignUp(VkDeviceSize value, VkDeviceSize alignment,
                           VkDeviceSize& result) noexcept
{
    if (alignment == 0U)
    {
        return false;
    }
    const VkDeviceSize remainder = value % alignment;
    return remainder == 0U ? (result = value, true)
                           : CheckedAdd(value, alignment - remainder, result);
}

[[nodiscard]] VkDeviceSize SelectGrowthCapacity(VkDeviceSize current, VkDeviceSize required,
                                                VkDeviceSize maximum) noexcept
{
    VkDeviceSize capacity = current == 0U ? std::min(kInitialUploadCapacity, maximum) : current;
    while (capacity < required)
    {
        if (capacity > maximum / 2U)
        {
            capacity = maximum;
        }
        else
        {
            capacity *= 2U;
        }
    }
    return capacity;
}
} // namespace

core::Result<VulkanDraw2DUploadLayout> ComputeVulkanDraw2DUploadLayout(
    VkDeviceSize vertexBytes, VkDeviceSize indexBytes, VkDeviceSize maximumUploadBytes)
{
    VkDeviceSize indexOffset{};
    VkDeviceSize requiredCapacity{};
    if (vertexBytes == 0U || indexBytes == 0U || maximumUploadBytes == 0U ||
        !AlignUp(vertexBytes, kIndexAlignment, indexOffset) ||
        !CheckedAdd(indexOffset, indexBytes, requiredCapacity) ||
        requiredCapacity > maximumUploadBytes)
    {
        return core::Result<VulkanDraw2DUploadLayout>::FromError(
            MakeUploadError(RenderErrorCode::InvalidArgument,
                            "Draw2D upload byte size overflows or exceeds the hard upload limit."));
    }

    return core::Result<VulkanDraw2DUploadLayout>::FromValue(
        VulkanDraw2DUploadLayout{vertexBytes, indexOffset, indexBytes, requiredCapacity});
}

VulkanDraw2DUploadBufferOwner::VulkanDraw2DUploadBufferOwner() noexcept = default;

VulkanDraw2DUploadBufferOwner::VulkanDraw2DUploadBufferOwner(
    void* allocator, VkBuffer buffer, void* allocation, std::byte* mappedData,
    VkDeviceSize capacity, VkDeviceSize allocationSize, bool coherent, std::uint64_t generation,
    VulkanGlobalDispatch::DestroyBufferFn destroyBuffer,
    VulkanGlobalDispatch::UnmapMemoryFn unmapMemory) noexcept
    : m_allocator{allocator}, m_buffer{buffer}, m_allocation{allocation}, m_mappedData{mappedData},
      m_capacity{capacity}, m_allocationSize{allocationSize}, m_coherent{coherent},
      m_generation{generation}, m_destroyBuffer{destroyBuffer}, m_unmapMemory{unmapMemory},
      m_ownerThread{std::this_thread::get_id()}
{
}

VulkanDraw2DUploadBufferOwner::VulkanDraw2DUploadBufferOwner(
    VulkanDraw2DUploadBufferOwner&& other) noexcept
    : m_allocator{std::exchange(other.m_allocator, nullptr)},
      m_buffer{std::exchange(other.m_buffer, VK_NULL_HANDLE)},
      m_allocation{std::exchange(other.m_allocation, nullptr)},
      m_mappedData{std::exchange(other.m_mappedData, nullptr)},
      m_capacity{std::exchange(other.m_capacity, 0U)},
      m_allocationSize{std::exchange(other.m_allocationSize, 0U)},
      m_coherent{std::exchange(other.m_coherent, false)},
      m_generation{std::exchange(other.m_generation, 0U)},
      m_destroyBuffer{std::exchange(other.m_destroyBuffer, nullptr)},
      m_unmapMemory{std::exchange(other.m_unmapMemory, nullptr)},
      m_ownerThread{std::exchange(other.m_ownerThread, {})}
{
}

VulkanDraw2DUploadBufferOwner& VulkanDraw2DUploadBufferOwner::operator=(
    VulkanDraw2DUploadBufferOwner&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_allocator = std::exchange(other.m_allocator, nullptr);
        m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
        m_allocation = std::exchange(other.m_allocation, nullptr);
        m_mappedData = std::exchange(other.m_mappedData, nullptr);
        m_capacity = std::exchange(other.m_capacity, 0U);
        m_allocationSize = std::exchange(other.m_allocationSize, 0U);
        m_coherent = std::exchange(other.m_coherent, false);
        m_generation = std::exchange(other.m_generation, 0U);
        m_destroyBuffer = std::exchange(other.m_destroyBuffer, nullptr);
        m_unmapMemory = std::exchange(other.m_unmapMemory, nullptr);
        m_ownerThread = std::exchange(other.m_ownerThread, {});
    }
    return *this;
}

VulkanDraw2DUploadBufferOwner::~VulkanDraw2DUploadBufferOwner()
{
    Reset();
}

bool VulkanDraw2DUploadBufferOwner::IsValid() const noexcept
{
    return m_allocator != nullptr && m_buffer != VK_NULL_HANDLE && m_allocation != nullptr &&
           m_mappedData != nullptr && m_capacity > 0U && m_generation > 0U &&
           m_allocationSize >= m_capacity && m_destroyBuffer != nullptr && m_unmapMemory != nullptr;
}

VkBuffer VulkanDraw2DUploadBufferOwner::GetBuffer() const noexcept
{
    return m_buffer;
}

std::byte* VulkanDraw2DUploadBufferOwner::GetMappedData() const noexcept
{
    return m_mappedData;
}

VkDeviceSize VulkanDraw2DUploadBufferOwner::GetCapacity() const noexcept
{
    return m_capacity;
}

bool VulkanDraw2DUploadBufferOwner::IsCoherent() const noexcept
{
    return m_coherent;
}

std::uint64_t VulkanDraw2DUploadBufferOwner::GetGeneration() const noexcept
{
    return m_generation;
}

void VulkanDraw2DUploadBufferOwner::Reset() noexcept
{
    if (m_allocator == nullptr)
    {
        return;
    }

    PONDER_VERIFY(std::this_thread::get_id() == m_ownerThread,
                  "VulkanDraw2DUploadBufferOwner destruction must occur on its owner thread");
    if (m_allocation != nullptr && m_unmapMemory != nullptr)
    {
        m_unmapMemory(m_allocator, m_allocation);
    }
    if (m_buffer != VK_NULL_HANDLE && m_allocation != nullptr && m_destroyBuffer != nullptr)
    {
        m_destroyBuffer(m_allocator, m_buffer, m_allocation);
    }

    m_allocator = nullptr;
    m_buffer = VK_NULL_HANDLE;
    m_allocation = nullptr;
    m_mappedData = nullptr;
    m_capacity = 0U;
    m_allocationSize = 0U;
    m_coherent = false;
    m_generation = 0U;
    m_destroyBuffer = nullptr;
    m_unmapMemory = nullptr;
    m_ownerThread = {};
}

VulkanDraw2DUploadArena::VulkanDraw2DUploadArena() noexcept = default;

VulkanDraw2DUploadArena::VulkanDraw2DUploadArena(VulkanGlobalDispatch dispatch, void* allocator,
                                                 VkDeviceSize nonCoherentAtomSize,
                                                 VkDeviceSize maximumUploadBytes,
                                                 std::vector<Slot> slots) noexcept
    : m_dispatch{dispatch}, m_allocator{allocator},
      m_nonCoherentAtomSize{std::max<VkDeviceSize>(nonCoherentAtomSize, 1U)},
      m_maximumUploadBytes{maximumUploadBytes}, m_slots{std::move(slots)},
      m_ownerThread{std::this_thread::get_id()}
{
}

VulkanDraw2DUploadArena::VulkanDraw2DUploadArena(VulkanDraw2DUploadArena&& other) noexcept
    : m_dispatch{std::exchange(other.m_dispatch, {})},
      m_allocator{std::exchange(other.m_allocator, nullptr)},
      m_nonCoherentAtomSize{std::exchange(other.m_nonCoherentAtomSize, 1U)},
      m_maximumUploadBytes{std::exchange(other.m_maximumUploadBytes, 0U)},
      m_slots{std::move(other.m_slots)}, m_stats{std::exchange(other.m_stats, {})},
      m_ownerThread{std::exchange(other.m_ownerThread, {})}
{
}

VulkanDraw2DUploadArena& VulkanDraw2DUploadArena::operator=(
    VulkanDraw2DUploadArena&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_dispatch = std::exchange(other.m_dispatch, {});
        m_allocator = std::exchange(other.m_allocator, nullptr);
        m_nonCoherentAtomSize = std::exchange(other.m_nonCoherentAtomSize, 1U);
        m_maximumUploadBytes = std::exchange(other.m_maximumUploadBytes, 0U);
        m_slots = std::move(other.m_slots);
        m_stats = std::exchange(other.m_stats, {});
        m_ownerThread = std::exchange(other.m_ownerThread, {});
    }
    return *this;
}

VulkanDraw2DUploadArena::~VulkanDraw2DUploadArena()
{
    Reset();
}

core::Result<VulkanDraw2DUploadArena> VulkanDraw2DUploadArena::Create(
    const VulkanGlobalDispatch& dispatch, const VulkanDeviceOwner& device, std::uint32_t slotCount,
    VkDeviceSize maximumUploadBytes)
{
    if (!device.IsValid() || device.GetAllocator() == nullptr || slotCount == 0U ||
        maximumUploadBytes == 0U)
    {
        return core::Result<VulkanDraw2DUploadArena>::FromError(MakeUploadError(
            RenderErrorCode::InvalidArgument,
            "Draw2D upload arena creation requires a live device, slots, and byte limit."));
    }
    if (dispatch.createBuffer == nullptr || dispatch.destroyBuffer == nullptr ||
        dispatch.mapMemory == nullptr || dispatch.unmapMemory == nullptr ||
        dispatch.flushAllocation == nullptr)
    {
        return core::Result<VulkanDraw2DUploadArena>::FromError(
            MakeUploadError(RenderErrorCode::UnsupportedCapability,
                            "Draw2D upload arena creation requires complete VMA buffer dispatch."));
    }

    try
    {
        std::vector<Slot> slots(slotCount);
        return core::Result<VulkanDraw2DUploadArena>::FromValue(VulkanDraw2DUploadArena{
            dispatch, device.GetAllocator(), device.GetNonCoherentAtomSize(), maximumUploadBytes,
            std::move(slots)});
    }
    catch (const std::bad_alloc&)
    {
        return core::Result<VulkanDraw2DUploadArena>::FromError(
            MakeUploadError(RenderErrorCode::OutOfMemory,
                            "Draw2D upload arena could not allocate completion-slot bookkeeping."));
    }
    catch (const std::length_error&)
    {
        return core::Result<VulkanDraw2DUploadArena>::FromError(MakeUploadError(
            RenderErrorCode::InvalidArgument,
            "Draw2D upload arena received an unrepresentable completion-slot count."));
    }
}

bool VulkanDraw2DUploadArena::IsValid() const noexcept
{
    return m_allocator != nullptr && m_maximumUploadBytes > 0U && !m_slots.empty() &&
           m_dispatch.createBuffer != nullptr && m_dispatch.destroyBuffer != nullptr &&
           m_dispatch.mapMemory != nullptr && m_dispatch.unmapMemory != nullptr &&
           m_dispatch.flushAllocation != nullptr;
}

std::uint32_t VulkanDraw2DUploadArena::GetSlotCount() const noexcept
{
    return static_cast<std::uint32_t>(m_slots.size());
}

VulkanDraw2DUploadSlotSnapshot VulkanDraw2DUploadArena::GetSlotSnapshot(
    std::uint32_t slotIndex) const noexcept
{
    if (slotIndex >= m_slots.size())
    {
        return {};
    }
    const Slot& slot = m_slots[slotIndex];
    return VulkanDraw2DUploadSlotSnapshot{.state = slot.state,
                                          .generation = slot.buffer.GetGeneration(),
                                          .capacityBytes = slot.buffer.GetCapacity(),
                                          .reservedBytes = slot.reservedBytes,
                                          .coherent = slot.buffer.IsCoherent()};
}

VulkanDraw2DUploadStats VulkanDraw2DUploadArena::GetStats() const noexcept
{
    return m_stats;
}

core::Result<VulkanDraw2DUploadBufferOwner> VulkanDraw2DUploadArena::CreateBuffer(
    VkDeviceSize capacity, std::uint64_t generation)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = capacity;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    const VulkanHostBufferAllocationRequest allocationRequest{};
    VulkanHostBufferAllocationResult allocationResult{};
    VkBuffer buffer{VK_NULL_HANDLE};
    void* allocation{};
    VkResult result = m_dispatch.createBuffer(m_allocator, &bufferInfo, &allocationRequest, &buffer,
                                              &allocation, &allocationResult);
    if (result != VK_SUCCESS)
    {
        return core::Result<VulkanDraw2DUploadBufferOwner>::FromError(
            MakeUploadVulkanError("vmaCreateBuffer.Draw2DUpload", result));
    }
    if (buffer == VK_NULL_HANDLE || allocation == nullptr ||
        allocationResult.allocationSize < capacity ||
        (allocationResult.memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0U)
    {
        if (buffer != VK_NULL_HANDLE && allocation != nullptr)
        {
            m_dispatch.destroyBuffer(m_allocator, buffer, allocation);
        }
        return core::Result<VulkanDraw2DUploadBufferOwner>::FromError(
            MakeUploadError(RenderErrorCode::UnsupportedCapability,
                            "VMA did not provide host-visible Draw2D upload memory."));
    }

    void* mappedData{};
    result = m_dispatch.mapMemory(m_allocator, allocation, &mappedData);
    if (result != VK_SUCCESS || mappedData == nullptr)
    {
        if (result == VK_SUCCESS)
        {
            m_dispatch.unmapMemory(m_allocator, allocation);
        }
        m_dispatch.destroyBuffer(m_allocator, buffer, allocation);
        return core::Result<VulkanDraw2DUploadBufferOwner>::FromError(
            result == VK_SUCCESS ? MakeUploadError(RenderErrorCode::BackendFailure,
                                                   "VMA returned a null Draw2D upload mapping.")
                                 : MakeUploadVulkanError("vmaMapMemory.Draw2DUpload", result));
    }

    const bool coherent =
        (allocationResult.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0U;
    return core::Result<VulkanDraw2DUploadBufferOwner>::FromValue(VulkanDraw2DUploadBufferOwner{
        m_allocator, buffer, allocation, static_cast<std::byte*>(mappedData), capacity,
        allocationResult.allocationSize, coherent, generation, m_dispatch.destroyBuffer,
        m_dispatch.unmapMemory});
}

core::VoidResult VulkanDraw2DUploadArena::CopyAndFlush(VulkanDraw2DUploadBufferOwner& buffer,
                                                       VkDeviceSize vertexOffset,
                                                       std::span<const std::byte> vertices,
                                                       VkDeviceSize indexOffset,
                                                       std::span<const std::byte> indices)
{
    const auto copyRange = [&buffer](VkDeviceSize offset, std::span<const std::byte> bytes)
    {
        if (!bytes.empty())
        {
            std::memcpy(buffer.GetMappedData() + static_cast<std::size_t>(offset), bytes.data(),
                        bytes.size());
        }
    };
    copyRange(vertexOffset, vertices);
    copyRange(indexOffset, indices);

    if (buffer.IsCoherent())
    {
        return core::VoidResult::Success();
    }

    const auto flushRange = [this, &buffer](VkDeviceSize offset,
                                            VkDeviceSize size) -> core::VoidResult
    {
        if (size == 0U)
        {
            return core::VoidResult::Success();
        }
        VkDeviceSize writeEnd{};
        if (!CheckedAdd(offset, size, writeEnd) || writeEnd > buffer.GetCapacity())
        {
            return core::VoidResult::FromError(
                MakeUploadError(RenderErrorCode::InvalidState,
                                "Draw2D upload flush range exceeds its mapped allocation."));
        }

        const VkDeviceSize flushOffset =
            offset - (offset % std::max<VkDeviceSize>(m_nonCoherentAtomSize, 1U));
        VkDeviceSize flushEnd{};
        if (!AlignUp(writeEnd, std::max<VkDeviceSize>(m_nonCoherentAtomSize, 1U), flushEnd))
        {
            flushEnd = buffer.m_allocationSize;
        }
        flushEnd = std::min(flushEnd, buffer.m_allocationSize);
        const VkDeviceSize flushSize = flushEnd - flushOffset;
        const VkResult result =
            m_dispatch.flushAllocation(m_allocator, buffer.m_allocation, flushOffset, flushSize);
        if (result != VK_SUCCESS)
        {
            return core::VoidResult::FromError(
                MakeUploadVulkanError("vmaFlushAllocation.Draw2DUpload", result));
        }
        if (m_stats.flushCount < std::numeric_limits<std::uint64_t>::max())
        {
            ++m_stats.flushCount;
        }
        return core::VoidResult::Success();
    };

    if (core::VoidResult flush = flushRange(vertexOffset, vertices.size()); !flush)
    {
        return flush;
    }
    return flushRange(indexOffset, indices.size());
}

core::Result<VulkanDraw2DUploadReservation> VulkanDraw2DUploadArena::ReserveAndUpload(
    std::uint32_t slotIndex, std::span<const std::byte> vertices,
    std::span<const std::byte> indices)
{
    if (!IsValid() || std::this_thread::get_id() != m_ownerThread)
    {
        return core::Result<VulkanDraw2DUploadReservation>::FromError(
            MakeUploadError(RenderErrorCode::InvalidState,
                            "Draw2D upload reservation requires its live owner-thread arena."));
    }
    if (slotIndex >= m_slots.size() || vertices.empty() || indices.empty())
    {
        return core::Result<VulkanDraw2DUploadReservation>::FromError(MakeUploadError(
            RenderErrorCode::InvalidArgument,
            "Draw2D upload reservation requires a valid slot and non-empty streams."));
    }

    Slot& slot = m_slots[slotIndex];
    if (slot.state != VulkanDraw2DUploadSlotState::Idle)
    {
        return core::Result<VulkanDraw2DUploadReservation>::FromError(
            MakeUploadError(RenderErrorCode::InvalidState,
                            "Draw2D upload completion slot already owns unfinished GPU work."));
    }

    core::Result<VulkanDraw2DUploadLayout> layout = ComputeVulkanDraw2DUploadLayout(
        static_cast<VkDeviceSize>(vertices.size()), static_cast<VkDeviceSize>(indices.size()),
        m_maximumUploadBytes);
    if (!layout)
    {
        return core::Result<VulkanDraw2DUploadReservation>::FromError(std::move(layout).GetError());
    }
    const VulkanDraw2DUploadLayout uploadLayout = layout.GetValue();

    VulkanDraw2DUploadBufferOwner candidate;
    VulkanDraw2DUploadBufferOwner* destination = &slot.buffer;
    const bool grows =
        !slot.buffer.IsValid() || slot.buffer.GetCapacity() < uploadLayout.uploadBytes;
    if (grows)
    {
        if (m_stats.generationCount == std::numeric_limits<std::uint64_t>::max())
        {
            return core::Result<VulkanDraw2DUploadReservation>::FromError(MakeUploadError(
                RenderErrorCode::InvalidState, "Draw2D upload generation sequence is exhausted."));
        }
        const VkDeviceSize capacity = SelectGrowthCapacity(
            slot.buffer.GetCapacity(), uploadLayout.uploadBytes, m_maximumUploadBytes);
        core::Result<VulkanDraw2DUploadBufferOwner> created =
            CreateBuffer(capacity, m_stats.generationCount + 1U);
        if (!created)
        {
            return core::Result<VulkanDraw2DUploadReservation>::FromError(
                std::move(created).GetError());
        }
        candidate = std::move(created).GetValue();
        destination = &candidate;
    }

    if (core::VoidResult copied =
            CopyAndFlush(*destination, 0U, vertices, uploadLayout.indexOffset, indices);
        !copied)
    {
        return core::Result<VulkanDraw2DUploadReservation>::FromError(std::move(copied).GetError());
    }

    if (grows)
    {
        if (slot.buffer.IsValid())
        {
            m_stats.currentCapacityBytes -= slot.buffer.GetCapacity();
            ++m_stats.growthCount;
            ++m_stats.retirementCount;
        }
        slot.buffer = std::move(candidate);
        m_stats.currentCapacityBytes += slot.buffer.GetCapacity();
        ++m_stats.allocationCount;
        m_stats.generationCount = slot.buffer.GetGeneration();
    }

    slot.state = VulkanDraw2DUploadSlotState::Reserved;
    slot.reservedBytes = uploadLayout.uploadBytes;
    m_stats.reservedBytes += uploadLayout.uploadBytes;
    m_stats.highWaterReservedBytes =
        std::max(m_stats.highWaterReservedBytes, m_stats.reservedBytes + m_stats.uploadedBytes);

    return core::Result<VulkanDraw2DUploadReservation>::FromValue(
        VulkanDraw2DUploadReservation{.slotIndex = slotIndex,
                                      .generation = slot.buffer.GetGeneration(),
                                      .buffer = slot.buffer.GetBuffer(),
                                      .vertexOffset = 0U,
                                      .vertexBytes = uploadLayout.vertexBytes,
                                      .indexOffset = uploadLayout.indexOffset,
                                      .indexBytes = uploadLayout.indexBytes,
                                      .uploadBytes = uploadLayout.uploadBytes});
}

void VulkanDraw2DUploadArena::MarkSubmitted(std::uint32_t slotIndex) noexcept
{
    PONDER_VERIFY(std::this_thread::get_id() == m_ownerThread,
                  "Draw2D upload submission must occur on the arena owner thread");
    PONDER_VERIFY(slotIndex < m_slots.size(),
                  "Draw2D upload submission requires a valid completion slot");
    PONDER_VERIFY(m_slots[slotIndex].state == VulkanDraw2DUploadSlotState::Reserved,
                  "Draw2D upload submission requires one exclusive reservation");

    Slot& slot = m_slots[slotIndex];
    m_stats.reservedBytes -= slot.reservedBytes;
    m_stats.uploadedBytes += slot.reservedBytes;
    slot.state = VulkanDraw2DUploadSlotState::Submitted;
}

void VulkanDraw2DUploadArena::Abandon(std::uint32_t slotIndex) noexcept
{
    PONDER_VERIFY(m_allocator == nullptr || std::this_thread::get_id() == m_ownerThread,
                  "Draw2D upload abandonment must occur on the arena owner thread");
    if (slotIndex >= m_slots.size())
    {
        return;
    }
    Slot& slot = m_slots[slotIndex];
    if (slot.state == VulkanDraw2DUploadSlotState::Reserved)
    {
        m_stats.reservedBytes -= slot.reservedBytes;
        slot.reservedBytes = 0U;
        slot.state = VulkanDraw2DUploadSlotState::Idle;
    }
}

void VulkanDraw2DUploadArena::Complete(std::uint32_t slotIndex) noexcept
{
    PONDER_VERIFY(m_allocator == nullptr || std::this_thread::get_id() == m_ownerThread,
                  "Draw2D upload completion must occur on the arena owner thread");
    if (slotIndex >= m_slots.size())
    {
        return;
    }
    Slot& slot = m_slots[slotIndex];
    if (slot.state == VulkanDraw2DUploadSlotState::Submitted)
    {
        m_stats.uploadedBytes -= slot.reservedBytes;
        slot.reservedBytes = 0U;
        slot.state = VulkanDraw2DUploadSlotState::Idle;
    }
}

void VulkanDraw2DUploadArena::Reset() noexcept
{
    if (m_allocator == nullptr)
    {
        return;
    }
    PONDER_VERIFY(std::this_thread::get_id() == m_ownerThread,
                  "VulkanDraw2DUploadArena destruction must occur on its owner thread");
    m_slots.clear();
    m_dispatch = {};
    m_allocator = nullptr;
    m_nonCoherentAtomSize = 1U;
    m_maximumUploadBytes = 0U;
    m_stats = {};
    m_ownerThread = {};
}
} // namespace pond::render::detail
