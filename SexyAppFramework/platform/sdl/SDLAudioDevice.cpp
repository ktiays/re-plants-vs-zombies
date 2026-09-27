#include "sound/AudioMixer.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <vector>

using namespace Sexy;

namespace
{

class SDLAudioDevice : public AudioDevice
{
public:
	SDL_AudioStream*		mStream = nullptr;
	AudioMixer*				mMixer = nullptr;
	std::vector<float>		mScratch;

	~SDLAudioDevice() override
	{
		if (mStream != nullptr)
			SDL_DestroyAudioStream(mStream);
	}

	static void SDLCALL Callback(void* theUserData, SDL_AudioStream* theStream, int theAdditionalAmount, int theTotalAmount)
	{
		(void)theTotalAmount;
		SDLAudioDevice* aDevice = (SDLAudioDevice*)theUserData;
		int aFrames = theAdditionalAmount / (int)(sizeof(float) * 2);
		if (aFrames <= 0)
			return;

		if ((int)aDevice->mScratch.size() < aFrames * 2)
			aDevice->mScratch.resize((size_t)aFrames * 2);

		aDevice->mMixer->Mix(aDevice->mScratch.data(), aFrames);
		SDL_PutAudioStreamData(theStream, aDevice->mScratch.data(), aFrames * (int)(sizeof(float) * 2));
	}

	bool Start(AudioMixer* theMixer) override
	{
		mMixer = theMixer;

		if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO))
		{
			std::fprintf(stderr, "SDL audio init failed: %s\n", SDL_GetError());
			return false;
		}

		SDL_AudioSpec aSpec;
		aSpec.format = SDL_AUDIO_F32;
		aSpec.channels = 2;
		aSpec.freq = theMixer->GetSampleRate();

		mStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &aSpec, Callback, this);
		if (mStream == nullptr)
		{
			std::fprintf(stderr, "SDL audio device open failed: %s\n", SDL_GetError());
			return false;
		}

		SDL_ResumeAudioStreamDevice(mStream);
		return true;
	}

	void Pause(bool thePause) override
	{
		if (mStream == nullptr)
			return;

		if (thePause)
			SDL_PauseAudioStreamDevice(mStream);
		else
			SDL_ResumeAudioStreamDevice(mStream);
	}
};

}

AudioDevice* Sexy::CreatePlatformAudioDevice()
{
	return new SDLAudioDevice();
}
