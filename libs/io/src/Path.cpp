#include <ponder/io/Path.hpp>

namespace pond::io
{
std::string PathToUtf8(const std::filesystem::path& path)
{
    const std::u8string utf8Path = path.u8string();
    std::string text;
    text.reserve(utf8Path.size());
    for (const char8_t character : utf8Path)
    {
        text.push_back(static_cast<char>(character));
    }
    return text;
}

std::filesystem::path PathFromUtf8(std::string_view utf8Path)
{
    std::u8string pathText;
    pathText.reserve(utf8Path.size());
    for (const char character : utf8Path)
    {
        pathText.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
    }
    return std::filesystem::path{pathText};
}
} // namespace pond::io