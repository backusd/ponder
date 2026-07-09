#include "StringValidation.hpp"

#include <ponder/core/String.hpp>

namespace pond::platform::detail
{
bool IsValidUtf8WithoutEmbeddedNull(std::string_view text)
{
    return text.find('\0') == std::string_view::npos &&
           core::Utf8ToWideString(text).HasValue();
}
} // namespace pond::platform::detail
