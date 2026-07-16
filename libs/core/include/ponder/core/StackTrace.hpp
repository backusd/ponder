#pragma once

#include <cstddef>
#include <format>
#include <ostream>
#include <source_location>
#include <span>
#include <string>
#include <vector>

namespace pond::core
{
class StackTrace final
{
public:
    StackTrace() = default;
    explicit StackTrace(std::vector<std::string> frames);

    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] std::span<const std::string> GetFrames() const noexcept;
    [[nodiscard]] std::string Format() const;

private:
    std::vector<std::string> m_frames;
};

inline std::ostream& operator<<(std::ostream& output, const StackTrace& stackTrace)
{
    return output << stackTrace.Format();
}

class StackTraceCaptureOptions final
{
public:
    constexpr StackTraceCaptureOptions() noexcept = default;
    constexpr StackTraceCaptureOptions(std::size_t skipFrames, std::size_t maxFrames) noexcept
        : m_skipFrames(skipFrames), m_maxFrames(maxFrames)
    {
    }

    [[nodiscard]] constexpr std::size_t GetSkipFrames() const noexcept
    {
        return m_skipFrames;
    }

    [[nodiscard]] constexpr std::size_t GetMaxFrames() const noexcept
    {
        return m_maxFrames;
    }

private:
    std::size_t m_skipFrames{0};
    std::size_t m_maxFrames{64};
};

[[nodiscard]] std::string FormatSourceLocation(
    std::source_location location = std::source_location::current());
[[nodiscard]] std::string FormatSourceLocationWithFunction(
    std::source_location location = std::source_location::current());
[[nodiscard]] bool IsStackTraceCaptureSupported() noexcept;
[[nodiscard]] StackTrace CaptureStackTrace(StackTraceCaptureOptions options);
[[nodiscard]] StackTrace CaptureStackTrace();
} // namespace pond::core

namespace std
{
template <>
struct formatter<pond::core::StackTrace> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::core::StackTrace& stackTrace, FormatContext& context) const
    {
        return formatter<string>::format(stackTrace.Format(), context);
    }
};
} // namespace std