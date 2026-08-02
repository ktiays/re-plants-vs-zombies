#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/parity/BehaviorCapture.h"
#include "pvz/platform/windows/LegacyInputCapture.h"

#include <chrono>
#include <cstddef>
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

void TestBehaviorCaptureBridge()
{
    std::error_code anError;
    const auto aUniqueSuffix = std::to_string(
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count());
    const auto anOutputPath =
        std::filesystem::temp_directory_path(anError) /
        ("pvz-reference-behavior-" +
         aUniqueSuffix +
         ".pvzb");
    Expect(!anError, "resolve temporary behavior capture directory");
    if (anError)
        return;

    Expect(
        pvz::platform::windows::ConfigureLegacyBehaviorCapture(
            anOutputPath.string()),
        "configure global reference behavior capture");
    pvz::platform::windows::RecordLegacyPointerPosition(12, 34);
    pvz::platform::windows::CaptureLegacyInputTick();
    Expect(
        !pvz::platform::windows::HasLegacyBehaviorCaptureStarted() &&
            pvz::platform::windows::GetLegacyInputCaptureFrameCount() == 0,
        "behavior capture ignores boot-time input");

    pvz::platform::windows::StartLegacyBehaviorCapture();
    pvz::platform::windows::RecordLegacyPointerPosition(100, 200);
    pvz::platform::windows::CaptureLegacyInputTick();

    pvz::game::BehaviorObservation anObservation;
    anObservation.mScene =
        pvz::game::BehaviorScene::AdventurePlaying;
    anObservation.mBoardStage =
        pvz::game::BehaviorBoardStage::Day;
    anObservation.mGridColumn = 1;
    anObservation.mGridRow = 2;
    anObservation.mOccupiedCells =
        std::uint64_t{1} << 19U;
    anObservation.mPlantCount = 1;
    anObservation.mSun = 50;
    anObservation.mSeedRefreshCounter = 25;
    anObservation.mSeedRefreshTime = 750;
    anObservation.mSeedRefreshing = true;
    anObservation.mTutorialPhase =
        pvz::game::BehaviorTutorialPhase::
            LevelOneRefreshPeashooter;
    pvz::platform::windows::RecordLegacyBehaviorObservation(
        anObservation);

    Expect(
        pvz::platform::windows::FinalizeLegacyInputCapture(),
        "publish global reference behavior capture");
    Expect(
        pvz::platform::windows::WasLegacyBehaviorCaptureRequested() &&
        pvz::platform::windows::GetLegacyInputCaptureFrameCount() == 1 &&
        std::filesystem::exists(anOutputPath, anError) &&
        !anError,
        "published reference behavior capture has one tick");

    std::ifstream aStream(anOutputPath, std::ios::binary);
    const auto aFileSize =
        std::filesystem::file_size(anOutputPath, anError);
    Expect(
        aStream && !anError && aFileSize <= 1'024'000,
        "open bounded published behavior capture");
    if (!aStream || anError || aFileSize > 1'024'000)
    {
        aStream.close();
        std::filesystem::remove(anOutputPath, anError);
        return;
    }
    std::vector<std::byte> aBytes(
        static_cast<std::size_t>(aFileSize));
    if (!aBytes.empty())
    {
        aStream.read(
            reinterpret_cast<char*>(aBytes.data()),
            static_cast<std::streamsize>(aBytes.size()));
    }

    pvz::engine::core::BinaryStateReader aReader(aBytes);
    pvz::parity::BehaviorCapture aCapture;
    pvz::parity::BehaviorCaptureError aCaptureError{};
    const bool hasLoaded =
        aStream && aCapture.Load(aReader, aCaptureError);
    Expect(
        hasLoaded,
        "published reference behavior capture is valid PVZB");
    if (hasLoaded)
    {
        const auto anObservations = aCapture.GetObservations();
        Expect(
            aCapture.GetProducer() ==
                pvz::parity::BehaviorProducer::LegacyWindows &&
            aCapture.GetFormatVersion() == 2 &&
            aCapture.GetInputReplay().GetFrames().size() == 1 &&
            anObservations.size() == 1 &&
            anObservations[0].mGridColumn == 1 &&
            anObservations[0].mGridRow == 2 &&
            anObservations[0].mPlantCount == 1 &&
            anObservations[0].mSun == 50 &&
            anObservations[0].mSeedRefreshCounter == 25 &&
            anObservations[0].mSeedRefreshTime == 750 &&
            anObservations[0].mSeedRefreshing,
            "reference behavior fields round trip");
    }

    aStream.close();
    std::filesystem::remove(anOutputPath, anError);
    Expect(!anError, "remove temporary reference behavior capture");
}

} // namespace

int main()
{
    TestBehaviorCaptureBridge();
    if (gFailureCount != 0)
    {
        std::cerr
            << gFailureCount
            << " Windows reference behavior assertion(s) failed\n";
        return 1;
    }
    std::cout << "Windows reference behavior tests passed\n";
    return 0;
}
