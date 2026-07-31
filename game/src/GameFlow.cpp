#include "pvz/game/GameFlow.h"

#include "pvz/game/BoardGeometry.h"

#include <array>
#include <cstddef>

namespace pvz::game
{
namespace
{

inline constexpr std::uint8_t kBoardColumnCount = 9;
inline constexpr std::uint8_t kBoardRowCount = 5;
inline constexpr std::uint8_t kNoGridCoordinate = 0xFF;
inline constexpr std::uint16_t kAdventureTransitionTicks = 450;
inline constexpr std::uint16_t kAdventureIntroTicks = 855;
inline constexpr std::uint16_t kUnavailableNoticeTicks = 150;
inline constexpr std::uint64_t kOccupiedCellMask =
    (std::uint64_t{1} << 45U) - 1U;

struct MenuItemLayout
{
    MainMenuItem mItem;
    engine::RectI mRect;
};

inline constexpr std::array<MenuItemLayout, 4> kMenuItemLayouts{{
    {
        .mItem = MainMenuItem::Adventure,
        .mRect =
            {
                .mOrigin = {445, 45},
                .mSize = {331, 146},
            },
    },
    {
        .mItem = MainMenuItem::Minigames,
        .mRect =
            {
                .mOrigin = {459, 178},
                .mSize = {313, 130},
            },
    },
    {
        .mItem = MainMenuItem::Puzzle,
        .mRect =
            {
                .mOrigin = {481, 295},
                .mSize = {284, 121},
            },
    },
    {
        .mItem = MainMenuItem::Survival,
        .mRect =
            {
                .mOrigin = {504, 405},
                .mSize = {267, 124},
            },
    },
}};

[[nodiscard]] bool Contains(
    const engine::RectI& theRect,
    engine::PointI thePoint)
{
    const auto aRight =
        theRect.mOrigin.mX +
        static_cast<std::int32_t>(theRect.mSize.mWidth);
    const auto aBottom =
        theRect.mOrigin.mY +
        static_cast<std::int32_t>(theRect.mSize.mHeight);
    return
        thePoint.mX >= theRect.mOrigin.mX &&
        thePoint.mX < aRight &&
        thePoint.mY >= theRect.mOrigin.mY &&
        thePoint.mY < aBottom;
}

[[nodiscard]] bool WasActivated(
    const engine::IInputFrame& theInput)
{
    return
        theInput.WasKeyPressed(engine::KeyCode::Enter) ||
        theInput.WasKeyPressed(engine::KeyCode::Space);
}

} // namespace

void GameFlow::Reset()
{
    mState = {};
    mLastPointerPosition = {};
    mHasPointerPosition = false;
    mGridActivationRequested = false;
}

void GameFlow::Update(const engine::IInputFrame& theInput)
{
    mGridActivationRequested = false;
    if (mState.mNoticeTicks > 0)
        --mState.mNoticeTicks;

    switch (mState.mScene)
    {
    case GameScene::Title:
        UpdateTitle(theInput);
        break;
    case GameScene::MainMenu:
        UpdateMainMenu(theInput);
        break;
    case GameScene::StartingAdventure:
        UpdateStartingAdventure(theInput);
        break;
    case GameScene::AdventureIntro:
        UpdateAdventureIntro(theInput);
        break;
    case GameScene::AdventureDay:
        UpdateAdventureDay(theInput);
        break;
    case GameScene::Count:
        break;
    }
}

GameScene GameFlow::GetScene() const
{
    return mState.mScene;
}

MainMenuItem GameFlow::GetSelectedMenuItem() const
{
    return mState.mMenuItem;
}

bool GameFlow::IsMenuItemAvailable(
    MainMenuItem theItem) const
{
    return theItem == MainMenuItem::Adventure;
}

std::uint16_t GameFlow::GetTransitionTicks() const
{
    return mState.mTransitionTicks;
}

std::uint16_t GameFlow::GetNoticeTicks() const
{
    return mState.mNoticeTicks;
}

std::uint8_t GameFlow::GetGridColumn() const
{
    return mState.mGridColumn;
}

std::uint8_t GameFlow::GetGridRow() const
{
    return mState.mGridRow;
}

bool GameFlow::WasGridActivationRequested() const
{
    return mGridActivationRequested;
}

bool GameFlow::IsGridCellOccupied(
    std::uint8_t theColumn,
    std::uint8_t theRow) const
{
    if (theColumn >= kBoardColumnCount ||
        theRow >= kBoardRowCount)
    {
        return false;
    }
    const auto anIndex =
        static_cast<std::uint32_t>(theRow) *
            kBoardColumnCount +
        theColumn;
    return
        (mState.mOccupiedCells &
         (std::uint64_t{1} << anIndex)) != 0;
}

void GameFlow::SetOccupiedCells(
    std::uint64_t theOccupiedCells)
{
    mState.mOccupiedCells =
        theOccupiedCells & kOccupiedCellMask;
}

GameFlowState GameFlow::GetState() const
{
    return mState;
}

bool GameFlow::RestoreState(const GameFlowState& theState)
{
    if (theState.mScene >= GameScene::Count ||
        theState.mMenuItem >= MainMenuItem::Count ||
        theState.mNoticeTicks > kUnavailableNoticeTicks ||
        (theState.mOccupiedCells & ~kOccupiedCellMask) != 0)
    {
        return false;
    }
    const bool hasValidTransition =
        (theState.mScene == GameScene::StartingAdventure &&
         theState.mTransitionTicks <=
             kAdventureTransitionTicks) ||
        (theState.mScene == GameScene::AdventureIntro &&
         theState.mTransitionTicks <= kAdventureIntroTicks) ||
        ((theState.mScene != GameScene::StartingAdventure &&
          theState.mScene != GameScene::AdventureIntro) &&
         theState.mTransitionTicks == 0);
    if (!hasValidTransition)
        return false;

    const bool hasNoGridSelection =
        theState.mGridColumn == kNoGridCoordinate &&
        theState.mGridRow == kNoGridCoordinate;
    const bool hasValidGridSelection =
        theState.mGridColumn < kBoardColumnCount &&
        theState.mGridRow < kBoardRowCount;
    if (!hasNoGridSelection && !hasValidGridSelection)
        return false;

    mState = theState;
    mLastPointerPosition = {};
    mHasPointerPosition = false;
    mGridActivationRequested = false;
    return true;
}

engine::RectI GameFlow::GetMenuItemRect(
    MainMenuItem theItem)
{
    for (const auto& aLayout : kMenuItemLayouts)
    {
        if (aLayout.mItem == theItem)
            return aLayout.mRect;
    }
    return {};
}

engine::RectI GameFlow::GetGridCellRect(
    std::uint8_t theColumn,
    std::uint8_t theRow)
{
    return BoardGeometry::GetCellRect(
        BoardStageLayout::Day,
        theColumn,
        theRow);
}

void GameFlow::UpdateTitle(
    const engine::IInputFrame& theInput)
{
    if (WasActivated(theInput) ||
        theInput.WasPointerButtonPressed(
            engine::PointerButton::Primary))
    {
        mState.mScene = GameScene::MainMenu;
        mHasPointerPosition = false;
    }
}

void GameFlow::UpdateMainMenu(
    const engine::IInputFrame& theInput)
{
    const auto aPointer = theInput.GetPointerState().mPosition;
    const bool isPrimaryPressed =
        theInput.WasPointerButtonPressed(
            engine::PointerButton::Primary);
    const bool hasPointerMoved =
        !mHasPointerPosition ||
        aPointer.mX != mLastPointerPosition.mX ||
        aPointer.mY != mLastPointerPosition.mY;
    if (hasPointerMoved || isPrimaryPressed)
    {
        for (const auto& aLayout : kMenuItemLayouts)
        {
            if (Contains(aLayout.mRect, aPointer))
            {
                mState.mMenuItem = aLayout.mItem;
                break;
            }
        }
    }
    mLastPointerPosition = aPointer;
    mHasPointerPosition = true;

    if (theInput.WasKeyPressed(engine::KeyCode::ArrowUp))
        SelectRelative(-1);
    else if (theInput.WasKeyPressed(engine::KeyCode::ArrowDown))
        SelectRelative(1);

    if (WasActivated(theInput))
    {
        ActivateMenuItem();
        return;
    }
    if (isPrimaryPressed)
    {
        const auto aSelectedRect =
            GetMenuItemRect(mState.mMenuItem);
        if (Contains(aSelectedRect, aPointer))
            ActivateMenuItem();
    }
}

void GameFlow::UpdateStartingAdventure(
    const engine::IInputFrame& theInput)
{
    mLastPointerPosition =
        theInput.GetPointerState().mPosition;
    mHasPointerPosition = true;
    if (mState.mTransitionTicks > 0)
        --mState.mTransitionTicks;
    if (mState.mTransitionTicks == 0)
    {
        mState.mScene = GameScene::AdventureIntro;
        mState.mTransitionTicks = kAdventureIntroTicks;
    }
}

void GameFlow::UpdateAdventureIntro(
    const engine::IInputFrame& theInput)
{
    mLastPointerPosition =
        theInput.GetPointerState().mPosition;
    mHasPointerPosition = true;
    if (mState.mTransitionTicks > 0)
        --mState.mTransitionTicks;
    if (mState.mTransitionTicks == 0)
    {
        mState.mScene = GameScene::AdventureDay;
        SelectGridCell(mLastPointerPosition, false);
        if (mState.mGridColumn == kNoGridCoordinate ||
            mState.mGridRow == kNoGridCoordinate)
        {
            mState.mGridColumn = 0;
            mState.mGridRow = 0;
        }
    }
}

void GameFlow::UpdateAdventureDay(
    const engine::IInputFrame& theInput)
{
    if (theInput.WasKeyPressed(engine::KeyCode::Escape))
    {
        mState.mScene = GameScene::MainMenu;
        mHasPointerPosition = false;
        return;
    }

    if (theInput.WasPointerButtonPressed(
            engine::PointerButton::Primary))
    {
        SelectGridCell(
            theInput.GetPointerState().mPosition,
            true);
    }
    if (theInput.WasKeyPressed(engine::KeyCode::ArrowLeft))
        MoveGridSelection(-1, 0);
    else if (theInput.WasKeyPressed(engine::KeyCode::ArrowRight))
        MoveGridSelection(1, 0);
    if (theInput.WasKeyPressed(engine::KeyCode::ArrowUp))
        MoveGridSelection(0, -1);
    else if (theInput.WasKeyPressed(engine::KeyCode::ArrowDown))
        MoveGridSelection(0, 1);
    if (WasActivated(theInput))
        RequestSelectedGridCellActivation();
}

void GameFlow::SelectRelative(std::int32_t theOffset)
{
    const auto aCount =
        static_cast<std::int32_t>(MainMenuItem::Count);
    auto anItem =
        static_cast<std::int32_t>(mState.mMenuItem);
    anItem = (anItem + theOffset + aCount) % aCount;
    mState.mMenuItem =
        static_cast<MainMenuItem>(anItem);
}

void GameFlow::ActivateMenuItem()
{
    if (!IsMenuItemAvailable(mState.mMenuItem))
    {
        mState.mNoticeTicks = kUnavailableNoticeTicks;
        return;
    }
    mState.mScene = GameScene::StartingAdventure;
    mState.mTransitionTicks = kAdventureTransitionTicks;
    mState.mNoticeTicks = 0;
    mState.mGridColumn = kNoGridCoordinate;
    mState.mGridRow = kNoGridCoordinate;
    mState.mOccupiedCells = 0;
}

void GameFlow::SelectGridCell(
    engine::PointI thePosition,
    bool theRequestActivation)
{
    GridCoordinate aCoordinate;
    if (!BoardGeometry::TryPixelToGrid(
            BoardStageLayout::Day,
            thePosition,
            aCoordinate))
    {
        return;
    }
    mState.mGridColumn = aCoordinate.mColumn;
    mState.mGridRow = aCoordinate.mRow;
    if (theRequestActivation)
        RequestSelectedGridCellActivation();
}

void GameFlow::MoveGridSelection(
    std::int32_t theColumnOffset,
    std::int32_t theRowOffset)
{
    if (mState.mGridColumn == kNoGridCoordinate ||
        mState.mGridRow == kNoGridCoordinate)
    {
        mState.mGridColumn = 0;
        mState.mGridRow = 0;
    }
    const auto aColumn =
        static_cast<std::int32_t>(mState.mGridColumn);
    const auto aRow =
        static_cast<std::int32_t>(mState.mGridRow);
    mState.mGridColumn = static_cast<std::uint8_t>(
        (aColumn + theColumnOffset + kBoardColumnCount) %
        kBoardColumnCount);
    mState.mGridRow = static_cast<std::uint8_t>(
        (aRow + theRowOffset + kBoardRowCount) %
        kBoardRowCount);
}

void GameFlow::RequestSelectedGridCellActivation()
{
    if (mState.mGridColumn >= kBoardColumnCount ||
        mState.mGridRow >= kBoardRowCount)
    {
        return;
    }
    mGridActivationRequested = true;
}

} // namespace pvz::game
