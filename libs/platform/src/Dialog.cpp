#include <ponder/core/Assert.hpp>
#include <ponder/core/String.hpp>
#include <ponder/io/Path.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <SDL3/SDL_error.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "PlatformRuntimeState.hpp"
#include "WindowImpl.hpp"

namespace pond::platform
{
namespace
{
constexpr core::ErrorCode kInvalidArgumentCode = ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kBackendFailureCode = ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr core::ErrorCode kNotFoundCode = ToErrorCode(PlatformErrorCode::NotFound);

[[nodiscard]] bool IsValidFilterPatternCharacter(char character) noexcept
{
    const auto value = static_cast<unsigned char>(character);
    return std::isalnum(value) != 0 || character == '-' || character == '_' || character == '.';
}

[[nodiscard]] bool IsValidFilterPattern(std::string_view pattern) noexcept
{
    if (pattern == "*")
    {
        return true;
    }

    bool segmentHasCharacters{};
    for (const char character : pattern)
    {
        if (character == ';')
        {
            if (!segmentHasCharacters)
            {
                return false;
            }
            segmentHasCharacters = false;
            continue;
        }

        if (!IsValidFilterPatternCharacter(character))
        {
            return false;
        }
        segmentHasCharacters = true;
    }

    return segmentHasCharacters;
}

[[nodiscard]] core::Result<std::optional<std::string>> ValidateDefaultLocation(
    const std::optional<std::filesystem::path>& location)
{
    if (!location.has_value())
    {
        return std::optional<std::string>{};
    }

    std::string text = io::PathToUtf8(*location);
    if (text.empty() || !core::IsValidUtf8WithoutEmbeddedNull(text))
    {
        return core::Result<std::optional<std::string>>::FromError(core::Error{
            kInvalidArgumentCode,
            "Dialog default location must be absent or non-empty UTF-8 without embedded nulls."});
    }

    return std::optional<std::string>{std::move(text)};
}

[[nodiscard]] core::VoidResult ValidateFilter(const DialogFileFilter& filter, std::size_t index)
{
    if (filter.name.empty() || !core::IsValidUtf8WithoutEmbeddedNull(filter.name))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode, "Dialog filter " + std::to_string(index) +
                                      " name must be non-empty UTF-8 without embedded nulls."});
    }

    if (filter.pattern.empty() || !core::IsValidUtf8WithoutEmbeddedNull(filter.pattern) ||
        !IsValidFilterPattern(filter.pattern))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode,
            "Dialog filter " + std::to_string(index) +
                " pattern must be '*', or a semicolon-separated list of ASCII file extensions."});
    }

    return core::VoidResult::Success();
}

struct PreparedDialogRequest final
{
    detail::BackendDialogKind kind{detail::BackendDialogKind::OpenFile};
    std::optional<WindowId> parentWindowId;
    std::optional<std::string> defaultLocation;
    std::vector<std::string> filterNames;
    std::vector<std::string> filterPatterns;
    std::vector<SDL_DialogFileFilter> filters;
    bool allowMultipleSelection{};
};

[[nodiscard]] core::Result<PreparedDialogRequest> PrepareDialogRequest(
    detail::BackendDialogKind kind, std::optional<WindowId> parentWindowId,
    const std::optional<std::filesystem::path>& defaultLocation,
    std::span<const DialogFileFilter> filters, bool allowMultipleSelection)
{
    if (parentWindowId.has_value() && !parentWindowId->IsValid())
    {
        return core::Result<PreparedDialogRequest>::FromError(
            core::Error{kInvalidArgumentCode, "Dialog parent window ID must be absent or valid."});
    }

    if (filters.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return core::Result<PreparedDialogRequest>::FromError(core::Error{
            kInvalidArgumentCode, "Dialog filter count exceeds the backend representation range."});
    }

    auto locationResult = ValidateDefaultLocation(defaultLocation);
    if (!locationResult.HasValue())
    {
        return core::Result<PreparedDialogRequest>::FromError(std::move(locationResult).GetError());
    }

    PreparedDialogRequest prepared{.kind = kind,
                                   .parentWindowId = parentWindowId,
                                   .defaultLocation = std::move(locationResult).GetValue(),
                                   .allowMultipleSelection = allowMultipleSelection};
    prepared.filterNames.reserve(filters.size());
    prepared.filterPatterns.reserve(filters.size());
    prepared.filters.reserve(filters.size());

    for (std::size_t index = 0; index < filters.size(); ++index)
    {
        core::VoidResult validation = ValidateFilter(filters[index], index);
        if (!validation.HasValue())
        {
            return core::Result<PreparedDialogRequest>::FromError(std::move(validation).GetError());
        }

        prepared.filterNames.push_back(filters[index].name);
        prepared.filterPatterns.push_back(filters[index].pattern);
    }

    for (std::size_t index = 0; index < filters.size(); ++index)
    {
        prepared.filters.push_back(
            SDL_DialogFileFilter{.name = prepared.filterNames[index].c_str(),
                                 .pattern = prepared.filterPatterns[index].c_str()});
    }

    return prepared;
}

