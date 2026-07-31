#include <ponder/application/Application.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/platform/Dialogs.hpp>
#include <ponder/platform/Hints.hpp>
#include <ponder/platform/PlatformError.hpp>

#include <chrono>
#include <concepts>
#include <filesystem>
#include <format>
#include <gtest/gtest.h>
#include <optional>
#include <sstream>
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
namespace application = ponder::application;
namespace platform = ponder::platform;
using platform::detail::MockRuntimeControl;
using platform::detail::ScopedMockRuntimeBinding;

#ifndef PONDER_PLATFORM_PROCESS_HELPER_PATH
#error "PONDER_PLATFORM_PROCESS_HELPER_PATH must name the process test helper."
#endif

template <typename Type>
concept FormattableAndStreamable = requires(const Type& value, std::ostream& output) {
    { std::format("{}", value) } -> std::same_as<std::string>;
    { output << value } -> std::same_as<std::ostream&>;
};

static_assert(FormattableAndStreamable<application::ApplicationDesc>);
static_assert(FormattableAndStreamable<application::ApplicationErrorCode>);
static_assert(FormattableAndStreamable<application::Application>);
static_assert(FormattableAndStreamable<application::BackgroundProcessDesc>);
static_assert(FormattableAndStreamable<application::BackgroundProcessId>);
static_assert(!std::is_copy_constructible_v<application::Application>);
static_assert(!std::is_move_constructible_v<application::Application>);

template <typename Value>
[[nodiscard]] Value RequireValue(ponder::core::Result<Value> result)
{
    if (!result)
    {
        throw std::runtime_error{std::format("Expected a successful Result, received {}", result.GetError())};
    }
    return std::move(result).GetValue();
}

class DialogApiProbeApplication final : public application::Application
{
public:
    DialogApiProbeApplication() :
        Application(application::ApplicationDesc{.applicationName = "Dialog API Probe"})
    {
    }

    using Application::DialogGetPending;
    using Application::DialogGetPendingCount;
    using Application::DialogHasPending;
    using Application::DialogShowOpenFile;
    using Application::DialogShowOpenFolder;
    using Application::DialogShowSaveFile;
};

using DialogIdResult = ponder::core::Result<platform::dialogs::DialogRequestId>;
static_assert(std::same_as<
              decltype(std::declval<DialogApiProbeApplication&>().DialogShowOpenFile(std::declval<const platform::dialogs::OpenFileDialogDesc&>())),
              DialogIdResult>);
static_assert(std::same_as<
              decltype(std::declval<DialogApiProbeApplication&>().DialogShowSaveFile(std::declval<const platform::dialogs::SaveFileDialogDesc&>())),
              DialogIdResult>);
static_assert(std::same_as<decltype(std::declval<DialogApiProbeApplication&>().DialogShowOpenFolder(
                               std::declval<const platform::dialogs::OpenFolderDialogDesc&>())),
                           DialogIdResult>);
static_assert(std::same_as<decltype(std::declval<const DialogApiProbeApplication&>().DialogGetPendingCount()), ponder::core::Result<std::size_t>>);
static_assert(std::same_as<decltype(std::declval<const DialogApiProbeApplication&>().DialogHasPending()), ponder::core::Result<bool>>);
static_assert(std::same_as<decltype(std::declval<const DialogApiProbeApplication&>().DialogGetPending()),
                           ponder::core::Result<std::vector<platform::DialogRequestInfo>>>);
static_assert(noexcept(std::declval<DialogApiProbeApplication&>().DialogShowOpenFile(std::declval<const platform::dialogs::OpenFileDialogDesc&>())));
static_assert(noexcept(std::declval<DialogApiProbeApplication&>().DialogShowSaveFile(std::declval<const platform::dialogs::SaveFileDialogDesc&>())));
static_assert(
    noexcept(std::declval<DialogApiProbeApplication&>().DialogShowOpenFolder(std::declval<const platform::dialogs::OpenFolderDialogDesc&>())));
static_assert(noexcept(std::declval<const DialogApiProbeApplication&>().DialogGetPendingCount()));
static_assert(noexcept(std::declval<const DialogApiProbeApplication&>().DialogHasPending()));
static_assert(noexcept(std::declval<const DialogApiProbeApplication&>().DialogGetPending()));

class ApplicationFixture : public testing::Test
{
protected:
    MockRuntimeControl m_control;
    ScopedMockRuntimeBinding m_binding{m_control};
};

