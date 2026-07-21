#include "SdlRuntimeBackend.hpp"

#include <ponder/core/Log.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/platform/PlatformError.hpp>

#include "SdlCommon.hpp"
#include "SdlError.hpp"

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_misc.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <exception>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pond::platform::detail
{
namespace
{
constexpr core::ErrorCode kBackendFailureCode =
    ToErrorCode(PlatformErrorCode::BackendFailure);

[[nodiscard]] const char* GetMetadataPropertyName(
    ApplicationMetadataProperty property) noexcept
{
    switch (property)
    {
    case ApplicationMetadataProperty::Name:
        return SDL_PROP_APP_METADATA_NAME_STRING;
    case ApplicationMetadataProperty::Version:
        return SDL_PROP_APP_METADATA_VERSION_STRING;
    case ApplicationMetadataProperty::Identifier:
        return SDL_PROP_APP_METADATA_IDENTIFIER_STRING;
    }

    return nullptr;
}

[[nodiscard]] SDL_SystemCursor ToSdlSystemCursor(SystemCursorShape shape) noexcept
{
    switch (shape)
    {
    case SystemCursorShape::Default:
        return SDL_SYSTEM_CURSOR_DEFAULT;
    case SystemCursorShape::TextInput:
        return SDL_SYSTEM_CURSOR_TEXT;
    case SystemCursorShape::Move:
        return SDL_SYSTEM_CURSOR_MOVE;
    case SystemCursorShape::ResizeNorthSouth:
        return SDL_SYSTEM_CURSOR_NS_RESIZE;
    case SystemCursorShape::ResizeEastWest:
        return SDL_SYSTEM_CURSOR_EW_RESIZE;
    case SystemCursorShape::ResizeNortheastSouthwest:
        return SDL_SYSTEM_CURSOR_NESW_RESIZE;
    case SystemCursorShape::ResizeNorthwestSoutheast:
        return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
    case SystemCursorShape::Pointer:
        return SDL_SYSTEM_CURSOR_POINTER;
    case SystemCursorShape::Wait:
        return SDL_SYSTEM_CURSOR_WAIT;
    case SystemCursorShape::Progress:
        return SDL_SYSTEM_CURSOR_PROGRESS;
    case SystemCursorShape::NotAllowed:
        return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    }

    LOG_ERROR_CATEGORY("platform", "Unrecognized system cursor shape value {}; using default",
                       static_cast<unsigned int>(shape));
    return SDL_SYSTEM_CURSOR_DEFAULT;
}

[[nodiscard]] CursorHandle ToCursorHandle(SDL_Cursor* cursor) noexcept
{
    return CursorHandle{reinterpret_cast<CursorHandle::ValueType>(cursor)};
}

[[nodiscard]] SDL_Cursor* ToSdlCursor(CursorHandle cursor) noexcept
{
    return reinterpret_cast<SDL_Cursor*>(cursor.GetValue());
}

class SdlDialogContext final
{
public:
    explicit SdlDialogContext(const BackendDialogRequestDesc& desc) : callback(desc.callback)
    {
        if (desc.defaultLocation.has_value())
        {
            defaultLocation.emplace(*desc.defaultLocation);
        }

        filters.assign(desc.filters.begin(), desc.filters.end());
        nativeFilters.reserve(filters.size());
        for (const BackendDialogFileFilter& filter : filters)
        {
            nativeFilters.push_back(SDL_DialogFileFilter{.name = filter.name.c_str(),
                                                             .pattern = filter.pattern.c_str()});
        }
    }

    IBackendDialogCallback& callback;
    std::optional<std::string> defaultLocation;
    std::vector<BackendDialogFileFilter> filters;
    std::vector<SDL_DialogFileFilter> nativeFilters;
};

[[nodiscard]] BackendDialogOutcome MakeDialogOutcome(const char* const* fileList,
                                                      int selectedFilter)
{
    if (fileList == nullptr)
    {
        const char* const rawError = SDL_GetError();
        return BackendDialogFailure{
            .message = rawError != nullptr ? std::string{rawError} : std::string{}};
    }

    if (fileList[0] == nullptr)
    {
        return BackendDialogCancellation{};
    }

    BackendDialogSelection selection;
    for (std::size_t index = 0; fileList[index] != nullptr; ++index)
    {
        selection.paths.emplace_back(fileList[index]);
    }
    if (selectedFilter >= 0)
    {
        selection.selectedFilterIndex = static_cast<std::size_t>(selectedFilter);
    }
    return selection;
}

void SDLCALL OnDialogCompleted(void* userdata, const char* const* fileList,
                               int selectedFilter) noexcept
{
    std::unique_ptr<SdlDialogContext> context{static_cast<SdlDialogContext*>(userdata)};
    if (context == nullptr)
    {
        return;
    }

    try
    {
        context->callback.OnDialogCompleted(MakeDialogOutcome(fileList, selectedFilter));
    }
    catch (...)
    {
        context->callback.OnDialogCallbackFailure();
    }
}
} // namespace

bool SdlHintBackend::IsMainThread() const noexcept
{
    return SDL_IsMainThread();
}

bool SdlHintBackend::HasInitializedSubsystems() const noexcept
{
    return SDL_WasInit(0) != 0;
}

const char* SdlHintBackend::GetHint(const char* name) const noexcept
{
    return SDL_GetHint(name);
}

core::VoidResult SdlHintBackend::SetHintOverride(const char* name, const char* value)
{
    if (!SDL_SetHintWithPriority(name, value, SDL_HINT_OVERRIDE))
    {
        return core::VoidResult::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_SetHintWithPriority",
            name != nullptr ? std::string_view{name} : std::string_view{"null hint name"}));
    }

    return core::VoidResult::Success();
}

