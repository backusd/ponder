#include <ponder/core/Exception.hpp>

#include <concepts>
#include <exception>
#include <format>
#include <gtest/gtest.h>
#include <ostream>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace exception_test
{
struct FormattableData
{
    int value;
};

struct StreamableData
{
    int value;
};

struct OpaqueData
{
    int value;
};

inline std::ostream& operator<<(std::ostream& output, const StreamableData& data)
{
    return output << "streamed:" << data.value;
}
} // namespace exception_test

namespace std
{
template <>
struct formatter<exception_test::FormattableData> : formatter<string>
{
    template <typename FormatContext>
    auto format(const exception_test::FormattableData& data, FormatContext& context) const
    {
        return formatter<string>::format(std::format("formatted:{}", data.value), context);
    }
};
} // namespace std

namespace
{
template <typename Value>
concept Streamable = requires(std::ostream& output, const Value& value) { output << value; };

constexpr bool ExplicitStackTraceConstructorIsConstexpr()
{
    const auto location = std::source_location::current();
    const ponder::core::Exception exception{"constexpr boom", ponder::core::StackTrace{}, location};

    return exception.GetMessage() == std::string_view{"constexpr boom"} && exception.GetLocation().line() == location.line();
}

constexpr bool MakeExceptionIsConstexpr()
{
    const auto location = std::source_location::current();
    const auto exception = ponder::core::MakeException("constexpr helper", location);

    return exception.GetMessage() == std::string_view{"constexpr helper"} && exception.GetLocation().line() == location.line();
}

constexpr bool MakeExceptionWithDataIsConstexpr()
{
    const auto location = std::source_location::current();
    auto exception = ponder::core::MakeExceptionWithData("constexpr data", 42, location);

    return exception.GetMessage() == std::string_view{"constexpr data"} && exception.GetData() == 42 &&
           exception.GetLocation().line() == location.line();
}

static_assert(ExplicitStackTraceConstructorIsConstexpr());
static_assert(MakeExceptionIsConstexpr());
static_assert(MakeExceptionWithDataIsConstexpr());
static_assert(noexcept(std::declval<const ponder::core::Exception&>().GetMessage()));
static_assert(noexcept(std::declval<const ponder::core::Exception&>().GetLocation()));
static_assert(noexcept(std::declval<const ponder::core::Exception&>().GetStackTrace()));
static_assert(noexcept(std::declval<ponder::core::ExceptionWithData<int>&>().GetData()));
static_assert(noexcept(std::declval<const ponder::core::ExceptionWithData<int>&>().GetData()));
static_assert(noexcept(std::declval<ponder::core::ExceptionWithData<int>&&>().GetData()));
static_assert(std::same_as<ponder::core::ExceptionWithData<int>::DataType, int>);
static_assert(std::derived_from<ponder::core::ExceptionWithData<int>, ponder::core::Exception>);
static_assert(std::has_virtual_destructor_v<ponder::core::Exception>);
static_assert(std::is_nothrow_move_constructible_v<ponder::core::Exception>);
static_assert(std::is_nothrow_move_assignable_v<ponder::core::Exception>);
static_assert(std::formattable<ponder::core::ExceptionWithData<exception_test::OpaqueData>, char>);
static_assert(Streamable<ponder::core::ExceptionWithData<exception_test::OpaqueData>>);

TEST(ExceptionTests, IsStandaloneProjectType)
{
    static_assert(!std::derived_from<ponder::core::Exception, std::exception>);
}

TEST(ExceptionTests, StoresMessageLocationAndStackTraceFallback)
{
    const auto location = std::source_location::current();
    const ponder::core::Exception exception{"boom", location};

    EXPECT_EQ(exception.GetMessage(), std::string_view{"boom"});
    EXPECT_STREQ(exception.GetLocation().file_name(), location.file_name());
    EXPECT_EQ(exception.GetLocation().line(), location.line());
    EXPECT_EQ(exception.GetStackTrace().IsEmpty(), exception.GetStackTrace().GetFrames().empty());

    if (!ponder::core::IsStackTraceCaptureSupported())
    {
        EXPECT_TRUE(exception.GetStackTrace().IsEmpty());
    }
}

TEST(ExceptionTests, AcceptsExplicitStackTrace)
{
    ponder::core::StackTrace stackTrace{{"frame A", "frame B"}};

    const ponder::core::Exception exception{std::string{"boom"}, std::move(stackTrace)};

    ASSERT_EQ(exception.GetStackTrace().GetFrames().size(), 2U);
    EXPECT_EQ(exception.GetStackTrace().GetFrames()[0], "frame A");
    EXPECT_EQ(exception.GetStackTrace().GetFrames()[1], "frame B");
}

TEST(ExceptionTests, ExceptionMacroFormatsMessageAndCapturesLocation)
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
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"formatted value 42"});
        EXPECT_STREQ(exception.GetLocation().file_name(), __FILE__);
        EXPECT_EQ(exception.GetLocation().line(), expectedLine);
        return;
    }

    FAIL() << "PONDER_EXCEPTION should throw";
}

