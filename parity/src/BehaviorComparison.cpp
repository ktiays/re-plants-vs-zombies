#include "pvz/parity/BehaviorComparison.h"

#include <algorithm>
#include <cstddef>

namespace pvz::parity
{

std::string_view GetBehaviorFieldName(BehaviorField theField)
{
    switch (theField)
    {
    case BehaviorField::None:
        return "none";
    case BehaviorField::Tick:
        return "tick";
    case BehaviorField::Scene:
        return "scene";
    case BehaviorField::BoardStage:
        return "board-stage";
    case BehaviorField::GridColumn:
        return "grid-column";
    case BehaviorField::GridRow:
        return "grid-row";
    case BehaviorField::OccupiedCells:
        return "occupied-cells";
    case BehaviorField::PlantCount:
        return "plant-count";
    case BehaviorField::ObservationCount:
        return "observation-count";
    }
    return "invalid";
}

BehaviorDifference FindFirstBehaviorDifference(
    std::span<const game::BehaviorObservation> theLeft,
    std::span<const game::BehaviorObservation> theRight)
{
    const auto aCount = std::min(theLeft.size(), theRight.size());
    for (std::size_t anIndex = 0; anIndex < aCount; ++anIndex)
    {
        const auto& aLeft = theLeft[anIndex];
        const auto& aRight = theRight[anIndex];
        const auto aTick = static_cast<std::uint64_t>(anIndex);
        if (aLeft.mTick != aRight.mTick)
            return {aTick, BehaviorField::Tick};
        if (aLeft.mScene != aRight.mScene)
            return {aTick, BehaviorField::Scene};
        if (aLeft.mBoardStage != aRight.mBoardStage)
            return {aTick, BehaviorField::BoardStage};
        if (aLeft.mGridColumn != aRight.mGridColumn)
            return {aTick, BehaviorField::GridColumn};
        if (aLeft.mGridRow != aRight.mGridRow)
            return {aTick, BehaviorField::GridRow};
        if (aLeft.mOccupiedCells != aRight.mOccupiedCells)
            return {aTick, BehaviorField::OccupiedCells};
        if (aLeft.mPlantCount != aRight.mPlantCount)
            return {aTick, BehaviorField::PlantCount};
    }
    if (theLeft.size() != theRight.size())
    {
        return {
            static_cast<std::uint64_t>(aCount),
            BehaviorField::ObservationCount,
        };
    }
    return {};
}

} // namespace pvz::parity
