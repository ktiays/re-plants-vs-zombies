#include "pvz/game/LevelOneRandomDecision.h"

#include <cstddef>
#include <limits>

namespace pvz::game
{

LevelOneRandomDecisionTape::LevelOneRandomDecisionTape(
    std::span<const LevelOneRandomDecision> theDecisions)
    : mDecisions(theDecisions)
{
}

bool LevelOneRandomDecisionTape::ReadNext(
    LevelOneRandomDecisionKind theExpectedKind,
    LevelOneRandomDecision& theDecision)
{
    if (mError != LevelOneRandomDecisionReadError::None)
        return false;
    mExpectedKind = theExpectedKind;
    if (mReadCount >= mDecisions.size())
    {
        mError = LevelOneRandomDecisionReadError::Exhausted;
        return false;
    }

    const auto& aDecision =
        mDecisions[static_cast<std::size_t>(mReadCount)];
    mActualKind = aDecision.mKind;
    if (aDecision.mKind != theExpectedKind)
    {
        mError = LevelOneRandomDecisionReadError::KindMismatch;
        return false;
    }

    theDecision = aDecision;
    ++mReadCount;
    return true;
}

std::uint32_t LevelOneRandomDecisionTape::GetReadCount() const
{
    return mReadCount;
}

std::uint32_t LevelOneRandomDecisionTape::GetRemainingCount() const
{
    const auto aRemaining = mDecisions.size() - mReadCount;
    if (aRemaining > std::numeric_limits<std::uint32_t>::max())
        return std::numeric_limits<std::uint32_t>::max();
    return static_cast<std::uint32_t>(aRemaining);
}

LevelOneRandomDecisionReadError
LevelOneRandomDecisionTape::GetError() const
{
    return mError;
}

LevelOneRandomDecisionKind
LevelOneRandomDecisionTape::GetExpectedKind() const
{
    return mExpectedKind;
}

LevelOneRandomDecisionKind
LevelOneRandomDecisionTape::GetActualKind() const
{
    return mActualKind;
}

} // namespace pvz::game
