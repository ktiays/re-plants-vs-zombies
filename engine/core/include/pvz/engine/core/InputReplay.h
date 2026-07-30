#pragma once

#include "pvz/engine/Input.h"
#include "pvz/engine/StateIO.h"

#include <cstdint>
#include <span>
#include <vector>

namespace pvz::engine::core
{

enum class InputReplayError : std::uint8_t
{
    None,
    IoError,
    InvalidMagic,
    UnsupportedVersion,
    InvalidTickFrequency,
    TooManyFrames,
    NonSequentialTick,
    InvalidKeyMask,
    InvalidPointerMask,
    TooMuchText,
    InvalidText,
    TrailingData,
};

struct RecordedInputFrame
{
    TickIndex mTick{};
    std::uint16_t mKeysDown{};
    std::uint16_t mKeysPressed{};
    std::uint8_t mPointerButtonsDown{};
    std::uint8_t mPointerButtonsPressed{};
    PointerState mPointer;
    std::vector<char32_t> mTextInput;

    void SetKeyDown(KeyCode theKey, bool theDown);
    void SetKeyPressed(KeyCode theKey, bool thePressed);
    void SetPointerButtonDown(
        PointerButton theButton,
        bool theDown);
    void SetPointerButtonPressed(
        PointerButton theButton,
        bool thePressed);
};

class ReplayInputFrame final : public IInputFrame
{
public:
    explicit ReplayInputFrame(
        const RecordedInputFrame& theFrame);

    [[nodiscard]] bool IsKeyDown(
        KeyCode theKey) const override;
    [[nodiscard]] bool WasKeyPressed(
        KeyCode theKey) const override;
    [[nodiscard]] bool IsPointerButtonDown(
        PointerButton theButton) const override;
    [[nodiscard]] bool WasPointerButtonPressed(
        PointerButton theButton) const override;
    [[nodiscard]] PointerState GetPointerState() const override;
    [[nodiscard]] std::span<const char32_t> GetTextInput()
        const override;

private:
    const RecordedInputFrame& mFrame;
};

class InputReplay
{
public:
    [[nodiscard]] bool AppendFrame(
        TickIndex theTick,
        const IInputFrame& theInput,
        InputReplayError& theError);
    [[nodiscard]] bool AppendFrame(
        RecordedInputFrame theFrame,
        InputReplayError& theError);
    void Clear();

    [[nodiscard]] bool Save(
        IStateWriter& theWriter,
        InputReplayError& theError) const;
    [[nodiscard]] bool Load(
        IStateReader& theReader,
        InputReplayError& theError);

    [[nodiscard]] std::span<const RecordedInputFrame>
        GetFrames() const;

private:
    std::vector<RecordedInputFrame> mFrames;
};

static_assert(sizeof(InputReplayError) == 1);

} // namespace pvz::engine::core
