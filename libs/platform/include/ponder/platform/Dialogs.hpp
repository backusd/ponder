#pragma once

#include <ponder/core/Identifier.hpp>
#include <ponder/platform/Identifiers.hpp>

#include <filesystem>
#include <format>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace ponder::platform::dialogs
{
namespace detail
{
struct DialogRequestIdTag final
{
};
} // namespace detail

enum class DialogKind : std::uint8_t
{
    OpenFile,
    SaveFile,
    OpenFolder
};

using DialogRequestId = ponder::core::Identifier<detail::DialogRequestIdTag>;

struct DialogFileFilter final
{
    std::string name;
    std::string pattern;

    [[nodiscard]] friend bool operator==(const DialogFileFilter& lhs, const DialogFileFilter& rhs) = default;
};

struct OpenFileDialogDesc final
{
    std::optional<WindowId> parentWindowId;
    std::optional<std::filesystem::path> defaultLocation;
    std::vector<DialogFileFilter> filters;
    bool allowMultipleSelection{};
};

struct SaveFileDialogDesc final
{
    std::optional<WindowId> parentWindowId;
    std::optional<std::filesystem::path> defaultLocation;
    std::vector<DialogFileFilter> filters;
};

struct OpenFolderDialogDesc final
{
    std::optional<WindowId> parentWindowId;
    std::optional<std::filesystem::path> defaultLocation;
    bool allowMultipleSelection{};
};
} // namespace ponder::platform::dialogs

namespace std
{
template <>
struct formatter<ponder::platform::dialogs::DialogKind> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::dialogs::DialogKind kind, FormatContext& context) const
    {
        using enum ponder::platform::dialogs::DialogKind;
        switch (kind)
        {
        case OpenFile:
            return formatter<string_view>::format("open-file", context);
        case SaveFile:
            return formatter<string_view>::format("save-file", context);
        case OpenFolder:
            return formatter<string_view>::format("open-folder", context);
        }

        return formatter<string_view>::format("unrecognized", context);
    }
};

template <>
struct formatter<ponder::platform::dialogs::DialogRequestId> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::dialogs::DialogRequestId id, FormatContext& context) const
    {
        const string text = id.IsValid() ? std::format("{}", id.GetValue()) : "invalid";
        return formatter<string>::format(text, context);
    }
};

template <>
struct formatter<ponder::platform::dialogs::DialogFileFilter> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::dialogs::DialogFileFilter& filter, FormatContext& context) const
    {
        return formatter<string>::format(std::format("'{}' ({})", filter.name, filter.pattern), context);
    }
};

template <>
struct formatter<ponder::platform::dialogs::OpenFileDialogDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::dialogs::OpenFileDialogDesc& desc, FormatContext& context) const
    {
        const string parent = desc.parentWindowId.has_value() ? std::format("{}", *desc.parentWindowId) : "none";
        const string location = desc.defaultLocation.has_value() ? std::format("'{}'", desc.defaultLocation->string()) : "none";
        return formatter<string>::format(std::format("open_file_dialog(parent={}, defaultLocation={}, filterCount={}, allowMultipleSelection={})",
                                                     parent, location, desc.filters.size(), desc.allowMultipleSelection),
                                         context);
    }
};

template <>
struct formatter<ponder::platform::dialogs::SaveFileDialogDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::dialogs::SaveFileDialogDesc& desc, FormatContext& context) const
    {
        const string parent = desc.parentWindowId.has_value() ? std::format("{}", *desc.parentWindowId) : "none";
        const string location = desc.defaultLocation.has_value() ? std::format("'{}'", desc.defaultLocation->string()) : "none";
        return formatter<string>::format(
            std::format("save_file_dialog(parent={}, defaultLocation={}, filterCount={})", parent, location, desc.filters.size()), context);
    }
};

template <>
struct formatter<ponder::platform::dialogs::OpenFolderDialogDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::platform::dialogs::OpenFolderDialogDesc& desc, FormatContext& context) const
    {
        const string parent = desc.parentWindowId.has_value() ? std::format("{}", *desc.parentWindowId) : "none";
        const string location = desc.defaultLocation.has_value() ? std::format("'{}'", desc.defaultLocation->string()) : "none";
        return formatter<string>::format(std::format("open_folder_dialog(parent={}, defaultLocation={}, allowMultipleSelection={})", parent, location,
                                                     desc.allowMultipleSelection),
                                         context);
    }
};
} // namespace std

namespace ponder::platform::dialogs
{
inline std::ostream& operator<<(std::ostream& output, DialogKind kind)
{
    return output << std::format("{}", kind);
}

inline std::ostream& operator<<(std::ostream& output, const DialogFileFilter& filter)
{
    return output << std::format("{}", filter);
}

inline std::ostream& operator<<(std::ostream& output, const OpenFileDialogDesc& desc)
{
    return output << std::format("{}", desc);
}

inline std::ostream& operator<<(std::ostream& output, const SaveFileDialogDesc& desc)
{
    return output << std::format("{}", desc);
}

inline std::ostream& operator<<(std::ostream& output, const OpenFolderDialogDesc& desc)
{
    return output << std::format("{}", desc);
}
} // namespace ponder::platform::dialogs

namespace ponder::platform::dialogs::detail
{
inline std::ostream& operator<<(std::ostream& output, DialogRequestId id)
{
    return output << std::format("{}", id);
}
} // namespace ponder::platform::dialogs::detail