TEST(ExceptionWithDataTests, MacroPreservesTypeDataMessageAndLocation)
{
    const auto expectedLine = __LINE__ + 3;
    void (*throwFunction)() = []()
    {
        throw PONDER_EXCEPTION_WITH_DATA(std::string{"payload"}, "formatted {} {}", "value", 42);
    };

    try
    {
        throwFunction();
    }
    catch (const ponder::core::ExceptionWithData<std::string>& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"formatted value 42"});
        EXPECT_EQ(exception.GetData(), "payload");
        EXPECT_STREQ(exception.GetLocation().file_name(), __FILE__);
        EXPECT_EQ(exception.GetLocation().line(), expectedLine);
        return;
    }

    FAIL() << "PONDER_EXCEPTION_WITH_DATA should preserve the data type";
}

TEST(ExceptionWithDataTests, CanBeCaughtAsBaseException)
{
    void (*throwFunction)() = []()
    {
        throw ponder::core::ExceptionWithData<int>{"typed failure", 42};
    };

    try
    {
        throwFunction();
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"typed failure"});
        return;
    }

    FAIL() << "ExceptionWithData should be catchable as Exception";
}

TEST(ExceptionWithDataTests, FactoryOwnsCopiedLvalueData)
{
    std::string payload{"original"};
    const auto exception = ponder::core::MakeExceptionWithData("typed failure", payload);

    payload = "changed";

    EXPECT_EQ(exception.GetData(), "original");
}

TEST(ExceptionWithDataTests, FormatsAndStreamsSupportedData)
{
    const ponder::core::ExceptionWithData<int> exception{"number", 42};
    std::ostringstream stream;
    stream << exception;

    EXPECT_EQ(std::format("{}", exception), "number (data: 42)");
    EXPECT_EQ(stream.str(), "number (data: 42)");
}

TEST(ExceptionWithDataTests, DegradesFormattingAndStreamingIndependently)
{
    const ponder::core::ExceptionWithData<exception_test::FormattableData> formattable{"formatted", {7}};
    const ponder::core::ExceptionWithData<exception_test::StreamableData> streamable{"streamed", {8}};
    const ponder::core::ExceptionWithData<exception_test::OpaqueData> opaque{"opaque", {9}};
    std::ostringstream formattableStream;
    std::ostringstream streamableStream;
    std::ostringstream opaqueStream;

    formattableStream << formattable;
    streamableStream << streamable;
    opaqueStream << opaque;

    EXPECT_EQ(std::format("{}", formattable), "formatted (data: formatted:7)");
    EXPECT_EQ(formattableStream.str(), "formatted (data: <unprintable>)");
    EXPECT_EQ(std::format("{}", streamable), "streamed (data: <unprintable>)");
    EXPECT_EQ(streamableStream.str(), "streamed (data: streamed:8)");
    EXPECT_EQ(std::format("{}", opaque), "opaque (data: <unprintable>)");
    EXPECT_EQ(opaqueStream.str(), "opaque (data: <unprintable>)");
}
} // namespace
