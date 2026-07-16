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
#include <format>
#include <iomanip>
#include <print>
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
struct OptionalWindowId final
{
    const std::optional<pond::platform::WindowId>& value;
};
} // namespace

namespace std
{
template <>
struct formatter<pond::platform::WindowId> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::WindowId id, FormatContext& context) const
    {
        if (!id.IsValid())
        {
            return formatter<string>::format("invalid", context);
        }

        return formatter<string>::format(std::to_string(id.GetValue()), context);
    }
};

template <>
struct formatter<OptionalWindowId> : formatter<string>
{
    template <typename FormatContext>
    auto format(const OptionalWindowId& id, FormatContext& context) const
    {
        if (!id.value)
        {
            return formatter<string>::format("none", context);
        }

        return formatter<string>::format(std::format("{}", *id.value), context);
    }
};
} // namespace std

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
    std::optional<platform::Duration> autoCloseAfter;
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
    platform::Timestamp startTimestamp;
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
    std::print(
        "Usage: {} [--auto-close-ms <milliseconds>]\n\n"
        "Controls:\n"
        "  F1            Print this help text.\n"
        "  1 / 2         Select the active monitor window.\n"
        "  T             Toggle text input for the active window.\n"
        "  I / A         Set or clear a fixed logical IME candidate area.\n"
        "  X             Clear active text composition.\n"
        "  G / R         Toggle mouse grab or relative mouse mode on the active window.\n"
        "  P / M         Toggle global capture or query global mouse position.\n"
        "  S / V         Cycle system cursor shape or toggle cursor visibility.\n"
        "  Q / Escape    Quit.\n",
        executableName);
}

