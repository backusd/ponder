#pragma once

#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace pond::core
{
enum class LogLevel
{
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

void LogMessage(LogLevel level, std::string_view message,
                std::source_location location = std::source_location::current()) noexcept;

void SetMinimumLogLevel(LogLevel level) noexcept;

template <typename... Args>
void LogFormatted(LogLevel level, std::source_location location,
                  std::format_string<Args...> messageFormat, Args&&... args) noexcept
{
    try
    {
        LogMessage(level, std::format(messageFormat, std::forward<Args>(args)...), location);
    }
    catch (...)
    {
    }
}
} // namespace pond::core

#define LOG_TRACE(...)                                                                             \
    ::pond::core::LogFormatted(::pond::core::LogLevel::Trace, std::source_location::current(),     \
                               __VA_ARGS__)

#define LOG_DEBUG(...)                                                                             \
    ::pond::core::LogFormatted(::pond::core::LogLevel::Debug, std::source_location::current(),     \
                               __VA_ARGS__)

#define LOG_INFO(...)                                                                              \
    ::pond::core::LogFormatted(::pond::core::LogLevel::Info, std::source_location::current(),      \
                               __VA_ARGS__)

#define LOG_WARNING(...)                                                                           \
    ::pond::core::LogFormatted(::pond::core::LogLevel::Warning, std::source_location::current(),   \
                               __VA_ARGS__)

#define LOG_ERROR(...)                                                                             \
    ::pond::core::LogFormatted(::pond::core::LogLevel::Error, std::source_location::current(),     \
                               __VA_ARGS__)

#define LOG_FATAL(...)                                                                             \
    ::pond::core::LogFormatted(::pond::core::LogLevel::Fatal, std::source_location::current(),     \
                               __VA_ARGS__)