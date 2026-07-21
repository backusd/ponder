#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/HintManager.hpp>

namespace pond::platform::detail
{
class IHintBackend
{
public:
    virtual ~IHintBackend() noexcept = default;

    IHintBackend(const IHintBackend&) = delete;
    IHintBackend& operator=(const IHintBackend&) = delete;
    IHintBackend(IHintBackend&&) = delete;
    IHintBackend& operator=(IHintBackend&&) = delete;

    [[nodiscard]] virtual bool IsMainThread() const noexcept = 0;
    [[nodiscard]] virtual bool HasInitializedSubsystems() const noexcept = 0;
    [[nodiscard]] virtual const char* GetHint(const char* name) const noexcept = 0;
    [[nodiscard]] virtual core::VoidResult SetHintOverride(const char* name,
                                                           const char* value) = 0;
    [[nodiscard]] virtual core::VoidResult ResetHint(const char* name) = 0;

protected:
    IHintBackend() noexcept = default;
};

class HintManagerAccess final
{
public:
    [[nodiscard]] static HintManager Create(IHintBackend& backend)
    {
        return HintManager{backend};
    }
};
} // namespace pond::platform::detail
