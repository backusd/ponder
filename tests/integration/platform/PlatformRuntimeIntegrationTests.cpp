#include <ponder/core/ScopeExit.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/platform/Process.hpp>

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace
{
constexpr char kFocusClickThroughHint[]{"SDL_MOUSE_FOCUS_CLICKTHROUGH"};
constexpr char kAutoCaptureHint[]{"SDL_MOUSE_AUTO_CAPTURE"};

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
           (std::string{"ponder-platform-integration-"} + std::to_string(nonce) + "-" +
            std::string{suffix} + ".txt");
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

class PlatformRuntimeIntegrationTests : public testing::Test
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
        static_cast<void>(SDL_ResetHint(kFocusClickThroughHint));
        static_cast<void>(SDL_ResetHint(kAutoCaptureHint));
        static_cast<void>(SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, nullptr));
        static_cast<void>(
            SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, nullptr));
        static_cast<void>(
            SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, nullptr));
        static_cast<void>(SDL_ClearError());
    }
};

TEST_F(PlatformRuntimeIntegrationTests, OwnsLiveSdlAndRestoresEffectiveGlobalState)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));
    ASSERT_TRUE(SDL_SetHintWithPriority(kFocusClickThroughHint, "prior-focus", SDL_HINT_OVERRIDE));
    ASSERT_TRUE(SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, "Prior App"));
    ASSERT_TRUE(SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, "1.0"));
    ASSERT_TRUE(
        SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, "org.ponder.prior"));

    const pond::platform::PlatformRuntimeDesc desc{.applicationName = "Ponder Integration Test",
                                                   .applicationVersion = std::string{"2.0"},
                                                   .applicationIdentifier =
                                                       std::string{"org.ponder.integration"}};

    const std::uint64_t ticksBefore = SDL_GetTicksNS();
    {
        auto result = pond::platform::PlatformRuntime::Create(desc);
        ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
        pond::platform::PlatformRuntime runtime = std::move(result).GetValue();

        EXPECT_NE(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO, 0U);
        EXPECT_NE(SDL_WasInit(SDL_INIT_EVENTS) & SDL_INIT_EVENTS, 0U);
        EXPECT_STREQ(SDL_GetCurrentVideoDriver(), "dummy");
        EXPECT_STREQ(SDL_GetHint(kFocusClickThroughHint), "1");
        EXPECT_STREQ(SDL_GetHint(kAutoCaptureHint), "0");
        EXPECT_STREQ(SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING),
                     "Ponder Integration Test");
        EXPECT_STREQ(SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING), "2.0");
        EXPECT_STREQ(SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING),
                     "org.ponder.integration");

        const std::int64_t timestamp = runtime.Now().GetTimeSinceEpoch().count();
        const std::uint64_t ticksAfter = SDL_GetTicksNS();
        EXPECT_LE(static_cast<std::int64_t>(ticksBefore), timestamp);
        EXPECT_LE(timestamp, static_cast<std::int64_t>(ticksAfter));
    }

    EXPECT_EQ(SDL_WasInit(0), 0U);
    EXPECT_STREQ(SDL_GetHint(kFocusClickThroughHint), "prior-focus");
    EXPECT_EQ(SDL_GetHint(kAutoCaptureHint), nullptr);
    EXPECT_STREQ(SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING), "Prior App");
    EXPECT_STREQ(SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING), "1.0");
    EXPECT_STREQ(SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING),
                 "org.ponder.prior");
}

TEST_F(PlatformRuntimeIntegrationTests, ReportsLiveSdlVideoInitializationFailure)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "ponder-driver-that-does-not-exist",
                                        SDL_HINT_OVERRIDE));

    const auto result =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(result.GetError().GetMessage().find("SDL_Init"), std::string_view::npos);
    EXPECT_EQ(SDL_WasInit(0), 0U);
}

