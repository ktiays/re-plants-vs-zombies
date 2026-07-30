#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/DeterministicHash.h"
#include "pvz/engine/core/InputReplay.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

extern void Expect(bool theCondition, const char* theMessage);

namespace
{

[[nodiscard]] pvz::engine::core::InputReplay MakeGoldenReplay()
{
    pvz::engine::core::InputReplay aReplay;
    pvz::engine::core::InputReplayError anError{};

    pvz::engine::core::RecordedInputFrame aFirst;
    aFirst.mTick = 0;
    aFirst.SetKeyDown(pvz::engine::KeyCode::Enter, true);
    aFirst.SetKeyPressed(pvz::engine::KeyCode::Enter, true);
    aFirst.SetPointerButtonDown(
        pvz::engine::PointerButton::Primary,
        true);
    aFirst.SetPointerButtonPressed(
        pvz::engine::PointerButton::Primary,
        true);
    aFirst.mPointer = {
        .mPosition = {12, -3},
        .mWheelDelta = 120,
    };
    aFirst.mTextInput.push_back(U'P');
    Expect(
        aReplay.AppendFrame(std::move(aFirst), anError),
        "append first golden replay frame");

    pvz::engine::core::RecordedInputFrame aSecond;
    aSecond.mTick = 1;
    aSecond.mPointer.mPosition = {-20, 40};
    Expect(
        aReplay.AppendFrame(std::move(aSecond), anError),
        "append second golden replay frame");
    return aReplay;
}

[[nodiscard]] std::vector<std::byte> SaveGoldenReplay()
{
    auto aReplay = MakeGoldenReplay();
    pvz::engine::core::BinaryStateWriter aWriter;
    pvz::engine::core::InputReplayError anError{};
    Expect(
        aReplay.Save(aWriter, anError),
        "save golden input replay");
    const auto aBytes = aWriter.GetBytes();
    return {aBytes.begin(), aBytes.end()};
}

void TestGoldenReplayEncoding()
{
    constexpr std::array<std::uint8_t, 74> kExpected{
        0x50, 0x56, 0x5A, 0x52,
        0x01, 0x00,
        0x64, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x04, 0x00,
        0x04, 0x00,
        0x01,
        0x01,
        0x0C, 0x00, 0x00, 0x00,
        0xFD, 0xFF, 0xFF, 0xFF,
        0x78, 0x00, 0x00, 0x00,
        0x01, 0x00,
        0x50, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,
        0x00, 0x00,
        0x00,
        0x00,
        0xEC, 0xFF, 0xFF, 0xFF,
        0x28, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,
    };

    const auto aBytes = SaveGoldenReplay();
    Expect(
        aBytes.size() == kExpected.size(),
        "golden replay has stable byte count");
    if (aBytes.size() != kExpected.size())
        return;
    for (std::size_t anIndex = 0;
         anIndex < kExpected.size();
         ++anIndex)
    {
        Expect(
            std::to_integer<std::uint8_t>(aBytes[anIndex]) ==
                kExpected[anIndex],
            "golden replay uses fixed-width little-endian encoding");
    }
}

void TestReplayRoundTripAndInputView()
{
    const auto aBytes = SaveGoldenReplay();
    pvz::engine::core::BinaryStateReader aReader(aBytes);
    pvz::engine::core::InputReplay aReplay;
    pvz::engine::core::InputReplayError anError{};
    Expect(
        aReplay.Load(aReader, anError),
        "golden replay loads");

    const auto aFrames = aReplay.GetFrames();
    Expect(aFrames.size() == 2, "golden replay restores frames");
    if (aFrames.size() != 2)
        return;

    const pvz::engine::core::ReplayInputFrame aFirst(aFrames[0]);
    Expect(
        aFirst.IsKeyDown(pvz::engine::KeyCode::Enter) &&
            aFirst.WasKeyPressed(pvz::engine::KeyCode::Enter) &&
            !aFirst.WasKeyPressed(pvz::engine::KeyCode::Escape),
        "replay input exposes key state and transition");
    Expect(
        aFirst.IsPointerButtonDown(
            pvz::engine::PointerButton::Primary) &&
            aFirst.WasPointerButtonPressed(
                pvz::engine::PointerButton::Primary),
        "replay input exposes pointer state and transition");
    Expect(
        aFirst.GetPointerState().mPosition.mX == 12 &&
            aFirst.GetPointerState().mPosition.mY == -3 &&
            aFirst.GetPointerState().mWheelDelta == 120,
        "replay input restores pointer coordinates and wheel");
    Expect(
        aFirst.GetTextInput().size() == 1 &&
            aFirst.GetTextInput()[0] == U'P',
        "replay input restores Unicode text");

    pvz::engine::core::InputReplay aCapturedReplay;
    Expect(
        aCapturedReplay.AppendFrame(0, aFirst, anError),
        "platform-neutral input frame can be captured");
    const auto aCapturedFrames = aCapturedReplay.GetFrames();
    Expect(
        aCapturedFrames.size() == 1 &&
            aCapturedFrames[0].mKeysDown ==
                aFrames[0].mKeysDown &&
            aCapturedFrames[0].mKeysPressed ==
                aFrames[0].mKeysPressed &&
            aCapturedFrames[0].mPointerButtonsDown ==
                aFrames[0].mPointerButtonsDown &&
            aCapturedFrames[0].mPointerButtonsPressed ==
                aFrames[0].mPointerButtonsPressed &&
            aCapturedFrames[0].mTextInput ==
                aFrames[0].mTextInput,
        "captured input retains every portable input channel");
}

void ExpectLoadError(
    std::vector<std::byte> theBytes,
    pvz::engine::core::InputReplayError theExpectedError,
    const char* theMessage)
{
    pvz::engine::core::InputReplay aReplay;
    pvz::engine::core::InputReplayError anError{};
    auto anExistingFrame =
        pvz::engine::core::RecordedInputFrame{};
    Expect(
        aReplay.AppendFrame(std::move(anExistingFrame), anError),
        "prepare transactional replay state");

    pvz::engine::core::BinaryStateReader aReader(theBytes);
    Expect(
        !aReplay.Load(aReader, anError) &&
            anError == theExpectedError,
        theMessage);
    Expect(
        aReplay.GetFrames().size() == 1,
        "failed replay load is transactional");
}

void TestMalformedReplays()
{
    auto aBytes = SaveGoldenReplay();
    aBytes[0] = std::byte{0};
    ExpectLoadError(
        aBytes,
        pvz::engine::core::InputReplayError::InvalidMagic,
        "invalid replay magic is rejected");

    aBytes = SaveGoldenReplay();
    aBytes[4] = std::byte{2};
    ExpectLoadError(
        aBytes,
        pvz::engine::core::InputReplayError::UnsupportedVersion,
        "unsupported replay version is rejected");

    aBytes = SaveGoldenReplay();
    aBytes[6] = std::byte{60};
    ExpectLoadError(
        aBytes,
        pvz::engine::core::InputReplayError::InvalidTickFrequency,
        "non-100-Hz replay is rejected");

    aBytes = SaveGoldenReplay();
    aBytes[10] = std::byte{0x41};
    aBytes[11] = std::byte{0x42};
    aBytes[12] = std::byte{0x0F};
    aBytes[13] = std::byte{0x00};
    ExpectLoadError(
        aBytes,
        pvz::engine::core::InputReplayError::TooManyFrames,
        "oversized replay is rejected before allocation");

    aBytes = SaveGoldenReplay();
    aBytes[14] = std::byte{1};
    ExpectLoadError(
        aBytes,
        pvz::engine::core::InputReplayError::NonSequentialTick,
        "non-sequential replay tick is rejected");

    aBytes = SaveGoldenReplay();
    aBytes[22] = std::byte{0};
    aBytes[23] = std::byte{0x80};
    ExpectLoadError(
        aBytes,
        pvz::engine::core::InputReplayError::InvalidKeyMask,
        "unknown replay key bit is rejected");

    aBytes = SaveGoldenReplay();
    aBytes[26] = std::byte{0x80};
    ExpectLoadError(
        aBytes,
        pvz::engine::core::InputReplayError::InvalidPointerMask,
        "unknown replay pointer bit is rejected");

    aBytes = SaveGoldenReplay();
    aBytes[40] = std::byte{0x01};
    aBytes[41] = std::byte{0x10};
    ExpectLoadError(
        aBytes,
        pvz::engine::core::InputReplayError::TooMuchText,
        "oversized replay text is rejected before allocation");

    aBytes = SaveGoldenReplay();
    aBytes[42] = std::byte{0x00};
    aBytes[43] = std::byte{0xD8};
    aBytes[44] = std::byte{0x00};
    aBytes[45] = std::byte{0x00};
    ExpectLoadError(
        aBytes,
        pvz::engine::core::InputReplayError::InvalidText,
        "surrogate replay text is rejected");

    aBytes = SaveGoldenReplay();
    aBytes.push_back(std::byte{0});
    ExpectLoadError(
        aBytes,
        pvz::engine::core::InputReplayError::TrailingData,
        "trailing replay data is rejected");

    aBytes = SaveGoldenReplay();
    aBytes.pop_back();
    ExpectLoadError(
        aBytes,
        pvz::engine::core::InputReplayError::IoError,
        "truncated replay data is rejected");
}

void TestDeterministicHash()
{
    Expect(
        pvz::engine::core::CalculateFnv1a64({}) ==
            pvz::engine::core::kFnv1a64Offset,
        "empty deterministic hash uses the standard offset");

    constexpr std::array<std::byte, 5> kHello{
        std::byte{'h'},
        std::byte{'e'},
        std::byte{'l'},
        std::byte{'l'},
        std::byte{'o'},
    };
    Expect(
        pvz::engine::core::CalculateFnv1a64(kHello) ==
            0xA430D84680AABD0BULL,
        "deterministic hash matches the FNV-1a reference");
}

} // namespace

void RunInputReplayTests()
{
    TestGoldenReplayEncoding();
    TestReplayRoundTripAndInputView();
    TestMalformedReplays();
    TestDeterministicHash();
}
