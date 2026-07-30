#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/NullFontResources.h"
#include "pvz/engine/core/NullImageStore.h"
#include "pvz/game/GameModule.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

int gFailureCount{};

void Expect(bool theCondition, const char* theMessage)
{
    if (theCondition)
        return;

    std::cerr << "FAIL: " << theMessage << '\n';
    ++gFailureCount;
}

namespace
{

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

class TestSoundResources final : public pvz::engine::ISoundResources
{
public:
    [[nodiscard]] bool Load(
        std::string_view theResourceId,
        pvz::engine::SoundResource& theResource,
        pvz::engine::SoundResourceDiagnostic& theDiagnostic) override
    {
        if (theResourceId != "SOUND_LOADINGBAR_FLOWER")
            return false;
        ++mLoadCount;
        theResource = {
            .mSound = {.mIndex = 9, .mGeneration = 1},
            .mDescriptor =
                {
                    .mSampleRate = 44'100,
                    .mChannelCount = 2,
                    .mFrameCount = 100,
                },
        };
        theDiagnostic = {};
        return true;
    }

    void Release(pvz::engine::SoundHandle theSound) override
    {
        if (theSound.mIndex == 9 && theSound.mGeneration == 1)
            ++mReleaseCount;
    }

    [[nodiscard]] bool Play(
        pvz::engine::SoundHandle theSound,
        const pvz::engine::SoundPlayback& thePlayback,
        pvz::engine::VoiceHandle& theVoice) override
    {
        static_cast<void>(thePlayback);
        if (theSound.mIndex != 9 || theSound.mGeneration != 1)
            return false;
        ++mPlayCount;
        theVoice = {.mIndex = 4, .mGeneration = 2};
        return true;
    }

    void Stop(pvz::engine::VoiceHandle theVoice) override
    {
        if (theVoice.mIndex == 4 && theVoice.mGeneration == 2)
            ++mStopCount;
    }

    void StopAll() override
    {
    }

    [[nodiscard]] bool IsPlaying(
        pvz::engine::VoiceHandle theVoice) const override
    {
        static_cast<void>(theVoice);
        return false;
    }

    void SetMasterVolume(float theVolume) override
    {
        static_cast<void>(theVolume);
    }

    std::uint32_t mLoadCount{};
    std::uint32_t mPlayCount{};
    std::uint32_t mStopCount{};
    std::uint32_t mReleaseCount{};
};

class TestMusicResources final : public pvz::engine::IMusicResources
{
public:
    [[nodiscard]] bool Load(
        std::string_view thePath,
        pvz::engine::MusicResource& theResource,
        pvz::engine::MusicResourceDiagnostic& theDiagnostic) override
    {
        if (thePath != "sounds/mainmusic.mo3")
            return false;
        ++mLoadCount;
        theResource = {
            .mModule = {.mIndex = 7, .mGeneration = 4},
            .mDescriptor =
                {
                    .mChannelCount = 30,
                    .mOrderCount = 240,
                },
        };
        theDiagnostic = {};
        return true;
    }

    void Release(pvz::engine::ModuleHandle theModule) override
    {
        if (theModule.mIndex == 7 &&
            theModule.mGeneration == 4)
        {
            ++mReleaseCount;
        }
    }

    [[nodiscard]] bool Play(
        pvz::engine::ModuleHandle theModule,
        const pvz::engine::MusicPlayback& thePlayback) override
    {
        if (theModule.mIndex != 7 ||
            theModule.mGeneration != 4)
        {
            return false;
        }
        mPlayback = thePlayback;
        ++mPlayCount;
        return true;
    }

    void Stop(pvz::engine::ModuleHandle theModule) override
    {
        if (theModule.mIndex == 7 &&
            theModule.mGeneration == 4)
        {
            ++mStopCount;
        }
    }

    void Pause(
        pvz::engine::ModuleHandle theModule,
        bool thePaused) override
    {
        if (theModule.mIndex == 7 &&
            theModule.mGeneration == 4)
        {
            mPaused = thePaused;
            ++mPauseCount;
        }
    }

    [[nodiscard]] bool IsPlaying(
        pvz::engine::ModuleHandle theModule) const override
    {
        return theModule.mIndex == 7 &&
               theModule.mGeneration == 4;
    }

    [[nodiscard]] bool SetPosition(
        pvz::engine::ModuleHandle theModule,
        pvz::engine::MusicPosition thePosition) override
    {
        static_cast<void>(theModule);
        static_cast<void>(thePosition);
        return true;
    }

