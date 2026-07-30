#include "pvz/game/GameFlow.h"

#include <array>
#include <cstddef>
#include <span>

extern void Expect(bool theCondition, const char* theMessage);

namespace
{

class TestInputFrame final : public pvz::engine::IInputFrame
{
public:
    void Clear()
    {
        mPressedKeys.fill(false);
        mPressedButtons.fill(false);
    }

    void PressKey(pvz::engine::KeyCode theKey)
    {
        mPressedKeys[static_cast<std::size_t>(theKey)] = true;
    }

    void PressPointer(
        pvz::engine::PointI thePosition,
        pvz::engine::PointerButton theButton =
            pvz::engine::PointerButton::Primary)
    {
        mPointer.mPosition = thePosition;
        mPressedButtons[static_cast<std::size_t>(theButton)] = true;
    }

    void MovePointer(pvz::engine::PointI thePosition)
    {
        mPointer.mPosition = thePosition;
    }

    [[nodiscard]] bool IsKeyDown(
        pvz::engine::KeyCode theKey) const override
    {
        static_cast<void>(theKey);
        return false;
    }

    [[nodiscard]] bool WasKeyPressed(
        pvz::engine::KeyCode theKey) const override
    {
        return mPressedKeys[static_cast<std::size_t>(theKey)];
    }

    [[nodiscard]] bool IsPointerButtonDown(
        pvz::engine::PointerButton theButton) const override
    {
        static_cast<void>(theButton);
        return false;
    }

    [[nodiscard]] bool WasPointerButtonPressed(
        pvz::engine::PointerButton theButton) const override
    {
        return mPressedButtons[static_cast<std::size_t>(theButton)];
    }

    [[nodiscard]] pvz::engine::PointerState GetPointerState()
        const override
    {
        return mPointer;
    }

