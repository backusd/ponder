#pragma once

#include <ponder/core/Result.hpp>

namespace pond::render
{
// Values from 0x0003'0000 through 0x0003'FFFF are reserved for render errors.
enum class RenderErrorCode : core::ErrorCodeValue
{
    InvalidArgument = 0x0003'0001,
    InvalidState = 0x0003'0002,
    UnsupportedBackend = 0x0003'0003,
    UnsupportedCapability = 0x0003'0004,
    UnsupportedSurface = 0x0003'0005,
    LoaderUnavailable = 0x0003'0006,
    NoCompatibleAdapter = 0x0003'0007,
    OutOfMemory = 0x0003'0008,
    SurfaceLost = 0x0003'0009,
    DeviceLost = 0x0003'000A,
    BackendFailure = 0x0003'000B
};

[[nodiscard]] constexpr core::ErrorCode ToErrorCode(RenderErrorCode code) noexcept
{
    const auto value = static_cast<core::ErrorCodeValue>(code);

    switch (code)
    {
    case RenderErrorCode::InvalidArgument:
    case RenderErrorCode::InvalidState:
        return core::ErrorCode{core::ErrorCategory::InvalidArgument, value};
    case RenderErrorCode::UnsupportedBackend:
    case RenderErrorCode::UnsupportedCapability:
    case RenderErrorCode::UnsupportedSurface:
        return core::ErrorCode{core::ErrorCategory::Unsupported, value};
    case RenderErrorCode::LoaderUnavailable:
    case RenderErrorCode::NoCompatibleAdapter:
        return core::ErrorCode{core::ErrorCategory::NotFound, value};
    case RenderErrorCode::OutOfMemory:
    case RenderErrorCode::SurfaceLost:
    case RenderErrorCode::DeviceLost:
    case RenderErrorCode::BackendFailure:
        return core::ErrorCode{core::ErrorCategory::General, value};
    }

    return core::ErrorCode{core::ErrorCategory::Internal, value};
}
} // namespace pond::render