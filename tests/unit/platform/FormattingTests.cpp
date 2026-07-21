#include <ponder/platform/Dialog.hpp>
#include <ponder/platform/Display.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/Keyboard.hpp>
#include <ponder/platform/Mouse.hpp>
#include <ponder/platform/NativeWindow.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformEvent.hpp>
#include <ponder/platform/PlatformRuntime.hpp>
#include <ponder/platform/Process.hpp>
#include <ponder/platform/TextInput.hpp>
#include <ponder/platform/Timing.hpp>
#include <ponder/platform/Window.hpp>
#include <ponder/platform/WindowGraphics.hpp>
#include <ponder/platform/WindowState.hpp>

#include <concepts>
#include <filesystem>
#include <format>
#include <gtest/gtest.h>
#include <ostream>
#include <sstream>
#include <string>

namespace
{
template <typename Type>
concept FormattableAndStreamable =
    std::formattable<Type, char> && requires(std::ostream& output, const Type& value) {
        { output << value } -> std::same_as<std::ostream&>;
    };


using namespace pond::platform;

static_assert(FormattableAndStreamable<DialogFileFilter>);
static_assert(FormattableAndStreamable<OpenFileDialogDesc>);
static_assert(FormattableAndStreamable<SaveFileDialogDesc>);
static_assert(FormattableAndStreamable<OpenFolderDialogDesc>);
static_assert(FormattableAndStreamable<DialogSelection>);
static_assert(FormattableAndStreamable<DialogCancellation>);
static_assert(FormattableAndStreamable<DialogFailure>);
static_assert(FormattableAndStreamable<DisplayOrientation>);
static_assert(FormattableAndStreamable<DisplayInfo>);
static_assert(FormattableAndStreamable<ScreenPosition>);
static_assert(FormattableAndStreamable<ScreenExtent>);
static_assert(FormattableAndStreamable<ScreenRectangle>);
static_assert(FormattableAndStreamable<LogicalPoint>);
static_assert(FormattableAndStreamable<LogicalExtent>);
static_assert(FormattableAndStreamable<LogicalRectangle>);
static_assert(FormattableAndStreamable<LogicalSize>);
static_assert(FormattableAndStreamable<PixelSize>);
static_assert(FormattableAndStreamable<WindowId>);
static_assert(FormattableAndStreamable<DisplayId>);
static_assert(FormattableAndStreamable<DialogRequestId>);
static_assert(FormattableAndStreamable<PhysicalKey>);
static_assert(FormattableAndStreamable<NamedKey>);
static_assert(FormattableAndStreamable<LogicalKey::Kind>);
static_assert(FormattableAndStreamable<LogicalKey>);
static_assert(FormattableAndStreamable<KeyModifiers>);
static_assert(FormattableAndStreamable<MousePosition>);
static_assert(FormattableAndStreamable<MouseButton>);
static_assert(FormattableAndStreamable<SystemCursorShape>);
static_assert(FormattableAndStreamable<NativeWin32Window>);
static_assert(FormattableAndStreamable<NativeX11Window>);
static_assert(FormattableAndStreamable<NativeWaylandWindow>);
static_assert(FormattableAndStreamable<PlatformErrorCode>);
static_assert(FormattableAndStreamable<PlatformRuntimeDesc>);
static_assert(FormattableAndStreamable<ProcessDesc>);
static_assert(FormattableAndStreamable<ProcessNormalExit>);
static_assert(FormattableAndStreamable<ProcessSignalTermination>);
static_assert(FormattableAndStreamable<ProcessUnknownTermination>);
static_assert(FormattableAndStreamable<ProcessTerminationMode>);
static_assert(FormattableAndStreamable<TextCompositionRange>);
static_assert(FormattableAndStreamable<TextInputArea>);
static_assert(FormattableAndStreamable<Duration>);
static_assert(FormattableAndStreamable<Timestamp>);
static_assert(FormattableAndStreamable<WindowDesc>);
static_assert(FormattableAndStreamable<WindowGraphicsCompatibility>);
static_assert(FormattableAndStreamable<WindowPresentation>);
static_assert(FormattableAndStreamable<WindowDecoration>);
static_assert(FormattableAndStreamable<WindowState>);

static_assert(FormattableAndStreamable<QuitRequestedEvent>);
static_assert(FormattableAndStreamable<WindowCloseRequestedEvent>);
static_assert(FormattableAndStreamable<WindowMovedEvent>);
static_assert(FormattableAndStreamable<WindowLogicalSizeChangedEvent>);
static_assert(FormattableAndStreamable<WindowPixelSizeChangedEvent>);
static_assert(FormattableAndStreamable<WindowFocusChangedEvent>);
static_assert(FormattableAndStreamable<WindowVisibilityChangedEvent>);
static_assert(FormattableAndStreamable<WindowStateChangedEvent>);
static_assert(FormattableAndStreamable<WindowPresentationChangedEvent>);
static_assert(FormattableAndStreamable<WindowDisplayChangedEvent>);
static_assert(FormattableAndStreamable<WindowDisplayScaleChangedEvent>);
static_assert(FormattableAndStreamable<WindowPointerEnteredEvent>);
static_assert(FormattableAndStreamable<WindowPointerLeftEvent>);
static_assert(FormattableAndStreamable<DisplayAddedEvent>);
static_assert(FormattableAndStreamable<DisplayRemovedEvent>);
static_assert(FormattableAndStreamable<DisplayMovedEvent>);
static_assert(FormattableAndStreamable<DisplayDesktopModeChangedEvent>);
static_assert(FormattableAndStreamable<DisplayCurrentModeChangedEvent>);
static_assert(FormattableAndStreamable<DisplayOrientationChangedEvent>);
static_assert(FormattableAndStreamable<DisplayContentScaleChangedEvent>);
static_assert(FormattableAndStreamable<DisplayUsableBoundsChangedEvent>);
static_assert(FormattableAndStreamable<KeyboardKeyEvent>);
static_assert(FormattableAndStreamable<TextInputEvent>);
static_assert(FormattableAndStreamable<TextCompositionEvent>);
static_assert(FormattableAndStreamable<MouseMotionEvent>);
static_assert(FormattableAndStreamable<MouseButtonEvent>);
static_assert(FormattableAndStreamable<MouseWheelEvent>);
static_assert(FormattableAndStreamable<DropBeginEvent>);
static_assert(FormattableAndStreamable<DroppedFileEvent>);
static_assert(FormattableAndStreamable<DroppedTextEvent>);
static_assert(FormattableAndStreamable<DropPositionEvent>);
static_assert(FormattableAndStreamable<DropCompleteEvent>);
static_assert(FormattableAndStreamable<DialogCompletedEvent>);

template <FormattableAndStreamable Type>
void ExpectStreamMatchesFormat(const Type& value)
{
    std::ostringstream output;
    output << value;
    EXPECT_EQ(output.str(), std::format("{}", value));
}

TEST(PlatformFormattingTests, StreamsRepresentativeValuesUsingTheirFormatters)
{
    ExpectStreamMatchesFormat(MouseButton::Left);
    ExpectStreamMatchesFormat(DisplayInfo{});
    ExpectStreamMatchesFormat(WindowDesc{});
    ExpectStreamMatchesFormat(QuitRequestedEvent{});
    ExpectStreamMatchesFormat(DialogCancellation{});
}

TEST(PlatformFormattingTests, DescriptorSummariesDoNotDumpProcessArguments)
{
    const ProcessDesc process{
        .executable = std::filesystem::path{"ponder-tool"},
        .arguments = {"--token", "do-not-print"}};
    const std::string processText = std::format("{}", process);

    EXPECT_EQ(processText, "process(executable='ponder-tool', argumentCount=2)");
    EXPECT_EQ(processText.find("do-not-print"), std::string::npos);

    const OpenFileDialogDesc dialog{
        .parentWindowId = WindowId{7},
        .defaultLocation = std::filesystem::path{"workspace"},
        .filters = {{.name = "Molecules", .pattern = "mol;cif"}},
        .allowMultipleSelection = true};
    EXPECT_EQ(std::format("{}", dialog),
              "open_file_dialog(parent=7, defaultLocation='workspace', filterCount=1, "
              "allowMultipleSelection=true)");
}
} // namespace
