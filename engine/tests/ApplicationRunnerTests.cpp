#include "pvz/engine/core/ApplicationRunner.h"
#include "pvz/engine/core/DeterministicHash.h"
#include "pvz/engine/core/NullFontResources.h"
#include "pvz/engine/core/NullImageStore.h"
#include "pvz/engine/core/NullMusicResources.h"
#include "pvz/engine/core/NullSoundResources.h"
#include "pvz/engine/core/ReplaySession.h"

#include "pvz/engine/core/BinaryStateIO.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

void Expect(bool theCondition, const char* theMessage);

namespace
{

class TestClock final : public pvz::engine::IMonotonicClock
{
public:
    [[nodiscard]] pvz::engine::MonotonicTimeMicroseconds
    GetTime() const override
    {
        return mTime;
    }

    void SetTime(pvz::engine::MonotonicTimeMicroseconds theTime)
    {
        mTime = theTime;
    }

private:
    pvz::engine::MonotonicTimeMicroseconds mTime{};
};

class TestEventLoop final : public pvz::engine::IPlatformEventLoop
{
public:
    explicit TestEventLoop(TestClock& theClock)
        : mClock(theClock)
    {
    }

    void PumpEvents() override
    {
        ++mPumpCount;
        if (mPumpCount >= 5)
            mQuitRequested = true;
    }

    [[nodiscard]] bool IsQuitRequested() const override
    {
        return mQuitRequested;
    }

    [[nodiscard]] bool IsSuspended() const override
    {
        return false;
    }

    void WaitUntil(
        pvz::engine::MonotonicTimeMicroseconds theDeadline) override
    {
        if (theDeadline > mClock.GetTime())
            mClock.SetTime(theDeadline);
    }

private:
    TestClock& mClock;
    std::uint32_t mPumpCount{};
    bool mQuitRequested{};
};

class TestInputFrame final : public pvz::engine::IInputFrame
{
public:
    [[nodiscard]] bool IsKeyDown(
        pvz::engine::KeyCode theKey) const override
    {
        static_cast<void>(theKey);
        return false;
    }

    [[nodiscard]] bool WasKeyPressed(
        pvz::engine::KeyCode theKey) const override
    {
        static_cast<void>(theKey);
        return false;
    }

    [[nodiscard]] bool IsPointerButtonDown(
        pvz::engine::PointerButton theButton) const override
    {
        static_cast<void>(theButton);
        return false;
    }

    [[nodiscard]] bool WasPointerButtonPressed(
        pvz::engine::PointerButton theButton) const override
    {
        static_cast<void>(theButton);
        return false;
    }

    [[nodiscard]] pvz::engine::PointerState GetPointerState()
        const override
    {
        return {};
    }

    [[nodiscard]] std::span<const char32_t> GetTextInput() const override
    {
        return {};
    }
};

class TestInputSystem final : public pvz::engine::IInputSystem
{
public:
    [[nodiscard]] const pvz::engine::IInputFrame&
    GetFrame() const override
    {
        return mFrame;
    }

    void ConsumeTransientEvents() override
    {
        ++mConsumeCount;
    }

    [[nodiscard]] std::uint32_t GetConsumeCount() const
    {
        return mConsumeCount;
    }

private:
    TestInputFrame mFrame;
    std::uint32_t mConsumeCount{};
};

class TestRenderFrame final : public pvz::engine::IRenderFrame
{
public:
    [[nodiscard]] pvz::engine::SizeI GetLogicalSize() const override
    {
        return {800, 600};
    }

    void Clear(pvz::engine::ColorRgba8 theColor) override
    {
        static_cast<void>(theColor);
        ++mClearCount;
    }

    void SubmitSprites(
        std::span<const pvz::engine::SpriteDraw> theDraws) override
    {
        static_cast<void>(theDraws);
    }

    [[nodiscard]] std::uint32_t GetClearCount() const
    {
        return mClearCount;
    }

private:
    std::uint32_t mClearCount{};
};

class TestRenderDevice final : public pvz::engine::IRenderDevice
{
public:
    [[nodiscard]] pvz::engine::RenderFrameResult BeginFrame(
        pvz::engine::IRenderFrame*& theFrame) override
    {
        ++mBeginCount;
        theFrame = &mFrame;
        return pvz::engine::RenderFrameResult::Ready;
    }

    [[nodiscard]] bool EndFrame() override
    {
        ++mEndCount;
        return true;
    }

    [[nodiscard]] std::uint32_t GetBeginCount() const
    {
        return mBeginCount;
    }

    [[nodiscard]] std::uint32_t GetEndCount() const
    {
        return mEndCount;
    }

