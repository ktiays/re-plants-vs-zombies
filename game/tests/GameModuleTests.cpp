#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/NullFontResources.h"
#include "pvz/engine/core/NullImageStore.h"
#include "pvz/game/GameModule.h"

#include <array>
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
    void PressKey(pvz::engine::KeyCode theKey)
    {
        mPressedKeys[static_cast<std::size_t>(theKey)] = true;
    }

    void PressPointer(pvz::engine::PointI thePosition)
    {
        mPointer.mPosition = thePosition;
        mPressedButtons[static_cast<std::size_t>(
            pvz::engine::PointerButton::Primary)] = true;
    }

    void Clear()
    {
        mPressedKeys.fill(false);
        mPressedButtons.fill(false);
    }

    [[nodiscard]] bool IsKeyDown(
        pvz::engine::KeyCode theKey) const override
    {
        static_cast<void>(theKey);
        return false;
    }

    [[nodiscard]] bool WasKeyPressed(
        pvz::engine::KeyCode theKey) const override
    {
        return mPressedKeys[static_cast<std::size_t>(theKey)];
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
        return mPressedButtons[static_cast<std::size_t>(
            theButton)];
    }

    [[nodiscard]] pvz::engine::PointerState GetPointerState()
        const override
    {
        return mPointer;
    }

    [[nodiscard]] std::span<const char32_t> GetTextInput() const override
    {
        return {};
    }

private:
    std::array<
        bool,
        static_cast<std::size_t>(pvz::engine::KeyCode::Count)>
        mPressedKeys{};
    std::array<
        bool,
        static_cast<std::size_t>(
            pvz::engine::PointerButton::Count)>
        mPressedButtons{};
    pvz::engine::PointerState mPointer{};
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
    Expect(aWriter.GetBytesWritten() == 765, "state schema has stable size");

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
    Expect(
        aRestoredGame.GetScene() == pvz::game::GameScene::Title,
        "portable game flow state round-trips");
    Expect(
        aRestoredGame.GetReanimationTick() ==
            aGame.GetReanimationTick(),
        "portable reanimation tick round-trips");
    Expect(
        aRestoredGame.GetLevelOneBoardState().mSun == 150,
        "portable Level 1 economy state round-trips");

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

    pvz::engine::core::BinaryStateWriter anInvalidBoardWriter;
    Expect(
        anInvalidBoardWriter.WriteU32(0x475A5650) &&
            anInvalidBoardWriter.WriteU16(5) &&
            anInvalidBoardWriter.WriteU64(200) &&
            anInvalidBoardWriter.WriteU64(300) &&
            anInvalidBoardWriter.WriteBool(false) &&
            anInvalidBoardWriter.WriteU8(
                static_cast<std::uint8_t>(
                    pvz::game::GameScene::AdventureDay)) &&
            anInvalidBoardWriter.WriteU8(
                static_cast<std::uint8_t>(
                    pvz::game::MainMenuItem::Adventure)) &&
            anInvalidBoardWriter.WriteU16(0) &&
            anInvalidBoardWriter.WriteU16(0) &&
            anInvalidBoardWriter.WriteU8(0) &&
            anInvalidBoardWriter.WriteU8(2) &&
            anInvalidBoardWriter.WriteU64(0) &&
            anInvalidBoardWriter.WriteU64(50) &&
            anInvalidBoardWriter.WriteU16(150) &&
            anInvalidBoardWriter.WriteU16(751) &&
            anInvalidBoardWriter.WriteBool(true) &&
            anInvalidBoardWriter.WriteU8(
                static_cast<std::uint8_t>(
                    pvz::game::LevelOneSeedSelection::None)),
        "invalid Level 1 state fixture writes");
    pvz::engine::core::BinaryStateReader anInvalidBoardReader(
        anInvalidBoardWriter.GetBytes());
    Expect(
        !aGame.LoadState(anInvalidBoardReader) &&
            aGame.GetLastTick() == 0 &&
            aGame.GetUpdateCount() == 0 &&
            aGame.GetLevelOneBoardState().mSun == 150,
        "invalid Level 1 state is rejected transactionally");
    aGame.Shutdown();
}

