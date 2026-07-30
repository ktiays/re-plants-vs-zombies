#pragma once

#include <stdint.h>

/*
 * Tremor's historical API uses native `long`. This is the only adapter
 * allowed to cross that API: every input, output, count, and PCM sample in
 * this header has an explicit width.
 */

#ifdef __cplusplus
extern "C"
{
#endif

enum
{
    PVZ_TREMOR_OK = 0,
    PVZ_TREMOR_INVALID_DATA = 1,
    PVZ_TREMOR_CHANNELS_UNSUPPORTED = 2,
    PVZ_TREMOR_SAMPLE_RATE_UNSUPPORTED = 3,
    PVZ_TREMOR_SIZE_OVERFLOW = 4,
    PVZ_TREMOR_ALLOCATION_FAILED = 5,
};

typedef struct PvzTremorDecodedSound
{
    uint32_t mSampleRate;
    uint32_t mChannelCount;
    uint64_t mFrameCount;
    int16_t* mInterleavedSamples;
} PvzTremorDecodedSound;

uint32_t pvz_tremor_decode(
    const uint8_t* theEncodedBytes,
    uint64_t theEncodedByteCount,
    PvzTremorDecodedSound* theSound);

void pvz_tremor_release(PvzTremorDecodedSound* theSound);

#ifdef __cplusplus
}
#endif
