#include <ponder/core/Exception.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/core/Timing.hpp>
#include <ponder/platform/Dialogs.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/Process.hpp>
#include <ponder/platform/Runtime.hpp>

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_platform_defines.h>
#include <SDL3/SDL_video.h>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace
{
constexpr char kFocusClickThroughHint[]{"SDL_MOUSE_FOCUS_CLICKTHROUGH"};
constexpr char kAutoCaptureHint[]{"SDL_MOUSE_AUTO_CAPTURE"};

[[nodiscard]] bool HasPlatformErrorPrefix(const ponder::core::Exception& exception, ponder::platform::PlatformErrorCode code)
{
    return exception.GetMessage().starts_with(std::format("Platform error [{}]: ", code));
}

void ExpectCoreTimestampWithin(ponder::core::Timestamp timestamp, ponder::core::Timestamp before, ponder::core::Timestamp after)
{
    EXPECT_GE(timestamp, before);
    EXPECT_LE(timestamp, after);
}

[[nodiscard]] SDL_Window* FindBackendWindow(std::string_view title)
{
    int windowCount{};
    SDL_Window** const windows = SDL_GetWindows(&windowCount);
    if (windows == nullptr)
    {
        return nullptr;
    }

    SDL_Window* result{};
    for (int index = 0; index < windowCount; ++index)
    {
        const char* const candidateTitle = SDL_GetWindowTitle(windows[index]);
        if (candidateTitle != nullptr && title == candidateTitle)
        {
            result = windows[index];
            break;
        }
    }

    SDL_free(windows);
    return result;
}

[[nodiscard]] std::uint32_t FindBackendWindowId(std::string_view title)
{
    SDL_Window* const window = FindBackendWindow(title);
    return window == nullptr ? 0U : SDL_GetWindowID(window);
}

#ifndef PONDER_PLATFORM_PROCESS_HELPER_PATH
#error "PONDER_PLATFORM_PROCESS_HELPER_PATH is required for integration tests."
#endif

[[nodiscard]] std::filesystem::path GetProcessHelperPath()
{
    return std::filesystem::path{PONDER_PLATFORM_PROCESS_HELPER_PATH};
}

[[nodiscard]] std::filesystem::path MakeTemporaryPath(std::string_view suffix)
{
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           (std::string{"ponder-platform-integration-"} + std::to_string(nonce) + "-" + std::string{suffix} + ".txt");
}

[[nodiscard]] std::vector<std::string> ReadLines(const std::filesystem::path& path)
{
    std::ifstream file{path, std::ios::binary};
    std::vector<std::string> lines;
    for (std::string line; std::getline(file, line);)
    {
        lines.push_back(line);
    }
    return lines;
}

class RuntimeIntegrationTests : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_EQ(SDL_WasInit(0), 0U);
        ClearTestState();
    }

    void TearDown() override
    {
        SDL_Quit();
        ClearTestState();
    }

private:
    static void ClearTestState()
    {
        static_cast<void>(SDL_ResetHint(SDL_HINT_VIDEO_DRIVER));
        static_cast<void>(SDL_ResetHint(SDL_HINT_FILE_DIALOG_DRIVER));
        static_cast<void>(SDL_ResetHint(kFocusClickThroughHint));
        static_cast<void>(SDL_ResetHint(kAutoCaptureHint));
        static_cast<void>(SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, nullptr));
        static_cast<void>(SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, nullptr));
        static_cast<void>(SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, nullptr));
        static_cast<void>(SDL_ClearError());
    }
};

TEST_F(RuntimeIntegrationTests, OwnsLiveSdlAndRestoresManagedHints)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(kFocusClickThroughHint, "prior-focus", SDL_HINT_OVERRIDE));
    ASSERT_TRUE(SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, "Prior App"));
    ASSERT_TRUE(SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, "1.0"));
    ASSERT_TRUE(SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, "org.ponder.prior"));

    {
        ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
        runtime.HintPush(ponder::platform::hints::VideoDriver{"dummy"});
        runtime.HintPush(ponder::platform::hints::MouseFocusClickThrough{true});
        runtime.HintPush(ponder::platform::hints::MouseAutoCapture{false});
        runtime.Initialize("Ponder Integration Test", std::string_view{"2.0"}, std::string_view{"org.ponder.integration"});

        EXPECT_NE(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO, 0U);
        EXPECT_NE(SDL_WasInit(SDL_INIT_EVENTS) & SDL_INIT_EVENTS, 0U);
        EXPECT_STREQ(SDL_GetCurrentVideoDriver(), "dummy");
        EXPECT_STREQ(SDL_GetHint(kFocusClickThroughHint), "1");
        EXPECT_STREQ(SDL_GetHint(kAutoCaptureHint), "0");
        EXPECT_STREQ(SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING), "Ponder Integration Test");
        EXPECT_STREQ(SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING), "2.0");
        EXPECT_STREQ(SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING), "org.ponder.integration");

        const ponder::core::Timestamp before = ponder::core::Timestamp::Now();
        const ponder::core::Timestamp timestamp = runtime.TimeNow();
        const ponder::core::Timestamp after = ponder::core::Timestamp::Now();
        ExpectCoreTimestampWithin(timestamp, before, after);
    }

    EXPECT_EQ(SDL_WasInit(0), 0U);
    EXPECT_STREQ(SDL_GetHint(kFocusClickThroughHint), "prior-focus");
    EXPECT_EQ(SDL_GetHint(kAutoCaptureHint), nullptr);
    EXPECT_EQ(SDL_GetHint(SDL_HINT_VIDEO_DRIVER), nullptr);
}

