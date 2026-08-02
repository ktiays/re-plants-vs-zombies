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
inline constexpr std::array<std::uint8_t, LevelOneCombat::kWaveCount>
    kNormalZombiesPerWave{1, 1, 1, 2};
inline constexpr std::uint16_t kNoWaveHealthThreshold = 0xFFFFU;
inline constexpr std::int32_t kAwardRowYMilliPixels = 337'000;

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
    mLastZombieDeathXMilliPixels = 0;
    mLastZombieDeathYMilliPixels = 0;
}

void LevelOneCombat::Update()
{
    if (mState.mPhase == LevelOneCombatPhase::Lost ||
        mState.mPhase == LevelOneCombatPhase::Won)
        return;

    mLastZombieDeathXMilliPixels = 0;
    mLastZombieDeathYMilliPixels = 0;
    ++mState.mTick;
    UpdatePlants();
    UpdateZombies();
    UpdateProjectiles();
    if (mState.mPhase != LevelOneCombatPhase::Lost)
        UpdateMower();
    RecountEntities();
    UpdateLevelProgress();
    if (mState.mPhase == LevelOneCombatPhase::Lost ||
        mState.mPhase == LevelOneCombatPhase::Won)
    {
        return;
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
        mState.mPhase == LevelOneCombatPhase::Lost ||
        mState.mPhase == LevelOneCombatPhase::Won)
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

    LevelOneRandomDecision aScheduleDecision;
    if (!ReadPeashooterScheduleDecision(
            theColumn,
            true,
            aScheduleDecision))
    {
        return false;
    }

    *aSlot = {
        .mActive = true,
        .mColumn = theColumn,
        .mRow = theRow,
        .mHealth = kPlantHealth,
        .mLaunchCounter =
            aScheduleDecision.mNextCountdown,
        .mShootingCounter =
            aScheduleDecision.mShootingCounter,
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
        mState.mZombieCountdownStart = kFirstWaveCountdown;
    }
    return true;
}

