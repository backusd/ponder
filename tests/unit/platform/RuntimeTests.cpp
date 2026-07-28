#include <ponder/core/Exception.hpp>
#include <ponder/core/Timing.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Runtime.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <format>
#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "MockRuntime.hpp"

namespace
{
using ponder::platform::PlatformErrorCode;
using ponder::platform::Runtime;
using ponder::platform::RuntimeDesc;
using ponder::platform::detail::MockRuntimeControl;
using ponder::platform::detail::ScopedMockRuntimeBinding;

static_assert(!std::is_copy_constructible_v<Runtime>);
static_assert(!std::is_copy_assignable_v<Runtime>);
static_assert(std::is_nothrow_move_constructible_v<Runtime>);
static_assert(std::is_nothrow_move_assignable_v<Runtime>);

[[nodiscard]] bool HasPlatformErrorCode(const ponder::core::Exception& exception, PlatformErrorCode expectedCode)
{
    return exception.GetMessage().starts_with(std::format("Platform error [{}]: ", expectedCode));
}

template <typename Action>
[[nodiscard]] bool ThrowsPlatformException(Action&& action, PlatformErrorCode expectedCode)
{
    try
    {
        std::forward<Action>(action)();
    }
    catch (const ponder::core::Exception& exception)
    {
        return HasPlatformErrorCode(exception, expectedCode);
    }
    catch (...)
    {
    }
    return false;
}

template <typename Action>
void ExpectPlatformException(Action&& action, PlatformErrorCode expectedCode, std::string_view expectedMessage = {})
{
    try
    {
        std::forward<Action>(action)();
        FAIL() << "Expected ponder::core::Exception";
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_TRUE(HasPlatformErrorCode(exception, expectedCode));
        if (!expectedMessage.empty())
        {
            EXPECT_NE(exception.GetMessage().find(expectedMessage), std::string_view::npos);
        }
    }
    catch (...)
    {
        FAIL() << "Expected ponder::core::Exception";
    }
}

thread_local int hintConfigurationCallCount{};
thread_local bool attemptReentrantRuntimeCreation{};
thread_local bool reentrantRuntimeCreationRejected{};
thread_local bool nonHintCallRejectedDuringConfiguration{};
thread_local bool pushOnceHintRejected{};

void ConfigureRuntimeHints(Runtime& runtime)
{
    ++hintConfigurationCallCount;
    runtime.HintPush(ponder::platform::hints::VideoDriver{"mock-video"});
    runtime.HintPush(ponder::platform::hints::VideoAllowScreensaver{true});
    runtime.HintPush(ponder::platform::hints::MouseFocusClickThrough{true});

    pushOnceHintRejected = ThrowsPlatformException(
        [&runtime]()
        {
            runtime.HintPush(ponder::platform::hints::VideoAllowScreensaver{false});
        },
        PlatformErrorCode::InvalidArgument);

    try
    {
        static_cast<void>(runtime.TimeNow());
    }
    catch (const ponder::core::Exception&)
    {
        nonHintCallRejectedDuringConfiguration = true;
    }

    if (attemptReentrantRuntimeCreation)
    {
        reentrantRuntimeCreationRejected = ThrowsPlatformException(
            []()
            {
                Runtime nestedRuntime = Runtime::Create(RuntimeDesc{});
                static_cast<void>(nestedRuntime);
            },
            PlatformErrorCode::RuntimeAlreadyActive);
    }
}

void ThrowFromHintConfiguration(Runtime&)
{
    throw std::runtime_error{"synthetic hint-configuration failure"};
}

class RuntimeTests : public testing::Test
{
protected:
    MockRuntimeControl m_control;
    ScopedMockRuntimeBinding m_binding{m_control};
};

TEST_F(RuntimeTests, RejectsInvalidDescriptorsBeforeMockConstructionAndPermitsRetry)
{
    const auto expectInvalid = [this](const RuntimeDesc& desc)
    {
        ExpectPlatformException(
            [&desc]()
            {
                Runtime runtime = Runtime::Create(desc);
                static_cast<void>(runtime);
            },
            PlatformErrorCode::InvalidArgument);
        EXPECT_EQ(m_control.constructionCount, 0U);
    };

    RuntimeDesc desc;
    desc.applicationName.clear();
    expectInvalid(desc);

    desc = RuntimeDesc{};
    desc.applicationName = std::string{"po\0nder", 7};
    expectInvalid(desc);

    desc = RuntimeDesc{};
    desc.applicationName = std::string{1, static_cast<char>(0xC3)};
    expectInvalid(desc);

    desc = RuntimeDesc{};
    desc.applicationVersion = std::string{};
    expectInvalid(desc);

    desc = RuntimeDesc{};
    desc.applicationVersion = std::string{"1\0.0", 4};
    expectInvalid(desc);

    desc = RuntimeDesc{};
    desc.applicationIdentifier = std::string{};
    expectInvalid(desc);

    desc = RuntimeDesc{};
    desc.applicationIdentifier = std::string{"org.ponder\0test", 15};
    expectInvalid(desc);

    {
        Runtime runtime = Runtime::Create(RuntimeDesc{});
        EXPECT_TRUE(m_control.runtimeActive);
    }
    EXPECT_EQ(m_control.constructionCount, 1U);
    EXPECT_EQ(m_control.destructionCount, 1U);
}

TEST_F(RuntimeTests, EnforcesSingletonAndSupportsSequentialLifetimes)
{
    RuntimeDesc desc;
    desc.applicationName = "Runtime Contract Tests";
    desc.applicationVersion = "2.5.0";
    desc.applicationIdentifier = "org.ponder.runtime-tests";

    {
        Runtime first = Runtime::Create(desc);
        EXPECT_TRUE(m_control.runtimeActive);
        EXPECT_EQ(m_control.constructionCount, 1U);
        EXPECT_EQ(m_control.successfulInitializationCount, 1U);

        ExpectPlatformException(
            []()
            {
                Runtime duplicate = Runtime::Create(RuntimeDesc{});
                static_cast<void>(duplicate);
            },
            PlatformErrorCode::RuntimeAlreadyActive);
        EXPECT_EQ(m_control.constructionCount, 1U);

        ASSERT_TRUE(m_control.lastRuntimeDesc.has_value());
        EXPECT_EQ(m_control.lastRuntimeDesc->applicationName, desc.applicationName);
        EXPECT_EQ(m_control.lastRuntimeDesc->applicationVersion, desc.applicationVersion);
        EXPECT_EQ(m_control.lastRuntimeDesc->applicationIdentifier, desc.applicationIdentifier);
    }

    EXPECT_FALSE(m_control.runtimeActive);
    EXPECT_EQ(m_control.destructionCount, 1U);

    {
        Runtime second = Runtime::Create(RuntimeDesc{});
        EXPECT_TRUE(m_control.runtimeActive);
        EXPECT_EQ(m_control.constructionCount, 2U);
        EXPECT_EQ(m_control.successfulInitializationCount, 2U);
    }

    EXPECT_FALSE(m_control.runtimeActive);
    EXPECT_EQ(m_control.destructionCount, 2U);
}

TEST_F(RuntimeTests, RollsBackInitializationAndHintCallbackFailuresAndPermitsRetry)
{
    m_control.failInitialization = true;
    ExpectPlatformException(
        []()
        {
            Runtime runtime = Runtime::Create(RuntimeDesc{});
            static_cast<void>(runtime);
        },
        PlatformErrorCode::BackendFailure);

    EXPECT_FALSE(m_control.runtimeActive);
    EXPECT_EQ(m_control.constructionCount, 1U);
    EXPECT_EQ(m_control.initializationAttemptCount, 1U);
    EXPECT_EQ(m_control.successfulInitializationCount, 0U);
    EXPECT_EQ(m_control.destructionCount, 1U);

    m_control.failInitialization = false;
    {
        Runtime retry = Runtime::Create(RuntimeDesc{});
        EXPECT_TRUE(m_control.runtimeActive);
    }
    EXPECT_EQ(m_control.constructionCount, 2U);
    EXPECT_EQ(m_control.initializationAttemptCount, 2U);
    EXPECT_EQ(m_control.successfulInitializationCount, 1U);
    EXPECT_EQ(m_control.destructionCount, 2U);

    RuntimeDesc callbackFailureDesc;
    callbackFailureDesc.configureHintsBeforeInitialization = &ThrowFromHintConfiguration;
    EXPECT_THROW(
        {
            Runtime runtime = Runtime::Create(callbackFailureDesc);
            static_cast<void>(runtime);
        },
        std::runtime_error);

    EXPECT_FALSE(m_control.runtimeActive);
    EXPECT_EQ(m_control.constructionCount, 3U);
    EXPECT_EQ(m_control.initializationAttemptCount, 2U);
    EXPECT_EQ(m_control.destructionCount, 3U);

    {
        Runtime retry = Runtime::Create(RuntimeDesc{});
        EXPECT_TRUE(m_control.runtimeActive);
    }
    EXPECT_EQ(m_control.constructionCount, 4U);
    EXPECT_EQ(m_control.successfulInitializationCount, 2U);
    EXPECT_EQ(m_control.destructionCount, 4U);
}

TEST_F(RuntimeTests, ConfiguresAndMutatesHintsThroughTheFlatRuntimeContract)
{
    hintConfigurationCallCount = 0;
    attemptReentrantRuntimeCreation = true;
    reentrantRuntimeCreationRejected = false;
    nonHintCallRejectedDuringConfiguration = false;
    pushOnceHintRejected = false;

    RuntimeDesc desc;
    desc.configureHintsBeforeInitialization = &ConfigureRuntimeHints;
    Runtime runtime = Runtime::Create(desc);

    EXPECT_EQ(hintConfigurationCallCount, 1);
    EXPECT_TRUE(reentrantRuntimeCreationRejected);
    EXPECT_TRUE(nonHintCallRejectedDuringConfiguration);
    EXPECT_TRUE(pushOnceHintRejected);
    EXPECT_EQ(m_control.constructionCount, 1U);

    EXPECT_EQ(runtime.HintGet<ponder::platform::hints::VideoDriver>(), ponder::platform::hints::VideoDriver{"mock-video"});
    EXPECT_EQ(runtime.HintGet<ponder::platform::hints::VideoAllowScreensaver>(), ponder::platform::hints::VideoAllowScreensaver{true});
    EXPECT_EQ(runtime.HintGet<ponder::platform::hints::MouseFocusClickThrough>(), ponder::platform::hints::MouseFocusClickThrough{true});

    ExpectPlatformException(
        [&runtime]()
        {
            runtime.HintPop<ponder::platform::hints::VideoDriver>();
        },
        PlatformErrorCode::InvalidArgument);

    runtime.HintPush(ponder::platform::hints::MouseFocusClickThrough{false});
    EXPECT_EQ(runtime.HintGet<ponder::platform::hints::MouseFocusClickThrough>(), ponder::platform::hints::MouseFocusClickThrough{false});

    runtime.HintPop<ponder::platform::hints::MouseFocusClickThrough>();
    EXPECT_EQ(runtime.HintGet<ponder::platform::hints::MouseFocusClickThrough>(), ponder::platform::hints::MouseFocusClickThrough{true});

    runtime.HintClear<ponder::platform::hints::MouseFocusClickThrough>();
    EXPECT_EQ(runtime.HintGet<ponder::platform::hints::MouseFocusClickThrough>(), std::nullopt);
    runtime.HintClear<ponder::platform::hints::MouseFocusClickThrough>();

    ExpectPlatformException(
        [&runtime]()
        {
            runtime.HintPop<ponder::platform::hints::MouseFocusClickThrough>();
        },
        PlatformErrorCode::NotFound);
}

TEST_F(RuntimeTests, MovesOwnershipAndGuardsMovedFromRuntimeCalls)
{
    m_control.currentTime = ponder::core::Timestamp{std::chrono::nanoseconds{41}};

    Runtime original = Runtime::Create(RuntimeDesc{});
    Runtime moved = std::move(original);

    EXPECT_THROW(static_cast<void>(original.TimeNow()), ponder::core::Exception);
    EXPECT_THROW(static_cast<void>(original.ClipboardGetText()), ponder::core::Exception);
    EXPECT_THROW(static_cast<void>(original.DisplayEnumerate()), ponder::core::Exception);
    EXPECT_THROW(static_cast<void>(original.HintGet<ponder::platform::hints::MouseFocusClickThrough>()), ponder::core::Exception);

    moved = std::move(moved);
    EXPECT_EQ(moved.TimeNow(), m_control.currentTime);

    original = std::move(moved);
    EXPECT_THROW(static_cast<void>(moved.EventPoll()), ponder::core::Exception);
    EXPECT_EQ(original.TimeNow(), m_control.currentTime);
    EXPECT_EQ(m_control.destructionCount, 0U);
}

TEST_F(RuntimeTests, RejectsWrongThreadCallsBeforeForwarding)
{
    m_control.clipboardText = "owner text";
    m_control.globalMousePosition = {12.0F, 34.0F};
    Runtime runtime = Runtime::Create(RuntimeDesc{});

    std::array<bool, 9> rejected{};
    std::thread worker{[&runtime, &rejected]()
                       {
                           rejected[0] = ThrowsPlatformException(
                               [&runtime]()
                               {
                                   static_cast<void>(runtime.TimeNow());
                               },
                               PlatformErrorCode::WrongThread);
                           rejected[1] = ThrowsPlatformException(
                               [&runtime]()
                               {
                                   static_cast<void>(runtime.HintGet<ponder::platform::hints::MouseFocusClickThrough>());
                               },
                               PlatformErrorCode::WrongThread);
                           rejected[2] = ThrowsPlatformException(
                               [&runtime]()
                               {
                                   static_cast<void>(runtime.ClipboardGetText());
                               },
                               PlatformErrorCode::WrongThread);
                           rejected[3] = ThrowsPlatformException(
                               [&runtime]()
                               {
                                   static_cast<void>(runtime.DialogGetPendingCount());
                               },
                               PlatformErrorCode::WrongThread);
                           rejected[4] = ThrowsPlatformException(
                               [&runtime]()
                               {
                                   static_cast<void>(runtime.EventPoll());
                               },
                               PlatformErrorCode::WrongThread);
                           rejected[5] = ThrowsPlatformException(
                               [&runtime]()
                               {
                                   static_cast<void>(runtime.DisplayEnumerate());
                               },
                               PlatformErrorCode::WrongThread);
                           rejected[6] = ThrowsPlatformException(
                               [&runtime]()
                               {
                                   static_cast<void>(runtime.MouseGetGlobalPosition());
                               },
                               PlatformErrorCode::WrongThread);
                           rejected[7] = ThrowsPlatformException(
                               [&runtime]()
                               {
                                   static_cast<void>(runtime.UriOpenExternal("https://example.invalid/wrong"));
                               },
                               PlatformErrorCode::WrongThread);
                           rejected[8] = ThrowsPlatformException(
                               [&runtime]()
                               {
                                   static_cast<void>(runtime.ClipboardSetText("worker text"));
                               },
                               PlatformErrorCode::WrongThread);
                       }};
    worker.join();

    for (const bool callRejected : rejected)
    {
        EXPECT_TRUE(callRejected);
    }
    EXPECT_EQ(m_control.clipboardText, "owner text");
    EXPECT_TRUE(m_control.openedUris.empty());
}

TEST_F(RuntimeTests, ForwardsClipboardResultsAndReportsRecoverableFailures)
{
    m_control.clipboardText = "initial clipboard";
    Runtime runtime = Runtime::Create(RuntimeDesc{});

    const ponder::core::Result<std::string> initial = runtime.ClipboardGetText();
    ASSERT_TRUE(initial.HasValue());
    EXPECT_EQ(initial.GetValue(), "initial clipboard");

    const ponder::core::VoidResult updated = runtime.ClipboardSetText("updated clipboard");
    ASSERT_TRUE(updated.HasValue());

    const ponder::core::Result<std::string> afterUpdate = runtime.ClipboardGetText();
    ASSERT_TRUE(afterUpdate.HasValue());
    EXPECT_EQ(afterUpdate.GetValue(), "updated clipboard");
    EXPECT_EQ(m_control.clipboardText, "updated clipboard");

    const std::string embeddedNull{"invalid\0clipboard", 17};
    const ponder::core::VoidResult invalid = runtime.ClipboardSetText(embeddedNull);
    ASSERT_FALSE(invalid.HasValue());
    EXPECT_EQ(invalid.GetError().GetCode(), ponder::platform::ToErrorCode(PlatformErrorCode::InvalidArgument));
    EXPECT_EQ(m_control.clipboardText, "updated clipboard");

    m_control.clipboardGetError.emplace(ponder::platform::ToErrorCode(PlatformErrorCode::BackendFailure), "mock clipboard read failed");
    const ponder::core::Result<std::string> failedRead = runtime.ClipboardGetText();
    ASSERT_FALSE(failedRead.HasValue());
    EXPECT_EQ(failedRead.GetError().GetCode(), ponder::platform::ToErrorCode(PlatformErrorCode::BackendFailure));

    m_control.clipboardGetError.reset();
    m_control.clipboardSetError.emplace(ponder::platform::ToErrorCode(PlatformErrorCode::Unsupported), "mock clipboard write is unsupported");
    const ponder::core::VoidResult failedWrite = runtime.ClipboardSetText("not applied");
    ASSERT_FALSE(failedWrite.HasValue());
    EXPECT_EQ(failedWrite.GetError().GetCode(), ponder::platform::ToErrorCode(PlatformErrorCode::Unsupported));
    EXPECT_EQ(m_control.clipboardText, "updated clipboard");
}

TEST_F(RuntimeTests, TracksDialogPendingCompletionAndShutdown)
{
    const ponder::core::Timestamp timestamp{std::chrono::nanoseconds{7'500}};
    m_control.currentTime = timestamp;
    m_control.dialogOutcomesOnShow.emplace_back(
        ponder::platform::DialogSelection{.paths = {std::filesystem::path{"alpha.xyz"}, std::filesystem::path{"beta.xyz"}},
                                          .selectedFilterIndex = 1U});
    m_control.dialogOutcomesOnShow.emplace_back(ponder::platform::DialogCancellation{});
    m_control.dialogOutcomesOnShow.emplace_back(ponder::platform::DialogFailure{
        ponder::core::Error{ponder::platform::ToErrorCode(PlatformErrorCode::BackendFailure), "synthetic dialog failure"}});

    Runtime runtime = Runtime::Create(RuntimeDesc{});

    ponder::platform::OpenFileDialogDesc openDesc;
    openDesc.filters = {{"Coordinates", "xyz"}, {"All files", "*"}};
    openDesc.allowMultipleSelection = true;
    const ponder::platform::DialogRequestId openId = runtime.DialogShowOpenFile(openDesc);

    ponder::platform::SaveFileDialogDesc saveDesc;
    saveDesc.filters = {{"Coordinates", "xyz"}};
    const ponder::platform::DialogRequestId saveId = runtime.DialogShowSaveFile(saveDesc);

    ponder::platform::OpenFolderDialogDesc folderDesc;
    folderDesc.allowMultipleSelection = true;
    const ponder::platform::DialogRequestId folderId = runtime.DialogShowOpenFolder(folderDesc);

    EXPECT_EQ(openId, ponder::platform::DialogRequestId{1});
    EXPECT_EQ(saveId, ponder::platform::DialogRequestId{2});
    EXPECT_EQ(folderId, ponder::platform::DialogRequestId{3});
    EXPECT_TRUE(runtime.DialogHasPending());
    EXPECT_EQ(runtime.DialogGetPendingCount(), 3U);
    EXPECT_EQ(runtime.DialogGetOutstandingRequestCount(), 3U);

    const std::vector<ponder::platform::DialogRequestInfo> pending = runtime.DialogGetPending();
    ASSERT_EQ(pending.size(), 3U);
    EXPECT_EQ(pending[0], (ponder::platform::DialogRequestInfo{.id = openId,
                                                               .kind = ponder::platform::DialogKind::OpenFile,
                                                               .requestedAt = timestamp,
                                                               .parentWindowId = std::nullopt,
                                                               .filterCount = 2U,
                                                               .allowMultipleSelection = true}));
    EXPECT_EQ(pending[1].id, saveId);
    EXPECT_EQ(pending[1].kind, ponder::platform::DialogKind::SaveFile);
    EXPECT_EQ(pending[1].filterCount, 1U);
    EXPECT_FALSE(pending[1].allowMultipleSelection);
    EXPECT_EQ(pending[2].id, folderId);
    EXPECT_EQ(pending[2].kind, ponder::platform::DialogKind::OpenFolder);
    EXPECT_TRUE(pending[2].allowMultipleSelection);

    ExpectPlatformException(
        [&runtime]()
        {
            runtime.DialogShutdown();
        },
        PlatformErrorCode::InvalidArgument);

    const std::optional<ponder::platform::DialogCompletedEvent> openCompletion = runtime.DialogPollCompletion();
    ASSERT_TRUE(openCompletion.has_value());
    EXPECT_EQ(openCompletion->timestamp, timestamp);
    EXPECT_EQ(openCompletion->request, pending[0]);
    ASSERT_TRUE(std::holds_alternative<ponder::platform::DialogSelection>(openCompletion->outcome));
    EXPECT_EQ(std::get<ponder::platform::DialogSelection>(openCompletion->outcome).paths,
              (std::vector<std::filesystem::path>{"alpha.xyz", "beta.xyz"}));

    const std::optional<ponder::platform::DialogCompletedEvent> saveCompletion = runtime.DialogPollCompletion();
    ASSERT_TRUE(saveCompletion.has_value());
    EXPECT_EQ(saveCompletion->request.id, saveId);
    EXPECT_TRUE(std::holds_alternative<ponder::platform::DialogCancellation>(saveCompletion->outcome));

    const std::optional<ponder::platform::DialogCompletedEvent> folderCompletion = runtime.DialogPollCompletion();
    ASSERT_TRUE(folderCompletion.has_value());
    EXPECT_EQ(folderCompletion->request.id, folderId);
    ASSERT_TRUE(std::holds_alternative<ponder::platform::DialogFailure>(folderCompletion->outcome));
    EXPECT_EQ(std::get<ponder::platform::DialogFailure>(folderCompletion->outcome).error.GetCode(),
              ponder::platform::ToErrorCode(PlatformErrorCode::BackendFailure));

    EXPECT_FALSE(runtime.DialogPollCompletion().has_value());
    EXPECT_FALSE(runtime.DialogHasPending());
    EXPECT_EQ(runtime.DialogGetPendingCount(), 0U);
    EXPECT_EQ(runtime.DialogGetOutstandingRequestCount(), 0U);

    runtime.DialogShutdown();
    runtime.DialogShutdown();
    ExpectPlatformException(
        [&runtime]()
        {
            static_cast<void>(runtime.DialogShowOpenFolder(ponder::platform::OpenFolderDialogDesc{}));
        },
        PlatformErrorCode::InvalidArgument);
}

TEST_F(RuntimeTests, ForwardsTimeAndPollsDialogCompletionsBeforeInjectedEvents)
{
    const ponder::core::Timestamp now{std::chrono::nanoseconds{9'000}};
    const ponder::core::Timestamp quitTimestamp{std::chrono::nanoseconds{9'100}};
    m_control.currentTime = now;
    m_control.dialogOutcomesOnShow.emplace_back(ponder::platform::DialogCancellation{});
    m_control.events.emplace_back(ponder::platform::QuitRequestedEvent{quitTimestamp});

    Runtime runtime = Runtime::Create(RuntimeDesc{});
    EXPECT_EQ(runtime.TimeNow(), now);
    const ponder::platform::DialogRequestId dialogId = runtime.DialogShowOpenFolder(ponder::platform::OpenFolderDialogDesc{});

    const std::optional<ponder::platform::PlatformEvent> first = runtime.EventPoll();
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(std::holds_alternative<ponder::platform::DialogCompletedEvent>(*first));
    EXPECT_EQ(std::get<ponder::platform::DialogCompletedEvent>(*first).request.id, dialogId);

    const std::optional<ponder::platform::PlatformEvent> second = runtime.EventPoll();
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(std::holds_alternative<ponder::platform::QuitRequestedEvent>(*second));
    EXPECT_EQ(std::get<ponder::platform::QuitRequestedEvent>(*second).timestamp, quitTimestamp);

    EXPECT_FALSE(runtime.EventPoll().has_value());
    EXPECT_EQ(runtime.DialogGetOutstandingRequestCount(), 0U);
}

TEST_F(RuntimeTests, ForwardsDisplayQueriesAndReturnsNarrowNotFoundResults)
{
    const ponder::platform::DisplayInfo display{.id = ponder::platform::DisplayId{7},
                                                .name = "Mock Main Display",
                                                .bounds = {{-100, 20}, {1920, 1080}},
                                                .usableBounds = {{-100, 40}, {1920, 1040}},
                                                .refreshRateHertz = 59.94F,
                                                .orientation = ponder::platform::DisplayOrientation::Landscape,
                                                .contentScale = 1.5F};
    m_control.displays = {display};

    Runtime runtime = Runtime::Create(RuntimeDesc{});

    EXPECT_EQ(runtime.DisplayEnumerate(), (std::vector<ponder::platform::DisplayInfo>{display}));

    const ponder::core::Result<ponder::platform::DisplayInfo> found = runtime.DisplayGetInfo(display.id);
    ASSERT_TRUE(found.HasValue());
    EXPECT_EQ(found.GetValue(), display);

    const ponder::core::Result<ponder::platform::DisplayInfo> missing = runtime.DisplayGetInfo(ponder::platform::DisplayId{8});
    ASSERT_FALSE(missing.HasValue());
    EXPECT_EQ(missing.GetError().GetCode(), ponder::platform::ToErrorCode(PlatformErrorCode::NotFound));

    ExpectPlatformException(
        [&runtime]()
        {
            static_cast<void>(runtime.DisplayGetInfo(ponder::platform::DisplayId{}));
        },
        PlatformErrorCode::InvalidArgument);
}

TEST_F(RuntimeTests, ForwardsMouseAndExternalUriServices)
{
    m_control.globalMousePosition = {125.5F, -44.25F};
    Runtime runtime = Runtime::Create(RuntimeDesc{});

    const ponder::core::Result<ponder::platform::LogicalPoint> position = runtime.MouseGetGlobalPosition();
    ASSERT_TRUE(position.HasValue());
    EXPECT_EQ(position.GetValue(), m_control.globalMousePosition);

    const ponder::core::VoidResult captureEnabled = runtime.MouseSetCapture(true);
    EXPECT_TRUE(captureEnabled.HasValue());
    EXPECT_TRUE(m_control.mouseCaptured);

    runtime.MouseSetSystemCursor(ponder::platform::SystemCursorShape::Pointer);
    EXPECT_EQ(m_control.selectedCursor, ponder::platform::SystemCursorShape::Pointer);
    runtime.MouseHideCursor();
    EXPECT_FALSE(runtime.MouseIsCursorVisible());
    runtime.MouseShowCursor();
    EXPECT_TRUE(runtime.MouseIsCursorVisible());

    const ponder::core::VoidResult opened = runtime.UriOpenExternal("https://example.invalid/ponder");
    ASSERT_TRUE(opened.HasValue());
    EXPECT_EQ(m_control.openedUris, (std::vector<std::string>{"https://example.invalid/ponder"}));

    m_control.mouseCaptureError.emplace(ponder::platform::ToErrorCode(PlatformErrorCode::Unsupported), "mock capture is unavailable");
    const ponder::core::VoidResult captureRejected = runtime.MouseSetCapture(false);
    ASSERT_FALSE(captureRejected.HasValue());
    EXPECT_EQ(captureRejected.GetError().GetCode(), ponder::platform::ToErrorCode(PlatformErrorCode::Unsupported));
    EXPECT_TRUE(m_control.mouseCaptured);

    m_control.globalMouseSupported = false;
    const ponder::core::Result<ponder::platform::LogicalPoint> unsupportedPosition = runtime.MouseGetGlobalPosition();
    ASSERT_FALSE(unsupportedPosition.HasValue());
    EXPECT_EQ(unsupportedPosition.GetError().GetCode(), ponder::platform::ToErrorCode(PlatformErrorCode::Unsupported));

    m_control.externalUriError.emplace(ponder::platform::ToErrorCode(PlatformErrorCode::BackendFailure), "mock URI launch failed");
    const ponder::core::VoidResult failedUri = runtime.UriOpenExternal("https://example.invalid/failure");
    ASSERT_FALSE(failedUri.HasValue());
    EXPECT_EQ(failedUri.GetError().GetCode(), ponder::platform::ToErrorCode(PlatformErrorCode::BackendFailure));
    EXPECT_EQ(m_control.openedUris.size(), 1U);

    m_control.externalUriError.reset();
    const std::string invalidUri{"https://example.invalid/\0bad", 28};
    const ponder::core::VoidResult invalid = runtime.UriOpenExternal(invalidUri);
    ASSERT_FALSE(invalid.HasValue());
    EXPECT_EQ(invalid.GetError().GetCode(), ponder::platform::ToErrorCode(PlatformErrorCode::InvalidArgument));
    EXPECT_EQ(m_control.openedUris.size(), 1U);
}

TEST_F(RuntimeTests, CreatesAndMutatesARepresentativeWindow)
{
    m_control.windowPixelDensity = 1.75F;
    m_control.windowDisplayScale = 1.5F;

    Runtime runtime = Runtime::Create(RuntimeDesc{});
    {
        ponder::platform::WindowDesc desc;
        desc.title = "Mock Laboratory";
        desc.logicalSize = {640, 480};
        desc.visible = false;
        desc.resizable = false;
        desc.highPixelDensity = false;
        desc.minimumLogicalSize = ponder::platform::LogicalSize{320, 240};

        ponder::platform::Window original = runtime.WindowCreate(desc);
        EXPECT_EQ(m_control.windowCreationCount, 1U);
        EXPECT_EQ(m_control.liveWindowCount, 1U);
        EXPECT_EQ(original.GetId(), ponder::platform::WindowId{1});
        EXPECT_EQ(original.GetGraphicsCompatibility(), ponder::platform::WindowGraphicsCompatibility::Default);
        EXPECT_EQ(original.GetTitle(), "Mock Laboratory");
        EXPECT_EQ(original.GetLogicalSize(), (ponder::platform::LogicalSize{640, 480}));
        EXPECT_EQ(original.GetPixelSize(), (ponder::platform::PixelSize{640, 480}));
        EXPECT_FALSE(original.IsVisible());
        EXPECT_FALSE(original.IsResizable());

        ponder::platform::Window window = std::move(original);
        EXPECT_THROW(static_cast<void>(original.GetId()), ponder::core::Exception);

        bool wrongThreadRejected{};
        std::thread worker{[&window, &wrongThreadRejected]()
                           {
                               wrongThreadRejected = ThrowsPlatformException(
                                   [&window]()
                                   {
                                       static_cast<void>(window.GetTitle());
                                   },
                                   PlatformErrorCode::WrongThread);
                           }};
        worker.join();
        EXPECT_TRUE(wrongThreadRejected);

        window.SetTitle("Updated Laboratory");
        window.SetPosition({-25, 75});
        window.SetLogicalSize({800, 600});
        EXPECT_EQ(window.GetTitle(), "Updated Laboratory");
        EXPECT_EQ(window.GetPosition(), (ponder::platform::ScreenPosition{-25, 75}));
        EXPECT_EQ(window.GetLogicalSize(), (ponder::platform::LogicalSize{800, 600}));
        EXPECT_EQ(window.GetPixelSize(), (ponder::platform::PixelSize{800, 600}));

        const ponder::core::Result<ponder::platform::DisplayId> displayId = window.GetDisplayId();
        ASSERT_TRUE(displayId.HasValue());
        EXPECT_EQ(displayId.GetValue(), ponder::platform::DisplayId{1});
        EXPECT_FLOAT_EQ(window.GetPixelDensity(), 1.75F);
        EXPECT_FLOAT_EQ(window.GetDisplayScale(), 1.5F);

        const ponder::core::Result<ponder::platform::NativeWindowHandle> nativeHandle = window.GetNativeHandle();
        ASSERT_FALSE(nativeHandle.HasValue());
        EXPECT_EQ(nativeHandle.GetError().GetCode(), ponder::platform::ToErrorCode(PlatformErrorCode::InvalidArgument));

        window.Show();
        EXPECT_TRUE(window.IsVisible());
        window.Hide();
        EXPECT_FALSE(window.IsVisible());

        window.SetResizable(true);
        EXPECT_TRUE(window.IsResizable());
        window.SetAlwaysOnTop(true);
        EXPECT_TRUE(window.IsAlwaysOnTop());

        window.SetDecoration(ponder::platform::WindowDecoration::Borderless);
        EXPECT_EQ(window.GetDecoration(), ponder::platform::WindowDecoration::Borderless);
        window.SetPresentation(ponder::platform::WindowPresentation::DesktopFullscreen);
        EXPECT_EQ(window.GetPresentation(), ponder::platform::WindowPresentation::DesktopFullscreen);
        window.SetPresentation(ponder::platform::WindowPresentation::Windowed);
        EXPECT_EQ(window.GetPresentation(), ponder::platform::WindowPresentation::Windowed);

        window.Minimize();
        EXPECT_EQ(window.GetState(), ponder::platform::WindowState::Minimized);
        window.Restore();
        EXPECT_EQ(window.GetState(), ponder::platform::WindowState::Normal);
        window.Maximize();
        EXPECT_EQ(window.GetState(), ponder::platform::WindowState::Maximized);
        window.Restore();
        EXPECT_EQ(window.GetState(), ponder::platform::WindowState::Normal);

        window.StartTextInput();
        EXPECT_TRUE(window.IsTextInputActive());
        window.StopTextInput();
        EXPECT_FALSE(window.IsTextInputActive());

        window.SetMouseGrab(true);
        EXPECT_TRUE(window.IsMouseGrabbed());
        window.SetRelativeMouseMode(true);
        EXPECT_TRUE(window.IsRelativeMouseModeEnabled());
    }

    EXPECT_EQ(m_control.liveWindowCount, 0U);
    EXPECT_EQ(m_control.windowDestructionCount, 1U);
}

TEST_F(RuntimeTests, RollsBackFailedAndInvalidWindowCreation)
{
    Runtime runtime = Runtime::Create(RuntimeDesc{});

    ponder::platform::WindowDesc invalidDesc;
    invalidDesc.logicalSize = {0, 480};
    ExpectPlatformException(
        [&runtime, &invalidDesc]()
        {
            static_cast<void>(runtime.WindowCreate(invalidDesc));
        },
        PlatformErrorCode::InvalidArgument);
    EXPECT_EQ(m_control.windowCreationCount, 0U);
    EXPECT_EQ(m_control.liveWindowCount, 0U);

    m_control.failWindowCreation = true;
    ExpectPlatformException(
        [&runtime]()
        {
            static_cast<void>(runtime.WindowCreate(ponder::platform::WindowDesc{}));
        },
        PlatformErrorCode::BackendFailure);
    EXPECT_EQ(m_control.liveWindowCount, 0U);

    m_control.failWindowCreation = false;
    {
        ponder::platform::Window retry = runtime.WindowCreate(ponder::platform::WindowDesc{.visible = false});
        EXPECT_TRUE(retry.GetId().IsValid());
        EXPECT_EQ(m_control.liveWindowCount, 1U);
    }
    EXPECT_EQ(m_control.liveWindowCount, 0U);
    EXPECT_EQ(m_control.windowDestructionCount, 1U);
}
} // namespace