class LifecycleApplication final : public application::Application
{
public:
    LifecycleApplication(std::vector<std::string>& calls, MockRuntimeControl& control) :
        Application(application::ApplicationDesc{.applicationName = "Lifecycle Test",
                                                 .applicationVersion = "1.2.3",
                                                 .applicationIdentifier = "org.ponder.lifecycle-test"}),
        m_calls(calls),
        m_control(control)
    {
    }

    bool runtimeImplementationConstructedDuringPreInitialization{};
    bool runtimeUninitializedDuringPreInitialization{};

private:
    void PrePlatformInitialization(platform::Runtime& runtime) override
    {
        runtimeImplementationConstructedDuringPreInitialization = m_control.constructionCount == 1;
        runtimeUninitializedDuringPreInitialization = m_control.initializationAttemptCount == 0 && !m_control.runtimeActive;
        m_calls.emplace_back("pre-platform");
        runtime.HintPush(platform::hints::QuitOnLastWindowClose{false});
    }

    void OnStart() override
    {
        m_calls.emplace_back("start");
        m_windowId = WindowCreate(platform::WindowDesc{.title = "Lifecycle"}).GetId();
    }

    void OnUpdate(ponder::core::Duration) override
    {
        m_calls.emplace_back("update");
    }

    void OnRender() override
    {
        m_calls.emplace_back("render");
        WindowClose(m_windowId);
    }

    void OnStop() override
    {
        m_calls.emplace_back("stop");
    }

    std::vector<std::string>& m_calls;
    MockRuntimeControl& m_control;
    platform::WindowId m_windowId;
};

TEST_F(ApplicationFixture, OwnsRuntimeLoopWindowLifetimeAndLifecycleOrder)
{
    std::vector<std::string> calls;
    LifecycleApplication application{calls, m_control};

    EXPECT_EQ(application.Run(), 0);
    EXPECT_EQ(calls, (std::vector<std::string>{"pre-platform", "start", "update", "render", "stop"}));
    EXPECT_TRUE(application.runtimeImplementationConstructedDuringPreInitialization);
    EXPECT_TRUE(application.runtimeUninitializedDuringPreInitialization);
    EXPECT_EQ(m_control.lastApplicationName, "Lifecycle Test");
    EXPECT_EQ(m_control.lastApplicationVersion, "1.2.3");
    EXPECT_EQ(m_control.lastApplicationIdentifier, "org.ponder.lifecycle-test");
    EXPECT_EQ(m_control.windowCreationCount, 1U);
    EXPECT_EQ(m_control.windowDestructionCount, 1U);
    EXPECT_EQ(m_control.liveWindowCount, 0U);
    EXPECT_EQ(m_control.initializationAttemptCount, 1U);
    EXPECT_EQ(m_control.successfulInitializationCount, 1U);
    EXPECT_EQ(m_control.destructionCount, 1U);
    EXPECT_FALSE(application.IsRunning());
}

class RuntimeInitializationApplication final : public application::Application
{
public:
    explicit RuntimeInitializationApplication(bool throwFromPreInitialization = false) :
        Application(application::ApplicationDesc{.applicationName = "Initialization Boundary Test",
                                                 .applicationVersion = "2.0.0",
                                                 .applicationIdentifier = "org.ponder.initialization-boundary"}),
        m_throwFromPreInitialization(throwFromPreInitialization)
    {
    }

    bool preInitializationEntered{};
    bool startEntered{};

private:
    void PrePlatformInitialization(platform::Runtime& runtime) override
    {
        preInitializationEntered = true;
        runtime.HintPush(platform::hints::QuitOnLastWindowClose{false});
        if (m_throwFromPreInitialization)
        {
            throw std::runtime_error{"synthetic pre-platform initialization failure"};
        }
    }

    void OnStart() override
    {
        startEntered = true;
    }

    bool m_throwFromPreInitialization{};
};

TEST_F(ApplicationFixture, PrePlatformInitializationFailureDestroysTheUninitializedRuntime)
{
    RuntimeInitializationApplication application{true};

    EXPECT_THROW(static_cast<void>(application.Run()), std::runtime_error);
    EXPECT_TRUE(application.preInitializationEntered);
    EXPECT_FALSE(application.startEntered);
    EXPECT_EQ(m_control.constructionCount, 1U);
    EXPECT_EQ(m_control.initializationAttemptCount, 0U);
    EXPECT_EQ(m_control.successfulInitializationCount, 0U);
    EXPECT_EQ(m_control.destructionCount, 1U);
    EXPECT_FALSE(m_control.runtimeActive);
    EXPECT_FALSE(application.IsRunning());
}