    [[nodiscard]] bool GetPosition(
        pvz::engine::ModuleHandle theModule,
        pvz::engine::MusicPosition& thePosition) const override
    {
        static_cast<void>(theModule);
        thePosition = mPlayback.mPosition;
        return true;
    }

    [[nodiscard]] bool SetChannelEnabled(
        pvz::engine::ModuleHandle theModule,
        std::uint32_t theChannel,
        bool theEnabled) override
    {
        static_cast<void>(theModule);
        static_cast<void>(theChannel);
        static_cast<void>(theEnabled);
        return true;
    }

    [[nodiscard]] bool SetVolume(
        pvz::engine::ModuleHandle theModule,
        float theVolume) override
    {
        static_cast<void>(theModule);
        static_cast<void>(theVolume);
        return true;
    }

    [[nodiscard]] bool SetTempoFactor(
        pvz::engine::ModuleHandle theModule,
        float theFactor) override
    {
        static_cast<void>(theModule);
        static_cast<void>(theFactor);
        return true;
    }

    void SetMasterVolume(float theVolume) override
    {
        static_cast<void>(theVolume);
    }

    pvz::engine::MusicPlayback mPlayback;
    std::uint32_t mLoadCount{};
    std::uint32_t mPlayCount{};
    std::uint32_t mStopCount{};
    std::uint32_t mPauseCount{};
    std::uint32_t mReleaseCount{};
    bool mPaused{};
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

    [[nodiscard]] TestLogger& GetTestLogger()
    {
        return mLogger;
    }

    [[nodiscard]] TestSoundResources& GetTestSoundResources()
    {
        return mSoundResources;
    }

    [[nodiscard]] TestMusicResources& GetTestMusicResources()
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

    TestLogger mLogger;
    EmptyResourceStore mResources;
    EmptyXmlDocumentLoader mXmlDocuments;
    pvz::engine::core::NullImageStore mImages;
    pvz::engine::core::NullImageResources mImageResources;
    pvz::engine::core::NullFontResources mFontResources;
    TestSoundResources mSoundResources;
    TestMusicResources mMusicResources;
};

class EmptyInputFrame final : public pvz::engine::IInputFrame
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

class TestRenderFrame final : public pvz::engine::IRenderFrame
{
public:
    [[nodiscard]] pvz::engine::SizeI GetLogicalSize() const override
    {
        return {800, 600};
    }

    void Clear(pvz::engine::ColorRgba8 theColor) override
    {
        mClearColor = theColor;
        ++mClearCount;
    }

    void SubmitSprites(
        std::span<const pvz::engine::SpriteDraw> theDraws) override
    {
        mSpriteCount += static_cast<std::uint64_t>(theDraws.size());
    }

    [[nodiscard]] std::uint32_t GetClearCount() const
    {
        return mClearCount;
    }

