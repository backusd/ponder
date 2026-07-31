#include <ponder/core/Exception.hpp>
#include <ponder/core/ScopeExit.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/WindowGraphics.hpp>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_platform_defines.h>
#include <SDL3/SDL_video.h>
#include <concepts>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "IPlatformDisplayBackend.hpp"
#include "ProcessBackend.hpp"
#include "SdlCommon.hpp"
#include "SdlRuntime.hpp"
#include "SdlWindowBackend.hpp"

namespace
{
template <typename Type>
concept FormattableAndStreamable = std::formattable<Type, char> && requires(std::ostream& output, const Type& value) {
    { output << value } -> std::same_as<std::ostream&>;
};

template <typename... Types>
consteval bool AreFormattableAndStreamable()
{
    return (FormattableAndStreamable<Types> && ...);
}

using namespace ponder::platform::detail;

using WindowBackend = ponder::platform::detail::IPlatformWindowBackend;

template <typename Operation>
void ExpectPlatformException(Operation&& operation, ponder::platform::PlatformErrorCode errorCode, std::string_view operationName,
                             std::string_view expectedSourceFunction = {})
{
    try
    {
        std::forward<Operation>(operation)();
        FAIL() << "Expected ponder::core::Exception";
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_TRUE(exception.GetMessage().starts_with(std::format("Platform error [{}]: ", errorCode)));
        EXPECT_NE(exception.GetMessage().find(operationName), std::string_view::npos);
        if (!expectedSourceFunction.empty())
        {
            EXPECT_NE(std::string_view{exception.GetLocation().function_name()}.find(expectedSourceFunction), std::string_view::npos);
        }
    }
    catch (...)
    {
        FAIL() << "Expected ponder::core::Exception";
    }
}

static_assert(std::same_as<decltype(std::declval<WindowBackend&>().Create(std::declval<const BackendWindowCreateDesc&>())), BackendWindowHandle>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().GetId(std::declval<BackendWindowHandle>())), std::uint32_t>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().GetTitle(std::declval<BackendWindowHandle>())), std::string>);
static_assert(
    std::same_as<decltype(std::declval<WindowBackend&>().SetTitle(std::declval<BackendWindowHandle>(), std::declval<std::string_view>())), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().GetPosition(std::declval<BackendWindowHandle>())), BackendWindowPosition>);
static_assert(
    std::same_as<decltype(std::declval<WindowBackend&>().SetPosition(std::declval<BackendWindowHandle>(), std::declval<BackendWindowPosition>())),
                 void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().GetSize(std::declval<BackendWindowHandle>())), BackendWindowLogicalSize>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().GetSizeInPixels(std::declval<BackendWindowHandle>())), BackendWindowPixelSize>);
static_assert(std::same_as<
              decltype(std::declval<WindowBackend&>().SetSize(std::declval<BackendWindowHandle>(), std::declval<BackendWindowLogicalSize>())), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().SetMinimumSize(std::declval<BackendWindowHandle>(),
                                                                                  std::declval<BackendWindowLogicalSize>())),
                           void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().Show(std::declval<BackendWindowHandle>())), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().Hide(std::declval<BackendWindowHandle>())), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().GetProperties(std::declval<BackendWindowHandle>())), BackendWindowProperties>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().SetFullscreenModeToDesktop(std::declval<BackendWindowHandle>())), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().SetFullscreen(std::declval<BackendWindowHandle>(), true)), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().SetBordered(std::declval<BackendWindowHandle>(), true)), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().SetResizable(std::declval<BackendWindowHandle>(), true)), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().SetAlwaysOnTop(std::declval<BackendWindowHandle>(), true)), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().Minimize(std::declval<BackendWindowHandle>())), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().Maximize(std::declval<BackendWindowHandle>())), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().Restore(std::declval<BackendWindowHandle>())), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().StartTextInput(std::declval<BackendWindowHandle>())), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().StopTextInput(std::declval<BackendWindowHandle>())), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().ClearTextComposition(std::declval<BackendWindowHandle>())), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().SetTextInputArea(std::declval<BackendWindowHandle>(),
                                                                                    std::declval<std::optional<BackendTextInputArea>>())),
                           void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().SetMouseGrab(std::declval<BackendWindowHandle>(), true)), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().SetRelativeMouseMode(std::declval<BackendWindowHandle>(), true)), void>);
static_assert(std::same_as<decltype(std::declval<WindowBackend&>().GetNativeHandle(std::declval<BackendWindowHandle>())),
                           ponder::core::Result<ponder::platform::NativeWindowHandle>>);
