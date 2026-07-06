#include <ponder/core/Log.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <format>
#include <gtest/gtest.h>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

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
std::mutex g_primaryCaptureMutex;
std::mutex g_secondaryCaptureMutex;
std::vector<pond::core::LogEntry> g_primaryCapturedEntries;
std::vector<pond::core::LogEntry> g_secondaryCapturedEntries;

void ResetFatalCapture()
{
    g_fatalHandlerCallCount = 0;
    g_fatalCategory.clear();
    g_fatalMessage.clear();
    g_fatalLocation = {};
}

void CaptureFatalLog(std::string_view category, std::string_view message,
                     std::source_location location)
{
    ++g_fatalHandlerCallCount;
    g_fatalCategory = category;
    g_fatalMessage = message;
    g_fatalLocation = location;
}

void CapturePrimaryLogEntry(const pond::core::LogEntry& entry)
{
    const std::scoped_lock lock{g_primaryCaptureMutex};
    g_primaryCapturedEntries.push_back(entry);
}

void CaptureSecondaryLogEntry(const pond::core::LogEntry& entry)
{
    const std::scoped_lock lock{g_secondaryCaptureMutex};
    g_secondaryCapturedEntries.push_back(entry);
}

void ClearCapturedLogEntries()
{
    const std::scoped_lock lock{g_primaryCaptureMutex, g_secondaryCaptureMutex};
    g_primaryCapturedEntries.clear();
    g_secondaryCapturedEntries.clear();
}

std::vector<pond::core::LogEntry> GetPrimaryCapturedEntries()
{
    const std::scoped_lock lock{g_primaryCaptureMutex};
    return g_primaryCapturedEntries;
}

std::vector<pond::core::LogEntry> GetSecondaryCapturedEntries()
{
    const std::scoped_lock lock{g_secondaryCaptureMutex};
    return g_secondaryCapturedEntries;
}

[[nodiscard]] bool EndsWith(std::string_view value, std::string_view suffix) noexcept
{
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}
} // namespace

template <>
struct std::formatter<ThrowsDuringFormatting>
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

TEST(LogCaptureTests, CapturesMessageMetadataAndTimestamp)
{
    ClearCapturedLogEntries();
    const pond::core::ScopedMinimumLogLevel minimumLevel{pond::core::LogLevel::Trace};
    const pond::core::ScopedLogSinkHandler sink{CapturePrimaryLogEntry};

    const auto beforeLog = std::chrono::system_clock::now();
    const auto expectedLine = __LINE__ + 1;
    LOG_INFO_CATEGORY("core-test", "value {}", 42);
    pond::core::FlushLog();
    const auto afterLog = std::chrono::system_clock::now();

    const auto entries = GetPrimaryCapturedEntries();
    ASSERT_EQ(entries.size(), 1);

    const auto& entry = entries.front();
    EXPECT_EQ(entry.GetLevel(), pond::core::LogLevel::Info);
    EXPECT_EQ(entry.GetCategory(), "core-test");
    EXPECT_EQ(entry.GetMessage(), "value 42");
    EXPECT_GE(entry.GetTimestamp(), beforeLog);
    EXPECT_LE(entry.GetTimestamp(), afterLog);
    EXPECT_EQ(entry.GetLocation().line(), expectedLine);
    EXPECT_TRUE(EndsWith(entry.GetLocation().file_name(), "LogTests.cpp"));
    EXPECT_NE(std::string_view{entry.GetLocation().function_name()}.find(
                  "CapturesMessageMetadataAndTimestamp"),
              std::string_view::npos);
}

TEST(LogCaptureTests, CapturesEachLogLevelInQueueOrder)
{
    ClearCapturedLogEntries();
    const pond::core::ScopedMinimumLogLevel minimumLevel{pond::core::LogLevel::Trace};
    const pond::core::ScopedLogSinkHandler sink{CapturePrimaryLogEntry};

    LOG_TRACE_CATEGORY("levels", "trace");
    LOG_DEBUG_CATEGORY("levels", "debug");
    LOG_INFO_CATEGORY("levels", "info");
    LOG_WARNING_CATEGORY("levels", "warning");
    LOG_ERROR_CATEGORY("levels", "error");
    LOG_FATAL_CATEGORY("levels", "fatal");
    pond::core::FlushLog();

    const auto entries = GetPrimaryCapturedEntries();
    ASSERT_EQ(entries.size(), 6);

    EXPECT_EQ(entries[0].GetLevel(), pond::core::LogLevel::Trace);
    EXPECT_EQ(entries[1].GetLevel(), pond::core::LogLevel::Debug);
    EXPECT_EQ(entries[2].GetLevel(), pond::core::LogLevel::Info);
    EXPECT_EQ(entries[3].GetLevel(), pond::core::LogLevel::Warning);
    EXPECT_EQ(entries[4].GetLevel(), pond::core::LogLevel::Error);
    EXPECT_EQ(entries[5].GetLevel(), pond::core::LogLevel::Fatal);

    EXPECT_EQ(entries[0].GetMessage(), "trace");
    EXPECT_EQ(entries[5].GetMessage(), "fatal");
    EXPECT_TRUE(std::ranges::all_of(entries,
                                    [](const pond::core::LogEntry& entry)
                                    {
                                        return entry.GetCategory() == "levels";
                                    }));
}

