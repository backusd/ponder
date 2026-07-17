#include <ponder/ui/paint/PaintRecorder.hpp>

#include <concepts>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

namespace
{
using pond::ui::paint::FillRectangleCommand;
using pond::ui::paint::PaintCommandKind;
using pond::ui::paint::PaintCommandRecord;
using pond::ui::paint::PaintRecorder;
using pond::ui::paint::PaintRecorderState;
using pond::ui::paint::PushClipRectangleCommand;
using pond::ui::paint::SealedPaintList;

static_assert(!std::copy_constructible<PaintRecorder>);
static_assert(std::move_constructible<PaintRecorder>);
static_assert(!std::copy_constructible<SealedPaintList>);
static_assert(std::move_constructible<SealedPaintList>);
static_assert(std::is_same_v<decltype(std::declval<const SealedPaintList&>().GetCommands()),
                             std::span<const PaintCommandRecord>>);
static_assert(std::is_same_v<decltype(std::declval<const SealedPaintList&>().GetFillRectangles()),
                             std::span<const FillRectangleCommand>>);
static_assert(
    std::is_same_v<decltype(std::declval<const SealedPaintList&>().GetPushClipRectangles()),
                   std::span<const PushClipRectangleCommand>>);

[[nodiscard]] pond::ui::SrgbStraightAlphaColor MakeColor(float red, float green, float blue,
                                                         float alpha)
{
    auto color = pond::ui::MakeSrgbStraightAlphaColor(red, green, blue, alpha);
    EXPECT_TRUE(color.HasValue());
    return *color;
}

[[nodiscard]] pond::ui::LogicalRect MakeRect(float left, float top, float right, float bottom)
{
    auto rect = pond::ui::MakeLogicalRectFromEdges(left, top, right, bottom);
    EXPECT_TRUE(rect.HasValue());
    return *rect;
}

void ExpectUiErrorCode(const pond::core::Error& error, pond::ui::UiErrorCode code)
{
    EXPECT_EQ(error.GetCode(), pond::ui::ToErrorCode(code));
}

[[nodiscard]] pond::ui::UiHardLimits MakeRecorderTestLimits()
{
    pond::ui::UiHardLimits limits = pond::ui::kDefaultUiHardLimits;
    limits.maxPaintCommandCount = 64U;
    limits.maxPaintCommandPayloadBytes = 4096U;
    limits.maxClipDepth = 8U;
    return limits;
}
} // namespace

TEST(UiPaintRecorderTests, RecordsOrderedMixedCommandsWithStableIndices)
{
    PaintRecorder recorder{MakeRecorderTestLimits()};

    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(0.0F, 0.0F, 10.0F, 20.0F), MakeColor(1.0F, 0.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(1.0F, 2.0F, 9.0F, 18.0F)).HasValue());
    ASSERT_TRUE(
        recorder.FillRectangle(MakeRect(2.0F, 3.0F, 8.0F, 17.0F), MakeColor(0.0F, 1.0F, 0.0F, 0.5F))
            .HasValue());
    ASSERT_TRUE(recorder.PopClip().HasValue());

    auto sealedResult = recorder.Seal();
    ASSERT_TRUE(sealedResult.HasValue());
    SealedPaintList sealed = std::move(sealedResult).GetValue();

    const std::span<const PaintCommandRecord> commands = sealed.GetCommands();
    ASSERT_EQ(commands.size(), 4U);
    EXPECT_EQ(commands[0], (PaintCommandRecord{0U, PaintCommandKind::FillRectangle, 0U}));
    EXPECT_EQ(commands[1], (PaintCommandRecord{1U, PaintCommandKind::PushClipRectangle, 0U}));
    EXPECT_EQ(commands[2], (PaintCommandRecord{2U, PaintCommandKind::FillRectangle, 1U}));
    EXPECT_EQ(commands[3], (PaintCommandRecord{3U, PaintCommandKind::PopClip,
                                               pond::ui::paint::kNoPaintPayloadIndex}));

    const std::span<const FillRectangleCommand> fills = sealed.GetFillRectangles();
    ASSERT_EQ(fills.size(), 2U);
    EXPECT_EQ(fills[0].rectangle, MakeRect(0.0F, 0.0F, 10.0F, 20.0F));
    EXPECT_NEAR(fills[0].color.red, 1.0F, 0.000001F);
    EXPECT_NEAR(fills[0].color.alpha, 1.0F, 0.000001F);
    EXPECT_FALSE(fills[0].isZeroArea);
    EXPECT_FALSE(fills[0].isTransparent);
    EXPECT_EQ(sealed.GetPushClipRectangles()[0].rectangle, MakeRect(1.0F, 2.0F, 9.0F, 18.0F));

    const auto stats = sealed.GetStats();
    EXPECT_EQ(stats.state, PaintRecorderState::Sealed);
    EXPECT_EQ(stats.commandCount, 4U);
    EXPECT_EQ(stats.fillRectangleCount, 2U);
    EXPECT_EQ(stats.pushClipRectangleCount, 1U);
    EXPECT_EQ(stats.popClipCount, 1U);
    EXPECT_EQ(stats.clipDepth, 1U);
    EXPECT_EQ(stats.maxClipDepthObserved, 2U);
}

