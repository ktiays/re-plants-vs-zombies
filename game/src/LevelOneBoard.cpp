#include "pvz/game/LevelOneBoard.h"

namespace pvz::game
{
namespace
{

inline constexpr std::uint8_t kColumnCount = 9;
inline constexpr std::uint8_t kRowCount = 5;
inline constexpr std::uint16_t kMaximumSun = 9'990;

} // namespace

void LevelOneBoard::Reset()
{
    mState = {};
}

void LevelOneBoard::Update()
{
    if (!mState.mSeedRefreshing)
        return;

    ++mState.mSeedRefreshCounter;
    if (mState.mSeedRefreshCounter >
        kPeashooterRefreshTime)
    {
        mState.mSeedRefreshCounter = 0;
        mState.mSeedRefreshing = false;
    }
}

LevelOneSeedSelectionResult
LevelOneBoard::SelectPeashooter()
{
    if (IsPeashooterSelected())
    {
        return
            LevelOneSeedSelectionResult::AlreadySelected;
    }
    if (mState.mSeedRefreshing)
        return LevelOneSeedSelectionResult::Refreshing;
    if (mState.mSun < kPeashooterCost)
    {
        return
            LevelOneSeedSelectionResult::InsufficientSun;
    }

    mState.mSeedSelection =
        LevelOneSeedSelection::Peashooter;
    return LevelOneSeedSelectionResult::Selected;
}

LevelOnePlacementResult LevelOneBoard::PlacePeashooter(
    std::uint8_t theColumn,
    std::uint8_t theRow,
    std::uint64_t& theOccupiedCells)
{
    if (!IsPeashooterSelected())
        return LevelOnePlacementResult::NoSeedSelected;
    if (!IsPlantableCell(theColumn, theRow))
        return LevelOnePlacementResult::InvalidCell;

    const auto anIndex =
        static_cast<std::uint32_t>(theRow) *
            kColumnCount +
        theColumn;
    const auto aCellMask =
        std::uint64_t{1} << anIndex;
    if ((theOccupiedCells & aCellMask) != 0)
        return LevelOnePlacementResult::Occupied;
    if (mState.mSun < kPeashooterCost)
        return LevelOnePlacementResult::InsufficientSun;

    theOccupiedCells |= aCellMask;
    mState.mSun = static_cast<std::uint16_t>(
        mState.mSun - kPeashooterCost);
    mState.mSeedRefreshCounter = 0;
    mState.mSeedRefreshing = true;
    mState.mSeedSelection =
        LevelOneSeedSelection::None;
    return LevelOnePlacementResult::Planted;
}

void LevelOneBoard::AddSun(std::uint16_t theAmount)
{
    const auto aSun = static_cast<std::uint32_t>(
        mState.mSun) + theAmount;
    mState.mSun = static_cast<std::uint16_t>(
        aSun > kMaximumSun ? kMaximumSun : aSun);
}

void LevelOneBoard::CancelSelection()
{
    mState.mSeedSelection =
        LevelOneSeedSelection::None;
}

LevelOneBoardState LevelOneBoard::GetState() const
{
    return mState;
}

bool LevelOneBoard::RestoreState(
    const LevelOneBoardState& theState)
{
    if (theState.mSun > kMaximumSun ||
        theState.mSeedSelection >=
            LevelOneSeedSelection::Count ||
        (theState.mSeedRefreshing &&
         theState.mSeedRefreshCounter >
             kPeashooterRefreshTime) ||
        (!theState.mSeedRefreshing &&
         theState.mSeedRefreshCounter != 0) ||
        (theState.mSeedSelection ==
             LevelOneSeedSelection::Peashooter &&
         (theState.mSeedRefreshing ||
          theState.mSun < kPeashooterCost)))
    {
        return false;
    }

    mState = theState;
    return true;
}

bool LevelOneBoard::CanSelectPeashooter() const
{
    return
        !mState.mSeedRefreshing &&
        mState.mSun >= kPeashooterCost;
}

bool LevelOneBoard::IsPeashooterSelected() const
{
    return
        mState.mSeedSelection ==
        LevelOneSeedSelection::Peashooter;
}

bool LevelOneBoard::IsPlantableCell(
    std::uint8_t theColumn,
    std::uint8_t theRow)
{
    return
        theColumn < kColumnCount &&
        theRow < kRowCount &&
        theRow == kPlantableRow;
}

engine::RectI LevelOneBoard::GetSeedPacketRect()
{
    return {
        .mOrigin = {95, 8},
        .mSize = {50, 70},
    };
}

} // namespace pvz::game
