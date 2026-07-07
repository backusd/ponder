#include <ponder/core/Log.hpp>

#include <atomic>
#include <concurrentqueue.h>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <spdlog/logger.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#if defined(_WIN32)
#include <spdlog/sinks/msvc_sink.h>
#endif
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace pond::core
{
namespace
{
void DefaultLogSinkHandler(const LogEntry&) {}
void DefaultFatalHandler(std::string_view, std::string_view, std::source_location) {}

void SuppressLoggingException() noexcept
{
    // Logging is a diagnostic path; failures here must not escape noexcept APIs.
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

std::shared_ptr<spdlog::logger> CreateLogger()
{
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());

#if defined(_WIN32)
    sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif

    auto logger = std::make_shared<spdlog::logger>("ponder", sinks.begin(), sinks.end());
    logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [%s:%#] [%!] %v");
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::err);

    return logger;
}

class LoggingBackend final
{
public:
    LoggingBackend() : m_logger(CreateLogger()), m_workerThread(&LoggingBackend::Run, this) {}

    ~LoggingBackend()
    {
        Shutdown();
    }

    void Log(LogEntry entry) noexcept
    {
        if (!IsLogLevelEnabled(entry.GetLevel(), m_minimumLevel.load(std::memory_order_relaxed)))
        {
            return;
        }

        const bool isFatal = entry.GetLevel() == LogLevel::Fatal;
        const LogEntry synchronousEntry = entry;
        const std::string fatalCategory{entry.GetCategory()};
        const std::string fatalMessage{entry.GetMessage()};
        const std::source_location fatalLocation = entry.GetLocation();

        if (!m_acceptingQueuedMessages.load(std::memory_order_acquire))
        {
            WriteEntry(entry);
            FlushLogger();
            InvokeFatalHandlerIfNeeded(isFatal, fatalCategory, fatalMessage, fatalLocation);
            return;
        }

        m_pendingEntries.fetch_add(1, std::memory_order_acq_rel);

        if (!m_queue.enqueue(std::move(entry)))
        {
            m_pendingEntries.fetch_sub(1, std::memory_order_acq_rel);
            m_waitCondition.notify_all();
            WriteEntry(LogEntry{LogLevel::Error, "logging",
                                "Failed to enqueue log record; writing synchronously.",
                                std::chrono::system_clock::now(), std::source_location::current()});
            WriteEntry(synchronousEntry);
            FlushLogger();
            InvokeFatalHandlerIfNeeded(isFatal, fatalCategory, fatalMessage, fatalLocation);
            return;
        }

        m_waitCondition.notify_one();

        if (isFatal)
        {
            Flush();
            InvokeFatalHandlerIfNeeded(true, fatalCategory, fatalMessage, fatalLocation);
        }
    }

    [[nodiscard]] LogLevel GetMinimumLevel() const noexcept
    {
        return m_minimumLevel.load(std::memory_order_relaxed);
    }

    void SetMinimumLevel(LogLevel level) noexcept
    {
        m_minimumLevel.store(level, std::memory_order_relaxed);
    }

    void Flush() noexcept
    {
        if (std::this_thread::get_id() == m_workerThread.get_id())
        {
            FlushLogger();
            return;
        }

        std::unique_lock lock{m_waitMutex};
        m_waitCondition.wait(lock,
                             [this]()
                             {
                                 return m_pendingEntries.load(std::memory_order_acquire) == 0;
                             });
        FlushLogger();
    }

    void Shutdown() noexcept
    {
        const bool wasAccepting =
            m_acceptingQueuedMessages.exchange(false, std::memory_order_acq_rel);
        const bool wasStopped = m_stopRequested.exchange(true, std::memory_order_acq_rel);

        m_waitCondition.notify_all();

        if (!wasStopped && m_workerThread.joinable() &&
            std::this_thread::get_id() != m_workerThread.get_id())
        {
            m_workerThread.join();
        }

        if (wasAccepting)
        {
            FlushLogger();
        }
    }

    LogSinkHandler SetSinkHandler(LogSinkHandler handler) noexcept
    {
        return m_sinkHandler.exchange(handler ? handler : DefaultLogSinkHandler,
                                      std::memory_order_acq_rel);
    }

    LogFatalHandler SetFatalHandler(LogFatalHandler handler) noexcept
    {
        return m_fatalHandler.exchange(handler ? handler : DefaultFatalHandler,
                                       std::memory_order_acq_rel);
    }

private:
    void Run() noexcept
    {
        while (true)
        {
            DrainQueue();

            if (m_stopRequested.load(std::memory_order_acquire) &&
                m_pendingEntries.load(std::memory_order_acquire) == 0)
            {
                break;
            }

            std::unique_lock lock{m_waitMutex};
            m_waitCondition.wait(lock,
                                 [this]()
                                 {
                                     return m_stopRequested.load(std::memory_order_acquire) ||
                                            m_pendingEntries.load(std::memory_order_acquire) > 0;
                                 });
        }

        DrainQueue();
        FlushLogger();
    }

    void DrainQueue() noexcept
    {
        LogEntry entry;
        while (m_queue.try_dequeue(entry))
        {
            WriteEntry(entry);
            if (m_pendingEntries.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                m_waitCondition.notify_all();
            }
        }
    }

    void WriteEntry(const LogEntry& entry) noexcept
    {
        InvokeSinkHandler(entry);

        try
        {
            m_logger->log(spdlog::source_loc{entry.GetLocation().file_name(),
                                             static_cast<int>(entry.GetLocation().line()),
                                             entry.GetLocation().function_name()},
                          ToSpdlogLevel(entry.GetLevel()), "{}", FormatPayload(entry));
        }
        catch (...)
        {
            SuppressLoggingException();
        }
    }

    [[nodiscard]] std::string FormatPayload(const LogEntry& entry) const
    {
        if (entry.GetCategory().empty())
        {
            return std::string{entry.GetMessage()};
        }

        std::string payload;
        payload.reserve(entry.GetCategory().size() + entry.GetMessage().size() + 3);
        payload.append("[");
        payload.append(entry.GetCategory());
        payload.append("] ");
        payload.append(entry.GetMessage());
        return payload;
    }

    void FlushLogger() noexcept
    {
        try
        {
            m_logger->flush();
        }
        catch (...)
        {
            SuppressLoggingException();
        }
    }

    void InvokeSinkHandler(const LogEntry& entry) noexcept
    {
        try
        {
            m_sinkHandler.load(std::memory_order_acquire)(entry);
        }
        catch (...)
        {
            SuppressLoggingException();
        }
    }

    void InvokeFatalHandlerIfNeeded(bool isFatal, std::string_view category,
                                    std::string_view message,
                                    std::source_location location) noexcept
    {
        if (!isFatal)
        {
            return;
        }

        try
        {
            m_fatalHandler.load(std::memory_order_acquire)(category, message, location);
        }
        catch (...)
        {
            SuppressLoggingException();
        }
    }

    std::shared_ptr<spdlog::logger> m_logger;
    moodycamel::ConcurrentQueue<LogEntry> m_queue;
    std::atomic<LogLevel> m_minimumLevel{LogLevel::Trace};
    std::atomic<LogSinkHandler> m_sinkHandler{DefaultLogSinkHandler};
    std::atomic<LogFatalHandler> m_fatalHandler{DefaultFatalHandler};
    std::atomic<std::uint64_t> m_pendingEntries{0};
    std::atomic<bool> m_acceptingQueuedMessages{true};
    std::atomic<bool> m_stopRequested{false};
    std::mutex m_waitMutex;
    std::condition_variable m_waitCondition;
    std::thread m_workerThread;
};

LoggingBackend& GetLoggingBackend()
{
    static LoggingBackend kBackend;
    return kBackend;
}
} // namespace

LogEntry::LogEntry(LogLevel level, std::string category, std::string message,
                   std::chrono::system_clock::time_point timestamp, std::source_location location)
    : m_level(level), m_category(std::move(category)), m_message(std::move(message)),
      m_timestamp(timestamp), m_location(location)
{
}

LogLevel LogEntry::GetLevel() const noexcept
{
    return m_level;
}

std::string_view LogEntry::GetCategory() const noexcept
{
    return m_category;
}

std::string_view LogEntry::GetMessage() const noexcept
{
    return m_message;
}

std::chrono::system_clock::time_point LogEntry::GetTimestamp() const noexcept
{
    return m_timestamp;
}

const std::source_location& LogEntry::GetLocation() const noexcept
{
    return m_location;
}

std::string_view GetLogLevelName(LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::Trace:
        return "trace";
    case LogLevel::Debug:
        return "debug";
    case LogLevel::Info:
        return "info";
    case LogLevel::Warning:
        return "warning";
    case LogLevel::Error:
        return "error";
    case LogLevel::Fatal:
        return "fatal";
    }

    return "unknown";
}

