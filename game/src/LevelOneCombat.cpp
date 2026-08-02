#include "pvz/game/LevelOneCombat.h"

#include "pvz/game/BoardGeometry.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace pvz::game
{
namespace
{

inline constexpr std::uint8_t kColumnCount = 9;
inline constexpr std::int32_t kProjectileMaximumXMilliPixels = 900'000;
inline constexpr std::int32_t kZombieLossXMilliPixels = -100'000;
inline constexpr std::int32_t kMinimumStateXMilliPixels = -200'000;
inline constexpr std::int32_t kMaximumStateXMilliPixels = 1'000'000;
inline constexpr std::int32_t kMinimumStateYMilliPixels = -100'000;
inline constexpr std::int32_t kMaximumStateYMilliPixels = 700'000;
inline constexpr std::uint16_t kSunGroundDisappearTicks = 750;
inline constexpr std::uint8_t kSunFadeTicks = 15;
inline constexpr std::uint16_t kMaximumSunCountdown = 1'224;
inline constexpr std::int32_t kMinimumSunSpawnXMilliPixels = 100'000;
inline constexpr std::int32_t kMaximumSunSpawnXMilliPixels = 649'000;
inline constexpr std::int32_t kMinimumSunGroundYMilliPixels = 300'000;
inline constexpr std::int32_t kMaximumSunGroundYMilliPixels = 549'000;
inline constexpr std::int32_t kMinimumZombieSpawnXMilliPixels = 780'000;
inline constexpr std::int32_t kMaximumZombieSpawnXMilliPixels = 819'000;
inline constexpr std::uint32_t kMinimumZombieSpeedMicroPixelsPerTick =
    230'000;
inline constexpr std::uint32_t kMaximumZombieSpeedMicroPixelsPerTick =
    320'000;
inline constexpr std::int32_t kSunCollectionDestinationXMilliPixels =
    15'000;
inline constexpr std::int32_t kSunCollectionDestinationYMilliPixels = 0;
inline constexpr std::int32_t kSunScoringDistanceMilliPixels = 8'000;
inline constexpr std::uint16_t kMaximumSunCollectionTicks = 256;

[[nodiscard]] std::int32_t ToPixels(
    std::int32_t theMilliPixels)
{
    return theMilliPixels / 1'000;
}

[[nodiscard]] std::uint64_t CellMask(
    std::uint8_t theColumn,
    std::uint8_t theRow)
{
    const auto anIndex =
        static_cast<std::uint32_t>(theRow) *
            kColumnCount +
        theColumn;
    return std::uint64_t{1} << anIndex;
}

[[nodiscard]] std::int32_t GetOverlap(
    std::int32_t theLeftX,
    std::int32_t theLeftWidth,
    std::int32_t theRightX,
    std::int32_t theRightWidth)
{
    return
        std::min(
            theLeftX + theLeftWidth,
            theRightX + theRightWidth) -
        std::max(theLeftX, theRightX);
}

template <typename Entry, std::size_t Size>
[[nodiscard]] std::uint8_t CountActive(
    const std::array<Entry, Size>& theEntries)
{
    const auto aCount = std::count_if(
        theEntries.begin(),
        theEntries.end(),
        [](const Entry& theEntry)
        {
            return theEntry.mActive;
        });
    return static_cast<std::uint8_t>(aCount);
}

} // namespace

void LevelOneCombat::SetRandomDecisionSource(
    ILevelOneRandomDecisionSource* theSource)
{
    mRandomDecisionSource = theSource;
}

void LevelOneCombat::Reset()
{
    mState = {};
    mCollectedSun = 0;
    mDestroyedCells = 0;
    mRandomDecisionFailure = false;
}

void LevelOneCombat::Update()
{
    if (mState.mPhase == LevelOneCombatPhase::Lost)
        return;

    ++mState.mTick;
    UpdatePlants();
    UpdateZombies();
    UpdateProjectiles();

    if (mState.mFirstWaveSpawned &&
        mState.mZombieCount == 0 &&
        !mState.mFirstWaveCleared)
    {
        mState.mFirstWaveCleared = true;
        mState.mPhase =
            LevelOneCombatPhase::FirstWaveCleared;
    }

    UpdateSun();
    UpdateWave();
    RecountEntities();
}

bool LevelOneCombat::AddPeashooter(
    std::uint8_t theColumn,
    std::uint8_t theRow)
{
    if (theColumn >= kColumnCount ||
        theRow != kLaneRow ||
        mState.mPhase == LevelOneCombatPhase::Lost)
    {
        return false;
    }
    for (const auto& aPlant : mState.mPlants)
    {
        if (aPlant.mActive &&
            aPlant.mColumn == theColumn &&
            aPlant.mRow == theRow)
        {
            return false;
        }
    }

    const auto aSlot = std::find_if(
        mState.mPlants.begin(),
        mState.mPlants.end(),
        [](const LevelOnePlantCombatState& thePlant)
        {
            return !thePlant.mActive;
        });
    if (aSlot == mState.mPlants.end())
        return false;

    *aSlot = {
        .mActive = true,
        .mColumn = theColumn,
        .mRow = theRow,
        .mHealth = kPlantHealth,
        .mLaunchCounter = 0,
        .mShootingCounter = 0,
    };
    RecountEntities();

    if (mState.mPlantCount == 1 &&
        mState.mPhase ==
            LevelOneCombatPhase::AwaitingFirstPlant)
    {
        mState.mPhase =
            LevelOneCombatPhase::AwaitingSecondPlant;
        mState.mSunCountdown = kTutorialSunCountdown;
    }
    else if (mState.mPlantCount >= 2 &&
             mState.mPhase ==
                 LevelOneCombatPhase::AwaitingSecondPlant)
    {
        mState.mPhase = LevelOneCombatPhase::Active;
        mState.mZombieCountdown = kFirstWaveCountdown;
    }
    return true;
}

bool LevelOneCombat::TryCollectSun(
    engine::PointI thePosition)
{
    constexpr std::int32_t kClickMargin = 15;
    constexpr std::int32_t kSunSize = 60;
    for (auto& aSun : mState.mSuns)
    {
        if (!aSun.mActive || aSun.mBeingCollected)
            continue;
        const auto anX = ToPixels(aSun.mXMilliPixels);
        const auto aY = ToPixels(aSun.mYMilliPixels);
        if (thePosition.mX < anX - kClickMargin ||
            thePosition.mX >=
                anX + kSunSize + kClickMargin ||
            thePosition.mY < aY - kClickMargin ||
            thePosition.mY >=
                aY + kSunSize + kClickMargin)
        {
            continue;
        }

        aSun.mBeingCollected = true;
        return true;
    }
    return false;
}

std::uint16_t LevelOneCombat::ConsumeCollectedSun()
{
    const auto anAmount = mCollectedSun;
    mCollectedSun = 0;
    return anAmount;
}

std::uint64_t LevelOneCombat::ConsumeDestroyedCells()
{
    const auto aCells = mDestroyedCells;
    mDestroyedCells = 0;
    return aCells;
}

LevelOneCombatState LevelOneCombat::GetState() const
{
    return mState;
}

bool LevelOneCombat::RestoreState(
    const LevelOneCombatState& theState)
{
    if (theState.mPhase >= LevelOneCombatPhase::Count ||
        theState.mSunCountdown > kMaximumSunCountdown ||
        theState.mZombieCountdown > kFirstWaveCountdown)
    {
        return false;
    }
    for (const auto& aSun : theState.mSuns)
    {
        if (!aSun.mActive)
        {
            if (aSun.mBeingCollected)
                return false;
            continue;
        }
        if (aSun.mXMilliPixels <
                kMinimumStateXMilliPixels ||
            aSun.mXMilliPixels >
                kMaximumStateXMilliPixels ||
            aSun.mYMilliPixels <
                kMinimumStateYMilliPixels ||
            aSun.mYMilliPixels >
                kMaximumStateYMilliPixels ||
            aSun.mGroundYMilliPixels <
                kMinimumStateYMilliPixels ||
            aSun.mGroundYMilliPixels >
                kMaximumStateYMilliPixels ||
            aSun.mAge >
                CalculateSunLifetime(
                    aSun.mGroundYMilliPixels) +
                    (aSun.mBeingCollected
                         ? kMaximumSunCollectionTicks
                         : 0U))
        {
            return false;
        }
    }

    std::uint64_t anOccupiedCells{};
    for (const auto& aPlant : theState.mPlants)
    {
        if (!aPlant.mActive)
            continue;
        if (aPlant.mColumn >= kColumnCount ||
            aPlant.mRow != kLaneRow ||
            aPlant.mHealth == 0 ||
            aPlant.mHealth > kPlantHealth ||
            aPlant.mLaunchCounter > kPeashooterLaunchRate ||
            aPlant.mShootingCounter > kPeashooterFireDelay)
        {
            return false;
        }
        const auto aMask =
            CellMask(aPlant.mColumn, aPlant.mRow);
        if ((anOccupiedCells & aMask) != 0)
            return false;
        anOccupiedCells |= aMask;
    }
    for (const auto& aZombie : theState.mZombies)
    {
        if (!aZombie.mActive)
            continue;
        if (aZombie.mRow != kLaneRow ||
            aZombie.mHealth == 0 ||
            aZombie.mHealth > kNormalZombieHealth ||
            aZombie.mXMilliPixels <
                kMinimumStateXMilliPixels ||
            aZombie.mXMilliPixels >
                kMaximumStateXMilliPixels ||
            aZombie.mSpeedMicroPixelsPerTick <
                kMinimumZombieSpeedMicroPixelsPerTick ||
            aZombie.mSpeedMicroPixelsPerTick >
                kMaximumZombieSpeedMicroPixelsPerTick ||
            aZombie.mMovementRemainderMicroPixels >= 1'000)
        {
            return false;
        }
    }
    for (const auto& aProjectile : theState.mProjectiles)
    {
        if (!aProjectile.mActive)
            continue;
        if (aProjectile.mRow != kLaneRow ||
            aProjectile.mXMilliPixels <
                kMinimumStateXMilliPixels ||
            aProjectile.mXMilliPixels >
                kMaximumStateXMilliPixels ||
            aProjectile.mYMilliPixels <
                kMinimumStateYMilliPixels ||
            aProjectile.mYMilliPixels >
                kMaximumStateYMilliPixels)
        {
            return false;
        }
    }

    if (theState.mPlantCount !=
            CountActive(theState.mPlants) ||
        theState.mZombieCount !=
            CountActive(theState.mZombies) ||
        theState.mProjectileCount !=
            CountActive(theState.mProjectiles) ||
        theState.mSunCount !=
            CountActive(theState.mSuns) ||
        theState.mSunCount > theState.mSunsSpawned ||
        (theState.mPhase ==
             LevelOneCombatPhase::AwaitingFirstPlant &&
         theState.mPlantCount != 0) ||
        (theState.mPhase ==
             LevelOneCombatPhase::AwaitingSecondPlant &&
         theState.mPlantCount != 1) ||
        (theState.mFirstWaveCleared &&
         (!theState.mFirstWaveSpawned ||
          theState.mZombieCount != 0)))
    {
        return false;
    }

    mState = theState;
    mCollectedSun = 0;
    mDestroyedCells = 0;
    return true;
}

bool LevelOneCombat::SaveState(
    engine::IStateWriter& theWriter) const
{
    if (!theWriter.WriteU8(
            static_cast<std::uint8_t>(mState.mPhase)) ||
        !theWriter.WriteU32(mState.mTick) ||
        !theWriter.WriteU16(mState.mSunCountdown) ||
        !theWriter.WriteU8(mState.mSunsSpawned) ||
        !theWriter.WriteU8(mState.mSunCount))
    {
        return false;
    }
    for (const auto& aSun : mState.mSuns)
    {
        if (!theWriter.WriteBool(aSun.mActive) ||
            !theWriter.WriteBool(aSun.mBeingCollected) ||
            !theWriter.WriteI32(aSun.mXMilliPixels) ||
            !theWriter.WriteI32(aSun.mYMilliPixels) ||
            !theWriter.WriteI32(aSun.mGroundYMilliPixels) ||
            !theWriter.WriteU16(aSun.mAge))
        {
            return false;
        }
    }
    if (!theWriter.WriteU16(mState.mZombieCountdown) ||
        !theWriter.WriteBool(mState.mFirstWaveSpawned) ||
        !theWriter.WriteBool(mState.mFirstWaveCleared) ||
        !theWriter.WriteU8(mState.mPlantCount) ||
        !theWriter.WriteU8(mState.mZombieCount) ||
        !theWriter.WriteU8(mState.mProjectileCount))
    {
        return false;
    }

    for (const auto& aPlant : mState.mPlants)
    {
        if (!theWriter.WriteBool(aPlant.mActive) ||
            !theWriter.WriteU8(aPlant.mColumn) ||
            !theWriter.WriteU8(aPlant.mRow) ||
            !theWriter.WriteU16(aPlant.mHealth) ||
            !theWriter.WriteU16(aPlant.mLaunchCounter) ||
            !theWriter.WriteU8(aPlant.mShootingCounter))
        {
            return false;
        }
    }
    for (const auto& aZombie : mState.mZombies)
    {
        if (!theWriter.WriteBool(aZombie.mActive) ||
            !theWriter.WriteU8(aZombie.mRow) ||
            !theWriter.WriteU16(aZombie.mHealth) ||
            !theWriter.WriteI32(aZombie.mXMilliPixels) ||
            !theWriter.WriteU32(
                aZombie.mSpeedMicroPixelsPerTick) ||
            !theWriter.WriteU16(
                aZombie.mMovementRemainderMicroPixels) ||
            !theWriter.WriteU32(aZombie.mAge) ||
            !theWriter.WriteBool(aZombie.mEating))
        {
            return false;
        }
    }
    for (const auto& aProjectile : mState.mProjectiles)
    {
        if (!theWriter.WriteBool(aProjectile.mActive) ||
            !theWriter.WriteU8(aProjectile.mRow) ||
            !theWriter.WriteI32(
                aProjectile.mXMilliPixels) ||
            !theWriter.WriteI32(
                aProjectile.mYMilliPixels) ||
            !theWriter.WriteU16(aProjectile.mAge))
        {
            return false;
        }
    }
    return true;
}

bool LevelOneCombat::LoadState(
    engine::IStateReader& theReader)
{
    return LoadState(theReader, true);
}

bool LevelOneCombat::LoadState(
    engine::IStateReader& theReader,
    bool theHasExtendedCombatState)
{
    LevelOneCombatState aState;
    std::uint8_t aPhase{};
    if (!theReader.ReadU8(aPhase) ||
        !theReader.ReadU32(aState.mTick) ||
        !theReader.ReadU16(aState.mSunCountdown) ||
        !theReader.ReadU8(aState.mSunsSpawned) ||
        !theReader.ReadU8(aState.mSunCount))
    {
        return false;
    }
    for (auto& aSun : aState.mSuns)
    {
        if (!theReader.ReadBool(aSun.mActive) ||
            (theHasExtendedCombatState &&
             !theReader.ReadBool(aSun.mBeingCollected)) ||
            !theReader.ReadI32(aSun.mXMilliPixels) ||
            !theReader.ReadI32(aSun.mYMilliPixels) ||
            !theReader.ReadI32(aSun.mGroundYMilliPixels) ||
            !theReader.ReadU16(aSun.mAge))
        {
            return false;
        }
    }
    if (!theReader.ReadU16(aState.mZombieCountdown) ||
        !theReader.ReadBool(aState.mFirstWaveSpawned) ||
        !theReader.ReadBool(aState.mFirstWaveCleared) ||
        !theReader.ReadU8(aState.mPlantCount) ||
        !theReader.ReadU8(aState.mZombieCount) ||
        !theReader.ReadU8(aState.mProjectileCount))
    {
        return false;
    }
    aState.mPhase =
        static_cast<LevelOneCombatPhase>(aPhase);

    for (auto& aPlant : aState.mPlants)
    {
        if (!theReader.ReadBool(aPlant.mActive) ||
            !theReader.ReadU8(aPlant.mColumn) ||
            !theReader.ReadU8(aPlant.mRow) ||
            !theReader.ReadU16(aPlant.mHealth) ||
            !theReader.ReadU16(aPlant.mLaunchCounter) ||
            !theReader.ReadU8(aPlant.mShootingCounter))
        {
            return false;
        }
    }
    for (auto& aZombie : aState.mZombies)
    {
        std::uint16_t aLegacySpeedMilliPixelsPerTick{};
        if (!theReader.ReadBool(aZombie.mActive) ||
            !theReader.ReadU8(aZombie.mRow) ||
            !theReader.ReadU16(aZombie.mHealth) ||
            !theReader.ReadI32(aZombie.mXMilliPixels) ||
            (theHasExtendedCombatState
                 ? (!theReader.ReadU32(
                        aZombie.mSpeedMicroPixelsPerTick) ||
                    !theReader.ReadU16(
                        aZombie.mMovementRemainderMicroPixels))
                 : !theReader.ReadU16(
                       aLegacySpeedMilliPixelsPerTick)) ||
            !theReader.ReadU32(aZombie.mAge) ||
            !theReader.ReadBool(aZombie.mEating))
        {
            return false;
        }
        if (!theHasExtendedCombatState)
        {
            aZombie.mSpeedMicroPixelsPerTick =
                static_cast<std::uint32_t>(
                    aLegacySpeedMilliPixelsPerTick) *
                1'000U;
        }
    }
    for (auto& aProjectile : aState.mProjectiles)
    {
        if (!theReader.ReadBool(aProjectile.mActive) ||
            !theReader.ReadU8(aProjectile.mRow) ||
            !theReader.ReadI32(
                aProjectile.mXMilliPixels) ||
            !theReader.ReadI32(
                aProjectile.mYMilliPixels) ||
            !theReader.ReadU16(aProjectile.mAge))
        {
            return false;
        }
    }
    return RestoreState(aState);
}

std::uint64_t LevelOneCombat::GetOccupiedCells() const
{
    std::uint64_t anOccupiedCells{};
    for (const auto& aPlant : mState.mPlants)
    {
        if (aPlant.mActive)
        {
            anOccupiedCells |=
                CellMask(aPlant.mColumn, aPlant.mRow);
        }
    }
    return anOccupiedCells;
}

bool LevelOneCombat::HasRandomDecisionFailure() const
{
    return mRandomDecisionFailure;
}

void LevelOneCombat::UpdatePlants()
{
    for (auto& aPlant : mState.mPlants)
    {
        if (!aPlant.mActive)
            continue;

        if (aPlant.mShootingCounter > 0)
        {
            --aPlant.mShootingCounter;
            if (aPlant.mShootingCounter == 1)
                FirePea(aPlant);
        }

        if (aPlant.mLaunchCounter > 0)
            --aPlant.mLaunchCounter;
        if (aPlant.mLaunchCounter == 0)
        {
            aPlant.mLaunchCounter =
                kPeashooterLaunchRate;
            if (HasTarget(aPlant))
            {
                aPlant.mShootingCounter =
                    kPeashooterFireDelay;
            }
        }
    }
}

void LevelOneCombat::UpdateZombies()
{
    for (auto& aZombie : mState.mZombies)
    {
        if (!aZombie.mActive)
            continue;

        ++aZombie.mAge;
        if (!aZombie.mEating)
        {
            const auto aMovementMicroPixels =
                aZombie.mSpeedMicroPixelsPerTick +
                aZombie.mMovementRemainderMicroPixels;
            aZombie.mXMilliPixels -=
                static_cast<std::int32_t>(
                    aMovementMicroPixels / 1'000U);
            aZombie.mMovementRemainderMicroPixels =
                static_cast<std::uint16_t>(
                    aMovementMicroPixels % 1'000U);
        }

        if (aZombie.mAge % kEatInterval == 0)
        {
            auto* aPlant = FindPlantTarget(aZombie);
            if (aPlant)
            {
                aZombie.mEating = true;
                if (aPlant->mHealth <= kEatDamage)
                {
                    mDestroyedCells |=
                        CellMask(
                            aPlant->mColumn,
                            aPlant->mRow);
                    *aPlant = {};
                }
                else
                {
                    aPlant->mHealth =
                        static_cast<std::uint16_t>(
                            aPlant->mHealth - kEatDamage);
                }
            }
            else
            {
                aZombie.mEating = false;
            }
        }

        if (aZombie.mXMilliPixels <
            kZombieLossXMilliPixels)
        {
            mState.mPhase = LevelOneCombatPhase::Lost;
        }
    }
}

void LevelOneCombat::UpdateProjectiles()
{
    for (auto& aProjectile : mState.mProjectiles)
    {
        if (!aProjectile.mActive)
            continue;

        ++aProjectile.mAge;
        aProjectile.mXMilliPixels +=
            kPeaSpeedMilliPixelsPerTick;
        const auto aProjectileX =
            ToPixels(aProjectile.mXMilliPixels);

        LevelOneZombieState* aTarget{};
        std::int32_t aTargetX{};
        for (auto& aZombie : mState.mZombies)
        {
            if (!aZombie.mActive ||
                aZombie.mRow != aProjectile.mRow)
            {
                continue;
            }
            const auto aZombieX =
                ToPixels(aZombie.mXMilliPixels);
            if (GetOverlap(
                    aProjectileX - 15,
                    55,
                    aZombieX + kNormalZombieRectX,
                    kNormalZombieRectWidth) <= 0)
            {
                continue;
            }
            if (!aTarget || aZombieX < aTargetX)
            {
                aTarget = &aZombie;
                aTargetX = aZombieX;
            }
        }
        if (aTarget)
        {
            if (aTarget->mHealth <= kPeaDamage)
                *aTarget = {};
            else
            {
                aTarget->mHealth =
                    static_cast<std::uint16_t>(
                        aTarget->mHealth - kPeaDamage);
            }
            aProjectile = {};
        }
        else if (
            aProjectile.mXMilliPixels >
            kProjectileMaximumXMilliPixels)
        {
            aProjectile = {};
        }
    }
}

void LevelOneCombat::UpdateSun()
{
    if (mState.mPhase ==
        LevelOneCombatPhase::AwaitingFirstPlant)
    {
        return;
    }

    for (auto& aSun : mState.mSuns)
    {
        if (!aSun.mActive)
            continue;
        ++aSun.mAge;
        if (aSun.mBeingCollected)
        {
            const auto aDeltaX = std::abs(
                aSun.mXMilliPixels -
                kSunCollectionDestinationXMilliPixels);
            const auto aDeltaY = std::abs(
                aSun.mYMilliPixels -
                kSunCollectionDestinationYMilliPixels);
            if (aSun.mXMilliPixels >
                kSunCollectionDestinationXMilliPixels)
            {
                aSun.mXMilliPixels -= aDeltaX / 21;
            }
            else if (aSun.mXMilliPixels <
                     kSunCollectionDestinationXMilliPixels)
            {
                aSun.mXMilliPixels += aDeltaX / 21;
            }
            if (aSun.mYMilliPixels >
                kSunCollectionDestinationYMilliPixels)
            {
                aSun.mYMilliPixels -= aDeltaY / 21;
            }
            else if (aSun.mYMilliPixels <
                     kSunCollectionDestinationYMilliPixels)
            {
                aSun.mYMilliPixels += aDeltaY / 21;
            }
            const auto aDistanceSquared =
                static_cast<std::int64_t>(aDeltaX) * aDeltaX +
                static_cast<std::int64_t>(aDeltaY) * aDeltaY;
            constexpr auto kScoringDistanceSquared =
                static_cast<std::int64_t>(
                    kSunScoringDistanceMilliPixels) *
                kSunScoringDistanceMilliPixels;
            if (aDistanceSquared < kScoringDistanceSquared)
            {
                aSun = {};
                mCollectedSun = static_cast<std::uint16_t>(
                    mCollectedSun + kSunValue);
            }
            continue;
        }
        if (aSun.mYMilliPixels <
            aSun.mGroundYMilliPixels)
        {
            aSun.mYMilliPixels =
                std::min(
                    aSun.mGroundYMilliPixels,
                    aSun.mYMilliPixels +
                        kSunFallSpeedMilliPixelsPerTick);
        }
        if (aSun.mAge >=
            CalculateSunLifetime(
                aSun.mGroundYMilliPixels))
            aSun = {};
    }

    if (mState.mSunCountdown > 0)
        --mState.mSunCountdown;
    if (mState.mSunCountdown != 0)
        return;

    const auto aSlot = std::find_if(
        mState.mSuns.begin(),
        mState.mSuns.end(),
        [](const LevelOneSunState& theSun)
        {
            return !theSun.mActive;
        });
    if (aSlot == mState.mSuns.end())
        return;

    LevelOneRandomDecision aDecision;
    if (!ReadFallingSunDecision(aDecision))
        return;

    *aSlot = {
        .mActive = true,
        .mBeingCollected = false,
        .mXMilliPixels = aDecision.mXMilliPixels,
        .mYMilliPixels =
            kDeterministicSunSpawnYMilliPixels,
        .mGroundYMilliPixels =
            aDecision.mGroundYMilliPixels,
        .mAge = 0,
    };
    if (mState.mSunsSpawned <
        std::numeric_limits<std::uint8_t>::max())
    {
        ++mState.mSunsSpawned;
    }
    mState.mSunCountdown = aDecision.mNextCountdown;
}

void LevelOneCombat::UpdateWave()
{
    if (mState.mPhase != LevelOneCombatPhase::Active ||
        mState.mFirstWaveSpawned)
    {
        return;
    }
    if (mState.mZombieCountdown > 0)
        --mState.mZombieCountdown;
    if (mState.mZombieCountdown == 0)
        SpawnFirstWave();
}

bool LevelOneCombat::HasTarget(
    const LevelOnePlantCombatState& thePlant) const
{
    const auto aPlantOrigin =
        BoardGeometry::GridToPixel(
            BoardStageLayout::Day,
            thePlant.mColumn,
            thePlant.mRow);
    for (const auto& aZombie : mState.mZombies)
    {
        if (!aZombie.mActive ||
            aZombie.mRow != thePlant.mRow)
        {
            continue;
        }
        const auto aZombieX =
            ToPixels(aZombie.mXMilliPixels);
        if (GetOverlap(
                aPlantOrigin.mX + 60,
                800,
                aZombieX + kNormalZombieRectX,
                kNormalZombieRectWidth) > 0)
        {
            return true;
        }
    }
    return false;
}

void LevelOneCombat::FirePea(
    const LevelOnePlantCombatState& thePlant)
{
    const auto aSlot = std::find_if(
        mState.mProjectiles.begin(),
        mState.mProjectiles.end(),
        [](const LevelOneProjectileState& theProjectile)
        {
            return !theProjectile.mActive;
        });
    if (aSlot == mState.mProjectiles.end())
        return;

    const auto anOrigin =
        BoardGeometry::GridToPixel(
            BoardStageLayout::Day,
            thePlant.mColumn,
            thePlant.mRow);
    *aSlot = {
        .mActive = true,
        .mRow = thePlant.mRow,
        .mXMilliPixels =
            (anOrigin.mX + 68) * 1'000,
        .mYMilliPixels =
            (anOrigin.mY + 20) * 1'000,
        .mAge = 0,
    };
}

LevelOnePlantCombatState*
LevelOneCombat::FindPlantTarget(
    const LevelOneZombieState& theZombie)
{
    const auto aZombieX =
        ToPixels(theZombie.mXMilliPixels);
    for (auto& aPlant : mState.mPlants)
    {
        if (!aPlant.mActive ||
            aPlant.mRow != theZombie.mRow)
        {
            continue;
        }
        const auto aPlantOrigin =
            BoardGeometry::GridToPixel(
                BoardStageLayout::Day,
                aPlant.mColumn,
                aPlant.mRow);
        if (GetOverlap(
                aZombieX + kNormalZombieAttackRectX,
                kNormalZombieAttackRectWidth,
                aPlantOrigin.mX + 10,
                60) >= 20)
        {
            return &aPlant;
        }
    }
    return nullptr;
}

void LevelOneCombat::SpawnFirstWave()
{
    const auto aSlot = std::find_if(
        mState.mZombies.begin(),
        mState.mZombies.end(),
        [](const LevelOneZombieState& theZombie)
        {
            return !theZombie.mActive;
        });
    if (aSlot == mState.mZombies.end())
        return;

    LevelOneRandomDecision aDecision;
    if (!ReadNormalZombieDecision(aDecision))
        return;

    *aSlot = {
        .mActive = true,
        .mRow = kLaneRow,
        .mHealth = kNormalZombieHealth,
        .mXMilliPixels = aDecision.mXMilliPixels,
        .mSpeedMicroPixelsPerTick =
            aDecision.mSpeedMicroPixelsPerTick,
        .mMovementRemainderMicroPixels = 0,
        .mAge = 0,
        .mEating = false,
    };
    mState.mFirstWaveSpawned = true;
    RecountEntities();
}

void LevelOneCombat::RecountEntities()
{
    mState.mSunCount =
        CountActive(mState.mSuns);
    mState.mPlantCount =
        CountActive(mState.mPlants);
    mState.mZombieCount =
        CountActive(mState.mZombies);
    mState.mProjectileCount =
        CountActive(mState.mProjectiles);
}

bool LevelOneCombat::ReadFallingSunDecision(
    LevelOneRandomDecision& theDecision)
{
    if (mRandomDecisionFailure)
        return false;
    if (mRandomDecisionSource == nullptr)
    {
        theDecision = {
            .mKind = LevelOneRandomDecisionKind::FallingSun,
            .mNextCountdown = kDeterministicNextSunCountdown,
            .mXMilliPixels = kDeterministicSunSpawnXMilliPixels,
            .mGroundYMilliPixels =
                kDeterministicSunGroundYMilliPixels,
            .mSpeedMicroPixelsPerTick = 0,
        };
        return true;
    }
    if (!mRandomDecisionSource->ReadNext(
            LevelOneRandomDecisionKind::FallingSun,
            theDecision) ||
        theDecision.mKind !=
            LevelOneRandomDecisionKind::FallingSun ||
        theDecision.mNextCountdown <
            kDeterministicNextSunCountdown ||
        theDecision.mNextCountdown > kMaximumSunCountdown ||
        theDecision.mXMilliPixels <
            kMinimumSunSpawnXMilliPixels ||
        theDecision.mXMilliPixels >
            kMaximumSunSpawnXMilliPixels ||
        theDecision.mGroundYMilliPixels <
            kMinimumSunGroundYMilliPixels ||
        theDecision.mGroundYMilliPixels >
            kMaximumSunGroundYMilliPixels ||
        theDecision.mSpeedMicroPixelsPerTick != 0)
    {
        mRandomDecisionFailure = true;
        return false;
    }
    return true;
}

bool LevelOneCombat::ReadNormalZombieDecision(
    LevelOneRandomDecision& theDecision)
{
    if (mRandomDecisionFailure)
        return false;
    if (mRandomDecisionSource == nullptr)
    {
        theDecision = {
            .mKind = LevelOneRandomDecisionKind::NormalZombie,
            .mNextCountdown = 0,
            .mXMilliPixels =
                kDeterministicZombieSpawnXMilliPixels,
            .mGroundYMilliPixels = 0,
            .mSpeedMicroPixelsPerTick =
                static_cast<std::uint32_t>(
                    kDeterministicZombieSpeedMilliPixelsPerTick) *
                1'000U,
        };
        return true;
    }
    if (!mRandomDecisionSource->ReadNext(
            LevelOneRandomDecisionKind::NormalZombie,
            theDecision) ||
        theDecision.mKind !=
            LevelOneRandomDecisionKind::NormalZombie ||
        theDecision.mNextCountdown != 0 ||
        theDecision.mXMilliPixels <
            kMinimumZombieSpawnXMilliPixels ||
        theDecision.mXMilliPixels >
            kMaximumZombieSpawnXMilliPixels ||
        theDecision.mGroundYMilliPixels != 0 ||
        theDecision.mSpeedMicroPixelsPerTick <
            kMinimumZombieSpeedMicroPixelsPerTick ||
        theDecision.mSpeedMicroPixelsPerTick >
            kMaximumZombieSpeedMicroPixelsPerTick)
    {
        mRandomDecisionFailure = true;
        return false;
    }
    return true;
}

std::uint16_t LevelOneCombat::CalculateSunLifetime(
    std::int32_t theGroundYMilliPixels)
{
    const auto aFallDistance =
        theGroundYMilliPixels -
        kDeterministicSunSpawnYMilliPixels;
    const auto aFallTicks =
        (aFallDistance +
         kSunFallSpeedMilliPixelsPerTick - 1) /
        kSunFallSpeedMilliPixelsPerTick;
    return static_cast<std::uint16_t>(
        aFallTicks +
        static_cast<std::int32_t>(
            kSunGroundDisappearTicks - 1U + kSunFadeTicks));
}

} // namespace pvz::game