TEST_F(PlatformRuntimeIntegrationTests, OwnsMultipleLiveHiddenWindows)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue()) << runtimeResult.GetError().GetMessage();
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    const pond::platform::WindowDesc desc{.title = "Live Hidden Window",
                                          .logicalSize = {320, 240},
                                          .visible = false,
                                          .resizable = true,
                                          .highPixelDensity = true,
                                          .minimumLogicalSize = pond::platform::LogicalSize{64, 48},
                                          .graphicsCompatibility =
                                              pond::platform::WindowGraphicsCompatibility::Default};

    auto firstResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(firstResult.HasValue()) << firstResult.GetError().GetMessage();
    std::optional<pond::platform::Window> first;
    first.emplace(std::move(firstResult).GetValue());

    auto secondResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(secondResult.HasValue()) << secondResult.GetError().GetMessage();
    std::optional<pond::platform::Window> second;
    second.emplace(std::move(secondResult).GetValue());

    EXPECT_TRUE(first->GetId().IsValid());
    EXPECT_TRUE(second->GetId().IsValid());
    EXPECT_NE(first->GetId(), second->GetId());
    EXPECT_EQ(first->GetTitle(), "Live Hidden Window");

    auto logicalSize = first->GetLogicalSize();
    ASSERT_TRUE(logicalSize.HasValue());
    EXPECT_GT(logicalSize.GetValue().width, 0U);
    EXPECT_GT(logicalSize.GetValue().height, 0U);
    auto pixelSize = first->GetPixelSize();
    ASSERT_TRUE(pixelSize.HasValue());
    EXPECT_GT(pixelSize.GetValue().width, 0U);
    EXPECT_GT(pixelSize.GetValue().height, 0U);
    ASSERT_TRUE(first->GetPosition().HasValue());

    ASSERT_TRUE(first->SetTitle("Renamed Live Window").HasValue());
    EXPECT_EQ(first->GetTitle(), "Renamed Live Window");
    EXPECT_TRUE(first->SetLogicalSize({400, 300}).HasValue());
    EXPECT_TRUE(first->SetPosition({25, 35}).HasValue());
    EXPECT_TRUE(first->Show().HasValue());
    EXPECT_TRUE(first->Hide().HasValue());

    pond::platform::Window moved = std::move(*first);
    first.reset();
    EXPECT_TRUE(moved.GetId().IsValid());

    second.reset();
    EXPECT_TRUE(moved.GetLogicalSize().HasValue());
}

TEST_F(PlatformRuntimeIntegrationTests, SupportsLiveTextInputAndImeArea)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue()) << runtimeResult.GetError().GetMessage();
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.title = "Live Text Input Window";
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue()) << windowResult.GetError().GetMessage();
    pond::platform::Window window = std::move(windowResult).GetValue();

    SDL_Window* const backendWindow = FindBackendWindow(desc.title);
    ASSERT_NE(backendWindow, nullptr);
    EXPECT_FALSE(window.IsTextInputActive());
    EXPECT_FALSE(SDL_TextInputActive(backendWindow));

    ASSERT_TRUE(window.StartTextInput().HasValue());
    EXPECT_TRUE(window.IsTextInputActive());
    EXPECT_TRUE(SDL_TextInputActive(backendWindow));
    EXPECT_TRUE(window.StartTextInput().HasValue());

    const pond::platform::TextInputArea area{
        .rectangle = {.origin = {12.4F, -8.6F}, .extent = {140.2F, 22.8F}}, .cursorOffset = 19.6F};
    ASSERT_TRUE(window.SetTextInputArea(area).HasValue());

    SDL_Rect backendArea{};
    int backendCursor{};
    ASSERT_TRUE(SDL_GetTextInputArea(backendWindow, &backendArea, &backendCursor));
    EXPECT_EQ(backendArea.x, 12);
    EXPECT_EQ(backendArea.y, -9);
    EXPECT_EQ(backendArea.w, 140);
    EXPECT_EQ(backendArea.h, 23);
    EXPECT_EQ(backendCursor, 20);

    EXPECT_TRUE(window.ClearTextComposition().HasValue());
    ASSERT_TRUE(window.ClearTextInputArea().HasValue());
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

    auto keyEvent = runtime.PollEvent();
    ASSERT_TRUE(keyEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::KeyboardKeyEvent>(*keyEvent));
    EXPECT_EQ(std::get<pond::platform::KeyboardKeyEvent>(*keyEvent),
              (pond::platform::KeyboardKeyEvent{
                  .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{100}},
                  .windowId = window.GetId(),
                  .physicalKey = pond::platform::PhysicalKey::Q,
                  .logicalKey = pond::platform::LogicalKey::FromCharacter(U'a'),
                  .modifiers = pond::platform::KeyModifiers::LeftControl |
                               pond::platform::KeyModifiers::RightShift,
                  .pressed = true,
                  .repeat = true}));

    auto textEvent = runtime.PollEvent();
    ASSERT_TRUE(textEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::TextInputEvent>(*textEvent));
    EXPECT_EQ(std::get<pond::platform::TextInputEvent>(*textEvent),
              (pond::platform::TextInputEvent{
                  .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{200}},
                  .windowId = window.GetId(),
                  .text = "typed"}));

    auto compositionEvent = runtime.PollEvent();
    ASSERT_TRUE(compositionEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::TextCompositionEvent>(*compositionEvent));
    EXPECT_EQ(std::get<pond::platform::TextCompositionEvent>(*compositionEvent),
              (pond::platform::TextCompositionEvent{
                  .timestamp = pond::platform::PlatformTimestamp{std::chrono::nanoseconds{300}},
                  .windowId = window.GetId(),
                  .text = "pending",
                  .selection = pond::platform::TextCompositionRange{1, 2}}));
    EXPECT_FALSE(runtime.PollEvent().has_value());

    ASSERT_TRUE(window.StopTextInput().HasValue());
    EXPECT_FALSE(window.IsTextInputActive());
    EXPECT_FALSE(SDL_TextInputActive(backendWindow));
    EXPECT_TRUE(window.StopTextInput().HasValue());
}

