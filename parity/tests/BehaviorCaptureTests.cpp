#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/parity/BehaviorCapture.h"
#include "pvz/parity/BehaviorComparison.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

int gFailures = 0;

void Expect(bool theCondition, std::string_view theMessage)
{
    if (theCondition)
        return;
    std::cerr << "FAIL: " << theMessage << '\n';
    ++gFailures;
}

[[nodiscard]] pvz::engine::core::InputReplay MakeInputReplay()
{
    pvz::engine::core::InputReplay aReplay;
    pvz::engine::core::RecordedInputFrame aFrame;
    aFrame.mTick = 0;
    pvz::engine::core::InputReplayError anError{};
    Expect(
        aReplay.AppendFrame(std::move(aFrame), anError),
        "input fixture should append");
    return aReplay;
}

[[nodiscard]] pvz::parity::BehaviorCapture MakeCapture()
{
    pvz::parity::BehaviorCapture aCapture;
    aCapture.SetProducer(
        pvz::parity::BehaviorProducer::PortableGameModule);
    aCapture.SetInputReplay(MakeInputReplay());
    pvz::game::BehaviorObservation anObservation;
    anObservation.mScene = pvz::game::BehaviorScene::Title;
    pvz::parity::BehaviorCaptureError anError{};
    Expect(
        aCapture.AppendObservation(anObservation, anError),
        "behavior fixture should append");
    return aCapture;
}

[[nodiscard]] std::vector<std::byte> SaveCapture(
    const pvz::parity::BehaviorCapture& theCapture)
{
    pvz::engine::core::BinaryStateWriter aWriter;
    pvz::parity::BehaviorCaptureError anError{};
    Expect(
        theCapture.Save(aWriter, anError),
        "behavior capture should save");
    return {
        aWriter.GetBytes().begin(),
        aWriter.GetBytes().end(),
    };
}