TEST(UiPaintRecorderTests, RejectsInvalidFillsBeforeStateChange)
{
    PaintRecorder recorder{MakeRecorderTestLimits()};

    const auto negativeExtent = recorder.FillRectangle(
        pond::ui::LogicalRect{.origin = pond::ui::LogicalPoint{},
                              .size = pond::ui::LogicalSize{.width = -1.0F, .height = 2.0F}},
        MakeColor(1.0F, 1.0F, 1.0F, 1.0F));
    ASSERT_FALSE(negativeExtent.HasValue());
    ExpectUiErrorCode(negativeExtent.GetError(), pond::ui::UiErrorCode::InvalidPaintValue);

    const auto nonFinite = recorder.FillRectangle(
        pond::ui::LogicalRect{
            .origin = pond::ui::LogicalPoint{.x = 0.0F, .y = 0.0F},
            .size = pond::ui::LogicalSize{.width = std::numeric_limits<float>::infinity(),
                                          .height = 2.0F}},
        MakeColor(1.0F, 1.0F, 1.0F, 1.0F));
    ASSERT_FALSE(nonFinite.HasValue());
    ExpectUiErrorCode(nonFinite.GetError(), pond::ui::UiErrorCode::InvalidPaintValue);

    const auto invalidColor = recorder.FillRectangle(
        MakeRect(0.0F, 0.0F, 1.0F, 1.0F),
        pond::ui::SrgbStraightAlphaColor{.red = 1.1F, .green = 0.0F, .blue = 0.0F, .alpha = 1.0F});
    ASSERT_FALSE(invalidColor.HasValue());
    ExpectUiErrorCode(invalidColor.GetError(), pond::ui::UiErrorCode::InvalidPaintValue);

    EXPECT_EQ(recorder.GetSnapshot().stats.commandCount, 0U);
    EXPECT_EQ(recorder.GetSnapshot().stats.payloadBytes, 0U);
}

