#pragma once

#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace pond::core
{
enum class AssertionFailureKind
{
    Assertion,
    Verify,
    Unreachable
};

class AssertionFailure final
{
public:
    AssertionFailure(AssertionFailureKind kind, std::string expression, std::string message,
                     std::source_location location);

    [[nodiscard]] AssertionFailureKind GetKind() const noexcept;
    [[nodiscard]] std::string_view GetExpression() const noexcept;
    [[nodiscard]] std::string_view GetMessage() const noexcept;
    [[nodiscard]] const std::source_location& GetLocation() const noexcept;

private:
    AssertionFailureKind m_kind;
    std::string m_expression;
    std::string m_message;
    std::source_location m_location;
};

using AssertionFailureHandler = void (*)(const AssertionFailure& failure);

[[nodiscard]] AssertionFailureHandler SetAssertionFailureHandler(
    AssertionFailureHandler handler) noexcept;

[[nodiscard]] AssertionFailureHandler SetVerifyFailureHandler(
    AssertionFailureHandler handler) noexcept;

class ScopedAssertionFailureHandler final
{
public:
    explicit ScopedAssertionFailureHandler(AssertionFailureHandler handler) noexcept;
    ~ScopedAssertionFailureHandler();

    ScopedAssertionFailureHandler(const ScopedAssertionFailureHandler&) = delete;
    ScopedAssertionFailureHandler& operator=(const ScopedAssertionFailureHandler&) = delete;
    ScopedAssertionFailureHandler(ScopedAssertionFailureHandler&&) = delete;
    ScopedAssertionFailureHandler& operator=(ScopedAssertionFailureHandler&&) = delete;

private:
    AssertionFailureHandler m_previousHandler;
};

class ScopedVerifyFailureHandler final
{
public:
    explicit ScopedVerifyFailureHandler(AssertionFailureHandler handler) noexcept;
    ~ScopedVerifyFailureHandler();

    ScopedVerifyFailureHandler(const ScopedVerifyFailureHandler&) = delete;
    ScopedVerifyFailureHandler& operator=(const ScopedVerifyFailureHandler&) = delete;
    ScopedVerifyFailureHandler(ScopedVerifyFailureHandler&&) = delete;
    ScopedVerifyFailureHandler& operator=(ScopedVerifyFailureHandler&&) = delete;

private:
    AssertionFailureHandler m_previousHandler;
};

[[nodiscard]] std::string FormatAssertionFailure(const AssertionFailure& failure);

[[noreturn]] void HandleAssertionFailure(
    std::string_view expression, std::string message,
    std::source_location location = std::source_location::current());

template <typename... Args>
[[noreturn]] void HandleAssertionFailureFormatted(std::string_view expression,
                                                  std::source_location location,
                                                  std::format_string<Args...> messageFormat,
                                                  Args&&... args)
{
    HandleAssertionFailure(expression, std::format(messageFormat, std::forward<Args>(args)...),
                           location);
}

[[noreturn]] void HandleVerifyFailure(
    std::string_view expression, std::string message,
    std::source_location location = std::source_location::current());

template <typename... Args>
[[noreturn]] void HandleVerifyFailureFormatted(std::string_view expression,
                                               std::source_location location,
                                               std::format_string<Args...> messageFormat,
                                               Args&&... args)
{
    HandleVerifyFailure(expression, std::format(messageFormat, std::forward<Args>(args)...),
                        location);
}

[[noreturn]] void HandleUnreachable(
    std::string message, std::source_location location = std::source_location::current());

template <typename... Args>
[[noreturn]] void HandleUnreachableFormatted(std::source_location location,
                                             std::format_string<Args...> messageFormat,
                                             Args&&... args)
{
    HandleUnreachable(std::format(messageFormat, std::forward<Args>(args)...), location);
}

void DebugBreak() noexcept;
} // namespace pond::core

#if defined(NDEBUG)
#define PONDER_ASSERT(expression) ((void)0)
#define PONDER_ASSERT_MESSAGE(expression, ...) ((void)0)
#else
#define PONDER_ASSERT(expression)                                                                  \
    do                                                                                             \
    {                                                                                              \
        if (!(expression))                                                                         \
        {                                                                                          \
            ::pond::core::HandleAssertionFailure(#expression, std::string{},                       \
                                                 std::source_location::current());                 \
        }                                                                                          \
    } while (false)

#define PONDER_ASSERT_MESSAGE(expression, ...)                                                     \
    do                                                                                             \
    {                                                                                              \
        if (!(expression))                                                                         \
        {                                                                                          \
            ::pond::core::HandleAssertionFailureFormatted(                                         \
                #expression, std::source_location::current(), __VA_ARGS__);                        \
        }                                                                                          \
    } while (false)
#endif

#define PONDER_VERIFY(expression, ...)                                                             \
    do                                                                                             \
    {                                                                                              \
        if (!(expression))                                                                         \
        {                                                                                          \
            ::pond::core::HandleVerifyFailureFormatted(                                            \
                #expression, std::source_location::current(), __VA_ARGS__);                        \
        }                                                                                          \
    } while (false)

#define PONDER_UNREACHABLE(...)                                                                    \
    ::pond::core::HandleUnreachableFormatted(std::source_location::current(), __VA_ARGS__)

#define PONDER_DEBUG_BREAK() ::pond::core::DebugBreak()