void TestGoldenBytesAndRoundTrip()
{
    const auto aBytes = SaveCapture(MakeCapture());
    constexpr auto kExpected = []
    {
        std::array<std::uint8_t, 111> aResult{};
        aResult[0] = 0x50;
        aResult[1] = 0x56;
        aResult[2] = 0x5A;
        aResult[3] = 0x42;
        aResult[4] = 0x06;
        aResult[6] = 0x64;
        aResult[10] = 0x01;
        aResult[11] = 0x2A;
        aResult[15] = 0x50;
        aResult[16] = 0x56;
        aResult[17] = 0x5A;
        aResult[18] = 0x52;
        aResult[19] = 0x01;
        aResult[21] = 0x64;
        aResult[25] = 0x01;
        aResult[57] = 0x01;
        aResult[69] = 0x01;
        aResult[71] = 0xFF;
        aResult[72] = 0xFF;
        return aResult;
    }();
    Expect(
        aBytes.size() == kExpected.size(),
        "behavior golden byte size should remain stable");
    bool bytesMatch = aBytes.size() == kExpected.size();
    for (std::size_t anIndex = 0;
         bytesMatch && anIndex < aBytes.size();
         ++anIndex)
    {
        bytesMatch =
            static_cast<std::uint8_t>(aBytes[anIndex]) ==
            kExpected[anIndex];
    }
    Expect(bytesMatch, "behavior golden bytes should remain stable");

    pvz::engine::core::BinaryStateReader aReader(aBytes);
    pvz::parity::BehaviorCapture aLoaded;
    pvz::parity::BehaviorCaptureError anError{};
    Expect(
        aLoaded.Load(aReader, anError),
        "behavior golden bytes should load");
    Expect(
        aLoaded.GetProducer() ==
            pvz::parity::BehaviorProducer::PortableGameModule,
        "behavior producer should round trip");
    Expect(
        aLoaded.GetInputReplay().GetFrames().size() == 1,
        "nested input should round trip");
    const auto anObservations = aLoaded.GetObservations();
    Expect(
        anObservations.size() == 1 &&
        anObservations[0].mScene ==
            pvz::game::BehaviorScene::Title,
        "observation should round trip");

    auto aPeashooterBytes = aBytes;
    aPeashooterBytes[4] = std::byte{5};
    pvz::engine::core::BinaryStateReader aPeashooterReader(
        aPeashooterBytes);
    pvz::parity::BehaviorCapture aPeashooterCapture;
    Expect(
        aPeashooterCapture.Load(aPeashooterReader, anError) &&
        aPeashooterCapture.GetFormatVersion() == 5 &&
        aPeashooterCapture.GetRandomDecisions().empty(),
        "version 5 behavior capture should remain readable");

    std::vector<std::byte> aCompleteLevelBytes(
        aBytes.begin(),
        aBytes.begin() + 104);
    aCompleteLevelBytes[4] = std::byte{4};
    aCompleteLevelBytes.insert(
        aCompleteLevelBytes.end(),
        aBytes.begin() + 107,
        aBytes.end());
    pvz::engine::core::BinaryStateReader aCompleteLevelReader(
        aCompleteLevelBytes);
    pvz::parity::BehaviorCapture aCompleteLevelCapture;
    Expect(
        aCompleteLevelCapture.Load(
            aCompleteLevelReader,
            anError) &&
        aCompleteLevelCapture.GetFormatVersion() == 4 &&
        aCompleteLevelCapture.GetRandomDecisions().empty(),
        "version 4 behavior capture should remain readable");

    std::vector<std::byte> aRandomDecisionBytes(
        aBytes.begin(),
        aBytes.begin() + 97);
    aRandomDecisionBytes[4] = std::byte{3};
    aRandomDecisionBytes.insert(
        aRandomDecisionBytes.end(),
        aBytes.begin() + 107,
        aBytes.end());
    pvz::engine::core::BinaryStateReader aRandomDecisionReader(
        aRandomDecisionBytes);
    pvz::parity::BehaviorCapture aRandomDecisionCapture;
    Expect(
        aRandomDecisionCapture.Load(
            aRandomDecisionReader,
            anError) &&
        aRandomDecisionCapture.GetFormatVersion() == 3 &&
        aRandomDecisionCapture.GetRandomDecisions().empty(),
        "version 3 behavior capture should remain readable");

    auto anEconomyBytes = aBytes;
    anEconomyBytes[4] = std::byte{2};
    anEconomyBytes.resize(97);
    pvz::engine::core::BinaryStateReader anEconomyReader(
        anEconomyBytes);
    pvz::parity::BehaviorCapture anEconomyCapture;
    Expect(
        anEconomyCapture.Load(anEconomyReader, anError) &&
        anEconomyCapture.GetFormatVersion() == 2 &&
        anEconomyCapture.GetRandomDecisions().empty(),
        "version 2 behavior capture should remain readable");

    auto aLegacyBytes = aBytes;
    aLegacyBytes[4] = std::byte{1};
    aLegacyBytes.resize(85);
    pvz::engine::core::BinaryStateReader aLegacyReader(
        aLegacyBytes);
    pvz::parity::BehaviorCapture aLegacyCapture;
    Expect(
        aLegacyCapture.Load(aLegacyReader, anError) &&
        aLegacyCapture.GetFormatVersion() == 1 &&
        aLegacyCapture.GetObservations().size() == 1,
        "version 1 behavior capture should remain readable");
}

void ExpectLoadError(
    std::vector<std::byte> theBytes,
    pvz::parity::BehaviorCaptureError theExpected,
    std::string_view theMessage)
{
    pvz::engine::core::BinaryStateReader aReader(theBytes);
    auto aCapture = MakeCapture();
    pvz::parity::BehaviorCaptureError anError{};
    Expect(
        !aCapture.Load(aReader, anError),
        theMessage);
    if (anError != theExpected)
    {
        std::cerr
            << "behavior error mismatch for " << theMessage
            << ": expected="
            << static_cast<std::uint32_t>(theExpected)
            << " actual="
            << static_cast<std::uint32_t>(anError)
            << '\n';
    }
    Expect(
        anError == theExpected,
        "malformed behavior capture should report exact error");
    Expect(
        aCapture.GetObservations().size() == 1 &&
        aCapture.GetObservations()[0].mScene ==
            pvz::game::BehaviorScene::Title,
        "failed behavior load should be transactional");
}

