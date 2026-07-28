#include "PlatformCommon.hpp"

#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/core/String.hpp>
#include <ponder/platform/PlatformError.hpp>

#include <format>
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
