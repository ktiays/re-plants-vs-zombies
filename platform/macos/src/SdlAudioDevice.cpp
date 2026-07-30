#include "pvz/platform/macos/SdlAudioDevice.h"

#include <SDL.h>
#include <libopenmpt/libopenmpt_ext.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pvz::platform::macos
{
namespace
{

inline constexpr std::uint32_t kOutputSampleRate = 48'000;
inline constexpr std::uint32_t kOutputChannelCount = 2;
inline constexpr std::size_t kMaximumVoiceCount = 32;
inline constexpr std::size_t kModuleRenderChunkFrames = 1'024;
inline constexpr float kSampleScale = 1.0F / 32768.0F;

[[nodiscard]] std::uint32_t NextGeneration(
    std::uint32_t theGeneration)
{
    return theGeneration ==
               std::numeric_limits<std::uint32_t>::max()
        ? 1
        : theGeneration + 1;
}

[[nodiscard]] bool IsPlaybackValid(
    const engine::SoundPlayback& thePlayback)
{
    return std::isfinite(thePlayback.mVolume) &&
           std::isfinite(thePlayback.mPan) &&
           std::isfinite(thePlayback.mPitch) &&
           thePlayback.mVolume >= 0.0F &&
           thePlayback.mPitch > 0.0F;
}

class AudioDeviceLock
{
public:
    explicit AudioDeviceLock(SDL_AudioDeviceID theDevice)
        : mDevice(theDevice)
    {
        if (mDevice != 0)
            SDL_LockAudioDevice(mDevice);
    }

    ~AudioDeviceLock()
    {
        if (mDevice != 0)
            SDL_UnlockAudioDevice(mDevice);
    }

private:
    SDL_AudioDeviceID mDevice{};
};

} // namespace

struct SdlAudioDevice::Implementation
{
    struct SoundSlot
    {
        std::vector<float> mStereoSamples;
        std::uint64_t mFrameCount{};
        std::uint32_t mGeneration{1};
        bool mOccupied{};
    };

    struct VoiceSlot
    {
        double mFramePosition{};
        float mVolume{1.0F};
        float mPan{};
        float mPitch{1.0F};
        std::uint32_t mSoundIndex{};
        std::uint32_t mSoundGeneration{};
        std::uint32_t mGeneration{1};
        engine::SoundLoopMode mLoopMode{
            engine::SoundLoopMode::Once};
        bool mOccupied{};
    };

    struct ModuleSlot
    {
        ~ModuleSlot()
        {
            Reset();
        }

        ModuleSlot() = default;
        ModuleSlot(const ModuleSlot&) = delete;
        ModuleSlot& operator=(const ModuleSlot&) = delete;

        void Reset()
        {
            if (mModule != nullptr)
                openmpt_module_ext_destroy(mModule);
            mModule = nullptr;
            mInteractive = {};
            mDescriptor = {};
            mVolume = 1.0F;
            mOccupied = false;
            mPlaying = false;
            mPaused = false;
        }

        openmpt_module_ext* mModule{};
        openmpt_module_ext_interface_interactive mInteractive{};
        engine::ModuleDescriptor mDescriptor;
        std::uint32_t mGeneration{1};
        float mVolume{1.0F};
        bool mOccupied{};
        bool mPlaying{};
        bool mPaused{};
    };

    ~Implementation()
    {
        if (mDevice != 0)
        {
            SDL_PauseAudioDevice(mDevice, 1);
            SDL_CloseAudioDevice(mDevice);
        }
        if (mOwnsAudioSubsystem)
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    [[nodiscard]] bool IsSoundValid(
        engine::SoundHandle theSound) const
    {
        if (!theSound.IsValid() || theSound.mIndex == 0)
            return false;
        const auto anIndex =
            static_cast<std::size_t>(theSound.mIndex - 1);
        return anIndex < mSounds.size() &&
               mSounds[anIndex].mOccupied &&
               mSounds[anIndex].mGeneration ==
                   theSound.mGeneration;
    }

    [[nodiscard]] bool IsVoiceValid(
        engine::VoiceHandle theVoice) const
    {
        if (!theVoice.IsValid() ||
            theVoice.mIndex == 0 ||
            theVoice.mIndex > mVoices.size())
        {
            return false;
        }
        const auto& aVoice =
            mVoices[
                static_cast<std::size_t>(theVoice.mIndex - 1)];
        return aVoice.mOccupied &&
               aVoice.mGeneration == theVoice.mGeneration;
    }

    [[nodiscard]] bool IsModuleValid(
        engine::ModuleHandle theModule) const
    {
        if (!theModule.IsValid() || theModule.mIndex == 0)
            return false;
        const auto anIndex =
            static_cast<std::size_t>(theModule.mIndex - 1);
        return anIndex < mModules.size() &&
               mModules[anIndex] != nullptr &&
               mModules[anIndex]->mOccupied &&
               mModules[anIndex]->mGeneration ==
                   theModule.mGeneration;
    }

    [[nodiscard]] static bool SetPosition(
        ModuleSlot& theSlot,
        engine::MusicPosition thePosition)
    {
        if (thePosition.mOrder >=
                theSlot.mDescriptor.mOrderCount ||
            thePosition.mOrder >
                static_cast<std::uint32_t>(
                    std::numeric_limits<std::int32_t>::max()) ||
            thePosition.mRow >
                static_cast<std::uint32_t>(
                    std::numeric_limits<std::int32_t>::max()))
        {
            return false;
        }
        auto* const aModule =
            openmpt_module_ext_get_module(theSlot.mModule);
        if (aModule == nullptr)
            return false;
        static_cast<void>(
            openmpt_module_set_position_order_row(
                aModule,
                static_cast<std::int32_t>(thePosition.mOrder),
                static_cast<std::int32_t>(thePosition.mRow)));
        return
            openmpt_module_get_current_order(aModule) ==
                static_cast<std::int32_t>(thePosition.mOrder) &&
            openmpt_module_get_current_row(aModule) ==
                static_cast<std::int32_t>(thePosition.mRow);
    }

    void RetireVoice(VoiceSlot& theVoice)
    {
        theVoice.mOccupied = false;
        theVoice.mGeneration =
            NextGeneration(theVoice.mGeneration);
    }

    void Mix(float* theOutput, std::size_t theFrameCount)
    {
        for (auto& aVoice : mVoices)
        {
            if (!aVoice.mOccupied ||
                aVoice.mSoundIndex == 0)
            {
                continue;
            }
            const auto aSoundIndex =
                static_cast<std::size_t>(
                    aVoice.mSoundIndex - 1);
            if (aSoundIndex >= mSounds.size())
            {
                RetireVoice(aVoice);
                continue;
            }
            const auto& aSound = mSounds[aSoundIndex];
            if (!aSound.mOccupied ||
                aSound.mGeneration !=
                    aVoice.mSoundGeneration ||
                aSound.mFrameCount == 0)
            {
                RetireVoice(aVoice);
                continue;
            }

            const float aPan =
                std::clamp(aVoice.mPan, -1.0F, 1.0F);
            const float aLeftGain =
                aVoice.mVolume *
                (aPan > 0.0F ? 1.0F - aPan : 1.0F) *
                mMasterVolume;
            const float aRightGain =
                aVoice.mVolume *
                (aPan < 0.0F ? 1.0F + aPan : 1.0F) *
                mMasterVolume;

            for (std::size_t anOutputFrame = 0;
                 anOutputFrame < theFrameCount;
                 ++anOutputFrame)
            {
                if (aVoice.mFramePosition >=
                    static_cast<double>(aSound.mFrameCount))
                {
                    if (aVoice.mLoopMode ==
                        engine::SoundLoopMode::Loop)
                    {
                        aVoice.mFramePosition = std::fmod(
                            aVoice.mFramePosition,
                            static_cast<double>(
                                aSound.mFrameCount));
                    }
                    else
                    {
                        RetireVoice(aVoice);
                        break;
                    }
                }

                const auto aFrame = static_cast<std::uint64_t>(
                    aVoice.mFramePosition);
                const auto aNextFrame =
                    std::min(
                        aFrame + 1,
                        aSound.mFrameCount - 1);
                const float aFraction = static_cast<float>(
                    aVoice.mFramePosition -
                    static_cast<double>(aFrame));
                const auto anOffset =
                    static_cast<std::size_t>(aFrame) * 2;
                const auto aNextOffset =
                    static_cast<std::size_t>(aNextFrame) * 2;
                const float aLeft =
                    aSound.mStereoSamples[anOffset] +
                    (aSound.mStereoSamples[aNextOffset] -
                     aSound.mStereoSamples[anOffset]) *
                        aFraction;
                const float aRight =
                    aSound.mStereoSamples[anOffset + 1] +
                    (aSound.mStereoSamples[aNextOffset + 1] -
                     aSound.mStereoSamples[anOffset + 1]) *
                        aFraction;
                theOutput[anOutputFrame * 2] +=
                    aLeft * aLeftGain;
                theOutput[anOutputFrame * 2 + 1] +=
                    aRight * aRightGain;
                aVoice.mFramePosition += aVoice.mPitch;
            }
        }

        for (const auto& aModuleSlot : mModules)
        {
            if (aModuleSlot == nullptr ||
                !aModuleSlot->mOccupied ||
                !aModuleSlot->mPlaying ||
                aModuleSlot->mPaused)
            {
                continue;
            }
            auto* const aModule =
                openmpt_module_ext_get_module(
                    aModuleSlot->mModule);
            if (aModule == nullptr)
            {
                aModuleSlot->mPlaying = false;
                continue;
            }

            std::size_t anOutputFrame{};
            while (anOutputFrame < theFrameCount)
            {
                const auto aRequestedFrameCount =
                    std::min(
                        kModuleRenderChunkFrames,
                        theFrameCount - anOutputFrame);
                const auto aRenderedFrameCount =
                    openmpt_module_read_interleaved_float_stereo(
                        aModule,
                        static_cast<std::int32_t>(
                            mOutputSampleRate),
                        aRequestedFrameCount,
                        mModuleScratch.data());
                if (aRenderedFrameCount == 0)
                {
                    aModuleSlot->mPlaying = false;
                    break;
                }

                const float aGain =
                    aModuleSlot->mVolume *
                    mMusicMasterVolume;
                for (std::size_t aFrame = 0;
                     aFrame < aRenderedFrameCount;
                     ++aFrame)
                {
                    const auto anOutputOffset =
                        (anOutputFrame + aFrame) * 2;
                    const auto aModuleOffset = aFrame * 2;
                    theOutput[anOutputOffset] +=
                        mModuleScratch[aModuleOffset] * aGain;
                    theOutput[anOutputOffset + 1] +=
                        mModuleScratch[aModuleOffset + 1] *
                        aGain;
                }
                anOutputFrame += aRenderedFrameCount;
                if (aRenderedFrameCount <
                    aRequestedFrameCount)
                {
                    aModuleSlot->mPlaying = false;
                    break;
                }
            }
        }

        const auto aSampleCount = theFrameCount * 2;
        for (std::size_t aSample = 0;
             aSample < aSampleCount;
             ++aSample)
        {
            theOutput[aSample] =
                std::clamp(theOutput[aSample], -1.0F, 1.0F);
        }
    }

    static void AudioCallback(
        void* theUserData,
        std::uint8_t* theStream,
        int theByteCount)
    {
        auto& anImplementation =
            *static_cast<Implementation*>(theUserData);
        if (theByteCount <= 0)
            return;
        std::memset(
            theStream,
            0,
            static_cast<std::size_t>(theByteCount));
        const auto aFrameByteCount =
            sizeof(float) * kOutputChannelCount;
        const auto aFrameCount =
            static_cast<std::size_t>(theByteCount) /
            aFrameByteCount;
        anImplementation.Mix(
            reinterpret_cast<float*>(theStream),
            aFrameCount);
    }

    std::vector<SoundSlot> mSounds;
    std::array<VoiceSlot, kMaximumVoiceCount> mVoices;
    std::vector<std::unique_ptr<ModuleSlot>> mModules;
    std::array<float, kModuleRenderChunkFrames * 2>
        mModuleScratch;
    std::string mLastError;
    SDL_AudioDeviceID mDevice{};
    std::uint32_t mOutputSampleRate{};
    float mMasterVolume{1.0F};
    float mMusicMasterVolume{1.0F};
    bool mOwnsAudioSubsystem{};
};

SdlAudioDevice::SdlAudioDevice()
    : mImplementation(std::make_unique<Implementation>())
{
}

SdlAudioDevice::~SdlAudioDevice() = default;

bool SdlAudioDevice::Initialize()
{
    if (mImplementation->mDevice != 0)
        return true;
    mImplementation->mLastError.clear();

    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0)
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        {
            mImplementation->mLastError = SDL_GetError();
            return false;
        }
        mImplementation->mOwnsAudioSubsystem = true;
    }

    SDL_AudioSpec aRequested{};
    aRequested.freq = static_cast<int>(kOutputSampleRate);
    aRequested.format = AUDIO_F32SYS;
    aRequested.channels =
        static_cast<std::uint8_t>(kOutputChannelCount);
    aRequested.samples = 1'024;
    aRequested.callback = &Implementation::AudioCallback;
    aRequested.userdata = mImplementation.get();

    SDL_AudioSpec anObtained{};
    mImplementation->mDevice = SDL_OpenAudioDevice(
        nullptr,
        0,
        &aRequested,
        &anObtained,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (mImplementation->mDevice == 0)
    {
        mImplementation->mLastError = SDL_GetError();
        return false;
    }
    if (anObtained.format != AUDIO_F32SYS ||
        anObtained.channels != kOutputChannelCount ||
        anObtained.freq <= 0)
    {
        mImplementation->mLastError =
            "SDL returned an unsupported audio format";
        SDL_CloseAudioDevice(mImplementation->mDevice);
        mImplementation->mDevice = 0;
        return false;
    }
    mImplementation->mOutputSampleRate =
        static_cast<std::uint32_t>(anObtained.freq);
    SDL_PauseAudioDevice(mImplementation->mDevice, 0);
    return true;
}

std::string_view SdlAudioDevice::GetLastError() const
{
    return mImplementation->mLastError;
}

bool SdlAudioDevice::CreateSound(
    const engine::SoundDescriptor& theDescriptor,
    std::span<const std::int16_t> theInterleavedSamples,
    engine::SoundHandle& theSound)
{
    theSound = {};
    if (mImplementation->mDevice == 0 ||
        theDescriptor.mSampleRate == 0 ||
        (theDescriptor.mChannelCount != 1 &&
         theDescriptor.mChannelCount != 2) ||
        theDescriptor.mFrameCount == 0 ||
        theDescriptor.mFrameCount >
            std::numeric_limits<std::uint64_t>::max() /
                theDescriptor.mChannelCount)
    {
        return false;
    }
    const auto aSourceSampleCount =
        theDescriptor.mFrameCount *
        theDescriptor.mChannelCount;
    if (aSourceSampleCount != theInterleavedSamples.size() ||
        theDescriptor.mFrameCount >
            (std::numeric_limits<std::uint64_t>::max() -
             theDescriptor.mSampleRate + 1) /
                mImplementation->mOutputSampleRate)
    {
        return false;
    }

    const auto anOutputFrameCount =
        (theDescriptor.mFrameCount *
             mImplementation->mOutputSampleRate +
         theDescriptor.mSampleRate - 1) /
        theDescriptor.mSampleRate;
    if (anOutputFrameCount == 0 ||
        anOutputFrameCount >
            std::numeric_limits<std::size_t>::max() / 2)
    {
        return false;
    }

    std::vector<float> aStereoSamples;
    try
    {
        aStereoSamples.resize(
            static_cast<std::size_t>(anOutputFrameCount) * 2);
    }
    catch (...)
    {
        return false;
    }
    for (std::uint64_t anOutputFrame = 0;
         anOutputFrame < anOutputFrameCount;
         ++anOutputFrame)
    {
        const double aSourcePosition =
            static_cast<double>(anOutputFrame) *
            theDescriptor.mSampleRate /
            mImplementation->mOutputSampleRate;
        const auto aSourceFrame =
            std::min(
                static_cast<std::uint64_t>(aSourcePosition),
                theDescriptor.mFrameCount - 1);
        const auto aNextSourceFrame =
            std::min(
                aSourceFrame + 1,
                theDescriptor.mFrameCount - 1);
        const float aFraction = static_cast<float>(
            aSourcePosition -
            static_cast<double>(aSourceFrame));
        for (std::uint32_t anOutputChannel = 0;
             anOutputChannel < 2;
             ++anOutputChannel)
        {
            const auto aSourceChannel =
                theDescriptor.mChannelCount == 1
                    ? 0
                    : anOutputChannel;
            const auto aSourceOffset =
                static_cast<std::size_t>(
                    aSourceFrame *
                        theDescriptor.mChannelCount +
                    aSourceChannel);
            const auto aNextSourceOffset =
                static_cast<std::size_t>(
                    aNextSourceFrame *
                        theDescriptor.mChannelCount +
                    aSourceChannel);
            const float aFirst =
                static_cast<float>(
                    theInterleavedSamples[aSourceOffset]) *
                kSampleScale;
            const float aSecond =
                static_cast<float>(
                    theInterleavedSamples[aNextSourceOffset]) *
                kSampleScale;
            aStereoSamples[
                static_cast<std::size_t>(anOutputFrame) * 2 +
                anOutputChannel] =
                aFirst + (aSecond - aFirst) * aFraction;
        }
    }

    const AudioDeviceLock aLock(mImplementation->mDevice);
    std::size_t aSlotIndex{};
    for (; aSlotIndex < mImplementation->mSounds.size();
         ++aSlotIndex)
    {
        if (!mImplementation->mSounds[aSlotIndex].mOccupied)
            break;
    }
    if (aSlotIndex == mImplementation->mSounds.size())
    {
        if (aSlotIndex >=
            std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }
        mImplementation->mSounds.emplace_back();
    }

    auto& aSlot = mImplementation->mSounds[aSlotIndex];
    aSlot.mStereoSamples = std::move(aStereoSamples);
    aSlot.mFrameCount = anOutputFrameCount;
    aSlot.mOccupied = true;
    theSound = {
        .mIndex = static_cast<std::uint32_t>(aSlotIndex + 1),
        .mGeneration = aSlot.mGeneration,
    };
    return true;
}

void SdlAudioDevice::DestroySound(engine::SoundHandle theSound)
{
    const AudioDeviceLock aLock(mImplementation->mDevice);
    if (!mImplementation->IsSoundValid(theSound))
        return;

    for (auto& aVoice : mImplementation->mVoices)
    {
        if (aVoice.mOccupied &&
            aVoice.mSoundIndex == theSound.mIndex &&
            aVoice.mSoundGeneration == theSound.mGeneration)
        {
            mImplementation->RetireVoice(aVoice);
        }
    }
    auto& aSlot =
        mImplementation->mSounds[
            static_cast<std::size_t>(theSound.mIndex - 1)];
    std::vector<float>().swap(aSlot.mStereoSamples);
    aSlot.mFrameCount = 0;
    aSlot.mOccupied = false;
    aSlot.mGeneration = NextGeneration(aSlot.mGeneration);
}

bool SdlAudioDevice::Play(
    engine::SoundHandle theSound,
    const engine::SoundPlayback& thePlayback,
    engine::VoiceHandle& theVoice)
{
    theVoice = {};
    if (!IsPlaybackValid(thePlayback))
        return false;

    const AudioDeviceLock aLock(mImplementation->mDevice);
    if (!mImplementation->IsSoundValid(theSound))
        return false;

    for (std::size_t anIndex = 0;
         anIndex < mImplementation->mVoices.size();
         ++anIndex)
    {
        auto& aVoice = mImplementation->mVoices[anIndex];
        if (aVoice.mOccupied)
            continue;
        aVoice.mFramePosition = 0.0;
        aVoice.mVolume = thePlayback.mVolume;
        aVoice.mPan = thePlayback.mPan;
        aVoice.mPitch = thePlayback.mPitch;
        aVoice.mSoundIndex = theSound.mIndex;
        aVoice.mSoundGeneration = theSound.mGeneration;
        aVoice.mLoopMode = thePlayback.mLoopMode;
        aVoice.mOccupied = true;
        theVoice = {
            .mIndex = static_cast<std::uint32_t>(anIndex + 1),
            .mGeneration = aVoice.mGeneration,
        };
        return true;
    }
    return false;
}

void SdlAudioDevice::Stop(engine::VoiceHandle theVoice)
{
    const AudioDeviceLock aLock(mImplementation->mDevice);
    if (!mImplementation->IsVoiceValid(theVoice))
        return;
    auto& aVoice =
        mImplementation->mVoices[
            static_cast<std::size_t>(theVoice.mIndex - 1)];
    mImplementation->RetireVoice(aVoice);
}

void SdlAudioDevice::StopAll()
{
    const AudioDeviceLock aLock(mImplementation->mDevice);
    for (auto& aVoice : mImplementation->mVoices)
    {
        if (aVoice.mOccupied)
            mImplementation->RetireVoice(aVoice);
    }
}

bool SdlAudioDevice::IsPlaying(
    engine::VoiceHandle theVoice) const
{
    const AudioDeviceLock aLock(mImplementation->mDevice);
    return mImplementation->IsVoiceValid(theVoice);
}

void SdlAudioDevice::SetMasterVolume(float theVolume)
{
    if (!std::isfinite(theVolume))
        return;
    const AudioDeviceLock aLock(mImplementation->mDevice);
    mImplementation->mMasterVolume =
        std::clamp(theVolume, 0.0F, 1.0F);
}

bool SdlAudioDevice::CreateModule(
    std::span<const std::byte> theEncodedBytes,
    engine::ModuleHandle& theModule,
    engine::ModuleDescriptor& theDescriptor)
{
    theModule = {};
    theDescriptor = {};
    if (mImplementation->mDevice == 0 ||
        theEncodedBytes.empty())
    {
        return false;
    }

    int anError{};
    const char* anErrorMessage{};
    auto aSlot =
        std::make_unique<Implementation::ModuleSlot>();
    aSlot->mModule = openmpt_module_ext_create_from_memory(
        theEncodedBytes.data(),
        theEncodedBytes.size(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &anError,
        &anErrorMessage,
        nullptr);
    if (aSlot->mModule == nullptr)
    {
        mImplementation->mLastError =
            anErrorMessage != nullptr
                ? anErrorMessage
                : "libopenmpt rejected the module";
        if (anErrorMessage != nullptr)
            openmpt_free_string(anErrorMessage);
        static_cast<void>(anError);
        return false;
    }
    if (anErrorMessage != nullptr)
        openmpt_free_string(anErrorMessage);
    static_cast<void>(anError);

    auto* const aModule =
        openmpt_module_ext_get_module(aSlot->mModule);
    const auto aChannelCount =
        aModule != nullptr
            ? openmpt_module_get_num_channels(aModule)
            : 0;
    const auto anOrderCount =
        aModule != nullptr
            ? openmpt_module_get_num_orders(aModule)
            : 0;
    if (aModule == nullptr ||
        aChannelCount <= 0 ||
        anOrderCount <= 0 ||
        openmpt_module_ext_get_interface(
            aSlot->mModule,
            LIBOPENMPT_EXT_C_INTERFACE_INTERACTIVE,
            &aSlot->mInteractive,
            sizeof(aSlot->mInteractive)) == 0)
    {
        mImplementation->mLastError =
            "libopenmpt module lacks required interactive controls";
        return false;
    }
    aSlot->mDescriptor = {
        .mChannelCount =
            static_cast<std::uint32_t>(aChannelCount),
        .mOrderCount =
            static_cast<std::uint32_t>(anOrderCount),
    };
    aSlot->mOccupied = true;

    const AudioDeviceLock aLock(mImplementation->mDevice);
    std::size_t aSlotIndex{};
    for (; aSlotIndex < mImplementation->mModules.size();
         ++aSlotIndex)
    {
        if (mImplementation->mModules[aSlotIndex] == nullptr ||
            !mImplementation->mModules[aSlotIndex]->mOccupied)
        {
            break;
        }
    }
    if (aSlotIndex ==
        std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }
    if (aSlotIndex == mImplementation->mModules.size())
    {
        mImplementation->mModules.push_back(std::move(aSlot));
    }
    else
    {
        const auto aGeneration =
            mImplementation->mModules[aSlotIndex] != nullptr
                ? mImplementation->mModules[aSlotIndex]
                      ->mGeneration
                : 1;
        aSlot->mGeneration = aGeneration;
        mImplementation->mModules[aSlotIndex] =
            std::move(aSlot);
    }

    const auto& aStoredSlot =
        *mImplementation->mModules[aSlotIndex];
    theModule = {
        .mIndex = static_cast<std::uint32_t>(aSlotIndex + 1),
        .mGeneration = aStoredSlot.mGeneration,
    };
    theDescriptor = aStoredSlot.mDescriptor;
    return true;
}

void SdlAudioDevice::DestroyModule(
    engine::ModuleHandle theModule)
{
    const AudioDeviceLock aLock(mImplementation->mDevice);
    if (!mImplementation->IsModuleValid(theModule))
        return;

    const auto anIndex =
        static_cast<std::size_t>(theModule.mIndex - 1);
    auto& aSlot = *mImplementation->mModules[anIndex];
    const auto aNextGeneration =
        NextGeneration(aSlot.mGeneration);
    aSlot.Reset();
    aSlot.mGeneration = aNextGeneration;
}

bool SdlAudioDevice::PlayModule(
    engine::ModuleHandle theModule,
    const engine::MusicPlayback& thePlayback)
{
    if (!std::isfinite(thePlayback.mVolume) ||
        thePlayback.mVolume < 0.0F)
    {
        return false;
    }

    const AudioDeviceLock aLock(mImplementation->mDevice);
    if (!mImplementation->IsModuleValid(theModule))
        return false;
    auto& aSlot =
        *mImplementation->mModules[
            static_cast<std::size_t>(theModule.mIndex - 1)];
    auto* const aModule =
        openmpt_module_ext_get_module(aSlot.mModule);
    if (aModule == nullptr ||
        openmpt_module_set_repeat_count(
            aModule,
            thePlayback.mLoopMode == engine::MusicLoopMode::Loop
                ? -1
                : 0) == 0 ||
        !Implementation::SetPosition(
            aSlot,
            thePlayback.mPosition))
    {
        return false;
    }

    aSlot.mVolume =
        std::clamp(thePlayback.mVolume, 0.0F, 1.0F);
    aSlot.mPaused = false;
    aSlot.mPlaying = true;
    return true;
}

void SdlAudioDevice::StopModule(
    engine::ModuleHandle theModule)
{
    const AudioDeviceLock aLock(mImplementation->mDevice);
    if (!mImplementation->IsModuleValid(theModule))
        return;
    auto& aSlot =
        *mImplementation->mModules[
            static_cast<std::size_t>(theModule.mIndex - 1)];
    aSlot.mPlaying = false;
    aSlot.mPaused = false;
}

void SdlAudioDevice::PauseModule(
    engine::ModuleHandle theModule,
    bool thePaused)
{
    const AudioDeviceLock aLock(mImplementation->mDevice);
    if (!mImplementation->IsModuleValid(theModule))
        return;
    mImplementation
        ->mModules[
            static_cast<std::size_t>(theModule.mIndex - 1)]
        ->mPaused = thePaused;
}

bool SdlAudioDevice::IsModulePlaying(
    engine::ModuleHandle theModule) const
{
    const AudioDeviceLock aLock(mImplementation->mDevice);
    if (!mImplementation->IsModuleValid(theModule))
        return false;
    return mImplementation
        ->mModules[
            static_cast<std::size_t>(theModule.mIndex - 1)]
        ->mPlaying;
}

bool SdlAudioDevice::SetModulePosition(
    engine::ModuleHandle theModule,
    engine::MusicPosition thePosition)
{
    const AudioDeviceLock aLock(mImplementation->mDevice);
    if (!mImplementation->IsModuleValid(theModule))
        return false;
    return Implementation::SetPosition(
        *mImplementation
             ->mModules[
                 static_cast<std::size_t>(
                     theModule.mIndex - 1)],
        thePosition);
}

bool SdlAudioDevice::GetModulePosition(
    engine::ModuleHandle theModule,
    engine::MusicPosition& thePosition) const
{
    thePosition = {};
    const AudioDeviceLock aLock(mImplementation->mDevice);
    if (!mImplementation->IsModuleValid(theModule))
        return false;
    const auto& aSlot =
        *mImplementation
             ->mModules[
                 static_cast<std::size_t>(
                     theModule.mIndex - 1)];
    auto* const aModule =
        openmpt_module_ext_get_module(aSlot.mModule);
    if (aModule == nullptr)
        return false;
    const auto anOrder =
        openmpt_module_get_current_order(aModule);
    const auto aRow =
        openmpt_module_get_current_row(aModule);
    if (anOrder < 0 || aRow < 0)
        return false;
    thePosition = {
        .mOrder = static_cast<std::uint32_t>(anOrder),
        .mRow = static_cast<std::uint32_t>(aRow),
    };
    return true;
}

bool SdlAudioDevice::SetModuleChannelEnabled(
    engine::ModuleHandle theModule,
    std::uint32_t theChannel,
    bool theEnabled)
{
    const AudioDeviceLock aLock(mImplementation->mDevice);
    if (!mImplementation->IsModuleValid(theModule))
        return false;
    auto& aSlot =
        *mImplementation
             ->mModules[
                 static_cast<std::size_t>(
                     theModule.mIndex - 1)];
    if (theChannel >= aSlot.mDescriptor.mChannelCount ||
        theChannel >
            static_cast<std::uint32_t>(
                std::numeric_limits<std::int32_t>::max()))
    {
        return false;
    }
    return aSlot.mInteractive.set_channel_mute_status(
               aSlot.mModule,
               static_cast<std::int32_t>(theChannel),
               theEnabled ? 0 : 1) != 0;
}

bool SdlAudioDevice::SetModuleVolume(
    engine::ModuleHandle theModule,
    float theVolume)
{
    if (!std::isfinite(theVolume) || theVolume < 0.0F)
        return false;
    const AudioDeviceLock aLock(mImplementation->mDevice);
    if (!mImplementation->IsModuleValid(theModule))
        return false;
    mImplementation
        ->mModules[
            static_cast<std::size_t>(theModule.mIndex - 1)]
        ->mVolume = std::clamp(theVolume, 0.0F, 1.0F);
    return true;
}

bool SdlAudioDevice::SetModuleTempoFactor(
    engine::ModuleHandle theModule,
    float theFactor)
{
    if (!std::isfinite(theFactor) ||
        theFactor <= 0.0F ||
        theFactor > 4.0F)
    {
        return false;
    }
    const AudioDeviceLock aLock(mImplementation->mDevice);
    if (!mImplementation->IsModuleValid(theModule))
        return false;
    auto& aSlot =
        *mImplementation
             ->mModules[
                 static_cast<std::size_t>(
                     theModule.mIndex - 1)];
    return aSlot.mInteractive.set_tempo_factor(
               aSlot.mModule,
               static_cast<double>(theFactor)) != 0;
}

void SdlAudioDevice::SetMusicMasterVolume(float theVolume)
{
    if (!std::isfinite(theVolume))
        return;
    const AudioDeviceLock aLock(mImplementation->mDevice);
    mImplementation->mMusicMasterVolume =
        std::clamp(theVolume, 0.0F, 1.0F);
}

} // namespace pvz::platform::macos
