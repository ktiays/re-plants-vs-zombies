#ifndef __SEXY_AUDIOMIXER_H__
#define __SEXY_AUDIOMIXER_H__

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace Sexy
{

// Decoded 16-bit PCM, interleaved when stereo.
struct PCMSample
{
	std::vector<std::int16_t>	mData;
	int							mChannels = 1;
	int							mSampleRate = 44100;
	int							mFrames = 0;
};

// A voice plays one PCMSample. Its fields are owned by the mixer thread while
// the voice is active; change them only while holding AudioMixer::GetLock().
class MixerVoice
{
public:
	std::shared_ptr<const PCMSample>	mSample;
	double						mPosition = 0.0;	// in sample frames
	double						mRate = 1.0;		// playback speed multiplier
	float						mGainLeft = 1.0f;
	float						mGainRight = 1.0f;
	bool						mLooping = false;
	bool						mActive = false;	// registered with the mixer
	bool						mFinished = false;	// reached the end of a one-shot sample

	virtual ~MixerVoice() {}
};

// Something that renders into the mix directly, such as streamed music.
class MixerSource
{
public:
	virtual ~MixerSource() {}
	// Adds theFrames of interleaved stereo float samples into theBuffer.
	virtual void				MixInto(float* theBuffer, int theFrames, int theSampleRate) = 0;
};

// Platform-neutral software mixer. The platform audio device pulls
// interleaved stereo float samples from Mix() on its own thread.
class AudioMixer
{
public:
	AudioMixer(int theSampleRate);
	~AudioMixer();

	int							GetSampleRate() const { return mSampleRate; }
	std::recursive_mutex&		GetLock() { return mLock; }

	void						StartVoice(MixerVoice* theVoice);
	void						StopVoice(MixerVoice* theVoice);

	void						AddSource(MixerSource* theSource);
	void						RemoveSource(MixerSource* theSource);

	void						Mix(float* theBuffer, int theFrames);

	// Converts a DirectSound volume (hundredths of a decibel) to a linear gain.
	static float				DecibelsToGain(int theHundredthsDB);

private:
	int							mSampleRate;
	std::recursive_mutex		mLock;
	std::vector<MixerVoice*>	mVoices;
	std::vector<MixerSource*>	mSources;
};

// The platform's audio output. Owns the connection to the OS audio API and
// feeds it from an AudioMixer.
class AudioDevice
{
public:
	virtual ~AudioDevice() {}
	virtual bool				Start(AudioMixer* theMixer) = 0;
	virtual void				Pause(bool thePause) = 0;
};

AudioDevice*					CreatePlatformAudioDevice();

}

#endif //__SEXY_AUDIOMIXER_H__
