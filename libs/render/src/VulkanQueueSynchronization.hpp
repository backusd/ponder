#pragma once

#include <ponder/core/Assert.hpp>

#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <vector>
#include <volk.h>

namespace pond::render::detail
{
class VulkanQueueSynchronization final
{
public:
    class QueueOperationLock final
    {
    public:
        QueueOperationLock(std::shared_mutex& deviceGate, std::mutex& queueMutex)
            : m_deviceLock{deviceGate}, m_queueLock{queueMutex}
        {
        }

        QueueOperationLock(const QueueOperationLock&) = delete;
        QueueOperationLock& operator=(const QueueOperationLock&) = delete;
        QueueOperationLock(QueueOperationLock&&) noexcept = default;
        QueueOperationLock& operator=(QueueOperationLock&&) = delete;
        ~QueueOperationLock() = default;

    private:
        // Reverse destruction releases the exact queue before the shared device gate.
        std::shared_lock<std::shared_mutex> m_deviceLock;
        std::unique_lock<std::mutex> m_queueLock;
    };

    explicit VulkanQueueSynchronization(std::span<const VkQueue> queues)
    {
        m_queues.reserve(queues.size());
        for (const VkQueue queue : queues)
        {
            PONDER_VERIFY(queue != VK_NULL_HANDLE,
                          "Vulkan queue synchronization requires live queues");

            bool alreadyRegistered = false;
            for (const QueueEntry& entry : m_queues)
            {
                if (entry.queue == queue)
                {
                    alreadyRegistered = true;
                    break;
                }
            }

            if (!alreadyRegistered)
            {
                m_queues.push_back(
                    QueueEntry{.queue = queue, .mutex = std::make_unique<std::mutex>()});
            }
        }

        PONDER_VERIFY(!m_queues.empty(),
                      "Vulkan queue synchronization requires at least one queue");
    }

    VulkanQueueSynchronization(const VulkanQueueSynchronization&) = delete;
    VulkanQueueSynchronization& operator=(const VulkanQueueSynchronization&) = delete;
    VulkanQueueSynchronization(VulkanQueueSynchronization&&) = delete;
    VulkanQueueSynchronization& operator=(VulkanQueueSynchronization&&) = delete;
    ~VulkanQueueSynchronization() = default;

    [[nodiscard]] QueueOperationLock LockQueue(VkQueue queue) const
    {
        return QueueOperationLock{m_deviceGate, GetQueueMutex(queue)};
    }

    [[nodiscard]] std::unique_lock<std::shared_mutex> LockDevice() const
    {
        return std::unique_lock<std::shared_mutex>{m_deviceGate};
    }

    [[nodiscard]] std::size_t GetUniqueQueueCount() const noexcept
    {
        return m_queues.size();
    }

private:
    struct QueueEntry final
    {
        VkQueue queue{VK_NULL_HANDLE};
        std::unique_ptr<std::mutex> mutex;
    };

    [[nodiscard]] std::mutex& GetQueueMutex(VkQueue queue) const noexcept
    {
        for (const QueueEntry& entry : m_queues)
        {
            if (entry.queue == queue)
            {
                return *entry.mutex;
            }
        }

        PONDER_VERIFY(false, "Vulkan queue synchronization received an unowned queue");
        return *m_queues.front().mutex;
    }

    mutable std::shared_mutex m_deviceGate;
    std::vector<QueueEntry> m_queues;
};
} // namespace pond::render::detail