static_assert(noexcept(std::declval<WindowBackend&>().IsTextInputActive(std::declval<BackendWindowHandle>())));
static_assert(noexcept(std::declval<WindowBackend&>().IsMouseGrabbed(std::declval<BackendWindowHandle>())));
static_assert(noexcept(std::declval<WindowBackend&>().IsRelativeMouseModeEnabled(std::declval<BackendWindowHandle>())));

static_assert(AreFormattableAndStreamable<ApplicationMetadataProperty, CursorHandle, BackendEventKind, BackendEvent, EventTranslationContext,
                                          RuntimeDisplayLifecycleEventKind, RuntimeDisplayLifecycleEvent, RuntimePendingDisplayLifecycleRecovery,
                                          RuntimeDisplayRecord, RuntimeBackendDisplayRecord, SdlHintValueState, SdlRuntime, BackendWindowHandle,
                                          BackendWindowPosition, BackendWindowLogicalSize, BackendWindowPixelSize, BackendWindowCreateDesc,
                                          BackendWindowProperties, BackendTextInputArea, BackendNativeWindowDriver, BackendDisplayOrientation,
                                          BackendScreenRectangle, BackendProcessExitKind, BackendProcessExitStatus, BackendProcessKillResult>());

TEST(WindowBackendFormattingTests, FormatsAndStreamsBackendWindowValues)
{
    const BackendWindowHandle window{42};
    const BackendWindowPosition position{12, -4};
    const BackendWindowLogicalSize size{1280, 800};

    EXPECT_EQ(std::format("{}", window), "0x2A");
    EXPECT_EQ(std::format("{}", position), "(12, -4)");
    EXPECT_EQ(std::format("{}", size), "1280x800");
    EXPECT_EQ(std::format("{}", BackendNativeWindowDriver::Wayland), "wayland");

    std::ostringstream output;
    output << BackendTextInputArea{1, 2, 300, 40, 12};
    EXPECT_EQ(output.str(), "(1, 2) / 300x40, cursorOffset=12");
}

TEST(WindowBackendHandleTests, DistinguishesInvalidAndValidValues)
{
    using ponder::platform::detail::BackendWindowHandle;

    const BackendWindowHandle invalid;
    const BackendWindowHandle valid{42};

    EXPECT_FALSE(invalid.IsValid());
    EXPECT_TRUE(valid.IsValid());
    EXPECT_EQ(valid.GetValue(), 42U);
    EXPECT_NE(valid, invalid);
}

class SdlWindowBackendTests : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_EQ(SDL_WasInit(0), 0U);
        ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));
        ASSERT_TRUE(SDL_Init(SDL_INIT_VIDEO)) << SDL_GetError();
    }

    void TearDown() override
    {
        SDL_Quit();
        static_cast<void>(SDL_ResetHint(SDL_HINT_VIDEO_DRIVER));
    }
};

TEST_F(SdlWindowBackendTests, UsesDirectCreationGeometryAndVisibilityContracts)
{
    SdlWindowBackend backend;
    const BackendWindowCreateDesc desc{.title = "initial",
                                       .logicalSize = {640, 480},
                                       .resizable = true,
                                       .highPixelDensity = true,
                                       .graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Default};

    const BackendWindowHandle window = backend.Create(desc);
    const auto destroyWindow = ponder::core::MakeScopeExit(
        [&backend, window]() noexcept
        {
            backend.Destroy(window);
        });

    EXPECT_TRUE(window.IsValid());
    EXPECT_NE(backend.GetId(window), 0U);
    EXPECT_EQ(backend.GetTitle(window), "initial");

    backend.SetTitle(window, "updated");
    EXPECT_EQ(backend.GetTitle(window), "updated");

    backend.SetPosition(window, BackendWindowPosition{-20, 35});
    const BackendWindowPosition position = backend.GetPosition(window);
    EXPECT_EQ(position.x, -20);
    EXPECT_EQ(position.y, 35);

    backend.SetSize(window, BackendWindowLogicalSize{800, 600});
    const BackendWindowLogicalSize logicalSize = backend.GetSize(window);
    EXPECT_EQ(logicalSize.width, 800);
    EXPECT_EQ(logicalSize.height, 600);

    const BackendWindowPixelSize pixelSize = backend.GetSizeInPixels(window);
    EXPECT_GT(pixelSize.width, 0);
    EXPECT_GT(pixelSize.height, 0);

    EXPECT_NO_THROW(backend.SetMinimumSize(window, BackendWindowLogicalSize{320, 240}));
    EXPECT_NO_THROW(backend.Hide(window));
    EXPECT_NO_THROW(backend.Show(window));
}