void TestMalformedCaptures()
{
    const auto aGolden = SaveCapture(MakeCapture());

    auto aBytes = aGolden;
    aBytes[0] = std::byte{};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidMagic,
        "invalid behavior magic should fail");

    aBytes = aGolden;
    aBytes[4] = std::byte{7};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::UnsupportedVersion,
        "unsupported behavior version should fail");

    aBytes = aGolden;
    aBytes[6] = std::byte{99};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidTickFrequency,
        "invalid behavior frequency should fail");

    aBytes = aGolden;
    aBytes[10] = std::byte{3};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidProducer,
        "invalid behavior producer should fail");

    aBytes = aGolden;
    aBytes[11] = std::byte{0x01};
    aBytes[12] = std::byte{0x00};
    aBytes[13] = std::byte{0x00};
    aBytes[14] = std::byte{0x10};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InputReplayTooLarge,
        "oversized nested input should fail before allocation");

    aBytes = aGolden;
    aBytes[15] = std::byte{};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidInputReplay,
        "invalid nested input replay should fail");

    aBytes = aGolden;
    aBytes[57] = std::byte{0x41};
    aBytes[58] = std::byte{0x42};
    aBytes[59] = std::byte{0x0F};
    aBytes[60] = std::byte{0x00};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::TooManyObservations,
        "oversized behavior count should fail");

    aBytes = aGolden;
    aBytes[57] = std::byte{};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::CountMismatch,
        "behavior count mismatch should fail");

    aBytes = aGolden;
    aBytes[61] = std::byte{1};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::NonSequentialTick,
        "non-sequential behavior tick should fail");

    aBytes = aGolden;
    aBytes[69] = std::byte{6};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidScene,
        "invalid behavior scene should fail");

    aBytes = aGolden;
    aBytes[70] = std::byte{8};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidBoardStage,
        "invalid board stage should fail");

    aBytes = aGolden;
    aBytes[71] = std::byte{0};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidGridCoordinate,
        "half-empty grid coordinate should fail");

    aBytes = aGolden;
    aBytes[71] = std::byte{9};
    aBytes[72] = std::byte{0};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidGridCoordinate,
        "out-of-range grid coordinate should fail");

    aBytes = aGolden;
    aBytes[79] = std::byte{0x40};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidOccupiedCells,
        "out-of-range occupancy bit should fail");

    aBytes = aGolden;
    aBytes[92] = std::byte{3};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidSeedSelection,
        "invalid seed selection should fail");

    aBytes = aGolden;
    aBytes[93] = std::byte{6};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidTutorialPhase,
        "invalid tutorial phase should fail");

    aBytes = aGolden;
    aBytes[87] = std::byte{1};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidSeedRefresh,
        "invalid seed refresh state should fail");

    aBytes = aGolden;
    aBytes[94] = std::byte{1};
    aBytes[96] = std::byte{1};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidFirstSunState,
        "invalid first-sun state should fail");

    aBytes = aGolden;
    aBytes[101] = std::byte{4};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::
            InvalidLevelOneCombatState,
        "invalid Level 1 outcome should fail");

    aBytes = aGolden;
    aBytes[104] = std::byte{0x1D};
    aBytes[105] = std::byte{0x02};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::
            InvalidLevelOneCombatState,
        "invalid current-wave health should fail");

    aBytes = aGolden;
    aBytes.push_back(std::byte{0});
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::TrailingData,
        "trailing behavior data should fail");

    aBytes = aGolden;
    aBytes.pop_back();
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::IoError,
        "truncated behavior data should fail");

    auto aDecisionCapture = MakeCapture();
    pvz::parity::BehaviorCaptureError anError{};
    Expect(
        aDecisionCapture.AppendRandomDecision(
            {
                .mKind = pvz::game::LevelOneRandomDecisionKind::FallingSun,
                .mNextCountdown = 512,
                .mXMilliPixels = 321'000,
                .mGroundYMilliPixels = 444'000,
                .mSpeedMicroPixelsPerTick = 0,
            },
            anError),
        "valid random decision should append");
    const auto aDecisionGolden = SaveCapture(aDecisionCapture);

    aBytes = aDecisionGolden;
    aBytes[107] = std::byte{0xA1};
    aBytes[108] = std::byte{0x86};
    aBytes[109] = std::byte{0x01};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::TooManyRandomDecisions,
        "oversized random-decision count should fail");

    aBytes = aDecisionGolden;
    aBytes[111] = std::byte{7};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidRandomDecisionKind,
        "invalid random-decision kind should fail");

    aBytes = aDecisionGolden;
    aBytes[112] = std::byte{};
    aBytes[113] = std::byte{};
    ExpectLoadError(
        std::move(aBytes),
        pvz::parity::BehaviorCaptureError::InvalidRandomDecisionPayload,
        "invalid random-decision payload should fail");
}

