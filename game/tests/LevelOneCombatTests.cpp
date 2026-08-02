#include "fixtures/LegacyLevelOneCombatFixture.h"

#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/game/LevelOneCombat.h"
#include "pvz/game/LevelOneRandomDecision.h"

#include <array>
#include <cstddef>
#include <cstdint>
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
        .mSpeedMicroPixelsPerTick = 270'000,
        .mMovementRemainderMicroPixels = 0,
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
    Expect(
        CollectFirstAvailableSun(aCombat),
        "combat state fixture starts a sun collection flight");

    pvz::engine::core::BinaryStateWriter aWriter;
    Expect(
        aCombat.SaveState(aWriter) &&
            aWriter.GetBytesWritten() == 752,
        "combat state has a stable explicit 752-byte schema");

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
        aCurrentBytes.end());
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
}

} // namespace

void RunLevelOneCombatTests()
{
    TestSourceAuditedCombatConstants();
    TestTutorialSunAndFirstWaveGate();
    TestCombatClearsFirstWaveDeterministically();
    TestSemanticRandomDecisionTape();
    TestEatingUsesFourTickDamageCadence();
    TestCombatStateIsFixedWidthAndTransactional();
}
