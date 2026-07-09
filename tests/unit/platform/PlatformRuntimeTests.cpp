#include "PlatformRuntimeBackend.hpp"

#include <ponder/core/PonderException.hpp>
#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <SDL3/SDL_error.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
using pond::platform::detail::ApplicationMetadataProperty;
using pond::platform::detail::BackendDisplayOrientation;
using pond::platform::detail::BackendScreenRectangle;
using pond::platform::detail::BackendWindowCreateDesc;
using pond::platform::detail::PlatformDisplayBackend;
using pond::platform::detail::PlatformRuntimeBackend;
using pond::platform::detail::PlatformWindowBackend;

[[nodiscard]] constexpr std::size_t MetadataIndex(ApplicationMetadataProperty property) noexcept
{
    return static_cast<std::size_t>(property);
}

struct FakeWindow final
{
    std::uint32_t backendId{};
    std::string title;
    int x{};
    int y{};
    int width{};
    int height{};
    int pixelWidth{};
    int pixelHeight{};
    int minimumWidth{};
    int minimumHeight{};
    std::uint32_t backendDisplayId{1};
    float pixelDensity{1.0F};
    float displayScale{1.0F};
    bool visible{};
    bool resizable{};
    bool highPixelDensity{};
    pond::platform::WindowGraphicsCompatibility graphicsCompatibility{
        pond::platform::WindowGraphicsCompatibility::Default};
};

struct FakeDisplay final
{
    std::uint32_t backendId{};
    std::string name;
    BackendScreenRectangle bounds;
    BackendScreenRectangle usableBounds;
    float refreshRateHertz{};
    BackendDisplayOrientation orientation{BackendDisplayOrientation::Unknown};
    float contentScale{1.0F};
};

struct FakeRuntimeBackend final
{
    bool mainThread{true};
    bool initializedSubsystems{false};
    bool onlyRuntimeSubsystems{true};
    bool initializeVideoSucceeds{true};
    std::optional<ApplicationMetadataProperty> failingMetadataProperty;
    int metadataFailuresRemaining{};
    std::string failingHint;
    int hintFailuresRemaining{};
    std::uint64_t ticks{123'456};
    std::uint32_t nextBackendWindowId{100};
    std::uint32_t defaultWindowDisplayId{1};
    float defaultWindowPixelDensity{1.0F};
    float defaultWindowDisplayScale{1.0F};
    std::vector<std::uint32_t> scriptedBackendWindowIds;
    std::size_t scriptedBackendWindowIdIndex{};
    std::string failingWindowOperation;
    int windowFailuresRemaining{};
    std::string failingDisplayOperation;
    int displayFailuresRemaining{};
    std::optional<std::uint32_t> disconnectDisplayOnFailure;
    bool destroyOverwritesError{};

    int mainThreadChecks{};
    int initializedSubsystemChecks{};
    int runtimeSubsystemChecks{};
    int metadataCalls{};
    int initializeVideoCalls{};
    int quitCalls{};
    int tickCalls{};
    int createWindowCalls{};
    int destroyWindowCalls{};
    int enumerateDisplayCalls{};
    bool policiesObservedDuringInitialization{};
    bool metadataObservedDuringInitialization{};
    bool attemptCreateDuringRestoration{};
    bool attemptedCreateDuringRestoration{};
    std::optional<pond::core::ErrorCode> restorationCreateError;

    std::array<std::optional<std::string>, 3> metadata;
    std::unordered_map<std::string, std::string> hints;
    std::list<FakeWindow> windows;
    std::vector<std::uint32_t> connectedDisplayIds;
    std::unordered_map<std::uint32_t, FakeDisplay> displays;
    std::vector<std::string> calls;
};

[[nodiscard]] FakeRuntimeBackend& GetFake(void* context)
{
    return *static_cast<FakeRuntimeBackend*>(context);
}

[[nodiscard]] FakeWindow& GetFakeWindow(void* window)
{
    return *static_cast<FakeWindow*>(window);
}

[[nodiscard]] FakeDisplay& GetFakeDisplay(FakeRuntimeBackend& fake,
                                          std::uint32_t backendDisplayId)
{
    return fake.displays.at(backendDisplayId);
}

[[nodiscard]] bool FailWindowOperation(FakeRuntimeBackend& fake,
                                       std::string_view operation)
{
    if (fake.failingWindowOperation != operation ||
        fake.windowFailuresRemaining == 0)
    {
        return false;
    }

    --fake.windowFailuresRemaining;
    const std::string message = "synthetic " + std::string{operation} + " failure";
    static_cast<void>(SDL_SetError("%s", message.c_str()));
    return true;
}

[[nodiscard]] bool FailDisplayOperation(FakeRuntimeBackend& fake,
                                        std::string_view operation)
{
    if (fake.failingDisplayOperation != operation ||
        fake.displayFailuresRemaining == 0)
    {
        return false;
    }

    --fake.displayFailuresRemaining;
    const std::string message = "synthetic " + std::string{operation} + " failure";
    static_cast<void>(SDL_SetError("%s", message.c_str()));

    if (fake.disconnectDisplayOnFailure.has_value())
    {
        const std::uint32_t disconnectedId = *fake.disconnectDisplayOnFailure;
        std::erase(fake.connectedDisplayIds, disconnectedId);
        fake.disconnectDisplayOnFailure.reset();
    }
    return true;
}

[[nodiscard]] const char* GetHintValue(FakeRuntimeBackend& fake, const char* name)
{
    const auto iterator = fake.hints.find(name);
    return iterator != fake.hints.end() ? iterator->second.c_str() : nullptr;
}

void MaybeAttemptCreateDuringRestoration(FakeRuntimeBackend& fake)
{
    if (!fake.attemptCreateDuringRestoration || fake.quitCalls == 0 ||
        fake.attemptedCreateDuringRestoration)
    {
        return;
    }

    fake.attemptedCreateDuringRestoration = true;
    const auto result =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    if (!result.HasValue())
    {
        fake.restorationCreateError = result.GetError().GetCode();
    }
}

[[nodiscard]] bool FakeIsMainThread(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.mainThreadChecks;
    fake.calls.emplace_back("is-main-thread");
    return fake.mainThread;
}

[[nodiscard]] bool FakeHasInitializedSubsystems(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.initializedSubsystemChecks;
    fake.calls.emplace_back("was-init");
    return fake.initializedSubsystems;
}

[[nodiscard]] bool FakeHasExpectedRuntimeSubsystems(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.runtimeSubsystemChecks;
    fake.calls.emplace_back("has-runtime-subsystems");
    return fake.initializedSubsystems && fake.onlyRuntimeSubsystems;
}

[[nodiscard]] const char* FakeGetAppMetadataProperty(
    void* context, ApplicationMetadataProperty property)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-metadata");
    const std::optional<std::string>& value = fake.metadata[MetadataIndex(property)];
    return value.has_value() ? value->c_str() : nullptr;
}

[[nodiscard]] bool FakeSetAppMetadataProperty(void* context,
                                              ApplicationMetadataProperty property,
                                              const char* value)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.metadataCalls;
    fake.calls.emplace_back("set-metadata");

    if (fake.failingMetadataProperty == property && fake.metadataFailuresRemaining > 0)
    {
        --fake.metadataFailuresRemaining;
        static_cast<void>(SDL_SetError("synthetic metadata failure"));
        return false;
    }

    fake.metadata[MetadataIndex(property)] =
        value != nullptr ? std::optional<std::string>{value} : std::nullopt;
    return true;
}

[[nodiscard]] const char* FakeGetHint(void* context, const char* name)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back(std::string{"get-hint:"} + name);
    return GetHintValue(fake, name);
}

[[nodiscard]] bool FakeSetHintOverride(void* context, const char* name, const char* value)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back(std::string{"set-hint:"} + name + "=" + value);
    MaybeAttemptCreateDuringRestoration(fake);

    if (fake.failingHint == name && fake.hintFailuresRemaining > 0)
    {
        --fake.hintFailuresRemaining;
        static_cast<void>(SDL_SetError("synthetic hint failure"));
        return false;
    }

    fake.hints[name] = value;
    return true;
}

[[nodiscard]] bool FakeResetHint(void* context, const char* name)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back(std::string{"reset-hint:"} + name);
    MaybeAttemptCreateDuringRestoration(fake);
    return fake.hints.erase(name) == 1;
}

