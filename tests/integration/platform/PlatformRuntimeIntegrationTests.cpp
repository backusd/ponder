#include <ponder/platform/PlatformError.hpp>
#include <ponder/platform/PlatformRuntime.hpp>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <gtest/gtest.h>

#include <cmath>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
constexpr char kFocusClickThroughHint[]{"SDL_MOUSE_FOCUS_CLICKTHROUGH"};
constexpr char kAutoCaptureHint[]{"SDL_MOUSE_AUTO_CAPTURE"};

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
        static_cast<void>(
            SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, nullptr));
        static_cast<void>(
            SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, nullptr));
        static_cast<void>(SDL_SetAppMetadataProperty(
            SDL_PROP_APP_METADATA_IDENTIFIER_STRING, nullptr));
        static_cast<void>(SDL_ClearError());
    }
};

TEST_F(PlatformRuntimeIntegrationTests, OwnsLiveSdlAndRestoresEffectiveGlobalState)
{
    ASSERT_TRUE(
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));
    ASSERT_TRUE(
        SDL_SetHintWithPriority(kFocusClickThroughHint, "prior-focus", SDL_HINT_OVERRIDE));
    ASSERT_TRUE(SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, "Prior App"));
    ASSERT_TRUE(SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, "1.0"));
    ASSERT_TRUE(SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING,
                                           "org.ponder.prior"));

    const pond::platform::PlatformRuntimeDesc desc{
        .applicationName = "Ponder Integration Test",
        .applicationVersion = std::string{"2.0"},
        .applicationIdentifier = std::string{"org.ponder.integration"}};

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
        EXPECT_STREQ(SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING),
                     "2.0");
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
    EXPECT_STREQ(SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING),
                 "Prior App");
    EXPECT_STREQ(SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING), "1.0");
    EXPECT_STREQ(SDL_GetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING),
                 "org.ponder.prior");
}

TEST_F(PlatformRuntimeIntegrationTests, ReportsLiveSdlVideoInitializationFailure)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,
                                        "ponder-driver-that-does-not-exist",
                                        SDL_HINT_OVERRIDE));

    const auto result =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetCode(),
              pond::platform::ToErrorCode(
                  pond::platform::PlatformErrorCode::BackendFailure));
    EXPECT_NE(result.GetError().GetMessage().find("SDL_Init"), std::string_view::npos);
    EXPECT_EQ(SDL_WasInit(0), 0U);
}

TEST_F(PlatformRuntimeIntegrationTests, OwnsMultipleLiveHiddenWindows)
{
    ASSERT_TRUE(
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE));

    auto runtimeResult =
        pond::platform::PlatformRuntime::Create(pond::platform::PlatformRuntimeDesc{});
    ASSERT_TRUE(runtimeResult.HasValue()) << runtimeResult.GetError().GetMessage();
    pond::platform::PlatformRuntime runtime = std::move(runtimeResult).GetValue();

    const pond::platform::WindowDesc desc{
        .title = "Live Hidden Window",
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

TEST_F(PlatformRuntimeIntegrationTests,
       ExposesLiveDisplaySnapshotsAndWindowDensity)
{
    ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy,offscreen",
                                        SDL_HINT_OVERRIDE));

    std::vector<pond::platform::DisplayInfo> ownedSnapshots;
    std::string firstDisplayName;
    {
        auto runtimeResult = pond::platform::PlatformRuntime::Create(
            pond::platform::PlatformRuntimeDesc{});
        ASSERT_TRUE(runtimeResult.HasValue())
            << runtimeResult.GetError().GetMessage();
        pond::platform::PlatformRuntime runtime =
            std::move(runtimeResult).GetValue();

        const char* const videoDriver = SDL_GetCurrentVideoDriver();
        ASSERT_NE(videoDriver, nullptr);
        EXPECT_TRUE(std::string_view{videoDriver} == "dummy" ||
                    std::string_view{videoDriver} == "offscreen");

        auto displaysResult = runtime.EnumerateDisplays();
        ASSERT_TRUE(displaysResult.HasValue())
            << displaysResult.GetError().GetMessage();
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
            ASSERT_TRUE(infoResult.HasValue())
                << infoResult.GetError().GetMessage();
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
            .graphicsCompatibility =
                pond::platform::WindowGraphicsCompatibility::Default};

        auto windowResult = runtime.CreateWindow(desc);
        ASSERT_TRUE(windowResult.HasValue())
            << windowResult.GetError().GetMessage();
        pond::platform::Window window = std::move(windowResult).GetValue();

        auto logicalSizeResult = window.GetLogicalSize();
        ASSERT_TRUE(logicalSizeResult.HasValue())
            << logicalSizeResult.GetError().GetMessage();
        auto pixelSizeResult = window.GetPixelSize();
        ASSERT_TRUE(pixelSizeResult.HasValue())
            << pixelSizeResult.GetError().GetMessage();
        EXPECT_GT(logicalSizeResult.GetValue().width, 0U);
        EXPECT_GT(logicalSizeResult.GetValue().height, 0U);
        EXPECT_GT(pixelSizeResult.GetValue().width, 0U);
        EXPECT_GT(pixelSizeResult.GetValue().height, 0U);

        auto pixelDensityResult = window.GetPixelDensity();
        ASSERT_TRUE(pixelDensityResult.HasValue())
            << pixelDensityResult.GetError().GetMessage();
        EXPECT_TRUE(std::isfinite(pixelDensityResult.GetValue()));
        EXPECT_GT(pixelDensityResult.GetValue(), 0.0F);

        auto displayScaleResult = window.GetDisplayScale();
        ASSERT_TRUE(displayScaleResult.HasValue())
            << displayScaleResult.GetError().GetMessage();
        EXPECT_TRUE(std::isfinite(displayScaleResult.GetValue()));
        EXPECT_GT(displayScaleResult.GetValue(), 0.0F);

        auto windowDisplayResult = window.GetDisplayId();
        ASSERT_TRUE(windowDisplayResult.HasValue())
            << windowDisplayResult.GetError().GetMessage();
        EXPECT_TRUE(displayIds.contains(windowDisplayResult.GetValue()));

        auto windowDisplayInfoResult =
            runtime.GetDisplayInfo(windowDisplayResult.GetValue());
        ASSERT_TRUE(windowDisplayInfoResult.HasValue())
            << windowDisplayInfoResult.GetError().GetMessage();
        EXPECT_EQ(windowDisplayInfoResult.GetValue().id,
                  windowDisplayResult.GetValue());
    }

    ASSERT_FALSE(ownedSnapshots.empty());
    EXPECT_EQ(ownedSnapshots.front().name, firstDisplayName);
    EXPECT_TRUE(ownedSnapshots.front().id.IsValid());
}
} // namespace