bool LevelOneCombat::TryCollectSun(
    engine::PointI thePosition)
{
    constexpr std::int64_t kMilliPixelsPerPixel = 1'000;
    constexpr std::int64_t kClickMarginMilliPixels = 15'000;
    constexpr std::int64_t kSunSizeMilliPixels = 60'000;
    const auto aPointerX =
        static_cast<std::int64_t>(thePosition.mX) *
        kMilliPixelsPerPixel;
    const auto aPointerY =
        static_cast<std::int64_t>(thePosition.mY) *
        kMilliPixelsPerPixel;
    for (auto& aSun : mState.mSuns)
    {
        if (!aSun.mActive || aSun.mBeingCollected)
            continue;
        const auto aSunX =
            static_cast<std::int64_t>(aSun.mXMilliPixels);
        const auto aSunY =
            static_cast<std::int64_t>(aSun.mYMilliPixels);
        if (aPointerX < aSunX - kClickMarginMilliPixels ||
            aPointerX >=
                aSunX + kSunSizeMilliPixels +
                    kClickMarginMilliPixels ||
            aPointerY < aSunY - kClickMarginMilliPixels ||
            aPointerY >=
                aSunY + kSunSizeMilliPixels +
                    kClickMarginMilliPixels)
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
        theState.mZombieCountdown > kNextWaveCountdownMaximum ||
        theState.mZombieCountdownStart >
            kNextWaveCountdownMaximum ||
        theState.mZombieCountdown >
            theState.mZombieCountdownStart ||
        theState.mCurrentWave > kWaveCount ||
        theState.mMowerPhase >= LevelOneMowerPhase::Count ||
        theState.mMowerXMilliPixels < kMowerReadyXMilliPixels ||
        theState.mMowerXMilliPixels >
            kMowerMaximumXMilliPixels +
                kMowerSpeedMilliPixelsPerTick ||
        theState.mMowerChompCounter > 50)
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
            aPlant.mLaunchCounter >
                kPeashooterLaunchRate + 1U ||
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
            aZombie.mMovementRemainderMicroPixels >= 1'000 ||
            aZombie.mFromWave >= kWaveCount ||
            aZombie.mFromWave >= theState.mCurrentWave)
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
        (theState.mCurrentWave == 0 &&
         (theState.mFirstWaveSpawned ||
          theState.mFirstWaveCleared)) ||
        (theState.mCurrentWave > 0 &&
         !theState.mFirstWaveSpawned) ||
        (theState.mFirstWaveCleared &&
         (!theState.mFirstWaveSpawned ||
          std::any_of(
              theState.mZombies.begin(),
              theState.mZombies.end(),
              [](const LevelOneZombieState& theZombie)
              {
                  return theZombie.mActive &&
                         theZombie.mFromWave == 0;
              }))) ||
        (theState.mPhase == LevelOneCombatPhase::Won &&
         (!theState.mAwardSpawned ||
          theState.mCurrentWave != kWaveCount ||
          theState.mZombieCount != 0)) ||
        (theState.mAwardSpawned &&
         theState.mPhase != LevelOneCombatPhase::Won) ||
        (theState.mAwardSpawned &&
         (theState.mAwardXMilliPixels <
              kMinimumStateXMilliPixels ||
          theState.mAwardXMilliPixels >
              kMaximumStateXMilliPixels ||
          theState.mAwardYMilliPixels !=
              kAwardRowYMilliPixels)) ||
        (!theState.mAwardSpawned &&
         (theState.mAwardXMilliPixels != 0 ||
          theState.mAwardYMilliPixels != 0)) ||
        (theState.mMowerPhase == LevelOneMowerPhase::Ready &&
         (theState.mMowerXMilliPixels !=
              kMowerReadyXMilliPixels ||
          theState.mMowerChompCounter != 0)) ||
        (theState.mMowerPhase == LevelOneMowerPhase::Spent &&
         theState.mMowerXMilliPixels <=
             kMowerMaximumXMilliPixels) ||
        (theState.mCurrentWave == 0 &&
         theState.mZombieHealthToNextWave !=
             kNoWaveHealthThreshold) ||
        (theState.mCurrentWave > 0 &&
         (theState.mZombieHealthWaveStart !=
              static_cast<std::uint16_t>(
                  kNormalZombiesPerWave[
                      theState.mCurrentWave - 1U] *
                  kNormalZombieHealth) ||
          theState.mZombieHealthToNextWave ==
              kNoWaveHealthThreshold ||
          theState.mZombieHealthToNextWave <
              theState.mZombieHealthWaveStart / 2U ||
          theState.mZombieHealthToNextWave >
              (static_cast<std::uint32_t>(
                   theState.mZombieHealthWaveStart) *
               65U) /
                  100U)))
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
    if (!theWriter.WriteU8(mState.mCurrentWave) ||
        !theWriter.WriteU16(mState.mZombieCountdownStart) ||
        !theWriter.WriteU16(mState.mZombieHealthWaveStart) ||
        !theWriter.WriteU16(mState.mZombieHealthToNextWave) ||
        !theWriter.WriteBool(mState.mAwardSpawned) ||
        !theWriter.WriteI32(mState.mAwardXMilliPixels) ||
        !theWriter.WriteI32(mState.mAwardYMilliPixels) ||
        !theWriter.WriteU8(
            static_cast<std::uint8_t>(mState.mMowerPhase)) ||
        !theWriter.WriteI32(mState.mMowerXMilliPixels) ||
        !theWriter.WriteU8(mState.mMowerChompCounter))
    {
        return false;
    }
    for (const auto& aZombie : mState.mZombies)
    {
        if (!theWriter.WriteU8(aZombie.mFromWave))
            return false;
    }
    return true;
}

bool LevelOneCombat::LoadState(
    engine::IStateReader& theReader)
{
    return LoadState(theReader, true, true);
}

