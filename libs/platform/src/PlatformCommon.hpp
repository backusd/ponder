#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/core/Timing.hpp>

#include <cstdint>
#include <string_view>
#include <thread>

namespace ponder::platform::detail
{
[[nodiscard]] ponder::core::VoidResult ValidateNullTerminatedUtf8(std::string_view text, std::string_view description);
[[nodiscard]] std::int32_t GetEventWaitTimeoutMilliseconds(ponder::core::Duration timeout);
[[nodiscard]] bool IsPlatformProcessEntryThread() noexcept;

class RuntimeOwnerThreadGuard final
{
public:
    RuntimeOwnerThreadGuard() noexcept;

    [[nodiscard]] bool IsOwnerThread() const noexcept;
    void Verify(std::string_view operation) const;
    void VerifyForDestruction(std::string_view object) const noexcept;

private:
    std::thread::id m_ownerThread;
};
} // namespace ponder::platform::detail
