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

namespace ponder::platform
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
class MockRuntime;
class SdlRuntime;
class WindowImpl;
} // namespace detail

class Runtime;

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
    [[nodiscard]] ponder::core::Result<NativeWindowHandle> GetNativeHandle() const;

    [[nodiscard]] std::string GetTitle() const;
    void SetTitle(std::string_view title);

    [[nodiscard]] ScreenPosition GetPosition() const;
    void SetPosition(ScreenPosition position);

    [[nodiscard]] LogicalSize GetLogicalSize() const;
    [[nodiscard]] PixelSize GetPixelSize() const;
    void SetLogicalSize(LogicalSize size);
    [[nodiscard]] ponder::core::Result<DisplayId> GetDisplayId() const;
    [[nodiscard]] float GetPixelDensity() const;
    [[nodiscard]] float GetDisplayScale() const;

    [[nodiscard]] WindowPresentation GetPresentation() const;
    void SetPresentation(WindowPresentation presentation);
    [[nodiscard]] WindowDecoration GetDecoration() const;
    void SetDecoration(WindowDecoration decoration);
    [[nodiscard]] WindowState GetState() const;
    void Minimize();
    void Maximize();
    void Restore();

    [[nodiscard]] bool IsVisible() const;
    [[nodiscard]] bool IsResizable() const;
    void SetResizable(bool resizable);
    [[nodiscard]] bool IsFocused() const;
    [[nodiscard]] bool IsAlwaysOnTop() const;
    void SetAlwaysOnTop(bool alwaysOnTop);

    void StartTextInput();
    void StopTextInput();
    [[nodiscard]] bool IsTextInputActive() const;
    void ClearTextComposition();
    void SetTextInputArea(TextInputArea area);
    void ClearTextInputArea();

    void SetMouseGrab(bool grabbed);
    [[nodiscard]] bool IsMouseGrabbed() const;
    void SetRelativeMouseMode(bool enabled);
    [[nodiscard]] bool IsRelativeMouseModeEnabled() const;

    void Show();
    void Hide();

private:
    friend class Runtime;
    friend class detail::MockRuntime;
    friend class detail::SdlRuntime;

    explicit Window(std::unique_ptr<detail::WindowImpl> state) noexcept;

    std::unique_ptr<detail::WindowImpl> m_state;
};
} // namespace ponder::platform

namespace std
{
template <>
struct formatter<ponder::platform::WindowDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::WindowDesc& desc, FormatContext& context) const
    {
        const string minimumSize = desc.minimumLogicalSize.has_value() ? std::format("{}", *desc.minimumLogicalSize) : "none";
        return formatter<string>::format(std::format("title='{}', logicalSize={}, visible={}, resizable={}, "
                                                     "highPixelDensity={}, minimumLogicalSize={}, graphics={}",
                                                     desc.title, desc.logicalSize, desc.visible, desc.resizable, desc.highPixelDensity, minimumSize,
                                                     desc.graphicsCompatibility),
                                         context);
    }
};
} // namespace std

namespace ponder::platform
{
inline std::ostream& operator<<(std::ostream& output, const WindowDesc& desc)
{
    return output << std::format("{}", desc);
}
} // namespace ponder::platform
