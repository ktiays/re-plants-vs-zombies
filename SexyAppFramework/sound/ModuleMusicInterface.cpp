#include "ModuleMusicInterface.h"
#include "OggDecoder.h"
#include "paklib/PakInterface.h"

#include <libopenmpt/libopenmpt.hpp>
#include <libopenmpt/libopenmpt_ext.hpp>

#include <cstdio>

using namespace Sexy;

ModuleMusicInterface::Track::Track()
{
}

ModuleMusicInterface::Track::~Track()
{
}

ModuleMusicInterface::ModuleMusicInterface(AudioMixer* theMixer)
{
	mMixer = theMixer;
	mMasterVolume = 1.0;
	mMixer->AddSource(this);
}

ModuleMusicInterface::~ModuleMusicInterface()
{
	mMixer->RemoveSource(this);

	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	mTracks.clear();
}

ModuleMusicInterface::Track* ModuleMusicInterface::FindTrack(int theSongId)
{
	auto anItr = mTracks.find(theSongId);
	return anItr != mTracks.end() ? anItr->second.get() : nullptr;
}

bool ModuleMusicInterface::LoadMusic(int theSongId, const std::string& theFileName)
{
	std::vector<unsigned char> aData;
	if (!PakReadFile(theFileName, aData))
		return false;

	std::unique_ptr<Track> aTrack(new Track());

	std::string anExt;
	size_t aDotPos = theFileName.find_last_of('.');
	if (aDotPos != std::string::npos)
		anExt = StringToLower(theFileName.substr(aDotPos + 1));

	if (anExt == "ogg")
	{
		aTrack->mStream.reset(new OggStream());
		if (!aTrack->mStream->Open(std::move(aData)))
			return false;
	}
	else
	{
		try
		{
			aTrack->mModule.reset(new openmpt::module_ext(aData.data(), aData.size()));
			aTrack->mModule->set_render_param(openmpt::module::RENDER_INTERPOLATIONFILTER_LENGTH, 2);
			aTrack->mModule->set_repeat_count(-1);
			aTrack->mInteractive = static_cast<openmpt::ext::interactive*>(aTrack->mModule->get_interface(openmpt::ext::interactive_id));
			aTrack->mInteractive3 = static_cast<openmpt::ext::interactive3*>(aTrack->mModule->get_interface(openmpt::ext::interactive3_id));
		}
		catch (const std::exception& anException)
		{
			std::fprintf(stderr, "Unable to load music '%s': %s\n", theFileName.c_str(), anException.what());
			return false;
		}
	}

	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	mTracks[theSongId] = std::move(aTrack);
	return true;
}

void ModuleMusicInterface::StartTrack(Track* theTrack, int theOffset, bool noLoop)
{
	theTrack->mVolume = theTrack->mVolumeCap;
	theTrack->mVolumeAdd = 0.0;
	theTrack->mStopOnFade = noLoop;
	theTrack->mLoop = !noLoop;
	theTrack->mPaused = false;

	if (theTrack->mModule != nullptr)
	{
		theTrack->mModule->set_repeat_count(noLoop ? 0 : -1);
		theTrack->mModule->set_position_order_row(std::max(theOffset, 0), 0);
	}
	else if (theTrack->mStream != nullptr)
	{
		if (theOffset >= 0)
		{
			int aBytesPerFrame = std::max(theTrack->mStream->GetChannels(), 1) * 2;
			theTrack->mStream->SeekToFrame(theOffset / aBytesPerFrame);
			theTrack->mStreamBufferFrames = 0;
			theTrack->mStreamBufferPos = 0;
			theTrack->mStreamPosition = 0.0;
		}
	}

	theTrack->mPlaying = true;
}

void ModuleMusicInterface::PlayMusic(int theSongId, int theOffset, bool noLoop)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack != nullptr)
		StartTrack(aTrack, theOffset, noLoop);
}

void ModuleMusicInterface::StopMusic(int theSongId)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack != nullptr)
	{
		aTrack->mVolume = 0.0;
		aTrack->mPlaying = false;
		aTrack->mPaused = false;
	}
}

void ModuleMusicInterface::StopAllMusic()
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	for (auto& aPair : mTracks)
	{
		aPair.second->mVolume = 0.0;
		aPair.second->mPlaying = false;
		aPair.second->mPaused = false;
	}
}

void ModuleMusicInterface::UnloadMusic(int theSongId)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	mTracks.erase(theSongId);
}

void ModuleMusicInterface::UnloadAllMusic()
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	mTracks.clear();
}

void ModuleMusicInterface::PauseMusic(int theSongId)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack != nullptr && aTrack->mPlaying)
		aTrack->mPaused = true;
}

void ModuleMusicInterface::ResumeMusic(int theSongId)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack != nullptr)
		aTrack->mPaused = false;
}