TEST_F(ApplicationFixture, RuntimeInitializationFailureDestroysThePartiallyConstructedRuntime)
{
    m_control.failInitialization = true;
    RuntimeInitializationApplication application;

    EXPECT_THROW(static_cast<void>(application.Run()), ponder::core::Exception);
    EXPECT_TRUE(application.preInitializationEntered);
    EXPECT_FALSE(application.startEntered);
    EXPECT_EQ(m_control.lastApplicationName, "Initialization Boundary Test");
    EXPECT_EQ(m_control.lastApplicationVersion, "2.0.0");
    EXPECT_EQ(m_control.lastApplicationIdentifier, "org.ponder.initialization-boundary");
    EXPECT_EQ(m_control.constructionCount, 1U);
    EXPECT_EQ(m_control.initializationAttemptCount, 1U);
    EXPECT_EQ(m_control.successfulInitializationCount, 0U);
    EXPECT_EQ(m_control.destructionCount, 1U);
    EXPECT_FALSE(m_control.runtimeActive);
    EXPECT_FALSE(application.IsRunning());
}

class EventApplication final : public application::Application
{
public:
    EventApplication() :
        Application(application::ApplicationDesc{.applicationName = "Event Test"})
    {
    }

    int keyboardEventCount{};
    int closeEventCount{};
    bool windowBorrowWasLiveDuringClose{};

private:
    void OnStart() override
    {
        m_windowId = WindowCreate(platform::WindowDesc{.title = "Events"}).GetId();
    }

    void OnKeyboardKeyEvent(const platform::KeyboardKeyEvent&) override
    {
        ++keyboardEventCount;
    }

    void OnWindowCloseRequestedEvent(const platform::WindowCloseRequestedEvent& event) override
    {
        ++closeEventCount;
        windowBorrowWasLiveDuringClose = WindowFind(event.windowId) != nullptr;
    }

    platform::WindowId m_windowId;
};

TEST_F(ApplicationFixture, DispatchesTypedEventsAndInternallyClosesRequestedWindow)
{
    m_control.events.emplace_back(platform::KeyboardKeyEvent{.windowId = platform::WindowId{1},
                                                             .physicalKey = platform::PhysicalKey::A,
                                                             .logicalKey = platform::LogicalKey{},
                                                             .pressed = true});
    m_control.events.emplace_back(platform::WindowCloseRequestedEvent{.windowId = platform::WindowId{1}});

    EventApplication application;
    EXPECT_EQ(application.Run(), 0);
    EXPECT_EQ(application.keyboardEventCount, 1);
    EXPECT_EQ(application.closeEventCount, 1);
    EXPECT_TRUE(application.windowBorrowWasLiveDuringClose);
    EXPECT_EQ(m_control.windowDestructionCount, 1U);
}

class AllEventApplication final : public application::Application
{
public:
    int hookCount{};

private:
    void OnStart() override
    {
        static_cast<void>(WindowCreate(platform::WindowDesc{.title = "All Events"}));
    }

#define PONDER_TEST_EVENT_HOOK(EventType, HookName)                                                                                                  \
    void HookName(const platform::EventType&) override                                                                                               \
    {                                                                                                                                                \
        ++hookCount;                                                                                                                                 \
    }

