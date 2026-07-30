#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace pvz::game::test::legacy_reference
{

// Frozen from the reconstructed Windows implementation at the revision below.
// Sources:
//   GameConstants.h: BOARD_OFFSET, LAWN_XMIN, LAWN_YMIN
//   Lawn/Board.cpp: Board::PixelToGridX, Board::PixelToGridY,
//                   Board::GridToPixelX, Board::GridToPixelY
//
// This fixture intentionally does not include or call portable BoardGeometry.
// Updating it requires re-capturing or re-auditing the Windows reference first.
inline constexpr std::string_view kLegacySourceRevision =
    "5808aade906fb506f0d4cadf5e629ad8dc4228c1";

enum class Stage : std::uint8_t
{
    Day,
    Pool,
    Roof,
};

struct StageFixture
{
    Stage mStage;
    std::uint8_t mRowCount;
    std::uint32_t mCellHeight;
    std::array<std::int32_t, 6> mFlatPlantOriginsY;
    std::array<std::int32_t, 9> mRoofPlantBaseY;
    std::array<std::int32_t, 9> mRoofHitBaseY;
};

inline constexpr std::array<std::int32_t, 9> kColumnOriginsX{
    40,
    120,
    200,
    280,
    360,
    440,
    520,
    600,
    680,
};

inline constexpr std::array<StageFixture, 3> kStages{{
    {
        .mStage = Stage::Day,
        .mRowCount = 5,
        .mCellHeight = 100,
        .mFlatPlantOriginsY = {80, 180, 280, 380, 480, -1},
        .mRoofPlantBaseY = {},
        .mRoofHitBaseY = {},
    },
    {
        .mStage = Stage::Pool,
        .mRowCount = 6,
        .mCellHeight = 85,
        .mFlatPlantOriginsY = {80, 165, 250, 335, 420, 505},
        .mRoofPlantBaseY = {},
        .mRoofHitBaseY = {},
    },
    {
        .mStage = Stage::Roof,
        .mRowCount = 5,
        .mCellHeight = 85,
        .mFlatPlantOriginsY = {},
        .mRoofPlantBaseY =
            {
                170,
                150,
                130,
                110,
                90,
                70,
                70,
                70,
                70,
            },
        .mRoofHitBaseY =
            {
                160,
                140,
                120,
                100,
                80,
                80,
                80,
                80,
                80,
            },
    },
}};

inline constexpr std::int32_t kBoardBackgroundSourceX = 220;
inline constexpr std::uint32_t kBoardWidth = 800;
inline constexpr std::uint32_t kBoardHeight = 600;
inline constexpr std::uint32_t kCellWidth = 80;

} // namespace pvz::game::test::legacy_reference
