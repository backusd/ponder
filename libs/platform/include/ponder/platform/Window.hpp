#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/WindowGraphics.hpp>

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
class WindowState;
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

    [[nodiscard]] core::VoidResult Show();
    [[nodiscard]] core::VoidResult Hide();

private:
    friend class PlatformRuntime;

    explicit Window(std::unique_ptr<detail::WindowState> state) noexcept;

    std::unique_ptr<detail::WindowState> m_state;
};
} // namespace pond::platform
