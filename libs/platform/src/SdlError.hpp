#pragma once

#include <ponder/core/Result.hpp>

#include <source_location>
#include <string_view>

namespace pond::platform::detail
{
// Call only after the caller observes a documented SDL failure, with no intervening SDL call.
// This function snapshots but does not clear SDL's error, detect failure, or select the code.
// The operation must be non-empty.
[[nodiscard]] core::Error CaptureSdlFailure(
    core::ErrorCode code, std::string_view operation, std::string_view objectContext = {},
    std::source_location location = std::source_location::current());
} // namespace pond::platform::detail
