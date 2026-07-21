#include "PlatformRuntimeBackend.hpp"

#include <ponder/core/Assert.hpp>

#include <atomic>
#include <memory>

#include "SdlDisplayBackend.hpp"
#include "SdlRuntimeBackend.hpp"
#include "SdlWindowBackend.hpp"

namespace pond::platform::detail
{
namespace
{
std::atomic<const IPlatformRuntimeBackendFactory*> backendFactoryOverride{nullptr};
std::atomic<const IPlatformWindowBackendFactory*> windowBackendFactoryOverride{nullptr};
std::atomic<const IPlatformDisplayBackendFactory*> displayBackendFactoryOverride{nullptr};
} // namespace

std::unique_ptr<IPlatformRuntimeBackend> GetPlatformRuntimeBackend()
{
    const IPlatformRuntimeBackendFactory* const factory =
        backendFactoryOverride.load(std::memory_order_acquire);
    if (factory == nullptr)
    {
        return std::make_unique<SdlRuntimeBackend>();
    }

    std::unique_ptr<IPlatformRuntimeBackend> backend = factory->Create();
    PONDER_VERIFY(backend != nullptr, "Platform runtime backend factory returned null");
    return backend;
}

std::unique_ptr<IPlatformWindowBackend> GetPlatformWindowBackend()
{
    const IPlatformWindowBackendFactory* const factory =
        windowBackendFactoryOverride.load(std::memory_order_acquire);
    if (factory == nullptr)
    {
        return std::make_unique<SdlWindowBackend>();
    }

    std::unique_ptr<IPlatformWindowBackend> backend = factory->Create();
    PONDER_VERIFY(backend != nullptr, "Platform window backend factory returned null");
    return backend;
}

std::unique_ptr<IPlatformDisplayBackend> GetPlatformDisplayBackend()
{
    const IPlatformDisplayBackendFactory* const factory =
        displayBackendFactoryOverride.load(std::memory_order_acquire);
    if (factory == nullptr)
    {
        return std::make_unique<SdlDisplayBackend>();
    }

    std::unique_ptr<IPlatformDisplayBackend> backend = factory->Create();
    PONDER_VERIFY(backend != nullptr, "Platform display backend factory returned null");
    return backend;
}

void SetPlatformRuntimeBackendForTesting(
    const IPlatformRuntimeBackendFactory* factory) noexcept
{
    backendFactoryOverride.store(factory, std::memory_order_release);
}

void SetPlatformWindowBackendForTesting(
    const IPlatformWindowBackendFactory* factory) noexcept
{
    windowBackendFactoryOverride.store(factory, std::memory_order_release);
}

void SetPlatformDisplayBackendForTesting(
    const IPlatformDisplayBackendFactory* factory) noexcept
{
    displayBackendFactoryOverride.store(factory, std::memory_order_release);
}
} // namespace pond::platform::detail