TEST_F(PlatformRuntimeIntegrationTests, SupportsLiveMouseStateWithoutRetainingCapture)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue()) << runtimeResult.GetError().GetMessage();
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.title = "Live Mouse State Window";
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue()) << windowResult.GetError().GetMessage();
    pond::platform::Window window = std::move(windowResult).GetValue();

    auto restoreMouseState = pond::core::MakeScopeExit(
        [&runtime, &window]() noexcept
        {
            static_cast<void>(runtime.SetMouseCapture(false));
            static_cast<void>(window.SetRelativeMouseMode(false));
            static_cast<void>(window.SetMouseGrab(false));
            static_cast<void>(runtime.ShowCursor());
        });

    EXPECT_FALSE(window.IsMouseGrabbed());
    ASSERT_TRUE(window.SetMouseGrab(true).HasValue());
    ASSERT_TRUE(window.SetMouseGrab(false).HasValue());
    EXPECT_FALSE(window.IsMouseGrabbed());

    EXPECT_FALSE(window.IsRelativeMouseModeEnabled());
    ASSERT_TRUE(window.SetRelativeMouseMode(true).HasValue());
    EXPECT_TRUE(window.IsRelativeMouseModeEnabled());
    ASSERT_TRUE(window.SetRelativeMouseMode(false).HasValue());
    EXPECT_FALSE(window.IsRelativeMouseModeEnabled());

    const pond::core::VoidResult captureResult = runtime.SetMouseCapture(true);
    ASSERT_FALSE(captureResult.HasValue());
    EXPECT_EQ(captureResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));
    EXPECT_TRUE(runtime.SetMouseCapture(false).HasValue());

    const auto globalPosition = runtime.GetGlobalMousePosition();
    ASSERT_FALSE(globalPosition.HasValue());
    EXPECT_EQ(globalPosition.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));

    ASSERT_TRUE(runtime.HideCursor().HasValue());
    EXPECT_FALSE(runtime.IsCursorVisible());
    ASSERT_TRUE(runtime.ShowCursor().HasValue());
    EXPECT_TRUE(runtime.IsCursorVisible());
}

TEST_F(PlatformRuntimeIntegrationTests, LaunchesHelperProcessWithoutShell)
{
    const std::filesystem::path argumentsPath = MakeTemporaryPath("process-arguments");
    auto removeArgumentsFile = pond::core::MakeScopeExit(
        [&argumentsPath]() noexcept
        {
            std::error_code ignored;
            static_cast<void>(std::filesystem::remove(argumentsPath, ignored));
        });

    const std::string nonAsciiArgument{"angstrom-\xC3\x85"};
    auto processResult = pond::platform::LaunchProcess(pond::platform::ProcessDesc{
        .executable = GetProcessHelperPath(),
        .arguments = {"--write-args", argumentsPath.string(), "--exit-code", "23", "--",
                      "alpha beta", nonAsciiArgument}});
    ASSERT_TRUE(processResult.HasValue()) << processResult.GetError().GetMessage();
    pond::platform::Process process = std::move(processResult).GetValue();

    auto waitResult = process.Wait();
    ASSERT_TRUE(waitResult.HasValue()) << waitResult.GetError().GetMessage();
    ASSERT_TRUE(std::holds_alternative<pond::platform::ProcessNormalExit>(*waitResult));
    EXPECT_EQ(std::get<pond::platform::ProcessNormalExit>(*waitResult).exitCode, 23U);
    EXPECT_EQ(ReadLines(argumentsPath), (std::vector<std::string>{"alpha beta", nonAsciiArgument}));
}