void ModuleMusicInterface::PauseAllMusic()
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	for (auto& aPair : mTracks)
	{
		if (aPair.second->mPlaying)
			aPair.second->mPaused = true;
	}
}

void ModuleMusicInterface::ResumeAllMusic()
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	for (auto& aPair : mTracks)
		aPair.second->mPaused = false;
}

void ModuleMusicInterface::FadeIn(int theSongId, int theOffset, double theSpeed, bool noLoop)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack == nullptr)
		return;

	double aVolume = aTrack->mVolume;
	StartTrack(aTrack, theOffset, noLoop);
	aTrack->mVolume = aVolume;
	aTrack->mVolumeAdd = theSpeed;
}

void ModuleMusicInterface::FadeOut(int theSongId, bool stopSong, double theSpeed)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack == nullptr)
		return;

	if (aTrack->mVolume != 0.0)
		aTrack->mVolumeAdd = -theSpeed;
	aTrack->mStopOnFade = stopSong;
}

void ModuleMusicInterface::FadeOutAll(bool stopSong, double theSpeed)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	for (auto& aPair : mTracks)
	{
		aPair.second->mVolumeAdd = -theSpeed;
		aPair.second->mStopOnFade = stopSong;
	}
}

void ModuleMusicInterface::SetVolume(double theVolume)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	mMasterVolume = theVolume;
}

void ModuleMusicInterface::SetSongVolume(int theSongId, double theVolume)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack != nullptr)
		aTrack->mVolume = theVolume;
}

void ModuleMusicInterface::SetSongMaxVolume(int theSongId, double theMaxVolume)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack != nullptr)
	{
		aTrack->mVolumeCap = theMaxVolume;
		aTrack->mVolume = std::min(aTrack->mVolume, theMaxVolume);
	}
}

bool ModuleMusicInterface::IsPlaying(int theSongId)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	return aTrack != nullptr && aTrack->mPlaying && !aTrack->mPaused;
}

void ModuleMusicInterface::SetMusicAmplify(int theSongId, double theAmp)
{
	(void)theSongId;
	(void)theAmp;
}

void ModuleMusicInterface::Update()
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	for (auto& aPair : mTracks)
	{
		Track* aTrack = aPair.second.get();
		if (aTrack->mVolumeAdd != 0.0)
		{
			aTrack->mVolume += aTrack->mVolumeAdd;
			if (aTrack->mVolume > aTrack->mVolumeCap)
			{
				aTrack->mVolume = aTrack->mVolumeCap;
				aTrack->mVolumeAdd = 0.0;
			}
			else if (aTrack->mVolume < 0.0)
			{
				aTrack->mVolume = 0.0;
				aTrack->mVolumeAdd = 0.0;

				if (aTrack->mStopOnFade)
					aTrack->mPlaying = false;
			}
		}
	}
}

bool ModuleMusicInterface::IsMusicLoaded(int theSongId)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	return FindTrack(theSongId) != nullptr;
}

bool ModuleMusicInterface::IsModule(int theSongId)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	return aTrack != nullptr && aTrack->mModule != nullptr;
}

void ModuleMusicInterface::PlayModule(int theSongId, int theOrder, double theVolume, bool theLoop)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack == nullptr)
		return;

	StartTrack(aTrack, theOrder, !theLoop);
	aTrack->mStopOnFade = false;
	aTrack->mVolume = aTrack->mVolumeCap * theVolume;
}

void ModuleMusicInterface::SetModuleOrder(int theSongId, int theOrder)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack != nullptr && aTrack->mModule != nullptr)
		aTrack->mModule->set_position_order_row(theOrder, 0);
}

int ModuleMusicInterface::GetMusicOrder(int theSongId)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack == nullptr)
		return -1;

	if (aTrack->mModule != nullptr)
	{
		int anOrder = aTrack->mModule->get_current_order();
		int aRow = aTrack->mModule->get_current_row();
		return (int)MAKELONG(anOrder, aRow * 4);
	}

	if (aTrack->mStream != nullptr)
		return (int)(aTrack->mStream->TellFrame() * aTrack->mStream->GetChannels() * 2);

	return -1;
}

void ModuleMusicInterface::SetModuleChannelVolume(int theSongId, int theChannel, double theVolume)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack != nullptr && aTrack->mInteractive != nullptr && theChannel < aTrack->mModule->get_num_channels())
		aTrack->mInteractive->set_channel_volume(theChannel, theVolume);
}

void ModuleMusicInterface::SetModuleTempo(int theSongId, float theBPM)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack == nullptr || aTrack->mModule == nullptr)
		return;

	try
	{
		if (aTrack->mInteractive3 != nullptr)
			aTrack->mInteractive3->set_current_tempo2(theBPM);
	}
	catch (const std::exception&)
	{
	}
}

