#include "pvz/parity/BehaviorCapture.h"

#include "pvz/engine/Types.h"
#include "pvz/engine/core/BinaryStateIO.h"

#include <cstddef>
#include <limits>
#include <utility>

namespace pvz::parity
{
namespace
{

inline constexpr std::uint32_t kCaptureMagic = 0x425A5650;
inline constexpr std::uint16_t kLegacyCaptureVersion = 1;
inline constexpr std::uint16_t kEconomyCaptureVersion = 2;
inline constexpr std::uint16_t kRandomDecisionCaptureVersion = 3;
inline constexpr std::uint16_t kCompleteLevelCaptureVersion = 4;
inline constexpr std::uint16_t kPeashooterCaptureVersion = 5;
inline constexpr std::uint32_t kMaximumObservationCount = 1'000'000;
inline constexpr std::uint32_t kMaximumRandomDecisionCount = 100'000;
inline constexpr std::uint32_t kMaximumInputReplaySize =
    256U * 1'024U * 1'024U;
inline constexpr std::uint64_t kLegacyObservationByteCount = 24;
inline constexpr std::uint64_t kEconomyObservationByteCount = 36;
inline constexpr std::uint64_t kCompleteLevelObservationByteCount = 43;
inline constexpr std::uint64_t kObservationByteCount = 46;
inline constexpr std::uint64_t kLegacyRandomDecisionByteCount = 15;
inline constexpr std::uint64_t kCompleteLevelRandomDecisionByteCount = 17;
inline constexpr std::uint64_t kRandomDecisionByteCount = 19;

[[nodiscard]] bool ValidateRandomDecision(
    const game::LevelOneRandomDecision& theDecision,
    BehaviorCaptureError& theError)
{
    if (theDecision.mKind >=
        game::LevelOneRandomDecisionKind::Count)
    {
        theError = BehaviorCaptureError::InvalidRandomDecisionKind;
        return false;
    }
    switch (theDecision.mKind)
    {
    case game::LevelOneRandomDecisionKind::FallingSun:
        if (theDecision.mNextCountdown < 435 ||
            theDecision.mNextCountdown > 1'224 ||
            theDecision.mXMilliPixels < 100'000 ||
            theDecision.mXMilliPixels > 649'000 ||
            theDecision.mGroundYMilliPixels < 300'000 ||
            theDecision.mGroundYMilliPixels > 549'000 ||
            theDecision.mSpeedMicroPixelsPerTick != 0 ||
            theDecision.mWaveHealthThreshold != 0 ||
            theDecision.mPlantColumn != 0xFFU ||
            theDecision.mShootingCounter != 0)
        {
            theError =
                BehaviorCaptureError::InvalidRandomDecisionPayload;
            return false;
        }
        break;
    case game::LevelOneRandomDecisionKind::NormalZombie:
        if (theDecision.mNextCountdown != 0 ||
            theDecision.mXMilliPixels < 780'000 ||
            theDecision.mXMilliPixels > 819'000 ||
            theDecision.mGroundYMilliPixels != 0 ||
            theDecision.mSpeedMicroPixelsPerTick < 230'000 ||
            theDecision.mSpeedMicroPixelsPerTick > 320'000 ||
            theDecision.mWaveHealthThreshold != 0 ||
            theDecision.mPlantColumn != 0xFFU ||
            theDecision.mShootingCounter != 0)
        {
            theError =
                BehaviorCaptureError::InvalidRandomDecisionPayload;
            return false;
        }
        break;
    case game::LevelOneRandomDecisionKind::WaveSchedule:
        if (theDecision.mNextCountdown < 2'500 ||
            theDecision.mNextCountdown > 3'099 ||
            theDecision.mXMilliPixels != 0 ||
            theDecision.mGroundYMilliPixels != 0 ||
            theDecision.mSpeedMicroPixelsPerTick != 0 ||
            theDecision.mWaveHealthThreshold < 135 ||
            theDecision.mWaveHealthThreshold > 351 ||
            theDecision.mPlantColumn != 0xFFU ||
            theDecision.mShootingCounter != 0)
        {
            theError =
                BehaviorCaptureError::InvalidRandomDecisionPayload;
            return false;
        }
        break;
    case game::LevelOneRandomDecisionKind::PeashooterSchedule:
        if (theDecision.mNextCountdown == 0 ||
            theDecision.mNextCountdown > 151 ||
            theDecision.mXMilliPixels != 0 ||
            theDecision.mGroundYMilliPixels != 0 ||
            theDecision.mSpeedMicroPixelsPerTick != 0 ||
            theDecision.mWaveHealthThreshold != 0 ||
            theDecision.mPlantColumn >= 9 ||
            (theDecision.mShootingCounter != 0 &&
             theDecision.mShootingCounter != 33))
        {
            theError =
                BehaviorCaptureError::InvalidRandomDecisionPayload;
            return false;
        }
        break;
    case game::LevelOneRandomDecisionKind::ProjectileSpawn:
        if (theDecision.mNextCountdown != 0 ||
            theDecision.mXMilliPixels < -100'000 ||
            theDecision.mXMilliPixels > 900'000 ||
            theDecision.mGroundYMilliPixels < -100'000 ||
            theDecision.mGroundYMilliPixels > 700'000 ||
            theDecision.mSpeedMicroPixelsPerTick != 0 ||
            theDecision.mWaveHealthThreshold != 0 ||
            theDecision.mPlantColumn >= 9 ||
            theDecision.mShootingCounter != 0)
        {
            theError =
                BehaviorCaptureError::InvalidRandomDecisionPayload;
            return false;
        }
        break;
    case game::LevelOneRandomDecisionKind::ZombieMotion:
        if (theDecision.mNextCountdown != 0 ||
            theDecision.mXMilliPixels < -200'000 ||
            theDecision.mXMilliPixels > 1'000'000 ||
            theDecision.mGroundYMilliPixels != 0 ||
            theDecision.mSpeedMicroPixelsPerTick != 0 ||
            theDecision.mWaveHealthThreshold != 0 ||
            theDecision.mPlantColumn >= 8 ||
            theDecision.mShootingCounter > 1)
        {
            theError =
                BehaviorCaptureError::InvalidRandomDecisionPayload;
            return false;
        }
        break;
    case game::LevelOneRandomDecisionKind::ProjectileMotion:
        if (theDecision.mNextCountdown != 0 ||
            theDecision.mXMilliPixels < -200'000 ||
            theDecision.mXMilliPixels > 1'000'000 ||
            theDecision.mGroundYMilliPixels != 0 ||
            theDecision.mSpeedMicroPixelsPerTick != 0 ||
            theDecision.mWaveHealthThreshold != 0 ||
            theDecision.mPlantColumn >= 32 ||
            theDecision.mShootingCounter != 0)
        {
            theError =
                BehaviorCaptureError::InvalidRandomDecisionPayload;
            return false;
        }
        break;
    case game::LevelOneRandomDecisionKind::Count:
        theError = BehaviorCaptureError::InvalidRandomDecisionKind;
        return false;
    }
    theError = BehaviorCaptureError::None;
    return true;
}

[[nodiscard]] bool ValidateObservation(
    const game::BehaviorObservation& theObservation,
    std::uint64_t theExpectedTick,
    BehaviorCaptureError& theError)
{
    if (theObservation.mTick != theExpectedTick)
    {
        theError = BehaviorCaptureError::NonSequentialTick;
        return false;
    }
    if (theObservation.mScene >= game::BehaviorScene::Count)
    {
        theError = BehaviorCaptureError::InvalidScene;
        return false;
    }
    if (theObservation.mBoardStage >=
        game::BehaviorBoardStage::Count)
    {
        theError = BehaviorCaptureError::InvalidBoardStage;
        return false;
    }

    const bool hasNoColumn =
        theObservation.mGridColumn == game::kNoGridCoordinate;
    const bool hasNoRow =
        theObservation.mGridRow == game::kNoGridCoordinate;
    if (hasNoColumn != hasNoRow ||
        (!hasNoColumn &&
         (theObservation.mGridColumn >=
              game::kBehaviorBoardColumnCount ||
          theObservation.mGridRow >=
              game::kBehaviorBoardRowCount)))
    {
        theError = BehaviorCaptureError::InvalidGridCoordinate;
        return false;
    }
    if ((theObservation.mOccupiedCells &
         ~game::kBehaviorOccupiedCellMask) != 0)
    {
        theError = BehaviorCaptureError::InvalidOccupiedCells;
        return false;
    }
    if (theObservation.mSeedSelection >=
        game::BehaviorSeedSelection::Count)
    {
        theError = BehaviorCaptureError::InvalidSeedSelection;
        return false;
    }
    if (theObservation.mTutorialPhase >=
        game::BehaviorTutorialPhase::Count)
    {
        theError = BehaviorCaptureError::InvalidTutorialPhase;
        return false;
    }
    if ((theObservation.mSeedRefreshing &&
         (theObservation.mSeedRefreshTime == 0 ||
          theObservation.mSeedRefreshCounter >
              theObservation.mSeedRefreshTime)) ||
        (!theObservation.mSeedRefreshing &&
         (theObservation.mSeedRefreshCounter != 0 ||
          theObservation.mSeedRefreshTime != 0)))
    {
        theError = BehaviorCaptureError::InvalidSeedRefresh;
        return false;
    }
    if (theObservation.mFirstSunSpawned &&
        theObservation.mFirstSunCountdown != 0)
    {
        theError = BehaviorCaptureError::InvalidFirstSunState;
        return false;
    }
    if (theObservation.mLevelOutcome >=
            game::BehaviorLevelOutcome::Count ||
        theObservation.mMowerState >=
            game::BehaviorMowerState::Count ||
        theObservation.mCurrentWave > 4 ||
        theObservation.mZombieCountdown > 3'099 ||
        theObservation.mZombieCount > 8 ||
        theObservation.mZombieWaveHealth > 540 ||
        theObservation.mProjectileCount > 32 ||
        (theObservation.mLevelAwardSpawned &&
         theObservation.mLevelOutcome !=
             game::BehaviorLevelOutcome::Won) ||
        (theObservation.mLevelOutcome ==
             game::BehaviorLevelOutcome::None &&
         (theObservation.mCurrentWave != 0 ||
          theObservation.mZombieCountdown != 0 ||
          theObservation.mZombieCount != 0 ||
          theObservation.mZombieWaveHealth != 0 ||
          theObservation.mProjectileCount != 0 ||
          theObservation.mMowerState !=
              game::BehaviorMowerState::None ||
          theObservation.mLevelAwardSpawned)))
    {
        theError =
            BehaviorCaptureError::InvalidLevelOneCombatState;
        return false;
    }
    theError = BehaviorCaptureError::None;
    return true;
}

[[nodiscard]] bool ValidateCapture(
    BehaviorProducer theProducer,
    const engine::core::InputReplay& theInputReplay,
    std::span<const game::BehaviorObservation> theObservations,
    std::span<const game::LevelOneRandomDecision> theRandomDecisions,
    BehaviorCaptureError& theError)
{
    if (theProducer >= BehaviorProducer::Count)
    {
        theError = BehaviorCaptureError::InvalidProducer;
        return false;
    }
    if (theObservations.size() > kMaximumObservationCount)
    {
        theError = BehaviorCaptureError::TooManyObservations;
        return false;
    }
    if (theRandomDecisions.size() > kMaximumRandomDecisionCount)
    {
        theError = BehaviorCaptureError::TooManyRandomDecisions;
        return false;
    }
    if (theInputReplay.GetFrames().size() !=
        theObservations.size())
    {
        theError = BehaviorCaptureError::CountMismatch;
        return false;
    }
    for (const auto& aDecision : theRandomDecisions)
    {
        if (!ValidateRandomDecision(aDecision, theError))
            return false;
    }
    for (std::size_t anIndex = 0;
         anIndex < theObservations.size();
         ++anIndex)
    {
        if (!ValidateObservation(
                theObservations[anIndex],
                static_cast<std::uint64_t>(anIndex),
                theError))
        {
            return false;
        }
    }
    theError = BehaviorCaptureError::None;
    return true;
}

} // namespace

std::string_view GetBehaviorProducerName(
    BehaviorProducer theProducer)
{
    switch (theProducer)
    {
    case BehaviorProducer::Unknown:
        return "unknown";
    case BehaviorProducer::PortableGameModule:
        return "portable-game";
    case BehaviorProducer::LegacyWindows:
        return "legacy-windows";
    case BehaviorProducer::Count:
        break;
    }
    return "invalid";
}

std::string_view GetBehaviorCaptureErrorMessage(
    BehaviorCaptureError theError)
{
    switch (theError)
    {
    case BehaviorCaptureError::None:
        return "no behavior capture error";
    case BehaviorCaptureError::IoError:
        return "behavior capture I/O failed";
    case BehaviorCaptureError::InvalidMagic:
        return "behavior capture magic is invalid";
    case BehaviorCaptureError::UnsupportedVersion:
        return "behavior capture version is unsupported";
    case BehaviorCaptureError::InvalidTickFrequency:
        return "behavior capture frequency is not 100 Hz";
    case BehaviorCaptureError::InvalidProducer:
        return "behavior capture producer is invalid";
    case BehaviorCaptureError::InputReplayTooLarge:
        return "behavior capture input replay is too large";
    case BehaviorCaptureError::InvalidInputReplay:
        return "behavior capture input replay is invalid";
    case BehaviorCaptureError::TooManyObservations:
        return "behavior capture has too many observations";
    case BehaviorCaptureError::CountMismatch:
        return "behavior capture input and observation counts differ";
    case BehaviorCaptureError::NonSequentialTick:
        return "behavior capture ticks are not sequential";
    case BehaviorCaptureError::InvalidScene:
        return "behavior capture scene is invalid";
    case BehaviorCaptureError::InvalidBoardStage:
        return "behavior capture board stage is invalid";
    case BehaviorCaptureError::InvalidGridCoordinate:
        return "behavior capture grid coordinate is invalid";
    case BehaviorCaptureError::InvalidOccupiedCells:
        return "behavior capture occupied-cell mask is invalid";
    case BehaviorCaptureError::InvalidSeedSelection:
        return "behavior capture seed selection is invalid";
    case BehaviorCaptureError::InvalidTutorialPhase:
        return "behavior capture tutorial phase is invalid";
    case BehaviorCaptureError::InvalidSeedRefresh:
        return "behavior capture seed refresh state is invalid";
    case BehaviorCaptureError::InvalidFirstSunState:
        return "behavior capture first-sun state is invalid";
    case BehaviorCaptureError::InvalidLevelOneCombatState:
        return "behavior capture Level 1 combat state is invalid";
    case BehaviorCaptureError::TooManyRandomDecisions:
        return "behavior capture has too many random decisions";
    case BehaviorCaptureError::InvalidRandomDecisionKind:
        return "behavior capture random-decision kind is invalid";
    case BehaviorCaptureError::InvalidRandomDecisionPayload:
        return "behavior capture random-decision payload is invalid";
    case BehaviorCaptureError::TrailingData:
        return "behavior capture has trailing data";
    }
    return "unknown behavior capture error";
}

void BehaviorCapture::SetProducer(BehaviorProducer theProducer)
{
    mProducer = theProducer;
}

void BehaviorCapture::SetInputReplay(
    engine::core::InputReplay theInputReplay)
{
    mInputReplay = std::move(theInputReplay);
}

bool BehaviorCapture::AppendObservation(
    game::BehaviorObservation theObservation,
    BehaviorCaptureError& theError)
{
    if (mObservations.size() >= kMaximumObservationCount)
    {
        theError = BehaviorCaptureError::TooManyObservations;
        return false;
    }
    if (!ValidateObservation(
            theObservation,
            static_cast<std::uint64_t>(mObservations.size()),
            theError))
    {
        return false;
    }
    mObservations.push_back(theObservation);
    theError = BehaviorCaptureError::None;
    return true;
}

bool BehaviorCapture::AppendRandomDecision(
    game::LevelOneRandomDecision theDecision,
    BehaviorCaptureError& theError)
{
    if (mRandomDecisions.size() >= kMaximumRandomDecisionCount)
    {
        theError = BehaviorCaptureError::TooManyRandomDecisions;
        return false;
    }
    if (!ValidateRandomDecision(theDecision, theError))
        return false;
    mRandomDecisions.push_back(theDecision);
    theError = BehaviorCaptureError::None;
    return true;
}

void BehaviorCapture::Clear()
{
    mFormatVersion = kCurrentFormatVersion;
    mProducer = BehaviorProducer::Unknown;
    mInputReplay.Clear();
    mObservations.clear();
    mRandomDecisions.clear();
}

bool BehaviorCapture::Save(
    engine::IStateWriter& theWriter,
    BehaviorCaptureError& theError) const
{
    if (!ValidateCapture(
            mProducer,
            mInputReplay,
            mObservations,
            mRandomDecisions,
            theError))
    {
        return false;
    }

    engine::core::BinaryStateWriter anInputWriter;
    engine::core::InputReplayError anInputError{};
    if (!mInputReplay.Save(anInputWriter, anInputError))
    {
        theError = BehaviorCaptureError::InvalidInputReplay;
        return false;
    }
    const auto anInputBytes = anInputWriter.GetBytes();
    if (anInputBytes.size() > kMaximumInputReplaySize ||
        anInputBytes.size() >
            std::numeric_limits<std::uint32_t>::max())
    {
        theError = BehaviorCaptureError::InputReplayTooLarge;
        return false;
    }

    if (!theWriter.WriteU32(kCaptureMagic) ||
        !theWriter.WriteU16(kCurrentFormatVersion) ||
        !theWriter.WriteU32(engine::kSimulationFrequencyHz) ||
        !theWriter.WriteU8(
            static_cast<std::uint8_t>(mProducer)) ||
        !theWriter.WriteU32(
            static_cast<std::uint32_t>(anInputBytes.size())) ||
        !theWriter.WriteBytes(anInputBytes) ||
        !theWriter.WriteU32(
            static_cast<std::uint32_t>(
                mObservations.size())))
    {
        theError = BehaviorCaptureError::IoError;
        return false;
    }

    for (const auto& anObservation : mObservations)
    {
        if (!theWriter.WriteU64(anObservation.mTick) ||
            !theWriter.WriteU8(
                static_cast<std::uint8_t>(
                    anObservation.mScene)) ||
            !theWriter.WriteU8(
                static_cast<std::uint8_t>(
                    anObservation.mBoardStage)) ||
            !theWriter.WriteU8(anObservation.mGridColumn) ||
            !theWriter.WriteU8(anObservation.mGridRow) ||
            !theWriter.WriteU64(
                anObservation.mOccupiedCells) ||
            !theWriter.WriteU32(anObservation.mPlantCount) ||
            !theWriter.WriteU16(anObservation.mSun) ||
            !theWriter.WriteU16(
                anObservation.mSeedRefreshCounter) ||
            !theWriter.WriteU16(
                anObservation.mSeedRefreshTime) ||
            !theWriter.WriteBool(
                anObservation.mSeedRefreshing) ||
            !theWriter.WriteU8(
                static_cast<std::uint8_t>(
                    anObservation.mSeedSelection)) ||
            !theWriter.WriteU8(
                static_cast<std::uint8_t>(
                    anObservation.mTutorialPhase)) ||
            !theWriter.WriteU16(
                anObservation.mFirstSunCountdown) ||
            !theWriter.WriteBool(
                anObservation.mFirstSunSpawned) ||
            !theWriter.WriteU8(
                anObservation.mCurrentWave) ||
            !theWriter.WriteU16(
                anObservation.mZombieCountdown) ||
            !theWriter.WriteU8(
                anObservation.mZombieCount) ||
            !theWriter.WriteU8(
                static_cast<std::uint8_t>(
                    anObservation.mLevelOutcome)) ||
            !theWriter.WriteU8(
                static_cast<std::uint8_t>(
                    anObservation.mMowerState)) ||
            !theWriter.WriteBool(
                anObservation.mLevelAwardSpawned) ||
            !theWriter.WriteU16(
                anObservation.mZombieWaveHealth) ||
            !theWriter.WriteU8(
                anObservation.mProjectileCount))
        {
            theError = BehaviorCaptureError::IoError;
            return false;
        }
    }
    if (!theWriter.WriteU32(
            static_cast<std::uint32_t>(mRandomDecisions.size())))
    {
        theError = BehaviorCaptureError::IoError;
        return false;
    }
    for (const auto& aDecision : mRandomDecisions)
    {
        if (!theWriter.WriteU8(
                static_cast<std::uint8_t>(aDecision.mKind)) ||
            !theWriter.WriteU16(aDecision.mNextCountdown) ||
            !theWriter.WriteI32(aDecision.mXMilliPixels) ||
            !theWriter.WriteI32(
                aDecision.mGroundYMilliPixels) ||
            !theWriter.WriteU32(
                aDecision.mSpeedMicroPixelsPerTick) ||
            !theWriter.WriteU16(
                aDecision.mWaveHealthThreshold) ||
            !theWriter.WriteU8(aDecision.mPlantColumn) ||
            !theWriter.WriteU8(aDecision.mShootingCounter))
        {
            theError = BehaviorCaptureError::IoError;
            return false;
        }
    }
    theError = BehaviorCaptureError::None;
    return true;
}

bool BehaviorCapture::Load(
    engine::IStateReader& theReader,
    BehaviorCaptureError& theError)
{
    std::uint32_t aMagic{};
    std::uint16_t aVersion{};
    std::uint32_t aFrequency{};
    std::uint8_t aProducer{};
    std::uint32_t anInputSize{};
    if (!theReader.ReadU32(aMagic) ||
        !theReader.ReadU16(aVersion) ||
        !theReader.ReadU32(aFrequency) ||
        !theReader.ReadU8(aProducer) ||
        !theReader.ReadU32(anInputSize))
    {
        theError = BehaviorCaptureError::IoError;
        return false;
    }
    if (aMagic != kCaptureMagic)
    {
        theError = BehaviorCaptureError::InvalidMagic;
        return false;
    }
    if (aVersion != kLegacyCaptureVersion &&
        aVersion != kEconomyCaptureVersion &&
        aVersion != kRandomDecisionCaptureVersion &&
        aVersion != kCompleteLevelCaptureVersion &&
        aVersion != kPeashooterCaptureVersion &&
        aVersion != kCurrentFormatVersion)
    {
        theError = BehaviorCaptureError::UnsupportedVersion;
        return false;
    }
    if (aFrequency != engine::kSimulationFrequencyHz)
    {
        theError = BehaviorCaptureError::InvalidTickFrequency;
        return false;
    }
    if (aProducer >=
        static_cast<std::uint8_t>(BehaviorProducer::Count))
    {
        theError = BehaviorCaptureError::InvalidProducer;
        return false;
    }
    if (anInputSize > kMaximumInputReplaySize)
    {
        theError = BehaviorCaptureError::InputReplayTooLarge;
        return false;
    }
    if (theReader.GetBytesRemaining() <
        static_cast<std::uint64_t>(anInputSize) +
            std::uint64_t{4})
    {
        theError = BehaviorCaptureError::IoError;
        return false;
    }

    std::vector<std::byte> anInputBytes(anInputSize);
    if (!theReader.ReadBytes(anInputBytes))
    {
        theError = BehaviorCaptureError::IoError;
        return false;
    }
    engine::core::BinaryStateReader anInputReader(anInputBytes);
    engine::core::InputReplay anInputReplay;
    engine::core::InputReplayError anInputError{};
    if (!anInputReplay.Load(anInputReader, anInputError))
    {
        theError = BehaviorCaptureError::InvalidInputReplay;
        return false;
    }

    std::uint32_t anObservationCount{};
    if (!theReader.ReadU32(anObservationCount))
    {
        theError = BehaviorCaptureError::IoError;
        return false;
    }
    if (anObservationCount > kMaximumObservationCount)
    {
        theError = BehaviorCaptureError::TooManyObservations;
        return false;
    }
    if (anInputReplay.GetFrames().size() !=
        anObservationCount)
    {
        theError = BehaviorCaptureError::CountMismatch;
        return false;
    }
    const auto anObservationByteCount =
        aVersion == kLegacyCaptureVersion
        ? kLegacyObservationByteCount
        : (aVersion < kCompleteLevelCaptureVersion
               ? kEconomyObservationByteCount
               : (aVersion < kPeashooterCaptureVersion
                      ? kCompleteLevelObservationByteCount
                      : kObservationByteCount));
    if (theReader.GetBytesRemaining() <
        static_cast<std::uint64_t>(anObservationCount) *
            anObservationByteCount)
    {
        theError = BehaviorCaptureError::IoError;
        return false;
    }

    std::vector<game::BehaviorObservation> anObservations;
    anObservations.reserve(anObservationCount);
    for (std::uint32_t anIndex = 0;
         anIndex < anObservationCount;
         ++anIndex)
    {
        game::BehaviorObservation anObservation;
        std::uint8_t aScene{};
        std::uint8_t aBoardStage{};
        std::uint8_t aSeedSelection{};
        std::uint8_t aTutorialPhase{};
        std::uint8_t aLevelOutcome{};
        std::uint8_t aMowerState{};
        if (!theReader.ReadU64(anObservation.mTick) ||
            !theReader.ReadU8(aScene) ||
            !theReader.ReadU8(aBoardStage) ||
            !theReader.ReadU8(anObservation.mGridColumn) ||
            !theReader.ReadU8(anObservation.mGridRow) ||
            !theReader.ReadU64(
                anObservation.mOccupiedCells) ||
            !theReader.ReadU32(anObservation.mPlantCount))
        {
            theError = BehaviorCaptureError::IoError;
            return false;
        }
        if (aVersion >= kEconomyCaptureVersion &&
            (!theReader.ReadU16(anObservation.mSun) ||
             !theReader.ReadU16(
                 anObservation.mSeedRefreshCounter) ||
             !theReader.ReadU16(
                 anObservation.mSeedRefreshTime) ||
             !theReader.ReadBool(
                 anObservation.mSeedRefreshing) ||
             !theReader.ReadU8(aSeedSelection) ||
             !theReader.ReadU8(aTutorialPhase) ||
             !theReader.ReadU16(
                 anObservation.mFirstSunCountdown) ||
             !theReader.ReadBool(
                 anObservation.mFirstSunSpawned)))
        {
            theError = BehaviorCaptureError::IoError;
            return false;
        }
        if (aVersion >= kCompleteLevelCaptureVersion &&
            (!theReader.ReadU8(
                 anObservation.mCurrentWave) ||
             !theReader.ReadU16(
                 anObservation.mZombieCountdown) ||
             !theReader.ReadU8(
                 anObservation.mZombieCount) ||
             !theReader.ReadU8(aLevelOutcome) ||
             !theReader.ReadU8(aMowerState) ||
             !theReader.ReadBool(
                 anObservation.mLevelAwardSpawned)))
        {
            theError = BehaviorCaptureError::IoError;
            return false;
        }
        if (aVersion >= kPeashooterCaptureVersion &&
            (!theReader.ReadU16(
                 anObservation.mZombieWaveHealth) ||
             !theReader.ReadU8(
                 anObservation.mProjectileCount)))
        {
            theError = BehaviorCaptureError::IoError;
            return false;
        }
        anObservation.mScene =
            static_cast<game::BehaviorScene>(aScene);
        anObservation.mBoardStage =
            static_cast<game::BehaviorBoardStage>(aBoardStage);
        anObservation.mSeedSelection =
            static_cast<game::BehaviorSeedSelection>(
                aSeedSelection);
        anObservation.mTutorialPhase =
            static_cast<game::BehaviorTutorialPhase>(
                aTutorialPhase);
        anObservation.mLevelOutcome =
            static_cast<game::BehaviorLevelOutcome>(
                aLevelOutcome);
        anObservation.mMowerState =
            static_cast<game::BehaviorMowerState>(
                aMowerState);
        if (!ValidateObservation(
                anObservation,
                static_cast<std::uint64_t>(anIndex),
                theError))
        {
            return false;
        }
        anObservations.push_back(anObservation);
    }
    std::vector<game::LevelOneRandomDecision> aRandomDecisions;
    if (aVersion >= kRandomDecisionCaptureVersion)
    {
        std::uint32_t aDecisionCount{};
        if (!theReader.ReadU32(aDecisionCount))
        {
            theError = BehaviorCaptureError::IoError;
            return false;
        }
        if (aDecisionCount > kMaximumRandomDecisionCount)
        {
            theError = BehaviorCaptureError::TooManyRandomDecisions;
            return false;
        }
        if (theReader.GetBytesRemaining() <
            static_cast<std::uint64_t>(aDecisionCount) *
                (aVersion >= kPeashooterCaptureVersion
                     ? kRandomDecisionByteCount
                     : (aVersion >= kCompleteLevelCaptureVersion
                            ? kCompleteLevelRandomDecisionByteCount
                            : kLegacyRandomDecisionByteCount)))
        {
            theError = BehaviorCaptureError::IoError;
            return false;
        }
        aRandomDecisions.reserve(aDecisionCount);
        for (std::uint32_t anIndex = 0;
             anIndex < aDecisionCount;
             ++anIndex)
        {
            game::LevelOneRandomDecision aDecision;
            std::uint8_t aKind{};
            if (!theReader.ReadU8(aKind) ||
                !theReader.ReadU16(aDecision.mNextCountdown) ||
                !theReader.ReadI32(aDecision.mXMilliPixels) ||
                !theReader.ReadI32(
                    aDecision.mGroundYMilliPixels) ||
                !theReader.ReadU32(
                    aDecision.mSpeedMicroPixelsPerTick) ||
                (aVersion >= kCompleteLevelCaptureVersion &&
                 !theReader.ReadU16(
                     aDecision.mWaveHealthThreshold)) ||
                (aVersion >= kPeashooterCaptureVersion &&
                 (!theReader.ReadU8(aDecision.mPlantColumn) ||
                  !theReader.ReadU8(
                      aDecision.mShootingCounter))))
            {
                theError = BehaviorCaptureError::IoError;
                return false;
            }
            aDecision.mKind =
                static_cast<game::LevelOneRandomDecisionKind>(
                    aKind);
            if (aVersion < kCurrentFormatVersion &&
                aDecision.mKind ==
                    game::LevelOneRandomDecisionKind::ProjectileMotion)
            {
                theError = BehaviorCaptureError::
                    InvalidRandomDecisionKind;
                return false;
            }
            if (!ValidateRandomDecision(aDecision, theError))
                return false;
            aRandomDecisions.push_back(aDecision);
        }
    }
    if (theReader.GetBytesRemaining() != 0)
    {
        theError = BehaviorCaptureError::TrailingData;
        return false;
    }
    mFormatVersion = aVersion;
    mProducer = static_cast<BehaviorProducer>(aProducer);
    mInputReplay = std::move(anInputReplay);
    mObservations = std::move(anObservations);
    mRandomDecisions = std::move(aRandomDecisions);
    theError = BehaviorCaptureError::None;
    return true;
}

BehaviorProducer BehaviorCapture::GetProducer() const
{
    return mProducer;
}

std::uint16_t BehaviorCapture::GetFormatVersion() const
{
    return mFormatVersion;
}

const engine::core::InputReplay&
BehaviorCapture::GetInputReplay() const
{
    return mInputReplay;
}

std::span<const game::BehaviorObservation>
BehaviorCapture::GetObservations() const
{
    return mObservations;
}

std::span<const game::LevelOneRandomDecision>
BehaviorCapture::GetRandomDecisions() const
{
    return mRandomDecisions;
}

} // namespace pvz::parity