TEST_F(RuntimeIntegrationTests, WaitCanBeWokenFromAnotherThreadWithoutPublishingTheSentinel)
{
    using namespace std::chrono_literals;

    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));
    ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
    runtime.Initialize();

    std::exception_ptr wakeFailure;
    std::thread worker{[&runtime, &wakeFailure]()
                       {
                           try
                           {
                               std::this_thread::sleep_for(5ms);
                               runtime.EventWake();
                           }
                           catch (...)
                           {
                               wakeFailure = std::current_exception();
                           }
                       }};

    const std::optional<ponder::platform::PlatformEvent> event = runtime.EventWait(ponder::core::Duration{5s});
    worker.join();

    EXPECT_EQ(wakeFailure, nullptr);
    EXPECT_FALSE(event.has_value());
    EXPECT_FALSE(runtime.EventPoll().has_value());

    SDL_Event quitEvent{};
    quitEvent.type = SDL_EVENT_QUIT;
    quitEvent.quit.timestamp = 42'000;
    ASSERT_TRUE(SDL_PushEvent(&quitEvent));

    const ponder::core::Timestamp before = ponder::core::Timestamp::Now();
    const std::optional<ponder::platform::PlatformEvent> translated = runtime.EventWait(ponder::core::Duration{1s});
    const ponder::core::Timestamp after = ponder::core::Timestamp::Now();
    ASSERT_TRUE(translated.has_value());
    ASSERT_TRUE(std::holds_alternative<ponder::platform::QuitRequestedEvent>(*translated));
    ExpectCoreTimestampWithin(std::get<ponder::platform::QuitRequestedEvent>(*translated).timestamp, before, after);
}

TEST_F(RuntimeIntegrationTests, ReportsLiveSdlVideoInitializationFailure)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "ponder-driver-that-does-not-exist", SDL_HINT_OVERRIDE));

    try
    {
        ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
        runtime.Initialize();
        FAIL() << "Expected live SDL video initialization to throw";
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_TRUE(HasPlatformErrorPrefix(exception, ponder::platform::PlatformErrorCode::BackendFailure)) << exception.GetMessage();
        EXPECT_NE(exception.GetMessage().find("SDL_Init"), std::string_view::npos);
    }
    catch (...)
    {
        FAIL() << "Expected ponder::core::Exception";
    }
    EXPECT_EQ(SDL_WasInit(0), 0U);

    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));
    {
        ponder::platform::Runtime retry = ponder::platform::Runtime::Create();
        retry.Initialize();
        EXPECT_NE(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO, 0U);
    }
    EXPECT_EQ(SDL_WasInit(0), 0U);
}

TEST_F(RuntimeIntegrationTests, ConvertsLiveDummyDialogCallbackFailuresWithoutEscaping)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_FILE_DIALOG_DRIVER, "ponder-unsupported-dialog-driver", SDL_HINT_OVERRIDE));

    ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
    runtime.Initialize();

    const ponder::core::Result<ponder::platform::dialogs::DialogRequestId> firstIdResult =
        runtime.DialogShowOpenFile(ponder::platform::dialogs::OpenFileDialogDesc{});
    ASSERT_TRUE(firstIdResult.HasValue()) << firstIdResult;
    const ponder::platform::dialogs::DialogRequestId firstId = firstIdResult.GetValue();
    EXPECT_EQ(firstId, ponder::platform::dialogs::DialogRequestId{1});
    EXPECT_EQ(runtime.DialogGetOutstandingRequestCount(), 1U);

    std::optional<ponder::platform::DialogCompletedEvent> completion = runtime.DialogPollCompletion();
    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->request.id, firstId);
    const auto* failure = std::get_if<ponder::platform::DialogFailure>(&completion->outcome);
    ASSERT_NE(failure, nullptr);
    EXPECT_EQ(failure->error.GetCode(), ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(failure->error.GetMessage().find("SDL_ShowOpenFileDialog"), std::string_view::npos);
    EXPECT_EQ(runtime.DialogGetOutstandingRequestCount(), 0U);

    SDL_Event quitEvent{};
    quitEvent.type = SDL_EVENT_QUIT;
    quitEvent.quit.timestamp = 1;
    ASSERT_TRUE(SDL_PushEvent(&quitEvent));
    const std::optional<ponder::platform::PlatformEvent> eventAfterStaleDialogWake =
        runtime.EventWait(ponder::core::Duration{std::chrono::seconds{1}});
    ASSERT_TRUE(eventAfterStaleDialogWake.has_value());
    EXPECT_TRUE(std::holds_alternative<ponder::platform::QuitRequestedEvent>(*eventAfterStaleDialogWake));

    const ponder::core::Result<ponder::platform::dialogs::DialogRequestId> secondIdResult =
        runtime.DialogShowOpenFolder(ponder::platform::dialogs::OpenFolderDialogDesc{});
    ASSERT_TRUE(secondIdResult.HasValue()) << secondIdResult;
    const ponder::platform::dialogs::DialogRequestId secondId = secondIdResult.GetValue();
    EXPECT_EQ(secondId, ponder::platform::dialogs::DialogRequestId{2});

    completion = runtime.DialogPollCompletion();
    ASSERT_TRUE(completion.has_value());
    EXPECT_EQ(completion->request.id, secondId);
    EXPECT_TRUE(std::holds_alternative<ponder::platform::DialogFailure>(completion->outcome));
    EXPECT_EQ(runtime.DialogGetOutstandingRequestCount(), 0U);
    const ponder::core::VoidResult shutdown = runtime.DialogShutdown();
    EXPECT_TRUE(shutdown.HasValue()) << shutdown;
}