    [[nodiscard]] const TestRenderFrame& GetFrame() const
    {
        return mFrame;
    }

private:
    TestRenderFrame mFrame;
    std::uint32_t mBeginCount{};
    std::uint32_t mEndCount{};
};

class TestLogger final : public pvz::engine::ILogger
{
public:
    void Log(
        pvz::engine::LogLevel theLevel,
        std::string_view theMessage) override
    {
        static_cast<void>(theLevel);
        static_cast<void>(theMessage);
        ++mMessageCount;
    }

    [[nodiscard]] std::uint32_t GetMessageCount() const
    {
        return mMessageCount;
    }

private:
    std::uint32_t mMessageCount{};
};

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
        return false;
    }
};

class TestServices final : public pvz::engine::IEngineServices
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
        return mDocuments;
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

    [[nodiscard]] const TestLogger& GetTestLogger() const
    {
        return mLogger;
    }

private:
    TestLogger mLogger;
    EmptyResourceStore mResources;
    EmptyXmlDocumentLoader mDocuments;
    pvz::engine::core::NullImageStore mImages;
    pvz::engine::core::NullImageResources mImageResources;
    pvz::engine::core::NullFontResources mFontResources;
    pvz::engine::core::NullSoundResources mSoundResources;
    pvz::engine::core::NullMusicResources mMusicResources;
};

class TestGame final : public pvz::engine::IGame
{
public:
    [[nodiscard]] pvz::engine::LifecycleResult Initialize(
        pvz::engine::IEngineServices& theServices) override
    {
        mServices = &theServices;
        mServices->GetLogger().Log(
            pvz::engine::LogLevel::Information,
            "initialized");
        return pvz::engine::LifecycleResult::Success;
    }

    void Update(
        const pvz::engine::GameTick& theTick,
        const pvz::engine::IInputFrame& theInput) override
    {
        static_cast<void>(theInput);
        mLastTick = theTick.mIndex;
        ++mUpdateCount;
    }

    void Render(pvz::engine::IRenderFrame& theFrame) const override
    {
        theFrame.Clear({1, 2, 3, 255});
    }

    [[nodiscard]] bool LoadState(
        pvz::engine::IStateReader& theReader) override
    {
        static_cast<void>(theReader);
        return true;
    }

    [[nodiscard]] bool SaveState(
        pvz::engine::IStateWriter& theWriter) const override
    {
        if (mFailStateSave)
            return false;
        return
            theWriter.WriteU64(mLastTick) &&
            theWriter.WriteU64(mUpdateCount);
    }

    void Suspend() override
    {
    }

    void Resume() override
    {
    }

    void Shutdown() override
    {
        mServices->GetLogger().Log(
            pvz::engine::LogLevel::Information,
            "shutdown");
        mServices = nullptr;
    }

    [[nodiscard]] std::uint64_t GetUpdateCount() const
    {
        return mUpdateCount;
    }

    [[nodiscard]] pvz::engine::TickIndex GetLastTick() const
    {
        return mLastTick;
    }

    void SetFailStateSave(bool theFailStateSave)
    {
        mFailStateSave = theFailStateSave;
    }

private:
    pvz::engine::IEngineServices* mServices{};
    pvz::engine::TickIndex mLastTick{};
    std::uint64_t mUpdateCount{};
    bool mFailStateSave{};
};

