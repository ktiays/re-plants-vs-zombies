#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/DeterministicHash.h"
#include "pvz/engine/core/InputReplay.h"
#include "pvz/engine/core/NullFontResources.h"
#include "pvz/engine/core/NullImageStore.h"
#include "pvz/engine/core/NullMusicResources.h"
#include "pvz/engine/core/NullSoundResources.h"
#include "pvz/engine/core/ReplaySession.h"
#include "pvz/game/GameModule.h"
#include "pvz/game/LevelOneRandomDecision.h"
#include "pvz/parity/BehaviorCapture.h"
#include "pvz/parity/BehaviorComparison.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

inline constexpr std::uint64_t kMaximumReplayInputFileSize =
    320ULL * 1'024ULL * 1'024ULL;
inline constexpr std::uint32_t kInputReplayMagic = 0x525A5650;
inline constexpr std::uint32_t kBehaviorCaptureMagic = 0x425A5650;

class HeadlessLogger final : public pvz::engine::ILogger
{
public:
    void Log(
        pvz::engine::LogLevel theLevel,
        std::string_view theMessage) override
    {
        static_cast<void>(theLevel);
        std::cout << theMessage << '\n';
    }
};

class HeadlessServices final : public pvz::engine::IEngineServices
{
public:
    [[nodiscard]] pvz::engine::ILogger& GetLogger() override
    {
        return mLogger;
    }

    [[nodiscard]] pvz::engine::IResourceStore& GetResources() override
    {
        return mResources;
    }

    [[nodiscard]] pvz::engine::IXmlDocumentLoader&
    GetXmlDocuments() override
    {
        return mXmlDocuments;
    }

    [[nodiscard]] pvz::engine::IImageStore& GetImages() override
    {
        return mImages;
    }

    [[nodiscard]] pvz::engine::IImageResources&
    GetImageResources() override
    {
        return mImageResources;
    }

    [[nodiscard]] pvz::engine::IFontResources&
    GetFontResources() override
    {
        return mFontResources;
    }

    [[nodiscard]] pvz::engine::ISoundResources&
    GetSoundResources() override
    {
        return mSoundResources;
    }

    [[nodiscard]] pvz::engine::IMusicResources&
    GetMusicResources() override
    {
        return mMusicResources;
    }

private:
    class EmptyResourceStore final : public pvz::engine::IResourceStore
    {
    public:
        [[nodiscard]] bool Contains(
            std::string_view thePath) const override
        {
            static_cast<void>(thePath);
            return false;
        }

        [[nodiscard]] bool GetSize(
            std::string_view thePath,
            std::uint64_t& theSize) const override
        {
            static_cast<void>(thePath);
            static_cast<void>(theSize);
            return false;
        }

        [[nodiscard]] bool ReadAll(
            std::string_view thePath,
            std::vector<std::byte>& theBytes) const override
        {
            static_cast<void>(thePath);
            static_cast<void>(theBytes);
            return false;
        }
    };

    class EmptyXmlDocumentLoader final
        : public pvz::engine::IXmlDocumentLoader
    {
    public:
        [[nodiscard]] bool Load(
            std::string_view thePath,
            pvz::engine::XmlDocumentMode theMode,
            std::vector<pvz::engine::XmlNode>& theRoots,
            pvz::engine::XmlDocumentDiagnostic& theDiagnostic)
            const override
        {
            static_cast<void>(thePath);
            static_cast<void>(theMode);
            static_cast<void>(theRoots);
            theDiagnostic.mError =
                pvz::engine::XmlDocumentError::ResourceReadFailed;
            theDiagnostic.mLine = 0;
            return false;
        }
    };

    HeadlessLogger mLogger;
    EmptyResourceStore mResources;
    EmptyXmlDocumentLoader mXmlDocuments;
    pvz::engine::core::NullImageStore mImages;
    pvz::engine::core::NullImageResources mImageResources;
    pvz::engine::core::NullFontResources mFontResources;
    pvz::engine::core::NullSoundResources mSoundResources;
    pvz::engine::core::NullMusicResources mMusicResources;
};