void TestAppendAndSaveValidation()
{
    pvz::parity::BehaviorCapture aCapture;
    pvz::parity::BehaviorCaptureError anError{};
    pvz::game::BehaviorObservation anObservation;
    anObservation.mTick = 1;
    Expect(
        !aCapture.AppendObservation(anObservation, anError) &&
        anError ==
            pvz::parity::BehaviorCaptureError::NonSequentialTick,
        "append should reject non-sequential ticks");

    aCapture.SetProducer(
        pvz::parity::BehaviorProducer::PortableGameModule);
    aCapture.SetInputReplay(MakeInputReplay());
    pvz::engine::core::BinaryStateWriter aWriter;
    Expect(
        !aCapture.Save(aWriter, anError) &&
        anError == pvz::parity::BehaviorCaptureError::CountMismatch,
        "save should reject unequal input and behavior counts");

    Expect(
        !aCapture.AppendRandomDecision(
            {
                .mKind =
                    pvz::game::LevelOneRandomDecisionKind::NormalZombie,
            },
            anError) &&
        anError ==
            pvz::parity::BehaviorCaptureError::
                InvalidRandomDecisionPayload,
        "append should reject invalid random decisions");

    Expect(
        aCapture.AppendRandomDecision(
            {
                .mKind =
                    pvz::game::LevelOneRandomDecisionKind::WaveSchedule,
                .mNextCountdown = 2'750,
                .mWaveHealthThreshold = 150,
            },
            anError),
        "append should accept a fixed-width wave schedule decision");

    Expect(
        aCapture.AppendRandomDecision(
            {
                .mKind = pvz::game::LevelOneRandomDecisionKind::
                    PeashooterSchedule,
                .mNextCountdown = 145,
                .mPlantColumn = 2,
                .mShootingCounter = 33,
            },
            anError),
        "append should accept a fixed-width Peashooter schedule decision");

    Expect(
        aCapture.AppendRandomDecision(
            {
                .mKind = pvz::game::LevelOneRandomDecisionKind::
                    ProjectileSpawn,
                .mXMilliPixels = 320'000,
                .mGroundYMilliPixels = 280'000,
                .mPlantColumn = 2,
            },
            anError),
        "append should accept a fixed-width projectile spawn decision");

    Expect(
        aCapture.AppendRandomDecision(
            {
                .mKind = pvz::game::LevelOneRandomDecisionKind::
                    ZombieMotion,
                .mXMilliPixels = 640'000,
                .mPlantColumn = 0,
                .mShootingCounter = 1,
            },
            anError),
        "append should accept zombie motion with a fixed-width decay bit");

    Expect(
        aCapture.AppendRandomDecision(
            {
                .mKind = pvz::game::LevelOneRandomDecisionKind::
                    ProjectileMotion,
                .mXMilliPixels = 643'000,
                .mPlantColumn = 0,
            },
            anError),
        "append should accept fixed-width projectile motion");
}

