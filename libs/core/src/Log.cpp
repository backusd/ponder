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
struct LogRecord final
{
    LogLevel Level{LogLevel::Info};
    std::string Category;
    std::string Message;
    std::source_location Location;
};

void DefaultFatalHandler(std::string_view, std::string_view, std::source_location) {}

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

    void Log(LogRecord record) noexcept
    {
        if (!IsLogLevelEnabled(record.Level, m_minimumLevel.load(std::memory_order_relaxed)))
        {
            return;
        }

        const bool isFatal = record.Level == LogLevel::Fatal;
        const LogRecord synchronousRecord = record;
        const std::string fatalCategory = record.Category;
        const std::string fatalMessage = record.Message;
        const std::source_location fatalLocation = record.Location;

        if (!m_acceptingQueuedMessages.load(std::memory_order_acquire))
        {
            WriteRecord(record);
            FlushLogger();
            InvokeFatalHandlerIfNeeded(isFatal, fatalCategory, fatalMessage, fatalLocation);
            return;
        }

        m_pendingRecords.fetch_add(1, std::memory_order_acq_rel);

        if (!m_queue.enqueue(std::move(record)))
        {
            m_pendingRecords.fetch_sub(1, std::memory_order_acq_rel);
            m_waitCondition.notify_all();
            WriteRecord(LogRecord{LogLevel::Error, "logging",
                                  "Failed to enqueue log record; writing synchronously.",
                                  std::source_location::current()});
            WriteRecord(synchronousRecord);
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
                                 return m_pendingRecords.load(std::memory_order_acquire) == 0;
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
                m_pendingRecords.load(std::memory_order_acquire) == 0)
            {
                break;
            }

            std::unique_lock lock{m_waitMutex};
            m_waitCondition.wait(lock,
                                 [this]()
                                 {
                                     return m_stopRequested.load(std::memory_order_acquire) ||
                                            m_pendingRecords.load(std::memory_order_acquire) > 0;
                                 });
        }

        DrainQueue();
        FlushLogger();
    }

    void DrainQueue() noexcept
    {
        LogRecord record;
        while (m_queue.try_dequeue(record))
        {
            WriteRecord(record);
            if (m_pendingRecords.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                m_waitCondition.notify_all();
            }
        }
    }

    void WriteRecord(const LogRecord& record) noexcept
    {
        try
        {
            m_logger->log(spdlog::source_loc{record.Location.file_name(),
                                             static_cast<int>(record.Location.line()),
                                             record.Location.function_name()},
                          ToSpdlogLevel(record.Level), "{}", FormatPayload(record));
        }
        catch (...)
        {
        }
    }

    [[nodiscard]] std::string FormatPayload(const LogRecord& record) const
    {
        if (record.Category.empty())
        {
            return record.Message;
        }

        std::string payload;
        payload.reserve(record.Category.size() + record.Message.size() + 3);
        payload.append("[");
        payload.append(record.Category);
        payload.append("] ");
        payload.append(record.Message);
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
        }
    }

    std::shared_ptr<spdlog::logger> m_logger;
    moodycamel::ConcurrentQueue<LogRecord> m_queue;
    std::atomic<LogLevel> m_minimumLevel{LogLevel::Trace};
    std::atomic<LogFatalHandler> m_fatalHandler{DefaultFatalHandler};
    std::atomic<std::uint64_t> m_pendingRecords{0};
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
        GetLoggingBackend().Log(
            LogRecord{level, std::string{category}, std::string{message}, location});
    }
    catch (...)
    {
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

LogFatalHandler SetLogFatalHandler(LogFatalHandler handler) noexcept
{
    return GetLoggingBackend().SetFatalHandler(handler);
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
