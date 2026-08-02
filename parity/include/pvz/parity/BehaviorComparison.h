#pragma once

#include "pvz/game/BehaviorObservation.h"

#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

namespace pvz::parity
{

inline constexpr std::uint64_t kNoBehaviorDifferenceTick =
    std::numeric_limits<std::uint64_t>::max();

enum class BehaviorField : std::uint8_t
{
    None,
    Tick,
    Scene,
    BoardStage,
    GridColumn,
    GridRow,
    OccupiedCells,
    PlantCount,
    Sun,
    SeedRefreshCounter,
    SeedRefreshTime,
    SeedRefreshing,
    SeedSelection,
    TutorialPhase,
    FirstSunCountdown,
    FirstSunSpawned,
    CurrentWave,
    ZombieCountdown,
    ZombieCount,
    LevelOutcome,
    MowerState,
    LevelAwardSpawned,
    ZombieWaveHealth,
    ProjectileCount,
    SunActive,
    SunBeingCollected,
    SunX,
    SunY,
    SunGroundY,
    SunAge,
    ObservationCount,
};

struct BehaviorDifference
{
    std::uint64_t mTick{kNoBehaviorDifferenceTick};
    BehaviorField mField{BehaviorField::None};
    std::uint8_t mSlot{game::kBehaviorSunSlotCount};
};

[[nodiscard]] std::string_view GetBehaviorFieldName(
    BehaviorField theField);
[[nodiscard]] BehaviorDifference FindFirstBehaviorDifference(
    std::span<const game::BehaviorObservation> theLeft,
    std::span<const game::BehaviorObservation> theRight,
    std::uint16_t theCommonFormatVersion = 2);
[[nodiscard]] BehaviorDifference FindFirstSunTrajectoryDifference(
    std::span<const game::BehaviorObservation> theLeft,
    std::span<const game::BehaviorObservation> theRight,
    std::uint16_t theCommonFormatVersion);

static_assert(sizeof(BehaviorField) == 1);

} // namespace pvz::parity
