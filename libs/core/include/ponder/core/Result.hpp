#pragma once

#include <cstdint>
#include <expected>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pond::core
{
enum class ErrorCategory
{
    General,
    InvalidArgument,
    NotFound,
    Io,
    Parse,
    Unsupported,
    Internal
};

using ErrorCodeValue = std::int32_t;

class ErrorCode final
{
public:
    constexpr ErrorCode() noexcept = default;
    constexpr ErrorCode(ErrorCategory category, ErrorCodeValue value) noexcept
        : m_category(category), m_value(value)
    {
    }

    [[nodiscard]] constexpr ErrorCategory GetCategory() const noexcept
    {
        return m_category;
    }

    [[nodiscard]] constexpr ErrorCodeValue GetValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] friend constexpr bool operator==(const ErrorCode& lhs,
                                                   const ErrorCode& rhs) noexcept = default;

private:
    ErrorCategory m_category{ErrorCategory::General};
    ErrorCodeValue m_value{0};
};

class StackTrace final
{
public:
    StackTrace() = default;
    explicit StackTrace(std::vector<std::string> frames);

    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] std::span<const std::string> GetFrames() const noexcept;
    [[nodiscard]] std::string Format() const;

private:
    std::vector<std::string> m_frames;
};

class Error final
{
public:
    explicit Error(std::string message,
                   std::source_location location = std::source_location::current());
    Error(ErrorCode code, std::string message,
          std::source_location location = std::source_location::current());
    Error(ErrorCode code, std::string message, StackTrace stackTrace,
          std::source_location location = std::source_location::current());

    [[nodiscard]] ErrorCode GetCode() const noexcept;
    [[nodiscard]] std::string_view GetMessage() const noexcept;
    [[nodiscard]] const std::source_location& GetLocation() const noexcept;
    [[nodiscard]] const StackTrace& GetStackTrace() const noexcept;

private:
    ErrorCode m_code;
    std::string m_message;
    std::source_location m_location;
    StackTrace m_stackTrace;
};

[[nodiscard]] std::string_view GetErrorCategoryName(ErrorCategory category) noexcept;
[[nodiscard]] std::string FormatErrorCode(ErrorCode code);
[[nodiscard]] std::string FormatError(const Error& error);
[[nodiscard]] StackTrace CaptureStackTrace();

template <typename Value, typename ErrorType = Error>
using Result = std::expected<Value, ErrorType>;

using VoidResult = Result<void>;

template <typename ErrorType = Error, typename... Args>
[[nodiscard]] std::unexpected<ErrorType> MakeUnexpected(Args&&... args)
{
    return std::unexpected<ErrorType>(ErrorType(std::forward<Args>(args)...));
}
} // namespace pond::core