TEST_F(SdlWindowBackendTests, UsesDirectPresentationAndPropertyContracts)
{
    SdlWindowBackend backend;
    const BackendWindowCreateDesc desc{.title = "properties",
                                       .logicalSize = {640, 480},
                                       .resizable = true,
                                       .highPixelDensity = false,
                                       .graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Default};

    const BackendWindowHandle window = backend.Create(desc);
    const auto destroyWindow = ponder::core::MakeScopeExit(
        [&backend, window]() noexcept
        {
            backend.Destroy(window);
        });

    const BackendWindowProperties initial = backend.GetProperties(window);
    EXPECT_FALSE(initial.desktopFullscreen);
    EXPECT_TRUE(initial.hidden);
    EXPECT_FALSE(initial.borderless);
    EXPECT_TRUE(initial.resizable);
    EXPECT_FALSE(initial.minimized);
    EXPECT_FALSE(initial.maximized);
    EXPECT_FALSE(initial.inputFocus);
    EXPECT_FALSE(initial.alwaysOnTop);

    EXPECT_NO_THROW(backend.SetFullscreenModeToDesktop(window));
    EXPECT_NO_THROW(backend.SetFullscreen(window, true));
    EXPECT_TRUE(backend.GetProperties(window).desktopFullscreen);
    EXPECT_NO_THROW(backend.SetFullscreen(window, false));
    EXPECT_FALSE(backend.GetProperties(window).desktopFullscreen);

    EXPECT_NO_THROW(backend.SetBordered(window, true));
    EXPECT_NO_THROW(backend.SetResizable(window, true));
    EXPECT_NO_THROW(backend.SetAlwaysOnTop(window, false));
}

TEST_F(SdlWindowBackendTests, ThrowsUnsupportedWhenSuccessfulMutationIsIgnored)
{
    SdlWindowBackend backend;
    const BackendWindowCreateDesc desc{.title = "unsupported mutations",
                                       .logicalSize = {640, 480},
                                       .resizable = true,
                                       .highPixelDensity = false,
                                       .graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Default};

    const BackendWindowHandle window = backend.Create(desc);
    const auto destroyWindow = ponder::core::MakeScopeExit(
        [&backend, window]() noexcept
        {
            backend.Destroy(window);
        });

    ExpectPlatformException(
        [&backend, window]()
        {
            backend.SetBordered(window, false);
        },
        ponder::platform::PlatformErrorCode::Unsupported, "SDL_SetWindowBordered", "SetBordered");
    EXPECT_FALSE(backend.GetProperties(window).borderless);

    ExpectPlatformException(
        [&backend, window]()
        {
            backend.SetResizable(window, false);
        },
        ponder::platform::PlatformErrorCode::Unsupported, "SDL_SetWindowResizable");
    EXPECT_TRUE(backend.GetProperties(window).resizable);

    ExpectPlatformException(
        [&backend, window]()
        {
            backend.SetAlwaysOnTop(window, true);
        },
        ponder::platform::PlatformErrorCode::Unsupported, "SDL_SetWindowAlwaysOnTop");
    EXPECT_FALSE(backend.GetProperties(window).alwaysOnTop);
}

TEST_F(SdlWindowBackendTests, ThrowsBackendFailureForDocumentedSdlFailures)
{
    SdlWindowBackend backend;
    const BackendWindowCreateDesc desc{.title = "state failures",
                                       .logicalSize = {640, 480},
                                       .resizable = true,
                                       .highPixelDensity = false,
                                       .graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Default};

    const BackendWindowHandle window = backend.Create(desc);
    const auto destroyWindow = ponder::core::MakeScopeExit(
        [&backend, window]() noexcept
        {
            backend.Destroy(window);
        });

    ExpectPlatformException(
        [&backend, window]()
        {
            backend.Minimize(window);
        },
        ponder::platform::PlatformErrorCode::BackendFailure, "SDL_MinimizeWindow", "Minimize");
    ExpectPlatformException(
        [&backend, window]()
        {
            backend.Maximize(window);
        },
        ponder::platform::PlatformErrorCode::BackendFailure, "SDL_MaximizeWindow");
    ExpectPlatformException(
        [&backend, window]()
        {
            backend.Restore(window);
        },
        ponder::platform::PlatformErrorCode::BackendFailure, "SDL_RestoreWindow");
}

