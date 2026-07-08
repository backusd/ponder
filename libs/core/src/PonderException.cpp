#include <ponder/core/PonderException.hpp>

#include <utility>

namespace pond::core
{
[[noreturn]] void ThrowPonderException(std::string message, std::source_location location)
{
    throw MakePonderException(std::move(message), location);
}
} // namespace pond::core