TEST_F(PlatformRuntimeIntegrationTests, SupportsLiveClipboardTextAndRestoresPreviousText)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue()) << runtimeResult.GetError().GetMessage();

    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    auto previousTextResult = runtime.GetClipboardText();
    ASSERT_TRUE(previousTextResult.HasValue()) << previousTextResult.GetError().GetMessage();
    const std::string previousText = previousTextResult.GetValue();
    auto restoreClipboard = pond::core::MakeScopeExit(
        [&runtime, &previousText]() noexcept
        {
            static_cast<void>(runtime.SetClipboardText(previousText));
        });

    constexpr std::string_view kClipboardText{"ponder platform clipboard round trip"};
    const pond::core::VoidResult setResult = runtime.SetClipboardText(kClipboardText);
    ASSERT_TRUE(setResult.HasValue()) << setResult.GetError().GetMessage();
    EXPECT_TRUE(SDL_HasClipboardText());

    auto textResult = runtime.GetClipboardText();
    ASSERT_TRUE(textResult.HasValue()) << textResult.GetError().GetMessage();
    EXPECT_EQ(textResult.GetValue(), kClipboardText);

    const pond::core::VoidResult restoreResult = runtime.SetClipboardText(previousText);
    ASSERT_TRUE(restoreResult.HasValue()) << restoreResult.GetError().GetMessage();
    auto restoredTextResult = runtime.GetClipboardText();
    ASSERT_TRUE(restoredTextResult.HasValue()) << restoredTextResult.GetError().GetMessage();
    ASSERT_EQ(restoredTextResult.GetValue(), previousText);
    restoreClipboard.Dismiss();
}

