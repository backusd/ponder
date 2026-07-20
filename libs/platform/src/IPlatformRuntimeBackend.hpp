#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/HintManager.hpp>
#include <ponder/platform/Mouse.hpp>

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

union SDL_Event;

namespace pond::platform::detail
{
enum class ApplicationMetadataProperty : std::uint8_t
{
    Name,
    Version,
    Identifier
};

struct BackendClipboardTextResult final
{
    std::string text;
    std::string errorText;
    bool succeeded{};
};

class CursorHandle final
{
public:
    using ValueType = std::uintptr_t;

    constexpr CursorHandle() noexcept = default;
    explicit constexpr CursorHandle(ValueType value) noexcept : m_value(value) {}

    [[nodiscard]] constexpr bool IsValid() const noexcept { return m_value != 0; }
    [[nodiscard]] constexpr ValueType GetValue() const noexcept { return m_value; }

    friend constexpr bool operator==(const CursorHandle&, const CursorHandle&) noexcept = default;

private:
    ValueType m_value{};
};

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

enum class BackendDialogKind : std::uint8_t
{
    OpenFile,
    SaveFile,
    OpenFolder
};

struct BackendDialogFileFilter final
{
    std::string name;
    std::string pattern;
};

struct BackendDialogSelection final
{
    std::vector<std::string> paths;
    std::optional<std::size_t> selectedFilterIndex;
};

struct BackendDialogCancellation final
{
};

struct BackendDialogFailure final
{
    std::string message;
};

using BackendDialogOutcome =
    std::variant<BackendDialogSelection, BackendDialogCancellation, BackendDialogFailure>;

class IBackendDialogCallback
{
public:
    virtual ~IBackendDialogCallback() noexcept = default;

    IBackendDialogCallback(const IBackendDialogCallback&) = delete;
    IBackendDialogCallback& operator=(const IBackendDialogCallback&) = delete;
    IBackendDialogCallback(IBackendDialogCallback&&) = delete;
    IBackendDialogCallback& operator=(IBackendDialogCallback&&) = delete;

    virtual void OnDialogCompleted(BackendDialogOutcome outcome) noexcept = 0;
    virtual void OnDialogCallbackFailure() noexcept = 0;

protected:
    IBackendDialogCallback() noexcept = default;
};

struct BackendDialogRequestDesc final
{
    BackendDialogKind kind{BackendDialogKind::OpenFile};
    IBackendDialogCallback& callback;
    std::optional<BackendWindowHandle> parentWindow;
    std::span<const BackendDialogFileFilter> filters;
    std::optional<std::string_view> defaultLocation;
    bool allowMultipleSelection{};
};

class IPlatformRuntimeBackend
{
public:
    virtual ~IPlatformRuntimeBackend() noexcept = default;

    IPlatformRuntimeBackend(const IPlatformRuntimeBackend&) = delete;
    IPlatformRuntimeBackend& operator=(const IPlatformRuntimeBackend&) = delete;
    IPlatformRuntimeBackend(IPlatformRuntimeBackend&&) = delete;
    IPlatformRuntimeBackend& operator=(IPlatformRuntimeBackend&&) = delete;

    [[nodiscard]] virtual HintManager& GetHintManager() = 0;
    [[nodiscard]] virtual bool IsMainThread() = 0;
    [[nodiscard]] virtual bool HasInitializedSubsystems() = 0;
    [[nodiscard]] virtual bool HasExpectedRuntimeSubsystems() = 0;
    [[nodiscard]] virtual core::VoidResult SetAppMetadataProperty(
        ApplicationMetadataProperty property, const char* value) = 0;
    [[nodiscard]] virtual core::VoidResult InitializeVideo() = 0;
    virtual void Quit() = 0;
    [[nodiscard]] virtual std::uint64_t GetTicksNanoseconds() = 0;
    [[nodiscard]] virtual bool PollEvent(SDL_Event* event) = 0;
    [[nodiscard]] virtual bool SupportsGlobalMouse() = 0;
    [[nodiscard]] virtual MousePosition GetGlobalMousePosition() = 0;
    [[nodiscard]] virtual bool SetMouseCapture(bool enabled) = 0;
    [[nodiscard]] virtual CursorHandle CreateSystemCursor(SystemCursorShape shape) = 0;
    [[nodiscard]] virtual bool SetCursor(CursorHandle cursor) = 0;
    virtual void DestroyCursor(CursorHandle cursor) = 0;
    [[nodiscard]] virtual bool ShowCursor() = 0;
    [[nodiscard]] virtual bool HideCursor() = 0;
    [[nodiscard]] virtual bool IsCursorVisible() = 0;
    [[nodiscard]] virtual bool SupportsClipboardText() = 0;
    [[nodiscard]] virtual BackendClipboardTextResult GetClipboardText() = 0;
    [[nodiscard]] virtual bool SetClipboardText(std::string_view text) = 0;
    [[nodiscard]] virtual bool OpenExternalUri(std::string_view uri) = 0;
    virtual void ShowDialog(const BackendDialogRequestDesc& desc) = 0;

protected:
    IPlatformRuntimeBackend() noexcept = default;
};
} // namespace pond::platform::detail

namespace std
{
template <>
struct formatter<pond::platform::detail::ApplicationMetadataProperty> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::ApplicationMetadataProperty property,
                FormatContext& context) const
    {
        using pond::platform::detail::ApplicationMetadataProperty;

        string_view name{"unknown"};
        switch (property)
        {
        case ApplicationMetadataProperty::Name:
            name = "name";
            break;
        case ApplicationMetadataProperty::Version:
            name = "version";
            break;
        case ApplicationMetadataProperty::Identifier:
            name = "identifier";
            break;
        }

        return formatter<string_view>::format(name, context);
    }
};
} // namespace std