bool LevelOneCombat::LoadState(
    engine::IStateReader& theReader,
    bool theHasExtendedCombatState,
    bool theHasCompleteLevelState)
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
    if (theHasCompleteLevelState)
    {
        std::uint8_t aMowerPhase{};
        if (!theReader.ReadU8(aState.mCurrentWave) ||
            !theReader.ReadU16(
                aState.mZombieCountdownStart) ||
            !theReader.ReadU16(
                aState.mZombieHealthWaveStart) ||
            !theReader.ReadU16(
                aState.mZombieHealthToNextWave) ||
            !theReader.ReadBool(aState.mAwardSpawned) ||
            !theReader.ReadI32(
                aState.mAwardXMilliPixels) ||
            !theReader.ReadI32(
                aState.mAwardYMilliPixels) ||
            !theReader.ReadU8(aMowerPhase) ||
            !theReader.ReadI32(
                aState.mMowerXMilliPixels) ||
            !theReader.ReadU8(
                aState.mMowerChompCounter))
        {
            return false;
        }
        aState.mMowerPhase =
            static_cast<LevelOneMowerPhase>(aMowerPhase);
        for (auto& aZombie : aState.mZombies)
        {
            if (!theReader.ReadU8(aZombie.mFromWave))
                return false;
        }
    }
    else
    {
        aState.mCurrentWave =
            aState.mFirstWaveSpawned ? 1 : 0;
        aState.mZombieCountdownStart =
            aState.mCurrentWave == 0
            ? (aState.mPhase == LevelOneCombatPhase::Active
                   ? kFirstWaveCountdown
                   : 0)
            : kNextWaveCountdownMinimum;
        if (aState.mCurrentWave > 0)
        {
            aState.mZombieCountdown =
                kNextWaveCountdownMinimum;
            aState.mZombieHealthWaveStart =
                kNormalZombieHealth;
            aState.mZombieHealthToNextWave =
                kNormalZombieHealth / 2U;
        }
        if (aState.mPhase ==
            LevelOneCombatPhase::FirstWaveCleared)
        {
            aState.mPhase = LevelOneCombatPhase::Active;
        }
        for (auto& aZombie : aState.mZombies)
            aZombie.mFromWave = 0;
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
            {
                FirePea(aPlant);
                if (mRandomDecisionFailure)
                    return;
            }
        }

        if (aPlant.mLaunchCounter > 0)
            --aPlant.mLaunchCounter;
        if (aPlant.mLaunchCounter == 0)
        {
            LevelOneRandomDecision aScheduleDecision;
            if (!ReadPeashooterScheduleDecision(
                    aPlant.mColumn,
                    false,
                    aScheduleDecision))
            {
                return;
            }
            aPlant.mLaunchCounter =
                aScheduleDecision.mNextCountdown;
            aPlant.mShootingCounter =
                aScheduleDecision.mShootingCounter;
        }
    }
}

void LevelOneCombat::UpdateZombies()
{
    for (std::size_t aZombieIndex = 0;
         aZombieIndex < mState.mZombies.size();
         ++aZombieIndex)
    {
        auto& aZombie = mState.mZombies[aZombieIndex];
        if (!aZombie.mActive)
            continue;

        ++aZombie.mAge;
        LevelOneRandomDecision aMotionDecision;
        if (!ReadZombieMotionDecision(
                static_cast<std::uint8_t>(aZombieIndex),
                aMotionDecision))
        {
            return;
        }
        if (mRandomDecisionSource != nullptr &&
            mRandomDecisionSource->Supports(
                LevelOneRandomDecisionKind::ZombieMotion))
        {
            aZombie.mXMilliPixels =
                aMotionDecision.mXMilliPixels;
            aZombie.mMovementRemainderMicroPixels = 0;
        }
        else if (!aZombie.mEating)
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
            kZombieLossXMilliPixels ||
            ToPixels(aZombie.mXMilliPixels) <=
                ToPixels(kZombieLossXMilliPixels))
        {
            mState.mPhase = LevelOneCombatPhase::Lost;
        }

        // The legacy headless-zombie decay runs after movement, prey checks,
        // and the board-edge check, but before projectiles update.
        if (aMotionDecision.mShootingCounter != 0)
        {
            if (aZombie.mHealth <= 1)
            {
                mLastZombieDeathXMilliPixels =
                    aZombie.mXMilliPixels;
                mLastZombieDeathYMilliPixels =
                    kAwardRowYMilliPixels;
                aZombie = {};
                continue;
            }
            --aZombie.mHealth;
        }
    }
}