TEST_F(PlatformRuntimeIntegrationTests, PollsAndRoutesSyntheticEventsForMultipleLiveWindows)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue()) << runtimeResult.GetError().GetMessage();
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc firstDesc;
    firstDesc.title = "Polling Window One";
    firstDesc.visible = false;
    auto firstResult = runtime.CreateWindow(firstDesc);
    ASSERT_TRUE(firstResult.HasValue()) << firstResult.GetError().GetMessage();
    pond::platform::Window first = std::move(firstResult).GetValue();

    pond::platform::WindowDesc secondDesc;
    secondDesc.title = "Polling Window Two";
    secondDesc.visible = false;
    auto secondResult = runtime.CreateWindow(secondDesc);
    ASSERT_TRUE(secondResult.HasValue()) << secondResult.GetError().GetMessage();
    pond::platform::Window second = std::move(secondResult).GetValue();

    const std::uint32_t firstBackendId = FindBackendWindowId(firstDesc.title);
    const std::uint32_t secondBackendId = FindBackendWindowId(secondDesc.title);
    ASSERT_NE(firstBackendId, 0U);
    ASSERT_NE(secondBackendId, 0U);
    ASSERT_NE(firstBackendId, secondBackendId);

    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    SDL_Event ignored{};
    ignored.type = SDL_EVENT_USER;
    ASSERT_TRUE(SDL_PushEvent(&ignored));

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

    auto firstEvent = runtime.PollEvent();
    ASSERT_TRUE(firstEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::WindowCloseRequestedEvent>(*firstEvent));
    EXPECT_EQ(std::get<pond::platform::WindowCloseRequestedEvent>(*firstEvent).windowId,
              first.GetId());

    auto secondEvent = runtime.PollEvent();
    ASSERT_TRUE(secondEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::WindowCloseRequestedEvent>(*secondEvent));
    EXPECT_EQ(std::get<pond::platform::WindowCloseRequestedEvent>(*secondEvent).windowId,
              second.GetId());

    auto dropFileEvent = runtime.PollEvent();
    ASSERT_TRUE(dropFileEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::DroppedFileEvent>(*dropFileEvent));
    const pond::platform::DroppedFileEvent& droppedFilePayload =
        std::get<pond::platform::DroppedFileEvent>(*dropFileEvent);
    EXPECT_EQ(droppedFilePayload.windowId, first.GetId());
    EXPECT_EQ(droppedFilePayload.path, std::filesystem::path{"C:/tmp/live-drop.sdf"});
    EXPECT_EQ(droppedFilePayload.position, (pond::platform::LogicalPoint{12.5F, 24.25F}));
    ASSERT_TRUE(droppedFilePayload.sourceApplication.has_value());
    EXPECT_EQ(*droppedFilePayload.sourceApplication, kDropSource);

    auto dropTextEvent = runtime.PollEvent();
    ASSERT_TRUE(dropTextEvent.has_value());
    ASSERT_TRUE(std::holds_alternative<pond::platform::DroppedTextEvent>(*dropTextEvent));
    const pond::platform::DroppedTextEvent& droppedTextPayload =
        std::get<pond::platform::DroppedTextEvent>(*dropTextEvent);
    EXPECT_EQ(droppedTextPayload.windowId, second.GetId());
    EXPECT_EQ(droppedTextPayload.text, droppedText);
    EXPECT_EQ(droppedTextPayload.position, (pond::platform::LogicalPoint{4.5F, 8.25F}));
    EXPECT_FALSE(droppedTextPayload.sourceApplication.has_value());

    auto quitEvent = runtime.PollEvent();
    ASSERT_TRUE(quitEvent.has_value());
    EXPECT_TRUE(std::holds_alternative<pond::platform::QuitRequestedEvent>(*quitEvent));
    EXPECT_FALSE(runtime.PollEvent().has_value());

    EXPECT_EQ(first.GetTitle(), firstDesc.title);
    EXPECT_EQ(second.GetTitle(), secondDesc.title);
}