TEST(UiPaintRecorderTests, RecordsZeroAreaAndTransparentFillsAsValidatedNoOps)
{
    PaintRecorder recorder{MakeRecorderTestLimits()};

    ASSERT_TRUE(
        recorder.FillRectangle(MakeRect(0.0F, 0.0F, 0.0F, 10.0F), MakeColor(1.0F, 0.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(
        recorder.FillRectangle(MakeRect(1.0F, 1.0F, 4.0F, 5.0F), MakeColor(0.0F, 0.0F, 1.0F, 0.0F))
            .HasValue());

    auto sealedResult = recorder.Seal();
    ASSERT_TRUE(sealedResult.HasValue());
    SealedPaintList sealed = std::move(sealedResult).GetValue();

    const std::span<const FillRectangleCommand> fills = sealed.GetFillRectangles();
    ASSERT_EQ(fills.size(), 2U);
    EXPECT_TRUE(fills[0].isZeroArea);
    EXPECT_FALSE(fills[0].isTransparent);
    EXPECT_FALSE(fills[1].isZeroArea);
    EXPECT_TRUE(fills[1].isTransparent);
    EXPECT_EQ(sealed.GetCommands()[0].index, 0U);
    EXPECT_EQ(sealed.GetCommands()[1].index, 1U);
}

TEST(UiPaintRecorderTests, EnforcesClipSentinelAndFinalBalanceTransactionally)
{
    PaintRecorder recorder{MakeRecorderTestLimits()};

    const pond::core::VoidResult underflow = recorder.PopClip();
    ASSERT_FALSE(underflow.HasValue());
    ExpectUiErrorCode(underflow.GetError(), pond::ui::UiErrorCode::UnbalancedPaintState);
    EXPECT_EQ(recorder.GetSnapshot().stats.commandCount, 0U);
    EXPECT_EQ(recorder.GetSnapshot().stats.clipDepth, 1U);

    ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(0.0F, 0.0F, 10.0F, 10.0F)).HasValue());
    ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(1.0F, 1.0F, 9.0F, 9.0F)).HasValue());
    EXPECT_EQ(recorder.GetSnapshot().stats.clipDepth, 3U);
    EXPECT_EQ(recorder.GetSnapshot().stats.maxClipDepthObserved, 3U);

    auto failedSeal = recorder.Seal();
    ASSERT_FALSE(failedSeal.HasValue());
    ExpectUiErrorCode(failedSeal.GetError(), pond::ui::UiErrorCode::UnbalancedPaintState);
    EXPECT_TRUE(recorder.IsOpen());
    EXPECT_EQ(recorder.GetSnapshot().stats.clipDepth, 3U);

    ASSERT_TRUE(recorder.PopClip().HasValue());
    ASSERT_TRUE(recorder.PopClip().HasValue());
    auto sealed = recorder.Seal();
    ASSERT_TRUE(sealed.HasValue());
    EXPECT_TRUE(recorder.IsSealed());
    EXPECT_EQ(sealed->GetStats().popClipCount, 2U);
}

TEST(UiPaintRecorderTests, RejectsMutationAfterSealUntilReset)
{
    PaintRecorder recorder{MakeRecorderTestLimits()};
    ASSERT_TRUE(
        recorder.FillRectangle(MakeRect(0.0F, 0.0F, 1.0F, 1.0F), MakeColor(1.0F, 1.0F, 1.0F, 1.0F))
            .HasValue());
    auto sealed = recorder.Seal();
    ASSERT_TRUE(sealed.HasValue());

    const auto fillAfterSeal =
        recorder.FillRectangle(MakeRect(0.0F, 0.0F, 1.0F, 1.0F), MakeColor(1.0F, 1.0F, 1.0F, 1.0F));
    ASSERT_FALSE(fillAfterSeal.HasValue());
    ExpectUiErrorCode(fillAfterSeal.GetError(), pond::ui::UiErrorCode::InvalidPaintState);

    const auto pushAfterSeal = recorder.PushClipRectangle(MakeRect(0.0F, 0.0F, 1.0F, 1.0F));
    ASSERT_FALSE(pushAfterSeal.HasValue());
    ExpectUiErrorCode(pushAfterSeal.GetError(), pond::ui::UiErrorCode::InvalidPaintState);

    const auto popAfterSeal = recorder.PopClip();
    ASSERT_FALSE(popAfterSeal.HasValue());
    ExpectUiErrorCode(popAfterSeal.GetError(), pond::ui::UiErrorCode::InvalidPaintState);

    recorder.Reset();
    EXPECT_TRUE(recorder.IsOpen());
    ASSERT_TRUE(
        recorder.FillRectangle(MakeRect(2.0F, 2.0F, 3.0F, 3.0F), MakeColor(0.0F, 1.0F, 1.0F, 1.0F))
            .HasValue());
    auto resealed = recorder.Seal();
    ASSERT_TRUE(resealed.HasValue());
    EXPECT_EQ(resealed->GetCommands()[0].index, 0U);
}

