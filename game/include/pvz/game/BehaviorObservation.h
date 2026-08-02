#pragma once

#include <cstdint>

namespace pvz::game
{

inline constexpr std::uint8_t kNoGridCoordinate = 0xFF;
inline constexpr std::uint8_t kBehaviorBoardColumnCount = 9;
inline constexpr std::uint8_t kBehaviorBoardRowCount = 6;
inline constexpr std::uint64_t kBehaviorOccupiedCellMask =
    (std::uint64_t{1} << 54U) - 1U;

enum class BehaviorScene : std::uint8_t
{
    Loading,
    Title,
    MainMenu,
    AdventureIntro,
    AdventurePlaying,
    Other,
    Count,
};

enum class BehaviorBoardStage : std::uint8_t
{
    None,
    Day,
    Night,
    Pool,
    Fog,
    Roof,
    Boss,
    Other,
    Count,
};

enum class BehaviorSeedSelection : std::uint8_t
{
    None,
    Peashooter,
    Other,
    Count,
};

enum class BehaviorTutorialPhase : std::uint8_t
{
    None,
    LevelOnePickUpPeashooter,
    LevelOnePlantPeashooter,
    LevelOneRefreshPeashooter,
    LevelOneCompleted,
    Other,
    Count,
};

struct BehaviorObservation
{
    std::uint64_t mTick{};
    BehaviorScene mScene{BehaviorScene::Loading};
    BehaviorBoardStage mBoardStage{BehaviorBoardStage::None};
    std::uint8_t mGridColumn{kNoGridCoordinate};
    std::uint8_t mGridRow{kNoGridCoordinate};
    std::uint64_t mOccupiedCells{};
    std::uint32_t mPlantCount{};
    std::uint16_t mSun{};
    std::uint16_t mSeedRefreshCounter{};
    std::uint16_t mSeedRefreshTime{};
    bool mSeedRefreshing{};
    BehaviorSeedSelection mSeedSelection{
        BehaviorSeedSelection::None};
    BehaviorTutorialPhase mTutorialPhase{
        BehaviorTutorialPhase::None};
    std::uint16_t mFirstSunCountdown{};
    bool mFirstSunSpawned{};
};

static_assert(sizeof(BehaviorScene) == 1);
static_assert(sizeof(BehaviorBoardStage) == 1);
static_assert(sizeof(BehaviorSeedSelection) == 1);
static_assert(sizeof(BehaviorTutorialPhase) == 1);

} // namespace pvz::game
