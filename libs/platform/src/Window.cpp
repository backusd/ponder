#include <ponder/platform/Window.hpp>

#include "PlatformRuntimeBackend.hpp"
#include "PlatformRuntimeState.hpp"
#include "SdlError.hpp"
#include "StringValidation.hpp"
#include "WindowImpl.hpp"

#include <ponder/core/Assert.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace pond::platform
{
namespace
{
constexpr core::ErrorCode kInvalidArgumentCode =
    ToErrorCode(PlatformErrorCode::InvalidArgument);
constexpr core::ErrorCode kBackendFailureCode =
    ToErrorCode(PlatformErrorCode::BackendFailure);
constexpr core::ErrorCode kUnsupportedCode =
    ToErrorCode(PlatformErrorCode::Unsupported);

[[nodiscard]] core::VoidResult ValidatePositiveSize(LogicalSize size,
                                                    std::string_view context)
{
    constexpr auto kMaximumBackendDimension =
        static_cast<std::uint32_t>(std::numeric_limits<int>::max());
    if (size.width == 0 || size.height == 0 || size.width > kMaximumBackendDimension ||
        size.height > kMaximumBackendDimension)
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode,
            std::string{context} +
                " dimensions must be positive and representable by the backend."});
    }

    return core::VoidResult::Success();
}

[[nodiscard]] bool IsKnownGraphicsCompatibility(
    WindowGraphicsCompatibility compatibility) noexcept
{
    switch (compatibility)
    {
    case WindowGraphicsCompatibility::Default:
    case WindowGraphicsCompatibility::Vulkan:
        return true;
    }

    return false;
}

[[nodiscard]] core::VoidResult Validate(const WindowDesc& desc)
{
    if (!detail::IsValidUtf8WithoutEmbeddedNull(desc.title))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode,
            "Window title must be UTF-8 without embedded nulls."});
    }

    core::VoidResult sizeValidation =
        ValidatePositiveSize(desc.logicalSize, "Window logical size");
    if (!sizeValidation.HasValue())
    {
        return sizeValidation;
    }

    if (desc.minimumLogicalSize.has_value())
    {
        core::VoidResult minimumValidation =
            ValidatePositiveSize(*desc.minimumLogicalSize,
                                 "Window minimum logical size");
        if (!minimumValidation.HasValue())
        {
            return minimumValidation;
        }
    }

    if (!IsKnownGraphicsCompatibility(desc.graphicsCompatibility))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode, "Window graphics compatibility is invalid."});
    }

    if (!detail::IsWindowGraphicsCompatibilitySupported(desc.graphicsCompatibility))
    {
        return core::VoidResult::FromError(core::Error{
            kUnsupportedCode,
            "Window graphics compatibility is unsupported on this host."});
    }

    return core::VoidResult::Success();
}

[[nodiscard]] core::VoidResult ValidatePosition(ScreenPosition position)
{
    if (detail::IsReservedSdlWindowPosition(position.x) ||
        detail::IsReservedSdlWindowPosition(position.y))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode,
            "Window position collides with a backend-reserved position value."});
    }

    return core::VoidResult::Success();
}

[[nodiscard]] core::Error MakeInvalidBackendSizeError(std::string_view operation,
                                                      std::string_view context)
{
    return core::Error{
        kBackendFailureCode,
        std::string{operation} + " returned a negative size for " +
            std::string{context} + "."};
}
} // namespace

namespace detail
{
core::Result<std::unique_ptr<WindowImpl>> WindowImpl::Create(
    PlatformRuntimeState& runtime, const WindowDesc& desc)
{
    runtime.VerifyOwnerThread("window creation");

    core::VoidResult validation = Validate(desc);
    if (!validation.HasValue())
    {
        return core::Result<std::unique_ptr<WindowImpl>>::FromError(
            std::move(validation).GetError());
    }

    const PlatformWindowBackend backend = runtime.m_windowBackend;
    const BackendWindowCreateDesc backendDesc{
        desc.title.c_str(),
        static_cast<int>(desc.logicalSize.width),
        static_cast<int>(desc.logicalSize.height),
        desc.resizable,
        desc.highPixelDensity,
        desc.graphicsCompatibility};

    void* const nativeWindow = backend.create(backend.context, backendDesc);
    if (nativeWindow == nullptr)
    {
        return core::Result<std::unique_ptr<WindowImpl>>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_CreateWindow", "window"));
    }

    auto destroyNativeWindow = core::MakeScopeExit(
        [backend, nativeWindow]() noexcept
        {
            backend.destroy(backend.context, nativeWindow);
        });

    if (desc.minimumLogicalSize.has_value() &&
        !backend.setMinimumSize(
            backend.context, nativeWindow,
            static_cast<int>(desc.minimumLogicalSize->width),
            static_cast<int>(desc.minimumLogicalSize->height)))
    {
        return core::Result<std::unique_ptr<WindowImpl>>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_SetWindowMinimumSize",
                              "window creation"));
    }

    const std::uint32_t backendWindowId =
        backend.getId(backend.context, nativeWindow);
    if (backendWindowId == 0)
    {
        return core::Result<std::unique_ptr<WindowImpl>>::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_GetWindowID",
                              "window creation"));
    }

    auto state = std::make_unique<WindowImpl>(
        runtime, backend, nativeWindow, backendWindowId,
        desc.graphicsCompatibility);
    destroyNativeWindow.Dismiss();

    const WindowId id = runtime.RegisterWindow(state.get(), backendWindowId);
    state->CommitRegistration(id);

    if (desc.visible && !backend.show(backend.context, nativeWindow))
    {
        core::Error error = CaptureSdlFailure(
            kBackendFailureCode, "SDL_ShowWindow", "window creation");
        return core::Result<std::unique_ptr<WindowImpl>>::FromError(
            std::move(error));
    }

    return core::Result<std::unique_ptr<WindowImpl>>::FromValue(std::move(state));
}

