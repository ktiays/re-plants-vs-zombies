#include "pvz_tremor_adapter.h"

#include "ivorbisfile.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MemorySource
{
    const uint8_t* mBytes;
    uint64_t mSize;
    uint64_t mOffset;
} MemorySource;

static size_t ReadMemory(
    void* theDestination,
    size_t theElementSize,
    size_t theElementCount,
    void* theSource)
{
    MemorySource* aSource = (MemorySource*)theSource;
    size_t anAvailable;
    size_t anElementCount;
    size_t aByteCount;
    if (theElementSize == 0 || theElementCount == 0)
        return 0;
    if (aSource->mOffset > aSource->mSize)
        return 0;

    if (aSource->mSize - aSource->mOffset > SIZE_MAX)
        anAvailable = SIZE_MAX;
    else
        anAvailable =
            (size_t)(aSource->mSize - aSource->mOffset);
    anElementCount = anAvailable / theElementSize;
    if (anElementCount > theElementCount)
        anElementCount = theElementCount;
    aByteCount = anElementCount * theElementSize;
    memcpy(
        theDestination,
        aSource->mBytes + (size_t)aSource->mOffset,
        aByteCount);
    aSource->mOffset += aByteCount;
    return anElementCount;
}

static int SeekMemory(
    void* theSource,
    ogg_int64_t theOffset,
    int theOrigin)
{
    MemorySource* aSource = (MemorySource*)theSource;
    int64_t aBase;
    int64_t anOffset = (int64_t)theOffset;
    int64_t aPosition;
    if (aSource->mSize > INT64_MAX ||
        aSource->mOffset > INT64_MAX)
    {
        return -1;
    }

    if (theOrigin == SEEK_SET)
        aBase = 0;
    else if (theOrigin == SEEK_CUR)
        aBase = (int64_t)aSource->mOffset;
    else if (theOrigin == SEEK_END)
        aBase = (int64_t)aSource->mSize;
    else
        return -1;

    if ((anOffset > 0 && aBase > INT64_MAX - anOffset) ||
        (anOffset < 0 && aBase < INT64_MIN - anOffset))
    {
        return -1;
    }
    aPosition = aBase + anOffset;
    if (aPosition < 0 ||
        (uint64_t)aPosition > aSource->mSize)
    {
        return -1;
    }
    aSource->mOffset = (uint64_t)aPosition;
    return 0;
}

static int CloseMemory(void* theSource)
{
    (void)theSource;
    return 0;
}

static long TellMemory(void* theSource)
{
    MemorySource* aSource = (MemorySource*)theSource;
    if (aSource->mOffset > LONG_MAX)
        return -1;
    return (long)aSource->mOffset;
}

uint32_t pvz_tremor_decode(
    const uint8_t* theEncodedBytes,
    uint64_t theEncodedByteCount,
    PvzTremorDecodedSound* theSound)
{
    MemorySource aSource;
    OggVorbis_File aFile;
    ov_callbacks aCallbacks;
    vorbis_info* anInfo;
    ogg_int64_t aFrameCount;
    uint64_t aSampleCount;
    uint64_t aByteCount;
    uint64_t aDecodedByteCount;
    int aBitstream;
    uint32_t aResult = PVZ_TREMOR_INVALID_DATA;

    if (theSound == NULL)
        return PVZ_TREMOR_INVALID_DATA;
    memset(theSound, 0, sizeof(*theSound));
    if (theEncodedBytes == NULL ||
        theEncodedByteCount == 0 ||
        theEncodedByteCount > SIZE_MAX)
    {
        return PVZ_TREMOR_INVALID_DATA;
    }

    aSource.mBytes = theEncodedBytes;
    aSource.mSize = theEncodedByteCount;
    aSource.mOffset = 0;
    memset(&aFile, 0, sizeof(aFile));
    aCallbacks.read_func = ReadMemory;
    aCallbacks.seek_func = SeekMemory;
    aCallbacks.close_func = CloseMemory;
    aCallbacks.tell_func = TellMemory;
    if (ov_open_callbacks(
            &aSource,
            &aFile,
            NULL,
            0,
            aCallbacks) != 0)
    {
        return PVZ_TREMOR_INVALID_DATA;
    }

    if (ov_streams(&aFile) != 1)
        goto cleanup;
    anInfo = ov_info(&aFile, -1);
    if (anInfo == NULL)
        goto cleanup;
    if (anInfo->channels != 1 && anInfo->channels != 2)
    {
        aResult = PVZ_TREMOR_CHANNELS_UNSUPPORTED;
        goto cleanup;
    }
    if (anInfo->rate <= 0 ||
        (uint64_t)anInfo->rate > UINT32_MAX)
    {
        aResult = PVZ_TREMOR_SAMPLE_RATE_UNSUPPORTED;
        goto cleanup;
    }

    aFrameCount = ov_pcm_total(&aFile, -1);
    if (aFrameCount <= 0)
        goto cleanup;
    if ((uint64_t)aFrameCount >
        UINT64_MAX / (uint32_t)anInfo->channels)
    {
        aResult = PVZ_TREMOR_SIZE_OVERFLOW;
        goto cleanup;
    }
    aSampleCount =
        (uint64_t)aFrameCount * (uint32_t)anInfo->channels;
    if (aSampleCount > UINT64_MAX / sizeof(int16_t))
    {
        aResult = PVZ_TREMOR_SIZE_OVERFLOW;
        goto cleanup;
    }
    aByteCount = aSampleCount * sizeof(int16_t);
    if (aByteCount > SIZE_MAX)
    {
        aResult = PVZ_TREMOR_SIZE_OVERFLOW;
        goto cleanup;
    }

    theSound->mInterleavedSamples =
        (int16_t*)malloc((size_t)aByteCount);
    if (theSound->mInterleavedSamples == NULL)
    {
        aResult = PVZ_TREMOR_ALLOCATION_FAILED;
        goto cleanup;
    }

    aDecodedByteCount = 0;
    aBitstream = 0;
    while (aDecodedByteCount < aByteCount)
    {
        const uint64_t aRemaining =
            aByteCount - aDecodedByteCount;
        const int aRequest =
            aRemaining > 32768 ? 32768 : (int)aRemaining;
        const long aDecoded = ov_read(
            &aFile,
            (char*)theSound->mInterleavedSamples +
                (size_t)aDecodedByteCount,
            aRequest,
            &aBitstream);
        if (aDecoded <= 0)
            break;
        aDecodedByteCount += (uint64_t)aDecoded;
    }
    if (aDecodedByteCount != aByteCount)
        goto cleanup;

    theSound->mSampleRate = (uint32_t)anInfo->rate;
    theSound->mChannelCount = (uint32_t)anInfo->channels;
    theSound->mFrameCount = (uint64_t)aFrameCount;
    aResult = PVZ_TREMOR_OK;

cleanup:
    ov_clear(&aFile);
    if (aResult != PVZ_TREMOR_OK)
        pvz_tremor_release(theSound);
    return aResult;
}

void pvz_tremor_release(PvzTremorDecodedSound* theSound)
{
    if (theSound == NULL)
        return;
    free(theSound->mInterleavedSamples);
    memset(theSound, 0, sizeof(*theSound));
}
