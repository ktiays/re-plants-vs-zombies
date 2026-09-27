#ifndef __SEXY_OGGDECODER_H__
#define __SEXY_OGGDECODER_H__

#include "AudioMixer.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace Sexy
{

// Decodes a complete Ogg Vorbis file held in memory to 16-bit PCM.
bool DecodeOggVorbis(const unsigned char* theData, size_t theSize, PCMSample& theSample);

// Incremental decoder for streamed music. Owns a copy of the encoded data.
class OggStream
{
public:
	OggStream();
	~OggStream();

	bool				Open(std::vector<unsigned char> theData);
	int					GetChannels() const { return mChannels; }
	int					GetSampleRate() const { return mSampleRate; }

	// Decodes up to theFrames frames of interleaved 16-bit PCM. Returns the
	// number of frames produced; zero at the end of the stream.
	int					Read(std::int16_t* theBuffer, int theFrames);
	bool				SeekToFrame(std::int64_t theFrame);
	std::int64_t		TellFrame();

private:
	struct Impl;
	std::unique_ptr<Impl> mImpl;
	int					mChannels = 0;
	int					mSampleRate = 0;
};

}

#endif //__SEXY_OGGDECODER_H__
