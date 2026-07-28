#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/NativeWindow.hpp>
#include <ponder/platform/WindowGraphics.hpp>

#include <cstdint>
#include <format>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace ponder::platform::detail
{
class BackendWindowHandle final
{
public:
    using ValueType = std::uintptr_t;

    constexpr BackendWindowHandle() noexcept = default;
    explicit constexpr BackendWindowHandle(ValueType value) noexcept :
        m_value(value)
    {
    }

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return m_value != 0;
    }
    [[nodiscard]] constexpr ValueType GetValue() const noexcept
    {
        return m_value;
    }

    friend constexpr bool operator==(const BackendWindowHandle&, const BackendWindowHandle&) noexcept = default;

private:
    ValueType m_value{};
};

struct BackendWindowPosition final
{
    int x{};
    int y{};
};

struct BackendWindowLogicalSize final
{
    int width{};
    int height{};
};

struct BackendWindowPixelSize final
{
    int width{};
    int height{};
};

struct BackendWindowCreateDesc final
{
    std::string_view title;
    BackendWindowLogicalSize logicalSize;
    bool resizable{};
    bool highPixelDensity{};
    WindowGraphicsCompatibility graphicsCompatibility{WindowGraphicsCompatibility::Default};
};

struct BackendWindowProperties final
{
    bool desktopFullscreen{};
    bool hidden{};
    bool borderless{};
    bool resizable{};
    bool minimized{};
    bool maximized{};
    bool inputFocus{};
    bool alwaysOnTop{};
};

struct BackendTextInputArea final
{
    int x{};
    int y{};
    int width{};
    int height{};
    int cursorOffset{};
};

enum class BackendNativeWindowDriver : std::uint8_t
{
    Unsupported,
    Win32,
    X11,
    Wayland
};

class IPlatformWindowBackend
{
public:
    virtual ~IPlatformWindowBackend() noexcept = default;

    IPlatformWindowBackend(const IPlatformWindowBackend&) = delete;
    IPlatformWindowBackend& operator=(const IPlatformWindowBackend&) = delete;
    IPlatformWindowBackend(IPlatformWindowBackend&&) = delete;
    IPlatformWindowBackend& operator=(IPlatformWindowBackend&&) = delete;

    [[nodiscard]] virtual BackendWindowHandle Create(const BackendWindowCreateDesc& desc) = 0;
    virtual void Destroy(BackendWindowHandle window) noexcept = 0;
    [[nodiscard]] virtual std::uint32_t GetId(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual std::string GetTitle(BackendWindowHandle window) = 0;
    virtual void SetTitle(BackendWindowHandle window, std::string_view title) = 0;
    [[nodiscard]] virtual BackendWindowPosition GetPosition(BackendWindowHandle window) = 0;
    virtual void SetPosition(BackendWindowHandle window, BackendWindowPosition position) = 0;
    [[nodiscard]] virtual BackendWindowLogicalSize GetSize(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual BackendWindowPixelSize GetSizeInPixels(BackendWindowHandle window) = 0;
    virtual void SetSize(BackendWindowHandle window, BackendWindowLogicalSize size) = 0;
    virtual void SetMinimumSize(BackendWindowHandle window, BackendWindowLogicalSize size) = 0;
    virtual void Show(BackendWindowHandle window) = 0;
    virtual void Hide(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual BackendWindowProperties GetProperties(BackendWindowHandle window) = 0;
    virtual void SetFullscreenModeToDesktop(BackendWindowHandle window) = 0;
    virtual void SetFullscreen(BackendWindowHandle window, bool fullscreen) = 0;
    virtual void SetBordered(BackendWindowHandle window, bool bordered) = 0;
    virtual void SetResizable(BackendWindowHandle window, bool resizable) = 0;
    virtual void SetAlwaysOnTop(BackendWindowHandle window, bool alwaysOnTop) = 0;
    virtual void Minimize(BackendWindowHandle window) = 0;
    virtual void Maximize(BackendWindowHandle window) = 0;
    virtual void Restore(BackendWindowHandle window) = 0;
    virtual void StartTextInput(BackendWindowHandle window) = 0;
    virtual void StopTextInput(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual bool IsTextInputActive(BackendWindowHandle window) noexcept = 0;
    virtual void ClearTextComposition(BackendWindowHandle window) = 0;
    virtual void SetTextInputArea(BackendWindowHandle window, std::optional<BackendTextInputArea> area) = 0;
    virtual void SetMouseGrab(BackendWindowHandle window, bool grabbed) = 0;
    [[nodiscard]] virtual bool IsMouseGrabbed(BackendWindowHandle window) noexcept = 0;
    virtual void SetRelativeMouseMode(BackendWindowHandle window, bool enabled) = 0;
    [[nodiscard]] virtual bool IsRelativeMouseModeEnabled(BackendWindowHandle window) noexcept = 0;
    [[nodiscard]] virtual ponder::core::Result<NativeWindowHandle> GetNativeHandle(BackendWindowHandle window) = 0;

protected:
    IPlatformWindowBackend() noexcept = default;
};

} // namespace ponder::platform::detail

namespace std
{
template <>
struct formatter<ponder::platform::detail::BackendWindowHandle> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::detail::BackendWindowHandle window, FormatContext& context) const
    {
        const string text = window.IsValid() ? std::format("0x{:X}", window.GetValue()) : "invalid";
        return formatter<string>::format(text, context);
    }
};

template <>
struct formatter<ponder::platform::detail::BackendWindowPosition> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::detail::BackendWindowPosition position, FormatContext& context) const
    {
        return formatter<string>::format(std::format("({}, {})", position.x, position.y), context);
    }
};