void LevelOneCombat::UpdateProjectiles()
{
    for (std::size_t aProjectileIndex = 0;
         aProjectileIndex < mState.mProjectiles.size();
         ++aProjectileIndex)
    {
        auto& aProjectile = mState.mProjectiles[aProjectileIndex];
        if (!aProjectile.mActive)
            continue;

        ++aProjectile.mAge;
        // Legacy UpdateNormalMotion advances mPosX before collision, but
        // GetProjectileRect still reads the previous integer mX.  The integer
        // position is synchronized only after the collision check.
        const auto aProjectileX =
            ToPixels(aProjectile.mXMilliPixels);
        LevelOneRandomDecision aMotionDecision;
        if (!ReadProjectileMotionDecision(
                static_cast<std::uint8_t>(aProjectileIndex),
                aMotionDecision))
        {
            return;
        }
        if (mRandomDecisionSource != nullptr &&
            mRandomDecisionSource->Supports(
                LevelOneRandomDecisionKind::ProjectileMotion))
        {
            aProjectile.mXMilliPixels =
                aMotionDecision.mXMilliPixels;
        }
        else
        {
            aProjectile.mXMilliPixels +=
                kPeaSpeedMilliPixelsPerTick;
        }

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
            const std::uint16_t aHealthAfterDamage =
                aTarget->mHealth <= kPeaDamage
                ? std::uint16_t{0}
                : static_cast<std::uint16_t>(
                      aTarget->mHealth - kPeaDamage);
            const auto anActiveZombieCount =
                CountActive(mState.mZombies);
            // Zombie::UpdateDamageStates drops the head below one-third body
            // health. DropLoot then calls TrySpawnLevelAward; on the final
            // wave, a headless last enemy no longer counts as an enemy on
            // screen, so the legacy board awards the level and removes it
            // immediately even though body health has not reached zero.
            const bool dropsFinalLevelAward =
                mState.mCurrentWave == kWaveCount &&
                anActiveZombieCount == 1 &&
                aHealthAfterDamage <
                    kNormalZombieHeadLossHealth;
            if (aHealthAfterDamage == 0 ||
                dropsFinalLevelAward)
            {
                mLastZombieDeathXMilliPixels =
                    aTarget->mXMilliPixels;
                mLastZombieDeathYMilliPixels =
                    kAwardRowYMilliPixels;
                *aTarget = {};
            }
            else
            {
                aTarget->mHealth = aHealthAfterDamage;
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

void LevelOneCombat::UpdateMower()
{
    if (mState.mMowerPhase == LevelOneMowerPhase::Spent)
        return;

    const auto aMowerX = ToPixels(
        mState.mMowerXMilliPixels);
    for (auto& aZombie : mState.mZombies)
    {
        if (!aZombie.mActive ||
            aZombie.mRow != kLaneRow)
        {
            continue;
        }
        const auto aZombieX = ToPixels(
            aZombie.mXMilliPixels);
        if (GetOverlap(
                aMowerX,
                50,
                aZombieX + kNormalZombieRectX,
                kNormalZombieRectWidth) <= 0)
        {
            continue;
        }

        if (mState.mMowerPhase == LevelOneMowerPhase::Ready)
        {
            mState.mMowerPhase =
                LevelOneMowerPhase::Triggered;
            mState.mMowerChompCounter = 25;
        }
        else
        {
            mState.mMowerChompCounter = 50;
        }
        mLastZombieDeathXMilliPixels =
            aZombie.mXMilliPixels;
        mLastZombieDeathYMilliPixels =
            kAwardRowYMilliPixels;
        aZombie = {};
    }

    if (mState.mMowerPhase != LevelOneMowerPhase::Triggered)
        return;

    std::int32_t aSpeed = kMowerSpeedMilliPixelsPerTick;
    if (mState.mMowerChompCounter > 0)
    {
        --mState.mMowerChompCounter;
        const auto aTimeNumerator = static_cast<std::int32_t>(
            50U - mState.mMowerChompCounter);
        const auto aBounceNumerator =
            50 - std::abs(2 * aTimeNumerator - 50);
        const auto aWarpedNumerator =
            2 * aBounceNumerator * 50 -
            aBounceNumerator * aBounceNumerator;
        constexpr std::int32_t kCurveDenominator = 2'500;
        aSpeed =
            (kMowerSpeedMilliPixelsPerTick *
                 kCurveDenominator -
             2'330 * aWarpedNumerator +
             kCurveDenominator / 2) /
            kCurveDenominator;
    }
    mState.mMowerXMilliPixels += aSpeed;
    if (mState.mMowerXMilliPixels >
        kMowerMaximumXMilliPixels)
    {
        mState.mMowerPhase = LevelOneMowerPhase::Spent;
        mState.mMowerChompCounter = 0;
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
        mState.mCurrentWave >= kWaveCount)
    {
        return;
    }
    if (mState.mZombieCountdown > 0)
        --mState.mZombieCountdown;
    if (mState.mCurrentWave > 0 &&
        mState.mZombieCountdown >
            kWaveAccelerationCountdown &&
        mState.mZombieCountdownStart -
                mState.mZombieCountdown >
            kWaveAccelerationMinimumAge &&
        TotalZombieHealthInWave(
            static_cast<std::uint8_t>(
                mState.mCurrentWave - 1U)) <=
            mState.mZombieHealthToNextWave)
    {
        mState.mZombieCountdown =
            kWaveAccelerationCountdown;
    }
    if (mState.mZombieCountdown == 0)
        SpawnWave();
}

void LevelOneCombat::UpdateLevelProgress()
{
    if (mState.mFirstWaveSpawned &&
        !mState.mFirstWaveCleared)
    {
        const bool hasFirstWaveZombie = std::any_of(
            mState.mZombies.begin(),
            mState.mZombies.end(),
            [](const LevelOneZombieState& theZombie)
            {
                return theZombie.mActive &&
                       theZombie.mFromWave == 0;
            });
        mState.mFirstWaveCleared = !hasFirstWaveZombie;
    }

    if (mState.mCurrentWave != kWaveCount ||
        mState.mZombieCount != 0 ||
        mState.mAwardSpawned)
    {
        return;
    }
    mState.mAwardSpawned = true;
    mState.mPhase = LevelOneCombatPhase::Won;
    mState.mAwardXMilliPixels =
        mLastZombieDeathXMilliPixels != 0
        ? mLastZombieDeathXMilliPixels + 57'000
        : 400'000;
    mState.mAwardYMilliPixels =
        mLastZombieDeathYMilliPixels != 0
        ? mLastZombieDeathYMilliPixels
        : kAwardRowYMilliPixels;
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

    LevelOneRandomDecision aSpawnDecision;
    if (!ReadProjectileSpawnDecision(
            thePlant.mColumn,
            aSpawnDecision))
    {
        return;
    }
    *aSlot = {
        .mActive = true,
        .mRow = thePlant.mRow,
        .mXMilliPixels = aSpawnDecision.mXMilliPixels,
        .mYMilliPixels = aSpawnDecision.mGroundYMilliPixels,
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

void LevelOneCombat::SpawnWave()
{
    if (mState.mCurrentWave >= kWaveCount)
        return;

    const auto aZombieCount =
        kNormalZombiesPerWave[mState.mCurrentWave];
    const auto anAvailableCount = static_cast<std::uint8_t>(
        std::count_if(
            mState.mZombies.begin(),
            mState.mZombies.end(),
            [](const LevelOneZombieState& theZombie)
            {
                return !theZombie.mActive;
            }));
    if (anAvailableCount < aZombieCount)
        return;

    std::array<LevelOneRandomDecision, 2> aZombieDecisions;
    for (std::uint8_t anIndex = 0;
         anIndex < aZombieCount;
         ++anIndex)
    {
        if (!ReadNormalZombieDecision(
                aZombieDecisions[anIndex]))
        {
            return;
        }
    }
    const auto aWaveHealth = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(aZombieCount) *
        kNormalZombieHealth);
    LevelOneRandomDecision aScheduleDecision;
    if (!ReadWaveScheduleDecision(
            aWaveHealth,
            aScheduleDecision))
    {
        return;
    }

    for (std::uint8_t anIndex = 0;
         anIndex < aZombieCount;
         ++anIndex)
    {
        const auto aSlot = std::find_if(
            mState.mZombies.begin(),
            mState.mZombies.end(),
            [](const LevelOneZombieState& theZombie)
            {
                return !theZombie.mActive;
            });
        *aSlot = {
            .mActive = true,
            .mRow = kLaneRow,
            .mHealth = kNormalZombieHealth,
            .mXMilliPixels =
                aZombieDecisions[anIndex].mXMilliPixels,
            .mSpeedMicroPixelsPerTick =
                aZombieDecisions[anIndex]
                    .mSpeedMicroPixelsPerTick,
            .mMovementRemainderMicroPixels = 0,
            .mAge = 0,
            .mEating = false,
            .mFromWave = mState.mCurrentWave,
        };
    }
    ++mState.mCurrentWave;
    mState.mFirstWaveSpawned = true;
    mState.mZombieHealthWaveStart = aWaveHealth;
    mState.mZombieHealthToNextWave =
        aScheduleDecision.mWaveHealthThreshold;
    mState.mZombieCountdown =
        aScheduleDecision.mNextCountdown;
    mState.mZombieCountdownStart =
        aScheduleDecision.mNextCountdown;
    RecountEntities();
}

std::uint16_t LevelOneCombat::TotalZombieHealthInWave(
    std::uint8_t theWave) const
{
    std::uint16_t aHealth{};
    for (const auto& aZombie : mState.mZombies)
    {
        if (aZombie.mActive &&
            aZombie.mFromWave == theWave)
        {
            aHealth = static_cast<std::uint16_t>(
                aHealth + aZombie.mHealth);
        }
    }
    return aHealth;
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
        theDecision.mSpeedMicroPixelsPerTick != 0 ||
        theDecision.mWaveHealthThreshold != 0 ||
        theDecision.mPlantColumn != 0xFFU ||
        theDecision.mShootingCounter != 0)
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
            kMaximumZombieSpeedMicroPixelsPerTick ||
        theDecision.mWaveHealthThreshold != 0 ||
        theDecision.mPlantColumn != 0xFFU ||
        theDecision.mShootingCounter != 0)
    {
        mRandomDecisionFailure = true;
        return false;
    }
    return true;
}

bool LevelOneCombat::ReadWaveScheduleDecision(
    std::uint16_t theWaveHealth,
    LevelOneRandomDecision& theDecision)
{
    if (mRandomDecisionFailure)
        return false;
    if (mRandomDecisionSource == nullptr ||
        !mRandomDecisionSource->Supports(
            LevelOneRandomDecisionKind::WaveSchedule))
    {
        theDecision = {
            .mKind =
                LevelOneRandomDecisionKind::WaveSchedule,
            .mNextCountdown =
                kNextWaveCountdownMinimum,
            .mXMilliPixels = 0,
            .mGroundYMilliPixels = 0,
            .mSpeedMicroPixelsPerTick = 0,
            .mWaveHealthThreshold =
                static_cast<std::uint16_t>(
                    theWaveHealth / 2U),
        };
        return true;
    }

    const auto aMinimumHealth = static_cast<std::uint16_t>(
        theWaveHealth / 2U);
    const auto aMaximumHealth = static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(theWaveHealth) * 65U) /
        100U);
    if (!mRandomDecisionSource->ReadNext(
            LevelOneRandomDecisionKind::WaveSchedule,
            theDecision) ||
        theDecision.mKind !=
            LevelOneRandomDecisionKind::WaveSchedule ||
        theDecision.mNextCountdown <
            kNextWaveCountdownMinimum ||
        theDecision.mNextCountdown >
            kNextWaveCountdownMaximum ||
        theDecision.mXMilliPixels != 0 ||
        theDecision.mGroundYMilliPixels != 0 ||
        theDecision.mSpeedMicroPixelsPerTick != 0 ||
        theDecision.mWaveHealthThreshold <
            aMinimumHealth ||
        theDecision.mWaveHealthThreshold >
            aMaximumHealth ||
        theDecision.mPlantColumn != 0xFFU ||
        theDecision.mShootingCounter != 0)
    {
        mRandomDecisionFailure = true;
        return false;
    }
    return true;
}

