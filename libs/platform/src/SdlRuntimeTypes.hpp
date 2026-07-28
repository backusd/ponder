#pragma once

#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/Mouse.hpp>
#include <ponder/platform/PlatformEvent.hpp>
#include <ponder/platform/WindowGraphics.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "IPlatformWindowBackend.hpp"

namespace ponder::platform::detail
{
class SdlRuntime;

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
    explicit constexpr CursorHandle(ValueType value) noexcept :
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

    friend constexpr bool operator==(const CursorHandle&, const CursorHandle&) noexcept = default;

private:
    ValueType m_value{};
};

enum class BackendEventKind : std::uint8_t
{
    Other,
    DisplayAdded,
    DisplayRemoved,
    DisplayChanged,
    WindowDisplayChanged,
    WindowShown
};

class BackendEvent final
{
public:
    [[nodiscard]] BackendEventKind GetKind() const noexcept
    {
        return m_kind;
    }

    [[nodiscard]] std::uint32_t GetBackendWindowId() const noexcept
    {
        return m_backendWindowId;
    }

    [[nodiscard]] std::uint32_t GetBackendDisplayId() const noexcept
    {
        return m_backendDisplayId;
    }

private:
    friend class SdlRuntime;

    static constexpr std::size_t kStorageSize{128};

    std::array<std::byte, kStorageSize> m_storage{};
    BackendEventKind m_kind{BackendEventKind::Other};
    std::uint32_t m_backendWindowId{};
    std::uint32_t m_backendDisplayId{};
};

struct EventTranslationContext final
{
    void* context{};
    std::optional<WindowId> (*resolveWindowId)(void* context, std::uint32_t backendWindowId){};
    std::optional<DisplayId> (*resolveDisplayId)(void* context, std::uint32_t backendDisplayId){};
};

inline constexpr std::size_t kSystemCursorShapeCount{11};

[[nodiscard]] bool IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility compatibility) noexcept;
[[nodiscard]] BackendNativeWindowDriver GetNativeWindowDriver(std::string_view driverName) noexcept;
[[nodiscard]] std::uint64_t BuildSdlWindowFlags(const BackendWindowCreateDesc& desc) noexcept;
[[nodiscard]] bool IsReservedSdlWindowPosition(std::int32_t value) noexcept;
} // namespace ponder::platform::detail

namespace std
{
template <>
struct formatter<ponder::platform::detail::ApplicationMetadataProperty> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::detail::ApplicationMetadataProperty property, FormatContext& context) const
    {
        using ponder::platform::detail::ApplicationMetadataProperty;

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
struct formatter<ponder::platform::detail::CursorHandle> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::detail::CursorHandle cursor, FormatContext& context) const
    {
        const string text = cursor.IsValid() ? std::format("0x{:X}", cursor.GetValue()) : "invalid";
        return formatter<string>::format(text, context);
    }
};

template <>
struct formatter<ponder::platform::detail::BackendEventKind> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::detail::BackendEventKind kind, FormatContext& context) const
    {
        using ponder::platform::detail::BackendEventKind;

        string_view name{"unknown"};
        switch (kind)
        {
        case BackendEventKind::Other:
            name = "other";
            break;
        case BackendEventKind::DisplayAdded:
            name = "display-added";
            break;
        case BackendEventKind::DisplayRemoved:
            name = "display-removed";
            break;
        case BackendEventKind::DisplayChanged:
            name = "display-changed";
            break;
        case BackendEventKind::WindowDisplayChanged:
            name = "window-display-changed";
            break;
        case BackendEventKind::WindowShown:
            name = "window-shown";
            break;
        }

        return formatter<string_view>::format(name, context);
    }
};

template <>
struct formatter<ponder::platform::detail::BackendEvent> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::BackendEvent& event, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("kind={}, backendWindowId={}, backendDisplayId={}", event.GetKind(), event.GetBackendWindowId(), event.GetBackendDisplayId()),
            context);
    }
};

template <>
struct formatter<ponder::platform::detail::EventTranslationContext> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::detail::EventTranslationContext& translation, FormatContext& context) const
    {
        return formatter<string>::format(std::format("hasContext={}, resolvesWindows={}, resolvesDisplays={}", translation.context != nullptr,
                                                     translation.resolveWindowId != nullptr, translation.resolveDisplayId != nullptr),
                                         context);
    }
};
} // namespace std

namespace ponder::platform::detail
{
inline std::ostream& operator<<(std::ostream& output, ApplicationMetadataProperty property)
{
    return output << std::format("{}", property);
}

inline std::ostream& operator<<(std::ostream& output, CursorHandle cursor)
{
    return output << std::format("{}", cursor);
}

inline std::ostream& operator<<(std::ostream& output, BackendEventKind kind)
{
    return output << std::format("{}", kind);
}

inline std::ostream& operator<<(std::ostream& output, const BackendEvent& event)
{
    return output << std::format("{}", event);
}

inline std::ostream& operator<<(std::ostream& output, const EventTranslationContext& translation)
{
    return output << std::format("{}", translation);
}
} // namespace ponder::platform::detail