void LogMessage(LogLevel level, std::string_view message, std::source_location location) noexcept
{
    LogMessage(level, std::string_view{}, message, location);
}

void LogMessage(LogLevel level, std::string_view category, std::string_view message,
                std::source_location location) noexcept
{
    try
    {
        GetLoggingBackend().Log(LogEntry{level, std::string{category}, std::string{message},
                                         std::chrono::system_clock::now(), location});
    }
    catch (...)
    {
        SuppressLoggingException();
    }
}

LogLevel GetMinimumLogLevel() noexcept
{
    return GetLoggingBackend().GetMinimumLevel();
}

void SetMinimumLogLevel(LogLevel level) noexcept
{
    GetLoggingBackend().SetMinimumLevel(level);
}

void FlushLog() noexcept
{
    GetLoggingBackend().Flush();
}

void ShutdownLogging() noexcept
{
    GetLoggingBackend().Shutdown();
}

LogSinkHandler SetLogSinkHandler(LogSinkHandler handler) noexcept
{
    return GetLoggingBackend().SetSinkHandler(handler);
}

LogFatalHandler SetLogFatalHandler(LogFatalHandler handler) noexcept
{
    return GetLoggingBackend().SetFatalHandler(handler);
}

ScopedLogSinkHandler::ScopedLogSinkHandler(LogSinkHandler handler) noexcept
{
    FlushLog();
    m_previousHandler = SetLogSinkHandler(handler);
}

