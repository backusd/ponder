#pragma once

#include <ponder/core/StackTrace.hpp>

#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace pond::core
{
class PonderException final
{
public:
    explicit PonderException(std::string message,
                             std::source_location location = std::source_location::current());
    PonderException(std::string message, StackTrace stackTrace,
                    std::source_location location = std::source_location::current());

    [[nodiscard]] std::string_view GetMessage() const noexcept;
    [[nodiscard]] const std::source_location& GetLocation() const noexcept;
    [[nodiscard]] const StackTrace& GetStackTrace() const noexcept;

private:
    std::string m_message;
    std::source_location m_location;
    StackTrace m_stackTrace;
};

[[nodiscard]] PonderException MakePonderException(
    std::string message, std::source_location location = std::source_location::current());

[[noreturn]] void ThrowPonderException(
    std::string message, std::source_location location = std::source_location::current());

template <typename... Args>
[[nodiscard]] PonderException MakeFormattedPonderException(
    std::source_location location, std::format_string<Args...> messageFormat, Args&&... args)
{
    return MakePonderException(std::format(messageFormat, std::forward<Args>(args)...), location);
}
} // namespace pond::core

#define PONDER_EXCEPTION(...)                                                                      \
    ::pond::core::MakeFormattedPonderException(std::source_location::current(), __VA_ARGS__)
