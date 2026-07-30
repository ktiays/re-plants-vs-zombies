#include "pvz/engine/core/ReplaySession.h"

#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/DeterministicHash.h"

#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace pvz::engine::core
{
namespace
{

inline constexpr std::uint32_t kSessionMagic = 0x435A5650;
inline constexpr std::uint16_t kSessionVersion = 1;
inline constexpr std::uint32_t kMaximumReplayByteCount =
    256U * 1'024U * 1'024U;
inline constexpr std::uint32_t kMaximumHashCount = 1'000'000;

} // namespace

std::string_view GetReplaySessionErrorMessage(
    ReplaySessionError theError)
{
    switch (theError)
    {
    case ReplaySessionError::None:
        return "no replay session error";
    case ReplaySessionError::IoError:
        return "replay session I/O failed";
    case ReplaySessionError::InvalidMagic:
        return "replay session magic is invalid";
    case ReplaySessionError::UnsupportedVersion:
        return "replay session version is unsupported";
    case ReplaySessionError::InvalidTickFrequency:
        return "replay session frequency is not 100 Hz";
    case ReplaySessionError::ReplayTooLarge:
        return "nested input replay is too large";
    case ReplaySessionError::InvalidReplay:
        return "nested input replay is invalid";
    case ReplaySessionError::HashCountMismatch:
        return "replay frame and state hash counts differ";
    case ReplaySessionError::TooManyHashes:
        return "replay session has too many state hashes";
    case ReplaySessionError::TrailingData:
        return "replay session has trailing data";
    }
    return "unknown replay session error";
}

std::string_view GetReplayRecordingErrorMessage(
    ReplayRecordingError theError)
{
    switch (theError)
    {
    case ReplayRecordingError::None:
        return "no replay recording error";
    case ReplayRecordingError::InputCaptureFailed:
        return "logical input capture failed";
    case ReplayRecordingError::StateSaveFailed:
        return "game state capture failed";
    }
    return "unknown replay recording error";
}

bool ReplaySession::Save(
    IStateWriter& theWriter,
    ReplaySessionError& theError) const
{
    if (mStateHashes.size() !=
        mInputReplay.GetFrames().size())
    {
        theError = ReplaySessionError::HashCountMismatch;
        return false;
    }
    if (mStateHashes.size() > kMaximumHashCount ||
        mStateHashes.size() >
            std::numeric_limits<std::uint32_t>::max())
    {
        theError = ReplaySessionError::TooManyHashes;
        return false;
    }

    BinaryStateWriter aReplayWriter;
    InputReplayError aReplayError{};
    if (!mInputReplay.Save(aReplayWriter, aReplayError))
    {
        static_cast<void>(aReplayError);
        theError = ReplaySessionError::InvalidReplay;
        return false;
    }
    if (aReplayWriter.GetBytesWritten() >
        kMaximumReplayByteCount)
    {
        theError = ReplaySessionError::ReplayTooLarge;
        return false;
    }

    if (!theWriter.WriteU32(kSessionMagic) ||
        !theWriter.WriteU16(kSessionVersion) ||
        !theWriter.WriteU32(kSimulationFrequencyHz) ||
        !theWriter.WriteU32(
            static_cast<std::uint32_t>(
                aReplayWriter.GetBytesWritten())) ||
        !theWriter.WriteBytes(aReplayWriter.GetBytes()) ||
        !theWriter.WriteU32(
            static_cast<std::uint32_t>(
                mStateHashes.size())) ||
        !theWriter.WriteU64(mTranscriptHash))
    {
        theError = ReplaySessionError::IoError;
        return false;
    }
    for (const auto aHash : mStateHashes)
    {
        if (!theWriter.WriteU64(aHash))
        {
            theError = ReplaySessionError::IoError;
            return false;
        }
    }
    theError = ReplaySessionError::None;
    return true;
}

bool ReplaySession::Load(
    IStateReader& theReader,
    ReplaySessionError& theError)
{
    std::uint32_t aMagic{};
    std::uint16_t aVersion{};
    std::uint32_t aTickFrequency{};
    std::uint32_t aReplayByteCount{};
    if (!theReader.ReadU32(aMagic) ||
        !theReader.ReadU16(aVersion) ||
        !theReader.ReadU32(aTickFrequency) ||
        !theReader.ReadU32(aReplayByteCount))
    {
        theError = ReplaySessionError::IoError;
        return false;
    }
    if (aMagic != kSessionMagic)
    {
        theError = ReplaySessionError::InvalidMagic;
        return false;
    }
    if (aVersion != kSessionVersion)
    {
        theError = ReplaySessionError::UnsupportedVersion;
        return false;
    }
    if (aTickFrequency != kSimulationFrequencyHz)
    {
        theError = ReplaySessionError::InvalidTickFrequency;
        return false;
    }
    if (aReplayByteCount > kMaximumReplayByteCount)
    {
        theError = ReplaySessionError::ReplayTooLarge;
        return false;
    }

    std::vector<std::byte> aReplayBytes(aReplayByteCount);
    if (!theReader.ReadBytes(aReplayBytes))
    {
        theError = ReplaySessionError::IoError;
        return false;
    }
    BinaryStateReader aReplayReader(aReplayBytes);
    InputReplay anInputReplay;
    InputReplayError anInputReplayError{};
    if (!anInputReplay.Load(
            aReplayReader,
            anInputReplayError))
    {
        static_cast<void>(anInputReplayError);
        theError = ReplaySessionError::InvalidReplay;
        return false;
    }

    std::uint32_t aHashCount{};
    std::uint64_t aTranscriptHash{};
    if (!theReader.ReadU32(aHashCount) ||
        !theReader.ReadU64(aTranscriptHash))
    {
        theError = ReplaySessionError::IoError;
        return false;
    }
    if (aHashCount > kMaximumHashCount)
    {
        theError = ReplaySessionError::TooManyHashes;
        return false;
    }
    if (aHashCount != anInputReplay.GetFrames().size())
    {
        theError = ReplaySessionError::HashCountMismatch;
        return false;
    }

    std::vector<std::uint64_t> aStateHashes;
    aStateHashes.reserve(aHashCount);
    for (std::uint32_t anIndex = 0;
         anIndex < aHashCount;
         ++anIndex)
    {
        std::uint64_t aHash{};
        if (!theReader.ReadU64(aHash))
        {
            theError = ReplaySessionError::IoError;
            return false;
        }
        aStateHashes.push_back(aHash);
    }
    if (theReader.GetBytesRemaining() != 0)
    {
        theError = ReplaySessionError::TrailingData;
        return false;
    }

    mInputReplay = std::move(anInputReplay);
    mStateHashes = std::move(aStateHashes);
    mTranscriptHash = aTranscriptHash;
    theError = ReplaySessionError::None;
    return true;
}

const InputReplay& ReplaySession::GetInputReplay() const
{
    return mInputReplay;
}

std::span<const std::uint64_t>
ReplaySession::GetStateHashes() const
{
    return mStateHashes;
}

std::uint64_t ReplaySession::GetTranscriptHash() const
{
    return mTranscriptHash;
}

std::uint64_t ReplaySession::GetFinalStateHash() const
{
    if (mStateHashes.empty())
        return 0;
    return mStateHashes.back();
}

void ReplaySession::Reset()
{
    mInputReplay.Clear();
    mStateHashes.clear();
    mTranscriptHash = kFnv1a64Offset;
}

ReplayRecordingGame::ReplayRecordingGame(IGame& theGame)
    : mGame(theGame)
{
    mSession.Reset();
}

LifecycleResult ReplayRecordingGame::Initialize(
    IEngineServices& theServices)
{
    mSession.Reset();
    mRecordingError = ReplayRecordingError::None;
    mInputReplayError = InputReplayError::None;
    return mGame.Initialize(theServices);
}

void ReplayRecordingGame::Update(
    const GameTick& theTick,
    const IInputFrame& theInput)
{
    if (mRecordingError == ReplayRecordingError::None &&
        !mSession.mInputReplay.AppendFrame(
            theTick.mIndex,
            theInput,
            mInputReplayError))
    {
        mRecordingError =
            ReplayRecordingError::InputCaptureFailed;
    }

    mGame.Update(theTick, theInput);

    if (mRecordingError != ReplayRecordingError::None)
        return;
    BinaryStateWriter aStateWriter;
    if (!mGame.SaveState(aStateWriter))
    {
        mRecordingError =
            ReplayRecordingError::StateSaveFailed;
        return;
    }
    const auto aStateHash =
        CalculateFnv1a64(aStateWriter.GetBytes());
    mSession.mStateHashes.push_back(aStateHash);
    mSession.mTranscriptHash =
        CalculateFnv1a64(
            aStateWriter.GetBytes(),
            mSession.mTranscriptHash);
}

void ReplayRecordingGame::Render(
    IRenderFrame& theFrame) const
{
    mGame.Render(theFrame);
}

bool ReplayRecordingGame::LoadState(
    IStateReader& theReader)
{
    return mGame.LoadState(theReader);
}

bool ReplayRecordingGame::SaveState(
    IStateWriter& theWriter) const
{
    return mGame.SaveState(theWriter);
}

void ReplayRecordingGame::Suspend()
{
    mGame.Suspend();
}

void ReplayRecordingGame::Resume()
{
    mGame.Resume();
}

void ReplayRecordingGame::Shutdown()
{
    mGame.Shutdown();
}

const ReplaySession& ReplayRecordingGame::GetSession() const
{
    return mSession;
}

ReplayRecordingError
ReplayRecordingGame::GetRecordingError() const
{
    return mRecordingError;
}

InputReplayError ReplayRecordingGame::GetInputReplayError() const
{
    return mInputReplayError;
}

} // namespace pvz::engine::core