[[nodiscard]] bool FakeInitializeVideo(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.initializeVideoCalls;
    fake.calls.emplace_back("initialize-video");
    fake.policiesObservedDuringInitialization =
        GetHintValue(fake, pond::platform::detail::kMouseFocusClickThroughHint) ==
            std::string_view{"1"} &&
        GetHintValue(fake, pond::platform::detail::kMouseAutoCaptureHint) ==
            std::string_view{"0"};
    fake.metadataObservedDuringInitialization =
        fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)].has_value();

    if (!fake.initializeVideoSucceeds)
    {
        static_cast<void>(SDL_SetError("synthetic video initialization failure"));
        return false;
    }

    fake.initializedSubsystems = true;
    return true;
}

void FakeQuit(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.quitCalls;
    fake.calls.emplace_back("quit");
    fake.initializedSubsystems = false;
    fake.metadata = {};
    fake.hints.clear();
}

[[nodiscard]] std::uint64_t FakeGetTicksNanoseconds(void* context)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.tickCalls;
    fake.calls.emplace_back("ticks");
    return fake.ticks;
}

[[nodiscard]] void* FakeCreateWindow(void* context,
                                     const BackendWindowCreateDesc& desc)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.createWindowCalls;
    fake.calls.emplace_back("create-window");
    if (FailWindowOperation(fake, "create-window"))
    {
        return nullptr;
    }

    const std::uint32_t backendId =
        fake.scriptedBackendWindowIdIndex < fake.scriptedBackendWindowIds.size()
          ? fake.scriptedBackendWindowIds[fake.scriptedBackendWindowIdIndex++]
          : fake.nextBackendWindowId++;
    fake.windows.emplace_back(FakeWindow{
        .backendId = backendId,
        .title = desc.title,
        .width = desc.width,
        .height = desc.height,
        .pixelWidth = desc.width,
        .pixelHeight = desc.height,
        .backendDisplayId = fake.defaultWindowDisplayId,
        .pixelDensity = fake.defaultWindowPixelDensity,
        .displayScale = fake.defaultWindowDisplayScale,
        .resizable = desc.resizable,
        .highPixelDensity = desc.highPixelDensity,
        .graphicsCompatibility = desc.graphicsCompatibility});
    return &fake.windows.back();
}

void FakeDestroyWindow(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.destroyWindowCalls;
    fake.calls.emplace_back("destroy-window");
    if (fake.destroyOverwritesError)
    {
        static_cast<void>(SDL_SetError("cleanup overwrote the primary error"));
    }

    const auto iterator = std::ranges::find_if(
        fake.windows,
        [window](const FakeWindow& candidate)
        {
            return &candidate == window;
        });
    if (iterator != fake.windows.end())
    {
        fake.windows.erase(iterator);
    }
}

[[nodiscard]] std::uint32_t FakeGetWindowId(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-window-id");
    return FailWindowOperation(fake, "get-window-id")
             ? 0
             : GetFakeWindow(window).backendId;
}

[[nodiscard]] const char* FakeGetWindowTitle(void* context, void* window)
{
    GetFake(context).calls.emplace_back("get-window-title");
    return GetFakeWindow(window).title.c_str();
}

[[nodiscard]] bool FakeSetWindowTitle(void* context, void* window,
                                      const char* title)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-window-title");
    if (FailWindowOperation(fake, "set-window-title"))
    {
        return false;
    }
    GetFakeWindow(window).title = title;
    return true;
}

[[nodiscard]] bool FakeGetWindowPosition(void* context, void* window, int* x,
                                         int* y)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-window-position");
    if (FailWindowOperation(fake, "get-window-position"))
    {
        return false;
    }
    const FakeWindow& fakeWindow = GetFakeWindow(window);
    *x = fakeWindow.x;
    *y = fakeWindow.y;
    return true;
}

[[nodiscard]] bool FakeSetWindowPosition(void* context, void* window, int x,
                                         int y)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-window-position");
    if (FailWindowOperation(fake, "set-window-position"))
    {
        return false;
    }
    FakeWindow& fakeWindow = GetFakeWindow(window);
    fakeWindow.x = x;
    fakeWindow.y = y;
    return true;
}

[[nodiscard]] bool FakeGetWindowSize(void* context, void* window, int* width,
                                     int* height)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-window-size");
    if (FailWindowOperation(fake, "get-window-size"))
    {
        return false;
    }
    const FakeWindow& fakeWindow = GetFakeWindow(window);
    *width = fakeWindow.width;
    *height = fakeWindow.height;
    return true;
}

[[nodiscard]] bool FakeGetWindowSizeInPixels(void* context, void* window,
                                             int* width, int* height)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-window-size-in-pixels");
    if (FailWindowOperation(fake, "get-window-size-in-pixels"))
    {
        return false;
    }
    const FakeWindow& fakeWindow = GetFakeWindow(window);
    *width = fakeWindow.pixelWidth;
    *height = fakeWindow.pixelHeight;
    return true;
}

[[nodiscard]] bool FakeSetWindowSize(void* context, void* window, int width,
                                     int height)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-window-size");
    if (FailWindowOperation(fake, "set-window-size"))
    {
        return false;
    }
    FakeWindow& fakeWindow = GetFakeWindow(window);
    fakeWindow.width = width;
    fakeWindow.height = height;
    return true;
}

[[nodiscard]] bool FakeSetWindowMinimumSize(void* context, void* window,
                                            int width, int height)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("set-window-minimum-size");
    if (FailWindowOperation(fake, "set-window-minimum-size"))
    {
        return false;
    }
    FakeWindow& fakeWindow = GetFakeWindow(window);
    fakeWindow.minimumWidth = width;
    fakeWindow.minimumHeight = height;
    return true;
}

[[nodiscard]] bool FakeShowWindow(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("show-window");
    if (FailWindowOperation(fake, "show-window"))
    {
        return false;
    }
    GetFakeWindow(window).visible = true;
    return true;
}

[[nodiscard]] bool FakeHideWindow(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("hide-window");
    if (FailWindowOperation(fake, "hide-window"))
    {
        return false;
    }
    GetFakeWindow(window).visible = false;
    return true;
}

[[nodiscard]] bool FakeEnumerateDisplays(
    void* context, std::vector<std::uint32_t>& displayIds)
{
    FakeRuntimeBackend& fake = GetFake(context);
    ++fake.enumerateDisplayCalls;
    fake.calls.emplace_back("enumerate-displays");
    if (FailDisplayOperation(fake, "enumerate-displays"))
    {
        return false;
    }

    displayIds = fake.connectedDisplayIds;
    return true;
}

[[nodiscard]] const char* FakeGetDisplayName(void* context,
                                             std::uint32_t backendDisplayId)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-display-name");
    if (FailDisplayOperation(fake, "get-display-name"))
    {
        return nullptr;
    }
    return GetFakeDisplay(fake, backendDisplayId).name.c_str();
}

[[nodiscard]] bool FakeGetDisplayBounds(
    void* context, std::uint32_t backendDisplayId,
    BackendScreenRectangle* bounds)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-display-bounds");
    if (FailDisplayOperation(fake, "get-display-bounds"))
    {
        return false;
    }
    *bounds = GetFakeDisplay(fake, backendDisplayId).bounds;
    return true;
}

[[nodiscard]] bool FakeGetDisplayUsableBounds(
    void* context, std::uint32_t backendDisplayId,
    BackendScreenRectangle* bounds)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-display-usable-bounds");
    if (FailDisplayOperation(fake, "get-display-usable-bounds"))
    {
        return false;
    }
    *bounds = GetFakeDisplay(fake, backendDisplayId).usableBounds;
    return true;
}

[[nodiscard]] bool FakeGetCurrentDisplayRefreshRate(
    void* context, std::uint32_t backendDisplayId, float* refreshRateHertz)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-current-display-refresh-rate");
    if (FailDisplayOperation(fake, "get-current-display-refresh-rate"))
    {
        return false;
    }
    *refreshRateHertz = GetFakeDisplay(fake, backendDisplayId).refreshRateHertz;
    return true;
}

[[nodiscard]] BackendDisplayOrientation FakeGetCurrentDisplayOrientation(
    void* context, std::uint32_t backendDisplayId)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-current-display-orientation");
    return GetFakeDisplay(fake, backendDisplayId).orientation;
}

[[nodiscard]] float FakeGetDisplayContentScale(
    void* context, std::uint32_t backendDisplayId)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-display-content-scale");
    if (FailDisplayOperation(fake, "get-display-content-scale"))
    {
        return 0.0F;
    }
    return GetFakeDisplay(fake, backendDisplayId).contentScale;
}

[[nodiscard]] std::uint32_t FakeGetDisplayForWindow(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-display-for-window");
    if (FailDisplayOperation(fake, "get-display-for-window"))
    {
        return 0;
    }
    return GetFakeWindow(window).backendDisplayId;
}

