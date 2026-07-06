#include <ponder/core/StackTrace.hpp>

#include <cstddef>
#include <sstream>
#include <utility>

namespace pond::core
{
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

StackTrace CaptureStackTrace()
{
    // Stacktrace support is intentionally best-effort until CORE-009 evaluates
    // the supported compiler matrix. An empty stacktrace is the graceful
    // fallback and is part of the public contract.
    return StackTrace{};
}
} // namespace pond::core