bool LevelOneCombat::ReadPeashooterScheduleDecision(
    std::uint8_t theColumn,
    bool theIsNewPlant,
    LevelOneRandomDecision& theDecision)
{
    if (mRandomDecisionFailure)
        return false;
    if (mRandomDecisionSource == nullptr ||
        !mRandomDecisionSource->Supports(
            LevelOneRandomDecisionKind::PeashooterSchedule))
    {
        std::uint8_t aShootingCounter{};
        if (!theIsNewPlant)
        {
            const auto aPlant = std::find_if(
                mState.mPlants.begin(),
                mState.mPlants.end(),
                [theColumn](const LevelOnePlantCombatState& thePlant)
                {
                    return thePlant.mActive &&
                           thePlant.mColumn == theColumn &&
                           thePlant.mRow == kLaneRow;
                });
            if (aPlant != mState.mPlants.end() &&
                HasTarget(*aPlant))
            {
                aShootingCounter = kPeashooterFireDelay;
            }
        }
        theDecision = {
            .mKind =
                LevelOneRandomDecisionKind::PeashooterSchedule,
            .mNextCountdown = static_cast<std::uint16_t>(
                kPeashooterLaunchRate +
                (theIsNewPlant ? 1U : 0U)),
            .mPlantColumn = theColumn,
            .mShootingCounter = aShootingCounter,
        };
        return true;
    }

    if (!mRandomDecisionSource->ReadNext(
            LevelOneRandomDecisionKind::PeashooterSchedule,
            theDecision) ||
        theDecision.mKind !=
            LevelOneRandomDecisionKind::PeashooterSchedule ||
        theDecision.mNextCountdown == 0 ||
        theDecision.mNextCountdown >
            static_cast<std::uint16_t>(
                kPeashooterLaunchRate +
                (theIsNewPlant ? 1U : 0U)) ||
        theDecision.mXMilliPixels != 0 ||
        theDecision.mGroundYMilliPixels != 0 ||
        theDecision.mSpeedMicroPixelsPerTick != 0 ||
        theDecision.mWaveHealthThreshold != 0 ||
        theDecision.mPlantColumn != theColumn ||
        (theIsNewPlant
             ? theDecision.mShootingCounter != 0
             : (theDecision.mShootingCounter != 0 &&
                theDecision.mShootingCounter !=
                    kPeashooterFireDelay)))
    {
        mRandomDecisionFailure = true;
        return false;
    }
    return true;
}