core::VoidResult SdlHintBackend::ResetHint(const char* name)
{
    if (!SDL_ResetHint(name))
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_ResetHint",
                              name != nullptr ? std::string_view{name}
                                              : std::string_view{"null hint name"}));
    }

    return core::VoidResult::Success();
}

SdlRuntimeBackend::SdlRuntimeBackend()
    : m_hintManager(HintManagerAccess::Create(m_hintBackend))
{
}

HintManager& SdlRuntimeBackend::GetHintManager() noexcept
{
    return m_hintManager;
}

bool SdlRuntimeBackend::IsMainThread() noexcept
{
    return SDL_IsMainThread();
}

bool SdlRuntimeBackend::HasInitializedSubsystems() noexcept
{
    return SDL_WasInit(0) != 0;
}

bool SdlRuntimeBackend::HasExpectedRuntimeSubsystems() noexcept
{
    constexpr SDL_InitFlags kRuntimeSubsystems = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
    const SDL_InitFlags initializedSubsystems = SDL_WasInit(0);
    return (initializedSubsystems & kRuntimeSubsystems) == kRuntimeSubsystems &&
           (initializedSubsystems & ~kRuntimeSubsystems) == 0;
}

core::VoidResult SdlRuntimeBackend::SetAppMetadataProperty(
    ApplicationMetadataProperty property, const char* value)
{
    using ResultType = core::VoidResult;

    const char* const propertyName = GetMetadataPropertyName(property);
    if (propertyName == nullptr)
    {
        return ResultType::FromError(core::Error{kBackendFailureCode,
            std::format("Application metadata property {} is not recognized.",
                        static_cast<unsigned int>(property))});
    }

    if (!SDL_SetAppMetadataProperty(propertyName, value))
    {
        return ResultType::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_SetAppMetadataProperty", std::format("{}", property)));
    }

    return ResultType::Success();
}

core::VoidResult SdlRuntimeBackend::InitializeVideo()
{
    using ResultType = core::VoidResult;

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return ResultType::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_Init", "SDL_INIT_VIDEO"));
    }

    return ResultType::Success();
}

void SdlRuntimeBackend::Quit() noexcept
{
    SDL_Quit();
}

std::uint64_t SdlRuntimeBackend::GetTicksNanoseconds() noexcept
{
    return SDL_GetTicksNS();
}

bool SdlRuntimeBackend::PollEvent(SDL_Event* event) noexcept
{
    return SDL_PollEvent(event);
}

bool SdlRuntimeBackend::SupportsGlobalMouse() noexcept
{
    const char* const driver = SDL_GetCurrentVideoDriver();
    if (driver == nullptr)
    {
        return false;
    }

    const std::string_view name{driver};
    return name == "windows" || name == "cocoa" || name == "x11";
}

MousePosition SdlRuntimeBackend::GetGlobalMousePosition() noexcept
{
    MousePosition position;
    static_cast<void>(SDL_GetGlobalMouseState(&position.x, &position.y));
    return position;
}

core::VoidResult SdlRuntimeBackend::SetMouseCapture(bool enabled)
{
    if (!SDL_CaptureMouse(enabled))
    {
        return core::VoidResult::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_CaptureMouse", enabled ? "enable" : "disable"));
    }

    return core::VoidResult::Success();
}

