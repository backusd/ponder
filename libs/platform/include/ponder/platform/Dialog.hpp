#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/Identifiers.hpp>

#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace pond::platform
{
struct DialogFileFilter final
{
    std::string name;
    std::string pattern;

    [[nodiscard]] friend bool operator==(const DialogFileFilter& lhs,
                                         const DialogFileFilter& rhs) = default;
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

struct DialogSelection final
{
    std::vector<std::filesystem::path> paths;
    std::optional<std::size_t> selectedFilterIndex;

    [[nodiscard]] friend bool operator==(const DialogSelection& lhs,
                                         const DialogSelection& rhs) = default;
};

struct DialogCancellation final
{
    [[nodiscard]] friend constexpr bool operator==(DialogCancellation,
                                                   DialogCancellation) noexcept = default;
};

struct DialogFailure final
{
    core::Error error;

    [[nodiscard]] friend bool operator==(const DialogFailure& lhs,
                                         const DialogFailure& rhs) noexcept
    {
        return lhs.error.GetCode() == rhs.error.GetCode() &&
               lhs.error.GetMessage() == rhs.error.GetMessage();
    }
};

using DialogOutcome = std::variant<DialogSelection, DialogCancellation, DialogFailure>;
} // namespace pond::platform

namespace std
{
template <>
struct formatter<pond::platform::DialogFileFilter> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DialogFileFilter& filter, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("'{}' ({})", filter.name, filter.pattern), context);
    }
};

template <>
struct formatter<pond::platform::OpenFileDialogDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::OpenFileDialogDesc& desc, FormatContext& context) const
    {
        const string parent = desc.parentWindowId.has_value()
                                  ? std::format("{}", *desc.parentWindowId)
                                  : "none";
        const string location = desc.defaultLocation.has_value()
                                    ? std::format("'{}'", desc.defaultLocation->string())
                                    : "none";
        return formatter<string>::format(
            std::format("open_file_dialog(parent={}, defaultLocation={}, filterCount={}, "
                        "allowMultipleSelection={})",
                        parent, location, desc.filters.size(), desc.allowMultipleSelection),
            context);
    }
};

template <>
struct formatter<pond::platform::SaveFileDialogDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::SaveFileDialogDesc& desc, FormatContext& context) const
    {
        const string parent = desc.parentWindowId.has_value()
                                  ? std::format("{}", *desc.parentWindowId)
                                  : "none";
        const string location = desc.defaultLocation.has_value()
                                    ? std::format("'{}'", desc.defaultLocation->string())
                                    : "none";
        return formatter<string>::format(
            std::format("save_file_dialog(parent={}, defaultLocation={}, filterCount={})",
                        parent, location, desc.filters.size()),
            context);
    }
};

template <>
struct formatter<pond::platform::OpenFolderDialogDesc> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::OpenFolderDialogDesc& desc, FormatContext& context) const
    {
        const string parent = desc.parentWindowId.has_value()
                                  ? std::format("{}", *desc.parentWindowId)
                                  : "none";
        const string location = desc.defaultLocation.has_value()
                                    ? std::format("'{}'", desc.defaultLocation->string())
                                    : "none";
        return formatter<string>::format(
            std::format("open_folder_dialog(parent={}, defaultLocation={}, "
                        "allowMultipleSelection={})",
                        parent, location, desc.allowMultipleSelection),
            context);
    }
};
template <>
struct formatter<pond::platform::DialogSelection> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DialogSelection& selection, FormatContext& context) const
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
struct formatter<pond::platform::DialogCancellation> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::DialogCancellation, FormatContext& context) const
    {
        return formatter<string_view>::format("cancelled", context);
    }
};

template <>
struct formatter<pond::platform::DialogFailure> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::platform::DialogFailure& failure, FormatContext& context) const
    {
        return formatter<string>::format(std::format("failure({})", failure.error), context);
    }
};
} // namespace std

namespace pond::platform
{
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
inline std::ostream& operator<<(std::ostream& output, const DialogSelection& selection)
{
    return output << std::format("{}", selection);
}

inline std::ostream& operator<<(std::ostream& output, DialogCancellation cancellation)
{
    return output << std::format("{}", cancellation);
}

inline std::ostream& operator<<(std::ostream& output, const DialogFailure& failure)
{
    return output << std::format("{}", failure);
}
} // namespace pond::platform