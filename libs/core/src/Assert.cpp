#include <ponder/core/Assert.hpp>
#include <ponder/core/Log.hpp>
#include <ponder/core/PonderException.hpp>

#include <atomic>
#include <cstdlib>
#include <format>
#include <utility>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if !defined(_MSC_VER)
#include <csignal>
#endif

namespace pond::core
{
namespace
{
std::string_view GetFailurePrefix(AssertionFailureKind kind) noexcept
{
    switch (kind)
    {
    case AssertionFailureKind::Assertion:
        return "Assertion failed";
    case AssertionFailureKind::Verify:
        return "Verification failed";
    case AssertionFailureKind::Unreachable:
        return "Unreachable code reached";
    }

    return "Failure";
}

void DefaultAssertionFailureHandler(const AssertionFailure& failure)
{
    LogMessage(LogLevel::Fatal, FormatAssertionFailure(failure), failure.GetLocation());
}

void DefaultVerifyFailureHandler(const AssertionFailure& failure)
{
    LogMessage(LogLevel::Error, FormatAssertionFailure(failure), failure.GetLocation());
}

std::atomic<AssertionFailureHandler> assertionFailureHandler{DefaultAssertionFailureHandler};
std::atomic<AssertionFailureHandler> verifyFailureHandler{DefaultVerifyFailureHandler};
} // namespace

AssertionFailure::AssertionFailure(AssertionFailureKind kind, std::string expression,
                                   std::string message, std::source_location location)
    : m_kind(kind), m_expression(std::move(expression)), m_message(std::move(message)),
      m_location(location)
{
}

AssertionFailureKind AssertionFailure::GetKind() const noexcept
{
    return m_kind;
}

std::string_view AssertionFailure::GetExpression() const noexcept
{
    return m_expression;
}

std::string_view AssertionFailure::GetMessage() const noexcept
{
    return m_message;
}

const std::source_location& AssertionFailure::GetLocation() const noexcept
{
    return m_location;
}

AssertionFailureHandler SetAssertionFailureHandler(AssertionFailureHandler handler) noexcept
{
    return assertionFailureHandler.exchange(handler ? handler : DefaultAssertionFailureHandler);
}

AssertionFailureHandler SetVerifyFailureHandler(AssertionFailureHandler handler) noexcept
{
    return verifyFailureHandler.exchange(handler ? handler : DefaultVerifyFailureHandler);
}

ScopedAssertionFailureHandler::ScopedAssertionFailureHandler(
    AssertionFailureHandler handler) noexcept
    : m_previousHandler(SetAssertionFailureHandler(handler))
{
}

ScopedAssertionFailureHandler::~ScopedAssertionFailureHandler()
{
    (void)SetAssertionFailureHandler(m_previousHandler);
}

ScopedVerifyFailureHandler::ScopedVerifyFailureHandler(AssertionFailureHandler handler) noexcept
    : m_previousHandler(SetVerifyFailureHandler(handler))
{
}

ScopedVerifyFailureHandler::~ScopedVerifyFailureHandler()
{
    (void)SetVerifyFailureHandler(m_previousHandler);
}

std::string FormatAssertionFailure(const AssertionFailure& failure)
{
    std::string formatted{GetFailurePrefix(failure.GetKind())};

    if (!failure.GetExpression().empty())
    {
        formatted += std::format(": {}", failure.GetExpression());
    }

    if (!failure.GetMessage().empty())
    {
        formatted += std::format(" ({})", failure.GetMessage());
    }

    return formatted;
}

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

[[noreturn]] void HandleAssertionFailure(std::string_view expression, std::string message,
                                         std::source_location location)
{
    const AssertionFailure failure{AssertionFailureKind::Assertion, std::string{expression},
                                   std::move(message), location};

    assertionFailureHandler.load()(failure);

    DebugBreak();
    std::abort();
}

[[noreturn]] void HandleVerifyFailure(std::string_view expression, std::string message,
                                      std::source_location location)
{
    const AssertionFailure failure{AssertionFailureKind::Verify, std::string{expression},
                                   std::move(message), location};

    verifyFailureHandler.load()(failure);

    throw MakePonderException(FormatAssertionFailure(failure), location);
}

[[noreturn]] void HandleUnreachable(std::string message, std::source_location location)
{
    const AssertionFailure failure{AssertionFailureKind::Unreachable, std::string{},
                                   std::move(message), location};

    assertionFailureHandler.load()(failure);

    DebugBreak();
    std::abort();
}
} // namespace pond::core