TEST_F(SdlWindowBackendTests, UsesDirectTextInputAndMouseContracts)
{
    SdlWindowBackend backend;
    const BackendWindowCreateDesc desc{.title = "text and mouse",
                                       .logicalSize = {640, 480},
                                       .resizable = true,
                                       .highPixelDensity = false,
                                       .graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Default};

    const BackendWindowHandle window = backend.Create(desc);
    const auto destroyWindow = ponder::core::MakeScopeExit(
        [&backend, window]() noexcept
        {
            backend.Destroy(window);
        });

    EXPECT_FALSE(backend.IsTextInputActive(window));
    EXPECT_NO_THROW(backend.StartTextInput(window));
    EXPECT_TRUE(backend.IsTextInputActive(window));
    EXPECT_NO_THROW(backend.SetTextInputArea(window, BackendTextInputArea{10, 20, 200, 40, 12}));
    EXPECT_NO_THROW(backend.SetTextInputArea(window, std::nullopt));
    EXPECT_NO_THROW(backend.ClearTextComposition(window));
    EXPECT_NO_THROW(backend.StopTextInput(window));
    EXPECT_FALSE(backend.IsTextInputActive(window));

    EXPECT_NO_THROW(backend.SetMouseGrab(window, true));
    EXPECT_NO_THROW(backend.SetMouseGrab(window, false));
    EXPECT_FALSE(backend.IsMouseGrabbed(window));

    EXPECT_FALSE(backend.IsRelativeMouseModeEnabled(window));
    EXPECT_NO_THROW(backend.SetRelativeMouseMode(window, false));
}

TEST_F(SdlWindowBackendTests, RetainsUnsupportedNativeDriverAsAResultOutcome)
{
    SdlWindowBackend backend;
    const BackendWindowCreateDesc desc{.title = "unsupported native driver",
                                       .logicalSize = {640, 480},
                                       .resizable = false,
                                       .highPixelDensity = false,
                                       .graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Default};

    const BackendWindowHandle window = backend.Create(desc);
    const auto destroyWindow = ponder::core::MakeScopeExit(
        [&backend, window]() noexcept
        {
            backend.Destroy(window);
        });

    const ponder::core::Result<ponder::platform::NativeWindowHandle> handle = backend.GetNativeHandle(window);
    ASSERT_FALSE(handle.HasValue());
    EXPECT_EQ(handle.GetError().GetCode(), ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::Unsupported));
}

TEST(SdlWindowBackendErrorTests, ThrowsPlatformExceptionWhenCreateFails)
{
    ASSERT_EQ(SDL_WasInit(SDL_INIT_VIDEO), 0U);
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "ponder-window-backend-driver-that-does-not-exist", SDL_HINT_OVERRIDE));
    const auto resetVideoDriverHint = ponder::core::MakeScopeExit(
        []() noexcept
        {
            SDL_Quit();
            static_cast<void>(SDL_ResetHint(SDL_HINT_VIDEO_DRIVER));
        });

    SdlWindowBackend backend;
    const BackendWindowCreateDesc desc{.title = "creation failure",
                                       .logicalSize = {640, 480},
                                       .resizable = false,
                                       .highPixelDensity = false,
                                       .graphicsCompatibility = ponder::platform::WindowGraphicsCompatibility::Default};

    try
    {
        const BackendWindowHandle unexpectedWindow = backend.Create(desc);
        backend.Destroy(unexpectedWindow);
        FAIL() << "Expected ponder::core::Exception";
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_TRUE(exception.GetMessage().starts_with("Platform error [backend_failure]: "));
        EXPECT_NE(exception.GetMessage().find("SDL_CreateWindow"), std::string_view::npos);
        EXPECT_NE(exception.GetMessage().find("window"), std::string_view::npos);
        EXPECT_NE(std::string_view{exception.GetLocation().function_name()}.find("Create"), std::string_view::npos);
    }
    catch (...)
    {
        FAIL() << "Expected ponder::core::Exception";
    }
}

TEST(SdlWindowBackendErrorTests, ThrowsPlatformExceptionWhenVideoDriverIsUnavailable)
{
    ASSERT_EQ(SDL_WasInit(SDL_INIT_VIDEO), 0U);

    SdlWindowBackend backend;
    ExpectPlatformException(
        [&backend]()
        {
            static_cast<void>(backend.GetNativeHandle(BackendWindowHandle{42}));
        },
        ponder::platform::PlatformErrorCode::BackendFailure, "current video driver", "GetNativeHandle");
}

