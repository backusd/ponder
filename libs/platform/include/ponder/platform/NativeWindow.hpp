#pragma once

#include <cstdint>
#include <format>
#include <ostream>
#include <string>
#include <variant>

namespace pond::platform
{
struct NativeWin32Window final
{
    void* instance{};
    void* window{};

    friend constexpr bool operator==(NativeWin32Window, NativeWin32Window) noexcept = default;
};

struct NativeX11Window final
{
    void* display{};
    std::uintptr_t window{};

    friend constexpr bool operator==(NativeX11Window, NativeX11Window) noexcept = default;
};

struct NativeWaylandWindow final
{
    void* display{};
    void* surface{};

    friend constexpr bool operator==(NativeWaylandWindow, NativeWaylandWindow) noexcept = default;
};

using NativeWindowHandle =
    std::variant<NativeWin32Window, NativeX11Window, NativeWaylandWindow>;
} // namespace pond::platform

namespace std
{
template <>
struct formatter<pond::platform::NativeWin32Window> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::NativeWin32Window window, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("win32(instance=0x{:X}, window=0x{:X})",
                        reinterpret_cast<std::uintptr_t>(window.instance),
                        reinterpret_cast<std::uintptr_t>(window.window)),
            context);
    }
};

template <>
struct formatter<pond::platform::NativeX11Window> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::NativeX11Window window, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("x11(display=0x{:X}, window=0x{:X})",
                        reinterpret_cast<std::uintptr_t>(window.display), window.window),
            context);
    }
};

template <>
struct formatter<pond::platform::NativeWaylandWindow> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::NativeWaylandWindow window, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("wayland(display=0x{:X}, surface=0x{:X})",
                        reinterpret_cast<std::uintptr_t>(window.display),
                        reinterpret_cast<std::uintptr_t>(window.surface)),
            context);
    }
};
} // namespace std

namespace pond::platform
{
inline std::ostream& operator<<(std::ostream& output, NativeWin32Window window)
{
    return output << std::format("{}", window);
}

inline std::ostream& operator<<(std::ostream& output, NativeX11Window window)
{
    return output << std::format("{}", window);
}

inline std::ostream& operator<<(std::ostream& output, NativeWaylandWindow window)
{
    return output << std::format("{}", window);
}
} // namespace pond::platform