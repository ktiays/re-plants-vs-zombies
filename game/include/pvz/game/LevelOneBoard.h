#pragma once

#include "pvz/engine/Types.h"

#include <cstdint>

namespace pvz::game
{

enum class LevelOneSeedSelection : std::uint8_t
{
    None,
    Peashooter,
    Count,
};

enum class LevelOneSeedSelectionResult : std::uint8_t
{
    Selected,
    AlreadySelected,
    Refreshing,
    InsufficientSun,
};

enum class LevelOnePlacementResult : std::uint8_t
{
    Planted,
    NoSeedSelected,
    InvalidCell,
    Occupied,
    InsufficientSun,
};

struct LevelOneBoardState
{
    std::uint16_t mSun{150};
    std::uint16_t mSeedRefreshCounter{};
    bool mSeedRefreshing{};
    LevelOneSeedSelection mSeedSelection{
        LevelOneSeedSelection::None};
};

class LevelOneBoard
{
public:
    static constexpr std::uint16_t kInitialSun = 150;
    static constexpr std::uint16_t kPeashooterCost = 100;
    static constexpr std::uint16_t kPeashooterRefreshTime = 750;
    static constexpr std::uint8_t kPlantableRow = 2;

    void Reset();
    void Update();

    [[nodiscard]] LevelOneSeedSelectionResult
    SelectPeashooter();
    [[nodiscard]] LevelOnePlacementResult PlacePeashooter(
        std::uint8_t theColumn,
        std::uint8_t theRow,
        std::uint64_t& theOccupiedCells);
    void AddSun(std::uint16_t theAmount);
    void CancelSelection();

    [[nodiscard]] LevelOneBoardState GetState() const;
    [[nodiscard]] bool RestoreState(
        const LevelOneBoardState& theState);
    [[nodiscard]] bool CanSelectPeashooter() const;
    [[nodiscard]] bool IsPeashooterSelected() const;
    [[nodiscard]] static bool IsPlantableCell(
        std::uint8_t theColumn,
        std::uint8_t theRow);
    [[nodiscard]] static engine::RectI GetSeedPacketRect();

private:
    LevelOneBoardState mState;
};

static_assert(sizeof(LevelOneSeedSelection) == 1);
static_assert(sizeof(LevelOneSeedSelectionResult) == 1);
static_assert(sizeof(LevelOnePlacementResult) == 1);

} // namespace pvz::game
