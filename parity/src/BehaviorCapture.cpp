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
inline constexpr std::uint16_t kCaptureVersion = 1;
inline constexpr std::uint32_t kMaximumObservationCount = 1'000'000;
inline constexpr std::uint32_t kMaximumInputReplaySize =
    256U * 1'024U * 1'024U;
inline constexpr std::uint64_t kObservationByteCount = 24;

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
    theError = BehaviorCaptureError::None;
    return true;
}

[[nodiscard]] bool ValidateCapture(
    BehaviorProducer theProducer,
    const engine::core::InputReplay& theInputReplay,
    std::span<const game::BehaviorObservation> theObservations,
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
    if (theInputReplay.GetFrames().size() !=
        theObservations.size())
    {
        theError = BehaviorCaptureError::CountMismatch;
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

void BehaviorCapture::Clear()
{
    mProducer = BehaviorProducer::Unknown;
    mInputReplay.Clear();
    mObservations.clear();
}

bool BehaviorCapture::Save(
    engine::IStateWriter& theWriter,
    BehaviorCaptureError& theError) const
{
    if (!ValidateCapture(
            mProducer,
            mInputReplay,
            mObservations,
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
        !theWriter.WriteU16(kCaptureVersion) ||
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
            !theWriter.WriteU32(anObservation.mPlantCount))
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
    if (aVersion != kCaptureVersion)
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
    if (theReader.GetBytesRemaining() <
        static_cast<std::uint64_t>(anObservationCount) *
            kObservationByteCount)
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
        anObservation.mScene =
            static_cast<game::BehaviorScene>(aScene);
        anObservation.mBoardStage =
            static_cast<game::BehaviorBoardStage>(aBoardStage);
        if (!ValidateObservation(
                anObservation,
                static_cast<std::uint64_t>(anIndex),
                theError))
        {
            return false;
        }
        anObservations.push_back(anObservation);
    }
    if (theReader.GetBytesRemaining() != 0)
    {
        theError = BehaviorCaptureError::TrailingData;
        return false;
    }
    mProducer = static_cast<BehaviorProducer>(aProducer);
    mInputReplay = std::move(anInputReplay);
    mObservations = std::move(anObservations);
    theError = BehaviorCaptureError::None;
    return true;
}

BehaviorProducer BehaviorCapture::GetProducer() const
{
    return mProducer;
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

} // namespace pvz::parity
