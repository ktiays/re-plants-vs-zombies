#pragma once

#include <cstdint>
#include <span>

namespace pvz::game
{

enum class LevelOneRandomDecisionKind : std::uint8_t
{
    FallingSun,
    NormalZombie,
    Count,
};

struct LevelOneRandomDecision
{
    LevelOneRandomDecisionKind mKind{
        LevelOneRandomDecisionKind::FallingSun};
    std::uint16_t mNextCountdown{};
    std::int32_t mXMilliPixels{};
    std::int32_t mGroundYMilliPixels{};
    std::uint32_t mSpeedMicroPixelsPerTick{};
};

enum class LevelOneRandomDecisionReadError : std::uint8_t
{
    None,
    Exhausted,
    KindMismatch,
    Count,
};

class ILevelOneRandomDecisionSource
{
public:
    virtual ~ILevelOneRandomDecisionSource() = default;

    [[nodiscard]] virtual bool ReadNext(
        LevelOneRandomDecisionKind theExpectedKind,
        LevelOneRandomDecision& theDecision) = 0;
};

class LevelOneRandomDecisionTape final
    : public ILevelOneRandomDecisionSource
{
public:
    explicit LevelOneRandomDecisionTape(
        std::span<const LevelOneRandomDecision> theDecisions);

    [[nodiscard]] bool ReadNext(
        LevelOneRandomDecisionKind theExpectedKind,
        LevelOneRandomDecision& theDecision) override;

    [[nodiscard]] std::uint32_t GetReadCount() const;
    [[nodiscard]] std::uint32_t GetRemainingCount() const;
    [[nodiscard]] LevelOneRandomDecisionReadError GetError() const;
    [[nodiscard]] LevelOneRandomDecisionKind GetExpectedKind() const;
    [[nodiscard]] LevelOneRandomDecisionKind GetActualKind() const;

private:
    std::span<const LevelOneRandomDecision> mDecisions;
    std::uint32_t mReadCount{};
    LevelOneRandomDecisionReadError mError{
        LevelOneRandomDecisionReadError::None};
    LevelOneRandomDecisionKind mExpectedKind{
        LevelOneRandomDecisionKind::FallingSun};
    LevelOneRandomDecisionKind mActualKind{
        LevelOneRandomDecisionKind::FallingSun};
};

static_assert(sizeof(LevelOneRandomDecisionKind) == 1);
static_assert(sizeof(LevelOneRandomDecisionReadError) == 1);

} // namespace pvz::game
