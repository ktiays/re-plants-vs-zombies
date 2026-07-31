#include "fixtures/LegacyLevelOneCombatFixture.h"

#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/game/LevelOneCombat.h"

#include <cstdint>

extern void Expect(bool theCondition, const char* theMessage);

namespace
{

namespace legacy =
    pvz::game::test::legacy_combat_reference;

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
                legacy::kNormalZombieHealth,
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
            aCombat.ConsumeCollectedSun() == 25,
        "falling tutorial sun has legacy-sized hit padding and value");

    for (std::uint16_t aTick = 0;
         aTick <
             pvz::game::LevelOneCombat::
                 kDeterministicNextSunCountdown;
         ++aTick)
        aCombat.Update();
    Expect(
        aCombat.GetState().mSunCount == 1 &&
            aCombat.TryCollectSun({405, 90}) &&
            aCombat.ConsumeCollectedSun() == 25,
        "second deterministic sky sun makes the second plant affordable");

    for (std::uint16_t aTick = 0;
         aTick <
             pvz::game::LevelOneCombat::
                 kDeterministicNextSunCountdown *
                 2;
         ++aTick)
        aCombat.Update();
    Expect(
        aCombat.GetState().mSunCount == 2,
        "sky-sun countdown continues while an earlier sun remains active");
    Expect(
        aCombat.TryCollectSun({405, 400}) &&
            aCombat.ConsumeCollectedSun() == 25 &&
            aCombat.GetState().mSunCount == 1,
        "overlapping suns remain independently collectible");
    Expect(
        aCombat.TryCollectSun({405, 90}) &&
            aCombat.ConsumeCollectedSun() == 25 &&
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
                    .mSpeedMilliPixelsPerTick ==
                legacy::kPortableSpeedMilliPixelsPerTick,
        "first wave spawns one fixed-width normal zombie");
}

void TestCombatClearsFirstWaveDeterministically()
{
    pvz::game::LevelOneCombat aCombat;
    static_cast<void>(aCombat.AddPeashooter(2, 2));
    static_cast<void>(aCombat.AddPeashooter(3, 2));

    std::uint32_t aTicks{};
    while (aCombat.GetState().mPhase !=
               pvz::game::LevelOneCombatPhase::
                   FirstWaveCleared &&
           aTicks < 5'000)
    {
        aCombat.Update();
        ++aTicks;
    }
    const auto aState = aCombat.GetState();
    Expect(
        aState.mPhase ==
                pvz::game::LevelOneCombatPhase::
                    FirstWaveCleared &&
            aState.mZombieCount == 0 &&
            aState.mFirstWaveCleared &&
            aState.mPlantCount == 2 &&
            aState.mProjectileCount <= 2,
        "two Peashooters deterministically clear the first wave");
}

void TestEatingUsesFourTickDamageCadence()
{
    pvz::game::LevelOneCombat aCombat;
    auto aState = aCombat.GetState();
    aState.mPhase =
        pvz::game::LevelOneCombatPhase::Active;
    aState.mFirstWaveSpawned = true;
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
        .mSpeedMilliPixelsPerTick = 270,
        .mAge = 3,
        .mEating = false,
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

    pvz::engine::core::BinaryStateWriter aWriter;
    Expect(
        aCombat.SaveState(aWriter) &&
            aWriter.GetBytesWritten() == 712,
        "combat state has a stable explicit 712-byte schema");

    pvz::game::LevelOneCombat aRestored;
    pvz::engine::core::BinaryStateReader aReader(
        aWriter.GetBytes());
    Expect(
        aRestored.LoadState(aReader) &&
            aReader.GetBytesRemaining() == 0 &&
            aRestored.GetState().mTick == 400 &&
            aRestored.GetState().mSunCount == 1 &&
            aRestored.GetOccupiedCells() ==
                (std::uint64_t{1} << 20U),
        "fixed-width combat state round-trips fieldwise");

    auto anInvalidState = aRestored.GetState();
    anInvalidState.mPlantCount = 2;
    Expect(
        !aRestored.RestoreState(anInvalidState) &&
            aRestored.GetState().mPlantCount == 1,
        "invalid entity counts are rejected transactionally");
}

} // namespace

void RunLevelOneCombatTests()
{
    TestSourceAuditedCombatConstants();
    TestTutorialSunAndFirstWaveGate();
    TestCombatClearsFirstWaveDeterministically();
    TestEatingUsesFourTickDamageCadence();
    TestCombatStateIsFixedWidthAndTransactional();
}