[[nodiscard]] bool AppendReplayFrame(
    pvz::engine::core::InputReplay& theReplay,
    pvz::engine::core::RecordedInputFrame theFrame)
{
    pvz::engine::core::InputReplayError anError{};
    return theReplay.AppendFrame(
        std::move(theFrame),
        anError);
}

[[nodiscard]] bool BuildAdventureReplay(
    pvz::engine::core::InputReplay& theReplay)
{
    constexpr pvz::engine::TickIndex kFrameCount = 1'310;
    pvz::engine::PointI aPointerPosition{};
    for (pvz::engine::TickIndex aTick = 0;
         aTick < kFrameCount;
         ++aTick)
    {
        pvz::engine::core::RecordedInputFrame aFrame;
        aFrame.mTick = aTick;
        aFrame.mPointer.mPosition = aPointerPosition;
        if (aTick == 0 || aTick == 1)
        {
            aFrame.SetKeyDown(
                pvz::engine::KeyCode::Enter,
                true);
            aFrame.SetKeyPressed(
                pvz::engine::KeyCode::Enter,
                true);
        }
        else if (aTick == 1'307)
        {
            aFrame.SetPointerButtonDown(
                pvz::engine::PointerButton::Primary,
                true);
            aFrame.SetPointerButtonPressed(
                pvz::engine::PointerButton::Primary,
                true);
            aFrame.mPointer.mPosition = {100, 20};
            aPointerPosition = aFrame.mPointer.mPosition;
        }
        else if (aTick == 1'308)
        {
            aFrame.SetPointerButtonDown(
                pvz::engine::PointerButton::Primary,
                true);
            aFrame.SetPointerButtonPressed(
                pvz::engine::PointerButton::Primary,
                true);
            aFrame.mPointer.mPosition = {320, 330};
            aPointerPosition = aFrame.mPointer.mPosition;
        }
        else if (aTick == 1'309)
        {
            aFrame.SetKeyDown(
                pvz::engine::KeyCode::ArrowLeft,
                true);
            aFrame.SetKeyPressed(
                pvz::engine::KeyCode::ArrowLeft,
                true);
        }
        if (!AppendReplayFrame(theReplay, std::move(aFrame)))
            return false;
    }
    return true;
}

[[nodiscard]] bool LoadReplayInput(
    const std::filesystem::path& thePath,
    pvz::engine::core::InputReplay& theReplay,
    std::vector<pvz::game::LevelOneRandomDecision>& theRandomDecisions,
    std::vector<pvz::game::BehaviorObservation>&
        theExpectedObservations,
    std::uint16_t& theExpectedBehaviorVersion,
    bool& theHasRandomDecisionTape,
    bool& theSupportsWaveSchedule,
    bool& theSupportsPeashooterSchedule,
    bool& theSupportsProjectileSpawn,
    bool& theSupportsZombieMotion,
    bool& theSupportsProjectileMotion)
{
    std::error_code anError;
    const auto aFileSize =
        std::filesystem::file_size(thePath, anError);
    if (anError)
    {
        std::cerr
            << thePath.string()
            << ": could not read replay size: "
            << anError.message()
            << '\n';
        return false;
    }
    if (aFileSize > kMaximumReplayInputFileSize)
    {
        std::cerr
            << thePath.string()
            << ": replay input exceeds the 320 MiB limit\n";
        return false;
    }

    std::ifstream aStream(thePath, std::ios::binary);
    if (!aStream)
    {
        std::cerr
            << thePath.string()
            << ": could not open replay\n";
        return false;
    }
    std::vector<std::byte> aBytes(
        static_cast<std::size_t>(aFileSize));
    if (!aBytes.empty())
    {
        aStream.read(
            reinterpret_cast<char*>(aBytes.data()),
            static_cast<std::streamsize>(aBytes.size()));
    }
    if (!aStream ||
        aStream.gcount() !=
            static_cast<std::streamsize>(aBytes.size()))
    {
        std::cerr
            << thePath.string()
            << ": could not read complete replay\n";
        return false;
    }

    pvz::engine::core::BinaryStateReader aHeaderReader(aBytes);
    std::uint32_t aMagic{};
    if (!aHeaderReader.ReadU32(aMagic))
    {
        std::cerr
            << thePath.string()
            << ": replay header is truncated\n";
        return false;
    }
    if (aMagic == kInputReplayMagic)
    {
        pvz::engine::core::BinaryStateReader aReader(aBytes);
        pvz::engine::core::InputReplayError aReplayError{};
        if (!theReplay.Load(aReader, aReplayError))
        {
            std::cerr
                << thePath.string()
                << ": "
                << pvz::engine::core::GetInputReplayErrorMessage(
                       aReplayError)
                << '\n';
            return false;
        }
        return true;
    }
    if (aMagic == kBehaviorCaptureMagic)
    {
        pvz::engine::core::BinaryStateReader aReader(aBytes);
        pvz::parity::BehaviorCapture aCapture;
        pvz::parity::BehaviorCaptureError aCaptureError{};
        if (!aCapture.Load(aReader, aCaptureError))
        {
            std::cerr
                << thePath.string()
                << ": "
                << pvz::parity::GetBehaviorCaptureErrorMessage(
                       aCaptureError)
                << '\n';
            return false;
        }
        theReplay = aCapture.GetInputReplay();
        theRandomDecisions.assign(
            aCapture.GetRandomDecisions().begin(),
            aCapture.GetRandomDecisions().end());
        theExpectedObservations.assign(
            aCapture.GetObservations().begin(),
            aCapture.GetObservations().end());
        theExpectedBehaviorVersion =
            aCapture.GetFormatVersion();
        theHasRandomDecisionTape =
            aCapture.GetFormatVersion() >= 3;
        theSupportsWaveSchedule =
            aCapture.GetFormatVersion() >= 4;
        theSupportsPeashooterSchedule =
            aCapture.GetFormatVersion() >= 5;
        theSupportsProjectileSpawn =
            aCapture.GetFormatVersion() >= 5;
        theSupportsZombieMotion =
            aCapture.GetFormatVersion() >= 5;
        theSupportsProjectileMotion =
            aCapture.GetFormatVersion() >= 6;
        return true;
    }
    std::cerr
        << thePath.string()
        << ": replay magic is neither PVZR nor PVZB\n";
    return false;
}

class HeadlessRenderFrame final : public pvz::engine::IRenderFrame
{
public:
    [[nodiscard]] pvz::engine::SizeI GetLogicalSize() const override
    {
        return {800, 600};
    }

    void Clear(pvz::engine::ColorRgba8 theColor) override
    {
        mClearColor = theColor;
    }

    void SubmitSprites(
        std::span<const pvz::engine::SpriteDraw> theDraws) override
    {
        mSpriteCount += static_cast<std::uint64_t>(theDraws.size());
    }

private:
    pvz::engine::ColorRgba8 mClearColor{};
    std::uint64_t mSpriteCount{};
};

} // namespace

int main(int theArgumentCount, char** theArguments)
{
    std::optional<std::filesystem::path> aBehaviorOutputPath;
    std::optional<std::filesystem::path> aSessionOutputPath;
    std::optional<std::filesystem::path> aReplayInputPath;
    for (int anArgumentIndex = 1;
         anArgumentIndex < theArgumentCount;
         ++anArgumentIndex)
    {
        const std::string_view anArgument(
            theArguments[anArgumentIndex]);
        if (anArgument == "--write-session" &&
            !aSessionOutputPath.has_value() &&
            anArgumentIndex + 1 < theArgumentCount)
        {
            ++anArgumentIndex;
            aSessionOutputPath =
                std::filesystem::path(
                    theArguments[anArgumentIndex]);
        }
        else if (anArgument == "--write-behavior" &&
                 !aBehaviorOutputPath.has_value() &&
                 anArgumentIndex + 1 < theArgumentCount)
        {
            ++anArgumentIndex;
            aBehaviorOutputPath =
                std::filesystem::path(
                    theArguments[anArgumentIndex]);
        }
        else if (anArgument == "--replay" &&
                 !aReplayInputPath.has_value() &&
                 anArgumentIndex + 1 < theArgumentCount)
        {
            ++anArgumentIndex;
            aReplayInputPath =
                std::filesystem::path(
                    theArguments[anArgumentIndex]);
        }
        else
        {
            std::cerr
                << "usage: pvz_game_headless "
                   "[--replay input.pvzr-or-pvzb] "
                   "[--write-session capture.pvzc] "
                   "[--write-behavior capture.pvzb]\n";
            return 2;
        }
    }

    HeadlessServices aServices;
    HeadlessRenderFrame aFrame;
    pvz::game::GameModule aGame;
    pvz::engine::core::ReplayRecordingGame
        aRecordingGame(aGame);
    pvz::engine::core::InputReplay aReplay;
    std::vector<pvz::game::LevelOneRandomDecision>
        aRandomDecisions;
    std::vector<pvz::game::BehaviorObservation>
        anExpectedBehaviorObservations;
    std::uint16_t anExpectedBehaviorVersion{};
    bool hasRandomDecisionTape = false;
    bool supportsWaveSchedule = false;
    bool supportsPeashooterSchedule = false;
    bool supportsProjectileSpawn = false;
    bool supportsZombieMotion = false;
    bool supportsProjectileMotion = false;

    if (aReplayInputPath.has_value()
            ? !LoadReplayInput(
                  *aReplayInputPath,
                  aReplay,
                  aRandomDecisions,
                  anExpectedBehaviorObservations,
                  anExpectedBehaviorVersion,
                  hasRandomDecisionTape,
                  supportsWaveSchedule,
                  supportsPeashooterSchedule,
                  supportsProjectileSpawn,
                  supportsZombieMotion,
                  supportsProjectileMotion)
            : !BuildAdventureReplay(aReplay))
    {
        return 1;
    }

    pvz::engine::core::BinaryStateWriter aReplayWriter;
    pvz::engine::core::InputReplayError aReplayError{};
    if (!aReplay.Save(aReplayWriter, aReplayError))
        return 1;
    pvz::engine::core::BinaryStateReader aReplayReader(
        aReplayWriter.GetBytes());
    pvz::engine::core::InputReplay aLoadedReplay;
    if (!aLoadedReplay.Load(aReplayReader, aReplayError))
        return 1;

    pvz::parity::BehaviorCapture aBehaviorCapture;
    aBehaviorCapture.SetProducer(
        pvz::parity::BehaviorProducer::PortableGameModule);
    aBehaviorCapture.SetInputReplay(aLoadedReplay);
    pvz::parity::BehaviorCaptureError aBehaviorError{};
    for (const auto& aDecision : aRandomDecisions)
    {
        if (!aBehaviorCapture.AppendRandomDecision(
                aDecision,
                aBehaviorError))
        {
            return 1;
        }
    }

    std::optional<pvz::game::LevelOneRandomDecisionTape>
        aRandomDecisionTape;
    if (hasRandomDecisionTape)
    {
        aRandomDecisionTape.emplace(
            aRandomDecisions,
            supportsWaveSchedule,
            supportsPeashooterSchedule,
            supportsProjectileSpawn,
            supportsZombieMotion,
            supportsProjectileMotion);
        if (!aGame.SetLevelOneRandomDecisionSource(
                &*aRandomDecisionTape))
        {
            return 1;
        }
    }

    if (aRecordingGame.Initialize(aServices) !=
        pvz::engine::LifecycleResult::Success)
    {
        return 1;
    }

    std::optional<std::uint64_t> aRandomDecisionFailureTick;
    for (const auto& aRecordedFrame :
         aLoadedReplay.GetFrames())
    {
        const pvz::engine::core::ReplayInputFrame anInput(
            aRecordedFrame);
        aRecordingGame.Update(
            pvz::engine::GameTick{aRecordedFrame.mTick},
            anInput);
        if (aGame.HasRandomDecisionFailure() &&
            !aRandomDecisionFailureTick.has_value())
        {
            aRandomDecisionFailureTick =
                aRecordedFrame.mTick;
        }
        if (!aBehaviorCapture.AppendObservation(
                aGame.GetBehaviorObservation(),
                aBehaviorError))
        {
            return 1;
        }
    }
    if (aRecordingGame.GetRecordingError() !=
        pvz::engine::core::ReplayRecordingError::None)
    {
        return 1;
    }
    if (!anExpectedBehaviorObservations.empty())
    {
        const auto anActualObservations =
            aBehaviorCapture.GetObservations();
        const auto aDifference =
            pvz::parity::FindFirstBehaviorDifference(
                anExpectedBehaviorObservations,
                anActualObservations,
                anExpectedBehaviorVersion);
        if (aDifference.mTick !=
            pvz::parity::kNoBehaviorDifferenceTick)
        {
            const auto aDifferenceIndex =
                static_cast<std::size_t>(aDifference.mTick);
            const auto& anExpected =
                anExpectedBehaviorObservations[aDifferenceIndex];
            const auto& anActual =
                anActualObservations[aDifferenceIndex];
            std::cerr
                << "behavior-replay-first-difference"
                << " tick=" << aDifference.mTick
                << " field="
                << pvz::parity::GetBehaviorFieldName(
                       aDifference.mField)
                << " expected-sun=" << anExpected.mSun
                << " actual-sun=" << anActual.mSun
                << " expected-plants="
                << anExpected.mPlantCount
                << " actual-plants="
                << anActual.mPlantCount
                << '\n';
        }
        const auto aPlayingIterator = std::find_if(
            anExpectedBehaviorObservations.begin(),
            anExpectedBehaviorObservations.end(),
            [](const pvz::game::BehaviorObservation& theObservation)
            {
                return theObservation.mScene ==
                    pvz::game::BehaviorScene::AdventurePlaying;
            });
        if (aPlayingIterator !=
            anExpectedBehaviorObservations.end())
        {
            const auto aPlayingIndex =
                static_cast<std::size_t>(std::distance(
                    anExpectedBehaviorObservations.begin(),
                    aPlayingIterator));
            const auto aPlayingDifference =
                pvz::parity::FindFirstBehaviorDifference(
                    std::span<const pvz::game::BehaviorObservation>(
                        anExpectedBehaviorObservations)
                        .subspan(aPlayingIndex),
                    anActualObservations.subspan(aPlayingIndex),
                    anExpectedBehaviorVersion);
            if (aPlayingDifference.mTick !=
                pvz::parity::kNoBehaviorDifferenceTick)
            {
                const auto aPlayingDifferenceIndex =
                    aPlayingIndex +
                    static_cast<std::size_t>(
                        aPlayingDifference.mTick);
                const auto& anExpected =
                    anExpectedBehaviorObservations[
                        aPlayingDifferenceIndex];
                const auto& anActual =
                    anActualObservations[
                        aPlayingDifferenceIndex];
                std::cerr
                    << "behavior-replay-playing-first-difference"
                    << " tick=" << aPlayingDifferenceIndex
                    << " field="
                    << pvz::parity::GetBehaviorFieldName(
                           aPlayingDifference.mField)
                    << " expected-sun=" << anExpected.mSun
                    << " actual-sun=" << anActual.mSun
                    << " expected-plants="
                    << static_cast<std::uint32_t>(
                           anExpected.mPlantCount)
                    << " actual-plants="
                    << static_cast<std::uint32_t>(
                           anActual.mPlantCount)
                    << " expected-wave-health="
                    << anExpected.mZombieWaveHealth
                    << " actual-wave-health="
                    << anActual.mZombieWaveHealth
                    << " expected-projectiles="
                    << static_cast<std::uint32_t>(
                           anExpected.mProjectileCount)
                    << " actual-projectiles="
                    << static_cast<std::uint32_t>(
                           anActual.mProjectileCount)
                    << '\n';
            }

            if (anExpectedBehaviorVersion >= 4)
            {
                const auto aCombatCount = std::min(
                    anExpectedBehaviorObservations.size(),
                    anActualObservations.size());
                for (auto anIndex = aPlayingIndex;
                     anIndex < aCombatCount;
                     ++anIndex)
                {
                    const auto& anExpected =
                        anExpectedBehaviorObservations[anIndex];
                    const auto& anActual =
                        anActualObservations[anIndex];
                    auto aField = pvz::parity::BehaviorField::None;
                    if (anExpected.mCurrentWave !=
                        anActual.mCurrentWave)
                    {
                        aField =
                            pvz::parity::BehaviorField::CurrentWave;
                    }
                    else if (anExpected.mZombieWaveHealth !=
                             anActual.mZombieWaveHealth)
                    {
                        aField = pvz::parity::BehaviorField::
                            ZombieWaveHealth;
                    }
                    else if (anExpected.mProjectileCount !=
                             anActual.mProjectileCount)
                    {
                        aField = pvz::parity::BehaviorField::
                            ProjectileCount;
                    }
                    else if (anExpected.mZombieCountdown !=
                             anActual.mZombieCountdown)
                    {
                        aField = pvz::parity::BehaviorField::
                            ZombieCountdown;
                    }
                    else if (anExpected.mZombieCount !=
                             anActual.mZombieCount)
                    {
                        aField =
                            pvz::parity::BehaviorField::ZombieCount;
                    }
                    else if (anExpected.mLevelOutcome !=
                             anActual.mLevelOutcome)
                    {
                        aField =
                            pvz::parity::BehaviorField::LevelOutcome;
                    }
                    else if (anExpected.mMowerState !=
                             anActual.mMowerState)
                    {
                        aField =
                            pvz::parity::BehaviorField::MowerState;
                    }
                    else if (anExpected.mLevelAwardSpawned !=
                             anActual.mLevelAwardSpawned)
                    {
                        aField = pvz::parity::BehaviorField::
                            LevelAwardSpawned;
                    }
                    if (aField != pvz::parity::BehaviorField::None)
                    {
                        std::cerr
                            << "behavior-replay-combat-first-difference"
                            << " tick=" << anIndex
                            << " field="
                            << pvz::parity::GetBehaviorFieldName(aField)
                            << " expected-wave="
                            << static_cast<std::uint32_t>(
                                   anExpected.mCurrentWave)
                            << " actual-wave="
                            << static_cast<std::uint32_t>(
                                   anActual.mCurrentWave)
                            << " expected-countdown="
                            << anExpected.mZombieCountdown
                            << " actual-countdown="
                            << anActual.mZombieCountdown
                            << " expected-zombies="
                            << static_cast<std::uint32_t>(
                                   anExpected.mZombieCount)
                            << " actual-zombies="
                            << static_cast<std::uint32_t>(
                                   anActual.mZombieCount)
                            << " expected-wave-health="
                            << anExpected.mZombieWaveHealth
                            << " actual-wave-health="
                            << anActual.mZombieWaveHealth
                            << " expected-projectiles="
                            << static_cast<std::uint32_t>(
                                   anExpected.mProjectileCount)
                            << " actual-projectiles="
                            << static_cast<std::uint32_t>(
                                   anActual.mProjectileCount)
                            << '\n';
                        break;
                    }
                }
            }
        }
    }
    const auto writeDiagnosticBehavior = [&]() -> bool
    {
        if (!aBehaviorOutputPath.has_value())
            return true;
        pvz::engine::core::BinaryStateWriter aDiagnosticWriter;
        if (!aBehaviorCapture.Save(
                aDiagnosticWriter,
                aBehaviorError))
        {
            return false;
        }
        std::ofstream aDiagnosticStream(
            *aBehaviorOutputPath,
            std::ios::binary | std::ios::trunc);
        const auto aDiagnosticBytes =
            aDiagnosticWriter.GetBytes();
        if (!aDiagnosticStream)
            return false;
        aDiagnosticStream.write(
            reinterpret_cast<const char*>(
                aDiagnosticBytes.data()),
            static_cast<std::streamsize>(
                aDiagnosticBytes.size()));
        return static_cast<bool>(aDiagnosticStream);
    };
    if (aGame.HasRandomDecisionFailure())
    {
        if (!writeDiagnosticBehavior())
            return 1;
        const auto aReadCount =
            aRandomDecisionTape->GetReadCount();
        const auto aTapeError =
            aRandomDecisionTape->GetError();
        const auto aCandidateIndex =
            aTapeError ==
                    pvz::game::LevelOneRandomDecisionReadError::None &&
                aReadCount > 0
            ? aReadCount - 1U
            : aReadCount;
        std::cerr
            << "random-decision-tape-rejected"
            << " tick="
            << aRandomDecisionFailureTick.value_or(0)
            << " read="
            << aRandomDecisionTape->GetReadCount()
            << " remaining="
            << aRandomDecisionTape->GetRemainingCount()
            << " error="
            << static_cast<std::uint32_t>(
                   aRandomDecisionTape->GetError())
            << " expected-kind="
            << static_cast<std::uint32_t>(
                   aRandomDecisionTape->GetExpectedKind())
            << " actual-kind="
            << static_cast<std::uint32_t>(
                   aRandomDecisionTape->GetActualKind());
        if (aCandidateIndex < aRandomDecisions.size())
        {
            const auto& aDecision =
                aRandomDecisions[aCandidateIndex];
            std::cerr
                << " decision-index=" << aCandidateIndex
                << " next-countdown="
                << aDecision.mNextCountdown
                << " x-millipixels="
                << aDecision.mXMilliPixels
                << " ground-y-millipixels="
                << aDecision.mGroundYMilliPixels
                << " speed-micropixels-per-tick="
                << aDecision.mSpeedMicroPixelsPerTick
                << " wave-health-threshold="
                << aDecision.mWaveHealthThreshold
                << " plant-column="
                << static_cast<std::uint32_t>(
                       aDecision.mPlantColumn)
                << " shooting-counter="
                << static_cast<std::uint32_t>(
                       aDecision.mShootingCounter);
        }
        std::cerr << '\n';
        return 1;
    }
    if (aRandomDecisionTape.has_value() &&
        aRandomDecisionTape->GetRemainingCount() != 0)
    {
        if (!writeDiagnosticBehavior())
            return 1;
        std::cerr
            << "random-decision-tape-unused="
            << aRandomDecisionTape->GetRemainingCount()
            << '\n';
        return 1;
    }

    aRecordingGame.Render(aFrame);

    pvz::engine::core::BinaryStateWriter aWriter;
    if (!aRecordingGame.SaveState(aWriter))
        return 1;

    const auto aFinalHash =
        pvz::engine::core::CalculateFnv1a64(
            aWriter.GetBytes());
    const auto& aRecordedSession =
        aRecordingGame.GetSession();
    const auto aTranscriptHash =
        aRecordedSession.GetTranscriptHash();
    const auto aFlowState = aGame.GetFlowState();
    const auto aLevelOneBoardState =
        aGame.GetLevelOneBoardState();
    const auto aLevelOneCombatState =
        aGame.GetLevelOneCombatState();
    const auto aBehaviorObservations =
        aBehaviorCapture.GetObservations();
    const bool hasExpectedBuiltInState =
        aReplayInputPath.has_value() ||
        aGame.GetScene() ==
            pvz::game::GameScene::AdventureDay &&
        aGame.GetLastTick() == 1'309 &&
        aGame.GetUpdateCount() == 1'310 &&
        aFlowState.mGridColumn == 2 &&
        aFlowState.mGridRow == 2 &&
        aFlowState.mOccupiedCells ==
            (std::uint64_t{1} << 21U) &&
        aLevelOneBoardState.mSun == 50 &&
        aLevelOneBoardState.mSeedRefreshCounter == 2 &&
        aLevelOneBoardState.mSeedRefreshing &&
        aLevelOneBoardState.mSeedSelection ==
            pvz::game::LevelOneSeedSelection::None &&
        aLevelOneCombatState.mPhase ==
            pvz::game::LevelOneCombatPhase::
                AwaitingSecondPlant &&
        aLevelOneCombatState.mTick == 4 &&
        aLevelOneCombatState.mPlantCount == 1 &&
        aLevelOneCombatState.mSunCountdown == 398 &&
        aBehaviorObservations.size() == 1'310 &&
        aBehaviorObservations.back().mScene ==
            pvz::game::BehaviorScene::AdventurePlaying &&
        aBehaviorObservations.back().mBoardStage ==
            pvz::game::BehaviorBoardStage::Day &&
        aBehaviorObservations.back().mGridColumn == 2 &&
        aBehaviorObservations.back().mGridRow == 2 &&
        aBehaviorObservations.back().mOccupiedCells ==
            aFlowState.mOccupiedCells &&
        aBehaviorObservations.back().mPlantCount == 1 &&
        aFinalHash == 7'771'277'013'250'427'510ULL &&
        aTranscriptHash == 11'892'826'481'577'727'771ULL;

    pvz::engine::core::BinaryStateWriter aSessionWriter;
    pvz::engine::core::ReplaySessionError aSessionError{};
    if (!aRecordedSession.Save(
            aSessionWriter,
            aSessionError))
    {
        return 1;
    }
    pvz::engine::core::BinaryStateReader aSessionReader(
        aSessionWriter.GetBytes());
    pvz::engine::core::ReplaySession aLoadedSession;
    if (!aLoadedSession.Load(
            aSessionReader,
            aSessionError) ||
        aLoadedSession.GetFinalStateHash() !=
            (aLoadedReplay.GetFrames().empty()
                 ? std::uint64_t{}
                 : aFinalHash) ||
        aLoadedSession.GetTranscriptHash() !=
            aTranscriptHash)
    {
        return 1;
    }
    if (aSessionOutputPath.has_value())
    {
        std::ofstream aStream(
            *aSessionOutputPath,
            std::ios::binary | std::ios::trunc);
        const auto aBytes = aSessionWriter.GetBytes();
        if (!aStream)
            return 1;
        aStream.write(
            reinterpret_cast<const char*>(aBytes.data()),
            static_cast<std::streamsize>(aBytes.size()));
        if (!aStream)
            return 1;
    }

    pvz::engine::core::BinaryStateWriter aBehaviorWriter;
    if (!aBehaviorCapture.Save(
            aBehaviorWriter,
            aBehaviorError))
    {
        return 1;
    }
    pvz::engine::core::BinaryStateReader aBehaviorReader(
        aBehaviorWriter.GetBytes());
    pvz::parity::BehaviorCapture aLoadedBehavior;
    if (!aLoadedBehavior.Load(
            aBehaviorReader,
            aBehaviorError) ||
        aLoadedBehavior.GetProducer() !=
            pvz::parity::BehaviorProducer::PortableGameModule ||
        aLoadedBehavior.GetObservations().size() !=
            aLoadedReplay.GetFrames().size() ||
        aLoadedBehavior.GetRandomDecisions().size() !=
            aRandomDecisions.size())
    {
        return 1;
    }
    if (aBehaviorOutputPath.has_value())
    {
        std::ofstream aStream(
            *aBehaviorOutputPath,
            std::ios::binary | std::ios::trunc);
        const auto aBytes = aBehaviorWriter.GetBytes();
        if (!aStream)
            return 1;
        aStream.write(
            reinterpret_cast<const char*>(aBytes.data()),
            static_cast<std::streamsize>(aBytes.size()));
        if (!aStream)
            return 1;
    }

    std::cout << "replay-frames="
              << aLoadedReplay.GetFrames().size()
              << " replay-bytes="
              << aReplayWriter.GetBytesWritten()
              << " state-bytes=" << aWriter.GetBytesWritten()
              << " session-bytes="
              << aSessionWriter.GetBytesWritten()
              << " behavior-bytes="
              << aBehaviorWriter.GetBytesWritten()
              << " random-decisions="
              << aRandomDecisions.size()
              << " state-fnv1a=" << aFinalHash
              << " transcript-fnv1a=" << aTranscriptHash
              << '\n';
    aRecordingGame.Shutdown();
    return hasExpectedBuiltInState ? 0 : 1;
}