bool LevelOneCombat::ReadProjectileSpawnDecision(
    std::uint8_t theColumn,
    LevelOneRandomDecision& theDecision)
{
    if (mRandomDecisionFailure)
        return false;
    if (mRandomDecisionSource == nullptr ||
        !mRandomDecisionSource->Supports(
            LevelOneRandomDecisionKind::ProjectileSpawn))
    {
        const auto anOrigin =
            BoardGeometry::GridToPixel(
                BoardStageLayout::Day,
                theColumn,
                kLaneRow);
        theDecision = {
            .mKind = LevelOneRandomDecisionKind::ProjectileSpawn,
            .mXMilliPixels = (anOrigin.mX + 68) * 1'000,
            .mGroundYMilliPixels = (anOrigin.mY + 20) * 1'000,
            .mPlantColumn = theColumn,
        };
        return true;
    }

    if (!mRandomDecisionSource->ReadNext(
            LevelOneRandomDecisionKind::ProjectileSpawn,
            theDecision) ||
        theDecision.mKind !=
            LevelOneRandomDecisionKind::ProjectileSpawn ||
        theDecision.mNextCountdown != 0 ||
        theDecision.mXMilliPixels < -100'000 ||
        theDecision.mXMilliPixels > 900'000 ||
        theDecision.mGroundYMilliPixels < -100'000 ||
        theDecision.mGroundYMilliPixels > 700'000 ||
        theDecision.mSpeedMicroPixelsPerTick != 0 ||
        theDecision.mWaveHealthThreshold != 0 ||
        theDecision.mPlantColumn != theColumn ||
        theDecision.mShootingCounter != 0)
    {
        mRandomDecisionFailure = true;
        return false;
    }
    return true;
}