void TestPreviousStateSchemasRemainReadable()
{
    TestServices aServices;
    pvz::game::GameModule aGame;
    Expect(
        aGame.Initialize(aServices) ==
            pvz::engine::LifecycleResult::Success,
        "compatibility state game initializes");

    pvz::engine::core::BinaryStateWriter aVersionOneWriter;
    Expect(
        aVersionOneWriter.WriteU32(0x475A5650) &&
            aVersionOneWriter.WriteU16(1) &&
            aVersionOneWriter.WriteU64(25) &&
            aVersionOneWriter.WriteU64(50) &&
            aVersionOneWriter.WriteBool(false),
        "version-one state fixture writes");
    pvz::engine::core::BinaryStateReader aVersionOneReader(
        aVersionOneWriter.GetBytes());
    Expect(
        aGame.LoadState(aVersionOneReader) &&
            aGame.GetLastTick() == 25 &&
            aGame.GetUpdateCount() == 50 &&
            aGame.GetScene() == pvz::game::GameScene::Title &&
            aGame.GetReanimationTick() == 0 &&
            aGame.GetLevelOneBoardState().mSun == 150,
        "version-one state defaults newer portable fields");

    pvz::engine::core::BinaryStateWriter aVersionTwoWriter;
    Expect(
        aVersionTwoWriter.WriteU32(0x475A5650) &&
            aVersionTwoWriter.WriteU16(2) &&
            aVersionTwoWriter.WriteU64(75) &&
            aVersionTwoWriter.WriteU64(100) &&
            aVersionTwoWriter.WriteBool(false) &&
            aVersionTwoWriter.WriteU8(
                static_cast<std::uint8_t>(
                    pvz::game::GameScene::AdventureDay)) &&
            aVersionTwoWriter.WriteU8(
                static_cast<std::uint8_t>(
                    pvz::game::MainMenuItem::Adventure)) &&
            aVersionTwoWriter.WriteU16(0) &&
            aVersionTwoWriter.WriteU16(0) &&
            aVersionTwoWriter.WriteU8(2) &&
            aVersionTwoWriter.WriteU8(3) &&
            aVersionTwoWriter.WriteU64(1),
        "version-two state fixture writes");
    pvz::engine::core::BinaryStateReader aVersionTwoReader(
        aVersionTwoWriter.GetBytes());
    Expect(
        aGame.LoadState(aVersionTwoReader) &&
            aGame.GetLastTick() == 75 &&
            aGame.GetUpdateCount() == 100 &&
            aGame.GetScene() ==
                pvz::game::GameScene::AdventureDay &&
            aGame.GetFlowState().mGridColumn == 2 &&
            aGame.GetFlowState().mGridRow == 3 &&
            aGame.GetFlowState().mOccupiedCells == 1 &&
            aGame.GetReanimationTick() == 0,
        "version-two flow state defaults reanimation tick");

    pvz::engine::core::BinaryStateWriter aVersionThreeWriter;
    Expect(
        aVersionThreeWriter.WriteU32(0x475A5650) &&
            aVersionThreeWriter.WriteU16(3) &&
            aVersionThreeWriter.WriteU64(125) &&
            aVersionThreeWriter.WriteU64(150) &&
            aVersionThreeWriter.WriteBool(false) &&
            aVersionThreeWriter.WriteU8(
                static_cast<std::uint8_t>(
                    pvz::game::GameScene::AdventureDay)) &&
            aVersionThreeWriter.WriteU8(
                static_cast<std::uint8_t>(
                    pvz::game::MainMenuItem::Adventure)) &&
            aVersionThreeWriter.WriteU16(0) &&
            aVersionThreeWriter.WriteU16(0) &&
            aVersionThreeWriter.WriteU8(4) &&
            aVersionThreeWriter.WriteU8(2) &&
            aVersionThreeWriter.WriteU64(2) &&
            aVersionThreeWriter.WriteU64(123),
        "version-three state fixture writes");
    pvz::engine::core::BinaryStateReader aVersionThreeReader(
        aVersionThreeWriter.GetBytes());
    Expect(
        aGame.LoadState(aVersionThreeReader) &&
            aGame.GetLastTick() == 125 &&
            aGame.GetUpdateCount() == 150 &&
            aGame.GetScene() ==
                pvz::game::GameScene::AdventureDay &&
            aGame.GetFlowState().mGridColumn == 4 &&
            aGame.GetFlowState().mGridRow == 2 &&
            aGame.GetFlowState().mOccupiedCells == 2 &&
            aGame.GetReanimationTick() == 123 &&
            aGame.GetLevelOneBoardState().mSun == 150,
        "version-three state remains readable");

    pvz::engine::core::BinaryStateWriter aVersionFourWriter;
    Expect(
        aVersionFourWriter.WriteU32(0x475A5650) &&
            aVersionFourWriter.WriteU16(4) &&
            aVersionFourWriter.WriteU64(175) &&
            aVersionFourWriter.WriteU64(200) &&
            aVersionFourWriter.WriteBool(false) &&
            aVersionFourWriter.WriteU8(
                static_cast<std::uint8_t>(
                    pvz::game::GameScene::AdventureIntro)) &&
            aVersionFourWriter.WriteU8(
                static_cast<std::uint8_t>(
                    pvz::game::MainMenuItem::Adventure)) &&
            aVersionFourWriter.WriteU16(100) &&
            aVersionFourWriter.WriteU16(0) &&
            aVersionFourWriter.WriteU8(0xFF) &&
            aVersionFourWriter.WriteU8(0xFF) &&
            aVersionFourWriter.WriteU64(0) &&
            aVersionFourWriter.WriteU64(321),
        "version-four state fixture writes");
    pvz::engine::core::BinaryStateReader aVersionFourReader(
        aVersionFourWriter.GetBytes());
    Expect(
        aGame.LoadState(aVersionFourReader) &&
            aGame.GetLastTick() == 175 &&
            aGame.GetUpdateCount() == 200 &&
            aGame.GetScene() ==
                pvz::game::GameScene::AdventureIntro &&
            aGame.GetFlowState().mTransitionTicks == 100 &&
            aGame.GetReanimationTick() == 321 &&
            aGame.GetLevelOneBoardState().mSun == 150,
        "version-four intro state defaults Level 1 economy");

    pvz::engine::core::BinaryStateWriter aVersionFiveWriter;
    Expect(
        aVersionFiveWriter.WriteU32(0x475A5650) &&
            aVersionFiveWriter.WriteU16(5) &&
            aVersionFiveWriter.WriteU64(225) &&
            aVersionFiveWriter.WriteU64(250) &&
            aVersionFiveWriter.WriteBool(false) &&
            aVersionFiveWriter.WriteU8(
                static_cast<std::uint8_t>(
                    pvz::game::GameScene::AdventureDay)) &&
            aVersionFiveWriter.WriteU8(
                static_cast<std::uint8_t>(
                    pvz::game::MainMenuItem::Adventure)) &&
            aVersionFiveWriter.WriteU16(0) &&
            aVersionFiveWriter.WriteU16(0) &&
            aVersionFiveWriter.WriteU8(2) &&
            aVersionFiveWriter.WriteU8(2) &&
            aVersionFiveWriter.WriteU64(
                std::uint64_t{1} << 20U) &&
            aVersionFiveWriter.WriteU64(444) &&
            aVersionFiveWriter.WriteU16(50) &&
            aVersionFiveWriter.WriteU16(10) &&
            aVersionFiveWriter.WriteBool(true) &&
            aVersionFiveWriter.WriteU8(
                static_cast<std::uint8_t>(
                    pvz::game::LevelOneSeedSelection::None)),
        "version-five state fixture writes");
    pvz::engine::core::BinaryStateReader aVersionFiveReader(
        aVersionFiveWriter.GetBytes());
    Expect(
        aGame.LoadState(aVersionFiveReader) &&
            aGame.GetLastTick() == 225 &&
            aGame.GetScene() ==
                pvz::game::GameScene::AdventureDay &&
            aGame.GetLevelOneCombatState().mPhase ==
                pvz::game::LevelOneCombatPhase::
                    AwaitingSecondPlant &&
            aGame.GetLevelOneCombatState().mPlantCount == 1 &&
            aGame.GetLevelOneCombatState().mPlants[0].mColumn == 2,
        "version-five occupancy reconstructs combat entities");

    aGame.Shutdown();
}