ScopedLogSinkHandler::~ScopedLogSinkHandler()
{
    FlushLog();
    (void)SetLogSinkHandler(m_previousHandler);
}

ScopedLogFatalHandler::ScopedLogFatalHandler(LogFatalHandler handler) noexcept
    : m_previousHandler(SetLogFatalHandler(handler))
{
}

ScopedLogFatalHandler::~ScopedLogFatalHandler()
{
    (void)SetLogFatalHandler(m_previousHandler);
}

ScopedMinimumLogLevel::ScopedMinimumLogLevel(LogLevel level) noexcept
    : m_previousLevel(GetMinimumLogLevel())
{
    SetMinimumLogLevel(level);
}

ScopedMinimumLogLevel::~ScopedMinimumLogLevel()
{
    SetMinimumLogLevel(m_previousLevel);
}

void LogFormattingFailure(LogLevel level, std::string_view category, std::string_view reason,
                          std::source_location location) noexcept
{
    try
    {
        std::string message{"Failed to format "};
        message.append(GetLogLevelName(level));
        message.append(" log message");

        if (!category.empty())
        {
            message.append(" for category '");
            message.append(category);
            message.append("'");
        }

        if (!reason.empty())
        {
            message.append(": ");
            message.append(reason);
        }

        LogMessage(level == LogLevel::Fatal ? LogLevel::Fatal : LogLevel::Error, "logging", message,
                   location);
    }
    catch (...)
    {
        LogMessage(level == LogLevel::Fatal ? LogLevel::Fatal : LogLevel::Error, "logging",
                   "Failed to format log message.", location);
    }
}
} // namespace pond::core