TEST_F(RuntimeIntegrationTests, OwnsMultipleLiveHiddenWindows)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
    runtime.Initialize();

    const ponder::platform::WindowDesc desc{.title = "Live Hidden Window",
                                            .logicalSize = {320, 240},
                                            .visible = false,
                                            .resizable = true,
                                            .highPixelDensity = true,
                                            .minimumLogicalSize = ponder::platform::LogicalSize{64, 48},
                                            .graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Default};

    std::optional<ponder::platform::Window> first;
    first.emplace(runtime.WindowCreate(desc));

    std::optional<ponder::platform::Window> second;
    second.emplace(runtime.WindowCreate(desc));

    EXPECT_TRUE(first->GetId().IsValid());
    EXPECT_TRUE(second->GetId().IsValid());
    EXPECT_NE(first->GetId(), second->GetId());
    EXPECT_EQ(first->GetTitle(), "Live Hidden Window");

    EXPECT_EQ(first->GetLogicalSize(), desc.logicalSize);
    const ponder::platform::PixelSize pixelSize = first->GetPixelSize();
    EXPECT_GT(pixelSize.width, 0U);
    EXPECT_GT(pixelSize.height, 0U);
    static_cast<void>(first->GetPosition());

    first->SetTitle("Renamed Live Window");
    EXPECT_EQ(first->GetTitle(), "Renamed Live Window");
    const ponder::platform::LogicalSize resizedLogicalSize{400, 300};
    first->SetLogicalSize(resizedLogicalSize);
    EXPECT_EQ(first->GetLogicalSize(), resizedLogicalSize);
    const ponder::platform::ScreenPosition movedPosition{25, 35};
    first->SetPosition(movedPosition);
    EXPECT_EQ(first->GetPosition(), movedPosition);

    first->Show();
    first->Show();
    EXPECT_TRUE(first->IsVisible());
    first->Hide();
    first->Hide();
    EXPECT_FALSE(first->IsVisible());

    ponder::platform::Window moved = std::move(*first);
    first.reset();
    EXPECT_TRUE(moved.GetId().IsValid());

    second.reset();
    EXPECT_EQ(moved.GetLogicalSize(), resizedLogicalSize);
}

TEST_F(RuntimeIntegrationTests, SupportsLiveTextInputAndImeArea)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
    runtime.Initialize();

    ponder::platform::WindowDesc desc;
    desc.title = "Live Text Input Window";
    desc.visible = false;
    ponder::platform::Window window = runtime.WindowCreate(desc);

    SDL_Window* const backendWindow = FindBackendWindow(desc.title);
    ASSERT_NE(backendWindow, nullptr);
    EXPECT_FALSE(window.IsTextInputActive());
    EXPECT_FALSE(SDL_TextInputActive(backendWindow));

    ASSERT_NO_THROW(window.StartTextInput());
    EXPECT_TRUE(window.IsTextInputActive());
    EXPECT_TRUE(SDL_TextInputActive(backendWindow));
    EXPECT_NO_THROW(window.StartTextInput());

    const ponder::platform::TextInputArea area{.rectangle = {.origin = {12.4F, -8.6F}, .extent = {140.2F, 22.8F}}, .cursorOffset = 19.6F};
    ASSERT_NO_THROW(window.SetTextInputArea(area));

    SDL_Rect backendArea{};
    int backendCursor{};
    ASSERT_TRUE(SDL_GetTextInputArea(backendWindow, &backendArea, &backendCursor));
    EXPECT_EQ(backendArea.x, 12);
    EXPECT_EQ(backendArea.y, -9);
    EXPECT_EQ(backendArea.w, 140);
    EXPECT_EQ(backendArea.h, 23);
    EXPECT_EQ(backendCursor, 20);

    ponder::platform::TextInputArea invalidArea = area;
    invalidArea.rectangle.extent.width = -1.0F;
    try
    {
        window.SetTextInputArea(invalidArea);
        FAIL() << "Expected an invalid text input area to throw";
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_TRUE(HasPlatformErrorPrefix(exception, ponder::platform::PlatformErrorCode::InvalidArgument)) << exception.GetMessage();
    }
    catch (...)
    {
        FAIL() << "Expected ponder::core::Exception";
    }

    ASSERT_TRUE(SDL_GetTextInputArea(backendWindow, &backendArea, &backendCursor));
    EXPECT_EQ(backendArea.x, 12);
    EXPECT_EQ(backendArea.y, -9);
    EXPECT_EQ(backendArea.w, 140);
    EXPECT_EQ(backendArea.h, 23);
    EXPECT_EQ(backendCursor, 20);

    EXPECT_NO_THROW(window.ClearTextComposition());
    ASSERT_NO_THROW(window.ClearTextInputArea());
    ASSERT_TRUE(SDL_GetTextInputArea(backendWindow, &backendArea, &backendCursor));
    EXPECT_EQ(backendArea.x, 0);
    EXPECT_EQ(backendArea.y, 0);
    EXPECT_EQ(backendArea.w, 0);
    EXPECT_EQ(backendArea.h, 0);
    EXPECT_EQ(backendCursor, 0);

    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    SDL_Event key{};
    key.key.type = SDL_EVENT_KEY_DOWN;
    key.key.timestamp = 100;
    key.key.windowID = SDL_GetWindowID(backendWindow);
    key.key.scancode = SDL_SCANCODE_Q;
    key.key.key = SDLK_A;
    key.key.mod = static_cast<SDL_Keymod>(SDL_KMOD_LCTRL | SDL_KMOD_RSHIFT);
    key.key.down = true;
    key.key.repeat = true;
    ASSERT_TRUE(SDL_PushEvent(&key));

    SDL_Event text{};
    text.text.type = SDL_EVENT_TEXT_INPUT;
    text.text.timestamp = 200;
    text.text.windowID = SDL_GetWindowID(backendWindow);
    text.text.text = "typed";
    ASSERT_TRUE(SDL_PushEvent(&text));

    SDL_Event composition{};
    composition.edit.type = SDL_EVENT_TEXT_EDITING;
    composition.edit.timestamp = 300;
    composition.edit.windowID = SDL_GetWindowID(backendWindow);
    composition.edit.text = "pending";
    composition.edit.start = 1;
    composition.edit.length = 2;
    ASSERT_TRUE(SDL_PushEvent(&composition));

    const ponder::core::Timestamp keyBefore = ponder::core::Timestamp::Now();
    auto keyEvent = runtime.EventPoll();
    const ponder::core::Timestamp keyAfter = ponder::core::Timestamp::Now();
    ASSERT_TRUE(keyEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<ponder::platform::KeyboardKeyEvent>(*keyEvent));
    const ponder::platform::KeyboardKeyEvent& keyPayload = std::get<ponder::platform::KeyboardKeyEvent>(*keyEvent);
    ExpectCoreTimestampWithin(keyPayload.timestamp, keyBefore, keyAfter);
    EXPECT_EQ(keyPayload, (ponder::platform::KeyboardKeyEvent{.timestamp = keyPayload.timestamp,
                                                              .windowId = window.GetId(),
                                                              .physicalKey = ponder::platform::PhysicalKey::Q,
                                                              .logicalKey = ponder::platform::LogicalKey::FromCharacter(U'a'),
                                                              .modifiers = ponder::platform::KeyModifiers::LeftControl |
                                                                           ponder::platform::KeyModifiers::RightShift,
                                                              .pressed = true,
                                                              .repeat = true}));

    const ponder::core::Timestamp textBefore = ponder::core::Timestamp::Now();
    auto textEvent = runtime.EventPoll();
    const ponder::core::Timestamp textAfter = ponder::core::Timestamp::Now();
    ASSERT_TRUE(textEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<ponder::platform::TextInputEvent>(*textEvent));
    const ponder::platform::TextInputEvent& textPayload = std::get<ponder::platform::TextInputEvent>(*textEvent);
    ExpectCoreTimestampWithin(textPayload.timestamp, textBefore, textAfter);
    EXPECT_EQ(textPayload, (ponder::platform::TextInputEvent{.timestamp = textPayload.timestamp, .windowId = window.GetId(), .text = "typed"}));

    const ponder::core::Timestamp compositionBefore = ponder::core::Timestamp::Now();
    auto compositionEvent = runtime.EventPoll();
    const ponder::core::Timestamp compositionAfter = ponder::core::Timestamp::Now();
    ASSERT_TRUE(compositionEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<ponder::platform::TextCompositionEvent>(*compositionEvent));
    const ponder::platform::TextCompositionEvent& compositionPayload = std::get<ponder::platform::TextCompositionEvent>(*compositionEvent);
    ExpectCoreTimestampWithin(compositionPayload.timestamp, compositionBefore, compositionAfter);
    EXPECT_EQ(compositionPayload, (ponder::platform::TextCompositionEvent{.timestamp = compositionPayload.timestamp,
                                                                          .windowId = window.GetId(),
                                                                          .text = "pending",
                                                                          .selection = ponder::platform::TextCompositionRange{1, 2}}));
    EXPECT_FALSE(runtime.EventPoll().has_value());

    ASSERT_NO_THROW(window.StopTextInput());
    EXPECT_FALSE(window.IsTextInputActive());
    EXPECT_FALSE(SDL_TextInputActive(backendWindow));
    EXPECT_NO_THROW(window.StopTextInput());
}

