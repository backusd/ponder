#pragma once

#include <cstdint>
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

struct NativeCocoaWindow final
{
    void* metalLayer{};

    friend constexpr bool operator==(NativeCocoaWindow, NativeCocoaWindow) noexcept = default;
};

using NativeWindowHandle =
    std::variant<NativeWin32Window, NativeX11Window, NativeWaylandWindow, NativeCocoaWindow>;
} // namespace pond::platform
