#pragma once

#include "pvz/engine/Game.h"
#include "pvz/engine/core/InputReplay.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace pvz::engine::core
{

enum class ReplaySessionError : std::uint8_t
{
    None,
    IoError,
    InvalidMagic,
    UnsupportedVersion,
    InvalidTickFrequency,
    ReplayTooLarge,
    InvalidReplay,
    HashCountMismatch,
    TooManyHashes,
    TrailingData,
};

enum class ReplayRecordingError : std::uint8_t
{
    None,
    InputCaptureFailed,
    StateSaveFailed,
};

[[nodiscard]] std::string_view GetReplaySessionErrorMessage(
    ReplaySessionError theError);
[[nodiscard]] std::string_view GetReplayRecordingErrorMessage(
    ReplayRecordingError theError);

class ReplaySession
{
public:
    [[nodiscard]] bool Save(
        IStateWriter& theWriter,
        ReplaySessionError& theError) const;
    [[nodiscard]] bool Load(
        IStateReader& theReader,
        ReplaySessionError& theError);

    [[nodiscard]] const InputReplay& GetInputReplay() const;
    [[nodiscard]] std::span<const std::uint64_t>
        GetStateHashes() const;
    [[nodiscard]] std::uint64_t GetTranscriptHash() const;
    [[nodiscard]] std::uint64_t GetFinalStateHash() const;

private:
    friend class ReplayRecordingGame;

    void Reset();

    InputReplay mInputReplay;
    std::vector<std::uint64_t> mStateHashes;
    std::uint64_t mTranscriptHash{};
};

class ReplayRecordingGame final : public IGame
{
public:
    explicit ReplayRecordingGame(IGame& theGame);

    [[nodiscard]] LifecycleResult Initialize(
        IEngineServices& theServices) override;
    void Update(
        const GameTick& theTick,
        const IInputFrame& theInput) override;
    void Render(IRenderFrame& theFrame) const override;
    [[nodiscard]] bool LoadState(
        IStateReader& theReader) override;
    [[nodiscard]] bool SaveState(
        IStateWriter& theWriter) const override;
    void Suspend() override;
    void Resume() override;
    void Shutdown() override;

    [[nodiscard]] const ReplaySession& GetSession() const;
    [[nodiscard]] ReplayRecordingError GetRecordingError() const;
    [[nodiscard]] InputReplayError GetInputReplayError() const;

private:
    IGame& mGame;
    ReplaySession mSession;
    ReplayRecordingError mRecordingError{
        ReplayRecordingError::None};
    InputReplayError mInputReplayError{
        InputReplayError::None};
};

static_assert(sizeof(ReplaySessionError) == 1);
static_assert(sizeof(ReplayRecordingError) == 1);

} // namespace pvz::engine::core