template <>
struct formatter<ponder::platform::detail::BackendWindowLogicalSize> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::detail::BackendWindowLogicalSize size, FormatContext& context) const
    {
        return formatter<string>::format(std::format("{}x{}", size.width, size.height), context);
    }
};

template <>
struct formatter<ponder::platform::detail::BackendWindowPixelSize> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::detail::BackendWindowPixelSize size, FormatContext& context) const
    {
        return formatter<string>::format(std::format("{}x{}", size.width, size.height), context);
    }
};

template <>
struct formatter<ponder::platform::detail::BackendWindowCreateDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::BackendWindowCreateDesc& desc, FormatContext& context) const
    {
        return formatter<string>::format(std::format("title='{}', logicalSize={}, resizable={}, highPixelDensity={}, graphics={}", desc.title,
                                                     desc.logicalSize, desc.resizable, desc.highPixelDensity, desc.graphicsCompatibility),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::detail::BackendWindowProperties> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::BackendWindowProperties& properties, FormatContext& context) const
    {
        return formatter<string>::format(std::format("desktopFullscreen={}, hidden={}, borderless={}, resizable={}, "
                                                     "minimized={}, maximized={}, inputFocus={}, alwaysOnTop={}",
                                                     properties.desktopFullscreen, properties.hidden, properties.borderless, properties.resizable,
                                                     properties.minimized, properties.maximized, properties.inputFocus, properties.alwaysOnTop),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::detail::BackendTextInputArea> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::detail::BackendTextInputArea area, FormatContext& context) const
    {
        return formatter<string>::format(std::format("({}, {}) / {}x{}, cursorOffset={}", area.x, area.y, area.width, area.height, area.cursorOffset),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::detail::BackendNativeWindowDriver> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::detail::BackendNativeWindowDriver driver, FormatContext& context) const
    {
        using ponder::platform::detail::BackendNativeWindowDriver;

        string_view name{"unknown"};
        switch (driver)
        {
        case BackendNativeWindowDriver::Unsupported:
            name = "unsupported";
            break;
        case BackendNativeWindowDriver::Win32:
            name = "win32";
            break;
        case BackendNativeWindowDriver::X11:
            name = "x11";
            break;
        case BackendNativeWindowDriver::Wayland:
            name = "wayland";
            break;
        }

        return formatter<string_view>::format(name, context);
    }
};
} // namespace std

namespace ponder::platform::detail
{
inline std::ostream& operator<<(std::ostream& output, BackendWindowHandle window)
{
    return output << std::format("{}", window);
}

inline std::ostream& operator<<(std::ostream& output, BackendWindowPosition position)
{
    return output << std::format("{}", position);
}

inline std::ostream& operator<<(std::ostream& output, BackendWindowLogicalSize size)
{
    return output << std::format("{}", size);
}

inline std::ostream& operator<<(std::ostream& output, BackendWindowPixelSize size)
{
    return output << std::format("{}", size);
}

inline std::ostream& operator<<(std::ostream& output, const BackendWindowCreateDesc& desc)
{
    return output << std::format("{}", desc);
}

inline std::ostream& operator<<(std::ostream& output, const BackendWindowProperties& properties)
{
    return output << std::format("{}", properties);
}

inline std::ostream& operator<<(std::ostream& output, BackendTextInputArea area)
{
    return output << std::format("{}", area);
}

inline std::ostream& operator<<(std::ostream& output, BackendNativeWindowDriver driver)
{
    return output << std::format("{}", driver);
}
} // namespace ponder::platform::detail
