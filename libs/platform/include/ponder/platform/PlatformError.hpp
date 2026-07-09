#pragma once

#include <ponder/core/Result.hpp>

namespace pond::platform
{
// Values from 0x0001'0000 through 0x0001'FFFF are reserved for platform errors.
enum class PlatformErrorCode : core::ErrorCodeValue
{
    InvalidArgument = 0x0001'0001,
    RuntimeAlreadyActive = 0x0001'0002,
    BackendFailure = 0x0001'0003,
    NotFound = 0x0001'0004,
    Unsupported = 0x0001'0005
};

[[nodiscard]] constexpr core::ErrorCode ToErrorCode(PlatformErrorCode code) noexcept
{
    const auto value = static_cast<core::ErrorCodeValue>(code);

    switch (code)
    {
    case PlatformErrorCode::InvalidArgument:
        return core::ErrorCode{core::ErrorCategory::InvalidArgument, value};
    case PlatformErrorCode::RuntimeAlreadyActive:
    case PlatformErrorCode::BackendFailure:
        return core::ErrorCode{core::ErrorCategory::General, value};
    case PlatformErrorCode::NotFound:
        return core::ErrorCode{core::ErrorCategory::NotFound, value};
    case PlatformErrorCode::Unsupported:
        return core::ErrorCode{core::ErrorCategory::Unsupported, value};
    }

    return core::ErrorCode{core::ErrorCategory::Internal, value};
}
} // namespace pond::platform