[[nodiscard]] core::Result<platform::Duration> ParseMilliseconds(
    std::string_view text)
{
    std::uint64_t value{};
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [next, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || next != end)
    {
        return core::Result<platform::Duration>::FromError(
            MakeOptionError("Expected a non-negative integer millisecond value."));
    }

    using Milliseconds = std::chrono::milliseconds;
    constexpr auto kMaxMilliseconds = static_cast<std::uint64_t>(
        std::numeric_limits<Milliseconds::rep>::max());
    if (value > kMaxMilliseconds)
    {
        return core::Result<platform::Duration>::FromError(
            MakeOptionError("Auto-close duration is too large."));
    }

    return platform::Duration{
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
            RETURN_ERROR_IF_FAILED(duration);
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
    std::println("{} failed: {}", operation, error);
}

void PrintOperationResult(std::string_view operation, const core::VoidResult& result)
{
    if (result)
    {
        std::println("{} succeeded.", operation);
        return;
    }

    if (result.GetError() == platform::PlatformErrorCode::Unsupported)
    {
        std::println("{} is unsupported on this host: {}", operation, result.GetError());
        return;
    }

    PrintError(operation, result.GetError());
}

template <typename Value>
void PrintQueryResult(std::string_view label, const core::Result<Value>& result)
{
    if (result)
    {
        std::println("{}: {}", label, result.GetValue());
        return;
    }

    if (result.GetError() == platform::PlatformErrorCode::Unsupported)
    {
        std::println("{} unsupported: {}", label, result.GetError());
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

    std::println(
        "Logical key helper examples:\n"
        "  Unknown: {}\n"
        "  Character P: {}\n"
        "  Invalid null character: {}\n"
        "  Named Escape: {}\n"
        "  Combined modifiers: {}",
        FormatLogicalKey(unknown), FormatLogicalKey(character), FormatLogicalKey(invalidCharacter),
        FormatLogicalKey(named), FormatModifiers(combined));
}

[[nodiscard]] core::Result<WindowSlot> CreateWindowSlot(platform::PlatformRuntime& runtime,
                                                        const platform::WindowDesc& desc,
                                                        std::string label)
{
    auto window = runtime.CreateWindow(desc);
    RETURN_ERROR_IF_FAILED(window);

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
    std::println("{} succeeded; live active={}.", operation, slot.window.IsTextInputActive());
}

void SetActiveWindow(AppState& state, std::size_t index)
{
    if (index >= state.windows.size())
    {
        std::println("Window slot {} is not available.", index + 1U);
        return;
    }

    if (state.activeWindowIndex < state.windows.size() && state.activeWindowIndex != index &&
        state.textInputEnabled)
    {
        SyncTextInput(state.windows[state.activeWindowIndex], false);
    }

    state.activeWindowIndex = index;
    std::println("Active window is now {} (id {}).", state.windows[index].label,
                 state.windows[index].window.GetId());

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
        std::println("Fixed IME area is unexpectedly invalid.");
        return;
    }

    auto result = slot.window.SetTextInputArea(area);
    if (!result)
    {
        PrintOperationResult(slot.label + ".SetTextInputArea", result);
        return;
    }

    slot.imeAreaSet = true;
    std::println("{}.SetTextInputArea succeeded at origin {}, extent ({}, {}), cursorOffset={}.",
                 slot.label, area.rectangle.origin, area.rectangle.extent.width,
                 area.rectangle.extent.height, area.cursorOffset);
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
    std::println("{}.ClearTextInputArea succeeded.", slot.label);
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
    std::println("  live mouse grabbed={}", slot.window.IsMouseGrabbed());
}

void ToggleRelativeMouseMode(WindowSlot& slot)
{
    const bool target = !slot.window.IsRelativeMouseModeEnabled();
    auto result = slot.window.SetRelativeMouseMode(target);
    PrintOperationResult(slot.label + (target ? ".SetRelativeMouseMode(true)" :
                                               ".SetRelativeMouseMode(false)"),
                         result);
    std::println("  live relative mouse mode={}", slot.window.IsRelativeMouseModeEnabled());
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
    PrintQueryResult("PlatformRuntime::GetGlobalMousePosition", result);
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
    std::println("  live cursor visible={}", runtime.IsCursorVisible());
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
        std::println("Close request did not match an owned window.");
        return;
    }

    CleanupWindow(state.windows[index]);
    std::println("Released application-owned window id {}.", id);
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

void PrintEventHeader(std::string_view name, platform::Timestamp timestamp,
                      const AppState& state)
{
    std::print("[event {}] {} at {} (+{})", state.eventCount, name, timestamp,
               timestamp - state.startTimestamp);
}

struct EventVisitor final
{
    AppState& state;

    void operator()(const platform::QuitRequestedEvent& event) const
    {
        PrintEventHeader("QuitRequested", event.timestamp, state);
        std::println();
        state.quitRequested = true;
    }

    void operator()(const platform::WindowCloseRequestedEvent& event) const
    {
        PrintEventHeader("WindowCloseRequested", event.timestamp, state);
        std::println(" window={}", event.windowId);
        ReleaseWindow(state, event.windowId);
    }

    void operator()(const platform::WindowFocusChangedEvent& event) const
    {
        PrintEventHeader("WindowFocusChanged", event.timestamp, state);
        std::println(" window={} focused={}", event.windowId, event.focused);
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
        std::println(" window={}", event.windowId);
    }

    void operator()(const platform::WindowPointerLeftEvent& event) const
    {
        PrintEventHeader("WindowPointerLeft", event.timestamp, state);
        std::println(" window={}", event.windowId);
    }

    void operator()(const platform::KeyboardKeyEvent& event) const
    {
        PrintEventHeader("KeyboardKey", event.timestamp, state);
        std::println(" window={} physical={} logical={} modifiers={} pressed={} repeat={}",
                     OptionalWindowId{event.windowId}, ToString(event.physicalKey),
                     FormatLogicalKey(event.logicalKey), FormatModifiers(event.modifiers),
                     event.pressed, event.repeat);

        if (event.pressed && !event.repeat)
        {
            HandleCommand(state, event.physicalKey);
        }
    }

    void operator()(const platform::TextInputEvent& event) const
    {
        state.lastText = QuoteText(event.text);
        PrintEventHeader("TextInput", event.timestamp, state);
        std::println(" window={} text={}", OptionalWindowId{event.windowId}, state.lastText);
    }

    void operator()(const platform::TextCompositionEvent& event) const
    {
        PrintEventHeader("TextComposition", event.timestamp, state);
        std::println(" window={} text={} selection={}", OptionalWindowId{event.windowId},
                     QuoteText(event.text), FormatSelection(event.selection));
    }

    void operator()(const platform::MouseMotionEvent& event) const
    {
        PrintEventHeader("MouseMotion", event.timestamp, state);
        std::println(" window={} position={} relative={}", OptionalWindowId{event.windowId},
                     event.position, event.relativeMovement);
    }

    void operator()(const platform::MouseButtonEvent& event) const
    {
        PrintEventHeader("MouseButton", event.timestamp, state);
        std::println(" window={} position={} button={} pressed={}",
                     OptionalWindowId{event.windowId}, event.position, ToString(event.button),
                     event.pressed);
    }

    void operator()(const platform::MouseWheelEvent& event) const
    {
        PrintEventHeader("MouseWheel", event.timestamp, state);
        std::println(" window={} position={} horizontal={} vertical={}",
                     OptionalWindowId{event.windowId}, event.position, event.horizontal,
                     event.vertical);
    }

    void operator()(const platform::DropBeginEvent& event) const
    {
        state.lastDrop = "begin";
        PrintEventHeader("DropBegin", event.timestamp, state);
        std::println(" window={} source={}", OptionalWindowId{event.windowId},
                     FormatSourceApplication(event.sourceApplication));
    }

    void operator()(const platform::DroppedFileEvent& event) const
    {
        const std::string path = io::PathToUtf8(event.path);
        state.lastDrop = "file " + path;
        PrintEventHeader("DroppedFile", event.timestamp, state);
        std::println(" window={} position={} path={} source={}",
                     OptionalWindowId{event.windowId}, event.position, QuoteText(path),
                     FormatSourceApplication(event.sourceApplication));
    }

    void operator()(const platform::DroppedTextEvent& event) const
    {
        state.lastDrop = "text " + QuoteText(event.text);
        PrintEventHeader("DroppedText", event.timestamp, state);
        std::println(" window={} position={} text={} source={}",
                     OptionalWindowId{event.windowId}, event.position, QuoteText(event.text),
                     FormatSourceApplication(event.sourceApplication));
    }

    void operator()(const platform::DropPositionEvent& event) const
    {
        state.lastDrop = std::format("position {}", event.position);
        PrintEventHeader("DropPosition", event.timestamp, state);
        std::println(" window={} position={} source={}", OptionalWindowId{event.windowId},
                     event.position, FormatSourceApplication(event.sourceApplication));
    }

    void operator()(const platform::DropCompleteEvent& event) const
    {
        state.lastDrop = std::format("complete {}", event.position);
        PrintEventHeader("DropComplete", event.timestamp, state);
        std::println(" window={} position={} source={}", OptionalWindowId{event.windowId},
                     event.position, FormatSourceApplication(event.sourceApplication));
    }

    template <typename Event>
    void operator()(const Event& event) const
    {
        PrintEventHeader("Other platform event", event.timestamp, state);
        std::println(" observed by another example in the suite.");
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
    RETURN_ERROR_IF_FAILED(optionsResult);

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
    RETURN_ERROR_IF_FAILED(runtimeResult);

    platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    const platform::Timestamp start = runtime.Now();

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
    RETURN_ERROR_IF_FAILED(primary);
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
    RETURN_ERROR_IF_FAILED(secondary);
    windows.push_back(std::move(secondary).GetValue());

    AppState state{.runtime = runtime, .windows = windows, .startTimestamp = start};
    SetActiveWindow(state, 0);
    PrintUsage(argc > 0 ? argv[0] : "ponder-platform-2-input-drop-monitor");

    auto nextTitleUpdate = start;
    while (!state.quitRequested && !state.windows.empty())
    {
        DrainEvents(state);

        const platform::Timestamp now = runtime.Now();
        if (now - nextTitleUpdate >= std::chrono::milliseconds{250})
        {
            UpdateWindowTitles(state);
            nextTitleUpdate = now;
        }

        if (options.autoCloseAfter && now - start >= *options.autoCloseAfter)
        {
            std::println("Auto-close duration reached after {}.", now - start);
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
    try
    {
        const auto result = RunInputDropMonitor(argc, argv);
        if (!result)
        {
            std::println(stderr, "ponder-platform-2-input-drop-monitor failed: {}",
                         result.GetError());
            return 1;
        }
    }
    catch (const std::exception& exception)
    {
        std::println(stderr,
                     "ponder-platform-2-input-drop-monitor terminated with an exception: {}",
                     exception.what());
        return 1;
    }

    return 0;
}
