#include "pvz/engine/core/ResourceXmlDocumentLoader.h"
#include "pvz/engine/core/SoundResourceManager.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

void Expect(bool theCondition, const char* theMessage);

namespace
{

class MemoryResourceStore final : public pvz::engine::IResourceStore
{
public:
    void AddText(std::string thePath, std::string_view theText)
    {
        std::vector<std::byte> aBytes;
        aBytes.reserve(theText.size());
        for (const char aCharacter : theText)
        {
            aBytes.push_back(static_cast<std::byte>(
                static_cast<unsigned char>(aCharacter)));
        }
        mResources.emplace(std::move(thePath), std::move(aBytes));
    }

    void AddBytes(
        std::string thePath,
        std::vector<std::byte> theBytes)
    {
        mResources.emplace(std::move(thePath), std::move(theBytes));
    }

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
        theSize = aResource->second.size();
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

private:
    std::unordered_map<std::string, std::vector<std::byte>>
        mResources;
};

class TaggedAudioDecoder final : public pvz::engine::IAudioDecoder
{
public:
    [[nodiscard]] bool Decode(
        std::span<const std::byte> theEncodedBytes,
        pvz::engine::DecodedSound& theSound,
        pvz::engine::SoundDecodeError& theError) const override
    {
        ++mDecodeCount;
        if (theEncodedBytes.size() != 1 ||
            theEncodedBytes.front() != std::byte{1})
        {
            theSound = {};
            theError = pvz::engine::SoundDecodeError::InvalidData;
            return false;
        }
        theSound = {
            .mDescriptor =
                {
                    .mSampleRate = 22'050,
                    .mChannelCount = 1,
                    .mFrameCount = 2,
                },
            .mInterleavedSamples = {100, -100},
        };
        theError = pvz::engine::SoundDecodeError::None;
        return true;
    }

    mutable std::uint32_t mDecodeCount{};
};

class CapturingAudioDevice final : public pvz::engine::IAudioDevice
{
public:
    [[nodiscard]] bool CreateSound(
        const pvz::engine::SoundDescriptor& theDescriptor,
        std::span<const std::int16_t> theInterleavedSamples,
        pvz::engine::SoundHandle& theSound) override
    {
        ++mCreateCount;
        mDescriptor = theDescriptor;
        mSamples.assign(
            theInterleavedSamples.begin(),
            theInterleavedSamples.end());
        theSound = {
            .mIndex = 7,
            .mGeneration = 3,
        };
        mSound = theSound;
        return true;
    }

    void DestroySound(pvz::engine::SoundHandle theSound) override
    {
        if (theSound.mIndex == mSound.mIndex &&
            theSound.mGeneration == mSound.mGeneration)
        {
            ++mDestroyCount;
        }
    }

    [[nodiscard]] bool Play(
        pvz::engine::SoundHandle theSound,
        const pvz::engine::SoundPlayback& thePlayback,
        pvz::engine::VoiceHandle& theVoice) override
    {
        if (theSound.mIndex != mSound.mIndex ||
            theSound.mGeneration != mSound.mGeneration)
        {
            return false;
        }
        mPlayback = thePlayback;
        mPlaying = true;
        theVoice = {
            .mIndex = 2,
            .mGeneration = 5,
        };
        mVoice = theVoice;
        return true;
    }

    void Stop(pvz::engine::VoiceHandle theVoice) override
    {
        if (theVoice.mIndex == mVoice.mIndex &&
            theVoice.mGeneration == mVoice.mGeneration)
        {
            mPlaying = false;
        }
    }

    void StopAll() override
    {
        mPlaying = false;
    }

    [[nodiscard]] bool IsPlaying(
        pvz::engine::VoiceHandle theVoice) const override
    {
        return mPlaying &&
               theVoice.mIndex == mVoice.mIndex &&
               theVoice.mGeneration == mVoice.mGeneration;
    }

    void SetMasterVolume(float theVolume) override
    {
        mMasterVolume = theVolume;
    }

