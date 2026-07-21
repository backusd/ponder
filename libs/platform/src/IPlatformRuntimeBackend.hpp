#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/HintManager.hpp>
#include <ponder/platform/Mouse.hpp>

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "IPlatformWindowBackend.hpp"

union SDL_Event;

namespace pond::platform::detail
{
enum class ApplicationMetadataProperty : std::uint8_t
{
    Name,
    Version,
    Identifier
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
    [[nodiscard]] virtual core::VoidResult SetMouseCapture(bool enabled) = 0;
    [[nodiscard]] virtual core::Result<CursorHandle> CreateSystemCursor(
        SystemCursorShape shape) = 0;
    [[nodiscard]] virtual core::VoidResult SetCursor(CursorHandle cursor) = 0;
    virtual void DestroyCursor(CursorHandle cursor) = 0;
    [[nodiscard]] virtual core::VoidResult ShowCursor() = 0;
    [[nodiscard]] virtual core::VoidResult HideCursor() = 0;
    [[nodiscard]] virtual bool IsCursorVisible() = 0;
    [[nodiscard]] virtual bool SupportsClipboardText() = 0;
    [[nodiscard]] virtual core::Result<std::string> GetClipboardText() = 0;
    [[nodiscard]] virtual core::VoidResult SetClipboardText(std::string_view text) = 0;
    [[nodiscard]] virtual core::VoidResult OpenExternalUri(std::string_view uri) = 0;
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
template <>
struct formatter<pond::platform::detail::CursorHandle> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::CursorHandle cursor, FormatContext& context) const
    {
        const string text =
            cursor.IsValid() ? std::format("0x{:X}", cursor.GetValue()) : "invalid";
        return formatter<string>::format(text, context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendDialogKind> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::BackendDialogKind kind,
                FormatContext& context) const
    {
        using pond::platform::detail::BackendDialogKind;

        string_view name{"unknown"};
        switch (kind)
        {
        case BackendDialogKind::OpenFile:
            name = "open_file";
            break;
        case BackendDialogKind::SaveFile:
            name = "save_file";
            break;
        case BackendDialogKind::OpenFolder:
            name = "open_folder";
            break;
        }

        return formatter<string_view>::format(name, context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendDialogFileFilter> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::detail::BackendDialogFileFilter& filter,
                FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("'{}' ({})", filter.name, filter.pattern), context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendDialogSelection> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::detail::BackendDialogSelection& selection,
                FormatContext& context) const
    {
        const string filterIndex = selection.selectedFilterIndex.has_value()
                                       ? std::format("{}", *selection.selectedFilterIndex)
                                       : "none";
        return formatter<string>::format(
            std::format("selection(pathCount={}, selectedFilter={})", selection.paths.size(),
                        filterIndex),
            context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendDialogCancellation> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::detail::BackendDialogCancellation,
                FormatContext& context) const
    {
        return formatter<string_view>::format("cancelled", context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendDialogFailure> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::detail::BackendDialogFailure& failure,
                FormatContext& context) const
    {
        return formatter<string>::format(std::format("failure('{}')", failure.message), context);
    }
};

template <>
struct formatter<pond::platform::detail::BackendDialogRequestDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::detail::BackendDialogRequestDesc& desc,
                FormatContext& context) const
    {
        const string parent = desc.parentWindow.has_value()
                                  ? std::format("{}", *desc.parentWindow)
                                  : "none";
        return formatter<string>::format(
            std::format("kind={}, parent={}, filterCount={}, hasDefaultLocation={}, "
                        "allowMultipleSelection={}",
                        desc.kind, parent, desc.filters.size(), desc.defaultLocation.has_value(),
                        desc.allowMultipleSelection),
            context);
    }
};
} // namespace std

namespace pond::platform::detail
{
inline std::ostream& operator<<(std::ostream& output, ApplicationMetadataProperty property)
{
    return output << std::format("{}", property);
}

inline std::ostream& operator<<(std::ostream& output, CursorHandle cursor)
{
    return output << std::format("{}", cursor);
}

inline std::ostream& operator<<(std::ostream& output, BackendDialogKind kind)
{
    return output << std::format("{}", kind);
}
} // namespace pond::platform::detail
namespace pond::platform::detail
{
inline std::ostream& operator<<(std::ostream& output, const BackendDialogFileFilter& filter)
{
    return output << std::format("{}", filter);
}

inline std::ostream& operator<<(std::ostream& output, const BackendDialogSelection& selection)
{
    return output << std::format("{}", selection);
}

inline std::ostream& operator<<(std::ostream& output, BackendDialogCancellation cancellation)
{
    return output << std::format("{}", cancellation);
}

inline std::ostream& operator<<(std::ostream& output, const BackendDialogFailure& failure)
{
    return output << std::format("{}", failure);
}

inline std::ostream& operator<<(std::ostream& output, const BackendDialogRequestDesc& desc)
{
    return output << std::format("{}", desc);
}
} // namespace pond::platform::detail