TEST_F(PlatformRuntimeIntegrationTests, SupportsOrthogonalStateForALiveHiddenWindow)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue()) << runtimeResult.GetError().GetMessage();
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.title = "Live Hidden State Window";
    desc.visible = false;
    desc.resizable = true;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue()) << windowResult.GetError().GetMessage();
    pond::platform::Window window = std::move(windowResult).GetValue();

    auto presentation = window.GetPresentation();
    ASSERT_TRUE(presentation.HasValue()) << presentation.GetError().GetMessage();
    EXPECT_EQ(presentation.GetValue(), pond::platform::WindowPresentation::Windowed);
    auto decoration = window.GetDecoration();
    ASSERT_TRUE(decoration.HasValue()) << decoration.GetError().GetMessage();
    EXPECT_EQ(decoration.GetValue(), pond::platform::WindowDecoration::System);
    auto state = window.GetState();
    ASSERT_TRUE(state.HasValue()) << state.GetError().GetMessage();
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Normal);
    auto visible = window.IsVisible();
    ASSERT_TRUE(visible.HasValue()) << visible.GetError().GetMessage();
    EXPECT_FALSE(visible.GetValue());
    auto resizable = window.IsResizable();
    ASSERT_TRUE(resizable.HasValue()) << resizable.GetError().GetMessage();
    EXPECT_TRUE(resizable.GetValue());
    auto focused = window.IsFocused();
    ASSERT_TRUE(focused.HasValue()) << focused.GetError().GetMessage();
    EXPECT_FALSE(focused.GetValue());
    auto alwaysOnTop = window.IsAlwaysOnTop();
    ASSERT_TRUE(alwaysOnTop.HasValue()) << alwaysOnTop.GetError().GetMessage();
    EXPECT_FALSE(alwaysOnTop.GetValue());

    ASSERT_TRUE(
        window.SetPresentation(pond::platform::WindowPresentation::DesktopFullscreen).HasValue());
    presentation = window.GetPresentation();
    ASSERT_TRUE(presentation.HasValue()) << presentation.GetError().GetMessage();
    EXPECT_EQ(presentation.GetValue(), pond::platform::WindowPresentation::DesktopFullscreen);
    ASSERT_TRUE(window.SetPresentation(pond::platform::WindowPresentation::Windowed).HasValue());
    presentation = window.GetPresentation();
    ASSERT_TRUE(presentation.HasValue()) << presentation.GetError().GetMessage();
    EXPECT_EQ(presentation.GetValue(), pond::platform::WindowPresentation::Windowed);

    ASSERT_TRUE(window.Show().HasValue());
    visible = window.IsVisible();
    ASSERT_TRUE(visible.HasValue()) << visible.GetError().GetMessage();
    EXPECT_TRUE(visible.GetValue());
    ASSERT_TRUE(window.Hide().HasValue());
    visible = window.IsVisible();
    ASSERT_TRUE(visible.HasValue()) << visible.GetError().GetMessage();
    EXPECT_FALSE(visible.GetValue());

    ASSERT_TRUE(window.Restore().HasValue());
    const auto expectBackendFailure = [](const pond::core::VoidResult& result)
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::BackendFailure));
    };
    const auto expectUnsupported = [](const pond::core::VoidResult& result)
    {
        ASSERT_FALSE(result.HasValue());
        EXPECT_EQ(result.GetError().GetCode(),
                  pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));
    };
    expectBackendFailure(window.Minimize());
    expectBackendFailure(window.Maximize());
    state = window.GetState();
    ASSERT_TRUE(state.HasValue()) << state.GetError().GetMessage();
    EXPECT_EQ(state.GetValue(), pond::platform::WindowState::Normal);

    expectUnsupported(window.SetDecoration(pond::platform::WindowDecoration::Borderless));
    expectUnsupported(window.SetResizable(false));
    expectUnsupported(window.SetAlwaysOnTop(true));

    decoration = window.GetDecoration();
    ASSERT_TRUE(decoration.HasValue()) << decoration.GetError().GetMessage();
    EXPECT_EQ(decoration.GetValue(), pond::platform::WindowDecoration::System);
    resizable = window.IsResizable();
    ASSERT_TRUE(resizable.HasValue()) << resizable.GetError().GetMessage();
    EXPECT_TRUE(resizable.GetValue());
    alwaysOnTop = window.IsAlwaysOnTop();
    ASSERT_TRUE(alwaysOnTop.HasValue()) << alwaysOnTop.GetError().GetMessage();
    EXPECT_FALSE(alwaysOnTop.GetValue());
}

