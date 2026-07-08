#include <ponder/core/PonderException.hpp>

#include <concepts>
#include <exception>
#include <gtest/gtest.h>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace
{
constexpr bool ExplicitStackTraceConstructorIsConstexpr()
{
    const auto location = std::source_location::current();
    const pond::core::PonderException exception{"constexpr boom", pond::core::StackTrace{},
                                                location};

    return exception.GetMessage() == std::string_view{"constexpr boom"} &&
           exception.GetLocation().line() == location.line();
}

constexpr bool MakePonderExceptionIsConstexpr()
{
    const auto location = std::source_location::current();
    const auto exception = pond::core::MakePonderException("constexpr helper", location);

    return exception.GetMessage() == std::string_view{"constexpr helper"} &&
           exception.GetLocation().line() == location.line();
}

static_assert(ExplicitStackTraceConstructorIsConstexpr());
static_assert(MakePonderExceptionIsConstexpr());
static_assert(noexcept(std::declval<const pond::core::PonderException&>().GetMessage()));
static_assert(noexcept(std::declval<const pond::core::PonderException&>().GetLocation()));
static_assert(noexcept(std::declval<const pond::core::PonderException&>().GetStackTrace()));
TEST(PonderExceptionTests, IsStandaloneProjectType)
{
    static_assert(!std::derived_from<pond::core::PonderException, std::exception>);
}

TEST(PonderExceptionTests, StoresMessageLocationAndStackTraceFallback)
{
    const auto location = std::source_location::current();
    const pond::core::PonderException exception{"boom", location};

    EXPECT_EQ(exception.GetMessage(), std::string_view{"boom"});
    EXPECT_STREQ(exception.GetLocation().file_name(), location.file_name());
    EXPECT_EQ(exception.GetLocation().line(), location.line());
    EXPECT_EQ(exception.GetStackTrace().IsEmpty(), exception.GetStackTrace().GetFrames().empty());

    if (!pond::core::IsStackTraceCaptureSupported())
    {
        EXPECT_TRUE(exception.GetStackTrace().IsEmpty());
    }
}

TEST(PonderExceptionTests, AcceptsExplicitStackTrace)
{
    pond::core::StackTrace stackTrace{{"frame A", "frame B"}};

    const pond::core::PonderException exception{std::string{"boom"}, std::move(stackTrace)};

    ASSERT_EQ(exception.GetStackTrace().GetFrames().size(), 2U);
    EXPECT_EQ(exception.GetStackTrace().GetFrames()[0], "frame A");
    EXPECT_EQ(exception.GetStackTrace().GetFrames()[1], "frame B");
}

TEST(PonderExceptionTests, ThrowFunctionThrowsStandaloneException)
{
    using ThrowFunction = void (*)(std::string, std::source_location);

    const auto location = std::source_location::current();
    const ThrowFunction throwFunction = &pond::core::ThrowPonderException;

    try
    {
        throwFunction("thrown", location);
    }
    catch (const pond::core::PonderException& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"thrown"});
        EXPECT_STREQ(exception.GetLocation().file_name(), location.file_name());
        EXPECT_EQ(exception.GetLocation().line(), location.line());
        return;
    }

    FAIL() << "ThrowPonderException should throw";
}

TEST(PonderExceptionTests, ExceptionMacroFormatsMessageAndCapturesLocation)
{
    const auto expectedLine = __LINE__ + 3;
    void (*throwFunction)() = []()
    {
        throw PONDER_EXCEPTION("formatted {} {}", "value", 42);
    };

    try
    {
        throwFunction();
    }
    catch (const pond::core::PonderException& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"formatted value 42"});
        EXPECT_STREQ(exception.GetLocation().file_name(), __FILE__);
        EXPECT_EQ(exception.GetLocation().line(), expectedLine);
        return;
    }

    FAIL() << "PONDER_EXCEPTION should throw";
}
} // namespace
