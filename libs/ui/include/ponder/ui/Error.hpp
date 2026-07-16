#pragma once

#include <ponder/core/Result.hpp>

#include <source_location>
#include <string>
#include <utility>

namespace pond::ui
{
inline constexpr core::ErrorCodeValue kUiErrorCodeFirst{0x0004'0000};
inline constexpr core::ErrorCodeValue kUiErrorCodeLast{0x0004'FFFF};

// Values from 0x0004'0000 through 0x0004'FFFF are reserved for UI errors.
enum class UiErrorCode : core::ErrorCodeValue
{
    InvalidPaintValue = 0x0004'0001,
    UnbalancedPaintState = 0x0004'0002,
    InvalidMetrics = 0x0004'0003,
    MetricsMismatch = 0x0004'0004,
    CompilationFailure = 0x0004'0005,
    LimitExceeded = 0x0004'0006,
    InvalidPaintState = 0x0004'0007
};

[[nodiscard]] constexpr core::ErrorCode ToErrorCode(UiErrorCode code) noexcept
{
    const auto value = static_cast<core::ErrorCodeValue>(code);

    switch (code)
    {
    case UiErrorCode::InvalidPaintValue:
    case UiErrorCode::UnbalancedPaintState:
    case UiErrorCode::InvalidMetrics:
    case UiErrorCode::MetricsMismatch:
    case UiErrorCode::LimitExceeded:
    case UiErrorCode::InvalidPaintState:
        return core::ErrorCode{core::ErrorCategory::InvalidArgument, value};
    case UiErrorCode::CompilationFailure:
        return core::ErrorCode{core::ErrorCategory::General, value};
    }

    return core::ErrorCode{core::ErrorCategory::Internal, value};
}

[[nodiscard]] inline core::Error MakeUiError(
    UiErrorCode code, std::string message,
    std::source_location location = std::source_location::current())
{
    return core::Error{ToErrorCode(code), std::move(message), location};
}

template <typename Value>
[[nodiscard]] inline core::Result<Value> MakeUiFailure(
    UiErrorCode code, std::string message,
    std::source_location location = std::source_location::current())
{
    return core::Result<Value>::FromError(MakeUiError(code, std::move(message), location));
}

[[nodiscard]] inline core::VoidResult MakeUiFailure(
    UiErrorCode code, std::string message,
    std::source_location location = std::source_location::current())
{
    return core::VoidResult::FromError(MakeUiError(code, std::move(message), location));
}
} // namespace pond::ui
