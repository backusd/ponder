#include <ponder/core/Result.hpp>
#include <ponder/io/Path.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace
{
namespace core = pond::core;
namespace io = pond::io;
namespace platform = pond::platform;

using namespace std::string_view_literals;

constexpr std::array kPhysicalKeyNames{
    "Unknown"sv,        "Tab"sv,             "ArrowLeft"sv,
    "ArrowRight"sv,     "ArrowUp"sv,         "ArrowDown"sv,
    "PageUp"sv,         "PageDown"sv,        "Home"sv,
    "End"sv,            "Insert"sv,          "Delete"sv,
    "Backspace"sv,      "Space"sv,           "Enter"sv,
    "Escape"sv,         "LeftControl"sv,     "LeftShift"sv,
    "LeftAlt"sv,        "LeftSuper"sv,       "RightControl"sv,
    "RightShift"sv,     "RightAlt"sv,        "RightSuper"sv,
    "Menu"sv,           "Digit0"sv,          "Digit1"sv,
    "Digit2"sv,         "Digit3"sv,          "Digit4"sv,
    "Digit5"sv,         "Digit6"sv,          "Digit7"sv,
    "Digit8"sv,         "Digit9"sv,          "A"sv,
    "B"sv,              "C"sv,               "D"sv,
    "E"sv,              "F"sv,               "G"sv,
    "H"sv,              "I"sv,               "J"sv,
    "K"sv,              "L"sv,               "M"sv,
    "N"sv,              "O"sv,               "P"sv,
    "Q"sv,              "R"sv,               "S"sv,
    "T"sv,              "U"sv,               "V"sv,
    "W"sv,              "X"sv,               "Y"sv,
    "Z"sv,              "F1"sv,              "F2"sv,
    "F3"sv,             "F4"sv,              "F5"sv,
    "F6"sv,             "F7"sv,              "F8"sv,
    "F9"sv,             "F10"sv,             "F11"sv,
    "F12"sv,            "F13"sv,             "F14"sv,
    "F15"sv,            "F16"sv,             "F17"sv,
    "F18"sv,            "F19"sv,             "F20"sv,
    "F21"sv,            "F22"sv,             "F23"sv,
    "F24"sv,            "Apostrophe"sv,      "Comma"sv,
    "Minus"sv,          "Period"sv,          "Slash"sv,
    "Semicolon"sv,      "Equal"sv,           "LeftBracket"sv,
    "Backslash"sv,      "RightBracket"sv,    "GraveAccent"sv,
    "CapsLock"sv,       "ScrollLock"sv,      "NumLock"sv,
    "PrintScreen"sv,    "Pause"sv,           "Keypad0"sv,
    "Keypad1"sv,        "Keypad2"sv,         "Keypad3"sv,
    "Keypad4"sv,        "Keypad5"sv,         "Keypad6"sv,
    "Keypad7"sv,        "Keypad8"sv,         "Keypad9"sv,
    "KeypadDecimal"sv,  "KeypadDivide"sv,    "KeypadMultiply"sv,
    "KeypadSubtract"sv, "KeypadAdd"sv,       "KeypadEnter"sv,
    "KeypadEqual"sv,    "BrowserBack"sv,     "BrowserForward"sv,
    "NonUsBackslash"sv,
};

constexpr std::array kNamedKeyNames{
    "Unknown"sv,        "Tab"sv,          "ArrowLeft"sv,
    "ArrowRight"sv,     "ArrowUp"sv,      "ArrowDown"sv,
    "PageUp"sv,         "PageDown"sv,     "Home"sv,
    "End"sv,            "Insert"sv,       "Delete"sv,
    "Backspace"sv,      "Enter"sv,        "Escape"sv,
    "LeftControl"sv,    "LeftShift"sv,    "LeftAlt"sv,
    "LeftSuper"sv,      "RightControl"sv, "RightShift"sv,
    "RightAlt"sv,       "RightSuper"sv,   "Menu"sv,
    "F1"sv,             "F2"sv,           "F3"sv,
    "F4"sv,             "F5"sv,           "F6"sv,
    "F7"sv,             "F8"sv,           "F9"sv,
    "F10"sv,            "F11"sv,          "F12"sv,
    "F13"sv,            "F14"sv,          "F15"sv,
    "F16"sv,            "F17"sv,          "F18"sv,
    "F19"sv,            "F20"sv,          "F21"sv,
    "F22"sv,            "F23"sv,          "F24"sv,
    "CapsLock"sv,       "ScrollLock"sv,   "NumLock"sv,
    "PrintScreen"sv,    "Pause"sv,        "Keypad0"sv,
    "Keypad1"sv,        "Keypad2"sv,      "Keypad3"sv,
    "Keypad4"sv,        "Keypad5"sv,      "Keypad6"sv,
    "Keypad7"sv,        "Keypad8"sv,      "Keypad9"sv,
    "KeypadDecimal"sv,  "KeypadDivide"sv, "KeypadMultiply"sv,
    "KeypadSubtract"sv, "KeypadAdd"sv,    "KeypadEnter"sv,
    "KeypadEqual"sv,    "BrowserBack"sv,  "BrowserForward"sv,
};

constexpr std::array kMouseButtonNames{
    "Unknown"sv,
    "Left"sv,
    "Right"sv,
    "Middle"sv,
    "X1"sv,
    "X2"sv,
};

constexpr std::array kSystemCursorShapes{
    platform::SystemCursorShape::Default,
    platform::SystemCursorShape::TextInput,
    platform::SystemCursorShape::Move,
    platform::SystemCursorShape::ResizeNorthSouth,
    platform::SystemCursorShape::ResizeEastWest,
    platform::SystemCursorShape::ResizeNortheastSouthwest,
    platform::SystemCursorShape::ResizeNorthwestSoutheast,
    platform::SystemCursorShape::Pointer,
    platform::SystemCursorShape::Wait,
    platform::SystemCursorShape::Progress,
    platform::SystemCursorShape::NotAllowed,
};

constexpr std::array kSystemCursorShapeNames{
    "Default"sv,
    "TextInput"sv,
    "Move"sv,
    "ResizeNorthSouth"sv,
    "ResizeEastWest"sv,
    "ResizeNortheastSouthwest"sv,
    "ResizeNorthwestSoutheast"sv,
    "Pointer"sv,
    "Wait"sv,
    "Progress"sv,
    "NotAllowed"sv,
};

static_assert(kPhysicalKeyNames.size() ==
              static_cast<std::size_t>(platform::PhysicalKey::NonUsBackslash) + 1U);
static_assert(kNamedKeyNames.size() ==
              static_cast<std::size_t>(platform::NamedKey::BrowserForward) + 1U);
static_assert(kMouseButtonNames.size() ==
              static_cast<std::size_t>(platform::MouseButton::X2) + 1U);
static_assert(kSystemCursorShapes.size() == kSystemCursorShapeNames.size());

struct Options final
{
    std::optional<platform::PlatformTimestamp::Duration> autoCloseAfter;
    bool showHelp{};
};

struct WindowSlot final
{
    platform::Window window;
    std::string label;
    bool textInputActive{};
    bool imeAreaSet{};
    bool titleUpdateFailureReported{};
};

struct AppState final
{
    platform::PlatformRuntime& runtime;
    std::vector<WindowSlot>& windows;
    platform::PlatformTimestamp startTimestamp;
    std::size_t activeWindowIndex{};
    std::uint64_t eventCount{};
    bool quitRequested{};
    bool textInputEnabled{true};
    bool globalCaptureEnabled{};
    std::size_t cursorShapeIndex{};
    std::string lastText{"<none>"};
    std::string lastDrop{"<none>"};
};

[[nodiscard]] core::Error MakeOptionError(std::string message)
{
    return core::Error{core::ErrorCode{core::ErrorCategory::InvalidArgument, 0},
                       std::move(message)};
}

void PrintUsage(std::string_view executableName)
{
    std::cout
        << "Usage: " << executableName << " [--auto-close-ms <milliseconds>]\n\n"
        << "Controls:\n"
        << "  F1            Print this help text.\n"
        << "  1 / 2         Select the active monitor window.\n"
        << "  T             Toggle text input for the active window.\n"
        << "  I / A         Set or clear a fixed logical IME candidate area.\n"
        << "  X             Clear active text composition.\n"
        << "  G / R         Toggle mouse grab or relative mouse mode on the active window.\n"
        << "  P / M         Toggle global capture or query global mouse position.\n"
        << "  S / V         Cycle system cursor shape or toggle cursor visibility.\n"
        << "  Q / Escape    Quit.\n";
}

[[nodiscard]] core::Result<platform::PlatformTimestamp::Duration> ParseMilliseconds(
    std::string_view text)
{
    std::uint64_t value{};
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [next, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || next != end)
    {
        return core::Result<platform::PlatformTimestamp::Duration>::FromError(
            MakeOptionError("Expected a non-negative integer millisecond value."));
    }

    using Milliseconds = std::chrono::milliseconds;
    constexpr auto kMaxMilliseconds = static_cast<std::uint64_t>(
        std::numeric_limits<Milliseconds::rep>::max());
    if (value > kMaxMilliseconds)
    {
        return core::Result<platform::PlatformTimestamp::Duration>::FromError(
            MakeOptionError("Auto-close duration is too large."));
    }

    return platform::PlatformTimestamp::Duration{
        Milliseconds{static_cast<Milliseconds::rep>(value)}};
}

[[nodiscard]] core::Result<Options> ParseOptions(int argc, char** argv)
{
    Options options{};
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--help" || argument == "-h")
        {
            options.showHelp = true;
        }
        else if (argument == "--auto-close-ms")
        {
            if (index + 1 >= argc)
            {
                return core::Result<Options>::FromError(
                    MakeOptionError("--auto-close-ms requires a value."));
            }

            ++index;
            auto duration = ParseMilliseconds(argv[index]);
            if (!duration)
            {
                return core::Result<Options>::FromError(std::move(duration).GetError());
            }
            options.autoCloseAfter = std::move(duration).GetValue();
        }
        else
        {
            return core::Result<Options>::FromError(
                MakeOptionError("Unknown option: " + std::string{argument}));
        }
    }

    return options;
}