void TestFixedStepScheduler()
{
    pvz::engine::core::FixedStepScheduler aScheduler;
    aScheduler.Reset(0);
    Expect(
        aScheduler.GetNextUpdateDeadline() == 10'000,
        "fixed-step initial deadline");
    Expect(aScheduler.Advance(9'999) == 0, "partial tick waits");
    Expect(aScheduler.Advance(10'000) == 1, "complete tick updates");
    Expect(aScheduler.Advance(35'000) == 2, "scheduler catches up");

    pvz::engine::core::FixedStepScheduler anOverloadedScheduler({
        .mMaximumUpdatesPerFrame = 3,
        .mMaximumFrameDelta = 1'000'000,
    });
    anOverloadedScheduler.Reset(0);
    Expect(
        anOverloadedScheduler.Advance(100'000) == 3,
        "scheduler caps catch-up work");
    Expect(
        anOverloadedScheduler.GetDroppedUpdateCount() == 7,
        "scheduler records dropped catch-up ticks");
    Expect(
        anOverloadedScheduler.GetNextUpdateDeadline() == 110'000,
        "overloaded scheduler discards stale backlog");
}

void TestApplicationRunner()
{
    TestClock aClock;
    TestEventLoop anEvents(aClock);
    TestInputSystem anInput;
    TestRenderDevice aRenderer;
    TestServices aServices;
    TestGame aGame;

    const pvz::engine::core::ApplicationRunner aRunner;
    Expect(
        aRunner.Run(
            aGame,
            aServices,
            anEvents,
            aClock,
            anInput,
            aRenderer) ==
            pvz::engine::core::ApplicationRunResult::Success,
        "application runner completes");
    Expect(aGame.GetUpdateCount() == 3, "runner advances fixed ticks");
    Expect(aGame.GetLastTick() == 2, "runner uses stable tick indices");
    Expect(
        anInput.GetConsumeCount() == 3,
        "runner consumes transient input once per tick");
    Expect(
        aRenderer.GetBeginCount() == 4 &&
            aRenderer.GetEndCount() == 4 &&
            aRenderer.GetFrame().GetClearCount() == 4,
        "runner renders and presents each frame");
    Expect(
        aServices.GetTestLogger().GetMessageCount() == 2,
        "runner owns game initialization and shutdown");
}

[[nodiscard]] std::vector<std::byte> MakeRecordedSession()
{
    TestServices aServices;
    TestInputFrame anInput;
    TestGame aGame;
    pvz::engine::core::ReplayRecordingGame aRecordingGame(aGame);
    Expect(
        aRecordingGame.Initialize(aServices) ==
            pvz::engine::LifecycleResult::Success,
        "recording decorator initializes wrapped game");
    aRecordingGame.Update({0}, anInput);
    aRecordingGame.Update({1}, anInput);

    const auto& aSession = aRecordingGame.GetSession();
    Expect(
        aRecordingGame.GetRecordingError() ==
                pvz::engine::core::ReplayRecordingError::None &&
            aSession.GetInputReplay().GetFrames().size() == 2 &&
            aSession.GetStateHashes().size() == 2,
        "recording decorator captures input and state per tick");

    pvz::engine::core::BinaryStateWriter aFirstState;
    Expect(
        aFirstState.WriteU64(0) &&
            aFirstState.WriteU64(1),
        "first recording state fixture writes");
    pvz::engine::core::BinaryStateWriter aSecondState;
    Expect(
        aSecondState.WriteU64(1) &&
            aSecondState.WriteU64(2),
        "second recording state fixture writes");
    const auto aFirstHash =
        pvz::engine::core::CalculateFnv1a64(
            aFirstState.GetBytes());
    const auto aSecondHash =
        pvz::engine::core::CalculateFnv1a64(
            aSecondState.GetBytes());
    const auto aTranscriptHash =
        pvz::engine::core::CalculateFnv1a64(
            aSecondState.GetBytes(),
            pvz::engine::core::CalculateFnv1a64(
                aFirstState.GetBytes()));
    Expect(
        aSession.GetStateHashes()[0] == aFirstHash &&
            aSession.GetStateHashes()[1] == aSecondHash &&
            aSession.GetFinalStateHash() == aSecondHash &&
            aSession.GetTranscriptHash() == aTranscriptHash,
        "recording decorator hashes each state and full transcript");

    pvz::engine::core::BinaryStateWriter aSessionWriter;
    pvz::engine::core::ReplaySessionError aSessionError{};
    Expect(
        aSession.Save(aSessionWriter, aSessionError),
        "recorded session serializes");
    aRecordingGame.Shutdown();
    const auto aBytes = aSessionWriter.GetBytes();
    return {aBytes.begin(), aBytes.end()};
}

void TestReplaySessionRoundTrip()
{
    const auto aBytes = MakeRecordedSession();
    pvz::engine::core::BinaryStateReader aReader(aBytes);
    pvz::engine::core::ReplaySession aSession;
    pvz::engine::core::ReplaySessionError anError{};
    Expect(
        aSession.Load(aReader, anError),
        "recorded session round-trips");
    Expect(
        aSession.GetInputReplay().GetFrames().size() == 2 &&
            aSession.GetStateHashes().size() == 2 &&
            aSession.GetFinalStateHash() ==
                aSession.GetStateHashes()[1],
        "recorded session restores replay and hashes");
}

void ExpectSessionLoadError(
    std::vector<std::byte> theBytes,
    pvz::engine::core::ReplaySessionError theExpectedError,
    const char* theMessage)
{
    pvz::engine::core::ReplaySession aSession;
    pvz::engine::core::ReplaySessionError anError{};
    const auto aValidBytes = MakeRecordedSession();
    pvz::engine::core::BinaryStateReader aValidReader(
        aValidBytes);
    Expect(
        aSession.Load(aValidReader, anError),
        "prepare transactional replay session");

    pvz::engine::core::BinaryStateReader aReader(theBytes);
    Expect(
        !aSession.Load(aReader, anError) &&
            anError == theExpectedError,
        theMessage);
    Expect(
        aSession.GetStateHashes().size() == 2,
        "failed session load is transactional");
}

void TestMalformedReplaySessions()
{
    auto aBytes = MakeRecordedSession();
    aBytes[0] = std::byte{0};
    ExpectSessionLoadError(
        aBytes,
        pvz::engine::core::ReplaySessionError::InvalidMagic,
        "invalid session magic is rejected");

    aBytes = MakeRecordedSession();
    aBytes[4] = std::byte{2};
    ExpectSessionLoadError(
        aBytes,
        pvz::engine::core::ReplaySessionError::UnsupportedVersion,
        "unsupported session version is rejected");

    aBytes = MakeRecordedSession();
    aBytes[6] = std::byte{60};
    ExpectSessionLoadError(
        aBytes,
        pvz::engine::core::ReplaySessionError::InvalidTickFrequency,
        "invalid session tick frequency is rejected");

    aBytes = MakeRecordedSession();
    aBytes[10] = std::byte{0x01};
    aBytes[11] = std::byte{0x00};
    aBytes[12] = std::byte{0x00};
    aBytes[13] = std::byte{0x10};
    ExpectSessionLoadError(
        aBytes,
        pvz::engine::core::ReplaySessionError::ReplayTooLarge,
        "oversized nested replay is rejected before allocation");

    aBytes = MakeRecordedSession();
    aBytes[14] = std::byte{0};
    ExpectSessionLoadError(
        aBytes,
        pvz::engine::core::ReplaySessionError::InvalidReplay,
        "invalid nested replay is rejected");

    aBytes = MakeRecordedSession();
    constexpr std::size_t kHashCountOffset = 84;
    aBytes[kHashCountOffset] = std::byte{1};
    ExpectSessionLoadError(
        aBytes,
        pvz::engine::core::ReplaySessionError::HashCountMismatch,
        "session hash count mismatch is rejected");

    aBytes = MakeRecordedSession();
    aBytes[kHashCountOffset] = std::byte{0x41};
    aBytes[kHashCountOffset + 1] = std::byte{0x42};
    aBytes[kHashCountOffset + 2] = std::byte{0x0F};
    aBytes[kHashCountOffset + 3] = std::byte{0x00};
    ExpectSessionLoadError(
        aBytes,
        pvz::engine::core::ReplaySessionError::TooManyHashes,
        "oversized session hash count is rejected");

    aBytes = MakeRecordedSession();
    aBytes.push_back(std::byte{0});
    ExpectSessionLoadError(
        aBytes,
        pvz::engine::core::ReplaySessionError::TrailingData,
        "session trailing data is rejected");

    aBytes = MakeRecordedSession();
    aBytes.pop_back();
    ExpectSessionLoadError(
        aBytes,
        pvz::engine::core::ReplaySessionError::IoError,
        "truncated session is rejected");
}

void TestReplayRecordingErrors()
{
    TestServices aServices;
    TestInputFrame anInput;

    TestGame aNonSequentialGame;
    pvz::engine::core::ReplayRecordingGame
        aNonSequentialRecording(aNonSequentialGame);
    Expect(
        aNonSequentialRecording.Initialize(aServices) ==
            pvz::engine::LifecycleResult::Success,
        "non-sequential recording initializes");
    aNonSequentialRecording.Update({1}, anInput);
    Expect(
        aNonSequentialRecording.GetRecordingError() ==
                pvz::engine::core::ReplayRecordingError::
                    InputCaptureFailed &&
            aNonSequentialRecording.GetInputReplayError() ==
                pvz::engine::core::InputReplayError::
                    NonSequentialTick &&
            aNonSequentialGame.GetUpdateCount() == 1,
        "recording reports bad tick without blocking game update");
    aNonSequentialRecording.Shutdown();

    TestGame aFailingGame;
    aFailingGame.SetFailStateSave(true);
    pvz::engine::core::ReplayRecordingGame
        aFailingRecording(aFailingGame);
    Expect(
        aFailingRecording.Initialize(aServices) ==
            pvz::engine::LifecycleResult::Success,
        "state-failure recording initializes");
    aFailingRecording.Update({0}, anInput);
    Expect(
        aFailingRecording.GetRecordingError() ==
                pvz::engine::core::ReplayRecordingError::
                    StateSaveFailed &&
            aFailingGame.GetUpdateCount() == 1,
        "recording reports state failure without blocking update");
    aFailingRecording.Shutdown();
}

} // namespace

void RunApplicationRunnerTests()
{
    TestFixedStepScheduler();
    TestApplicationRunner();
    TestReplaySessionRoundTrip();
    TestMalformedReplaySessions();
    TestReplayRecordingErrors();
}