[[nodiscard]] std::string_view GetDialogOperation(detail::BackendDialogKind kind) noexcept
{
    switch (kind)
    {
    case detail::BackendDialogKind::OpenFile:
        return "SDL_ShowOpenFileDialog";
    case detail::BackendDialogKind::SaveFile:
        return "SDL_ShowSaveFileDialog";
    case detail::BackendDialogKind::OpenFolder:
        return "SDL_ShowOpenFolderDialog";
    }

    return "SDL_ShowFileDialog";
}

[[nodiscard]] DialogFailure MakeDialogFailure(detail::BackendDialogKind kind,
                                              std::string_view message)
{
    std::string text{GetDialogOperation(kind)};
    text += " failed for dialog request";
    if (!message.empty())
    {
        text += ": ";
        text += message;
    }
    else
    {
        text += ".";
    }

    return DialogFailure{core::Error{kBackendFailureCode, std::move(text)}};
}

[[nodiscard]] DialogOutcome CopyDialogOutcome(const detail::DialogRequestState& request,
                                              const char* const* fileList, int selectedFilter);
} // namespace

namespace detail
{
struct DialogRequestState final
{
    PlatformRuntimeState* runtime{};
    DialogRequestId id;
    BackendDialogKind kind{BackendDialogKind::OpenFile};
    std::optional<WindowId> parentWindowId;
    WindowImpl* parentWindow{};
    std::optional<std::string> defaultLocation;
    std::vector<std::string> filterNames;
    std::vector<std::string> filterPatterns;
    std::vector<SDL_DialogFileFilter> filters;
    bool allowMultipleSelection{};
    bool completionEnqueued{};
};
} // namespace detail

namespace
{
[[nodiscard]] DialogOutcome CopyDialogOutcome(const detail::DialogRequestState& request,
                                              const char* const* fileList, int selectedFilter)
{
    if (fileList == nullptr)
    {
        const char* const rawError = SDL_GetError();
        return MakeDialogFailure(request.kind, rawError != nullptr ? std::string_view{rawError}
                                                                   : std::string_view{});
    }

    if (fileList[0] == nullptr)
    {
        return DialogCancellation{};
    }

    std::vector<std::filesystem::path> paths;
    for (std::size_t index = 0; fileList[index] != nullptr; ++index)
    {
        const std::string pathText{fileList[index]};
        if (!core::IsValidUtf8WithoutEmbeddedNull(pathText))
        {
            return MakeDialogFailure(
                request.kind, "The backend returned a dialog path that was not valid UTF-8.");
        }
        paths.push_back(io::PathFromUtf8(pathText));
    }

    if (paths.empty())
    {
        return DialogCancellation{};
    }

    std::optional<std::size_t> selectedFilterIndex;
    if (selectedFilter >= 0)
    {
        const auto index = static_cast<std::size_t>(selectedFilter);
        if (index >= request.filters.size())
        {
            return MakeDialogFailure(
                request.kind, "The backend returned an out-of-range selected dialog filter index.");
        }
        selectedFilterIndex = index;
    }

    return DialogSelection{.paths = std::move(paths), .selectedFilterIndex = selectedFilterIndex};
}

void SDLCALL OnDialogCompleted(void* userdata, const char* const* fileList, int selectedFilter)
{
    auto* const rawRequest = static_cast<detail::DialogRequestState*>(userdata);
    if (rawRequest == nullptr || rawRequest->runtime == nullptr)
    {
        return;
    }

    std::shared_ptr<detail::DialogRequestState> request =
        rawRequest->runtime->AcquireDialogRequest(rawRequest->id);
    if (request == nullptr)
    {
        return;
    }

    DialogOutcome outcome = DialogFailure{
        core::Error{kBackendFailureCode, "Dialog callback failed before copying a result."}};
    try
    {
        outcome = CopyDialogOutcome(*request, fileList, selectedFilter);
    }
    catch (const std::exception& exception)
    {
        outcome = MakeDialogFailure(request->kind, exception.what());
    }
    catch (...)
    {
        outcome = MakeDialogFailure(request->kind, "Unknown dialog callback failure.");
    }

    request->runtime->EnqueueDialogCompletion(request->id, std::move(outcome));
}
} // namespace