TEST(WindowBackendNativeHandleTests, BuildsCompleteHandlesForEverySupportedDriver)
{
    void* const first = reinterpret_cast<void*>(std::uintptr_t{0x1111});
    void* const second = reinterpret_cast<void*>(std::uintptr_t{0x2222});
    const BackendWindowHandle backendWindow{42};

    const ponder::platform::NativeWindowHandle win32 = MakeWin32NativeWindowHandle(backendWindow, first, second);
    ASSERT_TRUE(std::holds_alternative<ponder::platform::NativeWin32Window>(win32));
    EXPECT_EQ(std::get<ponder::platform::NativeWin32Window>(win32), (ponder::platform::NativeWin32Window{.instance = first, .window = second}));

    const ponder::platform::NativeWindowHandle x11 = MakeX11NativeWindowHandle(backendWindow, first, 0x3333);
    ASSERT_TRUE(std::holds_alternative<ponder::platform::NativeX11Window>(x11));
    EXPECT_EQ(std::get<ponder::platform::NativeX11Window>(x11), (ponder::platform::NativeX11Window{.display = first, .window = 0x3333}));

    const ponder::platform::NativeWindowHandle wayland = MakeWaylandNativeWindowHandle(backendWindow, first, second);
    ASSERT_TRUE(std::holds_alternative<ponder::platform::NativeWaylandWindow>(wayland));
    EXPECT_EQ(std::get<ponder::platform::NativeWaylandWindow>(wayland), (ponder::platform::NativeWaylandWindow{.display = first, .surface = second}));
}

TEST(WindowBackendNativeHandleTests, ThrowsInsteadOfReturningPartialSupportedHandles)
{
    void* const pointer = reinterpret_cast<void*>(std::uintptr_t{0x1111});
    const BackendWindowHandle backendWindow{42};

    ExpectPlatformException(
        [backendWindow, pointer]()
        {
            static_cast<void>(MakeWin32NativeWindowHandle(backendWindow, pointer, nullptr));
        },
        ponder::platform::PlatformErrorCode::BackendFailure, "Win32", "MakeWin32NativeWindowHandle");
    ExpectPlatformException(
        [backendWindow, pointer]()
        {
            static_cast<void>(MakeX11NativeWindowHandle(backendWindow, pointer, 0));
        },
        ponder::platform::PlatformErrorCode::BackendFailure, "X11");
    ExpectPlatformException(
        [backendWindow, pointer]()
        {
            static_cast<void>(MakeWaylandNativeWindowHandle(backendWindow, nullptr, pointer));
        },
        ponder::platform::PlatformErrorCode::BackendFailure, "Wayland");
}

TEST(WindowBackendFlagTests, StagesEveryWindowHiddenAndKeepsPropertiesOrthogonal)
{
    const ponder::platform::detail::BackendWindowCreateDesc defaultDesc{.title = "ponder",
                                                                        .logicalSize = {1280, 800},
                                                                        .resizable = true,
                                                                        .highPixelDensity = true,
                                                                        .graphicsCompatibility =
                                                                            ponder::platform::WindowGraphicsCompatibility::Default};

    const std::uint64_t defaultFlags = ponder::platform::detail::BuildSdlWindowFlags(defaultDesc);
    EXPECT_NE(defaultFlags & SDL_WINDOW_HIDDEN, 0U);
    EXPECT_NE(defaultFlags & SDL_WINDOW_RESIZABLE, 0U);
    EXPECT_NE(defaultFlags & SDL_WINDOW_HIGH_PIXEL_DENSITY, 0U);
    EXPECT_EQ(defaultFlags & SDL_WINDOW_VULKAN, 0U);
    EXPECT_EQ(defaultFlags & SDL_WINDOW_METAL, 0U);

    ponder::platform::detail::BackendWindowCreateDesc minimalDesc = defaultDesc;
    minimalDesc.resizable = false;
    minimalDesc.highPixelDensity = false;
    const std::uint64_t minimalFlags = ponder::platform::detail::BuildSdlWindowFlags(minimalDesc);
    EXPECT_NE(minimalFlags & SDL_WINDOW_HIDDEN, 0U);
    EXPECT_EQ(minimalFlags & SDL_WINDOW_RESIZABLE, 0U);
    EXPECT_EQ(minimalFlags & SDL_WINDOW_HIGH_PIXEL_DENSITY, 0U);
}