    [[nodiscard]] pvz::engine::ColorRgba8 GetClearColor() const
    {
        return mClearColor;
    }

private:
    pvz::engine::ColorRgba8 mClearColor{};
    std::uint64_t mSpriteCount{};
    std::uint32_t mClearCount{};
};

void TestLifecycleAndState()
{
    TestServices aServices;
    EmptyInputFrame anInput;
    TestRenderFrame aFrame;
    pvz::game::GameModule aGame;

    Expect(!aGame.IsInitialized(), "game starts uninitialized");
    Expect(
        aGame.Initialize(aServices) == pvz::engine::LifecycleResult::Success,
        "game initializes");
    Expect(
        aServices.GetTestSoundResources().mLoadCount == 1 &&
            aServices.GetTestSoundResources().mPlayCount == 1,
        "game loads and plays sound through engine protocol");
    Expect(
        aServices.GetTestMusicResources().mLoadCount == 1 &&
            aServices.GetTestMusicResources().mPlayCount == 1 &&
            aServices.GetTestMusicResources()
                    .mPlayback.mPosition.mOrder == 0x98 &&
            aServices.GetTestMusicResources()
                    .mPlayback.mPosition.mRow == 0,
        "game starts title music through fixed-width protocol");
    Expect(
        aGame.Initialize(aServices) == pvz::engine::LifecycleResult::Failure,
        "game rejects duplicate initialization");

    aGame.Update(pvz::engine::GameTick{10}, anInput);
    aGame.Update(pvz::engine::GameTick{11}, anInput);
    Expect(aGame.GetUpdateCount() == 2, "updates are counted");
    Expect(aGame.GetLastTick() == 11, "last tick is recorded");

    aGame.Suspend();
    Expect(
        aServices.GetTestMusicResources().mPauseCount == 1 &&
            aServices.GetTestMusicResources().mPaused,
        "suspend pauses title music");
    aGame.Update(pvz::engine::GameTick{12}, anInput);
    Expect(aGame.GetUpdateCount() == 2, "suspended game does not update");
    aGame.Resume();
    Expect(
        aServices.GetTestMusicResources().mPauseCount == 2 &&
            !aServices.GetTestMusicResources().mPaused,
        "resume continues title music");
    aGame.Update(pvz::engine::GameTick{12}, anInput);
    Expect(aGame.GetUpdateCount() == 3, "resumed game updates");

    aGame.Render(aFrame);
    Expect(aFrame.GetClearCount() == 1, "game renders through frame protocol");
    const auto aClearColor = aFrame.GetClearColor();
    Expect(
        aClearColor.mRed == 0 &&
            aClearColor.mGreen == 0 &&
            aClearColor.mBlue == 0 &&
            aClearColor.mAlpha == 255,
        "headless frame receives expected clear color");

    pvz::engine::core::BinaryStateWriter aWriter;
    Expect(aGame.SaveState(aWriter), "initialized game saves state");
    Expect(aWriter.GetBytesWritten() == 23, "state schema has stable size");

    TestServices aRestoredServices;
    pvz::game::GameModule aRestoredGame;
    Expect(
        aRestoredGame.Initialize(aRestoredServices) ==
            pvz::engine::LifecycleResult::Success,
        "restored game initializes");
    pvz::engine::core::BinaryStateReader aReader(aWriter.GetBytes());
    Expect(aRestoredGame.LoadState(aReader), "saved state loads");
    Expect(
        aRestoredGame.GetLastTick() == aGame.GetLastTick(),
        "last tick round-trips");
    Expect(
        aRestoredGame.GetUpdateCount() == aGame.GetUpdateCount(),
        "update count round-trips");

    aGame.Shutdown();
    Expect(!aGame.IsInitialized(), "shutdown clears initialized state");
    Expect(
        aServices.GetTestLogger().GetMessageCount() == 4,
        "lifecycle messages use engine logger");
    Expect(
        aServices.GetTestSoundResources().mStopCount == 1 &&
            aServices.GetTestSoundResources().mReleaseCount == 1,
        "game releases sound through engine protocol");
    Expect(
        aServices.GetTestMusicResources().mStopCount == 1 &&
            aServices.GetTestMusicResources().mReleaseCount == 1,
        "game stops and releases title music");

    pvz::engine::core::BinaryStateWriter anUninitializedWriter;
    Expect(
        !aGame.SaveState(anUninitializedWriter),
        "uninitialized game cannot save");
}

void TestInvalidSchemaIsTransactional()
{
    TestServices aServices;
    pvz::game::GameModule aGame;
    Expect(
        aGame.Initialize(aServices) == pvz::engine::LifecycleResult::Success,
        "schema test game initializes");

    pvz::engine::core::BinaryStateWriter aWriter;
    Expect(aWriter.WriteU32(0xFFFFFFFF), "write invalid magic");
    Expect(aWriter.WriteU16(1), "write schema version");
    Expect(aWriter.WriteU64(100), "write schema tick");
    Expect(aWriter.WriteU64(100), "write schema count");
    Expect(aWriter.WriteBool(false), "write schema suspended state");

    pvz::engine::core::BinaryStateReader aReader(aWriter.GetBytes());
    Expect(!aGame.LoadState(aReader), "invalid schema is rejected");
    Expect(aGame.GetLastTick() == 0, "invalid schema does not modify tick");
    Expect(
        aGame.GetUpdateCount() == 0,
        "invalid schema does not modify update count");
    aGame.Shutdown();
}

} // namespace

void RunLegacyDataSyncTests();
void RunLegacySaveFormatTests();
void RunDefinitionLoaderTests();
void RunPlayerInfoSerializationTests();
void RunReanimationDefinitionTests();
void RunParameterTrackTests();
void RunParticleDefinitionTests();
void RunTrailDefinitionTests();

int main()
{
    TestLifecycleAndState();
    TestInvalidSchemaIsTransactional();
    RunLegacyDataSyncTests();
    RunLegacySaveFormatTests();
    RunDefinitionLoaderTests();
    RunPlayerInfoSerializationTests();
    RunReanimationDefinitionTests();
    RunParameterTrackTests();
    RunParticleDefinitionTests();
    RunTrailDefinitionTests();

    if (gFailureCount != 0)
    {
        std::cerr << gFailureCount << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "Portable game module tests passed\n";
    return 0;
}
