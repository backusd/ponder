#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace
{
#if defined(_WIN32)
[[nodiscard]] std::string WideToUtf8(std::wstring_view text)
{
    if (text.empty())
    {
        return {};
    }

    const int requiredBytes = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    if (requiredBytes <= 0)
    {
        return {};
    }

    std::string result(static_cast<std::size_t>(requiredBytes), '\0');
    const int writtenBytes = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
        result.data(), requiredBytes, nullptr, nullptr);
    if (writtenBytes != requiredBytes)
    {
        return {};
    }
    return result;
}
#endif

[[nodiscard]] bool ParseInt(std::string_view text, int& value)
{
    try
    {
        std::size_t parsedCharacters{};
        const int parsed = std::stoi(std::string{text}, &parsedCharacters, 10);
        if (parsedCharacters != text.size())
        {
            return false;
        }
        value = parsed;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

[[nodiscard]] bool TouchFile(const std::filesystem::path& path)
{
    if (path.has_parent_path())
    {
        std::error_code ignored;
        std::filesystem::create_directories(path.parent_path(), ignored);
    }

    std::ofstream file{path, std::ios::binary};
    return file.good();
}

[[nodiscard]] bool WriteArguments(const std::filesystem::path& path,
                                  const std::vector<std::string>& arguments,
                                  int firstArgument)
{
    if (path.has_parent_path())
    {
        std::error_code ignored;
        std::filesystem::create_directories(path.parent_path(), ignored);
    }

    std::ofstream file{path, std::ios::binary};
    if (!file)
    {
        return false;
    }

    for (int index = firstArgument; index < static_cast<int>(arguments.size()); ++index)
    {
        file << arguments[static_cast<std::size_t>(index)] << '\n';
    }
    return file.good();
}

int RunHelper(const std::vector<std::string>& arguments)
{
    const int argc = static_cast<int>(arguments.size());
    std::optional<std::filesystem::path> writeArgumentsPath;
    std::optional<std::filesystem::path> touchReadyPath;
    std::optional<std::filesystem::path> touchAfterSleepPath;
    int sleepMilliseconds{};
    int exitCode{};
    int firstPassthroughArgument = argc;

    for (int index = 1; index < argc;)
    {
        const std::string_view argument{arguments[static_cast<std::size_t>(index)]};
        if (argument == "--")
        {
            firstPassthroughArgument = index + 1;
            break;
        }

        if (index + 1 >= argc)
        {
            std::cerr << "Missing value for " << argument << '\n';
            return 2;
        }

        const std::string& value = arguments[static_cast<std::size_t>(index + 1)];
        if (argument == "--write-args")
        {
            writeArgumentsPath = std::filesystem::path{value};
        }
        else if (argument == "--touch-ready")
        {
            touchReadyPath = std::filesystem::path{value};
        }
        else if (argument == "--touch-after-sleep")
        {
            touchAfterSleepPath = std::filesystem::path{value};
        }
        else if (argument == "--sleep-ms")
        {
            if (!ParseInt(value, sleepMilliseconds) || sleepMilliseconds < 0)
            {
                std::cerr << "Invalid sleep duration: " << value << '\n';
                return 2;
            }
        }
        else if (argument == "--exit-code")
        {
            if (!ParseInt(value, exitCode))
            {
                std::cerr << "Invalid exit code: " << value << '\n';
                return 2;
            }
        }
        else
        {
            std::cerr << "Unknown argument: " << argument << '\n';
            return 2;
        }

        index += 2;
    }

    if (writeArgumentsPath.has_value() &&
        !WriteArguments(*writeArgumentsPath, arguments, firstPassthroughArgument))
    {
        std::cerr << "Unable to write argument file\n";
        return 3;
    }

    if (touchReadyPath.has_value() && !TouchFile(*touchReadyPath))
    {
        std::cerr << "Unable to write ready file\n";
        return 3;
    }

    if (sleepMilliseconds > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{sleepMilliseconds});
    }

    if (touchAfterSleepPath.has_value() && !TouchFile(*touchAfterSleepPath))
    {
        std::cerr << "Unable to write completion file\n";
        return 3;
    }

    return exitCode;
}
} // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t* argv[])
{
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index)
    {
        arguments.push_back(WideToUtf8(argv[index]));
    }
    return RunHelper(arguments);
}
#else
int main(int argc, char* argv[])
{
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index)
    {
        arguments.emplace_back(argv[index]);
    }
    return RunHelper(arguments);
}
#endif
