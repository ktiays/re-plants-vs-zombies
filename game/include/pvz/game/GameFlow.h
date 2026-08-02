#pragma once

#include "pvz/engine/Input.h"

#include <cstdint>

namespace pvz::game
{

enum class GameScene : std::uint8_t
{
    Title,
    MainMenu,
    StartingAdventure,
    AdventureDay,
    AdventureIntro,
    Count,
};

enum class MainMenuItem : std::uint8_t
{
    Adventure,
    Minigames,
    Puzzle,
    Survival,
    Count,
};

struct GameFlowState
{
    GameScene mScene{GameScene::Title};
    MainMenuItem mMenuItem{MainMenuItem::Adventure};
    std::uint16_t mTransitionTicks{};
    std::uint16_t mNoticeTicks{};
    std::uint8_t mGridColumn{0xFF};
    std::uint8_t mGridRow{0xFF};
    std::uint64_t mOccupiedCells{};
    bool mHasPointerPosition{};
    engine::PointI mLastPointerPosition{};
};

class GameFlow
{
public:
    void Reset();
    void Update(const engine::IInputFrame& theInput);

    [[nodiscard]] GameScene GetScene() const;
    [[nodiscard]] MainMenuItem GetSelectedMenuItem() const;
    [[nodiscard]] bool IsMenuItemAvailable(
        MainMenuItem theItem) const;
    [[nodiscard]] std::uint16_t GetTransitionTicks() const;
    [[nodiscard]] std::uint16_t GetNoticeTicks() const;
    [[nodiscard]] std::uint8_t GetGridColumn() const;
    [[nodiscard]] std::uint8_t GetGridRow() const;
    [[nodiscard]] bool WasGridActivationRequested() const;
    [[nodiscard]] bool IsGridCellOccupied(
        std::uint8_t theColumn,
        std::uint8_t theRow) const;
    void SetOccupiedCells(std::uint64_t theOccupiedCells);
    [[nodiscard]] GameFlowState GetState() const;
    [[nodiscard]] bool RestoreState(const GameFlowState& theState);

    [[nodiscard]] static engine::RectI GetMenuItemRect(
        MainMenuItem theItem);
    [[nodiscard]] static engine::RectI GetGridCellRect(
        std::uint8_t theColumn,
        std::uint8_t theRow);

private:
    void UpdateTitle(const engine::IInputFrame& theInput);
    void UpdateMainMenu(const engine::IInputFrame& theInput);
    void UpdateStartingAdventure(
        const engine::IInputFrame& theInput);
    void UpdateAdventureIntro(
        const engine::IInputFrame& theInput);
    void UpdateAdventureDay(const engine::IInputFrame& theInput);
    void SelectRelative(std::int32_t theOffset);
    void ActivateMenuItem();
    void SelectGridCell(
        engine::PointI thePosition,
        bool theRequestActivation);
    void MoveGridSelection(
        std::int32_t theColumnOffset,
        std::int32_t theRowOffset);
    void RequestSelectedGridCellActivation();

    GameFlowState mState;
    bool mGridActivationRequested{};
};

static_assert(sizeof(GameScene) == 1);
static_assert(sizeof(MainMenuItem) == 1);

} // namespace pvz::game
