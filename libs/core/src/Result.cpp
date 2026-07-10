#include <ponder/core/Result.hpp>

#include <format>
#include <utility>

namespace pond::core
{
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