    pvz::engine::SoundDescriptor mDescriptor;
    pvz::engine::SoundPlayback mPlayback;
    pvz::engine::SoundHandle mSound;
    pvz::engine::VoiceHandle mVoice;
    std::vector<std::int16_t> mSamples;
    std::uint32_t mCreateCount{};
    std::uint32_t mDestroyCount{};
    float mMasterVolume{};
    bool mPlaying{};
};

void TestManifestLoadCacheAndPlayback()
{
    MemoryResourceStore aResources;
    aResources.AddText(
        "properties/resources.xml",
        "<ResourceManifest><Resources>"
        "<SetDefaults path=\"sounds\" idprefix=\"SOUND_\"/>"
        "<Sound id=\"CLICK\" path=\"click\"/>"
        "<Image id=\"IGNORED\" path=\"ignored\"/>"
        "<Font id=\"IGNORED\" path=\"ignored\"/>"
        "<Sound path=\"natural.ogg\"/>"
        "</Resources></ResourceManifest>");
    aResources.AddBytes("sounds/click.ogg", {std::byte{1}});
    aResources.AddBytes("sounds/natural.ogg", {std::byte{1}});

    pvz::engine::core::ResourceXmlDocumentLoader aDocuments(
        aResources);
    TaggedAudioDecoder aDecoder;
    CapturingAudioDevice anAudioDevice;
    pvz::engine::core::SoundResourceManager aManager(
        aResources,
        aDocuments,
        aDecoder,
        anAudioDevice);

    Expect(
        aManager.LoadManifest("properties/resources.xml"),
        "sound manifest loads");
    Expect(aManager.IsManifestLoaded(), "sound manifest state");
    Expect(
        aManager.GetDefinitionCount() == 2,
        "sound manifest definition count");

    pvz::engine::SoundResource aSound;
    pvz::engine::SoundResourceDiagnostic aDiagnostic;
    Expect(
        aManager.Load("SOUND_CLICK", aSound, aDiagnostic),
        "sound resource loads");
    Expect(
        aSound.mDescriptor.mSampleRate == 22'050 &&
            aSound.mDescriptor.mChannelCount == 1 &&
            aSound.mDescriptor.mFrameCount == 2,
        "sound descriptor crosses fixed-width boundary");
    Expect(
        aDecoder.mDecodeCount == 1 &&
            anAudioDevice.mCreateCount == 1,
        "sound decodes and uploads once");

    const auto aHandle = aSound.mSound;
    Expect(
        aManager.Load("SOUND_CLICK", aSound, aDiagnostic),
        "sound resource is cached");
    Expect(
        aSound.mSound.mIndex == aHandle.mIndex &&
            aSound.mSound.mGeneration == aHandle.mGeneration &&
            aDecoder.mDecodeCount == 1 &&
            anAudioDevice.mCreateCount == 1,
        "cached sound handle is stable");

    pvz::engine::VoiceHandle aVoice;
    const pvz::engine::SoundPlayback aPlayback{
        .mVolume = 0.75F,
        .mPan = -0.25F,
        .mPitch = 1.25F,
        .mLoopMode = pvz::engine::SoundLoopMode::Loop,
    };
    Expect(
        aManager.Play(aHandle, aPlayback, aVoice),
        "sound playback delegates to device");
    Expect(
        aManager.IsPlaying(aVoice),
        "sound voice state delegates to device");
    Expect(
        anAudioDevice.mPlayback.mLoopMode ==
                pvz::engine::SoundLoopMode::Loop &&
            anAudioDevice.mPlayback.mPitch == 1.25F,
        "sound playback parameters are preserved");
    aManager.Stop(aVoice);
    Expect(!aManager.IsPlaying(aVoice), "sound voice stops");
    aManager.SetMasterVolume(0.4F);
    Expect(
        anAudioDevice.mMasterVolume == 0.4F,
        "sound master volume delegates to device");

    aManager.Release(aHandle);
    Expect(
        anAudioDevice.mDestroyCount == 0,
        "first sound release retains cached upload");
    aManager.Release(aHandle);
    Expect(
        anAudioDevice.mDestroyCount == 1,
        "last sound release destroys uploaded sound");
}

void TestManifestAndResourceDiagnostics()
{
    MemoryResourceStore aResources;
    aResources.AddText(
        "duplicate.xml",
        "<ResourceManifest><Resources>"
        "<Sound id=\"A\" path=\"one\"/>"
        "<Sound id=\"A\" path=\"two\"/>"
        "</Resources></ResourceManifest>");
    aResources.AddText(
        "missing.xml",
        "<ResourceManifest><Resources>"
        "<Sound id=\"MISSING\" path=\"missing\"/>"
        "</Resources></ResourceManifest>");
    pvz::engine::core::ResourceXmlDocumentLoader aDocuments(
        aResources);
    TaggedAudioDecoder aDecoder;
    CapturingAudioDevice anAudioDevice;
    pvz::engine::core::SoundResourceManager aManager(
        aResources,
        aDocuments,
        aDecoder,
        anAudioDevice);

    Expect(
        !aManager.LoadManifest("duplicate.xml"),
        "duplicate sound id fails");
    Expect(
        aManager.GetManifestError() ==
            pvz::engine::core::SoundManifestError::
                DuplicateResource,
        "duplicate sound id error");
    Expect(
        aManager.LoadManifest("missing.xml"),
        "sound manifest can reload after validation failure");

    pvz::engine::SoundResource aSound;
    pvz::engine::SoundResourceDiagnostic aDiagnostic;
    Expect(
        !aManager.Load("MISSING", aSound, aDiagnostic),
        "missing sound source fails");
    Expect(
        aDiagnostic.mError ==
            pvz::engine::SoundResourceError::SourceNotFound &&
            aDiagnostic.mPath == "missing",
        "missing sound source diagnostic is retained");
}

} // namespace

void RunSoundResourceManagerTests()
{
    TestManifestLoadCacheAndPlayback();
    TestManifestAndResourceDiagnostics();
}
