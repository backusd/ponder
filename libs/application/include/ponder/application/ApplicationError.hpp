#pragma once

#include <ponder/core/Exception.hpp>
#include <ponder/core/Result.hpp>

#include <format>
#include <ostream>
#include <source_location>
#include <string>

namespace ponder::application
{
// Values from 0x0005'0000 through 0x0005'FFFF are reserved for application errors.
enum class ApplicationErrorCode : ponder::core::ErrorCodeValue
{
    InvalidArgument = 0x0005'0001,
    InvalidState = 0x0005'0002,
    NotFound = 0x0005'0003,
    WrongThread = 0x0005'0004,
    ProcessFailure = 0x0005'0005,
    InternalFailure = 0x0005'0006
};

[[nodiscard]] constexpr ponder::core::ErrorCode ToErrorCode(ApplicationErrorCode code) noexcept
{
    const auto value = static_cast<ponder::core::ErrorCodeValue>(code);
    switch (code)
    {
    case ApplicationErrorCode::InvalidArgument:
        return ponder::core::ErrorCode{ponder::core::ErrorCategory::InvalidArgument, value};
    case ApplicationErrorCode::NotFound:
        return ponder::core::ErrorCode{ponder::core::ErrorCategory::NotFound, value};
    case ApplicationErrorCode::InvalidState:
    case ApplicationErrorCode::WrongThread:
    case ApplicationErrorCode::ProcessFailure:
        return ponder::core::ErrorCode{ponder::core::ErrorCategory::General, value};
    case ApplicationErrorCode::InternalFailure:
        return ponder::core::ErrorCode{ponder::core::ErrorCategory::Internal, value};
    }

    return ponder::core::ErrorCode{ponder::core::ErrorCategory::Internal, value};
}
} // namespace ponder::application

#define APPLICATION_EXCEPTION(errorCode, messageFormat, ...)                                                                                         \
    ::ponder::core::MakeFormattedException(std::source_location::current(), "Application error [{}]: {}", (errorCode),                               \
                                           std::format((messageFormat)__VA_OPT__(, ) __VA_ARGS__))

namespace std
{
template <>
struct formatter<ponder::application::ApplicationErrorCode> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::application::ApplicationErrorCode code, FormatContext& context) const
    {
        using ponder::application::ApplicationErrorCode;

        string text;
        switch (code)
        {
        case ApplicationErrorCode::InvalidArgument:
            text = "invalid_argument";
            break;
        case ApplicationErrorCode::InvalidState:
            text = "invalid_state";
            break;
        case ApplicationErrorCode::NotFound:
            text = "not_found";
            break;
        case ApplicationErrorCode::WrongThread:
            text = "wrong_thread";
            break;
        case ApplicationErrorCode::ProcessFailure:
            text = "process_failure";
            break;
        case ApplicationErrorCode::InternalFailure:
            text = "internal_failure";
            break;
        default:
            text = std::format("unknown({})", static_cast<ponder::core::ErrorCodeValue>(code));
            break;
        }

        return formatter<string>::format(text, context);
    }
};
} // namespace std

namespace ponder::application
{
inline std::ostream& operator<<(std::ostream& output, ApplicationErrorCode code)
{
    return output << std::format("{}", code);
}
} // namespace ponder::application
