#pragma once

#include "pvz/game/BehaviorObservation.h"

#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

namespace pvz::parity
{

inline constexpr std::uint64_t kNoBehaviorDifferenceTick =
    std::numeric_limits<std::uint64_t>::max();

enum class BehaviorField : std::uint8_t
{
    None,
    Tick,
    Scene,
    BoardStage,
    GridColumn,
    GridRow,
    OccupiedCells,
    PlantCount,
    ObservationCount,
};

struct BehaviorDifference
{
    std::uint64_t mTick{kNoBehaviorDifferenceTick};
    BehaviorField mField{BehaviorField::None};
};

[[nodiscard]] std::string_view GetBehaviorFieldName(
    BehaviorField theField);
[[nodiscard]] BehaviorDifference FindFirstBehaviorDifference(
    std::span<const game::BehaviorObservation> theLeft,
    std::span<const game::BehaviorObservation> theRight);

static_assert(sizeof(BehaviorField) == 1);

} // namespace pvz::parity