void TestSceneMusicTransitions()
{
    TestServices aServices;
    EmptyInputFrame anInput;
    pvz::game::GameModule aGame;
    Expect(
        aGame.Initialize(aServices) ==
            pvz::engine::LifecycleResult::Success,
        "scene music game initializes");

    anInput.PressKey(pvz::engine::KeyCode::Enter);
    aGame.Update(pvz::engine::GameTick{0}, anInput);
    anInput.Clear();
    Expect(
        aGame.GetScene() == pvz::game::GameScene::MainMenu &&
            aServices.GetTestMusicResources().mPlayCount == 1,
        "title music continues into main menu");

    anInput.PressKey(pvz::engine::KeyCode::Enter);
    aGame.Update(pvz::engine::GameTick{1}, anInput);
    anInput.Clear();
    Expect(
        aGame.GetScene() ==
                pvz::game::GameScene::StartingAdventure &&
            aServices.GetTestMusicResources().mStopCount == 1,
        "adventure transition stops title music");

    for (pvz::engine::TickIndex aTick = 2; aTick < 452; ++aTick)
        aGame.Update(pvz::engine::GameTick{aTick}, anInput);
    Expect(
        aGame.GetScene() ==
                pvz::game::GameScene::AdventureIntro &&
            aServices.GetTestMusicResources().mPlayCount == 2 &&
            aServices.GetTestMusicResources()
                    .mPlayback.mPosition.mOrder == 0,
        "level intro starts adventure music order");

    for (pvz::engine::TickIndex aTick = 452;
         aTick < 1'307;
         ++aTick)
    {
        aGame.Update(pvz::engine::GameTick{aTick}, anInput);
    }
    Expect(
        aGame.GetScene() == pvz::game::GameScene::AdventureDay &&
            aServices.GetTestMusicResources().mPlayCount == 2,
        "first-level intro enters playable day board");

    anInput.PressKey(pvz::engine::KeyCode::ArrowDown);
    aGame.Update(pvz::engine::GameTick{1'307}, anInput);
    anInput.Clear();
    anInput.PressKey(pvz::engine::KeyCode::ArrowDown);
    aGame.Update(pvz::engine::GameTick{1'308}, anInput);
    anInput.Clear();
    anInput.PressKey(pvz::engine::KeyCode::Space);
    aGame.Update(pvz::engine::GameTick{1'309}, anInput);
    anInput.Clear();
    Expect(
        aGame.GetLevelOneBoardState().mSeedSelection ==
            pvz::game::LevelOneSeedSelection::Peashooter,
        "keyboard selects the ready Peashooter packet");
    anInput.PressKey(pvz::engine::KeyCode::Space);
    aGame.Update(pvz::engine::GameTick{1'310}, anInput);
    anInput.Clear();
    const auto anEconomyObservation =
        aGame.GetBehaviorObservation();
    Expect(
        anEconomyObservation.mSun == 50 &&
            anEconomyObservation.mSeedRefreshCounter == 1 &&
            anEconomyObservation.mSeedRefreshTime == 750 &&
            anEconomyObservation.mSeedRefreshing &&
            anEconomyObservation.mTutorialPhase ==
                pvz::game::BehaviorTutorialPhase::
                    LevelOneRefreshPeashooter &&
            anEconomyObservation.mFirstSunCountdown == 399 &&
            !anEconomyObservation.mFirstSunSpawned,
        "behavior observation exports post-input Level 1 economy state");
    pvz::engine::core::BinaryStateWriter aWriter;
    Expect(
        aGame.SaveState(aWriter),
        "interactive board state saves");

    TestServices aRestoredServices;
    pvz::game::GameModule aRestoredGame;
    Expect(
        aRestoredGame.Initialize(aRestoredServices) ==
            pvz::engine::LifecycleResult::Success,
        "interactive state restore game initializes");
    pvz::engine::core::BinaryStateReader aReader(aWriter.GetBytes());
    Expect(
        aRestoredGame.LoadState(aReader) &&
            aRestoredGame.GetScene() ==
                pvz::game::GameScene::AdventureDay &&
            aRestoredGame.GetFlowState().mOccupiedCells ==
                (std::uint64_t{1} << 18U) &&
            aRestoredGame.GetLevelOneBoardState().mSun == 50 &&
            aRestoredGame.GetLevelOneBoardState()
                .mSeedRefreshing &&
            aRestoredGame.GetLevelOneCombatState().mPhase ==
                pvz::game::LevelOneCombatPhase::
                    AwaitingSecondPlant &&
            aRestoredGame.GetLevelOneCombatState().mPlantCount == 1,
        "versioned module state restores economy and combat entities");
    Expect(
        aRestoredServices.GetTestMusicResources()
                .mPlayback.mPosition.mOrder == 0,
        "state restore synchronizes adventure music");

    pvz::engine::TickIndex aCombatTick = 1'311;
    while (aGame.GetLevelOneCombatState().mSunsSpawned < 1)
        aGame.Update(pvz::engine::GameTick{aCombatTick++}, anInput);
    Expect(
        aGame.GetBehaviorObservation().mFirstSunSpawned &&
            aGame.GetBehaviorObservation().mFirstSunCountdown == 0,
        "behavior observation normalizes the deterministic first-sun gate");
    anInput.PressPointer({405, 90});
    aGame.Update(pvz::engine::GameTick{aCombatTick++}, anInput);
    anInput.Clear();
    Expect(
        aGame.GetLevelOneBoardState().mSun == 75,
        "first falling sun is collected through engine-neutral input");

    while (aGame.GetLevelOneCombatState().mSunsSpawned < 2)
        aGame.Update(pvz::engine::GameTick{aCombatTick++}, anInput);
    anInput.PressPointer({405, 90});
    aGame.Update(pvz::engine::GameTick{aCombatTick++}, anInput);
    anInput.Clear();
    while (aGame.GetLevelOneBoardState().mSeedRefreshing)
        aGame.Update(pvz::engine::GameTick{aCombatTick++}, anInput);
    Expect(
        aGame.GetLevelOneBoardState().mSun == 100,
        "two tutorial suns fund the second Peashooter");

    anInput.PressPointer({100, 20});
    aGame.Update(pvz::engine::GameTick{aCombatTick++}, anInput);
    anInput.Clear();
    anInput.PressPointer({160, 330});
    aGame.Update(pvz::engine::GameTick{aCombatTick++}, anInput);
    anInput.Clear();
    Expect(
        aGame.GetLevelOneCombatState().mPhase ==
                pvz::game::LevelOneCombatPhase::Active &&
            aGame.GetFlowState().mOccupiedCells ==
                ((std::uint64_t{1} << 18U) |
                 (std::uint64_t{1} << 19U)),
        "normal play reaches active combat after the second plant");

    while (!aGame.GetLevelOneCombatState().mFirstWaveSpawned)
        aGame.Update(pvz::engine::GameTick{aCombatTick++}, anInput);
    Expect(
        aGame.GetLevelOneCombatState().mZombieCount == 1,
        "GameModule starts the first normal-zombie wave");

    anInput.PressKey(pvz::engine::KeyCode::Escape);
    aGame.Update(pvz::engine::GameTick{aCombatTick++}, anInput);
    anInput.Clear();
    Expect(
        aGame.GetScene() == pvz::game::GameScene::MainMenu &&
            aServices.GetTestMusicResources().mPlayCount == 3 &&
            aServices.GetTestMusicResources()
                    .mPlayback.mPosition.mOrder == 0x98,
        "escape restores title music in main menu");

    anInput.PressKey(pvz::engine::KeyCode::Enter);
    aGame.Update(pvz::engine::GameTick{aCombatTick}, anInput);
    anInput.Clear();
    Expect(
        aGame.GetScene() ==
                pvz::game::GameScene::StartingAdventure &&
            aGame.GetFlowState().mOccupiedCells == 0 &&
            aGame.GetLevelOneBoardState().mSun == 150 &&
            !aGame.GetLevelOneBoardState().mSeedRefreshing &&
            aGame.GetLevelOneCombatState().mPlantCount == 0 &&
            !aGame.GetLevelOneCombatState().mFirstWaveSpawned,
        "new Adventure run resets occupancy, economy, and combat");

    aRestoredGame.Shutdown();
    aGame.Shutdown();
}

} // namespace

void RunLegacyDataSyncTests();
void RunLegacySaveFormatTests();
void RunDefinitionLoaderTests();
void RunGameFlowTests();
void RunLevelOneBoardTests();
void RunLevelOneCombatTests();
void RunPlayerInfoSerializationTests();
void RunReanimationDefinitionTests();
void RunReanimationPlayerTests();
void RunParameterTrackTests();
void RunParticleDefinitionTests();
void RunTrailDefinitionTests();

int main()
{
    TestLifecycleAndState();
    TestInvalidSchemaIsTransactional();
    TestPreviousStateSchemasRemainReadable();
    TestSceneMusicTransitions();
    RunGameFlowTests();
    RunLevelOneBoardTests();
    RunLevelOneCombatTests();
    RunLegacyDataSyncTests();
    RunLegacySaveFormatTests();
    RunDefinitionLoaderTests();
    RunPlayerInfoSerializationTests();
    RunReanimationDefinitionTests();
    RunReanimationPlayerTests();
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
