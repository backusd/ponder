#pragma once

#include <cstdint>
#include <format>
#include <ostream>
#include <string_view>

namespace pond::platform
{
enum class WindowPresentation : std::uint8_t
{
    Windowed = 0,
    DesktopFullscreen = 1
};

enum class WindowDecoration : std::uint8_t
{
    System = 0,
    Borderless = 1
};

enum class WindowState : std::uint8_t
{
    Normal = 0,
    Minimized = 1,
    Maximized = 2
};

[[nodiscard]] constexpr std::string_view GetWindowPresentationName(
    WindowPresentation presentation) noexcept
{
    switch (presentation)
    {
    case WindowPresentation::Windowed:
        return "windowed";
    case WindowPresentation::DesktopFullscreen:
        return "desktop_fullscreen";
    }

    return "unrecognized";
}

[[nodiscard]] constexpr std::string_view GetWindowDecorationName(
    WindowDecoration decoration) noexcept
{
    switch (decoration)
    {
    case WindowDecoration::System:
        return "system";
    case WindowDecoration::Borderless:
        return "borderless";
    }

    return "unrecognized";
}

[[nodiscard]] constexpr std::string_view GetWindowStateName(WindowState state) noexcept
{
    switch (state)
    {
    case WindowState::Normal:
        return "normal";
    case WindowState::Minimized:
        return "minimized";
    case WindowState::Maximized:
        return "maximized";
    }

    return "unrecognized";
}

inline std::ostream& operator<<(std::ostream& output, WindowPresentation presentation)
{
    return output << GetWindowPresentationName(presentation);
}

inline std::ostream& operator<<(std::ostream& output, WindowDecoration decoration)
{
    return output << GetWindowDecorationName(decoration);
}

inline std::ostream& operator<<(std::ostream& output, WindowState state)
{
    return output << GetWindowStateName(state);
}
} // namespace pond::platform

namespace std
{
template <>
struct formatter<pond::platform::WindowPresentation> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::WindowPresentation presentation, FormatContext& context) const
    {
        return formatter<string_view>::format(
            pond::platform::GetWindowPresentationName(presentation),
                                              context);
    }
};

template <>
struct formatter<pond::platform::WindowDecoration> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::WindowDecoration decoration, FormatContext& context) const
    {
        return formatter<string_view>::format(pond::platform::GetWindowDecorationName(decoration),
                                              context);
    }
};

template <>
struct formatter<pond::platform::WindowState> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::WindowState state, FormatContext& context) const
    {
        return formatter<string_view>::format(pond::platform::GetWindowStateName(state), context);
    }
};
} // namespace std