bool LevelOneCombat::ReadZombieMotionDecision(
    std::uint8_t theSlot,
    LevelOneRandomDecision& theDecision)
{
    if (mRandomDecisionFailure)
        return false;
    if (mRandomDecisionSource == nullptr ||
        !mRandomDecisionSource->Supports(
            LevelOneRandomDecisionKind::ZombieMotion))
    {
        theDecision = {
            .mKind = LevelOneRandomDecisionKind::ZombieMotion,
            .mPlantColumn = theSlot,
        };
        return true;
    }

    if (!mRandomDecisionSource->ReadNext(
            LevelOneRandomDecisionKind::ZombieMotion,
            theDecision) ||
        theDecision.mKind !=
            LevelOneRandomDecisionKind::ZombieMotion ||
        theDecision.mNextCountdown != 0 ||
        theDecision.mXMilliPixels < kMinimumStateXMilliPixels ||
        theDecision.mXMilliPixels > kMaximumStateXMilliPixels ||
        theDecision.mGroundYMilliPixels != 0 ||
        theDecision.mSpeedMicroPixelsPerTick != 0 ||
        theDecision.mWaveHealthThreshold != 0 ||
        theDecision.mPlantColumn != theSlot ||
        theDecision.mShootingCounter > 1)
    {
        mRandomDecisionFailure = true;
        return false;
    }
    return true;
}

