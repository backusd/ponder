#pragma once

#include <ponder/core/Result.hpp>

#include <string>
#include <string_view>

namespace pond::core
{
[[nodiscard]] Result<std::wstring> Utf8ToWideString(std::string_view text);
[[nodiscard]] Result<std::string> WideStringToUtf8(std::wstring_view text);
} // namespace pond::core