[[nodiscard]] float FakeGetWindowPixelDensity(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-window-pixel-density");
    if (FailDisplayOperation(fake, "get-window-pixel-density"))
    {
        return 0.0F;
    }
    return GetFakeWindow(window).pixelDensity;
}

[[nodiscard]] float FakeGetWindowDisplayScale(void* context, void* window)
{
    FakeRuntimeBackend& fake = GetFake(context);
    fake.calls.emplace_back("get-window-display-scale");
    if (FailDisplayOperation(fake, "get-window-display-scale"))
    {
        return 0.0F;
    }
    return GetFakeWindow(window).displayScale;
}

[[nodiscard]] PlatformRuntimeBackend MakeBackend(FakeRuntimeBackend& fake)
{
    return PlatformRuntimeBackend{
        &fake,
        FakeIsMainThread,
        FakeHasInitializedSubsystems,
        FakeHasExpectedRuntimeSubsystems,
        FakeGetAppMetadataProperty,
        FakeSetAppMetadataProperty,
        FakeGetHint,
        FakeSetHintOverride,
        FakeResetHint,
        FakeInitializeVideo,
        FakeQuit,
        FakeGetTicksNanoseconds};
}

[[nodiscard]] PlatformWindowBackend MakeWindowBackend(FakeRuntimeBackend& fake)
{
    return PlatformWindowBackend{
        &fake,
        FakeCreateWindow,
        FakeDestroyWindow,
        FakeGetWindowId,
        FakeGetWindowTitle,
        FakeSetWindowTitle,
        FakeGetWindowPosition,
        FakeSetWindowPosition,
        FakeGetWindowSize,
        FakeGetWindowSizeInPixels,
        FakeSetWindowSize,
        FakeSetWindowMinimumSize,
        FakeShowWindow,
        FakeHideWindow};
}

[[nodiscard]] PlatformDisplayBackend MakeDisplayBackend(FakeRuntimeBackend& fake)
{
    return PlatformDisplayBackend{
        &fake,
        FakeEnumerateDisplays,
        FakeGetDisplayName,
        FakeGetDisplayBounds,
        FakeGetDisplayUsableBounds,
        FakeGetCurrentDisplayRefreshRate,
        FakeGetCurrentDisplayOrientation,
        FakeGetDisplayContentScale,
        FakeGetDisplayForWindow,
        FakeGetWindowPixelDensity,
        FakeGetWindowDisplayScale};
}

class PlatformRuntimeBackendTests : public testing::Test
{
protected:
    void SetUp() override
    {
        static_cast<void>(SDL_ClearError());
        m_backend = MakeBackend(m_fake);
        m_windowBackend = MakeWindowBackend(m_fake);
        m_displayBackend = MakeDisplayBackend(m_fake);
        pond::platform::detail::SetPlatformRuntimeBackendForTesting(&m_backend);
        pond::platform::detail::SetPlatformWindowBackendForTesting(&m_windowBackend);
        pond::platform::detail::SetPlatformDisplayBackendForTesting(&m_displayBackend);
    }

    void TearDown() override
    {
        pond::platform::detail::SetPlatformDisplayBackendForTesting(nullptr);
        pond::platform::detail::SetPlatformWindowBackendForTesting(nullptr);
        pond::platform::detail::SetPlatformRuntimeBackendForTesting(nullptr);
        static_cast<void>(SDL_ClearError());
    }

    FakeRuntimeBackend m_fake;
    PlatformRuntimeBackend m_backend;
    PlatformWindowBackend m_windowBackend;
    PlatformDisplayBackend m_displayBackend;
};

TEST_F(PlatformRuntimeBackendTests, AppliesMetadataPoliciesAndProvidesTimestamps)
{
    m_fake.hints[pond::platform::detail::kMouseFocusClickThroughHint] = "previous";
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)] = "Prior App";
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)] = "1.0";
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)] =
        "org.ponder.prior";
    m_fake.ticks = 987'654'321;

    const pond::platform::PlatformRuntimeDesc desc{
        .applicationName = "Ponder Workbench",
        .applicationVersion = std::string{"2.4.1"},
        .applicationIdentifier = std::string{"org.ponder.workbench"}};

    {
        auto result = pond::platform::PlatformRuntime::Create(desc);
        ASSERT_TRUE(result.HasValue());
        pond::platform::PlatformRuntime runtime = std::move(result).GetValue();

        EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)],
                  "Ponder Workbench");
        EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)],
                  "2.4.1");
        EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)],
                  "org.ponder.workbench");
        EXPECT_EQ(GetHintValue(m_fake,
                               pond::platform::detail::kMouseFocusClickThroughHint),
                  std::string_view{"1"});
        EXPECT_EQ(GetHintValue(m_fake, pond::platform::detail::kMouseAutoCaptureHint),
                  std::string_view{"0"});
        EXPECT_TRUE(m_fake.policiesObservedDuringInitialization);
        EXPECT_TRUE(m_fake.metadataObservedDuringInitialization);
        EXPECT_EQ(runtime.Now().GetTimeSinceEpoch(), std::chrono::nanoseconds{987'654'321});
    }

    EXPECT_EQ(m_fake.quitCalls, 1);
    EXPECT_EQ(GetHintValue(m_fake, pond::platform::detail::kMouseFocusClickThroughHint),
              std::string_view{"previous"});
    EXPECT_EQ(GetHintValue(m_fake, pond::platform::detail::kMouseAutoCaptureHint),
              nullptr);
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)],
              "Prior App");
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)],
              "1.0");
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)],
              "org.ponder.prior");
    EXPECT_EQ(m_fake.runtimeSubsystemChecks, 1);
}

TEST_F(PlatformRuntimeBackendTests, RestoresAOriginallyPresentEmptyHint)
{
    m_fake.hints[pond::platform::detail::kMouseFocusClickThroughHint] = "";

    {
        auto result =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(result.HasValue());
        pond::platform::PlatformRuntime runtime = std::move(result).GetValue();
    }

    const auto iterator =
        m_fake.hints.find(pond::platform::detail::kMouseFocusClickThroughHint);
    ASSERT_NE(iterator, m_fake.hints.end());
    EXPECT_TRUE(iterator->second.empty());
}

TEST_F(PlatformRuntimeBackendTests, PassesAbsentOptionalMetadataAsNull)
{
    auto result =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(result.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(result).GetValue();

    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)], "ponder");
    EXPECT_FALSE(
        m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)].has_value());
    EXPECT_FALSE(
        m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)].has_value());
}

TEST_F(PlatformRuntimeBackendTests, DuplicateCreationDoesNoBackendWork)
{
    auto firstResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(firstResult.HasValue());
    pond::platform::PlatformRuntime first = std::move(firstResult).GetValue();
    const std::size_t callCount = m_fake.calls.size();

    pond::platform::PlatformRuntimeDesc invalidDesc;
    invalidDesc.applicationName.clear();
    const auto duplicateResult = pond::platform::PlatformRuntime::Create(invalidDesc);

    ASSERT_FALSE(duplicateResult.HasValue());
    EXPECT_EQ(duplicateResult.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::RuntimeAlreadyActive));
    EXPECT_EQ(m_fake.calls.size(), callCount);
}

TEST_F(PlatformRuntimeBackendTests, CompleteShutdownAllowsAnotherRuntime)
{
    {
        auto firstResult =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(firstResult.HasValue());
        pond::platform::PlatformRuntime first = std::move(firstResult).GetValue();
    }

    auto secondResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(secondResult.HasValue());
    pond::platform::PlatformRuntime second = std::move(secondResult).GetValue();

    EXPECT_EQ(m_fake.initializeVideoCalls, 2);
    EXPECT_EQ(m_fake.quitCalls, 1);
}

TEST_F(PlatformRuntimeBackendTests, RejectsInvalidMetadataBeforeCallingBackend)
{
    pond::platform::PlatformRuntimeDesc emptyName;
    emptyName.applicationName.clear();
    auto result = pond::platform::PlatformRuntime::Create(emptyName);
    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::InvalidArgument));

    pond::platform::PlatformRuntimeDesc emptyVersion;
    emptyVersion.applicationVersion = std::string{};
    result = pond::platform::PlatformRuntime::Create(emptyVersion);
    ASSERT_FALSE(result.HasValue());

    pond::platform::PlatformRuntimeDesc invalidUtf8;
    invalidUtf8.applicationIdentifier = std::string{"\xC0\xAF", 2};
    result = pond::platform::PlatformRuntime::Create(invalidUtf8);
    ASSERT_FALSE(result.HasValue());

    pond::platform::PlatformRuntimeDesc embeddedNull;
    embeddedNull.applicationName = std::string{"pon\0der", 7};
    result = pond::platform::PlatformRuntime::Create(embeddedNull);
    ASSERT_FALSE(result.HasValue());

    EXPECT_TRUE(m_fake.calls.empty());
}

