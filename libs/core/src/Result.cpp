#include <ponder/core/Result.hpp>

#include <cstddef>
#include <format>
#include <sstream>
#include <utility>

namespace pond::core
{
namespace
{
constexpr std::string_view kUnknownErrorCategory{"unknown"};
}

StackTrace::StackTrace(std::vector<std::string> frames) : m_frames(std::move(frames)) {}

bool StackTrace::IsEmpty() const noexcept
{
    return m_frames.empty();
}

std::span<const std::string> StackTrace::GetFrames() const noexcept
{
    return std::span<const std::string>{m_frames.data(), m_frames.size()};
}

std::string StackTrace::Format() const
{
    std::ostringstream formatted;

    for (std::size_t index = 0; index < m_frames.size(); ++index)
    {
        if (index > 0)
        {
            formatted << '\n';
        }

        formatted << index << ": " << m_frames[index];
    }

    return formatted.str();
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
    const auto& location = error.GetLocation();
    return std::format("[{}] {} ({}:{}:{})", FormatErrorCode(error.GetCode()), error.GetMessage(),
                       location.file_name(), location.line(), location.column());
}

StackTrace CaptureStackTrace()
{
    // Stacktrace support is intentionally best-effort until CORE-009 evaluates
    // the supported compiler matrix. An empty stacktrace is the graceful
    // fallback and is part of the public contract.
    return StackTrace{};
}
} // namespace pond::core
