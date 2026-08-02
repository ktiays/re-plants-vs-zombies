#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace pvz::game::test::legacy_combat_reference
{

// Source-audited from the reconstructed Windows implementation at this
// revision. This fixture intentionally does not include portable combat code.
// Runtime confirmation for the first-wave path is recorded separately by the
// Windows PVZB capture. Complete native captures are replayed through the v6
// health/projectile gate; this fixture remains the independent source oracle.
inline constexpr std::string_view kLegacySourceRevision =
    "79f7b4cc4d09eae842e0bb57ad798ffef8e25007";

inline constexpr std::uint8_t kLevelOneWaveCount = 4;
inline constexpr std::array<std::uint8_t, 4> kNormalZombiesPerWave{
    1,
    1,
    1,
    2,
};
inline constexpr std::uint16_t kTutorialSunCountdown = 400;
inline constexpr std::uint16_t kFirstWaveCountdown = 99;
inline constexpr std::uint16_t kNextWaveCountdownMinimum = 2'500;
inline constexpr std::uint16_t kNextWaveCountdownMaximum = 3'099;
inline constexpr std::uint16_t kWaveAccelerationCountdown = 200;
inline constexpr std::uint16_t kWaveAccelerationMinimumAge = 400;
inline constexpr std::uint16_t kSunValue = 25;
inline constexpr std::uint16_t kNextSunCountdownMinimum = 435;
inline constexpr std::int32_t kSunSpawnMinimumXMilliPixels = 100'000;
inline constexpr std::int32_t kSunSpawnMaximumXMilliPixels = 649'000;
inline constexpr std::int32_t kSunSpawnYMilliPixels = 60'000;
inline constexpr std::int32_t kSunGroundMinimumYMilliPixels = 300'000;
inline constexpr std::int32_t kSunGroundMaximumYMilliPixels = 549'000;
inline constexpr std::int32_t kSunFallSpeedMilliPixelsPerTick = 670;
inline constexpr std::uint16_t kSunGroundDisappearTicks = 750;
inline constexpr std::uint8_t kSunFadeTicks = 15;
inline constexpr std::uint16_t kPlantHealth = 300;
inline constexpr std::uint16_t kNormalZombieHealth = 270;
inline constexpr std::uint16_t kNormalZombieHeadLossHealth = 90;
inline constexpr std::uint16_t kPeaDamage = 20;
inline constexpr std::uint16_t kPeashooterLaunchRate = 150;
inline constexpr std::uint8_t kPeashooterFireDelay = 33;
inline constexpr std::uint8_t kEatInterval = 4;
inline constexpr std::uint16_t kEatDamage = 4;
inline constexpr std::int32_t kPeaSpeedMilliPixelsPerTick = 3'330;
inline constexpr std::int32_t kZombieSpawnMinimumXMilliPixels = 780'000;
inline constexpr std::int32_t kZombieSpawnMaximumXMilliPixels = 819'000;
inline constexpr std::uint16_t kZombieMinimumSpeedMilliPixelsPerTick = 230;
inline constexpr std::uint16_t kZombieMaximumSpeedMilliPixelsPerTick = 320;
inline constexpr std::int32_t kNormalZombieAttackRectX = 20;
inline constexpr std::int32_t kNormalZombieAttackRectWidth = 50;
inline constexpr std::int32_t kNormalZombieRectX = 36;
inline constexpr std::int32_t kNormalZombieRectWidth = 42;
inline constexpr std::int32_t kZombieLossXMilliPixels = -100'000;
inline constexpr std::int32_t kMowerReadyXMilliPixels = -21'000;
inline constexpr std::int32_t kMowerAttackWidthPixels = 50;
inline constexpr std::int32_t kMowerSpeedMilliPixelsPerTick = 3'330;
inline constexpr std::int32_t kMowerSpentXMilliPixels = 800'000;

// Deterministic choices within the source-audited random ranges. These are
// explicit provisional inputs, not claimed Windows RNG output.
inline constexpr std::int32_t kPortableSpawnXMilliPixels = 780'000;
inline constexpr std::uint16_t kPortableSpeedMilliPixelsPerTick = 270;
inline constexpr std::int32_t kPortableSunXMilliPixels = 375'000;
inline constexpr std::int32_t kPortableSunGroundYMilliPixels = 400'000;
inline constexpr std::uint16_t kPortableSunLifetime = 1'272;

} // namespace pvz::game::test::legacy_combat_reference