TEST_F(PlatformRuntimeIntegrationTests, ExposesLiveDisplaySnapshotsAndWindowDensity)
{
    ASSERT_TRUE(
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy,offscreen", SDL_HINT_OVERRIDE));

    std::vector<pond::platform::DisplayInfo> ownedSnapshots;
    std::string firstDisplayName;
    {
        auto runtimeResult =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(runtimeResult.HasValue()) << runtimeResult.GetError().GetMessage();
        pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

        const char* const videoDriver = SDL_GetCurrentVideoDriver();
        ASSERT_NE(videoDriver, nullptr);
        EXPECT_TRUE(std::string_view{videoDriver} == "dummy" ||
                    std::string_view{videoDriver} == "offscreen");

        auto displaysResult = runtime.EnumerateDisplays();
        ASSERT_TRUE(displaysResult.HasValue()) << displaysResult.GetError().GetMessage();
        ownedSnapshots = std::move(displaysResult).GetValue();
        ASSERT_FALSE(ownedSnapshots.empty());

        std::unordered_set<pond::platform::DisplayId> displayIds;
        for (const pond::platform::DisplayInfo& display : ownedSnapshots)
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

            auto infoResult = runtime.GetDisplayInfo(display.id);
            ASSERT_TRUE(infoResult.HasValue()) << infoResult.GetError().GetMessage();
            EXPECT_EQ(infoResult.GetValue(), display);
        }

        firstDisplayName = ownedSnapshots.front().name;

        const pond::platform::WindowDesc desc{
            .title = "Live Display Test Window",
            .logicalSize = {320, 240},
            .visible = false,
            .resizable = true,
            .highPixelDensity = true,
            .minimumLogicalSize = std::nullopt,
            .graphicsCompatibility = pond::platform::WindowGraphicsCompatibility::Default};

        auto windowResult = runtime.CreateWindow(desc);
        ASSERT_TRUE(windowResult.HasValue()) << windowResult.GetError().GetMessage();
        pond::platform::Window window = std::move(windowResult).GetValue();

        auto logicalSizeResult = window.GetLogicalSize();
        ASSERT_TRUE(logicalSizeResult.HasValue()) << logicalSizeResult.GetError().GetMessage();
        auto pixelSizeResult = window.GetPixelSize();
        ASSERT_TRUE(pixelSizeResult.HasValue()) << pixelSizeResult.GetError().GetMessage();
        EXPECT_GT(logicalSizeResult.GetValue().width, 0U);
        EXPECT_GT(logicalSizeResult.GetValue().height, 0U);
        EXPECT_GT(pixelSizeResult.GetValue().width, 0U);
        EXPECT_GT(pixelSizeResult.GetValue().height, 0U);

        auto pixelDensityResult = window.GetPixelDensity();
        ASSERT_TRUE(pixelDensityResult.HasValue()) << pixelDensityResult.GetError().GetMessage();
        EXPECT_TRUE(std::isfinite(pixelDensityResult.GetValue()));
        EXPECT_GT(pixelDensityResult.GetValue(), 0.0F);

        auto displayScaleResult = window.GetDisplayScale();
        ASSERT_TRUE(displayScaleResult.HasValue()) << displayScaleResult.GetError().GetMessage();
        EXPECT_TRUE(std::isfinite(displayScaleResult.GetValue()));
        EXPECT_GT(displayScaleResult.GetValue(), 0.0F);

        auto windowDisplayResult = window.GetDisplayId();
        ASSERT_TRUE(windowDisplayResult.HasValue()) << windowDisplayResult.GetError().GetMessage();
        EXPECT_TRUE(displayIds.contains(windowDisplayResult.GetValue()));

        auto windowDisplayInfoResult = runtime.GetDisplayInfo(windowDisplayResult.GetValue());
        ASSERT_TRUE(windowDisplayInfoResult.HasValue())
            << windowDisplayInfoResult.GetError().GetMessage();
        EXPECT_EQ(windowDisplayInfoResult.GetValue().id, windowDisplayResult.GetValue());
    }

    ASSERT_FALSE(ownedSnapshots.empty());
    EXPECT_EQ(ownedSnapshots.front().name, firstDisplayName);
    EXPECT_TRUE(ownedSnapshots.front().id.IsValid());
}

TEST_F(PlatformRuntimeIntegrationTests, ReportsNativeHandleUnsupportedUnderDummyDriver)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue()) << runtimeResult.GetError().GetMessage();
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    ASSERT_STREQ(SDL_GetCurrentVideoDriver(), "dummy");

    pond::platform::WindowDesc defaultDesc;
    defaultDesc.title = "Native Handle Default Window";
    defaultDesc.visible = false;
    auto defaultWindowResult = runtime.CreateWindow(defaultDesc);
    ASSERT_TRUE(defaultWindowResult.HasValue()) << defaultWindowResult.GetError().GetMessage();
    pond::platform::Window defaultWindow = std::move(defaultWindowResult).GetValue();

    auto invalidResult = defaultWindow.GetNativeHandle();
    ASSERT_FALSE(invalidResult.HasValue());
    EXPECT_EQ(invalidResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::InvalidArgument));

    pond::platform::WindowDesc vulkanDesc;
    vulkanDesc.title = "Native Handle Vulkan Window";
    vulkanDesc.visible = false;
    vulkanDesc.graphicsCompatibility = pond::platform::WindowGraphicsCompatibility::Vulkan;
    auto vulkanWindowResult = runtime.CreateWindow(vulkanDesc);
    if (!vulkanWindowResult.HasValue())
    {
        GTEST_SKIP() << "Dummy SDL driver cannot create a Vulkan-compatible "
                        "window on this host: "
                     << vulkanWindowResult.GetError().GetMessage();
    }
    pond::platform::Window vulkanWindow = std::move(vulkanWindowResult).GetValue();

    auto unsupportedResult = vulkanWindow.GetNativeHandle();
    ASSERT_FALSE(unsupportedResult.HasValue());
    EXPECT_EQ(unsupportedResult.GetError().GetCode(),
              pond::platform::ToErrorCode(pond::platform::PlatformErrorCode::Unsupported));
}
} // namespace
