#ifndef __SEXY_MIXERSOUNDMANAGER_H__
#define __SEXY_MIXERSOUNDMANAGER_H__

#include "SoundManager.h"
#include "AudioMixer.h"

#include <memory>

namespace Sexy
{

class MixerSoundInstance;

// Sound effects decoded to PCM and played through the software AudioMixer.
// Replaces the DirectSound manager; volume and pan keep DirectSound's
// logarithmic behavior so the game sounds the same.
class MixerSoundManager : public SoundManager
{
	friend class MixerSoundInstance;

protected:
	AudioMixer*						mMixer;
	std::shared_ptr<PCMSample>		mSourceSounds[MAX_SOURCE_SOUNDS];
	std::string						mSourceFileNames[MAX_SOURCE_SOUNDS];
	double							mBaseVolumes[MAX_SOURCE_SOUNDS];
	int								mBasePans[MAX_SOURCE_SOUNDS];
	MixerSoundInstance*				mPlayingSounds[MAX_CHANNELS];
	double							mMasterVolume;
	DWORD							mLastReleaseTick;

protected:
	int								FindFreeChannel();
	void							ReleaseFreeChannels();

	bool							LoadOGGSound(unsigned int theSfxID, const std::string& theFilename);
	bool							LoadAUSound(unsigned int theSfxID, const std::string& theFilename);
	bool							LoadWAVSound(unsigned int theSfxID, const std::string& theFilename);

public:
	MixerSoundManager(AudioMixer* theMixer);
	virtual ~MixerSoundManager();

	// DirectSound attenuation for a linear volume, in hundredths of a decibel.
	static int						VolumeToDB(double theVolume);

	virtual bool					Initialized();

	virtual bool					LoadSound(unsigned int theSfxID, const std::string& theFilename);
	virtual int						LoadSound(const std::string& theFilename);
	virtual void					ReleaseSound(unsigned int theSfxID);

	virtual void					SetVolume(double theVolume);
	virtual bool					SetBaseVolume(unsigned int theSfxID, double theBaseVolume);
	virtual bool					SetBasePan(unsigned int theSfxID, int theBasePan);

	virtual SoundInstance*			GetSoundInstance(unsigned int theSfxID);

	virtual void					ReleaseSounds();
	virtual void					ReleaseChannels();

	virtual double					GetMasterVolume();
	virtual void					SetMasterVolume(double theVolume);

	virtual void					Flush();
	virtual void					StopAllSounds();
	virtual int						GetFreeSoundId();
	virtual int						GetNumSounds();
};

}

#endif //__SEXY_MIXERSOUNDMANAGER_H__