TEST(WindowBackendFlagTests, SupportsOnlyApprovedGraphicsCompatibilityForTheCurrentHost)
{
    using ponder::platform::WindowGraphicsCompatibility;
    using ponder::platform::detail::IsWindowGraphicsCompatibilitySupported;

    EXPECT_TRUE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Default));
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
    EXPECT_TRUE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Vulkan));
    EXPECT_FALSE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Metal));
#elif defined(SDL_PLATFORM_MACOS)
    EXPECT_FALSE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Vulkan));
    EXPECT_TRUE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Metal));
#else
    EXPECT_FALSE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Vulkan));
    EXPECT_FALSE(IsWindowGraphicsCompatibilitySupported(WindowGraphicsCompatibility::Metal));
#endif
    EXPECT_FALSE(IsWindowGraphicsCompatibilitySupported(static_cast<WindowGraphicsCompatibility>(0xFF)));
}

TEST(WindowBackendFlagTests, MapsRendererCompatibilityOnlyOnSupportedHosts)
{
    using ponder::platform::WindowGraphicsCompatibility;
    using ponder::platform::detail::BackendWindowCreateDesc;
    using ponder::platform::detail::BuildSdlWindowFlags;

    const BackendWindowCreateDesc vulkanDesc{.title = "ponder",
                                             .logicalSize = {1280, 800},
                                             .resizable = true,
                                             .highPixelDensity = true,
                                             .graphicsCompatibility = WindowGraphicsCompatibility::Vulkan};
    const BackendWindowCreateDesc metalDesc{.title = "ponder",
                                            .logicalSize = {1280, 800},
                                            .resizable = true,
                                            .highPixelDensity = true,
                                            .graphicsCompatibility = WindowGraphicsCompatibility::Metal};

    const std::uint64_t vulkanFlags = BuildSdlWindowFlags(vulkanDesc);
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX)
    EXPECT_NE(vulkanFlags & SDL_WINDOW_VULKAN, 0U);
#else
    EXPECT_EQ(vulkanFlags & SDL_WINDOW_VULKAN, 0U);
#endif
    EXPECT_EQ(vulkanFlags & SDL_WINDOW_METAL, 0U);

    const std::uint64_t metalFlags = BuildSdlWindowFlags(metalDesc);
#if defined(SDL_PLATFORM_MACOS)
    EXPECT_NE(metalFlags & SDL_WINDOW_METAL, 0U);
#else
    EXPECT_EQ(metalFlags & SDL_WINDOW_METAL, 0U);
#endif
    EXPECT_EQ(metalFlags & SDL_WINDOW_VULKAN, 0U);
}

TEST(WindowBackendFlagTests, IdentifiesBackendReservedPositionEncodings)
{
    EXPECT_TRUE(ponder::platform::detail::IsReservedSdlWindowPosition(static_cast<std::int32_t>(SDL_WINDOWPOS_UNDEFINED)));
    EXPECT_TRUE(ponder::platform::detail::IsReservedSdlWindowPosition(static_cast<std::int32_t>(SDL_WINDOWPOS_CENTERED)));
    EXPECT_FALSE(ponder::platform::detail::IsReservedSdlWindowPosition(-250));
    EXPECT_FALSE(ponder::platform::detail::IsReservedSdlWindowPosition(250));
}

TEST(WindowBackendFlagTests, ClassifiesApprovedNativeWindowDrivers)
{
    using ponder::platform::detail::BackendNativeWindowDriver;

    EXPECT_EQ(ponder::platform::detail::GetNativeWindowDriver("windows"), BackendNativeWindowDriver::Win32);
    EXPECT_EQ(ponder::platform::detail::GetNativeWindowDriver("x11"), BackendNativeWindowDriver::X11);
    EXPECT_EQ(ponder::platform::detail::GetNativeWindowDriver("wayland"), BackendNativeWindowDriver::Wayland);
    EXPECT_EQ(ponder::platform::detail::GetNativeWindowDriver("cocoa"), BackendNativeWindowDriver::Unsupported);
    EXPECT_EQ(ponder::platform::detail::GetNativeWindowDriver("dummy"), BackendNativeWindowDriver::Unsupported);
    EXPECT_EQ(ponder::platform::detail::GetNativeWindowDriver(""), BackendNativeWindowDriver::Unsupported);
}
} // namespace
