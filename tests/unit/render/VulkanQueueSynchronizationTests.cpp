#include <array>
#include <chrono>
#include <cstdint>
#include <future>
#include <gtest/gtest.h>
#include <latch>
#include <thread>

#include "VulkanQueueSynchronization.hpp"

namespace pond::render::detail
{
namespace
{
[[nodiscard]] VkQueue MakeQueue(std::uintptr_t value) noexcept
{
    return reinterpret_cast<VkQueue>(value);
}
} // namespace

TEST(RenderVulkanQueueSynchronizationTests, AliasedHandlesShareOneQueueLock)
{
    using namespace std::chrono_literals;

    const VkQueue queue = MakeQueue(0x1000U);
    const std::array queues{queue, queue};
    VulkanQueueSynchronization synchronization{queues};
    EXPECT_EQ(synchronization.GetUniqueQueueCount(), 1U);

    std::latch firstLockAcquired{1};
    std::latch releaseFirstLock{1};
    std::jthread firstTarget{[&]
                             {
                                 [[maybe_unused]] auto lock = synchronization.LockQueue(queue);
                                 firstLockAcquired.count_down();
                                 releaseFirstLock.wait();
                             }};

    firstLockAcquired.wait();
    auto secondTarget = std::async(std::launch::async,
                                   [&]
                                   {
                                       [[maybe_unused]] auto lock =
                                           synchronization.LockQueue(queue);
                                   });

    EXPECT_EQ(secondTarget.wait_for(50ms), std::future_status::timeout);
    releaseFirstLock.count_down();
    EXPECT_EQ(secondTarget.wait_for(500ms), std::future_status::ready);
    secondTarget.get();
}

TEST(RenderVulkanQueueSynchronizationTests,
     OneTargetsBoundedWaitDoesNotBlockIndependentQueuePreparation)
{
    using namespace std::chrono_literals;

    const VkQueue graphicsQueue = MakeQueue(0x1000U);
    const VkQueue presentationQueue = MakeQueue(0x2000U);
    const std::array queues{graphicsQueue, presentationQueue};
    VulkanQueueSynchronization synchronization{queues};
    EXPECT_EQ(synchronization.GetUniqueQueueCount(), 2U);

    std::latch blockedTargetReady{1};
    std::latch releaseBlockedTarget{1};
    std::jthread blockedTarget{[&]
                               {
                                   [[maybe_unused]] auto lock =
                                       synchronization.LockQueue(presentationQueue);
                                   blockedTargetReady.count_down();
                                   releaseBlockedTarget.wait();
                               }};

    blockedTargetReady.wait();
    auto independentTarget = std::async(std::launch::async,
                                        [&]
                                        {
                                            [[maybe_unused]] auto lock =
                                                synchronization.LockQueue(graphicsQueue);
                                        });

    EXPECT_EQ(independentTarget.wait_for(500ms), std::future_status::ready);
    releaseBlockedTarget.count_down();
    independentTarget.get();
}

TEST(RenderVulkanQueueSynchronizationTests, DeviceWaitExcludesEveryQueueOperation)
{
    using namespace std::chrono_literals;

    const VkQueue graphicsQueue = MakeQueue(0x1000U);
    const VkQueue presentationQueue = MakeQueue(0x2000U);
    const std::array queues{graphicsQueue, presentationQueue};
    VulkanQueueSynchronization synchronization{queues};

    std::latch queueLockAcquired{1};
    std::latch releaseQueueLock{1};
    std::jthread queueOperation{[&]
                                {
                                    [[maybe_unused]] auto lock =
                                        synchronization.LockQueue(graphicsQueue);
                                    queueLockAcquired.count_down();
                                    releaseQueueLock.wait();
                                }};

    queueLockAcquired.wait();
    auto deviceWait = std::async(std::launch::async,
                                 [&]
                                 {
                                     [[maybe_unused]] auto lock = synchronization.LockDevice();
                                 });

    EXPECT_EQ(deviceWait.wait_for(50ms), std::future_status::timeout);
    releaseQueueLock.count_down();
    EXPECT_EQ(deviceWait.wait_for(500ms), std::future_status::ready);
    deviceWait.get();
}
} // namespace pond::render::detail