bool LevelOneCombat::ReadProjectileMotionDecision(
    std::uint8_t theSlot,
    LevelOneRandomDecision& theDecision)
{
    if (mRandomDecisionFailure)
        return false;
    if (mRandomDecisionSource == nullptr ||
        !mRandomDecisionSource->Supports(
            LevelOneRandomDecisionKind::ProjectileMotion))
    {
        theDecision = {
            .mKind = LevelOneRandomDecisionKind::ProjectileMotion,
            .mPlantColumn = theSlot,
        };
        return true;
    }

    if (!mRandomDecisionSource->ReadNext(
            LevelOneRandomDecisionKind::ProjectileMotion,
            theDecision) ||
        theDecision.mKind !=
            LevelOneRandomDecisionKind::ProjectileMotion ||
        theDecision.mNextCountdown != 0 ||
        theDecision.mXMilliPixels < kMinimumStateXMilliPixels ||
        theDecision.mXMilliPixels > kMaximumStateXMilliPixels ||
        theDecision.mGroundYMilliPixels != 0 ||
        theDecision.mSpeedMicroPixelsPerTick != 0 ||
        theDecision.mWaveHealthThreshold != 0 ||
        theDecision.mPlantColumn != theSlot ||
        theDecision.mShootingCounter != 0)
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