TEST(UiPaintRecorderTests, EnforcesCommandPayloadAndClipDepthLimitsBeforeGrowth)
{
    pond::ui::UiHardLimits commandLimited = MakeRecorderTestLimits();
    commandLimited.maxPaintCommandCount = 1U;
    PaintRecorder commandRecorder{commandLimited};
    ASSERT_TRUE(
        commandRecorder
            .FillRectangle(MakeRect(0.0F, 0.0F, 1.0F, 1.0F), MakeColor(1.0F, 0.0F, 0.0F, 1.0F))
            .HasValue());
    const auto commandLimit = commandRecorder.FillRectangle(MakeRect(1.0F, 1.0F, 2.0F, 2.0F),
                                                            MakeColor(0.0F, 1.0F, 0.0F, 1.0F));
    ASSERT_FALSE(commandLimit.HasValue());
    ExpectUiErrorCode(commandLimit.GetError(), pond::ui::UiErrorCode::LimitExceeded);
    EXPECT_EQ(commandRecorder.GetSnapshot().stats.commandCount, 1U);

    pond::ui::UiHardLimits payloadLimited = MakeRecorderTestLimits();
    payloadLimited.maxPaintCommandPayloadBytes = sizeof(FillRectangleCommand) - 1U;
    PaintRecorder payloadRecorder{payloadLimited};
    const auto payloadLimit = payloadRecorder.FillRectangle(MakeRect(0.0F, 0.0F, 1.0F, 1.0F),
                                                            MakeColor(1.0F, 0.0F, 0.0F, 1.0F));
    ASSERT_FALSE(payloadLimit.HasValue());
    ExpectUiErrorCode(payloadLimit.GetError(), pond::ui::UiErrorCode::LimitExceeded);
    EXPECT_EQ(payloadRecorder.GetSnapshot().stats.commandCount, 0U);
    EXPECT_EQ(payloadRecorder.GetSnapshot().stats.payloadBytes, 0U);

    pond::ui::UiHardLimits clipLimited = MakeRecorderTestLimits();
    clipLimited.maxClipDepth = 1U;
    PaintRecorder clipRecorder{clipLimited};
    const auto clipLimit = clipRecorder.PushClipRectangle(MakeRect(0.0F, 0.0F, 1.0F, 1.0F));
    ASSERT_FALSE(clipLimit.HasValue());
    ExpectUiErrorCode(clipLimit.GetError(), pond::ui::UiErrorCode::LimitExceeded);
    EXPECT_EQ(clipRecorder.GetSnapshot().stats.commandCount, 0U);
    EXPECT_EQ(clipRecorder.GetSnapshot().stats.clipDepth, 1U);
}

TEST(UiPaintRecorderTests, MovesRecorderAndSealedListWithoutBorrowingSourceStorage)
{
    PaintRecorder recorder{MakeRecorderTestLimits()};
    ASSERT_TRUE(
        recorder.FillRectangle(MakeRect(0.0F, 0.0F, 5.0F, 5.0F), MakeColor(1.0F, 0.0F, 1.0F, 1.0F))
            .HasValue());

    PaintRecorder movedRecorder = std::move(recorder);
    EXPECT_TRUE(recorder.IsOpen());
    const auto movedFromRecorder = recorder.GetSnapshot();
    EXPECT_EQ(movedFromRecorder.stats.commandCount, 0U);
    EXPECT_EQ(movedFromRecorder.stats.payloadBytes, 0U);
    EXPECT_EQ(movedFromRecorder.stats.clipDepth, 1U);
    EXPECT_EQ(movedFromRecorder.stats.maxClipDepthObserved, 1U);
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(10.0F, 10.0F, 12.0F, 12.0F), MakeColor(0.0F, 1.0F, 0.0F, 1.0F))
            .HasValue());
    auto movedFromSeal = recorder.Seal();
    ASSERT_TRUE(movedFromSeal.HasValue());
    EXPECT_EQ(movedFromSeal->GetStats().commandCount, 1U);
    EXPECT_EQ(movedFromSeal->GetStats().fillRectangleCount, 1U);

    ASSERT_TRUE(movedRecorder.PushClipRectangle(MakeRect(0.0F, 0.0F, 5.0F, 5.0F)).HasValue());
    ASSERT_TRUE(movedRecorder.PopClip().HasValue());
    auto sealedResult = movedRecorder.Seal();
    ASSERT_TRUE(sealedResult.HasValue());

    SealedPaintList sealed = std::move(sealedResult).GetValue();
    ASSERT_EQ(sealed.GetCommands().size(), 3U);
    SealedPaintList movedSealed = std::move(sealed);
    EXPECT_TRUE(sealed.IsEmpty());
    EXPECT_EQ(sealed.GetStats().commandCount, 0U);
    EXPECT_EQ(sealed.GetStats().payloadBytes, 0U);
    EXPECT_EQ(sealed.GetStats().clipDepth, 1U);
    EXPECT_EQ(sealed.GetStats().maxClipDepthObserved, 1U);
    EXPECT_EQ(movedSealed.GetCommands()[0].kind, PaintCommandKind::FillRectangle);
    EXPECT_EQ(movedSealed.GetCommands()[1].kind, PaintCommandKind::PushClipRectangle);
    EXPECT_EQ(movedSealed.GetCommands()[2].kind, PaintCommandKind::PopClip);
}

