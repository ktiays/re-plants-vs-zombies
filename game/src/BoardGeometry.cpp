#include "pvz/game/BoardGeometry.h"

namespace pvz::game
{
namespace
{

inline constexpr std::int32_t kLawnOriginX = 40;
inline constexpr std::int32_t kLawnOriginY = 80;
inline constexpr std::int32_t kColumnPitch = 80;
inline constexpr std::int32_t kDayRowPitch = 100;
inline constexpr std::int32_t kPoolRoofRowPitch = 85;
inline constexpr std::uint8_t kColumnCount = 9;
inline constexpr std::uint8_t kDayRoofRowCount = 5;
inline constexpr std::uint8_t kPoolRowCount = 6;

[[nodiscard]] std::int32_t GetRowPitch(
    BoardStageLayout theLayout)
{
    return theLayout == BoardStageLayout::Day
        ? kDayRowPitch
        : kPoolRoofRowPitch;
}

[[nodiscard]] std::int32_t GetRoofHitOffset(
    std::uint8_t theColumn)
{
    if (theColumn >= 5)
        return 0;
    return
        (4 - static_cast<std::int32_t>(theColumn)) * 20;
}

[[nodiscard]] std::int32_t GetCellOriginY(
    BoardStageLayout theLayout,
    std::uint8_t theColumn,
    std::uint8_t theRow)
{
    auto anOrigin =
        kLawnOriginY +
        static_cast<std::int32_t>(theRow) *
            GetRowPitch(theLayout);
    if (theLayout == BoardStageLayout::Roof)
        anOrigin += GetRoofHitOffset(theColumn);
    return anOrigin;
}

} // namespace

BoardDimensions BoardGeometry::GetDimensions(
    BoardStageLayout theLayout)
{
    switch (theLayout)
    {
    case BoardStageLayout::Day:
    case BoardStageLayout::Roof:
        return {kColumnCount, kDayRoofRowCount};
    case BoardStageLayout::Pool:
        return {kColumnCount, kPoolRowCount};
    case BoardStageLayout::Count:
        return {};
    }
    return {};
}

engine::PointI BoardGeometry::GridToPixel(
    BoardStageLayout theLayout,
    std::uint8_t theColumn,
    std::uint8_t theRow)
{
    const auto aDimensions = GetDimensions(theLayout);
    if (theColumn >= aDimensions.mColumnCount ||
        theRow >= aDimensions.mRowCount)
    {
        return {};
    }

    auto aY =
        kLawnOriginY +
        static_cast<std::int32_t>(theRow) *
            GetRowPitch(theLayout);
    if (theLayout == BoardStageLayout::Roof)
    {
        const auto aSlopeOffset =
            theColumn < 5
            ? (5 - static_cast<std::int32_t>(theColumn)) * 20
            : 0;
        aY += aSlopeOffset - 10;
    }
    return {
        kLawnOriginX +
            static_cast<std::int32_t>(theColumn) * kColumnPitch,
        aY,
    };
}

engine::RectI BoardGeometry::GetCellRect(
    BoardStageLayout theLayout,
    std::uint8_t theColumn,
    std::uint8_t theRow)
{
    const auto aDimensions = GetDimensions(theLayout);
    if (theColumn >= aDimensions.mColumnCount ||
        theRow >= aDimensions.mRowCount)
    {
        return {};
    }

    const auto aRowPitch = GetRowPitch(theLayout);
    return {
        .mOrigin =
            {
                kLawnOriginX +
                    static_cast<std::int32_t>(theColumn) *
                        kColumnPitch,
                GetCellOriginY(theLayout, theColumn, theRow),
            },
        .mSize =
            {
                static_cast<std::uint32_t>(kColumnPitch),
                static_cast<std::uint32_t>(aRowPitch),
            },
    };
}

bool BoardGeometry::TryPixelToGrid(
    BoardStageLayout theLayout,
    engine::PointI thePosition,
    GridCoordinate& theCoordinate)
{
    const auto aDimensions = GetDimensions(theLayout);
    if (aDimensions.mColumnCount == 0 ||
        thePosition.mX < kLawnOriginX)
    {
        return false;
    }

    const auto aColumn =
        (thePosition.mX - kLawnOriginX) / kColumnPitch;
    if (aColumn < 0 ||
        aColumn >=
            static_cast<std::int32_t>(
                aDimensions.mColumnCount))
    {
        return false;
    }

    auto anAdjustedY = thePosition.mY;
    if (theLayout == BoardStageLayout::Roof)
    {
        anAdjustedY -= GetRoofHitOffset(
            static_cast<std::uint8_t>(aColumn));
    }
    if (anAdjustedY < kLawnOriginY)
        return false;

    const auto aRow =
        (anAdjustedY - kLawnOriginY) /
        GetRowPitch(theLayout);
    if (aRow < 0 ||
        aRow >=
            static_cast<std::int32_t>(
                aDimensions.mRowCount))
    {
        return false;
    }

    theCoordinate = {
        static_cast<std::uint8_t>(aColumn),
        static_cast<std::uint8_t>(aRow),
    };
    return true;
}

} // namespace pvz::game
