#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/InputReplay.h"
#include "pvz/platform/windows/ReferenceInputRecorder.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace
{

std::uint32_t gFailureCount{};

void Expect(bool theCondition, const char* theMessage)
{
    if (theCondition)
        return;
    std::cerr << "FAIL: " << theMessage << '\n';
    ++gFailureCount;
}

void TestLogicalTickCapture()
{
    pvz::platform::windows::ReferenceInputRecorder aRecorder;
    aRecorder.RecordPointerPosition(321, 456);
    aRecorder.RecordVirtualKeyDown(0x0D, false);
    aRecorder.RecordPointerButtonDown(
        pvz::platform::windows::ReferencePointerButton::Primary);
    aRecorder.RecordMouseWheel(2);
    aRecorder.RecordText(U'P');
    Expect(aRecorder.CaptureTick(), "capture first reference tick");

    aRecorder.RecordVirtualKeyDown(0x0D, true);
    aRecorder.RecordPointerPosition(-10, 20);
    Expect(aRecorder.CaptureTick(), "capture repeated-key tick");

    aRecorder.RecordVirtualKeyUp(0x0D);
    aRecorder.RecordPointerButtonUp(
        pvz::platform::windows::ReferencePointerButton::Primary);
    Expect(aRecorder.CaptureTick(), "capture release tick");

    const auto aFrames = aRecorder.GetReplay().GetFrames();
    Expect(aFrames.size() == 3, "reference recorder emits one frame per tick");
    if (aFrames.size() != 3)
        return;

    const pvz::engine::core::ReplayInputFrame aFirst(aFrames[0]);
    Expect(
        aFirst.IsKeyDown(pvz::engine::KeyCode::Enter) &&
            aFirst.WasKeyPressed(pvz::engine::KeyCode::Enter) &&
            aFirst.IsPointerButtonDown(
                pvz::engine::PointerButton::Primary) &&
            aFirst.WasPointerButtonPressed(
                pvz::engine::PointerButton::Primary) &&
            aFirst.GetPointerState().mPosition.mX == 321 &&
            aFirst.GetPointerState().mPosition.mY == 456 &&
            aFirst.GetPointerState().mWheelDelta == 240 &&
            aFirst.GetTextInput().size() == 1 &&
            aFirst.GetTextInput()[0] == U'P',
        "first reference frame contains complete logical input");

    const pvz::engine::core::ReplayInputFrame aSecond(aFrames[1]);
    Expect(
        aSecond.IsKeyDown(pvz::engine::KeyCode::Enter) &&
            !aSecond.WasKeyPressed(pvz::engine::KeyCode::Enter) &&
            aSecond.IsPointerButtonDown(
                pvz::engine::PointerButton::Primary) &&
            !aSecond.WasPointerButtonPressed(
                pvz::engine::PointerButton::Primary) &&
            aSecond.GetPointerState().mWheelDelta == 0 &&
            aSecond.GetTextInput().empty(),
        "reference recorder clears transients but preserves held state");

    const pvz::engine::core::ReplayInputFrame aThird(aFrames[2]);
    Expect(
        !aThird.IsKeyDown(pvz::engine::KeyCode::Enter) &&
            !aThird.IsPointerButtonDown(
                pvz::engine::PointerButton::Primary),
        "reference recorder captures releases");
}

void TestVirtualKeyMappingAndSerialization()
{
    pvz::platform::windows::ReferenceInputRecorder aRecorder;
    constexpr std::uint32_t kVirtualKeys[]{
        0x08,
        0x0D,
        0x1B,
        0x20,
        0x2E,
        0x25,
        0x27,
        0x26,
        0x28,
        0x41,
    };
    for (const auto aVirtualKey : kVirtualKeys)
        aRecorder.RecordVirtualKeyDown(aVirtualKey, false);
    aRecorder.RecordText(0xD800);
    Expect(aRecorder.CaptureTick(), "capture mapped virtual keys");

    const auto& aFrame = aRecorder.GetReplay().GetFrames()[0];
    const pvz::engine::core::ReplayInputFrame anInput(aFrame);
    Expect(
        anInput.IsKeyDown(pvz::engine::KeyCode::Backspace) &&
            anInput.IsKeyDown(pvz::engine::KeyCode::Enter) &&
            anInput.IsKeyDown(pvz::engine::KeyCode::Escape) &&
            anInput.IsKeyDown(pvz::engine::KeyCode::Space) &&
            anInput.IsKeyDown(pvz::engine::KeyCode::DeleteKey) &&
            anInput.IsKeyDown(pvz::engine::KeyCode::ArrowLeft) &&
            anInput.IsKeyDown(pvz::engine::KeyCode::ArrowRight) &&
            anInput.IsKeyDown(pvz::engine::KeyCode::ArrowUp) &&
            anInput.IsKeyDown(pvz::engine::KeyCode::ArrowDown) &&
            anInput.IsKeyDown(pvz::engine::KeyCode::Unknown) &&
            anInput.GetTextInput().empty(),
        "Win32 virtual keys map to the fixed-width engine enum");

    pvz::engine::core::BinaryStateWriter aWriter;
    Expect(aRecorder.Save(aWriter), "serialize Windows reference replay");
    pvz::engine::core::BinaryStateReader aReader(aWriter.GetBytes());
    pvz::engine::core::InputReplay aLoadedReplay;
    pvz::engine::core::InputReplayError anError{};
    Expect(
        aLoadedReplay.Load(aReader, anError) &&
            aLoadedReplay.GetFrames().size() == 1,
        "Windows reference replay round-trips through PVZR");
}

void TestCaptureBridgePublishesReplay()
{
    std::error_code anError;
    const auto aUniqueSuffix = std::to_string(
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count());
    const auto anOutputPath =
        std::filesystem::temp_directory_path(anError) /
        ("pvz-reference-input-" + aUniqueSuffix + ".pvzr");
    Expect(!anError, "resolve temporary capture directory");
    if (anError)
        return;

    Expect(
        pvz::platform::windows::ConfigureLegacyInputCapture(
            anOutputPath.string()),
        "configure global reference input capture");
    pvz::platform::windows::RecordLegacyVirtualKeyDown(
        0x20,
        false);
    pvz::platform::windows::CaptureLegacyInputTick();
    Expect(
        pvz::platform::windows::FinalizeLegacyInputCapture(),
        "publish global reference input capture");
    Expect(
        pvz::platform::windows::GetLegacyInputCaptureFrameCount() == 1 &&
            std::filesystem::exists(anOutputPath, anError) &&
            !anError,
        "published reference input capture has one frame");

    std::ifstream aStream(anOutputPath, std::ios::binary);
    const auto aFileSize =
        std::filesystem::file_size(anOutputPath, anError);
    Expect(
        aStream && !anError && aFileSize <= 1'024'000,
        "open bounded published reference input capture");
    if (!aStream || anError || aFileSize > 1'024'000)
    {
        aStream.close();
        std::filesystem::remove(anOutputPath, anError);
        return;
    }
    std::vector<std::byte> aBytes(
        static_cast<std::size_t>(aFileSize));
    if (aStream && !aBytes.empty())
    {
        aStream.read(
            reinterpret_cast<char*>(aBytes.data()),
            static_cast<std::streamsize>(aBytes.size()));
    }
    pvz::engine::core::BinaryStateReader aReader(aBytes);
    pvz::engine::core::InputReplay aReplay;
    pvz::engine::core::InputReplayError aReplayError{};
    Expect(
        !anError &&
            aStream &&
            aReplay.Load(aReader, aReplayError) &&
            aReplay.GetFrames().size() == 1,
        "published reference input capture is valid PVZR");

    aStream.close();
    std::filesystem::remove(anOutputPath, anError);
    Expect(!anError, "remove temporary reference input capture");
}

} // namespace

int main()
{
    TestLogicalTickCapture();
    TestVirtualKeyMappingAndSerialization();
    TestCaptureBridgePublishesReplay();
    if (gFailureCount != 0)
    {
        std::cerr
            << gFailureCount
            << " Windows reference input assertion(s) failed\n";
        return 1;
    }
    std::cout << "Windows reference input tests passed\n";
    return 0;
}