TEST_F(PlatformRuntimeBackendTests, RejectsCreationOffTheBackendMainThread)
{
    m_fake.mainThread = false;

    EXPECT_THROW(
        static_cast<void>(
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{})),
        pond::core::PonderException);

    m_fake.mainThread = true;
    auto retry =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();
}

TEST_F(PlatformRuntimeBackendTests, RejectsWorkerThreadBeforeConsultingSdl)
{
    std::atomic_bool rejectedWorkerThread{false};
    std::thread worker{
        [&rejectedWorkerThread]()
        {
            try
            {
                static_cast<void>(pond::platform::PlatformRuntime::Create(
                    pond::platform::PlatformRuntimeDesc{}));
            }
            catch (const pond::core::PonderException&)
            {
                rejectedWorkerThread.store(true);
            }
        }};
    worker.join();

    EXPECT_TRUE(rejectedWorkerThread.load());
    EXPECT_TRUE(m_fake.calls.empty());

    auto retry =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();
}

TEST_F(PlatformRuntimeBackendTests, RejectsExternallyInitializedSdl)
{
    m_fake.initializedSubsystems = true;

    auto result =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_EQ(m_fake.initializeVideoCalls, 0);
    EXPECT_EQ(m_fake.quitCalls, 0);

    m_fake.initializedSubsystems = false;
    auto retry =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();
}

TEST_F(PlatformRuntimeBackendTests, RestoresHintsAfterHintFailureAndPermitsRetry)
{
    m_fake.hints[pond::platform::detail::kMouseFocusClickThroughHint] = "old-focus";
    m_fake.hints[pond::platform::detail::kMouseAutoCaptureHint] = "old-capture";
    m_fake.failingHint = pond::platform::detail::kMouseAutoCaptureHint;
    m_fake.hintFailuresRemaining = 1;

    auto result =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(result.GetError().GetMessage().find("synthetic hint failure"),
              std::string_view::npos);
    EXPECT_EQ(GetHintValue(m_fake, pond::platform::detail::kMouseFocusClickThroughHint),
              std::string_view{"old-focus"});
    EXPECT_EQ(GetHintValue(m_fake, pond::platform::detail::kMouseAutoCaptureHint),
              std::string_view{"old-capture"});
    EXPECT_EQ(m_fake.initializeVideoCalls, 0);

    auto retry =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();
}

TEST_F(PlatformRuntimeBackendTests, RollsBackMetadataFailureAndPermitsRetry)
{
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)] = "Prior App";
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)] = "1.0";
    m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)] =
        "org.ponder.prior";
    m_fake.failingMetadataProperty = ApplicationMetadataProperty::Version;
    m_fake.metadataFailuresRemaining = 1;

    auto result =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(result.GetError().GetMessage().find("synthetic metadata failure"),
              std::string_view::npos);
    EXPECT_EQ(m_fake.initializeVideoCalls, 0);
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Name)],
              "Prior App");
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Version)],
              "1.0");
    EXPECT_EQ(m_fake.metadata[MetadataIndex(ApplicationMetadataProperty::Identifier)],
              "org.ponder.prior");

    auto retry =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();
}

TEST_F(PlatformRuntimeBackendTests, CapturesInitializationFailureBeforeRollback)
{
    m_fake.hints[pond::platform::detail::kMouseFocusClickThroughHint] = "old-focus";
    m_fake.initializeVideoSucceeds = false;

    auto result =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(result.GetError().GetMessage().find("synthetic video initialization failure"),
              std::string_view::npos);
    EXPECT_EQ(m_fake.quitCalls, 1);
    EXPECT_EQ(GetHintValue(m_fake, pond::platform::detail::kMouseFocusClickThroughHint),
              std::string_view{"old-focus"});

    m_fake.initializeVideoSucceeds = true;
    auto retry =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(retry.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(retry).GetValue();
}

TEST_F(PlatformRuntimeBackendTests, MovesOwnershipWithoutDuplicateShutdown)
{
    {
        auto result =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(result.HasValue());
        pond::platform::PlatformRuntime original = std::move(result).GetValue();

        pond::platform::PlatformRuntime moved = std::move(original);
        EXPECT_THROW(static_cast<void>(original.Now()), pond::core::PonderException);
        EXPECT_EQ(m_fake.quitCalls, 0);

        moved = std::move(moved);
        EXPECT_EQ(moved.Now().GetTimeSinceEpoch(), std::chrono::nanoseconds{m_fake.ticks});

        original = std::move(moved);
        EXPECT_THROW(static_cast<void>(moved.Now()), pond::core::PonderException);
        EXPECT_EQ(original.Now().GetTimeSinceEpoch(), std::chrono::nanoseconds{m_fake.ticks});
        EXPECT_EQ(m_fake.quitCalls, 0);
    }

    EXPECT_EQ(m_fake.quitCalls, 1);
}

TEST_F(PlatformRuntimeBackendTests, RejectsWrongThreadAndOutOfRangeTimestamps)
{
    auto result =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(result.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(result).GetValue();

    std::atomic_bool rejectedWrongThread{false};
    std::thread worker{
        [&runtime, &rejectedWrongThread]()
        {
            try
            {
                static_cast<void>(runtime.Now());
            }
            catch (const pond::core::PonderException&)
            {
                rejectedWrongThread.store(true);
            }
        }};
    worker.join();

    EXPECT_TRUE(rejectedWrongThread.load());
    EXPECT_EQ(m_fake.tickCalls, 0);

    m_fake.ticks =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1U;
    EXPECT_THROW(static_cast<void>(runtime.Now()), pond::core::PonderException);
    EXPECT_EQ(m_fake.tickCalls, 1);
}

TEST_F(PlatformRuntimeBackendTests, HoldsSingletonUntilHintRestorationCompletes)
{
    m_fake.hints[pond::platform::detail::kMouseFocusClickThroughHint] = "old-focus";
    m_fake.attemptCreateDuringRestoration = true;

    {
        auto result =
            pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(result.HasValue());
        pond::platform::PlatformRuntime runtime = std::move(result).GetValue();
    }

    ASSERT_TRUE(m_fake.attemptedCreateDuringRestoration);
    ASSERT_TRUE(m_fake.restorationCreateError.has_value());
    EXPECT_EQ(*m_fake.restorationCreateError,
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::RuntimeAlreadyActive));
}

TEST_F(PlatformRuntimeBackendTests,
       CreatesMultipleWindowsWithMonotonicIdsAndReusedBackendIds)
{
    m_fake.scriptedBackendWindowIds = {41, 73, 73, 99};

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    const pond::platform::WindowDesc desc{
        .title = "Hidden Test Window",
        .logicalSize = {640, 480},
        .visible = false,
        .resizable = false,
        .highPixelDensity = false,
        .minimumLogicalSize = pond::platform::LogicalSize{320, 240},
        .graphicsCompatibility =
            pond::platform::WindowGraphicsCompatibility::Vulkan};

    auto firstResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(firstResult.HasValue());
    std::optional<pond::platform::Window> first;
    first.emplace(std::move(firstResult).GetValue());

    auto secondResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(secondResult.HasValue());
    std::optional<pond::platform::Window> second;
    second.emplace(std::move(secondResult).GetValue());

    EXPECT_EQ(first->GetId(), pond::platform::WindowId{1});
    EXPECT_EQ(second->GetId(), pond::platform::WindowId{2});
    ASSERT_EQ(m_fake.windows.size(), 2U);
    const FakeWindow& firstBackendWindow = m_fake.windows.front();
    EXPECT_EQ(firstBackendWindow.title, "Hidden Test Window");
    EXPECT_EQ(firstBackendWindow.width, 640);
    EXPECT_EQ(firstBackendWindow.height, 480);
    EXPECT_EQ(firstBackendWindow.minimumWidth, 320);
    EXPECT_EQ(firstBackendWindow.minimumHeight, 240);
    EXPECT_FALSE(firstBackendWindow.visible);
    EXPECT_FALSE(firstBackendWindow.resizable);
    EXPECT_FALSE(firstBackendWindow.highPixelDensity);
    EXPECT_EQ(firstBackendWindow.graphicsCompatibility,
              pond::platform::WindowGraphicsCompatibility::Vulkan);

    second.reset();
    EXPECT_EQ(m_fake.destroyWindowCalls, 1);

    auto thirdResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(thirdResult.HasValue());
    std::optional<pond::platform::Window> third;
    third.emplace(std::move(thirdResult).GetValue());
    EXPECT_EQ(third->GetId(), pond::platform::WindowId{3});

    first.reset();
    third.reset();
    EXPECT_TRUE(m_fake.windows.empty());
    EXPECT_EQ(m_fake.destroyWindowCalls, 3);

    auto fourthResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(fourthResult.HasValue());
    pond::platform::Window fourth = std::move(fourthResult).GetValue();
    EXPECT_EQ(fourth.GetId(), pond::platform::WindowId{4});
}

TEST_F(PlatformRuntimeBackendTests, RestartsProjectWindowIdsForANewRuntime)
{
    pond::platform::WindowDesc desc;
    desc.visible = false;

    {
        auto runtimeResult = pond::platform::PlatformRuntime::Create(
            pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(runtimeResult.HasValue());
        pond::platform::PlatformRuntime runtime =
            std::move(runtimeResult).GetValue();
        auto windowResult = runtime.CreateWindow(desc);
        ASSERT_TRUE(windowResult.HasValue());
        pond::platform::Window window = std::move(windowResult).GetValue();
        EXPECT_EQ(window.GetId(), pond::platform::WindowId{1});
    }

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    EXPECT_EQ(window.GetId(), pond::platform::WindowId{1});
}

TEST_F(PlatformRuntimeBackendTests, ValidatesWindowDescriptorsBeforeBackendCalls)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    const auto expectInvalid =
        [&runtime, this](const pond::platform::WindowDesc& desc)
        {
            const int callCount = m_fake.createWindowCalls;
            const auto result = runtime.CreateWindow(desc);
            EXPECT_FALSE(result.HasValue());
            if (!result.HasValue())
            {
                EXPECT_EQ(result.GetError().GetCode(),
                          pond::platform::ToErrorCode(
                              pond::platform::PlatformErrorCode::InvalidArgument));
            }
            EXPECT_EQ(m_fake.createWindowCalls, callCount);
        };

    pond::platform::WindowDesc desc;
    desc.logicalSize.width = 0;
    expectInvalid(desc);

    desc = {};
    desc.logicalSize.height = 0;
    expectInvalid(desc);

    desc = {};
    desc.logicalSize.width = std::numeric_limits<std::uint32_t>::max();
    expectInvalid(desc);

    desc = {};
    desc.minimumLogicalSize = pond::platform::LogicalSize{0, 100};
    expectInvalid(desc);

    desc = {};
    desc.minimumLogicalSize = pond::platform::LogicalSize{
        100, std::numeric_limits<std::uint32_t>::max()};
    expectInvalid(desc);

    desc = {};
    desc.title = std::string{"\xC0\xAF", 2};
    expectInvalid(desc);

    desc = {};
    desc.title = std::string{"bad\0title", 9};
    expectInvalid(desc);

    desc = {};
    desc.graphicsCompatibility =
        static_cast<pond::platform::WindowGraphicsCompatibility>(0xFF);
    expectInvalid(desc);

    desc = {};
    desc.title.clear();
    desc.visible = false;
    auto emptyTitleResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(emptyTitleResult.HasValue());
    pond::platform::Window emptyTitleWindow =
        std::move(emptyTitleResult).GetValue();
    EXPECT_TRUE(emptyTitleWindow.GetTitle().empty());
}

TEST_F(PlatformRuntimeBackendTests, RollsBackEveryWindowCreationFailure)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    m_fake.destroyOverwritesError = true;

    const auto expectFailure =
        [&runtime, this](std::string operation, pond::platform::WindowDesc desc,
                        int expectedDestroyCalls)
        {
            m_fake.failingWindowOperation = operation;
            m_fake.windowFailuresRemaining = 1;
            auto result = runtime.CreateWindow(desc);
            ASSERT_FALSE(result.HasValue());
            EXPECT_EQ(result.GetError().GetCode(),
                      pond::platform::ToErrorCode(
                          pond::platform::PlatformErrorCode::BackendFailure));
            EXPECT_NE(result.GetError().GetMessage().find("synthetic " + operation),
                      std::string_view::npos);
            EXPECT_EQ(m_fake.destroyWindowCalls, expectedDestroyCalls);
            EXPECT_TRUE(m_fake.windows.empty());
        };

    pond::platform::WindowDesc hiddenDesc;
    hiddenDesc.visible = false;
    expectFailure("create-window", hiddenDesc, 0);

    pond::platform::WindowDesc minimumDesc = hiddenDesc;
    minimumDesc.minimumLogicalSize = pond::platform::LogicalSize{100, 80};
    expectFailure("set-window-minimum-size", minimumDesc, 1);
    expectFailure("get-window-id", hiddenDesc, 2);

    pond::platform::WindowDesc visibleDesc = hiddenDesc;
    visibleDesc.visible = true;
    expectFailure("show-window", visibleDesc, 3);

    m_fake.failingWindowOperation.clear();
    auto retryResult = runtime.CreateWindow(hiddenDesc);
    ASSERT_TRUE(retryResult.HasValue());
    pond::platform::Window retry = std::move(retryResult).GetValue();
    EXPECT_EQ(retry.GetId(), pond::platform::WindowId{2});
}

