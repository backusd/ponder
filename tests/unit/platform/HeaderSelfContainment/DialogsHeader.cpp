#include <ponder/platform/Dialogs.hpp>

#include <concepts>
#include <cstdint>
#include <format>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>

namespace dialogs = ponder::platform::dialogs;

template <typename Type>
concept FormattableAndStreamable = std::formattable<Type, char> && requires(std::ostream& output, const Type& value) {
    { output << value } -> std::same_as<std::ostream&>;
};

static_assert(!std::same_as<dialogs::DialogRequestId, ponder::platform::WindowId>);
static_assert(!std::convertible_to<std::uint64_t, dialogs::DialogRequestId>);
static_assert(!std::convertible_to<dialogs::DialogRequestId, std::uint64_t>);
static_assert(std::is_trivially_copyable_v<dialogs::DialogRequestId>);
static_assert(!dialogs::DialogRequestId{}.IsValid());
static_assert(dialogs::DialogRequestId{17}.IsValid());
static_assert(dialogs::DialogRequestId{17}.GetValue() == 17U);
static_assert(std::is_scoped_enum_v<dialogs::DialogKind>);
static_assert(FormattableAndStreamable<dialogs::DialogKind>);
static_assert(FormattableAndStreamable<dialogs::DialogRequestId>);
static_assert(FormattableAndStreamable<dialogs::DialogFileFilter>);
static_assert(FormattableAndStreamable<dialogs::OpenFileDialogDesc>);
static_assert(FormattableAndStreamable<dialogs::SaveFileDialogDesc>);
static_assert(FormattableAndStreamable<dialogs::OpenFolderDialogDesc>);

static_assert(requires {
    std::unordered_set<dialogs::DialogRequestId>{};
    dialogs::OpenFileDialogDesc{};
    dialogs::SaveFileDialogDesc{};
    dialogs::OpenFolderDialogDesc{};
});

[[maybe_unused]] void VerifyDialogFormattingAndStreaming()
{
    const dialogs::DialogFileFilter filter{.name = "Molecules", .pattern = "sdf;mol"};
    [[maybe_unused]] const std::string formatted = std::format("{}", filter);
    std::ostringstream stream;
    stream << dialogs::DialogKind::OpenFile << filter << dialogs::DialogRequestId{1};
}
