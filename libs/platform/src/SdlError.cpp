#include "SdlError.hpp"

#include <SDL3/SDL_error.h>
#include <format>
#include <string>
#include <string_view>

namespace ponder::platform::detail
{
namespace
{
constexpr std::string_view kMissingSdlError{"SDL did not provide an error message"};

[[nodiscard]] std::string BuildFailureMessage(std::string_view operation, std::string_view objectContext, std::string_view errorText)
{
    return objectContext.empty() ? std::format("{} failed: {}", operation, errorText)
                                 : std::format("{} failed ({}): {}", operation, objectContext, errorText);
}
} // namespace

std::string CaptureSdlFailureMessage(std::string_view operation, std::string_view objectContext)
{
    const char* const rawError = SDL_GetError();
    const std::string errorText = rawError != nullptr && rawError[0] != '\0' ? std::string{rawError} : std::string{kMissingSdlError};

    return BuildFailureMessage(operation, objectContext, errorText);
}

std::string FormatCapturedSdlFailureMessage(std::string_view operation, std::string_view objectContext, std::string_view capturedError)
{
    const std::string_view errorText = !capturedError.empty() ? capturedError : kMissingSdlError;
    return BuildFailureMessage(operation, objectContext, errorText);
}

ponder::core::Error CaptureSdlFailure(ponder::core::ErrorCode code, std::string_view operation, std::string_view objectContext,
                                      std::source_location location)
{
    return ponder::core::Error{code, CaptureSdlFailureMessage(operation, objectContext), location};
}

ponder::core::Error CaptureSdlFailure(ponder::core::ErrorCode code, std::string_view operation, std::string_view objectContext,
                                      std::string_view capturedError, std::source_location location)
{
    return ponder::core::Error{code, FormatCapturedSdlFailureMessage(operation, objectContext, capturedError), location};
}
} // namespace ponder::platform::detail
