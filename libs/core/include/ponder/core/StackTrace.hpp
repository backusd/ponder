#pragma once

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

[[nodiscard]] StackTrace CaptureStackTrace();
} // namespace pond::core
