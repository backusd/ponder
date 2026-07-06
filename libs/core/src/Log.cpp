#include <ponder/core/Log.hpp>

#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace pond::core
{
namespace
{
std::shared_ptr<spdlog::logger> GetLogger()
{
    static const std::shared_ptr<spdlog::logger> kLogger = []()
    {
        auto logger = spdlog::stderr_color_mt("ponder");
        logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%s:%#] %v");
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::err);
        return logger;
    }();

    return kLogger;
}

spdlog::level::level_enum ToSpdlogLevel(LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::Trace:
        return spdlog::level::trace;
    case LogLevel::Debug:
        return spdlog::level::debug;
    case LogLevel::Info:
        return spdlog::level::info;
    case LogLevel::Warning:
        return spdlog::level::warn;
    case LogLevel::Error:
        return spdlog::level::err;
    case LogLevel::Fatal:
        return spdlog::level::critical;
    }

    return spdlog::level::info;
}
} // namespace

void LogMessage(LogLevel level, std::string_view message, std::source_location location) noexcept
{
    try
    {
        auto logger = GetLogger();
        logger->log(spdlog::source_loc{location.file_name(), static_cast<int>(location.line()),
                                       location.function_name()},
                    ToSpdlogLevel(level), "{}", message);
    }
    catch (...)
    {
    }
}

void SetMinimumLogLevel(LogLevel level) noexcept
{
    try
    {
        GetLogger()->set_level(ToSpdlogLevel(level));
    }
    catch (...)
    {
    }
}
} // namespace pond::core