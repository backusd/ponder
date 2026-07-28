#pragma once

#include <ponder/core/Exception.hpp>
#include <ponder/core/Result.hpp>

#include <format>
#include <ostream>
#include <source_location>
#include <string>
#include <string_view>

namespace ponder::platform
{
// Values from 0x0001'0000 through 0x0001'FFFF are reserved for platform errors.
enum class PlatformErrorCode : ponder::core::ErrorCodeValue
{
    InvalidArgument = 0x0001'0001,
    RuntimeAlreadyActive = 0x0001'0002,
    BackendFailure = 0x0001'0003,
    NotFound = 0x0001'0004,
    Unsupported = 0x0001'0005,
    WrongThread = 0x0001'0006
};

[[nodiscard]] constexpr ponder::core::ErrorCode ToErrorCode(PlatformErrorCode code) noexcept
{
    const auto value = static_cast<ponder::core::ErrorCodeValue>(code);

    switch (code)
    {
    case PlatformErrorCode::InvalidArgument:
        return ponder::core::ErrorCode{ponder::core::ErrorCategory::InvalidArgument, value};
    case PlatformErrorCode::RuntimeAlreadyActive:
    case PlatformErrorCode::BackendFailure:
    case PlatformErrorCode::WrongThread:
        return ponder::core::ErrorCode{ponder::core::ErrorCategory::General, value};
    case PlatformErrorCode::NotFound:
        return ponder::core::ErrorCode{ponder::core::ErrorCategory::NotFound, value};
    case PlatformErrorCode::Unsupported:
        return ponder::core::ErrorCode{ponder::core::ErrorCategory::Unsupported, value};
    }

    return ponder::core::ErrorCode{ponder::core::ErrorCategory::Internal, value};
}
} // namespace ponder::platform

#define PLATFORM_EXCEPTION(errorCode, messageFormat, ...)                                                                                            \
    ::ponder::core::MakeFormattedException(std::source_location::current(), "Platform error [{}]: {}", (errorCode),                                  \
                                           std::format((messageFormat)__VA_OPT__(, ) __VA_ARGS__))

namespace std
{
template <>
struct formatter<ponder::platform::PlatformErrorCode> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::PlatformErrorCode code, FormatContext& context) const
    {
        using ponder::platform::PlatformErrorCode;

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
        case PlatformErrorCode::WrongThread:
            text = "wrong_thread";
            break;
        default:
            text = std::format("unknown({})", static_cast<ponder::core::ErrorCodeValue>(code));
            break;
        }

        return formatter<string>::format(text, context);
    }
};
} // namespace std

namespace ponder::platform
{
inline std::ostream& operator<<(std::ostream& output, PlatformErrorCode code)
{
    return output << std::format("{}", code);
}
} // namespace ponder::platform