WindowImpl::WindowImpl(PlatformRuntimeState& runtime, PlatformWindowBackend backend,
                       void* nativeWindow, std::uint32_t backendWindowId,
                       WindowGraphicsCompatibility graphicsCompatibility) noexcept
    : m_runtime(&runtime), m_backend(backend), m_nativeWindow(nativeWindow),
      m_backendWindowId(backendWindowId),
      m_graphicsCompatibility(graphicsCompatibility)
{
}

WindowImpl::~WindowImpl() noexcept
{
    PONDER_VERIFY(m_runtime != nullptr, "Window runtime state is missing");
    m_runtime->VerifyOwnerThread("window destruction");
    PONDER_VERIFY(m_nativeWindow != nullptr, "Window native state is missing");

    if (m_registered)
    {
        m_runtime->BeginWindowDestruction(this, m_backendWindowId, m_id);
    }

    m_backend.destroy(m_backend.context, m_nativeWindow);
    m_nativeWindow = nullptr;

    if (m_registered)
    {
        m_runtime->FinishWindowDestruction(this);
        m_registered = false;
    }
}

void WindowImpl::CommitRegistration(WindowId id) noexcept
{
    PONDER_VERIFY(id.IsValid(), "Cannot commit an invalid platform window ID");
    PONDER_VERIFY(!m_registered, "Platform window is already registered");
    m_id = id;
    m_registered = true;
}

void WindowImpl::ObserveShownEvent() noexcept
{
    m_hiddenStateRequest.reset();
}

void WindowImpl::VerifyUsable(std::string_view operation) const
{
    PONDER_VERIFY(m_runtime != nullptr, "Cannot use a window without runtime state");
    PONDER_VERIFY(m_registered, "Cannot use an unregistered window");
    PONDER_VERIFY(m_nativeWindow != nullptr, "Cannot use a destroyed window");
    m_runtime->VerifyOwnerThread(operation);
}

std::string WindowImpl::GetErrorContext() const
{
    return "window " + std::to_string(m_id.GetValue());
}

WindowId WindowImpl::GetId() const
{
    VerifyUsable("ID query");
    return m_id;
}

WindowGraphicsCompatibility WindowImpl::GetGraphicsCompatibility() const
{
    VerifyUsable("graphics compatibility query");
    return m_graphicsCompatibility;
}

std::string WindowImpl::GetTitle() const
{
    VerifyUsable("title query");
    const char* const title = m_backend.getTitle(m_backend.context, m_nativeWindow);
    PONDER_VERIFY(title != nullptr, "Backend returned a null window title");
    return std::string{title};
}

core::VoidResult WindowImpl::SetTitle(std::string_view title)
{
    VerifyUsable("title update");
    if (!IsValidUtf8WithoutEmbeddedNull(title))
    {
        return core::VoidResult::FromError(core::Error{
            kInvalidArgumentCode,
            "Window title must be UTF-8 without embedded nulls."});
    }

    const std::string ownedTitle{title};
    const std::string context = GetErrorContext();
    if (!m_backend.setTitle(m_backend.context, m_nativeWindow,
                            ownedTitle.c_str()))
    {
        return core::VoidResult::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_SetWindowTitle", context));
    }

    return core::VoidResult::Success();
}