    PONDER_TEST_EVENT_HOOK(QuitRequestedEvent, OnQuitRequestedEvent)
    PONDER_TEST_EVENT_HOOK(WindowCloseRequestedEvent, OnWindowCloseRequestedEvent)
    PONDER_TEST_EVENT_HOOK(WindowMovedEvent, OnWindowMovedEvent)
    PONDER_TEST_EVENT_HOOK(WindowLogicalSizeChangedEvent, OnWindowLogicalSizeChangedEvent)
    PONDER_TEST_EVENT_HOOK(WindowPixelSizeChangedEvent, OnWindowPixelSizeChangedEvent)
    PONDER_TEST_EVENT_HOOK(WindowFocusChangedEvent, OnWindowFocusChangedEvent)
    PONDER_TEST_EVENT_HOOK(WindowVisibilityChangedEvent, OnWindowVisibilityChangedEvent)
    PONDER_TEST_EVENT_HOOK(WindowStateChangedEvent, OnWindowStateChangedEvent)
    PONDER_TEST_EVENT_HOOK(WindowPresentationChangedEvent, OnWindowPresentationChangedEvent)
    PONDER_TEST_EVENT_HOOK(WindowDisplayChangedEvent, OnWindowDisplayChangedEvent)
    PONDER_TEST_EVENT_HOOK(WindowDisplayScaleChangedEvent, OnWindowDisplayScaleChangedEvent)
    PONDER_TEST_EVENT_HOOK(WindowPointerEnteredEvent, OnWindowPointerEnteredEvent)
    PONDER_TEST_EVENT_HOOK(WindowPointerLeftEvent, OnWindowPointerLeftEvent)
    PONDER_TEST_EVENT_HOOK(DisplayAddedEvent, OnDisplayAddedEvent)
    PONDER_TEST_EVENT_HOOK(DisplayRemovedEvent, OnDisplayRemovedEvent)
    PONDER_TEST_EVENT_HOOK(DisplayMovedEvent, OnDisplayMovedEvent)
    PONDER_TEST_EVENT_HOOK(DisplayDesktopModeChangedEvent, OnDisplayDesktopModeChangedEvent)
    PONDER_TEST_EVENT_HOOK(DisplayCurrentModeChangedEvent, OnDisplayCurrentModeChangedEvent)
    PONDER_TEST_EVENT_HOOK(DisplayOrientationChangedEvent, OnDisplayOrientationChangedEvent)
    PONDER_TEST_EVENT_HOOK(DisplayContentScaleChangedEvent, OnDisplayContentScaleChangedEvent)
    PONDER_TEST_EVENT_HOOK(DisplayUsableBoundsChangedEvent, OnDisplayUsableBoundsChangedEvent)
    PONDER_TEST_EVENT_HOOK(KeyboardKeyEvent, OnKeyboardKeyEvent)
    PONDER_TEST_EVENT_HOOK(TextInputEvent, OnTextInputEvent)
    PONDER_TEST_EVENT_HOOK(TextCompositionEvent, OnTextCompositionEvent)
    PONDER_TEST_EVENT_HOOK(MouseMotionEvent, OnMouseMotionEvent)
    PONDER_TEST_EVENT_HOOK(MouseButtonEvent, OnMouseButtonEvent)
    PONDER_TEST_EVENT_HOOK(MouseWheelEvent, OnMouseWheelEvent)
    PONDER_TEST_EVENT_HOOK(DropBeginEvent, OnDropBeginEvent)
    PONDER_TEST_EVENT_HOOK(DroppedFileEvent, OnDroppedFileEvent)
    PONDER_TEST_EVENT_HOOK(DroppedTextEvent, OnDroppedTextEvent)
    PONDER_TEST_EVENT_HOOK(DropPositionEvent, OnDropPositionEvent)
    PONDER_TEST_EVENT_HOOK(DropCompleteEvent, OnDropCompleteEvent)
    PONDER_TEST_EVENT_HOOK(DialogCompletedEvent, OnDialogCompletedEvent)

#undef PONDER_TEST_EVENT_HOOK
};

