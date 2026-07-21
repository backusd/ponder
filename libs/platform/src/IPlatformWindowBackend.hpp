#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/NativeWindow.hpp>
#include <ponder/platform/WindowGraphics.hpp>

#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace pond::platform::detail
{
class BackendWindowHandle final
{
public:
    using ValueType = std::uintptr_t;

    constexpr BackendWindowHandle() noexcept = default;
    explicit constexpr BackendWindowHandle(ValueType value) noexcept : m_value(value) {}

    [[nodiscard]] constexpr bool IsValid() const noexcept { return m_value != 0; }
    [[nodiscard]] constexpr ValueType GetValue() const noexcept { return m_value; }

    friend constexpr bool operator==(const BackendWindowHandle&,
                                     const BackendWindowHandle&) noexcept = default;

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

    [[nodiscard]] virtual core::Result<BackendWindowHandle> Create(
        const BackendWindowCreateDesc& desc) = 0;
    virtual void Destroy(BackendWindowHandle window) noexcept = 0;
    [[nodiscard]] virtual core::Result<std::uint32_t> GetId(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual std::string GetTitle(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::VoidResult SetTitle(BackendWindowHandle window,
                                                    std::string_view title) = 0;
    [[nodiscard]] virtual core::Result<BackendWindowPosition> GetPosition(
        BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::VoidResult SetPosition(BackendWindowHandle window,
                                                       BackendWindowPosition position) = 0;
    [[nodiscard]] virtual core::Result<BackendWindowLogicalSize> GetSize(
        BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::Result<BackendWindowPixelSize> GetSizeInPixels(
        BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::VoidResult SetSize(BackendWindowHandle window,
                                                   BackendWindowLogicalSize size) = 0;
    [[nodiscard]] virtual core::VoidResult SetMinimumSize(
        BackendWindowHandle window, BackendWindowLogicalSize size) = 0;
    [[nodiscard]] virtual core::VoidResult Show(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::VoidResult Hide(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::Result<BackendWindowProperties> GetProperties(
        BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::VoidResult SetFullscreenModeToDesktop(
        BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::VoidResult SetFullscreen(BackendWindowHandle window,
                                                         bool fullscreen) = 0;
    [[nodiscard]] virtual core::VoidResult SetBordered(BackendWindowHandle window,
                                                       bool bordered) = 0;
    [[nodiscard]] virtual core::VoidResult SetResizable(BackendWindowHandle window,
                                                        bool resizable) = 0;
    [[nodiscard]] virtual core::VoidResult SetAlwaysOnTop(BackendWindowHandle window,
                                                          bool alwaysOnTop) = 0;
    [[nodiscard]] virtual core::VoidResult Minimize(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::VoidResult Maximize(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::VoidResult Restore(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::VoidResult StartTextInput(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::VoidResult StopTextInput(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual bool IsTextInputActive(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::VoidResult ClearTextComposition(
        BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::VoidResult SetTextInputArea(
        BackendWindowHandle window, std::optional<BackendTextInputArea> area) = 0;
    [[nodiscard]] virtual core::VoidResult SetMouseGrab(BackendWindowHandle window,
                                                        bool grabbed) = 0;
    [[nodiscard]] virtual bool IsMouseGrabbed(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::VoidResult SetRelativeMouseMode(BackendWindowHandle window,
                                                                bool enabled) = 0;
    [[nodiscard]] virtual bool IsRelativeMouseModeEnabled(BackendWindowHandle window) = 0;
    [[nodiscard]] virtual core::Result<NativeWindowHandle> GetNativeHandle(
        BackendWindowHandle window) = 0;

protected:
    IPlatformWindowBackend() noexcept = default;
};

class IPlatformWindowBackendFactory
{
public:
    virtual ~IPlatformWindowBackendFactory() noexcept = default;

    IPlatformWindowBackendFactory(const IPlatformWindowBackendFactory&) = delete;
    IPlatformWindowBackendFactory& operator=(const IPlatformWindowBackendFactory&) = delete;
    IPlatformWindowBackendFactory(IPlatformWindowBackendFactory&&) = delete;
    IPlatformWindowBackendFactory& operator=(IPlatformWindowBackendFactory&&) = delete;

    [[nodiscard]] virtual std::unique_ptr<IPlatformWindowBackend> Create() const = 0;

protected:
    IPlatformWindowBackendFactory() noexcept = default;
};
} // namespace pond::platform::detail

namespace std
{
template <>
struct formatter<pond::platform::detail::BackendWindowHandle> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::BackendWindowHandle window,
                FormatContext& context) const
    {
        const string text =
            window.IsValid() ? std::format("0x{:X}", window.GetValue()) : "invalid";
        return formatter<string>::format(text, context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendWindowPosition> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::BackendWindowPosition position,
                FormatContext& context) const
    {
        return formatter<string>::format(std::format("({}, {})", position.x, position.y), context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendWindowLogicalSize> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::BackendWindowLogicalSize size,
                FormatContext& context) const
    {
        return formatter<string>::format(std::format("{}x{}", size.width, size.height), context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendWindowPixelSize> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::BackendWindowPixelSize size,
                FormatContext& context) const
    {
        return formatter<string>::format(std::format("{}x{}", size.width, size.height), context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendWindowCreateDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::detail::BackendWindowCreateDesc& desc,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format(
                "title='{}', logicalSize={}, resizable={}, highPixelDensity={}, graphics={}",
                desc.title, desc.logicalSize, desc.resizable, desc.highPixelDensity,
                desc.graphicsCompatibility),
            context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendWindowProperties> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::detail::BackendWindowProperties& properties,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("desktopFullscreen={}, hidden={}, borderless={}, resizable={}, "
                        "minimized={}, maximized={}, inputFocus={}, alwaysOnTop={}",
                        properties.desktopFullscreen, properties.hidden, properties.borderless,
                        properties.resizable, properties.minimized, properties.maximized,
                        properties.inputFocus, properties.alwaysOnTop),
            context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendTextInputArea> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::BackendTextInputArea area,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("({}, {}) / {}x{}, cursorOffset={}", area.x, area.y, area.width,
                        area.height, area.cursorOffset),
            context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendNativeWindowDriver> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::BackendNativeWindowDriver driver,
                FormatContext& context) const
    {
        using pond::platform::detail::BackendNativeWindowDriver;

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

namespace pond::platform::detail
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
} // namespace pond::platform::detail