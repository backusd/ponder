#include <ponder/core/StackTrace.hpp>

#include <cstddef>
#include <format>
#include <sstream>
#include <string_view>
#include <utility>

#if defined(__has_include)
#if __has_include(<stacktrace>)
#include <stacktrace>
#endif
#endif

namespace pond::core
{
namespace
{
#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
constexpr bool kHasStdStackTrace{true};
#else
constexpr bool kHasStdStackTrace{false};
#endif

constexpr std::size_t kCaptureStackTraceInternalFrames{1};
} // namespace

StackTrace::StackTrace(std::vector<std::string> frames) : m_frames(std::move(frames)) {}

bool StackTrace::IsEmpty() const noexcept
{
    return m_frames.empty();
}

std::span<const std::string> StackTrace::GetFrames() const noexcept
{
    return std::span<const std::string>{m_frames.data(), m_frames.size()};
}

std::string StackTrace::Format() const
{
    std::ostringstream formatted;

    for (std::size_t index = 0; index < m_frames.size(); ++index)
    {
        if (index > 0)
        {
            formatted << '\n';
        }

        formatted << index << ": " << m_frames[index];
    }

    return formatted.str();
}

std::string FormatSourceLocation(std::source_location location)
{
    return std::format("{}:{}:{}", location.file_name(), location.line(), location.column());
}

std::string FormatSourceLocationWithFunction(std::source_location location)
{
    std::string formattedLocation = FormatSourceLocation(location);
    std::string_view functionName{location.function_name()};

    if (functionName.empty())
    {
        return formattedLocation;
    }

    return std::format("{} ({})", formattedLocation, functionName);
}

bool IsStackTraceCaptureSupported() noexcept
{
    return kHasStdStackTrace;
}

StackTrace CaptureStackTrace(StackTraceCaptureOptions options)
{
    if (options.GetMaxFrames() == 0)
    {
        return StackTrace{};
    }

#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
    std::stacktrace stackTrace = std::stacktrace::current(
        options.GetSkipFrames() + kCaptureStackTraceInternalFrames, options.GetMaxFrames());

    std::vector<std::string> frames;
    frames.reserve(stackTrace.size());

    for (const std::stacktrace_entry& entry : stackTrace)
    {
        frames.push_back(std::to_string(entry));
    }

    return StackTrace{std::move(frames)};
#else
    (void)options;
    return StackTrace{};
#endif
}

StackTrace CaptureStackTrace()
{
    return CaptureStackTrace(StackTraceCaptureOptions{});
}
} // namespace pond::core