void QueueEveryNonQuitEvent(MockRuntimeControl& control)
{
    const platform::WindowId windowId{1};
    const platform::DisplayId displayId{1};
    control.events.emplace_back(platform::WindowMovedEvent{.windowId = windowId, .position = {20, 30}});
    control.events.emplace_back(platform::WindowLogicalSizeChangedEvent{.windowId = windowId, .logicalSize = {800, 600}});
    control.events.emplace_back(platform::WindowPixelSizeChangedEvent{.windowId = windowId, .pixelSize = {1600, 1200}});
    control.events.emplace_back(platform::WindowFocusChangedEvent{.windowId = windowId, .focused = true});
    control.events.emplace_back(platform::WindowVisibilityChangedEvent{.windowId = windowId, .visible = true});
    control.events.emplace_back(platform::WindowStateChangedEvent{.windowId = windowId, .state = platform::WindowState::Maximized});
    control.events.emplace_back(
        platform::WindowPresentationChangedEvent{.windowId = windowId, .presentation = platform::WindowPresentation::DesktopFullscreen});
    control.events.emplace_back(platform::WindowDisplayChangedEvent{.windowId = windowId, .displayId = displayId});
    control.events.emplace_back(platform::WindowDisplayScaleChangedEvent{.windowId = windowId});
    control.events.emplace_back(platform::WindowPointerEnteredEvent{.windowId = windowId});
    control.events.emplace_back(platform::WindowPointerLeftEvent{.windowId = windowId});
    control.events.emplace_back(platform::DisplayAddedEvent{.displayId = displayId});
    control.events.emplace_back(platform::DisplayRemovedEvent{.displayId = displayId});
    control.events.emplace_back(platform::DisplayMovedEvent{.displayId = displayId});
    control.events.emplace_back(platform::DisplayDesktopModeChangedEvent{.displayId = displayId, .extent = platform::ScreenExtent{1920, 1080}});
    control.events.emplace_back(platform::DisplayCurrentModeChangedEvent{.displayId = displayId, .extent = platform::ScreenExtent{1920, 1080}});
    control.events.emplace_back(
        platform::DisplayOrientationChangedEvent{.displayId = displayId, .orientation = platform::DisplayOrientation::Landscape});
    control.events.emplace_back(platform::DisplayContentScaleChangedEvent{.displayId = displayId});
    control.events.emplace_back(platform::DisplayUsableBoundsChangedEvent{.displayId = displayId});
    control.events.emplace_back(platform::KeyboardKeyEvent{.windowId = windowId,
                                                           .physicalKey = platform::PhysicalKey::A,
                                                           .logicalKey = platform::LogicalKey{},
                                                           .pressed = true});
    control.events.emplace_back(platform::TextInputEvent{.windowId = windowId, .text = "text"});
    control.events.emplace_back(platform::TextCompositionEvent{.windowId = windowId, .text = "composition"});
    control.events.emplace_back(platform::MouseMotionEvent{.windowId = windowId, .position = {1.0F, 2.0F}, .relativeMovement = {0.5F, -0.5F}});
    control.events.emplace_back(
        platform::MouseButtonEvent{.windowId = windowId, .position = {1.0F, 2.0F}, .button = platform::MouseButton::Left, .pressed = true});
    control.events.emplace_back(platform::MouseWheelEvent{.windowId = windowId, .position = {1.0F, 2.0F}, .vertical = 1.0F});
    control.events.emplace_back(platform::DropBeginEvent{.windowId = windowId, .sourceApplication = "source"});
    control.events.emplace_back(platform::DroppedFileEvent{.windowId = windowId, .path = "input.xyz", .position = {1.0F, 2.0F}});
    control.events.emplace_back(platform::DroppedTextEvent{.windowId = windowId, .text = "drop", .position = {1.0F, 2.0F}});
    control.events.emplace_back(platform::DropPositionEvent{.windowId = windowId, .position = {1.0F, 2.0F}});
    control.events.emplace_back(platform::DropCompleteEvent{.windowId = windowId, .position = {1.0F, 2.0F}});
    control.events.emplace_back(platform::DialogCompletedEvent{.request = platform::DialogRequestInfo{.id = platform::dialogs::DialogRequestId{1}},
                                                               .outcome = platform::DialogCancellation{}});
    control.events.emplace_back(platform::WindowCloseRequestedEvent{.windowId = windowId});
}

TEST_F(ApplicationFixture, DispatchesEveryPlatformEventAlternativeToItsTypedHook)
{
    static_assert(std::variant_size_v<platform::PlatformEvent> == 33);

    QueueEveryNonQuitEvent(m_control);
    AllEventApplication nonQuitApplication;
    EXPECT_EQ(nonQuitApplication.Run(), 0);
    EXPECT_EQ(nonQuitApplication.hookCount, 32);

    m_control.events.emplace_back(platform::QuitRequestedEvent{});
    AllEventApplication quitApplication;
    EXPECT_EQ(quitApplication.Run(), 0);
    EXPECT_EQ(quitApplication.hookCount, 1);
    EXPECT_EQ(m_control.windowCreationCount, 2U);
    EXPECT_EQ(m_control.windowDestructionCount, 2U);
}

class MultiWindowApplication final : public application::Application
{
public:
    std::size_t countAfterFirstClose{};
    std::size_t countAfterCloseAll{};
    bool shutdownWasCommitted{};

private:
    void OnStart() override
    {
        m_firstWindowId = WindowCreate(platform::WindowDesc{.title = "First"}).GetId();
        static_cast<void>(WindowCreate(platform::WindowDesc{.title = "Second"}));
    }

    void OnRender() override
    {
        WindowClose(m_firstWindowId);
        countAfterFirstClose = WindowGetCount();
        WindowCloseAll();
        countAfterCloseAll = WindowGetCount();

        try
        {
            static_cast<void>(WindowCreate(platform::WindowDesc{.title = "Too Late"}));
        }
        catch (const ponder::core::Exception&)
        {
            shutdownWasCommitted = true;
        }
    }

    platform::WindowId m_firstWindowId;
};

TEST_F(ApplicationFixture, WaitsForTheLastWindowAndWindowCloseAllCommitsShutdown)
{
    MultiWindowApplication application;

    EXPECT_EQ(application.Run(), 0);
    EXPECT_EQ(application.countAfterFirstClose, 1U);
    EXPECT_EQ(application.countAfterCloseAll, 0U);
    EXPECT_TRUE(application.shutdownWasCommitted);
    EXPECT_EQ(m_control.windowCreationCount, 2U);
    EXPECT_EQ(m_control.windowDestructionCount, 2U);
}

