#include "pvz/engine/audio/PortableAudioDecoder.h"

#include "pvz_tremor_adapter.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>

namespace pvz::engine::audio
{
namespace
{

[[nodiscard]] bool IsOgg(std::span<const std::byte> theBytes)
{
    return theBytes.size() >= 4 &&
           theBytes[0] == std::byte{'O'} &&
           theBytes[1] == std::byte{'g'} &&
           theBytes[2] == std::byte{'g'} &&
           theBytes[3] == std::byte{'S'};
}

[[nodiscard]] SoundDecodeError MapError(std::uint32_t theError)
{
    switch (theError)
    {
    case PVZ_TREMOR_CHANNELS_UNSUPPORTED:
        return SoundDecodeError::ChannelsUnsupported;
    case PVZ_TREMOR_SAMPLE_RATE_UNSUPPORTED:
        return SoundDecodeError::SampleRateUnsupported;
    case PVZ_TREMOR_SIZE_OVERFLOW:
        return SoundDecodeError::SizeOverflow;
    case PVZ_TREMOR_ALLOCATION_FAILED:
        return SoundDecodeError::AllocationFailed;
    default:
        return SoundDecodeError::InvalidData;
    }
}

} // namespace

bool PortableAudioDecoder::Decode(
    std::span<const std::byte> theEncodedBytes,
    DecodedSound& theSound,
    SoundDecodeError& theError) const
{
    theSound = {};
    theError = SoundDecodeError::None;
    if (theEncodedBytes.empty())
    {
        theError = SoundDecodeError::EmptyInput;
        return false;
    }
    if (!IsOgg(theEncodedBytes))
    {
        theError = SoundDecodeError::UnsupportedFormat;
        return false;
    }
    if (theEncodedBytes.size() >
        std::numeric_limits<std::uint64_t>::max())
    {
        theError = SoundDecodeError::SizeOverflow;
        return false;
    }

    PvzTremorDecodedSound aDecoded{};
    const auto aResult = pvz_tremor_decode(
        reinterpret_cast<const std::uint8_t*>(
            theEncodedBytes.data()),
        static_cast<std::uint64_t>(theEncodedBytes.size()),
        &aDecoded);
    if (aResult != PVZ_TREMOR_OK)
    {
        theError = MapError(aResult);
        return false;
    }

    if (aDecoded.mChannelCount == 0 ||
        aDecoded.mFrameCount >
            std::numeric_limits<std::uint64_t>::max() /
                aDecoded.mChannelCount)
    {
        pvz_tremor_release(&aDecoded);
        theError = SoundDecodeError::SizeOverflow;
        return false;
    }
    const auto aSampleCount =
        aDecoded.mFrameCount * aDecoded.mChannelCount;
    if (aSampleCount > std::numeric_limits<std::size_t>::max())
    {
        pvz_tremor_release(&aDecoded);
        theError = SoundDecodeError::SizeOverflow;
        return false;
    }

    try
    {
        theSound.mInterleavedSamples.assign(
            aDecoded.mInterleavedSamples,
            aDecoded.mInterleavedSamples +
                static_cast<std::size_t>(aSampleCount));
    }
    catch (const std::bad_alloc&)
    {
        pvz_tremor_release(&aDecoded);
        theError = SoundDecodeError::AllocationFailed;
        return false;
    }
    catch (const std::length_error&)
    {
        pvz_tremor_release(&aDecoded);
        theError = SoundDecodeError::SizeOverflow;
        return false;
    }

    theSound.mDescriptor = {
        .mSampleRate = aDecoded.mSampleRate,
        .mChannelCount = aDecoded.mChannelCount,
        .mFrameCount = aDecoded.mFrameCount,
    };
    pvz_tremor_release(&aDecoded);
    return true;
}

} // namespace pvz::engine::audio