TEST_F(RuntimeIntegrationTests, SupportsLiveMouseStateWithoutRetainingCapture)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
    runtime.Initialize();

    ponder::platform::WindowDesc desc;
    desc.title = "Live Mouse State Window";
    desc.visible = false;
    ponder::platform::Window window = runtime.WindowCreate(desc);

    auto restoreMouseState = ponder::core::MakeScopeExit(
        [&runtime, &window]() noexcept
        {
            try
            {
                static_cast<void>(runtime.MouseSetCapture(false));
            }
            catch (...)
            {
            }
            try
            {
                window.SetRelativeMouseMode(false);
            }
            catch (...)
            {
            }
            try
            {
                window.SetMouseGrab(false);
            }
            catch (...)
            {
            }
            try
            {
                runtime.MouseShowCursor();
            }
            catch (...)
            {
            }
        });

    EXPECT_FALSE(window.IsMouseGrabbed());
    ASSERT_NO_THROW(window.SetMouseGrab(true));
    ASSERT_NO_THROW(window.SetMouseGrab(false));
    EXPECT_FALSE(window.IsMouseGrabbed());

    EXPECT_FALSE(window.IsRelativeMouseModeEnabled());
    ASSERT_NO_THROW(window.SetRelativeMouseMode(true));
    EXPECT_TRUE(window.IsRelativeMouseModeEnabled());
    ASSERT_NO_THROW(window.SetRelativeMouseMode(false));
    EXPECT_FALSE(window.IsRelativeMouseModeEnabled());

    const ponder::core::VoidResult captureResult = runtime.MouseSetCapture(true);
    ASSERT_FALSE(captureResult.HasValue());
    EXPECT_EQ(captureResult.GetError().GetCode(), ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::Unsupported));
    EXPECT_TRUE(runtime.MouseSetCapture(false).HasValue());

    const auto globalPosition = runtime.MouseGetGlobalPosition();
    ASSERT_FALSE(globalPosition.HasValue());
    EXPECT_EQ(globalPosition.GetError().GetCode(), ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::Unsupported));

    bool selectedSystemCursor{};
    try
    {
        runtime.MouseSetSystemCursor(ponder::platform::SystemCursorShape::Default);
        selectedSystemCursor = true;
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_TRUE(HasPlatformErrorPrefix(exception, ponder::platform::PlatformErrorCode::BackendFailure)) << exception.GetMessage();
    }
    catch (...)
    {
        FAIL() << "Expected system cursor selection to succeed or throw ponder::core::Exception";
    }
    if (selectedSystemCursor)
    {
        EXPECT_NO_THROW(runtime.MouseSetSystemCursor(ponder::platform::SystemCursorShape::Default));
    }
    ASSERT_NO_THROW(runtime.MouseHideCursor());
    EXPECT_FALSE(runtime.MouseIsCursorVisible());
    EXPECT_NO_THROW(runtime.MouseHideCursor());
    ASSERT_NO_THROW(runtime.MouseShowCursor());
    EXPECT_TRUE(runtime.MouseIsCursorVisible());
    EXPECT_NO_THROW(runtime.MouseShowCursor());
}