template <typename Id>
[[nodiscard]] std::string FormatId(Id id)
{
    if (!id.IsValid())
    {
        return "invalid";
    }
    return std::to_string(id.GetValue());
}

template <typename Id>
[[nodiscard]] std::string FormatOptionalId(const std::optional<Id>& id)
{
    if (!id)
    {
        return "none";
    }
    return FormatId(*id);
}

[[nodiscard]] std::string FormatDuration(platform::PlatformTimestamp::Duration duration)
{
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return std::to_string(milliseconds.count()) + " ms";
}

[[nodiscard]] std::string FormatTimestamp(platform::PlatformTimestamp timestamp)
{
    return std::to_string(timestamp.GetTimeSinceEpoch().count()) + " ns";
}

[[nodiscard]] std::string FormatFloat(float value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

[[nodiscard]] std::string FormatLogicalPoint(platform::LogicalPoint point)
{
    return "(" + FormatFloat(point.x) + ", " + FormatFloat(point.y) + ")";
}

template <typename Enum, std::size_t Size>
[[nodiscard]] std::string_view NameFromTable(Enum value,
                                             const std::array<std::string_view, Size>& names)
{
    const auto index = static_cast<std::size_t>(value);
    if (index >= names.size())
    {
        return "Unrecognized";
    }
    return names[index];
}

[[nodiscard]] std::string_view ToString(platform::PhysicalKey key)
{
    return NameFromTable(key, kPhysicalKeyNames);
}

[[nodiscard]] std::string_view ToString(platform::NamedKey key)
{
    return NameFromTable(key, kNamedKeyNames);
}

[[nodiscard]] std::string_view ToString(platform::MouseButton button)
{
    return NameFromTable(button, kMouseButtonNames);
}

[[nodiscard]] std::string_view ToString(platform::SystemCursorShape shape)
{
    return NameFromTable(shape, kSystemCursorShapeNames);
}

[[nodiscard]] std::string FormatCharacter(char32_t character)
{
    std::ostringstream stream;
    stream << "U+" << std::uppercase << std::hex << std::setfill('0');
    if (character <= 0xFFFFU)
    {
        stream << std::setw(4);
    }
    else
    {
        stream << std::setw(6);
    }
    stream << static_cast<std::uint32_t>(character);
    return stream.str();
}

[[nodiscard]] std::string FormatLogicalKey(platform::LogicalKey key)
{
    switch (key.GetKind())
    {
    case platform::LogicalKey::Kind::Unknown:
        return "Unknown";
    case platform::LogicalKey::Kind::Character:
        return "Character(" + FormatCharacter(*key.GetCharacter()) + ")";
    case platform::LogicalKey::Kind::Named:
        return "Named(" + std::string{ToString(*key.GetNamedKey())} + ")";
    }

    return "Unrecognized";
}

[[nodiscard]] std::string FormatModifiers(platform::KeyModifiers modifiers)
{
    std::string flags;
    const auto append = [&](platform::KeyModifiers flag, std::string_view name) {
        if (!platform::HasAnyKeyModifier(modifiers, flag))
        {
            return;
        }
        if (!flags.empty())
        {
            flags += '|';
        }
        flags += name;
    };

    append(platform::KeyModifiers::LeftShift, "LeftShift");
    append(platform::KeyModifiers::RightShift, "RightShift");
    append(platform::KeyModifiers::Level5Shift, "Level5Shift");
    append(platform::KeyModifiers::LeftControl, "LeftControl");
    append(platform::KeyModifiers::RightControl, "RightControl");
    append(platform::KeyModifiers::LeftAlt, "LeftAlt");
    append(platform::KeyModifiers::RightAlt, "RightAlt");
    append(platform::KeyModifiers::LeftSuper, "LeftSuper");
    append(platform::KeyModifiers::RightSuper, "RightSuper");
    append(platform::KeyModifiers::NumLock, "NumLock");
    append(platform::KeyModifiers::CapsLock, "CapsLock");
    append(platform::KeyModifiers::AltGraph, "AltGraph");
    append(platform::KeyModifiers::ScrollLock, "ScrollLock");

    if (flags.empty())
    {
        flags = "None";
    }

    const bool anyShift = platform::HasAnyKeyModifier(modifiers, platform::KeyModifiers::Shift);
    const bool allShift = platform::HasAllKeyModifiers(modifiers, platform::KeyModifiers::Shift);
    const bool anyControl =
        platform::HasAnyKeyModifier(modifiers, platform::KeyModifiers::Control);

    return flags + " [anyShift=" + (anyShift ? "true" : "false") +
           ", bothShifts=" + (allShift ? "true" : "false") +
           ", anyControl=" + (anyControl ? "true" : "false") + "]";
}

[[nodiscard]] std::string QuoteText(std::string_view text)
{
    std::ostringstream stream;
    stream << '"';
    for (unsigned char character : text)
    {
        switch (character)
        {
        case '\\':
            stream << "\\\\";
            break;
        case '"':
            stream << "\\\"";
            break;
        case '\n':
            stream << "\\n";
            break;
        case '\r':
            stream << "\\r";
            break;
        case '\t':
            stream << "\\t";
            break;
        default:
            if (character < 0x20U)
            {
                stream << "\\x" << std::uppercase << std::hex << std::setw(2)
                       << std::setfill('0') << static_cast<int>(character) << std::dec;
            }
            else
            {
                stream << static_cast<char>(character);
            }
            break;
        }
    }
    stream << '"';
    return stream.str();
}

[[nodiscard]] std::string FormatSourceApplication(const std::optional<std::string>& source)
{
    if (!source)
    {
        return "none";
    }
    return QuoteText(*source);
}

[[nodiscard]] std::string FormatSelection(
    const std::optional<platform::TextCompositionRange>& selection)
{
    if (!selection)
    {
        return "none";
    }
    return "[start=" + std::to_string(selection->start) +
           ", length=" + std::to_string(selection->length) + "]";
}

void PrintError(std::string_view operation, const core::Error& error)
{
    std::cout << operation << " failed: " << core::FormatError(error) << '\n';
}

[[nodiscard]] bool IsPlatformError(const core::Error& error, platform::PlatformErrorCode code)
{
    return error.GetCode() == platform::ToErrorCode(code);
}

void PrintOperationResult(std::string_view operation, const core::VoidResult& result)
{
    if (result)
    {
        std::cout << operation << " succeeded.\n";
        return;
    }

    if (IsPlatformError(result.GetError(), platform::PlatformErrorCode::Unsupported))
    {
        std::cout << operation << " is unsupported on this host: "
                  << core::FormatError(result.GetError()) << '\n';
        return;
    }

    PrintError(operation, result.GetError());
}

template <typename Value, typename Formatter>
void PrintQueryResult(std::string_view label, const core::Result<Value>& result,
                      Formatter formatter)
{
    if (result)
    {
        std::cout << label << ": " << formatter(result.GetValue()) << '\n';
        return;
    }

    if (IsPlatformError(result.GetError(), platform::PlatformErrorCode::Unsupported))
    {
        std::cout << label << " unsupported: " << core::FormatError(result.GetError()) << '\n';
        return;
    }

    PrintError(label, result.GetError());
}

void PrintLogicalKeyHelperExamples()
{
    constexpr platform::LogicalKey unknown = platform::LogicalKey::Unknown();
    constexpr platform::LogicalKey character = platform::LogicalKey::FromCharacter(U'P');
    constexpr platform::LogicalKey invalidCharacter = platform::LogicalKey::FromCharacter(U'\0');
    constexpr platform::LogicalKey named =
        platform::LogicalKey::FromNamed(platform::NamedKey::Escape);
    constexpr platform::KeyModifiers combined =
        platform::KeyModifiers::LeftShift | platform::KeyModifiers::LeftControl;

    static_assert(unknown.GetKind() == platform::LogicalKey::Kind::Unknown);
    static_assert(character.GetCharacter().has_value());
    static_assert(!invalidCharacter.GetCharacter().has_value());
    static_assert(named.GetNamedKey().has_value());
    static_assert(platform::HasAnyKeyModifier(combined, platform::KeyModifiers::Shift));
    static_assert(platform::HasAllKeyModifiers(combined, platform::KeyModifiers::LeftControl));

    std::cout << "Logical key helper examples:\n"
              << "  Unknown: " << FormatLogicalKey(unknown) << '\n'
              << "  Character P: " << FormatLogicalKey(character) << '\n'
              << "  Invalid null character: " << FormatLogicalKey(invalidCharacter) << '\n'
              << "  Named Escape: " << FormatLogicalKey(named) << '\n'
              << "  Combined modifiers: " << FormatModifiers(combined) << '\n';
}

[[nodiscard]] core::Result<WindowSlot> CreateWindowSlot(platform::PlatformRuntime& runtime,
                                                        const platform::WindowDesc& desc,
                                                        std::string label)
{
    auto window = runtime.CreateWindow(desc);
    if (!window)
    {
        return core::Result<WindowSlot>::FromError(std::move(window).GetError());
    }

    return WindowSlot{std::move(window).GetValue(), std::move(label)};
}

[[nodiscard]] WindowSlot* ActiveWindow(AppState& state)
{
    if (state.activeWindowIndex >= state.windows.size())
    {
        return nullptr;
    }
    return &state.windows[state.activeWindowIndex];
}

[[nodiscard]] std::size_t FindWindowIndex(const AppState& state, platform::WindowId id)
{
    const auto found = std::ranges::find_if(state.windows, [id](const WindowSlot& slot) {
        return slot.window.GetId() == id;
    });
    if (found == state.windows.end())
    {
        return state.windows.size();
    }
    return static_cast<std::size_t>(found - state.windows.begin());
}

void SyncTextInput(WindowSlot& slot, bool shouldBeActive)
{
    const std::string operation = slot.label + (shouldBeActive ? ".StartTextInput" :
                                                               ".StopTextInput");
    core::VoidResult result = shouldBeActive ? slot.window.StartTextInput() :
                                               slot.window.StopTextInput();
    if (!result)
    {
        PrintOperationResult(operation, result);
        return;
    }

    slot.textInputActive = shouldBeActive;
    std::cout << operation << " succeeded; live active=" << slot.window.IsTextInputActive()
              << ".\n";
}

void SetActiveWindow(AppState& state, std::size_t index)
{
    if (index >= state.windows.size())
    {
        std::cout << "Window slot " << (index + 1U) << " is not available.\n";
        return;
    }

    if (state.activeWindowIndex < state.windows.size() && state.activeWindowIndex != index &&
        state.textInputEnabled)
    {
        SyncTextInput(state.windows[state.activeWindowIndex], false);
    }

    state.activeWindowIndex = index;
    std::cout << "Active window is now " << state.windows[index].label << " (id "
              << FormatId(state.windows[index].window.GetId()) << ").\n";

    if (state.textInputEnabled)
    {
        SyncTextInput(state.windows[index], true);
    }
}

void UpdateWindowTitles(AppState& state)
{
    for (std::size_t index = 0; index < state.windows.size(); ++index)
    {
        WindowSlot& slot = state.windows[index];
        const std::string marker = index == state.activeWindowIndex ? "*" : " ";
        const std::string title = marker + slot.label + " | events " +
                                  std::to_string(state.eventCount) + " | text " +
                                  state.lastText + " | drop " + state.lastDrop;
        auto result = slot.window.SetTitle(title);
        if (!result && !slot.titleUpdateFailureReported)
        {
            PrintError("SetTitle during title update", result.GetError());
            slot.titleUpdateFailureReported = true;
        }
    }
}

void SetImeArea(WindowSlot& slot)
{
    const platform::TextInputArea area{
        .rectangle = {.origin = {.x = 32.0F, .y = 64.0F},
                      .extent = {.width = 320.0F, .height = 36.0F}},
        .cursorOffset = 24.0F,
    };

    if (!platform::IsValid(area))
    {
        std::cout << "Fixed IME area is unexpectedly invalid.\n";
        return;
    }

    auto result = slot.window.SetTextInputArea(area);
    if (!result)
    {
        PrintOperationResult(slot.label + ".SetTextInputArea", result);
        return;
    }

    slot.imeAreaSet = true;
    std::cout << slot.label << ".SetTextInputArea succeeded at origin "
              << FormatLogicalPoint(area.rectangle.origin) << ", extent ("
              << FormatFloat(area.rectangle.extent.width) << ", "
              << FormatFloat(area.rectangle.extent.height)
              << "), cursorOffset=" << FormatFloat(area.cursorOffset) << ".\n";
}

void ClearImeArea(WindowSlot& slot)
{
    auto result = slot.window.ClearTextInputArea();
    if (!result)
    {
        PrintOperationResult(slot.label + ".ClearTextInputArea", result);
        return;
    }

    slot.imeAreaSet = false;
    std::cout << slot.label << ".ClearTextInputArea succeeded.\n";
}

void ToggleTextInput(AppState& state)
{
    WindowSlot* const active = ActiveWindow(state);
    if (active == nullptr)
    {
        return;
    }

    state.textInputEnabled = !state.textInputEnabled;
    SyncTextInput(*active, state.textInputEnabled);
}

void ToggleMouseGrab(WindowSlot& slot)
{
    const bool target = !slot.window.IsMouseGrabbed();
    auto result = slot.window.SetMouseGrab(target);
    PrintOperationResult(slot.label + (target ? ".SetMouseGrab(true)" :
                                               ".SetMouseGrab(false)"),
                         result);
    std::cout << "  live mouse grabbed=" << slot.window.IsMouseGrabbed() << '\n';
}

void ToggleRelativeMouseMode(WindowSlot& slot)
{
    const bool target = !slot.window.IsRelativeMouseModeEnabled();
    auto result = slot.window.SetRelativeMouseMode(target);
    PrintOperationResult(slot.label + (target ? ".SetRelativeMouseMode(true)" :
                                               ".SetRelativeMouseMode(false)"),
                         result);
    std::cout << "  live relative mouse mode=" << slot.window.IsRelativeMouseModeEnabled() << '\n';
}

void ToggleGlobalMouseCapture(AppState& state)
{
    const bool target = !state.globalCaptureEnabled;
    auto result = state.runtime.SetMouseCapture(target);
    if (result)
    {
        state.globalCaptureEnabled = target;
    }
    PrintOperationResult(target ? "PlatformRuntime::SetMouseCapture(true)" :
                                  "PlatformRuntime::SetMouseCapture(false)",
                         result);
}

void QueryGlobalMousePosition(platform::PlatformRuntime& runtime)
{
    const auto result = runtime.GetGlobalMousePosition();
    PrintQueryResult("PlatformRuntime::GetGlobalMousePosition", result, FormatLogicalPoint);
}

void CycleCursorShape(AppState& state)
{
    state.cursorShapeIndex = (state.cursorShapeIndex + 1U) % kSystemCursorShapes.size();
    const platform::SystemCursorShape shape = kSystemCursorShapes[state.cursorShapeIndex];
    auto result = state.runtime.SetSystemCursor(shape);
    PrintOperationResult("PlatformRuntime::SetSystemCursor(" + std::string{ToString(shape)} + ")",
                         result);
}

void ToggleCursorVisibility(platform::PlatformRuntime& runtime)
{
    if (runtime.IsCursorVisible())
    {
        PrintOperationResult("PlatformRuntime::HideCursor", runtime.HideCursor());
    }
    else
    {
        PrintOperationResult("PlatformRuntime::ShowCursor", runtime.ShowCursor());
    }
    std::cout << "  live cursor visible=" << runtime.IsCursorVisible() << '\n';
}

void CleanupWindow(WindowSlot& slot)
{
    if (slot.imeAreaSet)
    {
        PrintOperationResult(slot.label + ".ClearTextInputArea", slot.window.ClearTextInputArea());
        slot.imeAreaSet = false;
    }

    PrintOperationResult(slot.label + ".StopTextInput", slot.window.StopTextInput());
    PrintOperationResult(slot.label + ".SetRelativeMouseMode(false)",
                         slot.window.SetRelativeMouseMode(false));
    PrintOperationResult(slot.label + ".SetMouseGrab(false)", slot.window.SetMouseGrab(false));
}

void ReleaseWindow(AppState& state, platform::WindowId id)
{
    const std::size_t index = FindWindowIndex(state, id);
    if (index >= state.windows.size())
    {
        std::cout << "Close request did not match an owned window.\n";
        return;
    }

    CleanupWindow(state.windows[index]);
    std::cout << "Released application-owned window id " << FormatId(id) << ".\n";
    state.windows.erase(state.windows.begin() + static_cast<std::ptrdiff_t>(index));

    if (state.windows.empty())
    {
        return;
    }

    if (state.activeWindowIndex >= state.windows.size())
    {
        state.activeWindowIndex = state.windows.size() - 1U;
    }
    if (state.textInputEnabled)
    {
        SyncTextInput(state.windows[state.activeWindowIndex], true);
    }
}

void HandleCommand(AppState& state, platform::PhysicalKey key)
{
    WindowSlot* const active = ActiveWindow(state);

    switch (key)
    {
    case platform::PhysicalKey::F1:
        PrintUsage("ponder-platform-2-input-drop-monitor");
        return;
    case platform::PhysicalKey::Digit1:
        SetActiveWindow(state, 0);
        return;
    case platform::PhysicalKey::Digit2:
        SetActiveWindow(state, 1);
        return;
    case platform::PhysicalKey::T:
        ToggleTextInput(state);
        return;
    case platform::PhysicalKey::I:
        if (active != nullptr)
        {
            SetImeArea(*active);
        }
        return;
    case platform::PhysicalKey::A:
        if (active != nullptr)
        {
            ClearImeArea(*active);
        }
        return;
    case platform::PhysicalKey::X:
        if (active != nullptr)
        {
            PrintOperationResult(active->label + ".ClearTextComposition",
                                 active->window.ClearTextComposition());
        }
        return;
    case platform::PhysicalKey::G:
        if (active != nullptr)
        {
            ToggleMouseGrab(*active);
        }
        return;
    case platform::PhysicalKey::R:
        if (active != nullptr)
        {
            ToggleRelativeMouseMode(*active);
        }
        return;
    case platform::PhysicalKey::P:
        ToggleGlobalMouseCapture(state);
        return;
    case platform::PhysicalKey::M:
        QueryGlobalMousePosition(state.runtime);
        return;
    case platform::PhysicalKey::S:
        CycleCursorShape(state);
        return;
    case platform::PhysicalKey::V:
        ToggleCursorVisibility(state.runtime);
        return;
    case platform::PhysicalKey::Q:
    case platform::PhysicalKey::Escape:
        state.quitRequested = true;
        return;
    default:
        return;
    }
}

void PrintEventHeader(std::string_view name, platform::PlatformTimestamp timestamp,
                      const AppState& state)
{
    std::cout << "[event " << state.eventCount << "] " << name << " at "
              << FormatTimestamp(timestamp) << " (+"
              << FormatDuration(timestamp - state.startTimestamp) << ")";
}

struct EventVisitor final
{
    AppState& state;

    void operator()(const platform::QuitRequestedEvent& event) const
    {
        PrintEventHeader("QuitRequested", event.timestamp, state);
        std::cout << '\n';
        state.quitRequested = true;
    }

    void operator()(const platform::WindowCloseRequestedEvent& event) const
    {
        PrintEventHeader("WindowCloseRequested", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId) << '\n';
        ReleaseWindow(state, event.windowId);
    }

    void operator()(const platform::WindowFocusChangedEvent& event) const
    {
        PrintEventHeader("WindowFocusChanged", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId) << " focused=" << event.focused
                  << '\n';
        if (event.focused)
        {
            const std::size_t index = FindWindowIndex(state, event.windowId);
            if (index < state.windows.size())
            {
                SetActiveWindow(state, index);
            }
        }
    }

    void operator()(const platform::WindowPointerEnteredEvent& event) const
    {
        PrintEventHeader("WindowPointerEntered", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId) << '\n';
    }

    void operator()(const platform::WindowPointerLeftEvent& event) const
    {
        PrintEventHeader("WindowPointerLeft", event.timestamp, state);
        std::cout << " window=" << FormatId(event.windowId) << '\n';
    }

    void operator()(const platform::KeyboardKeyEvent& event) const
    {
        PrintEventHeader("KeyboardKey", event.timestamp, state);
        std::cout << " window=" << FormatOptionalId(event.windowId)
                  << " physical=" << ToString(event.physicalKey)
                  << " logical=" << FormatLogicalKey(event.logicalKey)
                  << " modifiers=" << FormatModifiers(event.modifiers)
                  << " pressed=" << event.pressed << " repeat=" << event.repeat << '\n';

        if (event.pressed && !event.repeat)
        {
            HandleCommand(state, event.physicalKey);
        }
    }

    void operator()(const platform::TextInputEvent& event) const
    {
        state.lastText = QuoteText(event.text);
        PrintEventHeader("TextInput", event.timestamp, state);
        std::cout << " window=" << FormatOptionalId(event.windowId)
                  << " text=" << state.lastText << '\n';
    }

    void operator()(const platform::TextCompositionEvent& event) const
    {
        PrintEventHeader("TextComposition", event.timestamp, state);
        std::cout << " window=" << FormatOptionalId(event.windowId)
                  << " text=" << QuoteText(event.text)
                  << " selection=" << FormatSelection(event.selection) << '\n';
    }

    void operator()(const platform::MouseMotionEvent& event) const
    {
        PrintEventHeader("MouseMotion", event.timestamp, state);
        std::cout << " window=" << FormatOptionalId(event.windowId)
                  << " position=" << FormatLogicalPoint(event.position)
                  << " relative=" << FormatLogicalPoint(event.relativeMovement) << '\n';
    }

    void operator()(const platform::MouseButtonEvent& event) const
    {
        PrintEventHeader("MouseButton", event.timestamp, state);
        std::cout << " window=" << FormatOptionalId(event.windowId)
                  << " position=" << FormatLogicalPoint(event.position)
                  << " button=" << ToString(event.button) << " pressed=" << event.pressed
                  << '\n';
    }

    void operator()(const platform::MouseWheelEvent& event) const
    {
        PrintEventHeader("MouseWheel", event.timestamp, state);
        std::cout << " window=" << FormatOptionalId(event.windowId)
                  << " position=" << FormatLogicalPoint(event.position)
                  << " horizontal=" << FormatFloat(event.horizontal)
                  << " vertical=" << FormatFloat(event.vertical) << '\n';
    }

    void operator()(const platform::DropBeginEvent& event) const
    {
        state.lastDrop = "begin";
        PrintEventHeader("DropBegin", event.timestamp, state);
        std::cout << " window=" << FormatOptionalId(event.windowId)
                  << " source=" << FormatSourceApplication(event.sourceApplication) << '\n';
    }

    void operator()(const platform::DroppedFileEvent& event) const
    {
        const std::string path = io::PathToUtf8(event.path);
        state.lastDrop = "file " + path;
        PrintEventHeader("DroppedFile", event.timestamp, state);
        std::cout << " window=" << FormatOptionalId(event.windowId)
                  << " position=" << FormatLogicalPoint(event.position)
                  << " path=" << QuoteText(path)
                  << " source=" << FormatSourceApplication(event.sourceApplication) << '\n';
    }

    void operator()(const platform::DroppedTextEvent& event) const
    {
        state.lastDrop = "text " + QuoteText(event.text);
        PrintEventHeader("DroppedText", event.timestamp, state);
        std::cout << " window=" << FormatOptionalId(event.windowId)
                  << " position=" << FormatLogicalPoint(event.position)
                  << " text=" << QuoteText(event.text)
                  << " source=" << FormatSourceApplication(event.sourceApplication) << '\n';
    }

    void operator()(const platform::DropPositionEvent& event) const
    {
        state.lastDrop = "position " + FormatLogicalPoint(event.position);
        PrintEventHeader("DropPosition", event.timestamp, state);
        std::cout << " window=" << FormatOptionalId(event.windowId)
                  << " position=" << FormatLogicalPoint(event.position)
                  << " source=" << FormatSourceApplication(event.sourceApplication) << '\n';
    }

    void operator()(const platform::DropCompleteEvent& event) const
    {
        state.lastDrop = "complete " + FormatLogicalPoint(event.position);
        PrintEventHeader("DropComplete", event.timestamp, state);
        std::cout << " window=" << FormatOptionalId(event.windowId)
                  << " position=" << FormatLogicalPoint(event.position)
                  << " source=" << FormatSourceApplication(event.sourceApplication) << '\n';
    }

    template <typename Event>
    void operator()(const Event& event) const
    {
        PrintEventHeader("Other platform event", event.timestamp, state);
        std::cout << " observed by another example in the suite.\n";
    }
};

void DrainEvents(AppState& state)
{
    while (std::optional<platform::PlatformEvent> event = state.runtime.PollEvent())
    {
        ++state.eventCount;
        std::visit(EventVisitor{state}, *event);
    }
}

void Shutdown(AppState& state)
{
    for (WindowSlot& slot : state.windows)
    {
        CleanupWindow(slot);
    }

    PrintOperationResult("PlatformRuntime::SetMouseCapture(false)",
                         state.runtime.SetMouseCapture(false));
    PrintOperationResult("PlatformRuntime::SetSystemCursor(Default)",
                         state.runtime.SetSystemCursor(platform::SystemCursorShape::Default));

    if (!state.runtime.IsCursorVisible())
    {
        PrintOperationResult("PlatformRuntime::ShowCursor", state.runtime.ShowCursor());
    }

    state.windows.clear();
}

[[nodiscard]] core::VoidResult RunInputDropMonitor(int argc, char** argv)
{
    auto optionsResult = ParseOptions(argc, argv);
    if (!optionsResult)
    {
        return core::VoidResult::FromError(std::move(optionsResult).GetError());
    }

    const Options options = std::move(optionsResult).GetValue();
    if (options.showHelp)
    {
        PrintUsage(argc > 0 ? argv[0] : "ponder-platform-2-input-drop-monitor");
        return {};
    }

    PrintLogicalKeyHelperExamples();

    const platform::PlatformRuntimeDesc runtimeDesc{
        .applicationName = "Ponder Platform Input Drop Monitor",
        .applicationVersion = std::string{"0.1.0"},
        .applicationIdentifier = std::string{"org.ponder.examples.platform.input-drop-monitor"},
    };

    auto runtimeResult = platform::PlatformRuntime::Create(runtimeDesc);
    if (!runtimeResult)
    {
        return core::VoidResult::FromError(std::move(runtimeResult).GetError());
    }

    platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    const platform::PlatformTimestamp start = runtime.Now();

    std::vector<WindowSlot> windows;
    windows.reserve(2);

    const platform::WindowDesc primaryDesc{
        .title = "Ponder Input Drop Monitor - primary",
        .logicalSize = {840, 520},
        .visible = true,
        .resizable = true,
        .highPixelDensity = true,
        .minimumLogicalSize = platform::LogicalSize{320, 240},
        .graphicsCompatibility = platform::WindowGraphicsCompatibility::Default,
    };
    auto primary = CreateWindowSlot(runtime, primaryDesc, "primary");
    if (!primary)
    {
        return core::VoidResult::FromError(std::move(primary).GetError());
    }
    windows.push_back(std::move(primary).GetValue());

    const platform::WindowDesc secondaryDesc{
        .title = "Ponder Input Drop Monitor - secondary",
        .logicalSize = {560, 400},
        .visible = true,
        .resizable = true,
        .highPixelDensity = true,
        .minimumLogicalSize = platform::LogicalSize{280, 200},
        .graphicsCompatibility = platform::WindowGraphicsCompatibility::Default,
    };
    auto secondary = CreateWindowSlot(runtime, secondaryDesc, "secondary");
    if (!secondary)
    {
        return core::VoidResult::FromError(std::move(secondary).GetError());
    }
    windows.push_back(std::move(secondary).GetValue());

    AppState state{.runtime = runtime, .windows = windows, .startTimestamp = start};
    SetActiveWindow(state, 0);
    PrintUsage(argc > 0 ? argv[0] : "ponder-platform-2-input-drop-monitor");

    auto nextTitleUpdate = start;
    while (!state.quitRequested && !state.windows.empty())
    {
        DrainEvents(state);

        const platform::PlatformTimestamp now = runtime.Now();
        if (now - nextTitleUpdate >= std::chrono::milliseconds{250})
        {
            UpdateWindowTitles(state);
            nextTitleUpdate = now;
        }

        if (options.autoCloseAfter && now - start >= *options.autoCloseAfter)
        {
            std::cout << "Auto-close duration reached after " << FormatDuration(now - start)
                      << ".\n";
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{8});
    }

    Shutdown(state);
    return {};
}
} // namespace

int main(int argc, char** argv)
{
    std::cout << std::boolalpha;

    try
    {
        const auto result = RunInputDropMonitor(argc, argv);
        if (!result)
        {
            std::cerr << "ponder-platform-2-input-drop-monitor failed: "
                      << core::FormatError(result.GetError()) << '\n';
            return 1;
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << "ponder-platform-2-input-drop-monitor terminated with an exception: "
                  << exception.what() << '\n';
        return 1;
    }

    return 0;
}
