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
    constexpr explicit PonderException(
        std::string message, std::source_location location = std::source_location::current())
        : m_message(std::move(message)), m_location(location)
    {
        if consteval
        {
        }
        else
        {
            m_stackTrace = CaptureStackTrace();
        }
    }

    constexpr PonderException(std::string message, StackTrace stackTrace,
                              std::source_location location = std::source_location::current())
        : m_message(std::move(message)), m_location(location), m_stackTrace(std::move(stackTrace))
    {
    }

    [[nodiscard]] constexpr std::string_view GetMessage() const noexcept
    {
        return m_message;
    }

    [[nodiscard]] constexpr const std::source_location& GetLocation() const noexcept
    {
        return m_location;
    }

    [[nodiscard]] constexpr const StackTrace& GetStackTrace() const noexcept
    {
        return m_stackTrace;
    }

private:
    std::string m_message;
    std::source_location m_location;
    StackTrace m_stackTrace;
};

[[nodiscard]] constexpr PonderException MakePonderException(
    std::string message, std::source_location location = std::source_location::current())
{
    if consteval
    {
        return PonderException{std::move(message), StackTrace{}, location};
    }
    else
    {
        return PonderException{std::move(message), CaptureStackTrace(), location};
    }
}

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
