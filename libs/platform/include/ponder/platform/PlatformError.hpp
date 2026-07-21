#pragma once

#include <ponder/core/Result.hpp>

#include <format>
#include <ostream>
#include <string>
#include <string_view>

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

namespace std
{
template <>
struct formatter<pond::platform::PlatformErrorCode> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::PlatformErrorCode code, FormatContext& context) const
    {
        using pond::platform::PlatformErrorCode;

        string text;
        switch (code)
        {
        case PlatformErrorCode::InvalidArgument:
            text = "invalid_argument";
            break;
        case PlatformErrorCode::RuntimeAlreadyActive:
            text = "runtime_already_active";
            break;
        case PlatformErrorCode::BackendFailure:
            text = "backend_failure";
            break;
        case PlatformErrorCode::NotFound:
            text = "not_found";
            break;
        case PlatformErrorCode::Unsupported:
            text = "unsupported";
            break;
        default:
            text = std::format("unknown({})", static_cast<pond::core::ErrorCodeValue>(code));
            break;
        }

        return formatter<string>::format(text, context);
    }
};
} // namespace std

namespace pond::platform
{
inline std::ostream& operator<<(std::ostream& output, PlatformErrorCode code)
{
    return output << std::format("{}", code);
}
} // namespace pond::platform