core::Result<CursorHandle> SdlRuntimeBackend::CreateSystemCursor(SystemCursorShape shape)
{
    SDL_Cursor* const cursor = SDL_CreateSystemCursor(ToSdlSystemCursor(shape));
    if (cursor == nullptr)
    {
        return core::Result<CursorHandle>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_CreateSystemCursor", std::format("{}", shape)));
    }

    return ToCursorHandle(cursor);
}

core::VoidResult SdlRuntimeBackend::SetCursor(CursorHandle cursor)
{
    if (!SDL_SetCursor(ToSdlCursor(cursor)))
    {
        return core::VoidResult::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_SetCursor", std::format("cursor {}", cursor)));
    }

    return core::VoidResult::Success();
}

void SdlRuntimeBackend::DestroyCursor(CursorHandle cursor) noexcept
{
    SDL_DestroyCursor(ToSdlCursor(cursor));
}

core::VoidResult SdlRuntimeBackend::ShowCursor()
{
    if (!SDL_ShowCursor())
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_ShowCursor"));
    }

    return core::VoidResult::Success();
}

core::VoidResult SdlRuntimeBackend::HideCursor()
{
    if (!SDL_HideCursor())
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_HideCursor"));
    }

    return core::VoidResult::Success();
}

bool SdlRuntimeBackend::IsCursorVisible() noexcept
{
    return SDL_CursorVisible();
}

bool SdlRuntimeBackend::SupportsClipboardText() noexcept
{
    return true;
}

core::Result<std::string> SdlRuntimeBackend::GetClipboardText()
{
    static_cast<void>(SDL_ClearError());
    char* const rawText = SDL_GetClipboardText();
    [[maybe_unused]] auto freeText = core::MakeScopeExit(
        [rawText]() noexcept
        {
            SDL_free(rawText);
        });

    const char* const rawError = SDL_GetError();
    const std::string errorText = rawError != nullptr ? std::string{rawError} : std::string{};
    if (rawText == nullptr || (rawText[0] == '\0' && !errorText.empty()))
    {
        return core::Result<std::string>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetClipboardText", "clipboard text", errorText));
    }

    return std::string{rawText};
}

core::VoidResult SdlRuntimeBackend::SetClipboardText(std::string_view text)
{
    const std::string ownedText{text};
    if (!SDL_SetClipboardText(ownedText.c_str()))
    {
        return core::VoidResult::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_SetClipboardText", "clipboard text"));
    }

    return core::VoidResult::Success();
}

core::VoidResult SdlRuntimeBackend::OpenExternalUri(std::string_view uri)
{
    const std::string ownedUri{uri};
    if (!SDL_OpenURL(ownedUri.c_str()))
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_OpenURL", "external URI"));
    }

    return core::VoidResult::Success();
}

void SdlRuntimeBackend::ShowDialog(const BackendDialogRequestDesc& desc) noexcept
{
    try
    {
        switch (desc.kind)
        {
        case BackendDialogKind::OpenFile:
        case BackendDialogKind::SaveFile:
        case BackendDialogKind::OpenFolder:
            break;
        default:
            desc.callback.OnDialogCallbackFailure();
            return;
        }

        auto context = std::make_unique<SdlDialogContext>(desc);
        SDL_Window* const parentWindow =
            desc.parentWindow.has_value() ? ToSdlWindow(*desc.parentWindow) : nullptr;
        const SDL_DialogFileFilter* const filters =
            context->nativeFilters.empty() ? nullptr : context->nativeFilters.data();
        const int filterCount = static_cast<int>(context->nativeFilters.size());
        const char* const defaultLocation = context->defaultLocation.has_value()
                                                ? context->defaultLocation->c_str()
                                                : nullptr;
        SdlDialogContext* const rawContext = context.release();

        switch (desc.kind)
        {
        case BackendDialogKind::OpenFile:
            SDL_ShowOpenFileDialog(OnDialogCompleted, rawContext, parentWindow, filters,
                                   filterCount, defaultLocation, desc.allowMultipleSelection);
            break;
        case BackendDialogKind::SaveFile:
            SDL_ShowSaveFileDialog(OnDialogCompleted, rawContext, parentWindow, filters,
                                   filterCount, defaultLocation);
            break;
        case BackendDialogKind::OpenFolder:
            SDL_ShowOpenFolderDialog(OnDialogCompleted, rawContext, parentWindow, defaultLocation,
                                     desc.allowMultipleSelection);
            break;
        default:
            std::terminate();
        }
    }
    catch (...)
    {
        desc.callback.OnDialogCallbackFailure();
    }
}
} // namespace pond::platform::detail