TEST_F(RuntimeIntegrationTests, LaunchesHelperProcessWithoutShell)
{
    const std::filesystem::path argumentsPath = MakeTemporaryPath("process-arguments");
    auto removeArgumentsFile = ponder::core::MakeScopeExit(
        [&argumentsPath]() noexcept
        {
            std::error_code ignored;
            static_cast<void>(std::filesystem::remove(argumentsPath, ignored));
        });

    const std::string nonAsciiArgument{"angstrom-\xC3\x85"};
    auto processResult = ponder::platform::LaunchProcess(ponder::platform::ProcessDesc{
        .executable = GetProcessHelperPath(),
        .arguments = {"--write-args", argumentsPath.string(), "--exit-code", "23", "--", "alpha beta", nonAsciiArgument}});
    ASSERT_TRUE(processResult.HasValue()) << processResult.GetError().GetMessage();
    ponder::platform::Process process = std::move(processResult).GetValue();

    auto waitResult = process.Wait();
    ASSERT_TRUE(waitResult.HasValue()) << waitResult.GetError().GetMessage();
    ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessNormalExit>(*waitResult));
    EXPECT_EQ(std::get<ponder::platform::ProcessNormalExit>(*waitResult).exitCode, 23U);
    EXPECT_EQ(ReadLines(argumentsPath), (std::vector<std::string>{"alpha beta", nonAsciiArgument}));

    auto repeatedWaitResult = process.Wait();
    ASSERT_TRUE(repeatedWaitResult.HasValue()) << repeatedWaitResult.GetError().GetMessage();
    ASSERT_TRUE(std::holds_alternative<ponder::platform::ProcessNormalExit>(*repeatedWaitResult));
    EXPECT_EQ(std::get<ponder::platform::ProcessNormalExit>(*repeatedWaitResult).exitCode, 23U);
}

TEST_F(RuntimeIntegrationTests, ReportsMissingProcessExecutableAsAResult)
{
    const std::filesystem::path missingExecutable = MakeTemporaryPath("missing-process");
    std::error_code ignored;
    static_cast<void>(std::filesystem::remove(missingExecutable, ignored));

    auto processResult = ponder::platform::LaunchProcess(ponder::platform::ProcessDesc{.executable = missingExecutable});

    ASSERT_FALSE(processResult.HasValue());
    EXPECT_EQ(processResult.GetError().GetCode(), ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::BackendFailure));
}

TEST_F(RuntimeIntegrationTests, SupportsLiveClipboardTextAndRestoresPreviousText)
{
    ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
    runtime.Initialize();
    auto previousTextResult = runtime.ClipboardGetText();
    if (!previousTextResult.HasValue())
    {
        GTEST_SKIP() << "The host clipboard is unavailable, so its previous contents cannot be preserved and restored: "
                     << previousTextResult.GetError().GetMessage();
    }
    const std::string previousText = previousTextResult.GetValue();
    auto restoreClipboard = ponder::core::MakeScopeExit(
        [&runtime, &previousText]() noexcept
        {
            try
            {
                static_cast<void>(runtime.ClipboardSetText(previousText));
            }
            catch (...)
            {
            }
        });

    const std::string invalidUtf8{"\xC3\x28"};
    const std::string embeddedNull{"ponder\0clipboard", 16};
    for (const std::string_view invalidText : {std::string_view{invalidUtf8}, std::string_view{embeddedNull}})
    {
        const ponder::core::VoidResult invalidResult = runtime.ClipboardSetText(invalidText);
        ASSERT_FALSE(invalidResult.HasValue());
        EXPECT_EQ(invalidResult.GetError().GetCode(), ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::InvalidArgument));
    }

    const ponder::core::VoidResult clearResult = runtime.ClipboardSetText({});
    ASSERT_TRUE(clearResult.HasValue()) << clearResult.GetError().GetMessage();
    auto emptyTextResult = runtime.ClipboardGetText();
    ASSERT_TRUE(emptyTextResult.HasValue()) << emptyTextResult.GetError().GetMessage();
    EXPECT_TRUE(emptyTextResult.GetValue().empty());

    constexpr std::string_view kClipboardText{"ponder platform clipboard round trip"};
    const ponder::core::VoidResult setResult = runtime.ClipboardSetText(kClipboardText);
    ASSERT_TRUE(setResult.HasValue()) << setResult.GetError().GetMessage();
    EXPECT_TRUE(SDL_HasClipboardText());

    auto textResult = runtime.ClipboardGetText();
    ASSERT_TRUE(textResult.HasValue()) << textResult.GetError().GetMessage();
    EXPECT_EQ(textResult.GetValue(), kClipboardText);

    const ponder::core::VoidResult restoreResult = runtime.ClipboardSetText(previousText);
    ASSERT_TRUE(restoreResult.HasValue()) << restoreResult.GetError().GetMessage();
    auto restoredTextResult = runtime.ClipboardGetText();
    ASSERT_TRUE(restoredTextResult.HasValue()) << restoredTextResult.GetError().GetMessage();
    ASSERT_EQ(restoredTextResult.GetValue(), previousText);
    restoreClipboard.Dismiss();
}

