#include "PlatformCommon.hpp"

#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/core/String.hpp>
#include <ponder/platform/PlatformError.hpp>

#include <algorithm>
#include <cstdint>
#include <format>
#include <limits>
#include <thread>

namespace ponder::platform::detail
{
namespace
{
constexpr ponder::core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);

const std::thread::id processEntryThread = std::this_thread::get_id();
} // namespace

ponder::core::VoidResult ValidateNullTerminatedUtf8(std::string_view text, std::string_view description)
{
    if (!ponder::core::IsValidUtf8WithoutEmbeddedNull(text))
    {
        return ponder::core::VoidResult::FromError(
            ponder::core::Error{kInvalidArgumentCode, std::format("{} must be UTF-8 without embedded nulls.", description)});
    }

    return ponder::core::VoidResult::Success();
}

std::int32_t GetEventWaitTimeoutMilliseconds(ponder::core::Duration timeout)
{
    constexpr std::int64_t kNanosecondsPerMillisecond{1'000'000};

    const std::int64_t nanoseconds = timeout.GetNanoseconds().count();
    if (nanoseconds < 0)
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::InvalidArgument, "Platform event wait timeout must be nonnegative.");
    }

    const std::int64_t wholeMilliseconds = nanoseconds / kNanosecondsPerMillisecond;
    const std::int64_t roundedMilliseconds = wholeMilliseconds + (nanoseconds % kNanosecondsPerMillisecond != 0 ? 1 : 0);
    return static_cast<std::int32_t>(std::min(roundedMilliseconds, static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())));
}

bool IsPlatformProcessEntryThread() noexcept
{
    return std::this_thread::get_id() == processEntryThread;
}

RuntimeOwnerThreadGuard::RuntimeOwnerThreadGuard() noexcept :
    m_ownerThread(std::this_thread::get_id())
{
}

bool RuntimeOwnerThreadGuard::IsOwnerThread() const noexcept
{
    return std::this_thread::get_id() == m_ownerThread;
}

void RuntimeOwnerThreadGuard::Verify(std::string_view operation) const
{
    if (!IsOwnerThread())
    {
        throw PLATFORM_EXCEPTION(PlatformErrorCode::WrongThread, "Platform runtime {} must run on its owner thread", operation);
    }
}

void RuntimeOwnerThreadGuard::VerifyForDestruction(std::string_view object) const noexcept
{
    PONDER_VERIFY(IsOwnerThread(), "{} destruction must run on its owner thread", object);
}
} // namespace ponder::platform::detail
