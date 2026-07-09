#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/platform/Identifiers.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
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
