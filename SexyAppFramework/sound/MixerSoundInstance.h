#ifndef __SEXY_MIXERSOUNDINSTANCE_H__
#define __SEXY_MIXERSOUNDINSTANCE_H__

#include "SoundInstance.h"
#include "AudioMixer.h"

#include <memory>

namespace Sexy
{

class MixerSoundManager;

class MixerSoundInstance : public SoundInstance, public MixerVoice
{
	friend class MixerSoundManager;

protected:
	MixerSoundManager*		mSoundManagerP;
	bool					mAutoRelease;
	bool					mHasPlayed;
	bool					mReleased;
	int						mBasePan;
	double					mBaseVolume;
	int						mPan;
	double					mVolume;
	double					mPitchMultiplier;

protected:
	void					RehupVolume();

public:
	MixerSoundInstance(MixerSoundManager* theSoundManager, std::shared_ptr<const PCMSample> theSample);
	virtual ~MixerSoundInstance();

	virtual void			Release();
	virtual void			SetBaseVolume(double theBaseVolume);
	virtual void			SetBasePan(int theBasePan);
	virtual void			AdjustPitch(double theNumSteps);
	virtual void			SetVolume(double theVolume);
	virtual void			SetPan(int thePosition);
	virtual bool			Play(bool looping, bool autoRelease);
	virtual void			Stop();
	virtual bool			IsPlaying();
	virtual bool			IsReleased();
	virtual double			GetVolume();

	virtual int				GetSoundPosition();
	virtual void			SetSoundPosition(int thePosition);
	virtual bool			HasSoundData();
};

}

#endif //__SEXY_MIXERSOUNDINSTANCE_H__
