#pragma once

#include <ponder/core/Result.hpp>

#include <source_location>
#include <string>
#include <string_view>

namespace ponder::platform::detail
{
// Call immediately after a documented SDL failure, before any other SDL call.
// The returned owned message is suitable for PLATFORM_EXCEPTION at the
// failure site so the core exception message includes the PlatformErrorCode and
// keeps the caller source location. This function does not clear SDL's error or select a code.
// The operation must be non-empty.
[[nodiscard]] std::string CaptureSdlFailureMessage(std::string_view operation, std::string_view objectContext = {});

// Formats an SDL error string that the caller already captured. This function
// performs no SDL calls, allowing exceptional clipboard paths to preserve their
// required snapshot ordering. The operation must be non-empty.
[[nodiscard]] std::string FormatCapturedSdlFailureMessage(std::string_view operation, std::string_view objectContext, std::string_view capturedError);

// Call only after the caller observes a documented SDL failure, with no intervening SDL call.
// This function snapshots but does not clear SDL's error, detect failure, or select the code.
// The operation must be non-empty.
[[nodiscard]] ponder::core::Error CaptureSdlFailure(ponder::core::ErrorCode code, std::string_view operation, std::string_view objectContext = {},
                                                    std::source_location location = std::source_location::current());

// Constructs a retained Error from an SDL error string that the caller already
// captured. This overload performs no SDL calls.
[[nodiscard]] ponder::core::Error CaptureSdlFailure(ponder::core::ErrorCode code, std::string_view operation, std::string_view objectContext,
                                                    std::string_view capturedError, std::source_location location = std::source_location::current());
} // namespace ponder::platform::detail