TEST(UiPaintRecorderTests, ResetPreservesCapacityAndRestartsCommandIndices)
{
    PaintRecorder recorder{MakeRecorderTestLimits()};
    ASSERT_TRUE(
        recorder.FillRectangle(MakeRect(0.0F, 0.0F, 1.0F, 1.0F), MakeColor(1.0F, 0.0F, 0.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(0.0F, 0.0F, 1.0F, 1.0F)).HasValue());
    ASSERT_TRUE(recorder.PopClip().HasValue());
    const auto beforeReset = recorder.GetSnapshot();

    recorder.Reset();
    const auto afterReset = recorder.GetSnapshot();
    EXPECT_EQ(afterReset.stats.state, PaintRecorderState::Open);
    EXPECT_EQ(afterReset.stats.commandCount, 0U);
    EXPECT_EQ(afterReset.stats.payloadBytes, 0U);
    EXPECT_EQ(afterReset.stats.clipDepth, 1U);
    EXPECT_GE(afterReset.commandCapacity, beforeReset.commandCapacity);
    EXPECT_GE(afterReset.fillRectangleCapacity, beforeReset.fillRectangleCapacity);
    EXPECT_GE(afterReset.pushClipRectangleCapacity, beforeReset.pushClipRectangleCapacity);

    ASSERT_TRUE(
        recorder.FillRectangle(MakeRect(2.0F, 2.0F, 4.0F, 4.0F), MakeColor(0.0F, 0.0F, 1.0F, 1.0F))
            .HasValue());
    auto sealed = recorder.Seal();
    ASSERT_TRUE(sealed.HasValue());
    EXPECT_EQ(sealed->GetCommands()[0].index, 0U);
}

TEST(UiPaintRecorderTests, SealIntoReusesEveryWarmedOutputStream)
{
    PaintRecorder recorder{MakeRecorderTestLimits()};
    SealedPaintList reusableList;
    const auto recordPaint = [&]
    {
        ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(0.0F, 0.0F, 8.0F, 8.0F)).HasValue());
        ASSERT_TRUE(recorder
                        .FillRectangle(MakeRect(1.0F, 2.0F, 5.0F, 6.0F),
                                       MakeColor(0.25F, 0.5F, 0.75F, 1.0F))
                        .HasValue());
        ASSERT_TRUE(recorder.PopClip().HasValue());
    };

    ASSERT_NO_FATAL_FAILURE(recordPaint());
    ASSERT_TRUE(recorder.SealInto(reusableList).HasValue());
    const PaintCommandRecord* const commands = reusableList.GetCommands().data();
    const FillRectangleCommand* const fills = reusableList.GetFillRectangles().data();
    const PushClipRectangleCommand* const clips = reusableList.GetPushClipRectangles().data();
    ASSERT_NE(commands, nullptr);
    ASSERT_NE(fills, nullptr);
    ASSERT_NE(clips, nullptr);

    recorder.Reset();
    ASSERT_NO_FATAL_FAILURE(recordPaint());
    ASSERT_TRUE(recorder.SealInto(reusableList).HasValue());
    EXPECT_EQ(reusableList.GetCommands().data(), commands);
    EXPECT_EQ(reusableList.GetFillRectangles().data(), fills);
    EXPECT_EQ(reusableList.GetPushClipRectangles().data(), clips);
}

TEST(UiPaintRecorderTests, SealedListOwnsImmutableDataAfterRecorderReset)
{
    PaintRecorder recorder{MakeRecorderTestLimits()};
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(0.0F, 0.0F, 4.0F, 4.0F), MakeColor(0.25F, 0.5F, 0.75F, 1.0F))
            .HasValue());

    auto sealedResult = recorder.Seal();
    ASSERT_TRUE(sealedResult.HasValue());
    SealedPaintList sealed = std::move(sealedResult).GetValue();
    recorder.Reset();
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(10.0F, 10.0F, 12.0F, 12.0F), MakeColor(1.0F, 1.0F, 0.0F, 1.0F))
            .HasValue());

    ASSERT_EQ(sealed.GetCommands().size(), 1U);
    ASSERT_EQ(sealed.GetFillRectangles().size(), 1U);
    EXPECT_EQ(sealed.GetFillRectangles()[0].rectangle, MakeRect(0.0F, 0.0F, 4.0F, 4.0F));
    EXPECT_EQ(sealed.GetStats().state, PaintRecorderState::Sealed);
    EXPECT_FALSE(sealed.IsEmpty());
}

