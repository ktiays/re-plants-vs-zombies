#pragma once

#include "pvz/engine/StateIO.h"
#include "pvz/engine/core/InputReplay.h"
#include "pvz/game/BehaviorObservation.h"
#include "pvz/game/LevelOneRandomDecision.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace pvz::parity
{

enum class BehaviorProducer : std::uint8_t
{
    Unknown,
    PortableGameModule,
    LegacyWindows,
    Count,
};

enum class BehaviorCaptureError : std::uint8_t
{
    None,
    IoError,
    InvalidMagic,
    UnsupportedVersion,
    InvalidTickFrequency,
    InvalidProducer,
    InputReplayTooLarge,
    InvalidInputReplay,
    TooManyObservations,
    CountMismatch,
    NonSequentialTick,
    InvalidScene,
    InvalidBoardStage,
    InvalidGridCoordinate,
    InvalidOccupiedCells,
    InvalidSeedSelection,
    InvalidTutorialPhase,
    InvalidSeedRefresh,
    InvalidFirstSunState,
    InvalidLevelOneCombatState,
    TooManyRandomDecisions,
    InvalidRandomDecisionKind,
    InvalidRandomDecisionPayload,
    TrailingData,
};

[[nodiscard]] std::string_view GetBehaviorProducerName(
    BehaviorProducer theProducer);
[[nodiscard]] std::string_view GetBehaviorCaptureErrorMessage(
    BehaviorCaptureError theError);

class BehaviorCapture
{
public:
    static constexpr std::uint16_t kCurrentFormatVersion = 6;

    void SetProducer(BehaviorProducer theProducer);
    void SetInputReplay(
        engine::core::InputReplay theInputReplay);
    [[nodiscard]] bool AppendObservation(
        game::BehaviorObservation theObservation,
        BehaviorCaptureError& theError);
    [[nodiscard]] bool AppendRandomDecision(
        game::LevelOneRandomDecision theDecision,
        BehaviorCaptureError& theError);
    void Clear();

    [[nodiscard]] bool Save(
        engine::IStateWriter& theWriter,
        BehaviorCaptureError& theError) const;
    [[nodiscard]] bool Load(
        engine::IStateReader& theReader,
        BehaviorCaptureError& theError);

    [[nodiscard]] BehaviorProducer GetProducer() const;
    [[nodiscard]] std::uint16_t GetFormatVersion() const;
    [[nodiscard]] const engine::core::InputReplay&
        GetInputReplay() const;
    [[nodiscard]] std::span<const game::BehaviorObservation>
        GetObservations() const;
    [[nodiscard]] std::span<const game::LevelOneRandomDecision>
        GetRandomDecisions() const;

private:
    std::uint16_t mFormatVersion{kCurrentFormatVersion};
    BehaviorProducer mProducer{BehaviorProducer::Unknown};
    engine::core::InputReplay mInputReplay;
    std::vector<game::BehaviorObservation> mObservations;
    std::vector<game::LevelOneRandomDecision> mRandomDecisions;
};

static_assert(sizeof(BehaviorProducer) == 1);
static_assert(sizeof(BehaviorCaptureError) == 1);

} // namespace pvz::parity
