#pragma once

#include <chrono>
#include <exception>
#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace pond::core
{
enum class LogLevel
{
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Fatal = 5
};

class LogEntry final
{
public:
    LogEntry() = default;
    LogEntry(LogLevel level, std::string category, std::string message,
             std::chrono::system_clock::time_point timestamp, std::source_location location);
    LogEntry(const LogEntry&) = default;
    LogEntry(LogEntry&&) noexcept = default;
    LogEntry& operator=(const LogEntry&) = default;
    LogEntry& operator=(LogEntry&&) noexcept = default;
    ~LogEntry() = default;

    [[nodiscard]] LogLevel GetLevel() const noexcept;
    [[nodiscard]] std::string_view GetCategory() const noexcept;
    [[nodiscard]] std::string_view GetMessage() const noexcept;
    [[nodiscard]] std::chrono::system_clock::time_point GetTimestamp() const noexcept;
    [[nodiscard]] const std::source_location& GetLocation() const noexcept;

private:
    LogLevel m_level{LogLevel::Info};
    std::string m_category;
    std::string m_message;
    std::chrono::system_clock::time_point m_timestamp;
    std::source_location m_location;
};

using LogSinkHandler = void (*)(const LogEntry& entry);
using LogFatalHandler = void (*)(std::string_view category, std::string_view message,
                                 std::source_location location);

[[nodiscard]] std::string_view GetLogLevelName(LogLevel level) noexcept;
[[nodiscard]] constexpr bool IsLogLevelEnabled(LogLevel level, LogLevel minimumLevel) noexcept
{
    return static_cast<int>(level) >= static_cast<int>(minimumLevel);
}

void LogMessage(LogLevel level, std::string_view message,
                std::source_location location = std::source_location::current()) noexcept;

void LogMessage(LogLevel level, std::string_view category, std::string_view message,
                std::source_location location = std::source_location::current()) noexcept;

[[nodiscard]] LogLevel GetMinimumLogLevel() noexcept;
void SetMinimumLogLevel(LogLevel level) noexcept;
void FlushLog() noexcept;
void ShutdownLogging() noexcept;

[[nodiscard]] LogSinkHandler SetLogSinkHandler(LogSinkHandler handler) noexcept;
[[nodiscard]] LogFatalHandler SetLogFatalHandler(LogFatalHandler handler) noexcept;

class ScopedLogSinkHandler final
{
public:
    explicit ScopedLogSinkHandler(LogSinkHandler handler) noexcept;
    ~ScopedLogSinkHandler();

    ScopedLogSinkHandler(const ScopedLogSinkHandler&) = delete;
    ScopedLogSinkHandler& operator=(const ScopedLogSinkHandler&) = delete;
    ScopedLogSinkHandler(ScopedLogSinkHandler&&) = delete;
    ScopedLogSinkHandler& operator=(ScopedLogSinkHandler&&) = delete;

private:
    LogSinkHandler m_previousHandler{nullptr};
};

class ScopedLogFatalHandler final
{
public:
    explicit ScopedLogFatalHandler(LogFatalHandler handler) noexcept;
    ~ScopedLogFatalHandler();

    ScopedLogFatalHandler(const ScopedLogFatalHandler&) = delete;
    ScopedLogFatalHandler& operator=(const ScopedLogFatalHandler&) = delete;
    ScopedLogFatalHandler(ScopedLogFatalHandler&&) = delete;
    ScopedLogFatalHandler& operator=(ScopedLogFatalHandler&&) = delete;

private:
    LogFatalHandler m_previousHandler{nullptr};
};

class ScopedMinimumLogLevel final
{
public:
    explicit ScopedMinimumLogLevel(LogLevel level) noexcept;
    ~ScopedMinimumLogLevel();

    ScopedMinimumLogLevel(const ScopedMinimumLogLevel&) = delete;
    ScopedMinimumLogLevel& operator=(const ScopedMinimumLogLevel&) = delete;
    ScopedMinimumLogLevel(ScopedMinimumLogLevel&&) = delete;
    ScopedMinimumLogLevel& operator=(ScopedMinimumLogLevel&&) = delete;

private:
    LogLevel m_previousLevel{LogLevel::Trace};
};

void LogFormattingFailure(LogLevel level, std::string_view category, std::string_view reason,
                          std::source_location location) noexcept;

template <typename... Args>
void LogFormattedWithCategory(LogLevel level, std::string_view category,
                              std::source_location location,
                              std::format_string<Args...> messageFormat, Args&&... args) noexcept
{
    if (!IsLogLevelEnabled(level, GetMinimumLogLevel()))
    {
        return;
    }

    try
    {
        LogMessage(level, category, std::format(messageFormat, std::forward<Args>(args)...),
                   location);
    }
    catch (const std::exception& exception)
    {
        LogFormattingFailure(level, category, exception.what(), location);
    }
    catch (...)
    {
        LogFormattingFailure(level, category, "unknown formatting failure", location);
    }
}

template <typename... Args>
void LogFormatted(LogLevel level, std::source_location location,
                  std::format_string<Args...> messageFormat, Args&&... args) noexcept
{
    LogFormattedWithCategory(level, std::string_view{}, location, messageFormat,
                             std::forward<Args>(args)...);
}
} // namespace pond::core

