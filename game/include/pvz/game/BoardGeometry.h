#pragma once

#include "pvz/engine/Types.h"

#include <cstdint>

namespace pvz::game
{

enum class BoardStageLayout : std::uint8_t
{
    Day,
    Pool,
    Roof,
    Count,
};

struct GridCoordinate
{
    std::uint8_t mColumn{};
    std::uint8_t mRow{};
};

struct BoardDimensions
{
    std::uint8_t mColumnCount{};
    std::uint8_t mRowCount{};
};

class BoardGeometry
{
public:
    [[nodiscard]] static BoardDimensions GetDimensions(
        BoardStageLayout theLayout);
    [[nodiscard]] static engine::PointI GridToPixel(
        BoardStageLayout theLayout,
        std::uint8_t theColumn,
        std::uint8_t theRow);
    [[nodiscard]] static engine::RectI GetCellRect(
        BoardStageLayout theLayout,
        std::uint8_t theColumn,
        std::uint8_t theRow);
    [[nodiscard]] static bool TryPixelToGrid(
        BoardStageLayout theLayout,
        engine::PointI thePosition,
        GridCoordinate& theCoordinate);
};

static_assert(sizeof(BoardStageLayout) == 1);
static_assert(sizeof(GridCoordinate) == 2);
static_assert(sizeof(BoardDimensions) == 2);

} // namespace pvz::game
