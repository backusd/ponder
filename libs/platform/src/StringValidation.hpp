#pragma once

#include <string_view>

namespace pond::platform::detail
{
[[nodiscard]] bool IsValidUtf8WithoutEmbeddedNull(std::string_view text);
} // namespace pond::platform::detail
