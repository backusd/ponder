#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/NativeWindow.hpp>
#include <ponder/platform/TextInput.hpp>
#include <ponder/platform/WindowGraphics.hpp>
#include <ponder/platform/WindowState.hpp>

#include <format>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace pond::platform
{
struct WindowDesc final
{
    std::string title{"ponder"};
    LogicalSize logicalSize{1280, 800};
    bool visible{true};
    bool resizable{true};
    bool highPixelDensity{true};
    std::optional<LogicalSize> minimumLogicalSize;
    WindowGraphicsCompatibility graphicsCompatibility{WindowGraphicsCompatibility::Default};
};

namespace detail
{
class WindowImpl;
}

class PlatformRuntime;

class Window final
{
public:
    ~Window() noexcept;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    [[nodiscard]] WindowId GetId() const;
    [[nodiscard]] WindowGraphicsCompatibility GetGraphicsCompatibility() const;
    [[nodiscard]] core::Result<NativeWindowHandle> GetNativeHandle() const;

    [[nodiscard]] std::string GetTitle() const;
    [[nodiscard]] core::VoidResult SetTitle(std::string_view title);

    [[nodiscard]] core::Result<ScreenPosition> GetPosition() const;
    [[nodiscard]] core::VoidResult SetPosition(ScreenPosition position);

    [[nodiscard]] core::Result<LogicalSize> GetLogicalSize() const;
    [[nodiscard]] core::Result<PixelSize> GetPixelSize() const;
    [[nodiscard]] core::VoidResult SetLogicalSize(LogicalSize size);
    [[nodiscard]] core::Result<DisplayId> GetDisplayId() const;
    [[nodiscard]] core::Result<float> GetPixelDensity() const;
    [[nodiscard]] core::Result<float> GetDisplayScale() const;

    [[nodiscard]] core::Result<WindowPresentation> GetPresentation() const;
    [[nodiscard]] core::VoidResult SetPresentation(WindowPresentation presentation);
    [[nodiscard]] core::Result<WindowDecoration> GetDecoration() const;
    [[nodiscard]] core::VoidResult SetDecoration(WindowDecoration decoration);
    [[nodiscard]] core::Result<WindowState> GetState() const;
    [[nodiscard]] core::VoidResult Minimize();
    [[nodiscard]] core::VoidResult Maximize();
    [[nodiscard]] core::VoidResult Restore();

    [[nodiscard]] core::Result<bool> IsVisible() const;
    [[nodiscard]] core::Result<bool> IsResizable() const;
    [[nodiscard]] core::VoidResult SetResizable(bool resizable);
    [[nodiscard]] core::Result<bool> IsFocused() const;
    [[nodiscard]] core::Result<bool> IsAlwaysOnTop() const;
    [[nodiscard]] core::VoidResult SetAlwaysOnTop(bool alwaysOnTop);

    [[nodiscard]] core::VoidResult StartTextInput();
    [[nodiscard]] core::VoidResult StopTextInput();
    [[nodiscard]] bool IsTextInputActive() const;
    [[nodiscard]] core::VoidResult ClearTextComposition();
    [[nodiscard]] core::VoidResult SetTextInputArea(TextInputArea area);
    [[nodiscard]] core::VoidResult ClearTextInputArea();

    [[nodiscard]] core::VoidResult SetMouseGrab(bool grabbed);
    [[nodiscard]] bool IsMouseGrabbed() const;
    [[nodiscard]] core::VoidResult SetRelativeMouseMode(bool enabled);
    [[nodiscard]] bool IsRelativeMouseModeEnabled() const;

    [[nodiscard]] core::VoidResult Show();
    [[nodiscard]] core::VoidResult Hide();

private:
    friend class PlatformRuntime;

    explicit Window(std::unique_ptr<detail::WindowImpl> state) noexcept;

    std::unique_ptr<detail::WindowImpl> m_state;
};
} // namespace pond::platform

namespace std
{
template <>
struct formatter<pond::platform::WindowDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::WindowDesc& desc, FormatContext& context) const
    {
        const string minimumSize = desc.minimumLogicalSize.has_value()
                                       ? std::format("{}", *desc.minimumLogicalSize)
                                       : "none";
        return formatter<string>::format(
            std::format("title='{}', logicalSize={}, visible={}, resizable={}, "
                        "highPixelDensity={}, minimumLogicalSize={}, graphics={}",
                        desc.title, desc.logicalSize, desc.visible, desc.resizable,
                        desc.highPixelDensity, minimumSize, desc.graphicsCompatibility),
            context);
    }
};
} // namespace std

namespace pond::platform
{
inline std::ostream& operator<<(std::ostream& output, const WindowDesc& desc)
{
    return output << std::format("{}", desc);
}
} // namespace pond::platform