TEST(UiPaintRecorderTests, RejectsInvalidHardLimitsWithoutChangingState)
{
    pond::ui::UiHardLimits invalidLimits = MakeRecorderTestLimits();
    invalidLimits.maxCompilerScratchBytes = 0U;
    PaintRecorder recorder{invalidLimits};

    const auto fill =
        recorder.FillRectangle(MakeRect(0.0F, 0.0F, 1.0F, 1.0F), MakeColor(1.0F, 1.0F, 1.0F, 1.0F));
    ASSERT_FALSE(fill.HasValue());
    ExpectUiErrorCode(fill.GetError(), pond::ui::UiErrorCode::InvalidPaintValue);

    const auto seal = recorder.Seal();
    ASSERT_FALSE(seal.HasValue());
    ExpectUiErrorCode(seal.GetError(), pond::ui::UiErrorCode::InvalidPaintValue);

    const auto snapshot = recorder.GetSnapshot();
    EXPECT_EQ(snapshot.stats.state, PaintRecorderState::Open);
    EXPECT_EQ(snapshot.stats.commandCount, 0U);
    EXPECT_EQ(snapshot.stats.clipDepth, 1U);
}

TEST(UiPaintRecorderTests, AcceptsExactPayloadAndClipLimits)
{
    pond::ui::UiHardLimits limits = MakeRecorderTestLimits();
    limits.maxPaintCommandCount = 3U;
    limits.maxPaintCommandPayloadBytes =
        sizeof(FillRectangleCommand) + sizeof(PushClipRectangleCommand);
    limits.maxClipDepth = 2U;
    PaintRecorder recorder{limits};

    ASSERT_TRUE(
        recorder.FillRectangle(MakeRect(0.0F, 0.0F, 1.0F, 1.0F), MakeColor(1.0F, 1.0F, 1.0F, 1.0F))
            .HasValue());
    ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(0.0F, 0.0F, 1.0F, 1.0F)).HasValue());

    const auto excessClip = recorder.PushClipRectangle(MakeRect(0.0F, 0.0F, 1.0F, 1.0F));
    ASSERT_FALSE(excessClip.HasValue());
    ExpectUiErrorCode(excessClip.GetError(), pond::ui::UiErrorCode::LimitExceeded);

    ASSERT_TRUE(recorder.PopClip().HasValue());
    auto sealed = recorder.Seal();
    ASSERT_TRUE(sealed.HasValue());
    EXPECT_EQ(sealed->GetStats().commandCount, limits.maxPaintCommandCount);
    EXPECT_EQ(sealed->GetStats().payloadBytes, limits.maxPaintCommandPayloadBytes);
    EXPECT_EQ(sealed->GetStats().maxClipDepthObserved, limits.maxClipDepth);
}

TEST(UiPaintRecorderTests, RecordsEmptyClipAsValidatedSemanticState)
{
    PaintRecorder recorder{MakeRecorderTestLimits()};

    ASSERT_TRUE(recorder.PushClipRectangle(MakeRect(2.0F, 3.0F, 2.0F, 8.0F)).HasValue());
    ASSERT_TRUE(
        recorder
            .FillRectangle(MakeRect(0.0F, 0.0F, 10.0F, 10.0F), MakeColor(1.0F, 0.5F, 0.25F, 1.0F))
            .HasValue());
    ASSERT_TRUE(recorder.PopClip().HasValue());

    auto sealed = recorder.Seal();
    ASSERT_TRUE(sealed.HasValue());
    ASSERT_EQ(sealed->GetPushClipRectangles().size(), 1U);
    EXPECT_TRUE(sealed->GetPushClipRectangles()[0].isZeroArea);
    EXPECT_EQ(sealed->GetStats().commandCount, 3U);
}
