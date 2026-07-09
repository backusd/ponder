#include "SdlError.hpp"

#include <SDL3/SDL_error.h>

#include <string>
#include <string_view>

namespace pond::platform::detail
{
namespace
{
constexpr std::string_view kMissingSdlError{"SDL did not provide an error message"};

[[nodiscard]] std::string BuildFailureMessage(std::string_view operation,
                                              std::string_view objectContext,
                                              std::string_view errorText)
{
    std::string message;
    message.reserve(operation.size() + objectContext.size() + errorText.size() + 16);
    message.append(operation);
    message.append(" failed");

    if (!objectContext.empty())
    {
        message.append(" (");
        message.append(objectContext);
        message.push_back(')');
    }

    message.append(": ");
    message.append(errorText);
    return message;
}
} // namespace

core::Error CaptureSdlFailure(core::ErrorCode code, std::string_view operation,
                              std::string_view objectContext, std::source_location location)
{
    const char* const rawError = SDL_GetError();
    const std::string errorText = rawError != nullptr && rawError[0] != '\0'
                                      ? std::string{rawError}
                                      : std::string{kMissingSdlError};

    return core::Error{code, BuildFailureMessage(operation, objectContext, errorText), location};
}
} // namespace pond::platform::detail