TEST_F(PlatformRuntimeBackendTests, ShowsVisibleWindowsAfterNativeSetupCompletes)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.minimumLogicalSize = pond::platform::LogicalSize{320, 200};
    const std::size_t firstWindowCall = m_fake.calls.size();

    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const std::vector<std::string> expectedCalls{
        "create-window", "set-window-minimum-size", "get-window-id", "show-window"};
    ASSERT_GE(m_fake.calls.size(), firstWindowCall + expectedCalls.size());
    EXPECT_TRUE(std::ranges::equal(
        expectedCalls,
        std::span{m_fake.calls}.subspan(firstWindowCall, expectedCalls.size())));
    ASSERT_EQ(m_fake.windows.size(), 1U);
    EXPECT_TRUE(m_fake.windows.front().visible);
}

TEST_F(PlatformRuntimeBackendTests, ExposesOwnedBasicWindowOperations)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.title = "Initial";
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    FakeWindow& backendWindow = m_fake.windows.front();
    backendWindow.x = -120;
    backendWindow.y = 45;
    backendWindow.width = 700;
    backendWindow.height = 500;
    backendWindow.pixelWidth = 1400;
    backendWindow.pixelHeight = 1000;

    EXPECT_EQ(window.GetTitle(), "Initial");
    const std::string titleBuffer{"Updated plus ignored suffix"};
    ASSERT_TRUE(window.SetTitle(std::string_view{titleBuffer.data(), 7}).HasValue());
    EXPECT_EQ(window.GetTitle(), "Updated");

    auto position = window.GetPosition();
    ASSERT_TRUE(position.HasValue());
    EXPECT_EQ(position.GetValue(), (pond::platform::ScreenPosition{-120, 45}));
    ASSERT_TRUE(window.SetPosition({-55, 90}).HasValue());
    EXPECT_EQ(backendWindow.x, -55);
    EXPECT_EQ(backendWindow.y, 90);

    auto logicalSize = window.GetLogicalSize();
    ASSERT_TRUE(logicalSize.HasValue());
    EXPECT_EQ(logicalSize.GetValue(), (pond::platform::LogicalSize{700, 500}));
    auto pixelSize = window.GetPixelSize();
    ASSERT_TRUE(pixelSize.HasValue());
    EXPECT_EQ(pixelSize.GetValue(), (pond::platform::PixelSize{1400, 1000}));

    ASSERT_TRUE(window.SetLogicalSize({900, 600}).HasValue());
    EXPECT_EQ(backendWindow.width, 900);
    EXPECT_EQ(backendWindow.height, 600);
    ASSERT_TRUE(window.Show().HasValue());
    EXPECT_TRUE(backendWindow.visible);
    ASSERT_TRUE(window.Hide().HasValue());
    EXPECT_FALSE(backendWindow.visible);
    EXPECT_EQ(window.GetGraphicsCompatibility(),
              pond::platform::WindowGraphicsCompatibility::Default);
}

