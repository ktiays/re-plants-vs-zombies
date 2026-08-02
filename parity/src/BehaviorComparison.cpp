#include "pvz/parity/BehaviorComparison.h"

#include <algorithm>
#include <cstddef>

namespace pvz::parity
{
namespace
{

[[nodiscard]] BehaviorDifference FindSunDifferenceAtTick(
    const game::BehaviorObservation& theLeft,
    const game::BehaviorObservation& theRight,
    std::uint64_t theTick)
{
    for (std::size_t aSlot = 0;
         aSlot < game::kBehaviorSunSlotCount;
         ++aSlot)
    {
        const auto& aLeftSun = theLeft.mSuns[aSlot];
        const auto& aRightSun = theRight.mSuns[aSlot];
        const auto aSunSlot = static_cast<std::uint8_t>(aSlot);
        if (aLeftSun.mActive != aRightSun.mActive)
            return {theTick, BehaviorField::SunActive, aSunSlot};
        if (aLeftSun.mBeingCollected != aRightSun.mBeingCollected)
        {
            return {
                theTick,
                BehaviorField::SunBeingCollected,
                aSunSlot,
            };
        }
        if (aLeftSun.mXMilliPixels != aRightSun.mXMilliPixels)
            return {theTick, BehaviorField::SunX, aSunSlot};
        if (aLeftSun.mYMilliPixels != aRightSun.mYMilliPixels)
            return {theTick, BehaviorField::SunY, aSunSlot};
        if (aLeftSun.mGroundYMilliPixels !=
            aRightSun.mGroundYMilliPixels)
        {
            return {theTick, BehaviorField::SunGroundY, aSunSlot};
        }
        if (aLeftSun.mAge != aRightSun.mAge)
            return {theTick, BehaviorField::SunAge, aSunSlot};
    }
    return {};
}

} // namespace

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
    case BehaviorField::Sun:
        return "sun";
    case BehaviorField::SeedRefreshCounter:
        return "seed-refresh-counter";
    case BehaviorField::SeedRefreshTime:
        return "seed-refresh-time";
    case BehaviorField::SeedRefreshing:
        return "seed-refreshing";
    case BehaviorField::SeedSelection:
        return "seed-selection";
    case BehaviorField::TutorialPhase:
        return "tutorial-phase";
    case BehaviorField::FirstSunCountdown:
        return "first-sun-countdown";
    case BehaviorField::FirstSunSpawned:
        return "first-sun-spawned";
    case BehaviorField::CurrentWave:
        return "current-wave";
    case BehaviorField::ZombieCountdown:
        return "zombie-countdown";
    case BehaviorField::ZombieCount:
        return "zombie-count";
    case BehaviorField::LevelOutcome:
        return "level-outcome";
    case BehaviorField::MowerState:
        return "mower-state";
    case BehaviorField::LevelAwardSpawned:
        return "level-award-spawned";
    case BehaviorField::ZombieWaveHealth:
        return "zombie-wave-health";
    case BehaviorField::ProjectileCount:
        return "projectile-count";
    case BehaviorField::SunActive:
        return "sun-active";
    case BehaviorField::SunBeingCollected:
        return "sun-being-collected";
    case BehaviorField::SunX:
        return "sun-x";
    case BehaviorField::SunY:
        return "sun-y";
    case BehaviorField::SunGroundY:
        return "sun-ground-y";
    case BehaviorField::SunAge:
        return "sun-age";
    case BehaviorField::ObservationCount:
        return "observation-count";
    }
    return "invalid";
}

BehaviorDifference FindFirstBehaviorDifference(
    std::span<const game::BehaviorObservation> theLeft,
    std::span<const game::BehaviorObservation> theRight,
    std::uint16_t theCommonFormatVersion)
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
        if (theCommonFormatVersion < 2)
            continue;
        if (aLeft.mSun != aRight.mSun)
            return {aTick, BehaviorField::Sun};
        if (aLeft.mSeedRefreshCounter !=
            aRight.mSeedRefreshCounter)
        {
            return {aTick, BehaviorField::SeedRefreshCounter};
        }
        if (aLeft.mSeedRefreshTime != aRight.mSeedRefreshTime)
            return {aTick, BehaviorField::SeedRefreshTime};
        if (aLeft.mSeedRefreshing != aRight.mSeedRefreshing)
            return {aTick, BehaviorField::SeedRefreshing};
        if (aLeft.mSeedSelection != aRight.mSeedSelection)
            return {aTick, BehaviorField::SeedSelection};
        if (aLeft.mTutorialPhase != aRight.mTutorialPhase)
            return {aTick, BehaviorField::TutorialPhase};
        if (aLeft.mFirstSunCountdown !=
            aRight.mFirstSunCountdown)
        {
            return {aTick, BehaviorField::FirstSunCountdown};
        }
        if (aLeft.mFirstSunSpawned !=
            aRight.mFirstSunSpawned)
        {
            return {aTick, BehaviorField::FirstSunSpawned};
        }
        if (theCommonFormatVersion < 4)
            continue;
        if (aLeft.mCurrentWave != aRight.mCurrentWave)
            return {aTick, BehaviorField::CurrentWave};
        if (aLeft.mZombieCountdown != aRight.mZombieCountdown)
            return {aTick, BehaviorField::ZombieCountdown};
        if (aLeft.mZombieCount != aRight.mZombieCount)
            return {aTick, BehaviorField::ZombieCount};
        if (aLeft.mLevelOutcome != aRight.mLevelOutcome)
            return {aTick, BehaviorField::LevelOutcome};
        if (aLeft.mMowerState != aRight.mMowerState)
            return {aTick, BehaviorField::MowerState};
        if (aLeft.mLevelAwardSpawned !=
            aRight.mLevelAwardSpawned)
        {
            return {aTick, BehaviorField::LevelAwardSpawned};
        }
        if (theCommonFormatVersion < 5)
            continue;
        if (aLeft.mZombieWaveHealth !=
            aRight.mZombieWaveHealth)
        {
            return {aTick, BehaviorField::ZombieWaveHealth};
        }
        if (aLeft.mProjectileCount != aRight.mProjectileCount)
            return {aTick, BehaviorField::ProjectileCount};
        if (theCommonFormatVersion < 7)
            continue;
        const auto aSunDifference =
            FindSunDifferenceAtTick(aLeft, aRight, aTick);
        if (aSunDifference.mField != BehaviorField::None)
            return aSunDifference;
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

BehaviorDifference FindFirstSunTrajectoryDifference(
    std::span<const game::BehaviorObservation> theLeft,
    std::span<const game::BehaviorObservation> theRight,
    std::uint16_t theCommonFormatVersion)
{
    if (theCommonFormatVersion < 7)
        return {};
    const auto aCount = std::min(theLeft.size(), theRight.size());
    for (std::size_t anIndex = 0; anIndex < aCount; ++anIndex)
    {
        const auto& aLeft = theLeft[anIndex];
        const auto& aRight = theRight[anIndex];
        const auto aTick = static_cast<std::uint64_t>(anIndex);
        const auto aSunDifference =
            FindSunDifferenceAtTick(aLeft, aRight, aTick);
        if (aSunDifference.mField != BehaviorField::None)
            return aSunDifference;
    }
    return {};
}

} // namespace pvz::parity
