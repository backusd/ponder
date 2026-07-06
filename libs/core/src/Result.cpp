#include <ponder/core/Result.hpp>

#include <format>
#include <utility>

namespace pond::core
{
namespace
{
constexpr std::string_view kUnknownErrorCategory{"unknown"};
}

Error::Error(std::string message, std::source_location location)
    : Error(ErrorCode{}, std::move(message), location)
{
}

Error::Error(ErrorCode code, std::string message, std::source_location location)
    : Error(code, std::move(message), CaptureStackTrace(), location)
{
}

Error::Error(ErrorCode code, std::string message, StackTrace stackTrace,
             std::source_location location)
    : m_code(code), m_message(std::move(message)), m_location(location),
      m_stackTrace(std::move(stackTrace))
{
}

ErrorCode Error::GetCode() const noexcept
{
    return m_code;
}

std::string_view Error::GetMessage() const noexcept
{
    return m_message;
}

const std::source_location& Error::GetLocation() const noexcept
{
    return m_location;
}

const StackTrace& Error::GetStackTrace() const noexcept
{
    return m_stackTrace;
}

std::string_view GetErrorCategoryName(ErrorCategory category) noexcept
{
    switch (category)
    {
    case ErrorCategory::General:
        return "general";
    case ErrorCategory::InvalidArgument:
        return "invalid_argument";
    case ErrorCategory::NotFound:
        return "not_found";
    case ErrorCategory::Io:
        return "io";
    case ErrorCategory::Parse:
        return "parse";
    case ErrorCategory::Unsupported:
        return "unsupported";
    case ErrorCategory::Internal:
        return "internal";
    }

    return kUnknownErrorCategory;
}

std::string FormatErrorCode(ErrorCode code)
{
    return std::format("{}:{}", GetErrorCategoryName(code.GetCategory()), code.GetValue());
}

std::string FormatError(const Error& error)
{
    return std::format("[{}] {} ({})", FormatErrorCode(error.GetCode()), error.GetMessage(),
                       FormatSourceLocation(error.GetLocation()));
}
} // namespace pond::core