TEST_F(PlatformRuntimeBackendTests, RejectsInvalidWindowMutatorsWithoutBackendCalls)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto expectNoBackendCall =
        [this](const auto& result, std::size_t callCount)
        {
            EXPECT_FALSE(result.HasValue());
            if (!result.HasValue())
            {
                EXPECT_EQ(result.GetError().GetCode(),
                          pond::platform::ToErrorCode(
                              pond::platform::PlatformErrorCode::InvalidArgument));
            }
            EXPECT_EQ(m_fake.calls.size(), callCount);
        };

    std::size_t callCount = m_fake.calls.size();
    expectNoBackendCall(window.SetTitle(std::string{"\xC0\xAF", 2}), callCount);
    callCount = m_fake.calls.size();
    expectNoBackendCall(window.SetTitle(std::string{"bad\0title", 9}), callCount);
    callCount = m_fake.calls.size();
    expectNoBackendCall(window.SetLogicalSize({0, 100}), callCount);
    callCount = m_fake.calls.size();
    expectNoBackendCall(
        window.SetLogicalSize({std::numeric_limits<std::uint32_t>::max(), 100}),
        callCount);
    callCount = m_fake.calls.size();
    expectNoBackendCall(window.SetPosition({0x1FFF0000, 10}), callCount);
    callCount = m_fake.calls.size();
    expectNoBackendCall(window.SetPosition({10, 0x2FFF0000}), callCount);
}

TEST_F(PlatformRuntimeBackendTests, ReturnsContextualWindowOperationFailures)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto configureFailure =
        [this](std::string operation)
        {
            m_fake.failingWindowOperation = std::move(operation);
            m_fake.windowFailuresRemaining = 1;
        };
    const auto expectBackendFailure =
        [](const auto& result, std::string_view operation)
        {
            ASSERT_FALSE(result.HasValue());
            EXPECT_EQ(result.GetError().GetCode(),
                      pond::platform::ToErrorCode(
                          pond::platform::PlatformErrorCode::BackendFailure));
            EXPECT_NE(result.GetError().GetMessage().find(operation),
                      std::string_view::npos);
            EXPECT_NE(result.GetError().GetMessage().find("window 1"),
                      std::string_view::npos);
        };

    configureFailure("set-window-title");
    expectBackendFailure(window.SetTitle("new"), "SDL_SetWindowTitle");
    configureFailure("get-window-position");
    expectBackendFailure(window.GetPosition(), "SDL_GetWindowPosition");
    configureFailure("set-window-position");
    expectBackendFailure(window.SetPosition({1, 2}), "SDL_SetWindowPosition");
    configureFailure("get-window-size");
    expectBackendFailure(window.GetLogicalSize(), "SDL_GetWindowSize");
    configureFailure("get-window-size-in-pixels");
    expectBackendFailure(window.GetPixelSize(), "SDL_GetWindowSizeInPixels");
    configureFailure("set-window-size");
    expectBackendFailure(window.SetLogicalSize({200, 100}), "SDL_SetWindowSize");
    configureFailure("show-window");
    expectBackendFailure(window.Show(), "SDL_ShowWindow");
    configureFailure("hide-window");
    expectBackendFailure(window.Hide(), "SDL_HideWindow");

    m_fake.failingWindowOperation.clear();
    m_fake.windows.front().width = -1;
    expectBackendFailure(window.GetLogicalSize(), "SDL_GetWindowSize");
    m_fake.windows.front().width = 100;
    m_fake.windows.front().pixelHeight = -1;
    expectBackendFailure(window.GetPixelSize(), "SDL_GetWindowSizeInPixels");
}

TEST_F(PlatformRuntimeBackendTests, MovesWindowsWithoutChangingTheirStableIdentity)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto firstResult = runtime.CreateWindow(desc);
    auto secondResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(firstResult.HasValue());
    ASSERT_TRUE(secondResult.HasValue());
    pond::platform::Window first = std::move(firstResult).GetValue();
    pond::platform::Window second = std::move(secondResult).GetValue();

    pond::platform::Window moved = std::move(first);
    EXPECT_THROW(static_cast<void>(first.GetId()), pond::core::PonderException);
    EXPECT_EQ(moved.GetId(), pond::platform::WindowId{1});
    moved = std::move(moved);
    EXPECT_EQ(moved.GetId(), pond::platform::WindowId{1});

    second = std::move(moved);
    EXPECT_EQ(m_fake.destroyWindowCalls, 1);
    EXPECT_THROW(static_cast<void>(moved.GetId()), pond::core::PonderException);
    EXPECT_EQ(second.GetId(), pond::platform::WindowId{1});
}

TEST_F(PlatformRuntimeBackendTests, RejectsWindowUseFromTheWrongThread)
{
    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    const std::size_t callCount = m_fake.calls.size();

    std::atomic_bool rejected{false};
    std::thread worker{
        [&window, &rejected]()
        {
            try
            {
                static_cast<void>(window.SetTitle("wrong thread"));
            }
            catch (const pond::core::PonderException&)
            {
                rejected.store(true);
            }
        }};
    worker.join();

    EXPECT_TRUE(rejected.load());
    EXPECT_EQ(m_fake.calls.size(), callCount);
}

TEST_F(PlatformRuntimeBackendTests, EnumeratesOwnedDisplaySnapshots)
{
    m_fake.connectedDisplayIds = {41, 73};
    m_fake.displays.emplace(
        41, FakeDisplay{41,
                        "Primary Display",
                        {-1920, 0, 1920, 1080},
                        {-1920, 40, 1920, 1040},
                        59.94F,
                        BackendDisplayOrientation::Landscape,
                        1.25F});
    m_fake.displays.emplace(
        73, FakeDisplay{73,
                        "Portrait Display",
                        {0, -120, 1440, 2560},
                        {0, -80, 1440, 2520},
                        0.0F,
                        BackendDisplayOrientation::PortraitFlipped,
                        2.0F});

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto result = runtime.EnumerateDisplays();
    ASSERT_TRUE(result.HasValue()) << result.GetError().GetMessage();
    const std::vector<pond::platform::DisplayInfo>& displays = result.GetValue();
    ASSERT_EQ(displays.size(), 2U);

    EXPECT_EQ(displays[0].id, pond::platform::DisplayId{1});
    EXPECT_EQ(displays[0].name, "Primary Display");
    EXPECT_EQ(displays[0].bounds,
              (pond::platform::ScreenRectangle{{-1920, 0}, {1920, 1080}}));
    EXPECT_EQ(displays[0].usableBounds,
              (pond::platform::ScreenRectangle{{-1920, 40}, {1920, 1040}}));
    ASSERT_TRUE(displays[0].refreshRateHertz.has_value());
    EXPECT_FLOAT_EQ(*displays[0].refreshRateHertz, 59.94F);
    EXPECT_EQ(displays[0].orientation,
              pond::platform::DisplayOrientation::Landscape);
    EXPECT_FLOAT_EQ(displays[0].contentScale, 1.25F);

    EXPECT_EQ(displays[1].id, pond::platform::DisplayId{2});
    EXPECT_EQ(displays[1].name, "Portrait Display");
    EXPECT_EQ(displays[1].bounds,
              (pond::platform::ScreenRectangle{{0, -120}, {1440, 2560}}));
    EXPECT_EQ(displays[1].usableBounds,
              (pond::platform::ScreenRectangle{{0, -80}, {1440, 2520}}));
    EXPECT_FALSE(displays[1].refreshRateHertz.has_value());
    EXPECT_EQ(displays[1].orientation,
              pond::platform::DisplayOrientation::PortraitFlipped);
    EXPECT_FLOAT_EQ(displays[1].contentScale, 2.0F);

    m_fake.displays.at(41).name = "Backend Storage Changed";
    m_fake.displays.at(73).name.clear();
    EXPECT_EQ(displays[0].name, "Primary Display");
    EXPECT_EQ(displays[1].name, "Portrait Display");
}