namespace detail
{
core::Result<DialogRequestId> PlatformRuntimeState::ShowDialog(
    BackendDialogKind kind, std::optional<WindowId> parentWindowId,
    const std::optional<std::filesystem::path>& defaultLocation,
    std::span<const DialogFileFilter> filters, bool allowMultipleSelection)
{
    VerifyOwnerThread("dialog request");

    auto preparedResult = PrepareDialogRequest(kind, parentWindowId, defaultLocation, filters,
                                               allowMultipleSelection);
    if (!preparedResult.HasValue())
    {
        return core::Result<DialogRequestId>::FromError(std::move(preparedResult).GetError());
    }

    PreparedDialogRequest prepared = std::move(preparedResult).GetValue();
    WindowImpl* parentWindow{};
    void* parentNativeWindow{};
    if (prepared.parentWindowId.has_value())
    {
        const auto parent = std::ranges::find_if(m_windowsByBackendId,
                                                 [id = *prepared.parentWindowId](const auto& entry)
                                                 {
                                                     return entry.second.id == id;
                                                 });
        if (parent == m_windowsByBackendId.end())
        {
            return core::Result<DialogRequestId>::FromError(
                core::Error{kNotFoundCode, "Dialog parent window was not found."});
        }

        parentWindow = parent->second.window;
        PONDER_VERIFY(parentWindow != nullptr, "Dialog parent window record has no owning window");
        parentWindow->VerifyUsable("dialog parent lookup");
        parentNativeWindow = parentWindow->m_nativeWindow;
    }

    PONDER_VERIFY(m_nextDialogRequestId != 0, "Platform dialog request ID space is exhausted");
    const DialogRequestId id{m_nextDialogRequestId};

    auto request = std::make_shared<DialogRequestState>();
    request->runtime = this;
    request->id = id;
    request->kind = prepared.kind;
    request->parentWindowId = prepared.parentWindowId;
    request->parentWindow = parentWindow;
    request->defaultLocation = std::move(prepared.defaultLocation);
    request->filterNames = std::move(prepared.filterNames);
    request->filterPatterns = std::move(prepared.filterPatterns);
    request->filters.reserve(request->filterNames.size());
    for (std::size_t index = 0; index < request->filterNames.size(); ++index)
    {
        request->filters.push_back(
            SDL_DialogFileFilter{.name = request->filterNames[index].c_str(),
                                 .pattern = request->filterPatterns[index].c_str()});
    }
    request->allowMultipleSelection = prepared.allowMultipleSelection;

    {
        std::scoped_lock lock{m_dialogMutex};
        const auto [iterator, inserted] = m_dialogRequests.emplace(id, request);
        static_cast<void>(iterator);
        PONDER_VERIFY(inserted, "Dialog request ID {} is already registered", id.GetValue());
    }

    bool requestRegistered{};
    bool parentCounted{};
    try
    {
        RegisterRequest(request.get());
        requestRegistered = true;
        if (parentWindow != nullptr)
        {
            parentWindow->IncrementPendingDialogRequestCount();
            parentCounted = true;
        }
    }
    catch (...)
    {
        std::scoped_lock lock{m_dialogMutex};
        m_dialogRequests.erase(id);
        if (parentCounted)
        {
            parentWindow->DecrementPendingDialogRequestCount();
        }
        throw;
    }

    static_cast<void>(requestRegistered);

    const BackendDialogRequestDesc backendDesc{
        .kind = request->kind,
        .callback = OnDialogCompleted,
        .userdata = request.get(),
        .parentWindow = parentNativeWindow,
        .filters = request->filters.empty() ? nullptr : request->filters.data(),
        .filterCount = static_cast<int>(request->filters.size()),
        .defaultLocation =
            request->defaultLocation.has_value() ? request->defaultLocation->c_str() : nullptr,
        .allowMultipleSelection = request->allowMultipleSelection};
    m_backend.showDialog(m_backend.context, backendDesc);

    ++m_nextDialogRequestId;
    return id;
}

core::Result<DialogRequestId> PlatformRuntimeState::ShowOpenFileDialog(
    const OpenFileDialogDesc& desc)
{
    return ShowDialog(BackendDialogKind::OpenFile, desc.parentWindowId, desc.defaultLocation,
                      desc.filters, desc.allowMultipleSelection);
}

core::Result<DialogRequestId> PlatformRuntimeState::ShowSaveFileDialog(
    const SaveFileDialogDesc& desc)
{
    return ShowDialog(BackendDialogKind::SaveFile, desc.parentWindowId, desc.defaultLocation,
                      desc.filters, false);
}

core::Result<DialogRequestId> PlatformRuntimeState::ShowOpenFolderDialog(
    const OpenFolderDialogDesc& desc)
{
    return ShowDialog(BackendDialogKind::OpenFolder, desc.parentWindowId, desc.defaultLocation, {},
                      desc.allowMultipleSelection);
}

std::shared_ptr<DialogRequestState> PlatformRuntimeState::AcquireDialogRequest(
    DialogRequestId id) noexcept
{
    std::scoped_lock lock{m_dialogMutex};
    const auto request = m_dialogRequests.find(id);
    return request != m_dialogRequests.end() ? request->second : nullptr;
}

void PlatformRuntimeState::EnqueueDialogCompletion(DialogRequestId id,
                                                   DialogOutcome outcome) noexcept
{
    std::scoped_lock lock{m_dialogMutex};
    const auto request = m_dialogRequests.find(id);
    if (request == m_dialogRequests.end() || request->second->completionEnqueued)
    {
        return;
    }

    request->second->completionEnqueued = true;
    m_dialogCompletions.push_back(
        DialogCompletionRecord{.requestId = id, .outcome = std::move(outcome)});
}

std::optional<PlatformEvent> PlatformRuntimeState::PollDialogCompletion()
{
    VerifyOwnerThread("dialog completion polling");

    std::optional<DialogCompletionRecord> completion;
    std::shared_ptr<DialogRequestState> request;
    {
        std::scoped_lock lock{m_dialogMutex};
        if (m_dialogCompletions.empty())
        {
            return std::nullopt;
        }

        completion = std::move(m_dialogCompletions.front());
        m_dialogCompletions.pop_front();

        const auto requestIterator = m_dialogRequests.find(completion->requestId);
        PONDER_VERIFY(requestIterator != m_dialogRequests.end(),
                      "Dialog request {} completed after it was unregistered",
                      completion->requestId.GetValue());
        request = std::move(requestIterator->second);
        m_dialogRequests.erase(requestIterator);
    }

    if (request->parentWindow != nullptr)
    {
        request->parentWindow->DecrementPendingDialogRequestCount();
    }
    UnregisterRequest(request.get());

    return PlatformEvent{DialogCompletedEvent{.timestamp = Now(),
                                              .requestId = completion->requestId,
                                              .outcome = std::move(completion->outcome)}};
}
} // namespace detail

core::Result<DialogRequestId> PlatformRuntime::ShowOpenFileDialog(const OpenFileDialogDesc& desc)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->ShowOpenFileDialog(desc);
}

core::Result<DialogRequestId> PlatformRuntime::ShowSaveFileDialog(const SaveFileDialogDesc& desc)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->ShowSaveFileDialog(desc);
}

core::Result<DialogRequestId> PlatformRuntime::ShowOpenFolderDialog(
    const OpenFolderDialogDesc& desc)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");
    return m_state->ShowOpenFolderDialog(desc);
}
} // namespace pond::platform
