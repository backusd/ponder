#pragma once

#include <expected>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace pond::core
{
class Error
{
public:
    explicit Error(std::string message,
                   std::source_location location = std::source_location::current());

    [[nodiscard]] std::string_view GetMessage() const noexcept;
    [[nodiscard]] const std::source_location& GetLocation() const noexcept;

private:
    std::string m_message;
    std::source_location m_location;
};

template <typename Value, typename ErrorType = Error>
using Result = std::expected<Value, ErrorType>;

using VoidResult = Result<void>;

template <typename ErrorType = Error, typename... Args>
[[nodiscard]] std::unexpected<ErrorType> MakeUnexpected(Args&&... args)
{
    return std::unexpected<ErrorType>(ErrorType(std::forward<Args>(args)...));
}
} // namespace pond::core