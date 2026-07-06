#include <ponder/core/Assert.hpp>
#include <ponder/core/Log.hpp>

#include <cstdlib>
#include <format>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if !defined(_MSC_VER)
#include <csignal>
#endif

namespace pond::core
{
void DebugBreak() noexcept
{
#if defined(_MSC_VER)
    __debugbreak();
#elif defined(__has_builtin)
#if __has_builtin(__builtin_debugtrap)
    __builtin_debugtrap();
#else
    std::raise(SIGTRAP);
#endif
#else
    std::raise(SIGTRAP);
#endif
}

[[noreturn]] void HandleAssertionFailure(std::string_view expression, std::string_view message,
                                         std::source_location location)
{
    if (message.empty())
    {
        LogMessage(LogLevel::Fatal, std::format("Assertion failed: {}", expression), location);
    }
    else
    {
        LogMessage(LogLevel::Fatal, std::format("Assertion failed: {} ({})", expression, message),
                   location);
    }

    DebugBreak();
    std::abort();
}
} // namespace pond::core