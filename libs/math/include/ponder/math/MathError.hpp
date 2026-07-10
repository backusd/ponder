#pragma once

#include <ponder/core/Result.hpp>

namespace pond::math
{
inline constexpr core::ErrorCodeValue kMathErrorCodeFirst{0x0002'0000};
inline constexpr core::ErrorCodeValue kMathErrorCodeLast{0x0002'FFFF};

// Values from 0x0002'0000 through 0x0002'FFFF are reserved for math errors.
enum class MathErrorCode : core::ErrorCodeValue
{
    InvalidArgument = 0x0002'0001,
    NonFiniteInput = 0x0002'0002,
    DegenerateInput = 0x0002'0003,
    SingularMatrix = 0x0002'0004,
    UndefinedHomogeneousCoordinate = 0x0002'0005
};

[[nodiscard]] constexpr core::ErrorCode ToErrorCode(MathErrorCode code) noexcept
{
    const auto value = static_cast<core::ErrorCodeValue>(code);

    switch (code)
    {
    case MathErrorCode::InvalidArgument:
    case MathErrorCode::NonFiniteInput:
        return core::ErrorCode{core::ErrorCategory::InvalidArgument, value};
    case MathErrorCode::DegenerateInput:
    case MathErrorCode::SingularMatrix:
    case MathErrorCode::UndefinedHomogeneousCoordinate:
        return core::ErrorCode{core::ErrorCategory::General, value};
    }

    return core::ErrorCode{core::ErrorCategory::Internal, value};
}
} // namespace pond::math