TEST_F(ApplicationFixture, InactiveDialogApiReturnsInvalidStateWithoutThrowing)
{
    DialogApiProbeApplication application;

    const ponder::core::Result<bool> pending = application.DialogHasPending();
    ASSERT_FALSE(pending);
    EXPECT_EQ(pending.GetError(), application::ApplicationErrorCode::InvalidState);

    const DialogIdResult submission = application.DialogShowOpenFolder(platform::dialogs::OpenFolderDialogDesc{});
    ASSERT_FALSE(submission);
    EXPECT_EQ(submission.GetError(), application::ApplicationErrorCode::InvalidState);

    ASSERT_EQ(application.Run(), 0);
    const ponder::core::Result<std::size_t> afterRun = application.DialogGetPendingCount();
    ASSERT_FALSE(afterRun);
    EXPECT_EQ(afterRun.GetError(), application::ApplicationErrorCode::InvalidState);
}

class DialogValidationApplication final : public application::Application
{
public:
    ponder::core::ErrorCode invalidParentError;
    ponder::core::ErrorCode missingParentError;
    ponder::core::ErrorCode wrongThreadError;
    std::size_t pendingCount{};
    bool hasPending{true};
    std::size_t pendingListSize{};

private:
    void OnStart() override
    {
        const platform::WindowId windowId = WindowCreate(platform::WindowDesc{.title = "Dialog Validation"}).GetId();
        pendingCount = RequireValue(DialogGetPendingCount());
        hasPending = RequireValue(DialogHasPending());
        pendingListSize = RequireValue(DialogGetPending()).size();

        const DialogIdResult invalidParent = DialogShowOpenFolder(platform::dialogs::OpenFolderDialogDesc{.parentWindowId = platform::WindowId{}});
        if (!invalidParent)
        {
            invalidParentError = invalidParent.GetError().GetCode();
        }

        const DialogIdResult missingParent = DialogShowOpenFolder(platform::dialogs::OpenFolderDialogDesc{.parentWindowId = platform::WindowId{999}});
        if (!missingParent)
        {
            missingParentError = missingParent.GetError().GetCode();
        }

        std::jthread wrongThread(
            [this]
            {
                const ponder::core::Result<bool> result = DialogHasPending();
                if (!result)
                {
                    wrongThreadError = result.GetError().GetCode();
                }
            });
        wrongThread.join();
        WindowClose(windowId);
    }
};

TEST_F(ApplicationFixture, DialogApiReturnsPreciseValidationErrorsWithoutThrowing)
{
    DialogValidationApplication application;

    EXPECT_EQ(application.Run(), 0);
    EXPECT_EQ(application.pendingCount, 0U);
    EXPECT_FALSE(application.hasPending);
    EXPECT_EQ(application.pendingListSize, 0U);
    EXPECT_EQ(application.invalidParentError, application::ToErrorCode(application::ApplicationErrorCode::InvalidArgument));
    EXPECT_EQ(application.missingParentError, application::ToErrorCode(application::ApplicationErrorCode::NotFound));
    EXPECT_EQ(application.wrongThreadError, application::ToErrorCode(application::ApplicationErrorCode::WrongThread));
}

class DialogPlatformFailureApplication final : public application::Application
{
public:
    explicit DialogPlatformFailureApplication(MockRuntimeControl& control) :
        m_control(control)
    {
    }

    std::optional<ponder::core::Error> submissionError;

private:
    void OnStart() override
    {
        const platform::WindowId windowId = WindowCreate(platform::WindowDesc{.title = "Dialog Platform Failure"}).GetId();
        m_control.dialogOperationException = std::make_exception_ptr(PONDER_EXCEPTION("synthetic platform dialog failure"));
        const DialogIdResult result = DialogShowOpenFolder(platform::dialogs::OpenFolderDialogDesc{.parentWindowId = windowId});
        m_control.dialogOperationException = nullptr;
        if (!result)
        {
            submissionError.emplace(result.GetError());
        }
        WindowClose(windowId);
    }

    MockRuntimeControl& m_control;
};

