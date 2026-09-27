#include "AudioMixer.h"

#include <algorithm>
#include <cmath>

using namespace Sexy;

AudioMixer::AudioMixer(int theSampleRate)
{
	mSampleRate = theSampleRate;
}

AudioMixer::~AudioMixer()
{
	std::lock_guard<std::recursive_mutex> aLock(mLock);
	for (MixerVoice* aVoice : mVoices)
		aVoice->mActive = false;
	mVoices.clear();
	mSources.clear();
}

float AudioMixer::DecibelsToGain(int theHundredthsDB)
{
	if (theHundredthsDB <= -10000)
		return 0.0f;
	if (theHundredthsDB >= 0)
		return 1.0f;
	return std::pow(10.0f, theHundredthsDB / 2000.0f);
}

void AudioMixer::StartVoice(MixerVoice* theVoice)
{
	std::lock_guard<std::recursive_mutex> aLock(mLock);
	theVoice->mFinished = false;
	if (!theVoice->mActive)
	{
		theVoice->mActive = true;
		mVoices.push_back(theVoice);
	}
}

void AudioMixer::StopVoice(MixerVoice* theVoice)
{
	std::lock_guard<std::recursive_mutex> aLock(mLock);
	if (theVoice->mActive)
	{
		theVoice->mActive = false;
		mVoices.erase(std::remove(mVoices.begin(), mVoices.end(), theVoice), mVoices.end());
	}
}

void AudioMixer::AddSource(MixerSource* theSource)
{
	std::lock_guard<std::recursive_mutex> aLock(mLock);
	if (std::find(mSources.begin(), mSources.end(), theSource) == mSources.end())
		mSources.push_back(theSource);
}

void AudioMixer::RemoveSource(MixerSource* theSource)
{
	std::lock_guard<std::recursive_mutex> aLock(mLock);
	mSources.erase(std::remove(mSources.begin(), mSources.end(), theSource), mSources.end());
}

static inline float SampleAt(const PCMSample& theSample, int theFrame, int theChannel)
{
	return theSample.mData[(size_t)theFrame * theSample.mChannels + theChannel] * (1.0f / 32768.0f);
}

void AudioMixer::Mix(float* theBuffer, int theFrames)
{
	std::fill(theBuffer, theBuffer + (size_t)theFrames * 2, 0.0f);

	std::lock_guard<std::recursive_mutex> aLock(mLock);

	for (size_t aVoiceIdx = 0; aVoiceIdx < mVoices.size(); )
	{
		MixerVoice* aVoice = mVoices[aVoiceIdx];
		const PCMSample* aSample = aVoice->mSample.get();
		if (aSample == nullptr || aSample->mFrames <= 0)
		{
			aVoice->mActive = false;
			aVoice->mFinished = true;
			mVoices.erase(mVoices.begin() + aVoiceIdx);
			continue;
		}

		const double aStep = aVoice->mRate * aSample->mSampleRate / mSampleRate;
		const int aLastFrame = aSample->mFrames - 1;
		const bool isStereo = aSample->mChannels == 2;
		double aPosition = aVoice->mPosition;
		bool isDone = false;

		for (int i = 0; i < theFrames; i++)
		{
			if (aPosition >= aSample->mFrames)
			{
				if (aVoice->mLooping)
				{
					aPosition = std::fmod(aPosition, (double)aSample->mFrames);
				}
				else
				{
					isDone = true;
					break;
				}
			}

			int aFrame = (int)aPosition;
			float aFrac = (float)(aPosition - aFrame);
			int aNextFrame = aFrame + 1;
			if (aNextFrame > aLastFrame)
				aNextFrame = aVoice->mLooping ? 0 : aLastFrame;

			float aLeft, aRight;
			if (isStereo)
			{
				aLeft = SampleAt(*aSample, aFrame, 0) + (SampleAt(*aSample, aNextFrame, 0) - SampleAt(*aSample, aFrame, 0)) * aFrac;
				aRight = SampleAt(*aSample, aFrame, 1) + (SampleAt(*aSample, aNextFrame, 1) - SampleAt(*aSample, aFrame, 1)) * aFrac;
			}
			else
			{
				aLeft = SampleAt(*aSample, aFrame, 0) + (SampleAt(*aSample, aNextFrame, 0) - SampleAt(*aSample, aFrame, 0)) * aFrac;
				aRight = aLeft;
			}

			theBuffer[i * 2] += aLeft * aVoice->mGainLeft;
			theBuffer[i * 2 + 1] += aRight * aVoice->mGainRight;
			aPosition += aStep;
		}

		aVoice->mPosition = aPosition;
		if (isDone)
		{
			aVoice->mActive = false;
			aVoice->mFinished = true;
			mVoices.erase(mVoices.begin() + aVoiceIdx);
			continue;
		}

		aVoiceIdx++;
	}

	for (MixerSource* aSource : mSources)
		aSource->MixInto(theBuffer, theFrames, mSampleRate);

	for (int i = 0; i < theFrames * 2; i++)
		theBuffer[i] = std::clamp(theBuffer[i], -1.0f, 1.0f);
}
