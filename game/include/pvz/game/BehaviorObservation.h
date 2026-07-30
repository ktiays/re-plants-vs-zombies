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

struct BehaviorObservation
{
    std::uint64_t mTick{};
    BehaviorScene mScene{BehaviorScene::Loading};
    BehaviorBoardStage mBoardStage{BehaviorBoardStage::None};
    std::uint8_t mGridColumn{kNoGridCoordinate};
    std::uint8_t mGridRow{kNoGridCoordinate};
    std::uint64_t mOccupiedCells{};
    std::uint32_t mPlantCount{};
};

static_assert(sizeof(BehaviorScene) == 1);
static_assert(sizeof(BehaviorBoardStage) == 1);

} // namespace pvz::game
