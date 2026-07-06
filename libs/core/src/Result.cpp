#include <ponder/core/Result.hpp>

#include <utility>

namespace pond::core
{
Error::Error(std::string message, std::source_location location)
    : m_message(std::move(message)), m_location(location)
{
}

std::string_view Error::GetMessage() const noexcept
{
    return m_message;
}

const std::source_location& Error::GetLocation() const noexcept
{
    return m_location;
}
} // namespace pond::core