float ModuleMusicInterface::GetModuleTempo(int theSongId)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack == nullptr || aTrack->mModule == nullptr)
		return 0;

	return (float)aTrack->mModule->get_current_tempo2();
}

void ModuleMusicInterface::SetModuleSpeed(int theSongId, float theSpeed)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack == nullptr || aTrack->mInteractive == nullptr)
		return;

	try
	{
		aTrack->mInteractive->set_current_speed((std::int32_t)theSpeed);
	}
	catch (const std::exception&)
	{
	}
}

float ModuleMusicInterface::GetModuleSpeed(int theSongId)
{
	std::lock_guard<std::recursive_mutex> aLock(mMixer->GetLock());
	Track* aTrack = FindTrack(theSongId);
	if (aTrack == nullptr || aTrack->mModule == nullptr)
		return 0;

	return (float)aTrack->mModule->get_current_speed();
}

void ModuleMusicInterface::MixStream(Track* theTrack, float* theBuffer, int theFrames, int theSampleRate, float theGain)
{
	OggStream* aStream = theTrack->mStream.get();
	int aChannels = aStream->GetChannels();
	double aStep = (double)aStream->GetSampleRate() / theSampleRate;

	for (int i = 0; i < theFrames; i++)
	{
		// Interpolation needs the current and next frame; refill when the
		// next frame is not decoded yet. Unconsumed frames move to the front.
		while (theTrack->mStreamBufferPos + 1 >= theTrack->mStreamBufferFrames)
		{
			int aKeep = std::max(theTrack->mStreamBufferFrames - theTrack->mStreamBufferPos, 0);
			if (aKeep > 0)
				memmove(theTrack->mStreamBuffer.data(), theTrack->mStreamBuffer.data() + (size_t)theTrack->mStreamBufferPos * aChannels, (size_t)aKeep * aChannels * sizeof(std::int16_t));

			const int aChunkFrames = 4096;
			theTrack->mStreamBuffer.resize((size_t)(aKeep + aChunkFrames) * aChannels);
			theTrack->mStreamBufferPos = 0;
			theTrack->mStreamBufferFrames = aKeep;

			int aRead = aStream->Read(theTrack->mStreamBuffer.data() + (size_t)aKeep * aChannels, aChunkFrames);
			if (aRead <= 0 && theTrack->mLoop)
			{
				aStream->SeekToFrame(0);
				aRead = aStream->Read(theTrack->mStreamBuffer.data() + (size_t)aKeep * aChannels, aChunkFrames);
			}

			if (aRead <= 0)
			{
				theTrack->mPlaying = false;
				theTrack->mStreamBufferFrames = 0;
				theTrack->mStreamBufferPos = 0;
				theTrack->mStreamPosition = 0.0;
				return;
			}

			theTrack->mStreamBufferFrames = aKeep + aRead;
		}

		const std::int16_t* aFrame = &theTrack->mStreamBuffer[(size_t)theTrack->mStreamBufferPos * aChannels];
		const std::int16_t* aNext = aFrame + aChannels;
		float aFrac = (float)theTrack->mStreamPosition;
		float aLeft = (aFrame[0] + (aNext[0] - aFrame[0]) * aFrac) * (1.0f / 32768.0f);
		float aRight = aChannels == 2 ? (aFrame[1] + (aNext[1] - aFrame[1]) * aFrac) * (1.0f / 32768.0f) : aLeft;
		theBuffer[i * 2] += aLeft * theGain;
		theBuffer[i * 2 + 1] += aRight * theGain;

		theTrack->mStreamPosition += aStep;
		while (theTrack->mStreamPosition >= 1.0)
		{
			theTrack->mStreamPosition -= 1.0;
			theTrack->mStreamBufferPos++;
		}
	}
}

void ModuleMusicInterface::MixInto(float* theBuffer, int theFrames, int theSampleRate)
{
	if ((int)mRenderBuffer.size() < theFrames * 2)
		mRenderBuffer.resize((size_t)theFrames * 2);

	for (auto& aPair : mTracks)
	{
		Track* aTrack = aPair.second.get();
		if (!aTrack->mPlaying || aTrack->mPaused)
			continue;

		float aGain = (float)(aTrack->mVolume * mMasterVolume);

		if (aTrack->mModule != nullptr)
		{
			size_t aRendered = 0;
			try
			{
				aRendered = aTrack->mModule->read_interleaved_stereo(theSampleRate, (size_t)theFrames, mRenderBuffer.data());
			}
			catch (const std::exception&)
			{
				aRendered = 0;
			}

			for (size_t i = 0; i < aRendered * 2; i++)
				theBuffer[i] += mRenderBuffer[i] * aGain;

			if (aRendered < (size_t)theFrames)
				aTrack->mPlaying = false;
		}
		else if (aTrack->mStream != nullptr)
		{
			MixStream(aTrack, theBuffer, theFrames, theSampleRate, aGain);
		}
	}
}
