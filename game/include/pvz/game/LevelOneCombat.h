#pragma once

#include "pvz/engine/StateIO.h"
#include "pvz/engine/Types.h"
#include "pvz/game/LevelOneRandomDecision.h"

#include <array>
#include <cstdint>

namespace pvz::game
{

enum class LevelOneCombatPhase : std::uint8_t
{
    AwaitingFirstPlant,
    AwaitingSecondPlant,
    Active,
    FirstWaveCleared,
    Lost,
    Count,
};

struct LevelOneSunState
{
    bool mActive{};
    bool mBeingCollected{};
    std::int32_t mXMilliPixels{};
    std::int32_t mYMilliPixels{};
    std::int32_t mGroundYMilliPixels{};
    std::uint16_t mAge{};
};

struct LevelOnePlantCombatState
{
    bool mActive{};
    std::uint8_t mColumn{};
    std::uint8_t mRow{};
    std::uint16_t mHealth{};
    std::uint16_t mLaunchCounter{};
    std::uint8_t mShootingCounter{};
};

struct LevelOneZombieState
{
    bool mActive{};
    std::uint8_t mRow{};
    std::uint16_t mHealth{};
    std::int32_t mXMilliPixels{};
    std::uint32_t mSpeedMicroPixelsPerTick{};
    std::uint16_t mMovementRemainderMicroPixels{};
    std::uint32_t mAge{};
    bool mEating{};
};

struct LevelOneProjectileState
{
    bool mActive{};
    std::uint8_t mRow{};
    std::int32_t mXMilliPixels{};
    std::int32_t mYMilliPixels{};
    std::uint16_t mAge{};
};

struct LevelOneCombatState
{
    LevelOneCombatPhase mPhase{
        LevelOneCombatPhase::AwaitingFirstPlant};
    std::uint32_t mTick{};
    std::uint16_t mSunCountdown{};
    std::uint8_t mSunsSpawned{};
    std::uint8_t mSunCount{};
    std::array<LevelOneSunState, 8> mSuns;
    std::uint16_t mZombieCountdown{};
    bool mFirstWaveSpawned{};
    bool mFirstWaveCleared{};
    std::uint8_t mPlantCount{};
    std::uint8_t mZombieCount{};
    std::uint8_t mProjectileCount{};
    std::array<LevelOnePlantCombatState, 9> mPlants;
    std::array<LevelOneZombieState, 8> mZombies;
    std::array<LevelOneProjectileState, 32> mProjectiles;
};

class LevelOneCombat
{
public:
    static constexpr std::uint8_t kLaneRow = 2;
    static constexpr std::uint16_t kSunValue = 25;
    static constexpr std::uint16_t kTutorialSunCountdown = 400;
    static constexpr std::uint16_t kFirstWaveCountdown = 99;
    static constexpr std::uint16_t kPlantHealth = 300;
    static constexpr std::uint16_t kNormalZombieHealth = 270;
    static constexpr std::uint16_t kPeaDamage = 20;
    static constexpr std::uint16_t kPeashooterLaunchRate = 150;
    static constexpr std::uint8_t kPeashooterFireDelay = 33;
    static constexpr std::uint8_t kEatInterval = 4;
    static constexpr std::uint16_t kEatDamage = 4;
    static constexpr std::int32_t kNormalZombieAttackRectX = 20;
    static constexpr std::int32_t kNormalZombieAttackRectWidth = 50;
    static constexpr std::int32_t kNormalZombieRectX = 36;
    static constexpr std::int32_t kNormalZombieRectWidth = 42;
    static constexpr std::int32_t kPeaSpeedMilliPixelsPerTick = 3'330;
    static constexpr std::uint16_t
        kDeterministicNextSunCountdown = 435;
    static constexpr std::int32_t
        kDeterministicSunSpawnXMilliPixels = 375'000;
    static constexpr std::int32_t
        kDeterministicSunSpawnYMilliPixels = 60'000;
    static constexpr std::int32_t
        kDeterministicSunGroundYMilliPixels = 400'000;
    static constexpr std::int32_t kSunFallSpeedMilliPixelsPerTick = 670;
    static constexpr std::uint16_t kDeterministicSunLifetime = 1'272;
    static constexpr std::int32_t kDeterministicZombieSpawnXMilliPixels =
        780'000;
    static constexpr std::uint16_t
        kDeterministicZombieSpeedMilliPixelsPerTick = 270;

    void SetRandomDecisionSource(
        ILevelOneRandomDecisionSource* theSource);
    void Reset();
    void Update();
    [[nodiscard]] bool AddPeashooter(
        std::uint8_t theColumn,
        std::uint8_t theRow);
    [[nodiscard]] bool TryCollectSun(engine::PointI thePosition);
    [[nodiscard]] std::uint16_t ConsumeCollectedSun();
    [[nodiscard]] std::uint64_t ConsumeDestroyedCells();

    [[nodiscard]] LevelOneCombatState GetState() const;
    [[nodiscard]] bool RestoreState(
        const LevelOneCombatState& theState);
    [[nodiscard]] bool SaveState(
        engine::IStateWriter& theWriter) const;
    [[nodiscard]] bool LoadState(engine::IStateReader& theReader);
    [[nodiscard]] bool LoadState(
        engine::IStateReader& theReader,
        bool theHasExtendedCombatState);
    [[nodiscard]] std::uint64_t GetOccupiedCells() const;
    [[nodiscard]] bool HasRandomDecisionFailure() const;

private:
    void UpdatePlants();
    void UpdateZombies();
    void UpdateProjectiles();
    void UpdateSun();
    void UpdateWave();
    [[nodiscard]] bool HasTarget(
        const LevelOnePlantCombatState& thePlant) const;
    void FirePea(const LevelOnePlantCombatState& thePlant);
    [[nodiscard]] LevelOnePlantCombatState* FindPlantTarget(
        const LevelOneZombieState& theZombie);
    void SpawnFirstWave();
    void RecountEntities();
    [[nodiscard]] bool ReadFallingSunDecision(
        LevelOneRandomDecision& theDecision);
    [[nodiscard]] bool ReadNormalZombieDecision(
        LevelOneRandomDecision& theDecision);
    [[nodiscard]] static std::uint16_t CalculateSunLifetime(
        std::int32_t theGroundYMilliPixels);

    LevelOneCombatState mState;
    ILevelOneRandomDecisionSource* mRandomDecisionSource{};
    std::uint16_t mCollectedSun{};
    std::uint64_t mDestroyedCells{};
    bool mRandomDecisionFailure{};
};

static_assert(sizeof(LevelOneCombatPhase) == 1);

} // namespace pvz::game
