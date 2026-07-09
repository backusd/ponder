#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/TextInput.hpp>
#include <ponder/platform/WindowGraphics.hpp>
#include <ponder/platform/WindowState.hpp>

#include <memory>
#include <optional>
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
    WindowGraphicsCompatibility graphicsCompatibility{
        WindowGraphicsCompatibility::Default};
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

    [[nodiscard]] core::VoidResult Show();
    [[nodiscard]] core::VoidResult Hide();

private:
    friend class PlatformRuntime;

    explicit Window(std::unique_ptr<detail::WindowImpl> state) noexcept;

    std::unique_ptr<detail::WindowImpl> m_state;
};
} // namespace pond::platform
