#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace pond::io
{
[[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view utf8Path);
} // namespace pond::io