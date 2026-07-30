#include "pvz/engine/core/ModuleMusicResourceManager.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

extern int gFailureCount;

namespace
{

int gModuleMusicFailureCount{};

void Expect(bool theCondition, const char* theMessage)
{
    if (theCondition)
        return;
    std::cerr << "FAIL: " << theMessage << '\n';
    ++gModuleMusicFailureCount;
}

class MemoryResourceStore final : public pvz::engine::IResourceStore
{
public:
    [[nodiscard]] bool Contains(
        std::string_view thePath) const override
    {
        return mResources.contains(std::string(thePath));
    }

    [[nodiscard]] bool GetSize(
        std::string_view thePath,
        std::uint64_t& theSize) const override
    {
        const auto aResource =
            mResources.find(std::string(thePath));
        if (aResource == mResources.end())
            return false;
        theSize =
            static_cast<std::uint64_t>(aResource->second.size());
        return true;
    }

    [[nodiscard]] bool ReadAll(
        std::string_view thePath,
        std::vector<std::byte>& theBytes) const override
    {
        const auto aResource =
            mResources.find(std::string(thePath));
        if (aResource == mResources.end())
            return false;
        theBytes = aResource->second;
        return true;
    }

    std::unordered_map<std::string, std::vector<std::byte>>
        mResources;
};

class CapturingMusicDevice final
    : public pvz::engine::IModuleMusicDevice
{
public:
    [[nodiscard]] bool CreateModule(
        std::span<const std::byte> theEncodedBytes,
        pvz::engine::ModuleHandle& theModule,
        pvz::engine::ModuleDescriptor& theDescriptor) override
    {
        if (theEncodedBytes.empty())
            return false;
        ++mCreateCount;
        theModule = {
            .mIndex = mCreateCount,
            .mGeneration = 3,
        };
        theDescriptor = {
            .mChannelCount = 30,
            .mOrderCount = 240,
        };
        return true;
    }

    void DestroyModule(
        pvz::engine::ModuleHandle theModule) override
    {
        mLastModule = theModule;
        ++mDestroyCount;
    }

    [[nodiscard]] bool PlayModule(
        pvz::engine::ModuleHandle theModule,
        const pvz::engine::MusicPlayback& thePlayback) override
    {
        mLastModule = theModule;
        mPlayback = thePlayback;
        ++mPlayCount;
        return true;
    }

    void StopModule(
        pvz::engine::ModuleHandle theModule) override
    {
        mLastModule = theModule;
        ++mStopCount;
    }

    void PauseModule(
        pvz::engine::ModuleHandle theModule,
        bool thePaused) override
    {
        mLastModule = theModule;
        mPaused = thePaused;
        ++mPauseCount;
    }

    [[nodiscard]] bool IsModulePlaying(
        pvz::engine::ModuleHandle theModule) const override
    {
        return theModule.mIndex == 1 &&
               theModule.mGeneration == 3;
    }

    [[nodiscard]] bool SetModulePosition(
        pvz::engine::ModuleHandle theModule,
        pvz::engine::MusicPosition thePosition) override
    {
        mLastModule = theModule;
        mPosition = thePosition;
        return true;
    }

    [[nodiscard]] bool GetModulePosition(
        pvz::engine::ModuleHandle theModule,
        pvz::engine::MusicPosition& thePosition) const override
    {
        if (theModule.mIndex != 1 ||
            theModule.mGeneration != 3)
        {
            return false;
        }
        thePosition = mPosition;
        return true;
    }

    [[nodiscard]] bool SetModuleChannelEnabled(
        pvz::engine::ModuleHandle theModule,
        std::uint32_t theChannel,
        bool theEnabled) override
    {
        mLastModule = theModule;
        mChannel = theChannel;
        mChannelEnabled = theEnabled;
        return true;
    }

    [[nodiscard]] bool SetModuleVolume(
        pvz::engine::ModuleHandle theModule,
        float theVolume) override
    {
        mLastModule = theModule;
        mVolume = theVolume;
        return true;
    }

    [[nodiscard]] bool SetModuleTempoFactor(
        pvz::engine::ModuleHandle theModule,
        float theFactor) override
    {
        mLastModule = theModule;
        mTempoFactor = theFactor;
        return true;
    }

    void SetMusicMasterVolume(float theVolume) override
    {
        mMasterVolume = theVolume;
    }

    pvz::engine::ModuleHandle mLastModule;
    pvz::engine::MusicPlayback mPlayback;
    mutable pvz::engine::MusicPosition mPosition;
    std::uint32_t mCreateCount{};
    std::uint32_t mDestroyCount{};
    std::uint32_t mPlayCount{};
    std::uint32_t mStopCount{};
    std::uint32_t mPauseCount{};
    std::uint32_t mChannel{};
    float mVolume{};
    float mTempoFactor{};
    float mMasterVolume{};
    bool mPaused{};
    bool mChannelEnabled{};
};

void TestLoadCacheAndControl()
{
    MemoryResourceStore aResources;
    aResources.mResources.emplace(
        "sounds/mainmusic.mo3",
        std::vector<std::byte>{
            std::byte{'M'},
            std::byte{'O'},
            std::byte{'3'},
            std::byte{1},
        });
    CapturingMusicDevice aDevice;
    pvz::engine::core::ModuleMusicResourceManager aMusic(
        aResources,
        aDevice);

    pvz::engine::MusicResource aFirst;
    pvz::engine::MusicResourceDiagnostic aDiagnostic;
    Expect(
        aMusic.Load(
            "sounds/mainmusic.mo3",
            aFirst,
            aDiagnostic),
        "module resource loads");
    Expect(
        aFirst.mModule.mIndex == 1 &&
            aFirst.mModule.mGeneration == 3 &&
            aFirst.mDescriptor.mChannelCount == 30 &&
            aFirst.mDescriptor.mOrderCount == 240,
        "module descriptor crosses the fixed-width protocol");

    pvz::engine::MusicResource aSecond;
    Expect(
        aMusic.Load(
            "sounds/mainmusic.mo3",
            aSecond,
            aDiagnostic) &&
            aSecond.mModule.mIndex == aFirst.mModule.mIndex &&
            aDevice.mCreateCount == 1,
        "repeated loads share one backend module");

    const pvz::engine::MusicPlayback aPlayback{
        .mPosition = {.mOrder = 0x98, .mRow = 0},
        .mVolume = 0.75F,
        .mLoopMode = pvz::engine::MusicLoopMode::Loop,
    };
    Expect(
        aMusic.Play(aFirst.mModule, aPlayback) &&
            aDevice.mPlayCount == 1 &&
            aDevice.mPlayback.mPosition.mOrder == 0x98,
        "playback retains a separate order and row");
    Expect(
        aMusic.SetPosition(
            aFirst.mModule,
            {.mOrder = 0x30, .mRow = 12}),
        "module order and row can be changed");
    pvz::engine::MusicPosition aPosition;
    Expect(
        aMusic.GetPosition(aFirst.mModule, aPosition) &&
            aPosition.mOrder == 0x30 &&
            aPosition.mRow == 12,
        "module order and row can be queried");
    Expect(
        aMusic.SetChannelEnabled(aFirst.mModule, 29, false) &&
            aDevice.mChannel == 29 &&
            !aDevice.mChannelEnabled,
        "module channels can be muted");
    Expect(
        aMusic.SetVolume(aFirst.mModule, 0.5F) &&
            std::abs(aDevice.mVolume - 0.5F) < 0.001F,
        "module volume is forwarded");
    Expect(
        aMusic.SetTempoFactor(aFirst.mModule, 1.01F) &&
            std::abs(aDevice.mTempoFactor - 1.01F) < 0.001F,
        "module tempo factor is forwarded");
    aMusic.Pause(aFirst.mModule, true);
    Expect(
        aDevice.mPauseCount == 1 && aDevice.mPaused,
        "module pause is forwarded");
    Expect(
        aMusic.IsPlaying(aFirst.mModule),
        "module playback state is queried");
    aMusic.SetMasterVolume(0.6F);
    Expect(
        std::abs(aDevice.mMasterVolume - 0.6F) < 0.001F,
        "music master volume is forwarded");
    aMusic.Stop(aFirst.mModule);
    Expect(aDevice.mStopCount == 1, "module stop is forwarded");

    aMusic.Release(aFirst.mModule);
    Expect(
        aDevice.mDestroyCount == 0,
        "shared module remains until its final release");
    aMusic.Release(aSecond.mModule);
    Expect(
        aDevice.mDestroyCount == 1,
        "final release destroys the backend module");
}

void TestMissingAndInvalidResources()
{
    MemoryResourceStore aResources;
    aResources.mResources.emplace(
        "sounds/empty.mo3",
        std::vector<std::byte>{});
    CapturingMusicDevice aDevice;
    pvz::engine::core::ModuleMusicResourceManager aMusic(
        aResources,
        aDevice);
    pvz::engine::MusicResource aResource;
    pvz::engine::MusicResourceDiagnostic aDiagnostic;
    Expect(
        !aMusic.Load(
            "sounds/missing.mo3",
            aResource,
            aDiagnostic) &&
            aDiagnostic.mError ==
                pvz::engine::MusicResourceError::ResourceNotFound,
        "missing module resource is diagnosed");
    Expect(
        !aMusic.Load(
            "sounds/empty.mo3",
            aResource,
            aDiagnostic) &&
            aDiagnostic.mError ==
                pvz::engine::MusicResourceError::DeviceCreateFailed,
        "backend module rejection is diagnosed");
}

} // namespace

void RunModuleMusicResourceManagerTests()
{
    TestLoadCacheAndControl();
    TestMissingAndInvalidResources();
    if (gModuleMusicFailureCount != 0)
        gFailureCount += gModuleMusicFailureCount;
}
