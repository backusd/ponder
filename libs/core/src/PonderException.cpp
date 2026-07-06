#include <ponder/core/PonderException.hpp>

#include <utility>

namespace pond::core
{
PonderException::PonderException(std::string message, std::source_location location)
    : PonderException(std::move(message), CaptureStackTrace(), location)
{
}

PonderException::PonderException(std::string message, StackTrace stackTrace,
                                 std::source_location location)
    : m_message(std::move(message)), m_location(location), m_stackTrace(std::move(stackTrace))
{
}

std::string_view PonderException::GetMessage() const noexcept
{
    return m_message;
}

const std::source_location& PonderException::GetLocation() const noexcept
{
    return m_location;
}

const StackTrace& PonderException::GetStackTrace() const noexcept
{
    return m_stackTrace;
}

[[noreturn]] void ThrowPonderException(std::string message, std::source_location location)
{
    throw PonderException{std::move(message), location};
}
} // namespace pond::core