TEST_F(PlatformRuntimeBackendTests,
       PreservesDisplayIdsAcrossReorderRemovalAndReconnect)
{
    m_fake.connectedDisplayIds = {10, 20};
    m_fake.displays.emplace(
        10, FakeDisplay{10, "First", {0, 0, 800, 600}, {0, 0, 800, 560},
                        60.0F, BackendDisplayOrientation::Landscape, 1.0F});
    m_fake.displays.emplace(
        20, FakeDisplay{20, "Second", {800, 0, 1024, 768}, {800, 0, 1024, 728},
                        75.0F, BackendDisplayOrientation::Landscape, 1.5F});

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    auto firstEnumeration = runtime.EnumerateDisplays();
    ASSERT_TRUE(firstEnumeration.HasValue());
    ASSERT_EQ(firstEnumeration.GetValue().size(), 2U);
    const pond::platform::DisplayId firstId = firstEnumeration.GetValue()[0].id;
    const pond::platform::DisplayId secondId = firstEnumeration.GetValue()[1].id;
    EXPECT_EQ(firstId, pond::platform::DisplayId{1});
    EXPECT_EQ(secondId, pond::platform::DisplayId{2});

    m_fake.connectedDisplayIds = {20, 10};
    auto reordered = runtime.EnumerateDisplays();
    ASSERT_TRUE(reordered.HasValue());
    ASSERT_EQ(reordered.GetValue().size(), 2U);
    EXPECT_EQ(reordered.GetValue()[0].id, secondId);
    EXPECT_EQ(reordered.GetValue()[1].id, firstId);

    m_fake.connectedDisplayIds = {20};
    auto stale = runtime.GetDisplayInfo(firstId);
    ASSERT_FALSE(stale.HasValue());
    EXPECT_EQ(stale.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::NotFound));

    m_fake.displays.emplace(
        30, FakeDisplay{30, "First Reconnected", {0, 0, 800, 600},
                        {0, 0, 800, 560}, 60.0F,
                        BackendDisplayOrientation::Landscape, 1.0F});
    m_fake.connectedDisplayIds = {20, 30};
    auto reconnected = runtime.EnumerateDisplays();
    ASSERT_TRUE(reconnected.HasValue());
    ASSERT_EQ(reconnected.GetValue().size(), 2U);
    EXPECT_EQ(reconnected.GetValue()[0].id, secondId);
    EXPECT_EQ(reconnected.GetValue()[1].id, pond::platform::DisplayId{3});
    EXPECT_NE(reconnected.GetValue()[1].id, firstId);

    auto invalid = runtime.GetDisplayInfo(pond::platform::DisplayId::Invalid());
    ASSERT_FALSE(invalid.HasValue());
    EXPECT_EQ(invalid.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::InvalidArgument));

    auto unknown = runtime.GetDisplayInfo(pond::platform::DisplayId{999});
    ASSERT_FALSE(unknown.HasValue());
    EXPECT_EQ(unknown.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::NotFound));
}

TEST_F(PlatformRuntimeBackendTests, RestartsProjectDisplayIdsForANewRuntime)
{
    m_fake.connectedDisplayIds = {81};
    m_fake.displays.emplace(
        81, FakeDisplay{81, "Display", {0, 0, 1280, 720}, {0, 0, 1280, 680},
                        60.0F, BackendDisplayOrientation::Landscape, 1.0F});

    {
        auto runtimeResult = pond::platform::PlatformRuntime::Create(
            pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(runtimeResult.HasValue());
        pond::platform::PlatformRuntime runtime =
            std::move(runtimeResult).GetValue();
        auto displays = runtime.EnumerateDisplays();
        ASSERT_TRUE(displays.HasValue());
        ASSERT_EQ(displays.GetValue().size(), 1U);
        EXPECT_EQ(displays.GetValue()[0].id, pond::platform::DisplayId{1});
    }

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    auto displays = runtime.EnumerateDisplays();
    ASSERT_TRUE(displays.HasValue());
    ASSERT_EQ(displays.GetValue().size(), 1U);
    EXPECT_EQ(displays.GetValue()[0].id, pond::platform::DisplayId{1});
}

TEST_F(PlatformRuntimeBackendTests, ReportsContextualDisplayBackendFailures)
{
    m_fake.connectedDisplayIds = {55};
    m_fake.displays.emplace(
        55, FakeDisplay{55, "Display", {0, 0, 1920, 1080}, {0, 0, 1920, 1040},
                        60.0F, BackendDisplayOrientation::Landscape, 1.0F});

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    auto initial = runtime.EnumerateDisplays();
    ASSERT_TRUE(initial.HasValue());
    const pond::platform::DisplayId displayId = initial.GetValue().front().id;

    m_fake.failingDisplayOperation = "enumerate-displays";
    m_fake.displayFailuresRemaining = 1;
    auto enumerationFailure = runtime.EnumerateDisplays();
    ASSERT_FALSE(enumerationFailure.HasValue());
    EXPECT_EQ(enumerationFailure.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(enumerationFailure.GetError().GetMessage().find("SDL_GetDisplays"),
              std::string_view::npos);
    EXPECT_NE(enumerationFailure.GetError().GetMessage().find(
                  "synthetic enumerate-displays failure"),
              std::string_view::npos);

    const auto expectQueryFailure =
        [&runtime, &displayId, this](std::string operation,
                                    std::string_view backendOperation)
        {
            m_fake.failingDisplayOperation = std::move(operation);
            m_fake.displayFailuresRemaining = 1;
            auto result = runtime.GetDisplayInfo(displayId);
            ASSERT_FALSE(result.HasValue());
            EXPECT_EQ(result.GetError().GetCode(),
                      pond::platform::ToErrorCode(
                          pond::platform::PlatformErrorCode::BackendFailure));
            EXPECT_NE(result.GetError().GetMessage().find(backendOperation),
                      std::string_view::npos);
            EXPECT_NE(result.GetError().GetMessage().find("display 1"),
                      std::string_view::npos);
        };

    expectQueryFailure("get-display-name", "SDL_GetDisplayName");
    expectQueryFailure("get-display-bounds", "SDL_GetDisplayBounds");
    expectQueryFailure("get-display-usable-bounds", "SDL_GetDisplayUsableBounds");
    expectQueryFailure("get-current-display-refresh-rate",
                       "SDL_GetCurrentDisplayMode");
    expectQueryFailure("get-display-content-scale", "SDL_GetDisplayContentScale");
}

TEST_F(PlatformRuntimeBackendTests, RejectsMalformedDisplayBackendData)
{
    m_fake.connectedDisplayIds = {91};
    m_fake.displays.emplace(
        91, FakeDisplay{91, "Display", {0, 0, 1600, 900}, {0, 0, 1600, 860},
                        0.0F, BackendDisplayOrientation::Unknown, 1.0F});

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    m_fake.connectedDisplayIds = {0};
    auto zeroId = runtime.EnumerateDisplays();
    ASSERT_FALSE(zeroId.HasValue());
    EXPECT_EQ(zeroId.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::BackendFailure));

    m_fake.connectedDisplayIds = {91, 91};
    auto duplicateId = runtime.EnumerateDisplays();
    ASSERT_FALSE(duplicateId.HasValue());
    EXPECT_EQ(duplicateId.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::BackendFailure));

    m_fake.connectedDisplayIds = {91};
    auto valid = runtime.EnumerateDisplays();
    ASSERT_TRUE(valid.HasValue());
    ASSERT_EQ(valid.GetValue().size(), 1U);
    const pond::platform::DisplayId displayId = valid.GetValue()[0].id;
    EXPECT_FALSE(valid.GetValue()[0].refreshRateHertz.has_value());

    const auto expectMalformed =
        [&runtime, &displayId](const auto& configure)
        {
            configure();
            auto result = runtime.GetDisplayInfo(displayId);
            EXPECT_FALSE(result.HasValue());
            if (!result.HasValue())
            {
                EXPECT_EQ(result.GetError().GetCode(),
                          pond::platform::ToErrorCode(
                              pond::platform::PlatformErrorCode::BackendFailure));
            }
        };

    expectMalformed(
        [this]()
        {
            m_fake.displays.at(91).bounds.width = -1;
        });
    m_fake.displays.at(91).bounds.width = 1600;

    expectMalformed(
        [this]()
        {
            m_fake.displays.at(91).usableBounds.height = -1;
        });
    m_fake.displays.at(91).usableBounds.height = 860;

    expectMalformed(
        [this]()
        {
            m_fake.displays.at(91).refreshRateHertz = -1.0F;
        });
    m_fake.displays.at(91).refreshRateHertz = 60.0F;

    expectMalformed(
        [this]()
        {
            m_fake.displays.at(91).refreshRateHertz =
                std::numeric_limits<float>::quiet_NaN();
        });
    m_fake.displays.at(91).refreshRateHertz = 60.0F;

    expectMalformed(
        [this]()
        {
            m_fake.displays.at(91).contentScale = -0.5F;
        });
    m_fake.displays.at(91).contentScale = 1.0F;

    expectMalformed(
        [this]()
        {
            m_fake.displays.at(91).contentScale =
                std::numeric_limits<float>::infinity();
        });
    m_fake.displays.at(91).contentScale = 1.0F;

    m_fake.displays.at(91).orientation =
        static_cast<BackendDisplayOrientation>(0xFF);
    auto unknownOrientation = runtime.GetDisplayInfo(displayId);
    ASSERT_TRUE(unknownOrientation.HasValue());
    EXPECT_EQ(unknownOrientation.GetValue().orientation,
              pond::platform::DisplayOrientation::Unknown);
}