TEST(LogCaptureTests, ScopedSinkRestoresPreviousHandler)
{
    ClearCapturedLogEntries();
    const pond::core::ScopedMinimumLogLevel minimumLevel{pond::core::LogLevel::Trace};
    const pond::core::ScopedLogSinkHandler primarySink{CapturePrimaryLogEntry};

    LOG_INFO("primary one");

    {
        const pond::core::ScopedLogSinkHandler secondarySink{CaptureSecondaryLogEntry};
        LOG_INFO("secondary");
    }

    LOG_INFO("primary two");
    pond::core::FlushLog();

    const auto primaryEntries = GetPrimaryCapturedEntries();
    const auto secondaryEntries = GetSecondaryCapturedEntries();

    ASSERT_EQ(primaryEntries.size(), 2);
    ASSERT_EQ(secondaryEntries.size(), 1);
    EXPECT_EQ(primaryEntries[0].GetMessage(), "primary one");
    EXPECT_EQ(primaryEntries[1].GetMessage(), "primary two");
    EXPECT_EQ(secondaryEntries[0].GetMessage(), "secondary");
}

TEST(LogCaptureTests, FormattingFailuresAreCapturedAsLoggingDiagnostics)
{
    ClearCapturedLogEntries();
    const pond::core::ScopedMinimumLogLevel minimumLevel{pond::core::LogLevel::Trace};
    const pond::core::ScopedLogSinkHandler sink{CapturePrimaryLogEntry};

    EXPECT_NO_THROW(LOG_ERROR_CATEGORY("formatting", "{}", ThrowsDuringFormatting{}));
    pond::core::FlushLog();

    const auto entries = GetPrimaryCapturedEntries();
    ASSERT_EQ(entries.size(), 1);
    EXPECT_EQ(entries[0].GetLevel(), pond::core::LogLevel::Error);
    EXPECT_EQ(entries[0].GetCategory(), "logging");
    EXPECT_NE(entries[0].GetMessage().find("Failed to format error log message"),
              std::string_view::npos);
    EXPECT_NE(entries[0].GetMessage().find("formatting"), std::string_view::npos);
}

TEST(LogCaptureTests, MinimumLevelFiltersBeforeFormatting)
{
    ClearCapturedLogEntries();
    const pond::core::ScopedMinimumLogLevel minimumLevel{pond::core::LogLevel::Warning};
    const pond::core::ScopedLogSinkHandler sink{CapturePrimaryLogEntry};

    LOG_INFO("{}", ThrowsDuringFormatting{});
    LOG_WARNING("visible warning");
    pond::core::FlushLog();

    const auto entries = GetPrimaryCapturedEntries();
    ASSERT_EQ(entries.size(), 1);
    EXPECT_EQ(entries[0].GetLevel(), pond::core::LogLevel::Warning);
    EXPECT_EQ(entries[0].GetMessage(), "visible warning");
}

TEST(LogCaptureTests, FatalHandlerScopeRestoresPreviousHandler)
{
    ResetFatalCapture();
    const pond::core::ScopedMinimumLogLevel minimumLevel{pond::core::LogLevel::Trace};

    {
        const pond::core::ScopedLogFatalHandler fatalHandler{CaptureFatalLog};
        const auto expectedLine = __LINE__ + 1;
        LOG_FATAL_CATEGORY("fatal-test", "fatal {}", 7);

        EXPECT_EQ(g_fatalHandlerCallCount, 1);
        EXPECT_EQ(g_fatalCategory, "fatal-test");
        EXPECT_EQ(g_fatalMessage, "fatal 7");
        EXPECT_EQ(g_fatalLocation.line(), expectedLine);
    }

    LOG_FATAL("after restore");
    EXPECT_EQ(g_fatalHandlerCallCount, 1);
}

TEST(LogCaptureTests, ScopedMinimumLevelRestoresPreviousLevel)
{
    const auto originalLevel = pond::core::GetMinimumLogLevel();

    {
        const pond::core::ScopedMinimumLogLevel minimumLevel{pond::core::LogLevel::Error};
        EXPECT_EQ(pond::core::GetMinimumLogLevel(), pond::core::LogLevel::Error);
    }

    EXPECT_EQ(pond::core::GetMinimumLogLevel(), originalLevel);
}

TEST(LogCaptureTests, FlushMakesQueuedLoggingDeterministic)
{
    ClearCapturedLogEntries();
    const pond::core::ScopedMinimumLogLevel minimumLevel{pond::core::LogLevel::Trace};
    const pond::core::ScopedLogSinkHandler sink{CapturePrimaryLogEntry};

    constexpr int kMessageCount = 64;
    for (int index = 0; index < kMessageCount; ++index)
    {
        LOG_DEBUG_CATEGORY("queue", "message {}", index);
    }

    pond::core::FlushLog();

    const auto entries = GetPrimaryCapturedEntries();
    ASSERT_EQ(entries.size(), kMessageCount);
    for (int index = 0; index < kMessageCount; ++index)
    {
        EXPECT_EQ(entries[static_cast<std::size_t>(index)].GetMessage(),
                  std::format("message {}", index));
    }
}
} // namespace
