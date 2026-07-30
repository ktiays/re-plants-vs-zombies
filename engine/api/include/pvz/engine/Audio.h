#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pvz::engine
{

struct SoundHandle
{
    std::uint32_t mIndex{};
    std::uint32_t mGeneration{};

    [[nodiscard]] constexpr bool IsValid() const
    {
        return mGeneration != 0;
    }
};

struct VoiceHandle
{
    std::uint32_t mIndex{};
    std::uint32_t mGeneration{};

    [[nodiscard]] constexpr bool IsValid() const
    {
        return mGeneration != 0;
    }
};

struct SoundDescriptor
{
    std::uint32_t mSampleRate{};
    std::uint32_t mChannelCount{};
    std::uint64_t mFrameCount{};
};

enum class SoundDecodeError : std::uint8_t
{
    None,
    EmptyInput,
    UnsupportedFormat,
    InvalidData,
    ChannelsUnsupported,
    SampleRateUnsupported,
    SizeOverflow,
    AllocationFailed,
};

struct DecodedSound
{
    SoundDescriptor mDescriptor;
    std::vector<std::int16_t> mInterleavedSamples;
};

class IAudioDecoder
{
public:
    virtual ~IAudioDecoder() = default;

    [[nodiscard]] virtual bool Decode(
        std::span<const std::byte> theEncodedBytes,
        DecodedSound& theSound,
        SoundDecodeError& theError) const = 0;
};

enum class SoundLoopMode : std::uint8_t
{
    Once,
    Loop,
};

struct SoundPlayback
{
    float mVolume{1.0F};
    float mPan{};
    float mPitch{1.0F};
    SoundLoopMode mLoopMode{SoundLoopMode::Once};
};

class IAudioDevice
{
public:
    virtual ~IAudioDevice() = default;

    [[nodiscard]] virtual bool CreateSound(
        const SoundDescriptor& theDescriptor,
        std::span<const std::int16_t> theInterleavedSamples,
        SoundHandle& theSound) = 0;
    virtual void DestroySound(SoundHandle theSound) = 0;
    [[nodiscard]] virtual bool Play(
        SoundHandle theSound,
        const SoundPlayback& thePlayback,
        VoiceHandle& theVoice) = 0;
    virtual void Stop(VoiceHandle theVoice) = 0;
    virtual void StopAll() = 0;
    [[nodiscard]] virtual bool IsPlaying(
        VoiceHandle theVoice) const = 0;
    virtual void SetMasterVolume(float theVolume) = 0;
};

enum class SoundResourceError : std::uint8_t
{
    None,
    ManifestNotLoaded,
    ResourceNotFound,
    SourceNotFound,
    SourceReadFailed,
    DecodeFailed,
    DeviceUploadFailed,
    ReferenceCountOverflow,
};

struct SoundResourceDiagnostic
{
    SoundResourceError mError{SoundResourceError::None};
    SoundDecodeError mDecodeError{SoundDecodeError::None};
    std::string mPath;
};

struct SoundResource
{
    SoundHandle mSound{};
    SoundDescriptor mDescriptor{};
};

class ISoundResources
{
public:
    virtual ~ISoundResources() = default;

    [[nodiscard]] virtual bool Load(
        std::string_view theResourceId,
        SoundResource& theResource,
        SoundResourceDiagnostic& theDiagnostic) = 0;
    virtual void Release(SoundHandle theSound) = 0;
    [[nodiscard]] virtual bool Play(
        SoundHandle theSound,
        const SoundPlayback& thePlayback,
        VoiceHandle& theVoice) = 0;
    virtual void Stop(VoiceHandle theVoice) = 0;
    virtual void StopAll() = 0;
    [[nodiscard]] virtual bool IsPlaying(
        VoiceHandle theVoice) const = 0;
    virtual void SetMasterVolume(float theVolume) = 0;
};

static_assert(sizeof(SoundHandle) == 8);
static_assert(sizeof(VoiceHandle) == 8);
static_assert(sizeof(SoundDescriptor) == 16);
static_assert(sizeof(SoundLoopMode) == 1);
static_assert(sizeof(SoundDecodeError) == 1);
static_assert(sizeof(SoundResourceError) == 1);

} // namespace pvz::engine