TEST_F(PlatformRuntimeBackendTests,
       ConvertsSnapshotFailureToNotFoundWhenDisplayDisconnects)
{
    m_fake.connectedDisplayIds = {64};
    m_fake.displays.emplace(
        64, FakeDisplay{64, "Transient", {0, 0, 800, 600}, {0, 0, 800, 560},
                        60.0F, BackendDisplayOrientation::Landscape, 1.0F});

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    auto initial = runtime.EnumerateDisplays();
    ASSERT_TRUE(initial.HasValue());
    const pond::platform::DisplayId id = initial.GetValue().front().id;

    m_fake.failingDisplayOperation = "get-display-name";
    m_fake.displayFailuresRemaining = 1;
    m_fake.disconnectDisplayOnFailure = 64;
    auto result = runtime.GetDisplayInfo(id);

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::NotFound));
    EXPECT_EQ(m_fake.connectedDisplayIds.size(), 0U);
}

TEST_F(PlatformRuntimeBackendTests, ExposesWindowDisplayAndScaleQueries)
{
    m_fake.connectedDisplayIds = {77};
    m_fake.displays.emplace(
        77, FakeDisplay{77, "Window Display", {0, 0, 2560, 1440},
                        {0, 0, 2560, 1400}, 144.0F,
                        BackendDisplayOrientation::Landscape, 1.5F});
    m_fake.defaultWindowDisplayId = 77;
    m_fake.defaultWindowPixelDensity = 2.0F;
    m_fake.defaultWindowDisplayScale = 1.75F;

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto firstResult = runtime.CreateWindow(desc);
    auto secondResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(firstResult.HasValue());
    ASSERT_TRUE(secondResult.HasValue());
    pond::platform::Window first = std::move(firstResult).GetValue();
    pond::platform::Window second = std::move(secondResult).GetValue();

    auto firstDisplay = first.GetDisplayId();
    auto secondDisplay = second.GetDisplayId();
    ASSERT_TRUE(firstDisplay.HasValue());
    ASSERT_TRUE(secondDisplay.HasValue());
    EXPECT_EQ(firstDisplay.GetValue(), pond::platform::DisplayId{1});
    EXPECT_EQ(secondDisplay.GetValue(), firstDisplay.GetValue());

    auto density = first.GetPixelDensity();
    ASSERT_TRUE(density.HasValue());
    EXPECT_FLOAT_EQ(density.GetValue(), 2.0F);
    auto scale = first.GetDisplayScale();
    ASSERT_TRUE(scale.HasValue());
    EXPECT_FLOAT_EQ(scale.GetValue(), 1.75F);

    auto displays = runtime.EnumerateDisplays();
    ASSERT_TRUE(displays.HasValue());
    ASSERT_EQ(displays.GetValue().size(), 1U);
    EXPECT_EQ(displays.GetValue()[0].id, firstDisplay.GetValue());
}

TEST_F(PlatformRuntimeBackendTests, ReportsWindowDisplayAndScaleFailures)
{
    m_fake.connectedDisplayIds = {17};
    m_fake.displays.emplace(
        17, FakeDisplay{17, "Window Display", {0, 0, 1280, 720},
                        {0, 0, 1280, 680}, 60.0F,
                        BackendDisplayOrientation::Landscape, 1.0F});
    m_fake.defaultWindowDisplayId = 17;

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();

    const auto configureFailure =
        [this](std::string operation)
        {
            m_fake.failingDisplayOperation = std::move(operation);
            m_fake.displayFailuresRemaining = 1;
        };
    const auto expectBackendFailure =
        [](const auto& result, std::string_view operation)
        {
            ASSERT_FALSE(result.HasValue());
            EXPECT_EQ(result.GetError().GetCode(),
                      pond::platform::ToErrorCode(
                          pond::platform::PlatformErrorCode::BackendFailure));
            EXPECT_NE(result.GetError().GetMessage().find(operation),
                      std::string_view::npos);
            EXPECT_NE(result.GetError().GetMessage().find("window 1"),
                      std::string_view::npos);
        };

    configureFailure("get-display-for-window");
    expectBackendFailure(window.GetDisplayId(), "SDL_GetDisplayForWindow");
    configureFailure("get-window-pixel-density");
    expectBackendFailure(window.GetPixelDensity(), "SDL_GetWindowPixelDensity");
    configureFailure("get-window-display-scale");
    expectBackendFailure(window.GetDisplayScale(), "SDL_GetWindowDisplayScale");

    m_fake.windows.front().backendDisplayId = 999;
    auto disconnected = window.GetDisplayId();
    ASSERT_FALSE(disconnected.HasValue());
    EXPECT_EQ(disconnected.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::NotFound));

    m_fake.windows.front().pixelDensity = -1.0F;
    expectBackendFailure(window.GetPixelDensity(), "SDL_GetWindowPixelDensity");
    m_fake.windows.front().pixelDensity = 1.0F;
    m_fake.windows.front().displayScale =
        std::numeric_limits<float>::quiet_NaN();
    expectBackendFailure(window.GetDisplayScale(), "SDL_GetWindowDisplayScale");
}

TEST_F(PlatformRuntimeBackendTests, GuardsNewDisplayApisAfterMovesAndAcrossThreads)
{
    m_fake.connectedDisplayIds = {5};
    m_fake.displays.emplace(
        5, FakeDisplay{5, "Display", {0, 0, 1024, 768}, {0, 0, 1024, 728},
                       60.0F, BackendDisplayOrientation::Landscape, 1.0F});
    m_fake.defaultWindowDisplayId = 5;

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    auto initialDisplays = runtime.EnumerateDisplays();
    ASSERT_TRUE(initialDisplays.HasValue());
    const pond::platform::DisplayId displayId = initialDisplays.GetValue().front().id;

    pond::platform::PlatformRuntime movedRuntime = std::move(runtime);
    const std::size_t movedRuntimeCallCount = m_fake.calls.size();
    EXPECT_THROW(static_cast<void>(runtime.EnumerateDisplays()),
                 pond::core::PonderException);
    EXPECT_THROW(static_cast<void>(runtime.GetDisplayInfo(displayId)),
                 pond::core::PonderException);
    EXPECT_EQ(m_fake.calls.size(), movedRuntimeCallCount);

    std::atomic_bool rejectedRuntimeThread{false};
    std::thread runtimeWorker{
        [&movedRuntime, &rejectedRuntimeThread]()
        {
            try
            {
                static_cast<void>(movedRuntime.EnumerateDisplays());
            }
            catch (const pond::core::PonderException&)
            {
                rejectedRuntimeThread.store(true);
            }
        }};
    runtimeWorker.join();
    EXPECT_TRUE(rejectedRuntimeThread.load());
    EXPECT_EQ(m_fake.calls.size(), movedRuntimeCallCount);
}

TEST_F(PlatformRuntimeBackendTests, GuardsNewWindowDisplayApisAfterMoveAndAcrossThreads)
{
    m_fake.connectedDisplayIds = {6};
    m_fake.displays.emplace(
        6, FakeDisplay{6, "Display", {0, 0, 1024, 768}, {0, 0, 1024, 728},
                       60.0F, BackendDisplayOrientation::Landscape, 1.0F});
    m_fake.defaultWindowDisplayId = 6;

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue());
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();
    pond::platform::WindowDesc desc;
    desc.visible = false;
    auto windowResult = runtime.CreateWindow(desc);
    ASSERT_TRUE(windowResult.HasValue());
    pond::platform::Window window = std::move(windowResult).GetValue();
    pond::platform::Window movedWindow = std::move(window);

    const std::size_t callCount = m_fake.calls.size();
    EXPECT_THROW(static_cast<void>(window.GetDisplayId()),
                 pond::core::PonderException);
    EXPECT_THROW(static_cast<void>(window.GetPixelDensity()),
                 pond::core::PonderException);
    EXPECT_THROW(static_cast<void>(window.GetDisplayScale()),
                 pond::core::PonderException);
    EXPECT_EQ(m_fake.calls.size(), callCount);

    std::atomic_int rejectedCalls{};
    std::thread worker{
        [&movedWindow, &rejectedCalls]()
        {
            try
            {
                static_cast<void>(movedWindow.GetDisplayId());
            }
            catch (const pond::core::PonderException&)
            {
                ++rejectedCalls;
            }
            try
            {
                static_cast<void>(movedWindow.GetPixelDensity());
            }
            catch (const pond::core::PonderException&)
            {
                ++rejectedCalls;
            }
            try
            {
                static_cast<void>(movedWindow.GetDisplayScale());
            }
            catch (const pond::core::PonderException&)
            {
                ++rejectedCalls;
            }
        }};
    worker.join();

    EXPECT_EQ(rejectedCalls.load(), 3);
    EXPECT_EQ(m_fake.calls.size(), callCount);
}
} // namespace