void TestFirstBehaviorDifference()
{
    std::array<pvz::game::BehaviorObservation, 2> aLeft{};
    aLeft[0].mScene = pvz::game::BehaviorScene::Title;
    aLeft[1].mTick = 1;
    aLeft[1].mScene =
        pvz::game::BehaviorScene::AdventurePlaying;
    aLeft[1].mBoardStage =
        pvz::game::BehaviorBoardStage::Day;
    aLeft[1].mGridColumn = 2;
    aLeft[1].mGridRow = 3;
    aLeft[1].mOccupiedCells = std::uint64_t{1} << 29U;
    aLeft[1].mPlantCount = 1;

    auto aRight = aLeft;
    auto aDifference =
        pvz::parity::FindFirstBehaviorDifference(
            aLeft,
            aRight);
    Expect(
        aDifference.mField ==
            pvz::parity::BehaviorField::None &&
        aDifference.mTick ==
            pvz::parity::kNoBehaviorDifferenceTick,
        "equal behavior observations should have no difference");

    aRight[1].mGridColumn = 4;
    aDifference =
        pvz::parity::FindFirstBehaviorDifference(
            aLeft,
            aRight);
    Expect(
        aDifference.mTick == 1 &&
        aDifference.mField ==
            pvz::parity::BehaviorField::GridColumn &&
        pvz::parity::GetBehaviorFieldName(
            aDifference.mField) == "grid-column",
        "behavior comparison should identify exact first field");

    aRight = aLeft;
    aRight[1].mSun = 25;
    aDifference =
        pvz::parity::FindFirstBehaviorDifference(
            aLeft,
            aRight);
    Expect(
        aDifference.mTick == 1 &&
        aDifference.mField ==
            pvz::parity::BehaviorField::Sun &&
        pvz::parity::GetBehaviorFieldName(
            aDifference.mField) == "sun",
        "behavior comparison should cover economy fields");

    aDifference =
        pvz::parity::FindFirstBehaviorDifference(
            aLeft,
            aRight,
            1);
    Expect(
        aDifference.mField ==
            pvz::parity::BehaviorField::None,
        "version 1 comparison should use the common schema prefix");

    aRight = aLeft;
    aRight[1].mFirstSunSpawned = true;
    aDifference =
        pvz::parity::FindFirstBehaviorDifference(
            aLeft,
            aRight);
    Expect(
        aDifference.mTick == 1 &&
        aDifference.mField ==
            pvz::parity::BehaviorField::FirstSunSpawned,
        "behavior comparison should cover the first-sun gate");

    aRight = aLeft;
    aRight[1].mCurrentWave = 1;
    aDifference =
        pvz::parity::FindFirstBehaviorDifference(
            aLeft,
            aRight,
            4);
    Expect(
        aDifference.mTick == 1 &&
        aDifference.mField ==
            pvz::parity::BehaviorField::CurrentWave &&
        pvz::parity::GetBehaviorFieldName(
            aDifference.mField) == "current-wave",
        "version 4 comparison should cover combat progression");

    aRight = aLeft;
    aRight[1].mZombieWaveHealth = 250;
    aDifference =
        pvz::parity::FindFirstBehaviorDifference(
            aLeft,
            aRight,
            5);
    Expect(
        aDifference.mTick == 1 &&
        aDifference.mField ==
            pvz::parity::BehaviorField::ZombieWaveHealth &&
        pvz::parity::GetBehaviorFieldName(
            aDifference.mField) == "zombie-wave-health",
        "version 5 comparison should expose damage timing drift");

    aDifference =
        pvz::parity::FindFirstBehaviorDifference(
            std::span<const pvz::game::BehaviorObservation>(
                aLeft.data(),
                1),
            aRight);
    Expect(
        aDifference.mTick == 1 &&
        aDifference.mField ==
            pvz::parity::BehaviorField::ObservationCount,
        "behavior comparison should identify count divergence");
}

} // namespace

int main()
{
    TestGoldenBytesAndRoundTrip();
    TestMalformedCaptures();
    TestAppendAndSaveValidation();
    TestFirstBehaviorDifference();
    if (gFailures != 0)
    {
        std::cerr
            << gFailures
            << " behavior capture test(s) failed\n";
        return 1;
    }
    std::cout << "behavior capture tests passed\n";
    return 0;
}
