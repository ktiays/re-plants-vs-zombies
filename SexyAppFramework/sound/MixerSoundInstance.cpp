#include "MixerSoundInstance.h"
#include "MixerSoundManager.h"

#include <cmath>

using namespace Sexy;

MixerSoundInstance::MixerSoundInstance(MixerSoundManager* theSoundManager, std::shared_ptr<const PCMSample> theSample)
{
	mSoundManagerP = theSoundManager;
	mSample = std::move(theSample);
	mReleased = false;
	mAutoRelease = false;
	mHasPlayed = false;

	mBaseVolume = 1.0;
	mBasePan = 0;

	mVolume = 1.0;
	mPan = 0;
	mPitchMultiplier = 1.0;

	RehupVolume();
}

MixerSoundInstance::~MixerSoundInstance()
{
	mSoundManagerP->mMixer->StopVoice(this);
}

void MixerSoundInstance::RehupVolume()
{
	float aGain = AudioMixer::DecibelsToGain(MixerSoundManager::VolumeToDB(mBaseVolume * mVolume * mSoundManagerP->mMasterVolume));

	// DirectSound pan: hundredths of a decibel of attenuation applied to the
	// opposite channel.
	int aPan = mBasePan + mPan;
	float aLeft = aGain;
	float aRight = aGain;
	if (aPan < 0)
		aRight *= AudioMixer::DecibelsToGain(aPan);
	else if (aPan > 0)
		aLeft *= AudioMixer::DecibelsToGain(-aPan);

	std::lock_guard<std::recursive_mutex> aLock(mSoundManagerP->mMixer->GetLock());
	mGainLeft = aLeft;
	mGainRight = aRight;
}

void MixerSoundInstance::Release()
{
	Stop();
	mReleased = true;
}

void MixerSoundInstance::SetVolume(double theVolume)
{
	mVolume = theVolume;
	RehupVolume();
}

void MixerSoundInstance::SetPan(int thePosition)
{
	mPan = thePosition;
	RehupVolume();
}

void MixerSoundInstance::SetBaseVolume(double theBaseVolume)
{
	mBaseVolume = theBaseVolume;
	RehupVolume();
}

void MixerSoundInstance::SetBasePan(int theBasePan)
{
	mBasePan = theBasePan;
	RehupVolume();
}

void MixerSoundInstance::AdjustPitch(double theNumSteps)
{
	if (mSample == nullptr)
		return;

	// DirectSound clamps buffer frequencies to [100, 200000] Hz.
	double aFrequency = mSample->mSampleRate * std::pow(1.0594630943592952645618252949463, theNumSteps);
	aFrequency = std::clamp(aFrequency, 100.0, 200000.0);

	std::lock_guard<std::recursive_mutex> aLock(mSoundManagerP->mMixer->GetLock());
	mRate = aFrequency / mSample->mSampleRate;
}

bool MixerSoundInstance::Play(bool looping, bool autoRelease)
{
	Stop();

	mHasPlayed = true;
	mAutoRelease = autoRelease;

	if (mSample == nullptr)
		return false;

	{
		std::lock_guard<std::recursive_mutex> aLock(mSoundManagerP->mMixer->GetLock());
		mLooping = looping;
		mPosition = 0.0;
	}
	mSoundManagerP->mMixer->StartVoice(this);
	return true;
}

void MixerSoundInstance::Stop()
{
	if (mSample != nullptr)
	{
		mSoundManagerP->mMixer->StopVoice(this);
		std::lock_guard<std::recursive_mutex> aLock(mSoundManagerP->mMixer->GetLock());
		mPosition = 0.0;
		mAutoRelease = false;
	}
}

bool MixerSoundInstance::IsPlaying()
{
	if (!mHasPlayed || mSample == nullptr)
		return false;

	std::lock_guard<std::recursive_mutex> aLock(mSoundManagerP->mMixer->GetLock());
	return mActive;
}

bool MixerSoundInstance::IsReleased()
{
	if ((!mReleased) && (mAutoRelease) && (mHasPlayed) && (!IsPlaying()))
		Release();

	return mReleased;
}

double MixerSoundInstance::GetVolume()
{
	return mVolume;
}

int MixerSoundInstance::GetSoundPosition()
{
	if (mSample == nullptr)
		return 0;

	std::lock_guard<std::recursive_mutex> aLock(mSoundManagerP->mMixer->GetLock());
	return (int)mPosition * mSample->mChannels * 2;
}

void MixerSoundInstance::SetSoundPosition(int thePosition)
{
	if (mSample == nullptr)
		return;

	std::lock_guard<std::recursive_mutex> aLock(mSoundManagerP->mMixer->GetLock());
	int aFrame = thePosition / (mSample->mChannels * 2);
	mPosition = std::clamp(aFrame, 0, std::max(mSample->mFrames - 1, 0));
}

bool MixerSoundInstance::HasSoundData()
{
	return mSample != nullptr;
}
