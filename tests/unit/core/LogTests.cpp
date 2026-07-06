#include <ponder/core/Log.hpp>

#include <atomic>
#include <format>
#include <gtest/gtest.h>
#include <source_location>
#include <string>
#include <string_view>

namespace
{
struct ThrowsDuringFormatting final
{
};

int g_fatalHandlerCallCount = 0;
std::string g_fatalCategory;
std::string g_fatalMessage;
std::source_location g_fatalLocation;
std::atomic_bool g_throwDuringFormatting{true};

void CaptureFatalLog(std::string_view category, std::string_view message,
                     std::source_location location)
{
    ++g_fatalHandlerCallCount;
    g_fatalCategory = category;
    g_fatalMessage = message;
    g_fatalLocation = location;
}
} // namespace

template <> struct std::formatter<ThrowsDuringFormatting>
{
    constexpr auto parse(std::format_parse_context& context)
    {
        return context.begin();
    }

    auto format(const ThrowsDuringFormatting&, std::format_context& context) const
        -> std::format_context::iterator
    {
        if (g_throwDuringFormatting.load())
        {
            throw std::format_error{"intentional formatter failure"};
        }

        return context.out();
    }
};

namespace
{
TEST(LogLevelTests, NamesAndOrderingAreDefined)
{
    EXPECT_TRUE(
        pond::core::IsLogLevelEnabled(pond::core::LogLevel::Fatal, pond::core::LogLevel::Trace));
    EXPECT_TRUE(
        pond::core::IsLogLevelEnabled(pond::core::LogLevel::Error, pond::core::LogLevel::Warning));
    EXPECT_FALSE(
        pond::core::IsLogLevelEnabled(pond::core::LogLevel::Debug, pond::core::LogLevel::Info));

    EXPECT_EQ(pond::core::GetLogLevelName(pond::core::LogLevel::Trace), "trace");
    EXPECT_EQ(pond::core::GetLogLevelName(pond::core::LogLevel::Debug), "debug");
    EXPECT_EQ(pond::core::GetLogLevelName(pond::core::LogLevel::Info), "info");
    EXPECT_EQ(pond::core::GetLogLevelName(pond::core::LogLevel::Warning), "warning");
    EXPECT_EQ(pond::core::GetLogLevelName(pond::core::LogLevel::Error), "error");
    EXPECT_EQ(pond::core::GetLogLevelName(pond::core::LogLevel::Fatal), "fatal");
}

TEST(LogFrontendTests, SupportsFormattedMessagesAndCategories)
{
    EXPECT_NO_THROW(LOG_TRACE("trace {}", 1));
    EXPECT_NO_THROW(LOG_DEBUG("debug {}", 2));
    EXPECT_NO_THROW(LOG_INFO("info {}", 3));
    EXPECT_NO_THROW(LOG_WARNING("warning {}", 4));
    EXPECT_NO_THROW(LOG_ERROR("error {}", 5));
    EXPECT_NO_THROW(LOG_INFO_CATEGORY("core-test", "category {}", 6));

    pond::core::FlushLog();
}

TEST(LogFrontendTests, FormattingFailuresAreReportedWithoutThrowing)
{
    EXPECT_NO_THROW(LOG_ERROR("{}", ThrowsDuringFormatting{}));
    pond::core::FlushLog();
}

TEST(LogFrontendTests, FatalLogsFlushAndInvokeFatalHandler)
{
    g_fatalHandlerCallCount = 0;
    g_fatalCategory.clear();
    g_fatalMessage.clear();
    g_fatalLocation = {};

    const auto previousHandler = pond::core::SetLogFatalHandler(CaptureFatalLog);
    const auto expectedLine = __LINE__ + 1;
    LOG_FATAL_CATEGORY("fatal-test", "fatal {}", 7);
    (void)pond::core::SetLogFatalHandler(previousHandler);

    EXPECT_EQ(g_fatalHandlerCallCount, 1);
    EXPECT_EQ(g_fatalCategory, "fatal-test");
    EXPECT_EQ(g_fatalMessage, "fatal 7");
    EXPECT_EQ(g_fatalLocation.line(), expectedLine);
}

TEST(LogFrontendTests, MinimumLevelCanBeChanged)
{
    const auto previousLevel = pond::core::GetMinimumLogLevel();
    pond::core::SetMinimumLogLevel(pond::core::LogLevel::Warning);
    LOG_INFO("filtered info");
    LOG_WARNING("visible warning");
    pond::core::FlushLog();
    pond::core::SetMinimumLogLevel(previousLevel);
}
} // namespace