TEST_F(RuntimeIntegrationTests, RejectsInvalidExternalUrisWithoutLaunchingHostApplication)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
    runtime.Initialize();

    const std::string invalidUtf8{"https://ponder.dev/\xC3\x28"};
    const std::string embeddedNull{"https://ponder.dev/\0hidden", 26};
    for (const std::string_view invalidUri : {std::string_view{}, std::string_view{invalidUtf8}, std::string_view{embeddedNull}})
    {
        const ponder::core::VoidResult result = runtime.UriOpenExternal(invalidUri);
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(), ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::InvalidArgument));
    }
}

TEST_F(RuntimeIntegrationTests, PollsAndRoutesSyntheticEventsForMultipleLiveWindows)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
    runtime.Initialize();

    ponder::platform::WindowDesc firstDesc;
    firstDesc.title = "Polling Window One";
    firstDesc.visible = false;
    ponder::platform::Window first = runtime.WindowCreate(firstDesc);

    ponder::platform::WindowDesc secondDesc;
    secondDesc.title = "Polling Window Two";
    secondDesc.visible = false;
    ponder::platform::Window second = runtime.WindowCreate(secondDesc);

    const std::uint32_t firstBackendId = FindBackendWindowId(firstDesc.title);
    const std::uint32_t secondBackendId = FindBackendWindowId(secondDesc.title);
    ASSERT_NE(firstBackendId, 0U);
    ASSERT_NE(secondBackendId, 0U);
    ASSERT_NE(firstBackendId, secondBackendId);

    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    SDL_Event ignored{};
    ignored.type = SDL_EVENT_USER;
    ASSERT_TRUE(SDL_PushEvent(&ignored));

    constexpr SDL_DisplayID kUnknownBackendDisplayId{0xFFFF'FFFEU};
    {
        int displayCount{};
        SDL_DisplayID* const backendDisplayIds = SDL_GetDisplays(&displayCount);
        ASSERT_NE(backendDisplayIds, nullptr);
        auto freeBackendDisplayIds = ponder::core::MakeScopeExit(
            [backendDisplayIds]() noexcept
            {
                SDL_free(backendDisplayIds);
            });
        for (int index = 0; index < displayCount; ++index)
        {
            ASSERT_NE(backendDisplayIds[index], kUnknownBackendDisplayId);
        }
    }

    SDL_Event staleDisplay{};
    staleDisplay.display.type = SDL_EVENT_DISPLAY_MOVED;
    staleDisplay.display.displayID = kUnknownBackendDisplayId;
    ASSERT_TRUE(SDL_PushEvent(&staleDisplay));

    SDL_Event firstClose{};
    firstClose.window.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    firstClose.window.windowID = firstBackendId;
    ASSERT_TRUE(SDL_PushEvent(&firstClose));

    SDL_Event secondClose{};
    secondClose.window.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    secondClose.window.windowID = secondBackendId;
    ASSERT_TRUE(SDL_PushEvent(&secondClose));

    const std::string droppedPath{"C:/tmp/live-drop.sdf"};
    const std::string droppedText{"H2O"};
    constexpr char kDropSource[]{"synthetic-source"};

    SDL_Event droppedFile{};
    droppedFile.drop.type = SDL_EVENT_DROP_FILE;
    droppedFile.drop.windowID = firstBackendId;
    droppedFile.drop.data = droppedPath.c_str();
    droppedFile.drop.source = kDropSource;
    droppedFile.drop.x = 12.5F;
    droppedFile.drop.y = 24.25F;
    ASSERT_TRUE(SDL_PushEvent(&droppedFile));

    SDL_Event droppedTextEvent{};
    droppedTextEvent.drop.type = SDL_EVENT_DROP_TEXT;
    droppedTextEvent.drop.windowID = secondBackendId;
    droppedTextEvent.drop.data = droppedText.c_str();
    droppedTextEvent.drop.x = 4.5F;
    droppedTextEvent.drop.y = 8.25F;
    ASSERT_TRUE(SDL_PushEvent(&droppedTextEvent));

    SDL_Event quit{};
    quit.type = SDL_EVENT_QUIT;
    ASSERT_TRUE(SDL_PushEvent(&quit));

    auto firstEvent = runtime.EventPoll();
    ASSERT_TRUE(firstEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<ponder::platform::WindowCloseRequestedEvent>(*firstEvent));
    EXPECT_EQ(std::get<ponder::platform::WindowCloseRequestedEvent>(*firstEvent).windowId, first.GetId());

    auto secondEvent = runtime.EventPoll();
    ASSERT_TRUE(secondEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<ponder::platform::WindowCloseRequestedEvent>(*secondEvent));
    EXPECT_EQ(std::get<ponder::platform::WindowCloseRequestedEvent>(*secondEvent).windowId, second.GetId());

    auto dropFileEvent = runtime.EventPoll();
    ASSERT_TRUE(dropFileEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<ponder::platform::DroppedFileEvent>(*dropFileEvent));
    const ponder::platform::DroppedFileEvent& droppedFilePayload = std::get<ponder::platform::DroppedFileEvent>(*dropFileEvent);
    EXPECT_EQ(droppedFilePayload.windowId, first.GetId());
    EXPECT_EQ(droppedFilePayload.path, std::filesystem::path{"C:/tmp/live-drop.sdf"});
    EXPECT_EQ(droppedFilePayload.position, (ponder::platform::LogicalPoint{12.5F, 24.25F}));
    ASSERT_TRUE(droppedFilePayload.sourceApplication.has_value());
    EXPECT_EQ(*droppedFilePayload.sourceApplication, kDropSource);

    auto dropTextEvent = runtime.EventPoll();
    ASSERT_TRUE(dropTextEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<ponder::platform::DroppedTextEvent>(*dropTextEvent));
    const ponder::platform::DroppedTextEvent& droppedTextPayload = std::get<ponder::platform::DroppedTextEvent>(*dropTextEvent);
    EXPECT_EQ(droppedTextPayload.windowId, second.GetId());
    EXPECT_EQ(droppedTextPayload.text, droppedText);
    EXPECT_EQ(droppedTextPayload.position, (ponder::platform::LogicalPoint{4.5F, 8.25F}));
    EXPECT_FALSE(droppedTextPayload.sourceApplication.has_value());

    auto quitEvent = runtime.EventPoll();
    ASSERT_TRUE(quitEvent.has_value());
    EXPECT_TRUE(std::holds_alternative<ponder::platform::QuitRequestedEvent>(*quitEvent));
    EXPECT_FALSE(runtime.EventPoll().has_value());

    EXPECT_EQ(first.GetTitle(), firstDesc.title);
    EXPECT_EQ(second.GetTitle(), secondDesc.title);
}