core::Result<ScreenPosition> WindowImpl::GetPosition() const
{
    VerifyUsable("position query");
    int x{};
    int y{};
    const std::string context = GetErrorContext();
    if (!m_backend.getPosition(m_backend.context, m_nativeWindow, &x, &y))
    {
        return core::Result<ScreenPosition>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetWindowPosition", context));
    }

    constexpr int kMinimumProjectPosition =
        static_cast<int>(std::numeric_limits<std::int32_t>::min());
    constexpr int kMaximumProjectPosition =
        static_cast<int>(std::numeric_limits<std::int32_t>::max());
    if (x < kMinimumProjectPosition || x > kMaximumProjectPosition ||
        y < kMinimumProjectPosition || y > kMaximumProjectPosition)
    {
        return core::Result<ScreenPosition>::FromError(core::Error{
            kBackendFailureCode,
            "SDL_GetWindowPosition returned an out-of-range value for " + context +
                "."});
    }

    return ScreenPosition{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
}

core::VoidResult WindowImpl::SetPosition(ScreenPosition position)
{
    VerifyUsable("position update");
    core::VoidResult validation = ValidatePosition(position);
    if (!validation.HasValue())
    {
        return validation;
    }

    const std::string context = GetErrorContext();
    if (!m_backend.setPosition(m_backend.context, m_nativeWindow,
                               static_cast<int>(position.x),
                               static_cast<int>(position.y)))
    {
        return core::VoidResult::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_SetWindowPosition", context));
    }

    return core::VoidResult::Success();
}

core::Result<LogicalSize> WindowImpl::GetLogicalSize() const
{
    VerifyUsable("logical size query");
    int width{};
    int height{};
    const std::string context = GetErrorContext();
    if (!m_backend.getSize(m_backend.context, m_nativeWindow, &width, &height))
    {
        return core::Result<LogicalSize>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetWindowSize", context));
    }
    if (width < 0 || height < 0)
    {
        return core::Result<LogicalSize>::FromError(
            MakeInvalidBackendSizeError("SDL_GetWindowSize", context));
    }

    return LogicalSize{static_cast<std::uint32_t>(width),
                       static_cast<std::uint32_t>(height)};
}

core::Result<PixelSize> WindowImpl::GetPixelSize() const
{
    VerifyUsable("pixel size query");
    int width{};
    int height{};
    const std::string context = GetErrorContext();
    if (!m_backend.getSizeInPixels(m_backend.context, m_nativeWindow, &width,
                                   &height))
    {
        return core::Result<PixelSize>::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_GetWindowSizeInPixels", context));
    }
    if (width < 0 || height < 0)
    {
        return core::Result<PixelSize>::FromError(
            MakeInvalidBackendSizeError("SDL_GetWindowSizeInPixels", context));
    }

    return PixelSize{static_cast<std::uint32_t>(width),
                     static_cast<std::uint32_t>(height)};
}

core::VoidResult WindowImpl::SetLogicalSize(LogicalSize size)
{
    VerifyUsable("logical size update");
    core::VoidResult validation =
        ValidatePositiveSize(size, "Window logical size");
    if (!validation.HasValue())
    {
        return validation;
    }

    const std::string context = GetErrorContext();
    if (!m_backend.setSize(m_backend.context, m_nativeWindow,
                           static_cast<int>(size.width),
                           static_cast<int>(size.height)))
    {
        return core::VoidResult::FromError(CaptureSdlFailure(
            kBackendFailureCode, "SDL_SetWindowSize", context));
    }

    return core::VoidResult::Success();
}

core::VoidResult WindowImpl::Show()
{
    VerifyUsable("show");
    const std::string context = GetErrorContext();
    if (!m_backend.show(m_backend.context, m_nativeWindow))
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_ShowWindow", context));
    }

    m_hiddenStateRequest.reset();
    return core::VoidResult::Success();
}

core::VoidResult WindowImpl::Hide()
{
    VerifyUsable("hide");
    const std::string context = GetErrorContext();
    if (!m_backend.hide(m_backend.context, m_nativeWindow))
    {
        return core::VoidResult::FromError(
            CaptureSdlFailure(kBackendFailureCode, "SDL_HideWindow", context));
    }

    return core::VoidResult::Success();
}
} // namespace detail

core::Result<Window> PlatformRuntime::CreateWindow(const WindowDesc& desc)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from PlatformRuntime");

    auto stateResult = detail::WindowImpl::Create(*m_state, desc);
    if (!stateResult.HasValue())
    {
        return core::Result<Window>::FromError(
            std::move(stateResult).GetError());
    }

    return Window{std::move(stateResult).GetValue()};
}

Window::Window(std::unique_ptr<detail::WindowImpl> state) noexcept
    : m_state(std::move(state))
{
}

Window::~Window() noexcept = default;

Window::Window(Window&& other) noexcept : m_state(std::move(other.m_state)) {}

Window& Window::operator=(Window&& other) noexcept
{
    if (this != &other)
    {
        m_state = std::move(other.m_state);
    }
    return *this;
}

WindowId Window::GetId() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetId();
}

WindowGraphicsCompatibility Window::GetGraphicsCompatibility() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetGraphicsCompatibility();
}

std::string Window::GetTitle() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetTitle();
}

core::VoidResult Window::SetTitle(std::string_view title)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->SetTitle(title);
}

core::Result<ScreenPosition> Window::GetPosition() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetPosition();
}

core::VoidResult Window::SetPosition(ScreenPosition position)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->SetPosition(position);
}

core::Result<LogicalSize> Window::GetLogicalSize() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetLogicalSize();
}

core::Result<PixelSize> Window::GetPixelSize() const
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->GetPixelSize();
}

core::VoidResult Window::SetLogicalSize(LogicalSize size)
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->SetLogicalSize(size);
}

core::VoidResult Window::Show()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->Show();
}

core::VoidResult Window::Hide()
{
    PONDER_VERIFY(m_state != nullptr, "Cannot use a moved-from Window");
    return m_state->Hide();
}
} // namespace pond::platform
