#include "fixtures/LegacyLevelOneCombatFixture.h"

#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/game/LevelOneCombat.h"
#include "pvz/game/LevelOneRandomDecision.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

extern void Expect(bool theCondition, const char* theMessage);

namespace
{

namespace legacy =
    pvz::game::test::legacy_combat_reference;

[[nodiscard]] std::uint16_t WaitForSunCredit(
    pvz::game::LevelOneCombat& theCombat,
    std::uint16_t theMaximumTicks = 256)
{
    for (std::uint16_t aTick = 1;
         aTick <= theMaximumTicks;
         ++aTick)
    {
        theCombat.Update();
        if (theCombat.ConsumeCollectedSun() ==
            pvz::game::LevelOneCombat::kSunValue)
        {
            return aTick;
        }
    }
    return 0;
}

[[nodiscard]] bool CollectFirstAvailableSun(
    pvz::game::LevelOneCombat& theCombat)
{
    for (const auto& aSun : theCombat.GetState().mSuns)
    {
        if (!aSun.mActive || aSun.mBeingCollected)
            continue;
        return theCombat.TryCollectSun(
            {
                aSun.mXMilliPixels / 1'000 + 30,
                aSun.mYMilliPixels / 1'000 + 30,
            });
    }
    return false;
}

[[nodiscard]] bool WaitForSunCount(
    pvz::game::LevelOneCombat& theCombat,
    std::uint8_t theExpectedCount,
    std::uint16_t theMaximumTicks = 2'000)
{
    for (std::uint16_t aTick = 0;
         aTick < theMaximumTicks;
         ++aTick)
    {
        if (theCombat.GetState().mSunCount >=
            theExpectedCount)
        {
            return true;
        }
        theCombat.Update();
    }
    return theCombat.GetState().mSunCount >=
           theExpectedCount;
}

void TestSourceAuditedCombatConstants()
{
    using Combat = pvz::game::LevelOneCombat;
    Expect(
        Combat::kTutorialSunCountdown ==
                legacy::kTutorialSunCountdown &&
            Combat::kFirstWaveCountdown ==
                legacy::kFirstWaveCountdown,
        "tutorial completion uses the legacy first-wave gate");
    Expect(
        Combat::kSunValue == legacy::kSunValue &&
            Combat::kDeterministicNextSunCountdown ==
                legacy::kNextSunCountdownMinimum &&
            Combat::kSunFallSpeedMilliPixelsPerTick ==
                legacy::kSunFallSpeedMilliPixelsPerTick &&
            Combat::kPlantHealth == legacy::kPlantHealth &&
            Combat::kNormalZombieHealth ==
                legacy::kNormalZombieHealth &&
            Combat::kNormalZombieHeadLossHealth ==
                legacy::kNormalZombieHeadLossHealth,
        "Level 1 sun and entity health match the legacy source");
    Expect(
        Combat::kDeterministicSunSpawnXMilliPixels >=
                legacy::kSunSpawnMinimumXMilliPixels &&
            Combat::kDeterministicSunSpawnXMilliPixels <=
                legacy::kSunSpawnMaximumXMilliPixels &&
            Combat::kDeterministicSunSpawnYMilliPixels ==
                legacy::kSunSpawnYMilliPixels &&
            Combat::kDeterministicSunGroundYMilliPixels >=
                legacy::kSunGroundMinimumYMilliPixels &&
            Combat::kDeterministicSunGroundYMilliPixels <=
                legacy::kSunGroundMaximumYMilliPixels &&
            Combat::kDeterministicSunLifetime ==
                legacy::kPortableSunLifetime,
        "deterministic sun fixture stays inside legacy source ranges");
    Expect(
        Combat::kPeaDamage == legacy::kPeaDamage &&
            Combat::kPeashooterLaunchRate ==
                legacy::kPeashooterLaunchRate &&
            Combat::kPeashooterFireDelay ==
                legacy::kPeashooterFireDelay &&
            Combat::kPeaSpeedMilliPixelsPerTick ==
                legacy::kPeaSpeedMilliPixelsPerTick,
        "Peashooter cadence and projectile values match legacy");
    Expect(
        Combat::kEatInterval == legacy::kEatInterval &&
            Combat::kEatDamage == legacy::kEatDamage,
        "normal zombie eating cadence matches legacy");
    Expect(
        Combat::kNormalZombieAttackRectX ==
                legacy::kNormalZombieAttackRectX &&
            Combat::kNormalZombieAttackRectWidth ==
                legacy::kNormalZombieAttackRectWidth &&
            Combat::kNormalZombieRectX ==
                legacy::kNormalZombieRectX &&
            Combat::kNormalZombieRectWidth ==
                legacy::kNormalZombieRectWidth,
        "normal zombie collision rectangles record LoadPlainZombieReanim");
    Expect(
        Combat::kDeterministicZombieSpawnXMilliPixels >=
                legacy::kZombieSpawnMinimumXMilliPixels &&
            Combat::kDeterministicZombieSpawnXMilliPixels <=
                legacy::kZombieSpawnMaximumXMilliPixels &&
            Combat::
                    kDeterministicZombieSpeedMilliPixelsPerTick >=
                legacy::
                    kZombieMinimumSpeedMilliPixelsPerTick &&
            Combat::
                    kDeterministicZombieSpeedMilliPixelsPerTick <=
                legacy::
                    kZombieMaximumSpeedMilliPixelsPerTick,
        "deterministic zombie fixture stays inside legacy RNG ranges");
    Expect(
        legacy::kNormalZombiesPerWave[0] == 1 &&
            legacy::kNormalZombiesPerWave[1] == 1 &&
            legacy::kNormalZombiesPerWave[2] == 1 &&
            legacy::kNormalZombiesPerWave[3] == 2,
        "source fixture records the complete Level 1 wave composition");
    Expect(
        Combat::kWaveCount == legacy::kLevelOneWaveCount &&
            Combat::kNextWaveCountdownMinimum ==
                legacy::kNextWaveCountdownMinimum &&
            Combat::kNextWaveCountdownMaximum ==
                legacy::kNextWaveCountdownMaximum &&
            Combat::kWaveAccelerationCountdown ==
                legacy::kWaveAccelerationCountdown &&
            Combat::kWaveAccelerationMinimumAge ==
                legacy::kWaveAccelerationMinimumAge,
        "complete wave scheduler uses the audited legacy boundaries");
    Expect(
        Combat::kZombieLossXMilliPixels ==
                legacy::kZombieLossXMilliPixels &&
            Combat::kMowerReadyXMilliPixels ==
                legacy::kMowerReadyXMilliPixels &&
            Combat::kMowerSpeedMilliPixelsPerTick ==
                legacy::kMowerSpeedMilliPixelsPerTick &&
            Combat::kMowerMaximumXMilliPixels ==
                legacy::kMowerSpentXMilliPixels,
        "mower and loss boundaries use the audited legacy coordinates");
}

void TestTutorialSunAndFirstWaveGate()
{
    pvz::game::LevelOneCombat aCombat;
    Expect(
        aCombat.AddPeashooter(2, 2) &&
            aCombat.GetState().mPhase ==
                pvz::game::LevelOneCombatPhase::
                    AwaitingSecondPlant,
        "first Peashooter starts the Level 1 sun tutorial");

    for (std::uint16_t aTick = 0;
         aTick <
             pvz::game::LevelOneCombat::
                 kTutorialSunCountdown -
                 1;
         ++aTick)
    {
        aCombat.Update();
    }
    Expect(
        aCombat.GetState().mSunCount == 0 &&
            aCombat.GetState().mSunCountdown == 1,
        "tutorial sun does not spawn before the source boundary");
    aCombat.Update();
    Expect(
        aCombat.GetState().mSunCount == 1 &&
            !aCombat.TryCollectSun({0, 0}) &&
            aCombat.TryCollectSun({405, 90}) &&
            aCombat.ConsumeCollectedSun() == 0,
        "falling tutorial sun begins the delayed collection flight");
    const auto aFirstCollectionTicks =
        WaitForSunCredit(aCombat);
    Expect(
        aFirstCollectionTicks > 1 &&
            aCombat.GetState().mSunCount == 0,
        "sun value is credited only after reaching the legacy counter");

    Expect(
        WaitForSunCount(aCombat, 1) &&
            CollectFirstAvailableSun(aCombat) &&
            WaitForSunCredit(aCombat) > 1,
        "second deterministic sky sun makes the second plant affordable");

    Expect(
        WaitForSunCount(aCombat, 2) &&
            aCombat.GetState().mSunCount == 2,
        "sky-sun countdown continues while an earlier sun remains active");
    Expect(
        CollectFirstAvailableSun(aCombat) &&
            WaitForSunCredit(aCombat) > 1 &&
            aCombat.GetState().mSunCount == 1,
        "overlapping suns remain independently collectible");
    Expect(
        CollectFirstAvailableSun(aCombat) &&
            WaitForSunCredit(aCombat) > 1 &&
            aCombat.GetState().mSunCount == 0,
        "newer overlapping sun remains after the older one is collected");

    Expect(
        aCombat.AddPeashooter(3, 2) &&
            aCombat.GetState().mPhase ==
                pvz::game::LevelOneCombatPhase::Active &&
            aCombat.GetState().mZombieCountdown ==
                pvz::game::LevelOneCombat::
                    kFirstWaveCountdown,
        "second Peashooter arms the 99-tick first wave");
    for (std::uint16_t aTick = 0; aTick < 98; ++aTick)
        aCombat.Update();
    Expect(
        aCombat.GetState().mZombieCount == 0 &&
            aCombat.GetState().mZombieCountdown == 1,
        "first zombie remains gated until countdown reaches zero");
    aCombat.Update();
    const auto aSpawnedState = aCombat.GetState();
    Expect(
        aSpawnedState.mZombieCount == 1 &&
            aSpawnedState.mFirstWaveSpawned &&
            aSpawnedState.mZombies[0].mHealth ==
                pvz::game::LevelOneCombat::
                    kNormalZombieHealth &&
            aSpawnedState.mZombies[0].mXMilliPixels ==
                legacy::kPortableSpawnXMilliPixels &&
            aSpawnedState.mZombies[0]
                    .mSpeedMicroPixelsPerTick ==
                static_cast<std::uint32_t>(
                    legacy::kPortableSpeedMilliPixelsPerTick) *
                    1'000U,
        "first wave spawns one fixed-width normal zombie");
}

void TestSunHitTestPreservesFractionalPosition()
{
    pvz::game::LevelOneCombat aCombat;
    Expect(
        aCombat.AddPeashooter(2, 2),
        "fractional sun hit-test fixture adds its first plant");

    auto aState = aCombat.GetState();
    aState.mSunsSpawned = 1;
    aState.mSunCount = 1;
    aState.mSuns[0] = {
        .mActive = true,
        .mBeingCollected = false,
        .mXMilliPixels = 608'000,
        .mYMilliPixels = 95'510,
        .mGroundYMilliPixels = 436'000,
        .mAge = 53,
    };
    Expect(
        aCombat.RestoreState(aState),
        "fractional sun hit-test fixture restores");
    Expect(
        !aCombat.TryCollectSun({660, 171}),
        "sun hit test keeps the native exclusive upper edge");
    Expect(
        aCombat.TryCollectSun({660, 170}),
        "sun hit test uses the native fractional position at the edge");
}

void TestCombatCompletesAllFourWavesDeterministically()
{
    pvz::game::LevelOneCombat aCombat;
    static_cast<void>(aCombat.AddPeashooter(2, 2));
    static_cast<void>(aCombat.AddPeashooter(3, 2));

    std::uint32_t aTicks{};
    while (aCombat.GetState().mPhase ==
               pvz::game::LevelOneCombatPhase::Active &&
           aTicks < 20'000)
    {
        aCombat.Update();
        ++aTicks;
    }
    const auto aState = aCombat.GetState();
    if (aTicks != 4'995)
        std::cerr << "complete-level tick mismatch: " << aTicks << '\n';
    Expect(
        aState.mPhase ==
                pvz::game::LevelOneCombatPhase::Won &&
            aState.mZombieCount == 0 &&
            aState.mFirstWaveCleared &&
            aState.mCurrentWave ==
                pvz::game::LevelOneCombat::kWaveCount &&
            aState.mAwardSpawned &&
            aState.mPlantCount >= 1 &&
            aState.mProjectileCount <= 2 &&
            aTicks == 4'995,
        "two Peashooters deterministically clear all four waves");
}

void TestSemanticRandomDecisionTape()
{
    constexpr std::array<pvz::game::LevelOneRandomDecision, 3>
        kDecisions{
            pvz::game::LevelOneRandomDecision{
                .mKind =
                    pvz::game::LevelOneRandomDecisionKind::FallingSun,
                .mNextCountdown = 512,
                .mXMilliPixels = 201'000,
                .mGroundYMilliPixels = 350'000,
                .mSpeedMicroPixelsPerTick = 0,
            },
            pvz::game::LevelOneRandomDecision{
                .mKind =
                    pvz::game::LevelOneRandomDecisionKind::FallingSun,
                .mNextCountdown = 600,
                .mXMilliPixels = 444'000,
                .mGroundYMilliPixels = 525'000,
                .mSpeedMicroPixelsPerTick = 0,
            },
            pvz::game::LevelOneRandomDecision{
                .mKind =
                    pvz::game::LevelOneRandomDecisionKind::NormalZombie,
                .mNextCountdown = 0,
                .mXMilliPixels = 817'000,
                .mGroundYMilliPixels = 0,
                .mSpeedMicroPixelsPerTick = 313'500,
            },
        };
    pvz::game::LevelOneRandomDecisionTape aTape(kDecisions);
    pvz::game::LevelOneCombat aCombat;
    aCombat.SetRandomDecisionSource(&aTape);
    static_cast<void>(aCombat.AddPeashooter(2, 2));
    for (std::uint16_t aTick = 0; aTick < 400; ++aTick)
        aCombat.Update();
    Expect(
        aCombat.GetState().mSuns[0].mXMilliPixels == 201'000 &&
            aCombat.GetState().mSuns[0].mGroundYMilliPixels ==
                350'000 &&
            aCombat.GetState().mSunCountdown == 512,
        "semantic tape supplies the first falling-sun choices");
    for (std::uint16_t aTick = 0; aTick < 512; ++aTick)
        aCombat.Update();
    Expect(
        aCombat.GetState().mSuns[1].mXMilliPixels == 444'000 &&
            aCombat.GetState().mSuns[1].mGroundYMilliPixels ==
                525'000 &&
            aCombat.GetState().mSunCountdown == 600,
        "semantic tape remains ordered across repeated sun events");

    static_cast<void>(aCombat.AddPeashooter(3, 2));
    for (std::uint16_t aTick = 0; aTick < 99; ++aTick)
        aCombat.Update();
    Expect(
        aCombat.GetState().mZombies[0].mXMilliPixels == 817'000 &&
            aCombat.GetState().mZombies[0]
                    .mSpeedMicroPixelsPerTick == 313'500 &&
            aTape.GetReadCount() == 3 &&
            aTape.GetRemainingCount() == 0 &&
            aTape.GetError() ==
                pvz::game::LevelOneRandomDecisionReadError::None &&
            !aCombat.HasRandomDecisionFailure(),
        "semantic tape supplies and exhausts the normal-zombie choice");
    aCombat.Update();
    Expect(
        aCombat.GetState().mZombies[0].mXMilliPixels == 816'687 &&
            aCombat.GetState().mZombies[0]
                    .mMovementRemainderMicroPixels == 500,
        "micro-pixel zombie speed retains its first fractional remainder");
    aCombat.Update();
    Expect(
        aCombat.GetState().mZombies[0].mXMilliPixels == 816'373 &&
            aCombat.GetState().mZombies[0]
                    .mMovementRemainderMicroPixels == 0,
        "micro-pixel zombie speed carries without cumulative drift");

    constexpr std::array<pvz::game::LevelOneRandomDecision, 1>
        kWrongKind{
            pvz::game::LevelOneRandomDecision{
                .mKind =
                    pvz::game::LevelOneRandomDecisionKind::NormalZombie,
                .mXMilliPixels = 780'000,
                .mSpeedMicroPixelsPerTick = 270'000,
            },
        };
    pvz::game::LevelOneRandomDecisionTape aWrongTape(kWrongKind);
    pvz::game::LevelOneCombat aRejectedCombat;
    aRejectedCombat.SetRandomDecisionSource(&aWrongTape);
    static_cast<void>(aRejectedCombat.AddPeashooter(2, 2));
    for (std::uint16_t aTick = 0; aTick < 400; ++aTick)
        aRejectedCombat.Update();
    Expect(
        aRejectedCombat.HasRandomDecisionFailure() &&
            aRejectedCombat.GetState().mSunCount == 0 &&
            aWrongTape.GetError() ==
                pvz::game::LevelOneRandomDecisionReadError::
                    KindMismatch,
        "semantic tape rejects kind drift instead of consuming it");
}

void TestWaveScheduleDecisionAndAcceleration()
{
    constexpr std::array<pvz::game::LevelOneRandomDecision, 2>
        kDecisions{
            pvz::game::LevelOneRandomDecision{
                .mKind = pvz::game::LevelOneRandomDecisionKind::NormalZombie,
                .mXMilliPixels = 800'000,
                .mSpeedMicroPixelsPerTick = 275'000,
            },
            pvz::game::LevelOneRandomDecision{
                .mKind = pvz::game::LevelOneRandomDecisionKind::WaveSchedule,
                .mNextCountdown = 2'777,
                .mWaveHealthThreshold = 150,
            },
        };
    pvz::game::LevelOneRandomDecisionTape aTape(
        kDecisions,
        true);
    pvz::game::LevelOneCombat aCombat;
    aCombat.SetRandomDecisionSource(&aTape);
    auto aState = aCombat.GetState();
    aState.mPhase = pvz::game::LevelOneCombatPhase::Active;
    aState.mSunCountdown = 500;
    aState.mZombieCountdown = 1;
    aState.mZombieCountdownStart =
        pvz::game::LevelOneCombat::kFirstWaveCountdown;
    Expect(
        aCombat.RestoreState(aState),
        "pre-wave schedule fixture restores");
    aCombat.Update();
    const auto aSpawned = aCombat.GetState();
    Expect(
        aSpawned.mCurrentWave == 1 &&
            aSpawned.mZombieCount == 1 &&
            aSpawned.mZombieCountdown == 2'777 &&
            aSpawned.mZombieCountdownStart == 2'777 &&
            aSpawned.mZombieHealthWaveStart == 270 &&
            aSpawned.mZombieHealthToNextWave == 150 &&
            aTape.GetReadCount() == 2 &&
            !aCombat.HasRandomDecisionFailure(),
        "semantic schedule captures countdown and health threshold");

    auto anAcceleratedState = aSpawned;
    anAcceleratedState.mZombieCountdown = 2'376;
    anAcceleratedState.mZombies[0].mHealth = 150;
    Expect(
        aCombat.RestoreState(anAcceleratedState),
        "wave-acceleration fixture restores");
    aCombat.Update();
    Expect(
        aCombat.GetState().mZombieCountdown ==
            pvz::game::LevelOneCombat::kWaveAccelerationCountdown,
        "damaged wave accelerates at the legacy age and health boundary");
}

void TestPeashooterScheduleDecision()
{
    constexpr std::array<pvz::game::LevelOneRandomDecision, 2>
        kDecisions{
            pvz::game::LevelOneRandomDecision{
                .mKind = pvz::game::LevelOneRandomDecisionKind::
                    PeashooterSchedule,
                .mNextCountdown = 2,
                .mPlantColumn = 2,
            },
            pvz::game::LevelOneRandomDecision{
                .mKind = pvz::game::LevelOneRandomDecisionKind::
                    PeashooterSchedule,
                .mNextCountdown = 140,
                .mPlantColumn = 2,
                .mShootingCounter = 33,
            },
        };
    pvz::game::LevelOneRandomDecisionTape aTape(
        kDecisions,
        false,
        true);
    pvz::game::LevelOneCombat aCombat;
    aCombat.SetRandomDecisionSource(&aTape);
    Expect(
        aCombat.AddPeashooter(2, 2) &&
            aCombat.GetState().mPlants[0].mLaunchCounter == 2 &&
            aTape.GetReadCount() == 1,
        "new Peashooter consumes its source launch schedule");

    aCombat.Update();
    Expect(
        aCombat.GetState().mPlants[0].mLaunchCounter == 1 &&
            aTape.GetReadCount() == 1,
        "Peashooter launch schedule counts down without drift");

    aCombat.Update();
    Expect(
        aCombat.GetState().mPlants[0].mLaunchCounter == 140 &&
            aCombat.GetState().mPlants[0].mShootingCounter == 33 &&
            aTape.GetReadCount() == 2 &&
            aTape.GetRemainingCount() == 0 &&
            !aCombat.HasRandomDecisionFailure(),
        "Peashooter reload consumes cadence and firing decisions together");

    constexpr std::array<pvz::game::LevelOneRandomDecision, 0>
        kMissingDecisions{};
    pvz::game::LevelOneRandomDecisionTape aMissingTape(
        kMissingDecisions,
        false,
        true);
    pvz::game::LevelOneCombat aRejectedCombat;
    aRejectedCombat.SetRandomDecisionSource(&aMissingTape);
    Expect(
        !aRejectedCombat.AddPeashooter(2, 2) &&
            aRejectedCombat.HasRandomDecisionFailure() &&
            aMissingTape.GetError() ==
                pvz::game::LevelOneRandomDecisionReadError::Exhausted,
        "advertised Peashooter scheduling rejects a missing decision");
}

void TestProjectileSpawnDecision()
{
    constexpr std::array<pvz::game::LevelOneRandomDecision, 1>
        kDecisions{
            pvz::game::LevelOneRandomDecision{
                .mKind = pvz::game::LevelOneRandomDecisionKind::
                    ProjectileSpawn,
                .mXMilliPixels = 123'000,
                .mGroundYMilliPixels = 456'000,
                .mPlantColumn = 2,
            },
        };
    pvz::game::LevelOneRandomDecisionTape aTape(
        kDecisions,
        false,
        false,
        true);
    pvz::game::LevelOneCombat aCombat;
    Expect(
        aCombat.AddPeashooter(2, 2),
        "projectile fixture adds its Peashooter");
    auto aState = aCombat.GetState();
    aState.mPlants[0].mLaunchCounter = 100;
    aState.mPlants[0].mShootingCounter = 2;
    Expect(
        aCombat.RestoreState(aState),
        "projectile spawn fixture restores");
    aCombat.SetRandomDecisionSource(&aTape);

    aCombat.Update();
    const auto aSpawned = aCombat.GetState();
    Expect(
        aSpawned.mProjectileCount == 1 &&
            aSpawned.mProjectiles[0].mActive &&
            aSpawned.mProjectiles[0].mXMilliPixels == 126'330 &&
            aSpawned.mProjectiles[0].mYMilliPixels == 456'000 &&
            aTape.GetReadCount() == 1 &&
            aTape.GetRemainingCount() == 0 &&
            !aCombat.HasRandomDecisionFailure(),
        "captured projectile origin precedes portable same-tick movement");
}

void TestZombieMotionDecision()
{
    constexpr std::array<pvz::game::LevelOneRandomDecision, 1>
        kDecisions{
            pvz::game::LevelOneRandomDecision{
                .mKind = pvz::game::LevelOneRandomDecisionKind::
                    ZombieMotion,
                .mXMilliPixels = 699'640,
                .mPlantColumn = 0,
                .mShootingCounter = 1,
            },
        };
    pvz::game::LevelOneRandomDecisionTape aTape(
        kDecisions,
        false,
        false,
        false,
        true);
    pvz::game::LevelOneCombat aCombat;
    auto aState = aCombat.GetState();
    aState.mPhase = pvz::game::LevelOneCombatPhase::Active;
    aState.mSunCountdown = 500;
    aState.mFirstWaveSpawned = true;
    aState.mCurrentWave = 1;
    aState.mZombieCountdown = 2'500;
    aState.mZombieCountdownStart = 2'500;
    aState.mZombieHealthWaveStart = 270;
    aState.mZombieHealthToNextWave = 135;
    aState.mZombieCount = 1;
    aState.mZombies[0] = {
        .mActive = true,
        .mRow = 2,
        .mHealth = 270,
        .mXMilliPixels = 700'000,
        .mSpeedMicroPixelsPerTick = 270'000,
        .mFromWave = 0,
    };
    Expect(
        aCombat.RestoreState(aState),
        "zombie motion fixture restores");
    aCombat.SetRandomDecisionSource(&aTape);

    aCombat.Update();
    Expect(
        aCombat.GetState().mZombies[0].mXMilliPixels == 699'640 &&
            aCombat.GetState().mZombies[0].mHealth == 269 &&
            aCombat.GetState().mZombies[0].mMovementRemainderMicroPixels ==
                0 &&
            aTape.GetReadCount() == 1 &&
            aTape.GetRemainingCount() == 0 &&
            !aCombat.HasRandomDecisionFailure(),
        "captured zombie motion includes the headless decay choice");
}

void TestProjectileCollisionUsesPreviousIntegerPosition()
{
    constexpr std::array<pvz::game::LevelOneRandomDecision, 2>
        kDecisions{
            pvz::game::LevelOneRandomDecision{
                .mKind = pvz::game::LevelOneRandomDecisionKind::
                    ZombieMotion,
                .mXMilliPixels = 500'000,
                .mPlantColumn = 0,
            },
            pvz::game::LevelOneRandomDecision{
                .mKind = pvz::game::LevelOneRandomDecisionKind::
                    ProjectileMotion,
                .mXMilliPixels = 498'000,
                .mPlantColumn = 0,
            },
        };
    pvz::game::LevelOneRandomDecisionTape aTape(
        kDecisions,
        false,
        false,
        false,
        true,
        true);
    pvz::game::LevelOneCombat aCombat;
    auto aState = aCombat.GetState();
    aState.mPhase = pvz::game::LevelOneCombatPhase::Active;
    aState.mSunCountdown = 500;
    aState.mFirstWaveSpawned = true;
    aState.mCurrentWave = 1;
    aState.mZombieCountdown = 2'500;
    aState.mZombieCountdownStart = 2'500;
    aState.mZombieHealthWaveStart = 270;
    aState.mZombieHealthToNextWave = 135;
    aState.mZombieCount = 1;
    aState.mProjectileCount = 1;
    aState.mZombies[0] = {
        .mActive = true,
        .mRow = 2,
        .mHealth = 270,
        .mXMilliPixels = 500'000,
        .mSpeedMicroPixelsPerTick = 270'000,
        .mFromWave = 0,
    };
    aState.mProjectiles[0] = {
        .mActive = true,
        .mRow = 2,
        .mXMilliPixels = 495'000,
        .mYMilliPixels = 337'000,
    };
    Expect(
        aCombat.RestoreState(aState),
        "projectile collision-order fixture restores");
    aCombat.SetRandomDecisionSource(&aTape);

    aCombat.Update();
    const auto& anUpdated = aCombat.GetState();
    Expect(
        anUpdated.mProjectiles[0].mActive &&
            anUpdated.mProjectiles[0].mXMilliPixels == 498'000 &&
            anUpdated.mZombies[0].mHealth == 270 &&
            !aCombat.HasRandomDecisionFailure(),
        "projectile collision uses legacy previous integer position");
}

void TestFinalZombieHeadLossDropsAward()
{
    constexpr std::array<pvz::game::LevelOneRandomDecision, 2>
        kDecisions{
            pvz::game::LevelOneRandomDecision{
                .mKind = pvz::game::LevelOneRandomDecisionKind::
                    ZombieMotion,
                .mXMilliPixels = 579'000,
                .mPlantColumn = 0,
            },
            pvz::game::LevelOneRandomDecision{
                .mKind = pvz::game::LevelOneRandomDecisionKind::
                    ProjectileMotion,
                .mXMilliPixels = 579'000,
                .mPlantColumn = 0,
            },
        };
    pvz::game::LevelOneRandomDecisionTape aTape(
        kDecisions,
        false,
        false,
        false,
        true,
        true);
    pvz::game::LevelOneCombat aCombat;
    auto aState = aCombat.GetState();
    aState.mPhase = pvz::game::LevelOneCombatPhase::Active;
    aState.mSunCountdown = 500;
    aState.mFirstWaveSpawned = true;
    aState.mFirstWaveCleared = true;
    aState.mCurrentWave =
        pvz::game::LevelOneCombat::kWaveCount;
    aState.mZombieCountdown = 3'017;
    aState.mZombieCountdownStart = 3'017;
    aState.mZombieHealthWaveStart = 540;
    aState.mZombieHealthToNextWave = 270;
    aState.mZombieCount = 1;
    aState.mProjectileCount = 1;
    aState.mZombies[0] = {
        .mActive = true,
        .mRow = 2,
        .mHealth =
            pvz::game::LevelOneCombat::
                kNormalZombieHeadLossHealth,
        .mXMilliPixels = 579'000,
        .mSpeedMicroPixelsPerTick = 270'000,
        .mFromWave = 3,
    };
    aState.mProjectiles[0] = {
        .mActive = true,
        .mRow = 2,
        .mXMilliPixels = 576'000,
        .mYMilliPixels = 337'000,
    };
    Expect(
        aCombat.RestoreState(aState),
        "final-zombie head-loss fixture restores");
    aCombat.SetRandomDecisionSource(&aTape);

    aCombat.Update();
    const auto& anUpdated = aCombat.GetState();
    Expect(
        anUpdated.mPhase ==
                pvz::game::LevelOneCombatPhase::Won &&
            anUpdated.mZombieCount == 0 &&
            anUpdated.mAwardSpawned &&
            anUpdated.mAwardXMilliPixels == 636'000 &&
            aTape.GetRemainingCount() == 0 &&
            !aCombat.HasRandomDecisionFailure(),
        "final zombie drops the award at the native head-loss threshold");
}

void TestMowerPrecedesLossAndIsSingleUse()
{
    pvz::game::LevelOneCombat aCombat;
    auto aState = aCombat.GetState();
    aState.mPhase = pvz::game::LevelOneCombatPhase::Active;
    aState.mSunCountdown = 500;
    aState.mFirstWaveSpawned = true;
    aState.mCurrentWave = 1;
    aState.mZombieCountdown = 2'500;
    aState.mZombieCountdownStart = 2'500;
    aState.mZombieHealthWaveStart = 270;
    aState.mZombieHealthToNextWave = 135;
    aState.mZombieCount = 1;
    aState.mZombies[0] = {
        .mActive = true,
        .mRow = 2,
        .mHealth = 270,
        .mXMilliPixels = -7'800,
        .mSpeedMicroPixelsPerTick = 270'000,
        .mFromWave = 0,
    };
    Expect(
        aCombat.RestoreState(aState),
        "ready-mower fixture restores");
    aCombat.Update();
    Expect(
        aCombat.GetState().mPhase ==
                pvz::game::LevelOneCombatPhase::Active &&
            aCombat.GetState().mZombieCount == 0 &&
            aCombat.GetState().mMowerPhase ==
                pvz::game::LevelOneMowerPhase::Triggered &&
            aCombat.GetState().mMowerXMilliPixels >
                pvz::game::LevelOneCombat::kMowerReadyXMilliPixels,
        "ready mower removes the approaching zombie before loss");

    auto aSpentState = aState;
    aSpentState.mMowerPhase =
        pvz::game::LevelOneMowerPhase::Spent;
    aSpentState.mMowerXMilliPixels = 803'330;
    aSpentState.mZombies[0].mXMilliPixels = -99'800;
    Expect(
        aCombat.RestoreState(aSpentState),
        "spent-mower fixture restores");
    aCombat.Update();
    Expect(
        aCombat.GetState().mPhase ==
            pvz::game::LevelOneCombatPhase::Lost,
        "a later zombie reaches the legacy loss edge after mower use");
}

void TestEatingUsesFourTickDamageCadence()
{
    pvz::game::LevelOneCombat aCombat;
    auto aState = aCombat.GetState();
    aState.mPhase =
        pvz::game::LevelOneCombatPhase::Active;
    aState.mFirstWaveSpawned = true;
    aState.mCurrentWave = 1;
    aState.mZombieCountdown = 2'500;
    aState.mZombieCountdownStart = 2'500;
    aState.mZombieHealthWaveStart = 270;
    aState.mZombieHealthToNextWave = 135;
    aState.mPlantCount = 1;
    aState.mZombieCount = 1;
    aState.mPlants[0] = {
        .mActive = true,
        .mColumn = 0,
        .mRow = 2,
        .mHealth = 300,
        .mLaunchCounter = 100,
        .mShootingCounter = 0,
    };
    aState.mZombies[0] = {
        .mActive = true,
        .mRow = 2,
        .mHealth = 270,
        .mXMilliPixels = 40'000,
        .mSpeedMicroPixelsPerTick = 270'000,
        .mMovementRemainderMicroPixels = 0,
        .mAge = 3,
        .mEating = false,
        .mFromWave = 0,
    };
    Expect(
        aCombat.RestoreState(aState),
        "source-cadence combat fixture restores");
    aCombat.Update();
    Expect(
        aCombat.GetState().mPlants[0].mHealth == 296 &&
            aCombat.GetState().mZombies[0].mEating,
        "zombie deals four damage when age reaches a multiple of four");
    for (std::uint8_t aTick = 0; aTick < 3; ++aTick)
        aCombat.Update();
    Expect(
        aCombat.GetState().mPlants[0].mHealth == 296,
        "zombie does not damage between four-tick eat events");
    aCombat.Update();
    Expect(
        aCombat.GetState().mPlants[0].mHealth == 292,
        "next eat event deals the next four damage");
}

void TestCombatStateIsFixedWidthAndTransactional()
{
    pvz::game::LevelOneCombat aCombat;
    static_cast<void>(aCombat.AddPeashooter(2, 2));
    for (std::uint16_t aTick = 0; aTick < 400; ++aTick)
        aCombat.Update();
    Expect(
        CollectFirstAvailableSun(aCombat),
        "combat state fixture starts a sun collection flight");

    pvz::engine::core::BinaryStateWriter aWriter;
    Expect(
        aCombat.SaveState(aWriter) &&
            aWriter.GetBytesWritten() == 782,
        "combat state has a stable explicit 782-byte schema");

    pvz::game::LevelOneCombat aRestored;
    pvz::engine::core::BinaryStateReader aReader(
        aWriter.GetBytes());
    Expect(
        aRestored.LoadState(aReader) &&
            aReader.GetBytesRemaining() == 0 &&
            aRestored.GetState().mTick == 400 &&
            aRestored.GetState().mSunCount == 1 &&
            aRestored.GetState().mSuns[0].mBeingCollected &&
            aRestored.GetOccupiedCells() ==
                (std::uint64_t{1} << 20U),
        "fixed-width combat state round-trips fieldwise");

    constexpr std::size_t kCombatHeaderBytes = 9;
    constexpr std::size_t kVersionSevenSunBytes = 16;
    constexpr std::size_t kSunSlotCount = 8;
    constexpr std::size_t kSharedPreZombieBytes = 79;
    constexpr std::size_t kVersionSevenZombieBytes = 19;
    constexpr std::size_t kZombieSlotCount = 8;
    const auto aCurrentBytes = aWriter.GetBytes();
    std::vector<std::byte> aVersionSevenBytes(
        aCurrentBytes.begin(),
        aCurrentBytes.begin() + 752);
    pvz::engine::core::BinaryStateReader aVersionSevenReader(
        aVersionSevenBytes);
    pvz::game::LevelOneCombat aVersionSevenRestored;
    Expect(
        aVersionSevenRestored.LoadState(
            aVersionSevenReader,
            true,
            false) &&
            aVersionSevenReader.GetBytesRemaining() == 0 &&
            aVersionSevenRestored.GetState().mCurrentWave == 0 &&
            aVersionSevenRestored.GetState().mMowerPhase ==
                pvz::game::LevelOneMowerPhase::Ready,
        "version-seven combat state defaults complete-level fields");

    std::vector<std::byte> aVersionSixBytes;
    aVersionSixBytes.reserve(712);
    aVersionSixBytes.insert(
        aVersionSixBytes.end(),
        aCurrentBytes.begin(),
        aCurrentBytes.begin() + kCombatHeaderBytes);
    auto aSunOffset = kCombatHeaderBytes;
    for (std::size_t aSunIndex = 0;
         aSunIndex < kSunSlotCount;
         ++aSunIndex)
    {
        aVersionSixBytes.push_back(aCurrentBytes[aSunOffset]);
        aVersionSixBytes.insert(
            aVersionSixBytes.end(),
            aCurrentBytes.begin() + aSunOffset + 2,
            aCurrentBytes.begin() +
                aSunOffset + kVersionSevenSunBytes);
        aSunOffset += kVersionSevenSunBytes;
    }
    aVersionSixBytes.insert(
        aVersionSixBytes.end(),
        aCurrentBytes.begin() + aSunOffset,
        aCurrentBytes.begin() +
            aSunOffset + kSharedPreZombieBytes);
    auto aZombieOffset =
        aSunOffset + kSharedPreZombieBytes;
    for (std::size_t aZombieIndex = 0;
         aZombieIndex < kZombieSlotCount;
         ++aZombieIndex)
    {
        aVersionSixBytes.insert(
            aVersionSixBytes.end(),
            aCurrentBytes.begin() + aZombieOffset,
            aCurrentBytes.begin() + aZombieOffset + 8);
        const auto aSpeedMicroPixelsPerTick =
            std::to_integer<std::uint32_t>(
                aCurrentBytes[aZombieOffset + 8]) |
            (std::to_integer<std::uint32_t>(
                 aCurrentBytes[aZombieOffset + 9]) << 8U) |
            (std::to_integer<std::uint32_t>(
                 aCurrentBytes[aZombieOffset + 10]) << 16U) |
            (std::to_integer<std::uint32_t>(
                 aCurrentBytes[aZombieOffset + 11]) << 24U);
        const auto aLegacySpeed =
            static_cast<std::uint16_t>(
                (aSpeedMicroPixelsPerTick + 500U) / 1'000U);
        aVersionSixBytes.push_back(
            static_cast<std::byte>(aLegacySpeed & 0xFFU));
        aVersionSixBytes.push_back(
            static_cast<std::byte>(aLegacySpeed >> 8U));
        aVersionSixBytes.insert(
            aVersionSixBytes.end(),
            aCurrentBytes.begin() + aZombieOffset + 14,
            aCurrentBytes.begin() +
                aZombieOffset + kVersionSevenZombieBytes);
        aZombieOffset += kVersionSevenZombieBytes;
    }
    aVersionSixBytes.insert(
        aVersionSixBytes.end(),
        aCurrentBytes.begin() + aZombieOffset,
        aCurrentBytes.begin() + 752);
    pvz::engine::core::BinaryStateReader aVersionSixReader(
        aVersionSixBytes);
    pvz::game::LevelOneCombat aVersionSixRestored;
    Expect(
        aVersionSixBytes.size() == 712 &&
            aVersionSixRestored.LoadState(
                aVersionSixReader,
                false) &&
            aVersionSixReader.GetBytesRemaining() == 0 &&
            !aVersionSixRestored.GetState().mSuns[0]
                 .mBeingCollected &&
            aVersionSixRestored.GetState().mZombies[0]
                    .mMovementRemainderMicroPixels == 0,
        "version-six combat state defaults extended fixed-width fields");

    auto anInvalidState = aRestored.GetState();
    anInvalidState.mPlantCount = 2;
    Expect(
        !aRestored.RestoreState(anInvalidState) &&
            aRestored.GetState().mPlantCount == 1,
        "invalid entity counts are rejected transactionally");

    anInvalidState = aRestored.GetState();
    anInvalidState.mZombieCountdown = 1;
    Expect(
        !aRestored.RestoreState(anInvalidState) &&
            aRestored.GetState().mZombieCountdown == 0,
        "countdown progress cannot exceed its fixed-width start value");

    anInvalidState = aRestored.GetState();
    anInvalidState.mAwardXMilliPixels = 1;
    Expect(
        !aRestored.RestoreState(anInvalidState) &&
            !aRestored.GetState().mAwardSpawned,
        "inactive award coordinates are rejected transactionally");
}

} // namespace

void RunLevelOneCombatTests()
{
    TestSourceAuditedCombatConstants();
    TestTutorialSunAndFirstWaveGate();
    TestSunHitTestPreservesFractionalPosition();
    TestCombatCompletesAllFourWavesDeterministically();
    TestSemanticRandomDecisionTape();
    TestWaveScheduleDecisionAndAcceleration();
    TestPeashooterScheduleDecision();
    TestProjectileSpawnDecision();
    TestZombieMotionDecision();
    TestProjectileCollisionUsesPreviousIntegerPosition();
    TestFinalZombieHeadLossDropsAward();
    TestMowerPrecedesLossAndIsSingleUse();
    TestEatingUsesFourTickDamageCadence();
    TestCombatStateIsFixedWidthAndTransactional();
}