#define LOG_TRACE(...)                                                                             \
    ::pond::core::LogFormatted(::pond::core::LogLevel::Trace, std::source_location::current(),     \
                               __VA_ARGS__)

#define LOG_TRACE_CATEGORY(category, ...)                                                          \
    ::pond::core::LogFormattedWithCategory(::pond::core::LogLevel::Trace, category,                \
                                           std::source_location::current(), __VA_ARGS__)

#define LOG_DEBUG(...)                                                                             \
    ::pond::core::LogFormatted(::pond::core::LogLevel::Debug, std::source_location::current(),     \
                               __VA_ARGS__)

#define LOG_DEBUG_CATEGORY(category, ...)                                                          \
    ::pond::core::LogFormattedWithCategory(::pond::core::LogLevel::Debug, category,                \
                                           std::source_location::current(), __VA_ARGS__)

#define LOG_INFO(...)                                                                              \
    ::pond::core::LogFormatted(::pond::core::LogLevel::Info, std::source_location::current(),      \
                               __VA_ARGS__)

#define LOG_INFO_CATEGORY(category, ...)                                                           \
    ::pond::core::LogFormattedWithCategory(::pond::core::LogLevel::Info, category,                 \
                                           std::source_location::current(), __VA_ARGS__)

#define LOG_WARNING(...)                                                                           \
    ::pond::core::LogFormatted(::pond::core::LogLevel::Warning, std::source_location::current(),   \
                               __VA_ARGS__)

#define LOG_WARNING_CATEGORY(category, ...)                                                        \
    ::pond::core::LogFormattedWithCategory(::pond::core::LogLevel::Warning, category,              \
                                           std::source_location::current(), __VA_ARGS__)

#define LOG_ERROR(...)                                                                             \
    ::pond::core::LogFormatted(::pond::core::LogLevel::Error, std::source_location::current(),     \
                               __VA_ARGS__)

#define LOG_ERROR_CATEGORY(category, ...)                                                          \
    ::pond::core::LogFormattedWithCategory(::pond::core::LogLevel::Error, category,                \
                                           std::source_location::current(), __VA_ARGS__)

#define LOG_FATAL(...)                                                                             \
    ::pond::core::LogFormatted(::pond::core::LogLevel::Fatal, std::source_location::current(),     \
                               __VA_ARGS__)

#define LOG_FATAL_CATEGORY(category, ...)                                                          \
    ::pond::core::LogFormattedWithCategory(::pond::core::LogLevel::Fatal, category,                \
                                           std::source_location::current(), __VA_ARGS__)
