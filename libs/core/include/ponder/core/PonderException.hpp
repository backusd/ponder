#pragma once

#include <source_location>
#include <stdexcept>
#include <string>

namespace pond::core
{
class PonderException final : public std::runtime_error
{
public:
    explicit PonderException(std::string message,
                             std::source_location location = std::source_location::current());

    [[nodiscard]] const std::source_location& GetLocation() const noexcept;

private:
    std::source_location m_location;
};
} // namespace pond::core