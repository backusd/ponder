#include <ponder/platform/Runtime.hpp>

#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

static_assert(std::same_as<decltype(ponder::platform::Runtime::Create(ponder::platform::RuntimeDesc{})), ponder::platform::Runtime>);
static_assert(
    std::same_as<decltype(std::declval<ponder::platform::Runtime&>().HintPush(std::declval<const ponder::platform::hints::VideoDriver&>())), void>);
static_assert(std::same_as<decltype(std::declval<ponder::platform::Runtime&>().HintPop<ponder::platform::hints::VideoDriver>()), void>);
static_assert(std::same_as<decltype(std::declval<ponder::platform::Runtime&>().HintClear<ponder::platform::hints::VideoDriver>()), void>);
static_assert(std::same_as<decltype(std::declval<const ponder::platform::Runtime&>().HintGet<ponder::platform::hints::VideoDriver>()),
                           std::optional<ponder::platform::hints::VideoDriver>>);
static_assert(std::same_as<decltype(std::declval<const ponder::platform::Runtime&>().ClipboardGetText()), ponder::core::Result<std::string>>);
static_assert(
    std::same_as<decltype(std::declval<ponder::platform::Runtime&>().ClipboardSetText(std::declval<std::string_view>())), ponder::core::VoidResult>);
static_assert(
    std::same_as<decltype(std::declval<ponder::platform::Runtime&>().DialogShowOpenFile(std::declval<const ponder::platform::OpenFileDialogDesc&>())),
                 ponder::platform::DialogRequestId>);
static_assert(
    std::same_as<decltype(std::declval<ponder::platform::Runtime&>().DialogShowSaveFile(std::declval<const ponder::platform::SaveFileDialogDesc&>())),
                 ponder::platform::DialogRequestId>);
static_assert(std::same_as<decltype(std::declval<ponder::platform::Runtime&>().DialogShowOpenFolder(
                               std::declval<const ponder::platform::OpenFolderDialogDesc&>())),
                           ponder::platform::DialogRequestId>);
static_assert(std::same_as<decltype(std::declval<const ponder::platform::Runtime&>().DialogGetPendingCount()), std::size_t>);
static_assert(std::same_as<decltype(std::declval<const ponder::platform::Runtime&>().DialogHasPending()), bool>);
static_assert(
    std::same_as<decltype(std::declval<const ponder::platform::Runtime&>().DialogGetPending()), std::vector<ponder::platform::DialogRequestInfo>>);
static_assert(
    std::same_as<decltype(std::declval<ponder::platform::Runtime&>().DialogPollCompletion()), std::optional<ponder::platform::DialogCompletedEvent>>);
static_assert(std::same_as<decltype(std::declval<const ponder::platform::Runtime&>().DialogGetOutstandingRequestCount()), std::size_t>);
static_assert(std::same_as<decltype(std::declval<ponder::platform::Runtime&>().DialogShutdown()), void>);
static_assert(std::same_as<decltype(std::declval<ponder::platform::Runtime&>().WindowCreate(std::declval<const ponder::platform::WindowDesc&>())),
                           ponder::platform::Window>);
static_assert(std::same_as<decltype(std::declval<const ponder::platform::Runtime&>().TimeNow()), ponder::core::Timestamp>);
static_assert(std::same_as<decltype(std::declval<ponder::platform::Runtime&>().EventPoll()), std::optional<ponder::platform::PlatformEvent>>);
static_assert(std::same_as<decltype(std::declval<ponder::platform::Runtime&>().DisplayEnumerate()), std::vector<ponder::platform::DisplayInfo>>);
static_assert(std::same_as<decltype(std::declval<ponder::platform::Runtime&>().DisplayGetInfo(std::declval<ponder::platform::DisplayId>())),
                           ponder::core::Result<ponder::platform::DisplayInfo>>);
static_assert(std::same_as<decltype(std::declval<ponder::platform::Runtime&>().MouseSetCapture(true)), ponder::core::VoidResult>);
static_assert(std::same_as<decltype(std::declval<const ponder::platform::Runtime&>().MouseGetGlobalPosition()),
                           ponder::core::Result<ponder::platform::LogicalPoint>>);
static_assert(
    std::same_as<decltype(std::declval<ponder::platform::Runtime&>().UriOpenExternal(std::declval<std::string_view>())), ponder::core::VoidResult>);
