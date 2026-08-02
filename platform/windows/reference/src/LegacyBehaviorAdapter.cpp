#include "pvz/platform/windows/LegacyBehaviorAdapter.h"

#include "Lawn/Board.h"
#include "Lawn/Coin.h"
#include "Lawn/CursorObject.h"
#include "Lawn/LawnMower.h"
#include "Lawn/Plant.h"
#include "Lawn/Projectile.h"
#include "Lawn/SeedPacket.h"
#include "Lawn/Zombie.h"
#include "Lawn/Widget/TitleScreen.h"
#include "LawnApp.h"
#include "pvz/platform/windows/LegacyInputCapture.h"
#include "widget/WidgetManager.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <limits>

namespace pvz::platform::windows
{
namespace
{

struct LevelOneDecisionObserverState
{
    Board* mBoard{};
    int mSunsFallen{};
    int mCurrentWave{};
    bool mTerminal{};
    std::array<unsigned int, 9> mPlantIds{};
    std::array<int, 9> mPlantLaunchCounters{};
    std::array<unsigned int, 32> mProjectileIds{};
    std::array<unsigned int, 8> mZombieIds{};
    std::array<int, 8> mZombieHealth{};
    std::array<unsigned int, game::kBehaviorSunSlotCount> mSunIds{};
};

[[nodiscard]] LevelOneDecisionObserverState&
GetLevelOneDecisionObserverState()
{
    static LevelOneDecisionObserverState aState;
    return aState;
}

void EnsureBehaviorCaptureEnvironment(LawnApp& theApp)
{
    const bool wasAppFocused = theApp.mHasFocus;
    theApp.mActive = true;
    theApp.mMinimized = false;
    theApp.mHasFocus = true;
    if (theApp.mWidgetManager != nullptr &&
        !theApp.mWidgetManager->mHasFocus)
    {
        theApp.mWidgetManager->GotFocus();
    }
    if (!wasAppFocused)
        theApp.GotFocus();
    if (theApp.mGameScene == GameScenes::SCENE_LEVEL_INTRO &&
        theApp.mBoard != nullptr &&
        theApp.mBoard->mDrawCount == 0)
    {
        theApp.mBoard->mDrawCount = 1;
    }
}

[[nodiscard]] game::BehaviorScene GetScene(const LawnApp& theApp)
{
    if (theApp.mTitleScreen != nullptr)
        return game::BehaviorScene::Title;
    if (theApp.mGameScene == GameScenes::SCENE_LOADING)
        return game::BehaviorScene::Loading;
    if (theApp.mGameSelector != nullptr ||
        theApp.mGameScene == GameScenes::SCENE_MENU)
    {
        return game::BehaviorScene::MainMenu;
    }
    if (theApp.mGameScene == GameScenes::SCENE_LEVEL_INTRO)
        return game::BehaviorScene::AdventureIntro;
    if (theApp.mGameScene == GameScenes::SCENE_PLAYING ||
        theApp.mGameScene == GameScenes::SCENE_ZOMBIES_WON)
        return game::BehaviorScene::AdventurePlaying;
    return game::BehaviorScene::Other;
}

[[nodiscard]] game::BehaviorBoardStage GetBoardStage(
    const Board* theBoard)
{
    if (theBoard == nullptr)
        return game::BehaviorBoardStage::None;
    switch (theBoard->mBackground)
    {
    case BackgroundType::BACKGROUND_1_DAY:
        return game::BehaviorBoardStage::Day;
    case BackgroundType::BACKGROUND_2_NIGHT:
        return game::BehaviorBoardStage::Night;
    case BackgroundType::BACKGROUND_3_POOL:
        return game::BehaviorBoardStage::Pool;
    case BackgroundType::BACKGROUND_4_FOG:
        return game::BehaviorBoardStage::Fog;
    case BackgroundType::BACKGROUND_5_ROOF:
        return game::BehaviorBoardStage::Roof;
    case BackgroundType::BACKGROUND_6_BOSS:
        return game::BehaviorBoardStage::Boss;
    case BackgroundType::BACKGROUND_MUSHROOM_GARDEN:
    case BackgroundType::BACKGROUND_GREENHOUSE:
    case BackgroundType::BACKGROUND_ZOMBIQUARIUM:
    case BackgroundType::BACKGROUND_TREEOFWISDOM:
        return game::BehaviorBoardStage::Other;
    }
    return game::BehaviorBoardStage::Other;
}

void ObserveGridFocus(
    const Board& theBoard,
    game::BehaviorObservation& theObservation)
{
    if (theBoard.mCursorPreview == nullptr)
        return;
    const int aColumn = theBoard.mCursorPreview->mGridX;
    const int aRow = theBoard.mCursorPreview->mGridY;
    if (aColumn < 0 ||
        aColumn >= static_cast<int>(
            game::kBehaviorBoardColumnCount) ||
        aRow < 0 ||
        aRow >= static_cast<int>(
            game::kBehaviorBoardRowCount))
    {
        return;
    }
    theObservation.mGridColumn =
        static_cast<std::uint8_t>(aColumn);
    theObservation.mGridRow =
        static_cast<std::uint8_t>(aRow);
}

void ObservePlants(
    Board& theBoard,
    game::BehaviorObservation& theObservation)
{
    Plant* aPlant = nullptr;
    while (theBoard.IteratePlants(aPlant))
    {
        if (!aPlant->mIsOnBoard ||
            aPlant->mPlantCol < 0 ||
            aPlant->mPlantCol >= static_cast<int>(
                game::kBehaviorBoardColumnCount) ||
            aPlant->mRow < 0 ||
            aPlant->mRow >= static_cast<int>(
                game::kBehaviorBoardRowCount))
        {
            continue;
        }
        const auto aColumn =
            static_cast<std::uint32_t>(aPlant->mPlantCol);
        const auto aRow =
            static_cast<std::uint32_t>(aPlant->mRow);
        const auto aBit = aRow *
            static_cast<std::uint32_t>(
                game::kBehaviorBoardColumnCount) +
            aColumn;
        theObservation.mOccupiedCells |=
            std::uint64_t{1} << aBit;
        ++theObservation.mPlantCount;
    }
}

[[nodiscard]] std::uint16_t NormalizeU16(int theValue)
{
    if (theValue <= 0)
        return 0;
    constexpr auto aMaximum =
        std::numeric_limits<std::uint16_t>::max();
    if (theValue >= static_cast<int>(aMaximum))
        return aMaximum;
    return static_cast<std::uint16_t>(theValue);
}

[[nodiscard]] std::int32_t NormalizeScaledFloat(
    float theValue,
    float theScale)
{
    const auto aScaled = std::round(theValue * theScale);
    constexpr auto aMinimum =
        static_cast<float>(
            std::numeric_limits<std::int32_t>::min());
    constexpr auto aMaximum =
        static_cast<float>(
            std::numeric_limits<std::int32_t>::max());
    if (aScaled <= aMinimum)
        return std::numeric_limits<std::int32_t>::min();
    if (aScaled >= aMaximum)
        return std::numeric_limits<std::int32_t>::max();
    return static_cast<std::int32_t>(aScaled);
}

void ObserveLevelOneRandomDecisions(Board& theBoard)
{
    auto& aState = GetLevelOneDecisionObserverState();
    if (aState.mBoard != &theBoard)
    {
        aState.mBoard = &theBoard;
        aState.mSunsFallen = theBoard.mNumSunsFallen;
        aState.mCurrentWave = theBoard.mCurrentWave;
        aState.mTerminal = false;
        aState.mPlantIds = {};
        aState.mPlantLaunchCounters = {};
        aState.mProjectileIds = {};
        aState.mZombieIds = {};
        aState.mZombieHealth = {};
        aState.mSunIds = {};
    }
    if (aState.mTerminal)
        return;

    std::array<unsigned int, 32> aCurrentProjectileIds{};
    std::array<Projectile*, 32> aNewProjectiles{};
    std::size_t aCurrentProjectileCount{};
    std::size_t aNewProjectileCount{};
    Projectile* aProjectile = nullptr;
    while (theBoard.IterateProjectiles(aProjectile))
    {
        if (aProjectile->mDead ||
            aProjectile->mProjectileType !=
                ProjectileType::PROJECTILE_PEA ||
            aProjectile->mRow != 2 ||
            aCurrentProjectileCount >=
                aCurrentProjectileIds.size())
        {
            continue;
        }
        const auto aProjectileId =
            theBoard.mProjectiles.DataArrayGetID(aProjectile);
        aCurrentProjectileIds[aCurrentProjectileCount] =
            aProjectileId;
        ++aCurrentProjectileCount;
        if (std::find(
                aState.mProjectileIds.begin(),
                aState.mProjectileIds.end(),
                aProjectileId) ==
                aState.mProjectileIds.end())
        {
            aNewProjectiles[aNewProjectileCount++] = aProjectile;
        }
    }

    std::array<bool, 9> aSeenPlantColumns{};
    std::size_t aNextNewProjectile{};
    Plant* aPlant = nullptr;
    while (theBoard.IteratePlants(aPlant))
    {
        if (!aPlant->mIsOnBoard ||
            aPlant->mSeedType != SeedType::SEED_PEASHOOTER ||
            aPlant->mRow != 2 ||
            aPlant->mPlantCol < 0 ||
            aPlant->mPlantCol >= 9)
        {
            continue;
        }
        const auto aColumn =
            static_cast<std::uint8_t>(aPlant->mPlantCol);
        const auto aColumnIndex =
            static_cast<std::size_t>(aColumn);
        const auto aPlantId =
            theBoard.mPlants.DataArrayGetID(aPlant);
        const bool isNewPlant =
            aState.mPlantIds[aColumnIndex] != aPlantId;
        const bool hasReset =
            !isNewPlant &&
            aPlant->mLaunchCounter >
                aState.mPlantLaunchCounters[aColumnIndex];
        if (!isNewPlant &&
            aPlant->mShootingCounter == 1 &&
            aNextNewProjectile < aNewProjectileCount)
        {
            const auto* aNewProjectile =
                aNewProjectiles[aNextNewProjectile++];
            RecordLegacyRandomDecision({
                .mKind = game::LevelOneRandomDecisionKind::
                    ProjectileSpawn,
                .mXMilliPixels = NormalizeScaledFloat(
                    aNewProjectile->mPosX - 3.33F,
                    1'000.0F),
                .mGroundYMilliPixels = NormalizeScaledFloat(
                    aNewProjectile->mPosY,
                    1'000.0F),
                .mPlantColumn = aColumn,
            });
        }
        if (isNewPlant)
        {
            if (aPlant->mShootingCounter == 33)
            {
                RecordLegacyRandomDecision({
                    .mKind = game::LevelOneRandomDecisionKind::
                        PeashooterSchedule,
                    .mNextCountdown = 1,
                    .mPlantColumn = aColumn,
                });
                RecordLegacyRandomDecision({
                    .mKind = game::LevelOneRandomDecisionKind::
                        PeashooterSchedule,
                    .mNextCountdown = NormalizeU16(
                        aPlant->mLaunchCounter),
                    .mPlantColumn = aColumn,
                    .mShootingCounter = 33,
                });
            }
            else
            {
                RecordLegacyRandomDecision({
                    .mKind = game::LevelOneRandomDecisionKind::
                        PeashooterSchedule,
                    .mNextCountdown = NormalizeU16(
                        aPlant->mLaunchCounter + 1),
                    .mPlantColumn = aColumn,
                });
            }
        }
        else if (hasReset)
        {
            RecordLegacyRandomDecision({
                .mKind = game::LevelOneRandomDecisionKind::
                    PeashooterSchedule,
                .mNextCountdown = NormalizeU16(
                    aPlant->mLaunchCounter),
                .mPlantColumn = aColumn,
                .mShootingCounter = static_cast<std::uint8_t>(
                    std::clamp(aPlant->mShootingCounter, 0, 255)),
            });
        }
        aSeenPlantColumns[aColumnIndex] = true;
        aState.mPlantIds[aColumnIndex] = aPlantId;
        aState.mPlantLaunchCounters[aColumnIndex] =
            aPlant->mLaunchCounter;
    }
    for (std::size_t aColumn = 0;
         aColumn < aSeenPlantColumns.size();
         ++aColumn)
    {
        if (!aSeenPlantColumns[aColumn])
        {
            aState.mPlantIds[aColumn] = 0;
            aState.mPlantLaunchCounters[aColumn] = 0;
        }
    }
    for (std::size_t aZombieSlot = 0;
         aZombieSlot < aState.mZombieIds.size();
         ++aZombieSlot)
    {
        const auto aZombieId = aState.mZombieIds[aZombieSlot];
        if (aZombieId == 0)
            continue;
        auto* aTrackedZombie =
            theBoard.mZombies.DataArrayTryToGet(aZombieId);
        if (aTrackedZombie == nullptr)
        {
            aState.mZombieIds[aZombieSlot] = 0;
            aState.mZombieHealth[aZombieSlot] = 0;
            continue;
        }
        const auto aPreviousHealth =
            aState.mZombieHealth[aZombieSlot];
        const auto aCurrentHealth =
            std::max(aTrackedZombie->mBodyHealth, 0);
        const auto aHealthDelta =
            std::max(aPreviousHealth - aCurrentHealth, 0);
        RecordLegacyRandomDecision({
            .mKind = game::LevelOneRandomDecisionKind::ZombieMotion,
            // Legacy collision and drawing read the synchronized integer mX,
            // not the animation-produced float mPosX.  Preserve that exact
            // truncation boundary so rounding cannot move a zombie by a
            // collision pixel.
            .mXMilliPixels =
                static_cast<std::int32_t>(aTrackedZombie->mX) *
                1'000,
            .mPlantColumn = static_cast<std::uint8_t>(aZombieSlot),
            // Peas account for 20-point chunks.  A remaining one-point
            // decrement is the post-choice result of the headless-zombie
            // Rand(5) decay in Zombie::Update.
            .mShootingCounter = static_cast<std::uint8_t>(
                aHealthDelta % 20 == 1 ? 1 : 0),
        });
        aState.mZombieHealth[aZombieSlot] = aCurrentHealth;
        if (aTrackedZombie->IsDeadOrDying())
        {
            aState.mZombieIds[aZombieSlot] = 0;
            aState.mZombieHealth[aZombieSlot] = 0;
        }
    }

    // Allocate new IDs before emitting motion, then emit every live/dead-this-
    // tick projectile in stable slot order. Portable combat updates its fixed
    // projectile array in that same order. Emitting existing IDs first would
    // invert the tape whenever a newly fired pea reused a lower free slot.
    for (std::size_t anIndex = 0;
         anIndex < aCurrentProjectileCount;
         ++anIndex)
    {
        const auto aProjectileId = aCurrentProjectileIds[anIndex];
        if (std::find(
                aState.mProjectileIds.begin(),
                aState.mProjectileIds.end(),
                aProjectileId) != aState.mProjectileIds.end())
        {
            continue;
        }
        const auto aFreeSlot = std::find(
            aState.mProjectileIds.begin(),
            aState.mProjectileIds.end(),
            0U);
        if (aFreeSlot == aState.mProjectileIds.end())
            continue;
        *aFreeSlot = aProjectileId;
    }

    std::array<bool, 32> aProjectileSlotsToClear{};
    for (std::size_t aProjectileSlot = 0;
         aProjectileSlot < aState.mProjectileIds.size();
         ++aProjectileSlot)
    {
        const auto aProjectileId =
            aState.mProjectileIds[aProjectileSlot];
        if (aProjectileId == 0)
            continue;
        auto* aTrackedProjectile =
            theBoard.mProjectiles.DataArrayTryToGet(aProjectileId);
        if (aTrackedProjectile == nullptr)
        {
            aState.mProjectileIds[aProjectileSlot] = 0;
            continue;
        }
        RecordLegacyRandomDecision({
            .mKind = game::LevelOneRandomDecisionKind::ProjectileMotion,
            .mXMilliPixels =
                static_cast<std::int32_t>(aTrackedProjectile->mX) *
                1'000,
            .mPlantColumn =
                static_cast<std::uint8_t>(aProjectileSlot),
        });
        if (aTrackedProjectile->mDead)
            aProjectileSlotsToClear[aProjectileSlot] = true;
    }
    for (std::size_t aProjectileSlot = 0;
         aProjectileSlot < aProjectileSlotsToClear.size();
         ++aProjectileSlot)
    {
        if (aProjectileSlotsToClear[aProjectileSlot])
            aState.mProjectileIds[aProjectileSlot] = 0;
    }

    if (theBoard.mNumSunsFallen > aState.mSunsFallen)
    {
        Coin* aNewestSun = nullptr;
        Coin* aCoin = nullptr;
        while (theBoard.IterateCoins(aCoin))
        {
            if (!aCoin->mDead &&
                aCoin->mCoinAge == 0 &&
                aCoin->mType == CoinType::COIN_SUN &&
                aCoin->mCoinMotion ==
                    CoinMotion::COIN_MOTION_FROM_SKY)
            {
                aNewestSun = aCoin;
                break;
            }
        }
        game::LevelOneRandomDecision aDecision;
        aDecision.mKind =
            game::LevelOneRandomDecisionKind::FallingSun;
        if (aNewestSun != nullptr)
        {
            aDecision.mNextCountdown =
                NormalizeU16(theBoard.mSunCountDown);
            aDecision.mXMilliPixels =
                NormalizeScaledFloat(aNewestSun->mPosX, 1'000.0F);
            aDecision.mGroundYMilliPixels =
                static_cast<std::int32_t>(
                    aNewestSun->mGroundY) *
                1'000;
        }
        RecordLegacyRandomDecision(aDecision);
    }
    aState.mSunsFallen = theBoard.mNumSunsFallen;

    Zombie* aZombie = nullptr;
    while (theBoard.IterateZombies(aZombie))
    {
        if (aZombie->mDead ||
            aZombie->mZombieAge != 0 ||
            aZombie->mZombieType != ZombieType::ZOMBIE_NORMAL ||
            aZombie->mFromWave < 0)
        {
            continue;
        }
        game::LevelOneRandomDecision aDecision;
        aDecision.mKind =
            game::LevelOneRandomDecisionKind::NormalZombie;
        aDecision.mXMilliPixels =
            NormalizeScaledFloat(aZombie->mPosX, 1'000.0F);
        const auto aSpeed =
            NormalizeScaledFloat(aZombie->mVelX, 1'000'000.0F);
        if (aSpeed > 0)
        {
            aDecision.mSpeedMicroPixelsPerTick =
                static_cast<std::uint32_t>(aSpeed);
        }
        RecordLegacyRandomDecision(aDecision);

        const auto aZombieId =
            theBoard.mZombies.DataArrayGetID(aZombie);
        if (std::find(
                aState.mZombieIds.begin(),
                aState.mZombieIds.end(),
                aZombieId) == aState.mZombieIds.end())
        {
            const auto aFreeSlot = std::find(
                aState.mZombieIds.begin(),
                aState.mZombieIds.end(),
                0U);
            if (aFreeSlot != aState.mZombieIds.end())
            {
                *aFreeSlot = aZombieId;
                const auto aSlot = static_cast<std::size_t>(
                    std::distance(
                        aState.mZombieIds.begin(),
                        aFreeSlot));
                aState.mZombieHealth[aSlot] =
                    std::max(aZombie->mBodyHealth, 0);
            }
        }
    }
    if (theBoard.mCurrentWave > aState.mCurrentWave)
    {
        game::LevelOneRandomDecision aDecision;
        aDecision.mKind =
            game::LevelOneRandomDecisionKind::WaveSchedule;
        aDecision.mNextCountdown =
            NormalizeU16(theBoard.mZombieCountDown);
        aDecision.mWaveHealthThreshold =
            NormalizeU16(theBoard.mZombieHealthToNextWave);
        RecordLegacyRandomDecision(aDecision);
    }
    aState.mCurrentWave = theBoard.mCurrentWave;
    aState.mTerminal =
        theBoard.mLevelAwardSpawned ||
        theBoard.mApp->mBoardResult ==
            BoardResult::BOARDRESULT_LOST ||
        theBoard.mApp->mGameScene ==
            GameScenes::SCENE_ZOMBIES_WON;
}

[[nodiscard]] game::BehaviorTutorialPhase GetTutorialPhase(
    TutorialState theState)
{
    switch (theState)
    {
    case TutorialState::TUTORIAL_OFF:
        return game::BehaviorTutorialPhase::None;
    case TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER:
        return game::BehaviorTutorialPhase::LevelOnePickUpPeashooter;
    case TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER:
        return game::BehaviorTutorialPhase::LevelOnePlantPeashooter;
    case TutorialState::TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER:
        return game::BehaviorTutorialPhase::LevelOneRefreshPeashooter;
    case TutorialState::TUTORIAL_LEVEL_1_COMPLETED:
        return game::BehaviorTutorialPhase::LevelOneCompleted;
    default:
        return game::BehaviorTutorialPhase::Other;
    }
}

void ObserveLevelOneState(
    Board& theBoard,
    game::BehaviorObservation& theObservation)
{
    theObservation.mSun = NormalizeU16(theBoard.mSunMoney);
    theObservation.mTutorialPhase =
        GetTutorialPhase(theBoard.mTutorialState);

    if (theBoard.mSeedBank != nullptr &&
        theBoard.mSeedBank->mNumPackets > 0)
    {
        const auto& aPacket =
            theBoard.mSeedBank->mSeedPackets[0];
        if (aPacket.mRefreshing)
        {
            theObservation.mSeedRefreshing = true;
            theObservation.mSeedRefreshCounter =
                NormalizeU16(aPacket.mRefreshCounter);
            theObservation.mSeedRefreshTime =
                NormalizeU16(aPacket.mRefreshTime);
        }
        if (theBoard.mCursorObject != nullptr &&
            theBoard.mCursorObject->mSeedBankIndex >= 0)
        {
            theObservation.mSeedSelection =
                aPacket.mPacketType ==
                        SeedType::SEED_PEASHOOTER
                ? game::BehaviorSeedSelection::Peashooter
                : game::BehaviorSeedSelection::Other;
        }
    }

    theObservation.mFirstSunSpawned =
        theBoard.mNumSunsFallen > 0;
    if (theObservation.mPlantCount > 0 &&
        !theObservation.mFirstSunSpawned)
    {
        theObservation.mFirstSunCountdown =
            NormalizeU16(theBoard.mSunCountDown);
    }

    auto& aDecisionState = GetLevelOneDecisionObserverState();
    for (auto& aSunId : aDecisionState.mSunIds)
    {
        if (aSunId == 0)
            continue;
        const auto* aSun = theBoard.mCoins.DataArrayTryToGet(aSunId);
        if (aSun == nullptr ||
            aSun->mDead ||
            aSun->mType != CoinType::COIN_SUN ||
            aSun->mCoinMotion != CoinMotion::COIN_MOTION_FROM_SKY)
        {
            aSunId = 0;
        }
    }
    Coin* aSun = nullptr;
    while (theBoard.IterateCoins(aSun))
    {
        if (aSun->mDead ||
            aSun->mType != CoinType::COIN_SUN ||
            aSun->mCoinMotion != CoinMotion::COIN_MOTION_FROM_SKY)
        {
            continue;
        }
        const auto aSunId = theBoard.mCoins.DataArrayGetID(aSun);
        auto aSlot = std::find(
            aDecisionState.mSunIds.begin(),
            aDecisionState.mSunIds.end(),
            aSunId);
        if (aSlot == aDecisionState.mSunIds.end())
        {
            aSlot = std::find(
                aDecisionState.mSunIds.begin(),
                aDecisionState.mSunIds.end(),
                0U);
            if (aSlot == aDecisionState.mSunIds.end())
                continue;
            *aSlot = aSunId;
        }
        const auto aSlotIndex = static_cast<std::size_t>(
            std::distance(aDecisionState.mSunIds.begin(), aSlot));
        theObservation.mSuns[aSlotIndex] = {
            .mActive = true,
            .mBeingCollected = aSun->mIsBeingCollected,
            .mXMilliPixels = NormalizeScaledFloat(
                aSun->mPosX,
                1'000.0F),
            .mYMilliPixels = NormalizeScaledFloat(
                aSun->mPosY,
                1'000.0F),
            .mGroundYMilliPixels =
                static_cast<std::int32_t>(aSun->mGroundY) * 1'000,
            .mAge = NormalizeU16(aSun->mCoinAge),
        };
    }

    theObservation.mCurrentWave =
        static_cast<std::uint8_t>(std::clamp(
            theBoard.mCurrentWave,
            0,
            255));
    if (theBoard.mTutorialState ==
        TutorialState::TUTORIAL_LEVEL_1_COMPLETED)
    {
        theObservation.mZombieCountdown =
            NormalizeU16(theBoard.mZombieCountDown);
    }
    Zombie* aZombie = nullptr;
    while (theBoard.IterateZombies(aZombie))
    {
        if (!aZombie->IsDeadOrDying() &&
            theObservation.mZombieCount <
                std::numeric_limits<std::uint8_t>::max())
        {
            ++theObservation.mZombieCount;
        }
    }
    if (theBoard.mLevelAwardSpawned)
    {
        theObservation.mLevelOutcome =
            game::BehaviorLevelOutcome::Won;
    }
    else if (theBoard.mApp->mBoardResult ==
                 BoardResult::BOARDRESULT_LOST ||
             theBoard.mApp->mGameScene ==
                 GameScenes::SCENE_ZOMBIES_WON)
    {
        theObservation.mLevelOutcome =
            game::BehaviorLevelOutcome::Lost;
    }
    else
    {
        theObservation.mLevelOutcome =
            game::BehaviorLevelOutcome::Playing;
    }
    theObservation.mLevelAwardSpawned =
        theBoard.mLevelAwardSpawned;

    if (theBoard.mCurrentWave > 0)
    {
        theObservation.mZombieWaveHealth = NormalizeU16(
            theBoard.TotalZombiesHealthInWave(
                theBoard.mCurrentWave - 1));
    }
    Projectile* aProjectile = nullptr;
    while (theObservation.mLevelOutcome ==
               game::BehaviorLevelOutcome::Playing &&
           theBoard.IterateProjectiles(aProjectile))
    {
        if (!aProjectile->mDead &&
            theObservation.mProjectileCount <
                std::numeric_limits<std::uint8_t>::max())
        {
            ++theObservation.mProjectileCount;
        }
    }

    LawnMower* aMower = theBoard.FindLawnMowerInRow(2);
    if (aMower == nullptr)
    {
        theObservation.mMowerState =
            game::BehaviorMowerState::Spent;
    }
    else if (aMower->mMowerState ==
             LawnMowerState::MOWER_TRIGGERED)
    {
        theObservation.mMowerState =
            game::BehaviorMowerState::Triggered;
    }
    else
    {
        theObservation.mMowerState =
            game::BehaviorMowerState::Ready;
    }
}

} // namespace

void CaptureLegacyBehaviorTick(LawnApp& theApp)
{
    if (!WasLegacyBehaviorCaptureRequested() ||
        !IsLegacyInputCaptureEnabled())
    {
        return;
    }
    EnsureBehaviorCaptureEnvironment(theApp);
    if (!HasLegacyBehaviorCaptureStarted())
    {
        // The application-level flag remains set after loading completes,
        // even if a click removes the title widget during this update.  Do
        // not arm during resource loading: recording every loading update can
        // perturb the reconstructed loader, while the former title-widget
        // flag could be missed in the same update that entered the menu.
        if (theApp.mLoadingThreadCompleted)
        {
            StartLegacyBehaviorCapture();
            std::cerr << "Reference behavior capture start gate reached\n";
        }
        return;
    }
    const auto aFrameCount =
        GetLegacyInputCaptureFrameCount();
    if (aFrameCount == 0)
        return;

    game::BehaviorObservation anObservation;
    anObservation.mTick = aFrameCount - 1U;
    anObservation.mScene = GetScene(theApp);
    anObservation.mBoardStage =
        GetBoardStage(theApp.mBoard);
    if (theApp.mBoard != nullptr)
    {
        if (anObservation.mScene ==
            game::BehaviorScene::AdventurePlaying)
        {
            ObserveGridFocus(*theApp.mBoard, anObservation);
        }
        ObservePlants(*theApp.mBoard, anObservation);
        if (anObservation.mScene ==
                game::BehaviorScene::AdventurePlaying &&
            theApp.mBoard->mLevel == 1)
        {
            ObserveLevelOneRandomDecisions(*theApp.mBoard);
            ObserveLevelOneState(
                *theApp.mBoard,
                anObservation);
        }
    }
    RecordLegacyBehaviorObservation(anObservation);
}

} // namespace pvz::platform::windows