TEST_F(ApplicationFixture, DialogApiPropagatesContainedPlatformFailureWithoutThrowing)
{
    DialogPlatformFailureApplication application{m_control};

    EXPECT_EQ(application.Run(), 0);
    ASSERT_TRUE(application.submissionError.has_value());
    EXPECT_EQ(*application.submissionError, platform::PlatformErrorCode::BackendFailure);
    EXPECT_NE(application.submissionError->GetMessage().find("synthetic platform dialog failure"), std::string_view::npos);
}

class DialogShutdownApplication final : public application::Application
{
public:
    explicit DialogShutdownApplication(MockRuntimeControl& control, bool throwFromCompletion = false) :
        Application(application::ApplicationDesc{.applicationName = "Dialog Shutdown Test"}),
        m_control(control),
        m_throwFromCompletion(throwFromCompletion)
    {
    }

    bool parentWasAliveDuringCompletion{};

private:
    void OnStart() override
    {
        const platform::WindowId parentId = WindowCreate(platform::WindowDesc{.title = "Dialog Parent"}).GetId();
        static_cast<void>(RequireValue(DialogShowOpenFolder(platform::dialogs::OpenFolderDialogDesc{.parentWindowId = parentId})));
        WindowClose(parentId);
    }

    void OnDialogCompletedEvent(const platform::DialogCompletedEvent&) override
    {
        parentWasAliveDuringCompletion = m_control.liveWindowCount == 1;
        if (m_throwFromCompletion)
        {
            throw std::runtime_error{"synthetic dialog completion failure"};
        }
    }

    MockRuntimeControl& m_control;
    bool m_throwFromCompletion{};
};

TEST_F(ApplicationFixture, DefersDialogParentDestructionUntilCompletionIsPumped)
{
    m_control.dialogOutcomesOnShow.emplace_back(platform::DialogCancellation{});
    DialogShutdownApplication application{m_control};

    EXPECT_EQ(application.Run(), 0);
    EXPECT_TRUE(application.parentWasAliveDuringCompletion);
    EXPECT_EQ(m_control.windowDestructionCount, 1U);
    EXPECT_EQ(m_control.liveWindowCount, 0U);
}

TEST_F(ApplicationFixture, DialogCallbackFailureStillReleasesParentAndRuntime)
{
    m_control.dialogOutcomesOnShow.emplace_back(platform::DialogCancellation{});
    DialogShutdownApplication application{m_control, true};

    EXPECT_THROW(static_cast<void>(application.Run()), std::runtime_error);
    EXPECT_TRUE(application.parentWasAliveDuringCompletion);
    EXPECT_EQ(m_control.liveWindowCount, 0U);
    EXPECT_FALSE(m_control.runtimeActive);
}

class ClosingDialogParentApplication final : public application::Application
{
public:
    bool rejectedClosingParent{};

private:
    void OnStart() override
    {
        const platform::WindowId parentId = WindowCreate(platform::WindowDesc{.title = "Dialog Parent"}).GetId();
        const platform::WindowId otherId = WindowCreate(platform::WindowDesc{.title = "Other"}).GetId();
        static_cast<void>(RequireValue(DialogShowOpenFolder(platform::dialogs::OpenFolderDialogDesc{.parentWindowId = parentId})));
        WindowClose(parentId);

        const DialogIdResult result = DialogShowOpenFolder(platform::dialogs::OpenFolderDialogDesc{.parentWindowId = parentId});
        rejectedClosingParent = !result && result.GetError() == application::ApplicationErrorCode::InvalidState;

        WindowClose(otherId);
    }
};

TEST_F(ApplicationFixture, RejectsNewDialogForALogicallyClosingParent)
{
    m_control.dialogOutcomesOnShow.emplace_back(platform::DialogCancellation{});
    ClosingDialogParentApplication application;

    EXPECT_EQ(application.Run(), 0);
    EXPECT_TRUE(application.rejectedClosingParent);
    EXPECT_EQ(m_control.windowCreationCount, 2U);
    EXPECT_EQ(m_control.windowDestructionCount, 2U);
}

class ThrowingApplication final : public application::Application
{
public:
    bool stopped{};

private:
    void OnStart() override
    {
        static_cast<void>(WindowCreate(platform::WindowDesc{.title = "Throwing"}));
    }

    void OnRender() override
    {
        throw std::runtime_error{"synthetic render failure"};
    }

    void OnStop() override
    {
        stopped = true;
    }
};

TEST_F(ApplicationFixture, CallbackFailureStillUnwindsOwnedResourcesAndPropagates)
{
    ThrowingApplication application;
    EXPECT_THROW(static_cast<void>(application.Run()), std::runtime_error);
    EXPECT_TRUE(application.stopped);
    EXPECT_EQ(m_control.liveWindowCount, 0U);
    EXPECT_FALSE(m_control.runtimeActive);
}