TEST_F(RuntimeIntegrationTests, SupportsOrthogonalStateForALiveHiddenWindow)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
    runtime.Initialize();

    ponder::platform::WindowDesc desc;
    desc.title = "Live Hidden State Window";
    desc.visible = false;
    desc.resizable = true;
    ponder::platform::Window window = runtime.WindowCreate(desc);

    EXPECT_EQ(window.GetPresentation(), ponder::platform::WindowPresentation::Windowed);
    EXPECT_EQ(window.GetDecoration(), ponder::platform::WindowDecoration::System);
    EXPECT_EQ(window.GetState(), ponder::platform::WindowState::Normal);
    EXPECT_FALSE(window.IsVisible());
    EXPECT_TRUE(window.IsResizable());
    EXPECT_FALSE(window.IsFocused());
    EXPECT_FALSE(window.IsAlwaysOnTop());

    window.SetDecoration(ponder::platform::WindowDecoration::System);
    window.SetDecoration(ponder::platform::WindowDecoration::System);
    window.SetResizable(true);
    window.SetResizable(true);
    window.SetAlwaysOnTop(false);
    window.SetAlwaysOnTop(false);

    window.SetPresentation(ponder::platform::WindowPresentation::DesktopFullscreen);
    window.SetPresentation(ponder::platform::WindowPresentation::DesktopFullscreen);
    EXPECT_EQ(window.GetPresentation(), ponder::platform::WindowPresentation::DesktopFullscreen);
    window.SetPresentation(ponder::platform::WindowPresentation::Windowed);
    window.SetPresentation(ponder::platform::WindowPresentation::Windowed);
    EXPECT_EQ(window.GetPresentation(), ponder::platform::WindowPresentation::Windowed);

    window.Show();
    EXPECT_TRUE(window.IsVisible());
    window.Hide();
    EXPECT_FALSE(window.IsVisible());

    window.Restore();
    window.Restore();
    const auto expectPlatformError = [](ponder::platform::PlatformErrorCode expectedCode, const auto& operation)
    {
        try
        {
            operation();
            FAIL() << "Expected ponder::core::Exception";
        }
        catch (const ponder::core::Exception& exception)
        {
            EXPECT_TRUE(HasPlatformErrorPrefix(exception, expectedCode)) << exception.GetMessage();
        }
        catch (...)
        {
            FAIL() << "Expected ponder::core::Exception";
        }
    };
    expectPlatformError(ponder::platform::PlatformErrorCode::BackendFailure,
                        [&window]
                        {
                            window.Minimize();
                        });
    expectPlatformError(ponder::platform::PlatformErrorCode::BackendFailure,
                        [&window]
                        {
                            window.Maximize();
                        });
    EXPECT_EQ(window.GetState(), ponder::platform::WindowState::Normal);

    expectPlatformError(ponder::platform::PlatformErrorCode::Unsupported,
                        [&window]
                        {
                            window.SetDecoration(ponder::platform::WindowDecoration::Borderless);
                        });
    expectPlatformError(ponder::platform::PlatformErrorCode::Unsupported,
                        [&window]
                        {
                            window.SetResizable(false);
                        });
    expectPlatformError(ponder::platform::PlatformErrorCode::Unsupported,
                        [&window]
                        {
                            window.SetAlwaysOnTop(true);
                        });

    EXPECT_EQ(window.GetDecoration(), ponder::platform::WindowDecoration::System);
    EXPECT_TRUE(window.IsResizable());
    EXPECT_FALSE(window.IsAlwaysOnTop());
}

