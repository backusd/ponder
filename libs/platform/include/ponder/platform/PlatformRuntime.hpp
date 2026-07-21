#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/Dialog.hpp>
#include <ponder/platform/Display.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/HintManager.hpp>
#include <ponder/platform/Mouse.hpp>
#include <ponder/platform/PlatformEvent.hpp>
#include <ponder/platform/Timing.hpp>
#include <ponder/platform/Window.hpp>

#include <format>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace pond::platform
{
using ConfigureHintsBeforeInitialization = void (*)(HintManager&);

struct PlatformRuntimeDesc final
{
    std::string applicationName{"ponder"};
    std::optional<std::string> applicationVersion;
    std::optional<std::string> applicationIdentifier;
    ConfigureHintsBeforeInitialization configureHintsBeforeInitialization{};
};

namespace detail
{
class PlatformRuntimeState;
}

class PlatformRuntime final
{
public:
    [[nodiscard]] static core::Result<PlatformRuntime> Create(const PlatformRuntimeDesc& desc);

    ~PlatformRuntime() noexcept;

    PlatformRuntime(const PlatformRuntime&) = delete;
    PlatformRuntime& operator=(const PlatformRuntime&) = delete;
    PlatformRuntime(PlatformRuntime&&) noexcept;
    PlatformRuntime& operator=(PlatformRuntime&&) noexcept;

    [[nodiscard]] HintManager& GetHintManager();
    [[nodiscard]] Timestamp Now() const;
    [[nodiscard]] std::optional<PlatformEvent> PollEvent();
    [[nodiscard]] core::Result<Window> CreateWindow(const WindowDesc& desc);
    [[nodiscard]] core::Result<std::vector<DisplayInfo>> EnumerateDisplays();
    [[nodiscard]] core::Result<DisplayInfo> GetDisplayInfo(DisplayId id);

    [[nodiscard]] core::VoidResult SetMouseCapture(bool enabled);
    [[nodiscard]] core::Result<LogicalPoint> GetGlobalMousePosition() const;
    [[nodiscard]] core::VoidResult SetSystemCursor(SystemCursorShape shape);
    [[nodiscard]] core::VoidResult ShowCursor();
    [[nodiscard]] core::VoidResult HideCursor();
    [[nodiscard]] bool IsCursorVisible() const;

    [[nodiscard]] core::Result<std::string> GetClipboardText() const;
    [[nodiscard]] core::VoidResult SetClipboardText(std::string_view text);
    [[nodiscard]] core::VoidResult OpenExternalUri(std::string_view uri);

    [[nodiscard]] core::Result<DialogRequestId> ShowOpenFileDialog(const OpenFileDialogDesc& desc);
    [[nodiscard]] core::Result<DialogRequestId> ShowSaveFileDialog(const SaveFileDialogDesc& desc);
    [[nodiscard]] core::Result<DialogRequestId> ShowOpenFolderDialog(
        const OpenFolderDialogDesc& desc);

private:
    explicit PlatformRuntime(std::unique_ptr<detail::PlatformRuntimeState> state) noexcept;

    std::unique_ptr<detail::PlatformRuntimeState> m_state;
};
} // namespace pond::platform

namespace std
{
template <>
struct formatter<pond::platform::PlatformRuntimeDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::PlatformRuntimeDesc& desc, FormatContext& context) const
    {
        const string version =
            desc.applicationVersion.has_value() ? *desc.applicationVersion : "none";
        const string identifier =
            desc.applicationIdentifier.has_value() ? *desc.applicationIdentifier : "none";
        return formatter<string>::format(
            std::format("applicationName='{}', applicationVersion='{}', "
                        "applicationIdentifier='{}', configuresHints={}",
                        desc.applicationName, version, identifier,
                        desc.configureHintsBeforeInitialization != nullptr),
            context);
    }
};
} // namespace std

namespace pond::platform
{
inline std::ostream& operator<<(std::ostream& output, const PlatformRuntimeDesc& desc)
{
    return output << std::format("{}", desc);
}
} // namespace pond::platform