    [[nodiscard]] std::span<const char32_t> GetTextInput() const override
    {
        return {};
    }

private:
    std::array<
        bool,
        static_cast<std::size_t>(pvz::engine::KeyCode::Count)>
        mPressedKeys{};
    std::array<
        bool,
        static_cast<std::size_t>(pvz::engine::PointerButton::Count)>
        mPressedButtons{};
    pvz::engine::PointerState mPointer{};
};

void EnterMainMenu(
    pvz::game::GameFlow& theFlow,
    TestInputFrame& theInput)
{
    theInput.PressKey(pvz::engine::KeyCode::Enter);
    theFlow.Update(theInput);
    theInput.Clear();
}

void EnterAdventureDay(
    pvz::game::GameFlow& theFlow,
    TestInputFrame& theInput)
{
    EnterMainMenu(theFlow, theInput);
    theInput.PressKey(pvz::engine::KeyCode::Enter);
    theFlow.Update(theInput);
    theInput.Clear();
    for (std::uint16_t aTick = 0; aTick < 1'305; ++aTick)
        theFlow.Update(theInput);
}

void TestTitleAndMenuInteraction()
{
    pvz::game::GameFlow aFlow;
    TestInputFrame anInput;

    Expect(
        aFlow.GetScene() == pvz::game::GameScene::Title,
        "game flow starts at title");
    EnterMainMenu(aFlow, anInput);
    Expect(
        aFlow.GetScene() == pvz::game::GameScene::MainMenu,
        "enter opens main menu");
    Expect(
        aFlow.GetSelectedMenuItem() ==
            pvz::game::MainMenuItem::Adventure,
        "adventure starts selected");

    anInput.PressKey(pvz::engine::KeyCode::ArrowDown);
    aFlow.Update(anInput);
    anInput.Clear();
    Expect(
        aFlow.GetSelectedMenuItem() ==
            pvz::game::MainMenuItem::Minigames,
        "down selects next menu item");

    anInput.PressKey(pvz::engine::KeyCode::Enter);
    aFlow.Update(anInput);
    anInput.Clear();
    Expect(
        aFlow.GetScene() == pvz::game::GameScene::MainMenu &&
            aFlow.GetNoticeTicks() == 150,
        "unavailable mode stays in menu and shows notice");

    const auto aPuzzleRect = pvz::game::GameFlow::GetMenuItemRect(
        pvz::game::MainMenuItem::Puzzle);
    anInput.MovePointer(
        {
            aPuzzleRect.mOrigin.mX +
                static_cast<std::int32_t>(
                    aPuzzleRect.mSize.mWidth / 2),
            aPuzzleRect.mOrigin.mY +
                static_cast<std::int32_t>(
                    aPuzzleRect.mSize.mHeight / 2),
        });
    aFlow.Update(anInput);
    Expect(
        aFlow.GetSelectedMenuItem() ==
            pvz::game::MainMenuItem::Puzzle,
        "pointer hover selects menu item");

    anInput.PressKey(pvz::engine::KeyCode::ArrowUp);
    aFlow.Update(anInput);
    anInput.Clear();
    Expect(
        aFlow.GetSelectedMenuItem() ==
            pvz::game::MainMenuItem::Minigames,
        "up selects previous menu item");
    aFlow.Update(anInput);
    Expect(
        aFlow.GetSelectedMenuItem() ==
            pvz::game::MainMenuItem::Minigames,
        "stationary pointer does not override keyboard selection");
}

void TestAdventureTransition()
{
    pvz::game::GameFlow aFlow;
    TestInputFrame anInput;
    EnterMainMenu(aFlow, anInput);

    const auto anAdventureRect = pvz::game::GameFlow::GetMenuItemRect(
        pvz::game::MainMenuItem::Adventure);
    anInput.PressPointer(
        {
            anAdventureRect.mOrigin.mX + 10,
            anAdventureRect.mOrigin.mY + 10,
        });
    aFlow.Update(anInput);
    anInput.Clear();
    Expect(
        aFlow.GetScene() ==
                pvz::game::GameScene::StartingAdventure &&
            aFlow.GetTransitionTicks() == 450,
        "adventure click starts legacy selector transition");

    for (std::uint16_t aTick = 0; aTick < 449; ++aTick)
        aFlow.Update(anInput);
    Expect(
        aFlow.GetScene() ==
                pvz::game::GameScene::StartingAdventure &&
            aFlow.GetTransitionTicks() == 1,
        "selector transition remains active before final tick");
    aFlow.Update(anInput);
    Expect(
        aFlow.GetScene() ==
                pvz::game::GameScene::AdventureIntro &&
            aFlow.GetTransitionTicks() == 855 &&
            aFlow.GetGridColumn() == 0xFF &&
            aFlow.GetGridRow() == 0xFF,
        "selector transition enters the first-level intro");

    for (std::uint16_t aTick = 0; aTick < 854; ++aTick)
        aFlow.Update(anInput);
    Expect(
        aFlow.GetScene() ==
                pvz::game::GameScene::AdventureIntro &&
            aFlow.GetTransitionTicks() == 1,
        "first-level intro remains active before final tick");
    aFlow.Update(anInput);
    Expect(
        aFlow.GetScene() ==
                pvz::game::GameScene::AdventureDay &&
            aFlow.GetGridColumn() == 0 &&
            aFlow.GetGridRow() == 0,
        "first-level intro enters playable day board");
}

void TestBoardInteraction()
{
    pvz::game::GameFlow aFlow;
    TestInputFrame anInput;
    EnterAdventureDay(aFlow, anInput);

    const auto aCellRect = pvz::game::GameFlow::GetGridCellRect(3, 2);
    anInput.PressPointer(
        {
            aCellRect.mOrigin.mX + 10,
            aCellRect.mOrigin.mY + 10,
        });
    aFlow.Update(anInput);
    anInput.Clear();
    Expect(
        aFlow.GetGridColumn() == 3 &&
            aFlow.GetGridRow() == 2 &&
            aFlow.IsGridCellOccupied(3, 2),
        "board click selects and toggles a portable grid cell");

    anInput.PressKey(pvz::engine::KeyCode::Enter);
    aFlow.Update(anInput);
    anInput.Clear();
    Expect(
        !aFlow.IsGridCellOccupied(3, 2),
        "enter toggles selected grid cell");

    anInput.PressKey(pvz::engine::KeyCode::ArrowLeft);
    aFlow.Update(anInput);
    anInput.Clear();
    Expect(
        aFlow.GetGridColumn() == 2 &&
            aFlow.GetGridRow() == 2,
        "arrow keys move grid selection");

    anInput.PressKey(pvz::engine::KeyCode::Escape);
    aFlow.Update(anInput);
    Expect(
        aFlow.GetScene() == pvz::game::GameScene::MainMenu,
        "escape returns from board to menu");
}

void TestStateValidation()
{
    pvz::game::GameFlow aFlow;
    TestInputFrame anInput;
    EnterAdventureDay(aFlow, anInput);
    anInput.PressKey(pvz::engine::KeyCode::Space);
    aFlow.Update(anInput);
    anInput.Clear();

    const auto aState = aFlow.GetState();
    pvz::game::GameFlow aRestoredFlow;
    Expect(
        aRestoredFlow.RestoreState(aState),
        "valid game flow state restores");
    Expect(
        aRestoredFlow.GetScene() ==
                pvz::game::GameScene::AdventureDay &&
            aRestoredFlow.IsGridCellOccupied(0, 0),
        "game flow state round-trips fixed-width board data");

    auto anInvalidState = aState;
    anInvalidState.mScene = pvz::game::GameScene::Count;
    Expect(
        !aRestoredFlow.RestoreState(anInvalidState),
        "invalid game scene is rejected");
    anInvalidState = aState;
    anInvalidState.mOccupiedCells = std::uint64_t{1} << 63U;
    Expect(
        !aRestoredFlow.RestoreState(anInvalidState),
        "out-of-range occupancy bits are rejected");
    anInvalidState = aState;
    anInvalidState.mGridColumn = 9;
    Expect(
        !aRestoredFlow.RestoreState(anInvalidState),
        "invalid grid coordinate is rejected");
    anInvalidState = aState;
    anInvalidState.mNoticeTicks = 151;
    Expect(
        !aRestoredFlow.RestoreState(anInvalidState),
        "invalid notice duration is rejected");
    anInvalidState = aState;
    anInvalidState.mScene = pvz::game::GameScene::AdventureIntro;
    anInvalidState.mTransitionTicks = 856;
    Expect(
        !aRestoredFlow.RestoreState(anInvalidState),
        "invalid first-level intro duration is rejected");
    Expect(
        aRestoredFlow.GetState().mOccupiedCells ==
            aState.mOccupiedCells,
        "rejected state does not modify game flow");
}

} // namespace

void RunGameFlowTests()
{
    TestTitleAndMenuInteraction();
    TestAdventureTransition();
    TestBoardInteraction();
    TestStateValidation();
}
