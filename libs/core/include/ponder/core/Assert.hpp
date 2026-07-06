#pragma once

#include <source_location>
#include <string_view>

namespace pond::core
{
[[noreturn]] void
HandleAssertionFailure(std::string_view expression, std::string_view message,
                       std::source_location location = std::source_location::current());

void DebugBreak() noexcept;
} // namespace pond::core

#if defined(NDEBUG)
#define PONDER_ASSERT(expression) ((void)0)
#define PONDER_ASSERT_MESSAGE(expression, message) ((void)0)
#else
#define PONDER_ASSERT(expression)                                                                  \
    do                                                                                             \
    {                                                                                              \
        if (!(expression))                                                                         \
        {                                                                                          \
            ::pond::core::HandleAssertionFailure(#expression, std::string_view{},                  \
                                                 std::source_location::current());                 \
        }                                                                                          \
    } while (false)

#define PONDER_ASSERT_MESSAGE(expression, message)                                                 \
    do                                                                                             \
    {                                                                                              \
        if (!(expression))                                                                         \
        {                                                                                          \
            ::pond::core::HandleAssertionFailure(#expression, std::string_view{message},           \
                                                 std::source_location::current());                 \
        }                                                                                          \
    } while (false)
#endif