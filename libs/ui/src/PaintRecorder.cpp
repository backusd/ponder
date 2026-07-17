#include <ponder/ui/paint/PaintRecorder.hpp>

#include <algorithm>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace pond::ui::paint
{
namespace
{
[[nodiscard]] core::VoidResult MakePaintFailure(
    UiErrorCode code, std::string message,
    std::source_location location = std::source_location::current())
{
    return MakeUiFailure(code, std::move(message), location);
}

[[nodiscard]] std::uint64_t ToCount(std::size_t value) noexcept
{
    return static_cast<std::uint64_t>(value);
}

[[nodiscard]] std::uint64_t PayloadSizeForFillRectangle() noexcept
{
    return static_cast<std::uint64_t>(sizeof(FillRectangleCommand));
}

[[nodiscard]] std::uint64_t PayloadSizeForPushClipRectangle() noexcept
{
    return static_cast<std::uint64_t>(sizeof(PushClipRectangleCommand));
}

[[nodiscard]] bool CheckedAdd(std::uint64_t lhs, std::uint64_t rhs, std::uint64_t& result) noexcept
{
    if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs)
    {
        return false;
    }

    result = lhs + rhs;
    return true;
}

template <typename Value>
[[nodiscard]] core::VoidResult ReserveChecked(std::vector<Value>& storage, std::uint64_t required,
                                              std::uint64_t hardMaximum,
                                              std::string_view storageName)
{
    if (required <= static_cast<std::uint64_t>(storage.capacity()))
    {
        return core::VoidResult::Success();
    }
    if (required > static_cast<std::uint64_t>(storage.max_size()) ||
        required > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    {
        return MakePaintFailure(UiErrorCode::LimitExceeded,
                                "UI paint " + std::string{storageName} +
                                    " capacity is not representable by the host container.");
    }

    const std::uint64_t currentCapacity = static_cast<std::uint64_t>(storage.capacity());
    const std::uint64_t growthBase = std::max<std::uint64_t>(currentCapacity, 4U);
    std::uint64_t grownCapacity{};
    if (!CheckedAdd(growthBase, growthBase, grownCapacity))
    {
        grownCapacity = hardMaximum;
    }
    const std::uint64_t desiredCapacity = std::min(hardMaximum, std::max(required, grownCapacity));

    try
    {
        storage.reserve(static_cast<std::size_t>(desiredCapacity));
    }
    catch (const std::bad_alloc&)
    {
        return MakePaintFailure(UiErrorCode::CompilationFailure,
                                "UI paint recorder could not reserve " + std::string{storageName} +
                                    " storage.");
    }
    catch (const std::length_error&)
    {
        return MakePaintFailure(UiErrorCode::CompilationFailure,
                                "UI paint " + std::string{storageName} +
                                    " storage exceeds the host container maximum.");
    }

    return core::VoidResult::Success();
}
} // namespace

SealedPaintList::SealedPaintList(SealedPaintList&& other) noexcept
    : m_commands{std::move(other.m_commands)}, m_fillRectangles{std::move(other.m_fillRectangles)},
      m_pushClipRectangles{std::move(other.m_pushClipRectangles)},
      m_payloadBytes{other.m_payloadBytes}, m_maxClipDepthObserved{other.m_maxClipDepthObserved}
{
    other.m_commands.clear();
    other.m_fillRectangles.clear();
    other.m_pushClipRectangles.clear();
    other.m_payloadBytes = 0U;
    other.m_maxClipDepthObserved = 1U;
}

SealedPaintList& SealedPaintList::operator=(SealedPaintList&& other) noexcept
{
    if (this != &other)
    {
        m_commands = std::move(other.m_commands);
        m_fillRectangles = std::move(other.m_fillRectangles);
        m_pushClipRectangles = std::move(other.m_pushClipRectangles);
        m_payloadBytes = other.m_payloadBytes;
        m_maxClipDepthObserved = other.m_maxClipDepthObserved;
        other.m_commands.clear();
        other.m_fillRectangles.clear();
        other.m_pushClipRectangles.clear();
        other.m_payloadBytes = 0U;
        other.m_maxClipDepthObserved = 1U;
    }
    return *this;
}

std::span<const PaintCommandRecord> SealedPaintList::GetCommands() const noexcept
{
    return m_commands;
}

std::span<const FillRectangleCommand> SealedPaintList::GetFillRectangles() const noexcept
{
    return m_fillRectangles;
}

std::span<const PushClipRectangleCommand> SealedPaintList::GetPushClipRectangles() const noexcept
{
    return m_pushClipRectangles;
}

PaintListStats SealedPaintList::GetStats() const noexcept
{
    std::uint64_t popClipCount{};
    for (const PaintCommandRecord& command : m_commands)
    {
        if (command.kind == PaintCommandKind::PopClip)
        {
            ++popClipCount;
        }
    }

    return PaintListStats{.state = PaintRecorderState::Sealed,
                          .commandCount = ToCount(m_commands.size()),
                          .fillRectangleCount = ToCount(m_fillRectangles.size()),
                          .pushClipRectangleCount = ToCount(m_pushClipRectangles.size()),
                          .popClipCount = popClipCount,
                          .payloadBytes = m_payloadBytes,
                          .clipDepth = 1U,
                          .maxClipDepthObserved = m_maxClipDepthObserved};
}

bool SealedPaintList::IsEmpty() const noexcept
{
    return m_commands.empty();
}

PaintRecorder::PaintRecorder(UiHardLimits limits) : m_limits{limits} {}

PaintRecorder::PaintRecorder(PaintRecorder&& other) noexcept
    : m_limits{other.m_limits}, m_state{other.m_state}, m_commands{std::move(other.m_commands)},
      m_fillRectangles{std::move(other.m_fillRectangles)},
      m_pushClipRectangles{std::move(other.m_pushClipRectangles)},
      m_payloadBytes{other.m_payloadBytes}, m_clipDepth{other.m_clipDepth},
      m_maxClipDepthObserved{other.m_maxClipDepthObserved}
{
    other.Reset();
}

PaintRecorder& PaintRecorder::operator=(PaintRecorder&& other) noexcept
{
    if (this != &other)
    {
        m_limits = other.m_limits;
        m_state = other.m_state;
        m_commands = std::move(other.m_commands);
        m_fillRectangles = std::move(other.m_fillRectangles);
        m_pushClipRectangles = std::move(other.m_pushClipRectangles);
        m_payloadBytes = other.m_payloadBytes;
        m_clipDepth = other.m_clipDepth;
        m_maxClipDepthObserved = other.m_maxClipDepthObserved;
        other.Reset();
    }
    return *this;
}

core::VoidResult PaintRecorder::FillRectangle(LogicalRect rectangle, SrgbStraightAlphaColor color)
{
    if (core::VoidResult open = EnsureOpen(); !open.HasValue())
    {
        return open;
    }

    if (!IsValid(rectangle))
    {
        return MakePaintFailure(UiErrorCode::InvalidPaintValue,
                                "UI fill rectangle requires finite non-negative logical bounds.");
    }

    if (!IsValid(color))
    {
        return MakePaintFailure(UiErrorCode::InvalidPaintValue,
                                "UI fill rectangle requires a bounded sRGB straight-alpha color.");
    }

    std::uint64_t nextCommandCount{};
    std::uint64_t nextFillCount{};
    std::uint64_t nextPayloadBytes{};
    if (!CheckedAdd(ToCount(m_commands.size()), 1U, nextCommandCount) ||
        !CheckedAdd(ToCount(m_fillRectangles.size()), 1U, nextFillCount) ||
        !CheckedAdd(m_payloadBytes, PayloadSizeForFillRectangle(), nextPayloadBytes))
    {
        return MakePaintFailure(UiErrorCode::LimitExceeded,
                                "UI fill rectangle count or payload arithmetic overflowed.");
    }

    if (core::VoidResult limit = CheckCommandLimit(nextCommandCount); !limit.HasValue())
    {
        return limit;
    }
    if (core::VoidResult limit = CheckPayloadLimit(nextPayloadBytes); !limit.HasValue())
    {
        return limit;
    }
    if (core::VoidResult reserved =
            ReserveForAppend(nextCommandCount, nextFillCount, ToCount(m_pushClipRectangles.size()));
        !reserved.HasValue())
    {
        return reserved;
    }

    const PaintCommandIndex commandIndex = ToCount(m_commands.size());
    const std::uint64_t payloadIndex = ToCount(m_fillRectangles.size());
    const LinearPremultipliedColor semanticColor = ToLinearPremultiplied(color);
    m_fillRectangles.push_back(FillRectangleCommand{.rectangle = rectangle,
                                                    .color = semanticColor,
                                                    .isZeroArea = IsEmpty(rectangle),
                                                    .isTransparent = IsTransparent(semanticColor)});
    m_commands.push_back(PaintCommandRecord{.index = commandIndex,
                                            .kind = PaintCommandKind::FillRectangle,
                                            .payloadIndex = payloadIndex});
    m_payloadBytes = nextPayloadBytes;
    return core::VoidResult::Success();
}

core::VoidResult PaintRecorder::PushClipRectangle(LogicalRect rectangle)
{
    if (core::VoidResult open = EnsureOpen(); !open.HasValue())
    {
        return open;
    }

    if (!IsValid(rectangle))
    {
        return MakePaintFailure(UiErrorCode::InvalidPaintValue,
                                "UI clip rectangle requires finite non-negative logical bounds.");
    }

    std::uint64_t nextCommandCount{};
    std::uint64_t nextClipCount{};
    std::uint64_t nextPayloadBytes{};
    std::uint64_t nextClipDepth{};
    if (!CheckedAdd(ToCount(m_commands.size()), 1U, nextCommandCount) ||
        !CheckedAdd(ToCount(m_pushClipRectangles.size()), 1U, nextClipCount) ||
        !CheckedAdd(m_payloadBytes, PayloadSizeForPushClipRectangle(), nextPayloadBytes) ||
        !CheckedAdd(m_clipDepth, 1U, nextClipDepth))
    {
        return MakePaintFailure(
            UiErrorCode::LimitExceeded,
            "UI clip rectangle count, payload, or depth arithmetic overflowed.");
    }

    if (core::VoidResult limit = CheckCommandLimit(nextCommandCount); !limit.HasValue())
    {
        return limit;
    }
    if (core::VoidResult limit = CheckPayloadLimit(nextPayloadBytes); !limit.HasValue())
    {
        return limit;
    }
    if (core::VoidResult limit = CheckClipDepthLimit(nextClipDepth); !limit.HasValue())
    {
        return limit;
    }
    if (core::VoidResult reserved =
            ReserveForAppend(nextCommandCount, ToCount(m_fillRectangles.size()), nextClipCount);
        !reserved.HasValue())
    {
        return reserved;
    }

    const PaintCommandIndex commandIndex = ToCount(m_commands.size());
    const std::uint64_t payloadIndex = ToCount(m_pushClipRectangles.size());
    m_pushClipRectangles.push_back(
        PushClipRectangleCommand{.rectangle = rectangle, .isZeroArea = IsEmpty(rectangle)});
    m_commands.push_back(PaintCommandRecord{.index = commandIndex,
                                            .kind = PaintCommandKind::PushClipRectangle,
                                            .payloadIndex = payloadIndex});
    m_payloadBytes = nextPayloadBytes;
    m_clipDepth = nextClipDepth;
    m_maxClipDepthObserved = std::max(m_maxClipDepthObserved, m_clipDepth);
    return core::VoidResult::Success();
}

core::VoidResult PaintRecorder::PopClip()
{
    if (core::VoidResult open = EnsureOpen(); !open.HasValue())
    {
        return open;
    }

    if (m_clipDepth == 1U)
    {
        return MakePaintFailure(UiErrorCode::UnbalancedPaintState,
                                "UI paint recorder cannot pop the unbounded clip sentinel.");
    }

    std::uint64_t nextCommandCount{};
    if (!CheckedAdd(ToCount(m_commands.size()), 1U, nextCommandCount))
    {
        return MakePaintFailure(UiErrorCode::LimitExceeded,
                                "UI pop-clip command count arithmetic overflowed.");
    }
    if (core::VoidResult limit = CheckCommandLimit(nextCommandCount); !limit.HasValue())
    {
        return limit;
    }
    if (core::VoidResult reserved =
            ReserveForAppend(nextCommandCount, ToCount(m_fillRectangles.size()),
                             ToCount(m_pushClipRectangles.size()));
        !reserved.HasValue())
    {
        return reserved;
    }

    const PaintCommandIndex commandIndex = ToCount(m_commands.size());
    m_commands.push_back(PaintCommandRecord{.index = commandIndex,
                                            .kind = PaintCommandKind::PopClip,
                                            .payloadIndex = kNoPaintPayloadIndex});
    --m_clipDepth;
    return core::VoidResult::Success();
}

core::Result<SealedPaintList> PaintRecorder::Seal()
{
    SealedPaintList list;
    if (core::VoidResult sealed = SealInto(list); !sealed)
    {
        return core::Result<SealedPaintList>::FromError(std::move(sealed).GetError());
    }
    return core::Result<SealedPaintList>::FromValue(std::move(list));
}

core::VoidResult PaintRecorder::SealInto(SealedPaintList& reusableList)
{
    if (core::VoidResult open = EnsureOpen(); !open.HasValue())
    {
        return open;
    }

    if (m_clipDepth != 1U)
    {
        return MakePaintFailure(
            UiErrorCode::UnbalancedPaintState,
            "UI paint recorder cannot seal while clip rectangles remain unbalanced.");
    }

    try
    {
        // Reserve every output stream before mutating any of them. A failed growth attempt leaves
        // the previously sealed reusable list intact; once all reserves succeed, these value types
        // copy without throwing and the publish step cannot become partial.
        reusableList.m_commands.reserve(m_commands.size());
        reusableList.m_fillRectangles.reserve(m_fillRectangles.size());
        reusableList.m_pushClipRectangles.reserve(m_pushClipRectangles.size());
    }
    catch (const std::bad_alloc&)
    {
        return MakePaintFailure(UiErrorCode::CompilationFailure,
                                "UI paint recorder could not allocate sealed paint list storage.");
    }
    catch (const std::length_error&)
    {
        return MakePaintFailure(UiErrorCode::CompilationFailure,
                                "UI sealed paint list exceeds the host container maximum.");
    }

    reusableList.m_commands.assign(m_commands.begin(), m_commands.end());
    reusableList.m_fillRectangles.assign(m_fillRectangles.begin(), m_fillRectangles.end());
    reusableList.m_pushClipRectangles.assign(m_pushClipRectangles.begin(),
                                             m_pushClipRectangles.end());
    reusableList.m_payloadBytes = m_payloadBytes;
    reusableList.m_maxClipDepthObserved = m_maxClipDepthObserved;
    m_state = PaintRecorderState::Sealed;
    return core::VoidResult::Success();
}

void PaintRecorder::Reset() noexcept
{
    m_commands.clear();
    m_fillRectangles.clear();
    m_pushClipRectangles.clear();
    m_payloadBytes = 0U;
    m_clipDepth = 1U;
    m_maxClipDepthObserved = 1U;
    m_state = PaintRecorderState::Open;
}

PaintRecorderState PaintRecorder::GetState() const noexcept
{
    return m_state;
}

bool PaintRecorder::IsOpen() const noexcept
{
    return m_state == PaintRecorderState::Open;
}

bool PaintRecorder::IsSealed() const noexcept
{
    return m_state == PaintRecorderState::Sealed;
}

PaintRecorderSnapshot PaintRecorder::GetSnapshot() const noexcept
{
    return PaintRecorderSnapshot{.stats = MakeStats(),
                                 .commandCapacity = ToCount(m_commands.capacity()),
                                 .fillRectangleCapacity = ToCount(m_fillRectangles.capacity()),
                                 .pushClipRectangleCapacity =
                                     ToCount(m_pushClipRectangles.capacity())};
}

UiHardLimits PaintRecorder::GetLimits() const noexcept
{
    return m_limits;
}

core::VoidResult PaintRecorder::EnsureOpen() const
{
    if (!IsValid(m_limits))
    {
        return MakePaintFailure(
            UiErrorCode::InvalidPaintValue,
            "UI paint recorder limits must all be non-zero and include the clip sentinel.");
    }

    if (IsOpen())
    {
        return core::VoidResult::Success();
    }

    return MakePaintFailure(UiErrorCode::InvalidPaintState,
                            "UI paint recorder cannot be mutated after sealing; call Reset first.");
}

core::VoidResult PaintRecorder::CheckCommandLimit(std::uint64_t requested,
                                                  std::source_location location) const
{
    return CheckUiHardLimit(UiHardLimitKind::PaintCommandCount, requested, m_limits, location);
}

core::VoidResult PaintRecorder::CheckPayloadLimit(std::uint64_t requested,
                                                  std::source_location location) const
{
    return CheckUiHardLimit(UiHardLimitKind::PaintCommandPayloadBytes, requested, m_limits,
                            location);
}

core::VoidResult PaintRecorder::CheckClipDepthLimit(std::uint64_t requested,
                                                    std::source_location location) const
{
    return CheckUiHardLimit(UiHardLimitKind::ClipDepth, requested, m_limits, location);
}

core::VoidResult PaintRecorder::ReserveForAppend(std::uint64_t nextCommandCount,
                                                 std::uint64_t nextFillCount,
                                                 std::uint64_t nextClipCount)
{
    if (core::VoidResult result =
            ReserveChecked(m_commands, nextCommandCount, m_limits.maxPaintCommandCount, "command");
        !result.HasValue())
    {
        return result;
    }
    if (core::VoidResult result =
            ReserveChecked(m_fillRectangles, nextFillCount, m_limits.maxPaintCommandCount,
                           "fill-rectangle payload");
        !result.HasValue())
    {
        return result;
    }
    return ReserveChecked(m_pushClipRectangles, nextClipCount, m_limits.maxPaintCommandCount,
                          "clip-rectangle payload");
}

PaintListStats PaintRecorder::MakeStats() const noexcept
{
    std::uint64_t popClipCount{};
    for (const PaintCommandRecord& command : m_commands)
    {
        if (command.kind == PaintCommandKind::PopClip)
        {
            ++popClipCount;
        }
    }

    return PaintListStats{.state = m_state,
                          .commandCount = ToCount(m_commands.size()),
                          .fillRectangleCount = ToCount(m_fillRectangles.size()),
                          .pushClipRectangleCount = ToCount(m_pushClipRectangles.size()),
                          .popClipCount = popClipCount,
                          .payloadBytes = m_payloadBytes,
                          .clipDepth = m_clipDepth,
                          .maxClipDepthObserved = m_maxClipDepthObserved};
}
} // namespace pond::ui::paint
