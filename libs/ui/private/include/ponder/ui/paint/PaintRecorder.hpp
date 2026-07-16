#pragma once

#include <ponder/core/Result.hpp>
#include <ponder/ui/Color.hpp>
#include <ponder/ui/Error.hpp>
#include <ponder/ui/Geometry.hpp>
#include <ponder/ui/Limits.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <source_location>
#include <span>
#include <vector>

namespace pond::ui::paint
{
using PaintCommandIndex = std::uint64_t;
inline constexpr std::uint64_t kNoPaintPayloadIndex{std::numeric_limits<std::uint64_t>::max()};

class SealedPaintList;

enum class PaintCommandKind : std::uint8_t
{
    FillRectangle,
    PushClipRectangle,
    PopClip
};

enum class PaintRecorderState : std::uint8_t
{
    Open,
    Sealed
};

struct FillRectangleCommand final
{
    LogicalRect rectangle{};
    LinearPremultipliedColor color{};
    bool isZeroArea{};
    bool isTransparent{};

    [[nodiscard]] friend constexpr bool operator==(
        const FillRectangleCommand& lhs, const FillRectangleCommand& rhs) noexcept = default;
};

struct PushClipRectangleCommand final
{
    LogicalRect rectangle{};
    bool isZeroArea{};

    [[nodiscard]] friend constexpr bool operator==(const PushClipRectangleCommand& lhs,
                                                   const PushClipRectangleCommand& rhs) noexcept =
        default;
};

struct PaintCommandRecord final
{
    PaintCommandIndex index{};
    PaintCommandKind kind{};
    std::uint64_t payloadIndex{kNoPaintPayloadIndex};

    [[nodiscard]] friend constexpr bool operator==(
        const PaintCommandRecord& lhs, const PaintCommandRecord& rhs) noexcept = default;
};

struct PaintListStats final
{
    PaintRecorderState state{};
    std::uint64_t commandCount{};
    std::uint64_t fillRectangleCount{};
    std::uint64_t pushClipRectangleCount{};
    std::uint64_t popClipCount{};
    std::uint64_t payloadBytes{};
    std::uint64_t clipDepth{};
    std::uint64_t maxClipDepthObserved{};

    [[nodiscard]] friend constexpr bool operator==(const PaintListStats& lhs,
                                                   const PaintListStats& rhs) noexcept = default;
};

struct PaintRecorderSnapshot final
{
    PaintListStats stats{};
    std::uint64_t commandCapacity{};
    std::uint64_t fillRectangleCapacity{};
    std::uint64_t pushClipRectangleCapacity{};

    [[nodiscard]] friend constexpr bool operator==(
        const PaintRecorderSnapshot& lhs, const PaintRecorderSnapshot& rhs) noexcept = default;
};

class SealedPaintList final
{
public:
    SealedPaintList() = default;
    SealedPaintList(const SealedPaintList&) = delete;
    SealedPaintList(SealedPaintList&& other) noexcept;
    SealedPaintList& operator=(const SealedPaintList&) = delete;
    SealedPaintList& operator=(SealedPaintList&& other) noexcept;
    ~SealedPaintList() = default;

    [[nodiscard]] std::span<const PaintCommandRecord> GetCommands() const noexcept;
    [[nodiscard]] std::span<const FillRectangleCommand> GetFillRectangles() const noexcept;
    [[nodiscard]] std::span<const PushClipRectangleCommand> GetPushClipRectangles() const noexcept;
    [[nodiscard]] PaintListStats GetStats() const noexcept;
    [[nodiscard]] bool IsEmpty() const noexcept;

private:
    friend class PaintRecorder;

    std::vector<PaintCommandRecord> m_commands;
    std::vector<FillRectangleCommand> m_fillRectangles;
    std::vector<PushClipRectangleCommand> m_pushClipRectangles;
    std::uint64_t m_payloadBytes{};
    std::uint64_t m_maxClipDepthObserved{1U};
};

class PaintRecorder final
{
public:
    explicit PaintRecorder(UiHardLimits limits = kDefaultUiHardLimits);
    PaintRecorder(const PaintRecorder&) = delete;
    PaintRecorder(PaintRecorder&& other) noexcept;
    PaintRecorder& operator=(const PaintRecorder&) = delete;
    PaintRecorder& operator=(PaintRecorder&& other) noexcept;
    ~PaintRecorder() = default;

    [[nodiscard]] core::VoidResult FillRectangle(LogicalRect rectangle,
                                                 SrgbStraightAlphaColor color);
    [[nodiscard]] core::VoidResult PushClipRectangle(LogicalRect rectangle);
    [[nodiscard]] core::VoidResult PopClip();
    [[nodiscard]] core::Result<SealedPaintList> Seal();
    [[nodiscard]] core::VoidResult SealInto(SealedPaintList& reusableList);

    void Reset() noexcept;

    [[nodiscard]] PaintRecorderState GetState() const noexcept;
    [[nodiscard]] bool IsOpen() const noexcept;
    [[nodiscard]] bool IsSealed() const noexcept;
    [[nodiscard]] PaintRecorderSnapshot GetSnapshot() const noexcept;
    [[nodiscard]] UiHardLimits GetLimits() const noexcept;

private:
    [[nodiscard]] core::VoidResult EnsureOpen() const;
    [[nodiscard]] core::VoidResult CheckCommandLimit(
        std::uint64_t requested,
        std::source_location location = std::source_location::current()) const;
    [[nodiscard]] core::VoidResult CheckPayloadLimit(
        std::uint64_t requested,
        std::source_location location = std::source_location::current()) const;
    [[nodiscard]] core::VoidResult CheckClipDepthLimit(
        std::uint64_t requested,
        std::source_location location = std::source_location::current()) const;
    [[nodiscard]] core::VoidResult ReserveForAppend(std::uint64_t nextCommandCount,
                                                    std::uint64_t nextFillCount,
                                                    std::uint64_t nextClipCount);
    [[nodiscard]] PaintListStats MakeStats() const noexcept;

    UiHardLimits m_limits{};
    PaintRecorderState m_state{PaintRecorderState::Open};
    std::vector<PaintCommandRecord> m_commands;
    std::vector<FillRectangleCommand> m_fillRectangles;
    std::vector<PushClipRectangleCommand> m_pushClipRectangles;
    std::uint64_t m_payloadBytes{};
    std::uint64_t m_clipDepth{1U};
    std::uint64_t m_maxClipDepthObserved{1U};
};
} // namespace pond::ui::paint