TEST_F(ApplicationFixture, RejectsASecondRun)
{
    std::vector<std::string> calls;
    LifecycleApplication application{calls, m_control};
    ASSERT_EQ(application.Run(), 0);
    EXPECT_THROW(static_cast<void>(application.Run()), ponder::core::Exception);
}

class WakeApplication final : public application::Application
{
public:
    int updateCount{};

private:
    void OnStart() override
    {
        m_windowId = WindowCreate(platform::WindowDesc{.title = "Wake"}).GetId();
    }

    void OnUpdate(ponder::core::Duration) override
    {
        ++updateCount;
        if (updateCount == 2)
        {
            WindowClose(m_windowId);
        }
    }

    void OnRender() override
    {
        if (!m_worker.joinable())
        {
            m_worker = std::jthread(
                [this]
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds{10});
                    RequestUpdate();
                });
        }
    }

    void OnStop() override
    {
        if (m_worker.joinable())
        {
            m_worker.join();
        }
    }

    std::jthread m_worker;
    platform::WindowId m_windowId;
};

TEST_F(ApplicationFixture, CrossThreadUpdateRequestWakesIdleLoop)
{
    WakeApplication application;
    EXPECT_EQ(application.Run(), 0);
    EXPECT_EQ(application.updateCount, 2);
    EXPECT_EQ(m_control.liveWindowCount, 0U);
}

class ManagedProcessApplication final : public application::Application
{
public:
    ManagedProcessApplication(bool forceTermination, int sleepMilliseconds) :
        m_forceTermination(forceTermination),
        m_sleepMilliseconds(sleepMilliseconds)
    {
    }

    int completedCount{};
    int detachedCount{};

private:
    void OnStart() override
    {
        const platform::WindowId windowId = WindowCreate(platform::WindowDesc{.title = "Managed Process"}).GetId();
        application::BackgroundProcessDesc desc{
            .process = platform::ProcessDesc{.executable = std::filesystem::path{PONDER_PLATFORM_PROCESS_HELPER_PATH},
                                             .arguments = {"--sleep-ms", std::to_string(m_sleepMilliseconds), "--exit-code", "0"}},
            .forceProcessTerminationOnApplicationExit = m_forceTermination,
        };
        ponder::core::Result<application::BackgroundProcessId> launchResult = ProcessLaunch(desc);
        if (!launchResult)
        {
            throw std::runtime_error{std::format("Process launch failed: {}", launchResult.GetError())};
        }
        WindowClose(windowId);
    }

    void OnProcessCompleted(application::BackgroundProcessId, const platform::ProcessExitStatus&) override
    {
        ++completedCount;
    }

    void OnProcessDetached(application::BackgroundProcessId) override
    {
        ++detachedCount;
    }

    bool m_forceTermination{};
    int m_sleepMilliseconds{};
};

TEST_F(ApplicationFixture, DetachesManagedProcessWhenForcedExitPolicyIsDisabled)
{
    ManagedProcessApplication application{false, 250};
    EXPECT_EQ(application.Run(), 0);
    EXPECT_EQ(application.detachedCount, 1);
    EXPECT_EQ(application.completedCount, 0);
}

TEST_F(ApplicationFixture, ForceTerminatesAndWaitsForOptedInManagedProcess)
{
    ManagedProcessApplication application{true, 5'000};
    EXPECT_EQ(application.Run(), 0);
    EXPECT_EQ(application.completedCount, 1);
    EXPECT_EQ(application.detachedCount, 0);
}

TEST(ApplicationValueTests, ProvidesStableDescriptionDefaultsAndFormatting)
{
    const application::ApplicationDesc desc;
    EXPECT_EQ(desc.applicationName, "ponder");
    EXPECT_FALSE(desc.applicationVersion.has_value());
    EXPECT_FALSE(desc.applicationIdentifier.has_value());
    EXPECT_EQ(std::format("{}", desc), "application(name='ponder', version='none', identifier='none')");
    EXPECT_EQ(std::format("{}", application::BackgroundProcessDesc{}),
              "background_process(process=process(executable='', argumentCount=0), forceTerminationOnExit=false)");
    EXPECT_EQ(std::format("{}", application::ApplicationErrorCode::WrongThread), "wrong_thread");
    EXPECT_EQ(std::format("{}", application::ApplicationErrorCode::InternalFailure), "internal_failure");
    EXPECT_EQ(std::format("{}", application::BackgroundProcessId{7}), "7");
}
} // namespace