TEST_F(RuntimeIntegrationTests, ExposesLiveDisplaySnapshotsAndWindowDensity)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy,offscreen", SDL_HINT_OVERRIDE));

    std::vector<ponder::platform::DisplayInfo> ownedSnapshots;
    std::string firstDisplayName;
    {
        ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
        runtime.Initialize();

        const char* const videoDriver = SDL_GetCurrentVideoDriver();
        ASSERT_NE(videoDriver, nullptr);
        EXPECT_TRUE(std::string_view{videoDriver} == "dummy" || std::string_view{videoDriver} == "offscreen");

        ownedSnapshots = runtime.DisplayEnumerate();
        ASSERT_FALSE(ownedSnapshots.empty());

        std::unordered_set<ponder::platform::DisplayId> displayIds;
        for (const ponder::platform::DisplayInfo& display : ownedSnapshots)
        {
            EXPECT_TRUE(display.id.IsValid());
            EXPECT_TRUE(displayIds.insert(display.id).second);
            EXPECT_GT(display.bounds.extent.width, 0U);
            EXPECT_GT(display.bounds.extent.height, 0U);
            EXPECT_TRUE(std::isfinite(display.contentScale));
            EXPECT_GT(display.contentScale, 0.0F);

            if (display.refreshRateHertz.has_value())
            {
                EXPECT_TRUE(std::isfinite(*display.refreshRateHertz));
                EXPECT_GT(*display.refreshRateHertz, 0.0F);
            }

            auto infoResult = runtime.DisplayGetInfo(display.id);
            ASSERT_TRUE(infoResult.HasValue()) << infoResult.GetError().GetMessage();
            EXPECT_EQ(infoResult.GetValue(), display);
        }

        firstDisplayName = ownedSnapshots.front().name;

        const ponder::platform::WindowDesc desc{.title = "Live Display Test Window",
                                                .logicalSize = {320, 240},
                                                .visible = false,
                                                .resizable = true,
                                                .highPixelDensity = true,
                                                .minimumLogicalSize = std::nullopt,
                                                .graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Default};

        ponder::platform::Window window = runtime.WindowCreate(desc);

        const ponder::platform::LogicalSize logicalSize = window.GetLogicalSize();
        const ponder::platform::PixelSize pixelSize = window.GetPixelSize();
        EXPECT_GT(logicalSize.width, 0U);
        EXPECT_GT(logicalSize.height, 0U);
        EXPECT_GT(pixelSize.width, 0U);
        EXPECT_GT(pixelSize.height, 0U);

        const float pixelDensity = window.GetPixelDensity();
        EXPECT_TRUE(std::isfinite(pixelDensity));
        EXPECT_GT(pixelDensity, 0.0F);

        const float displayScale = window.GetDisplayScale();
        EXPECT_TRUE(std::isfinite(displayScale));
        EXPECT_GT(displayScale, 0.0F);

        auto windowDisplayResult = window.GetDisplayId();
        ASSERT_TRUE(windowDisplayResult.HasValue()) << windowDisplayResult.GetError().GetMessage();
        EXPECT_TRUE(displayIds.contains(windowDisplayResult.GetValue()));

        auto windowDisplayInfoResult = runtime.DisplayGetInfo(windowDisplayResult.GetValue());
        ASSERT_TRUE(windowDisplayInfoResult.HasValue()) << windowDisplayInfoResult.GetError().GetMessage();
        EXPECT_EQ(windowDisplayInfoResult.GetValue().id, windowDisplayResult.GetValue());
    }

    ASSERT_FALSE(ownedSnapshots.empty());
    EXPECT_EQ(ownedSnapshots.front().name, firstDisplayName);
    EXPECT_TRUE(ownedSnapshots.front().id.IsValid());
}

TEST_F(RuntimeIntegrationTests, RejectsUnavailableGraphicsCompatibilityForCurrentHost)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
    runtime.Initialize();

    ponder::platform::WindowDesc desc;
    desc.title = "Unsupported Graphics Compatibility Window";
    desc.visible = false;
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
    desc.graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Metal;
#else
    desc.graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Vulkan;
#endif

    try
    {
        static_cast<void>(runtime.WindowCreate(desc));
        FAIL() << "Expected unavailable graphics compatibility to throw";
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_TRUE(HasPlatformErrorPrefix(exception, ponder::platform::PlatformErrorCode::Unsupported)) << exception.GetMessage();
    }
    catch (...)
    {
        FAIL() << "Expected ponder::core::Exception";
    }
}

TEST_F(RuntimeIntegrationTests, ReportsExpectedNativeHandleFailuresUnderDummyDriver)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    ponder::platform::Runtime runtime = ponder::platform::Runtime::Create();
    runtime.Initialize();
    ASSERT_STREQ(SDL_GetCurrentVideoDriver(), "dummy");

    ponder::platform::WindowDesc defaultDesc;
    defaultDesc.title = "Native Handle Default Window";
    defaultDesc.visible = false;
    ponder::platform::Window defaultWindow = runtime.WindowCreate(defaultDesc);

    auto invalidResult = defaultWindow.GetNativeHandle();
    ASSERT_FALSE(invalidResult.HasValue());
    EXPECT_EQ(invalidResult.GetError().GetCode(), ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::InvalidArgument));

    ponder::platform::WindowDesc rendererDesc;
    rendererDesc.visible = false;
#if defined(SDL_PLATFORM_MACOS)
    rendererDesc.title = "Native Handle Metal Window";
    rendererDesc.graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Metal;
#else
    rendererDesc.title = "Native Handle Vulkan Window";
    rendererDesc.graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Vulkan;
#endif

    const SDL_WindowFlags rendererFlags = SDL_WINDOW_HIDDEN |
#if defined(SDL_PLATFORM_MACOS)
                                          SDL_WINDOW_METAL;
#else
                                          SDL_WINDOW_VULKAN;
#endif
    SDL_Window* const rendererProbe = SDL_CreateWindow("Native Handle Capability Probe", 100, 100, rendererFlags);
    if (rendererProbe == nullptr)
    {
        GTEST_SKIP() << "Dummy SDL driver cannot create the host renderer-compatible window: " << SDL_GetError();
    }
    SDL_DestroyWindow(rendererProbe);

    ponder::platform::Window rendererWindow = runtime.WindowCreate(rendererDesc);
    auto nativeResult = rendererWindow.GetNativeHandle();
    ASSERT_FALSE(nativeResult.HasValue());
#if defined(SDL_PLATFORM_MACOS)
    EXPECT_EQ(nativeResult.GetError().GetCode(), ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::InvalidArgument));
#else
    EXPECT_EQ(nativeResult.GetError().GetCode(), ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::Unsupported));
#endif
}
} // namespace
