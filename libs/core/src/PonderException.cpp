#include <ponder/core/PonderException.hpp>

#include <utility>

namespace pond::core
{
PonderException::PonderException(std::string message, std::source_location location)
    : std::runtime_error(std::move(message)), m_location(location)
{
}

const std::source_location& PonderException::GetLocation() const noexcept
{
    return m_location;
}
} // namespace pond::core