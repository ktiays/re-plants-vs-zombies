#include "pvz/game/LevelOneBoard.h"

#include <cstdint>

extern void Expect(bool theCondition, const char* theMessage);

namespace
{

void TestLevelOneSourceAuditedConstants()
{
    Expect(
        pvz::game::LevelOneBoard::kInitialSun == 150,
        "level one starts with legacy first-run sun");
    Expect(
        pvz::game::LevelOneBoard::kPeashooterCost == 100,
        "Peashooter uses the legacy sun cost");
    Expect(
        pvz::game::LevelOneBoard::kPeashooterRefreshTime == 750,
        "Peashooter uses the legacy refresh time");
    Expect(
        pvz::game::LevelOneBoard::kPlantableRow == 2,
        "first level exposes only the center lawn row");

    const auto aPacketRect =
        pvz::game::LevelOneBoard::GetSeedPacketRect();
    Expect(
        aPacketRect.mOrigin.mX == 95 &&
            aPacketRect.mOrigin.mY == 8 &&
            aPacketRect.mSize.mWidth == 50 &&
            aPacketRect.mSize.mHeight == 70,
        "first packet follows the legacy seed-bank layout");
}

void TestSelectionPlacementAndEconomy()
{
    pvz::game::LevelOneBoard aBoard;
    std::uint64_t anOccupiedCells{};

    Expect(
        aBoard.GetState().mSun == 150 &&
            aBoard.CanSelectPeashooter(),
        "level one economy resets deterministically");
    Expect(
        aBoard.PlacePeashooter(3, 2, anOccupiedCells) ==
            pvz::game::LevelOnePlacementResult::
                NoSeedSelected,
        "planting requires a selected packet");
    Expect(
        aBoard.SelectPeashooter() ==
            pvz::game::LevelOneSeedSelectionResult::Selected &&
            aBoard.IsPeashooterSelected(),
        "ready Peashooter packet can be selected");
    Expect(
        aBoard.PlacePeashooter(3, 1, anOccupiedCells) ==
                pvz::game::LevelOnePlacementResult::InvalidCell &&
            aBoard.IsPeashooterSelected() &&
            anOccupiedCells == 0,
        "first-level dirt rows reject planting without clearing cursor");
    Expect(
        aBoard.PlacePeashooter(3, 2, anOccupiedCells) ==
                pvz::game::LevelOnePlacementResult::Planted &&
            anOccupiedCells ==
                (std::uint64_t{1} << 21U),
        "center-row placement occupies the selected cell");

    const auto aState = aBoard.GetState();
    Expect(
        aState.mSun == 50 &&
            aState.mSeedRefreshing &&
            aState.mSeedRefreshCounter == 0 &&
            !aBoard.IsPeashooterSelected(),
        "placement spends sun and starts packet refresh");
    Expect(
        aBoard.SelectPeashooter() ==
            pvz::game::LevelOneSeedSelectionResult::Refreshing,
        "refreshing packet cannot be selected");
}

void TestLegacyRefreshBoundary()
{
    pvz::game::LevelOneBoard aBoard;
    std::uint64_t anOccupiedCells{};
    static_cast<void>(aBoard.SelectPeashooter());
    static_cast<void>(
        aBoard.PlacePeashooter(0, 2, anOccupiedCells));

    for (std::uint16_t aTick = 0;
         aTick <
             pvz::game::LevelOneBoard::
                 kPeashooterRefreshTime;
         ++aTick)
    {
        aBoard.Update();
    }
    Expect(
        aBoard.GetState().mSeedRefreshing &&
            aBoard.GetState().mSeedRefreshCounter == 750,
        "packet remains inactive at the legacy refresh-time value");
    aBoard.Update();
    Expect(
        !aBoard.GetState().mSeedRefreshing &&
            aBoard.GetState().mSeedRefreshCounter == 0,
        "packet activates when the legacy counter exceeds refresh time");
    Expect(
        aBoard.SelectPeashooter() ==
            pvz::game::LevelOneSeedSelectionResult::
                InsufficientSun,
        "ready packet still enforces available sun");
}

void TestStateValidationIsTransactional()
{
    pvz::game::LevelOneBoard aBoard;
    auto aState = aBoard.GetState();
    aState.mSun = 125;
    Expect(
        aBoard.RestoreState(aState) &&
            aBoard.GetState().mSun == 125,
        "valid Level 1 state restores");

    auto anInvalidState = aState;
    anInvalidState.mSeedRefreshing = true;
    anInvalidState.mSeedRefreshCounter = 751;
    Expect(
        !aBoard.RestoreState(anInvalidState),
        "out-of-range refresh counter is rejected");
    anInvalidState = aState;
    anInvalidState.mSeedSelection =
        pvz::game::LevelOneSeedSelection::Count;
    Expect(
        !aBoard.RestoreState(anInvalidState),
        "invalid seed selection is rejected");
    Expect(
        aBoard.GetState().mSun == 125,
        "rejected Level 1 state is transactional");
}

} // namespace

void RunLevelOneBoardTests()
{
    TestLevelOneSourceAuditedConstants();
    TestSelectionPlacementAndEconomy();
    TestLegacyRefreshBoundary();
    TestStateValidationIsTransactional();
}
