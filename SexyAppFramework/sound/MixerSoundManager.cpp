#include "MixerSoundManager.h"
#include "MixerSoundInstance.h"
#include "OggDecoder.h"
#include "paklib/PakInterface.h"

#include <cmath>
#include <cstring>

using namespace Sexy;

MixerSoundManager::MixerSoundManager(AudioMixer* theMixer)
{
	mMixer = theMixer;
	mLastReleaseTick = 0;
	mMasterVolume = 1.0;

	for (int i = 0; i < MAX_SOURCE_SOUNDS; i++)
	{
		mBaseVolumes[i] = 1;
		mBasePans[i] = 0;
	}

	for (int i = 0; i < MAX_CHANNELS; i++)
		mPlayingSounds[i] = NULL;
}

MixerSoundManager::~MixerSoundManager()
{
	ReleaseChannels();
	ReleaseSounds();
}

int MixerSoundManager::VolumeToDB(double theVolume)
{
	int aVol = (int) ((log10(1 + theVolume*9) - 1.0) * 2333);
	if (aVol < -2000)
		aVol = -10000;

	return aVol;
}

bool MixerSoundManager::Initialized()
{
	return mMixer != NULL;
}

int MixerSoundManager::FindFreeChannel()
{
	DWORD aTick = GetTickCount();
	if (aTick-mLastReleaseTick > 1000)
	{
		ReleaseFreeChannels();
		mLastReleaseTick = aTick;
	}

	for (int i = 0; i < MAX_CHANNELS; i++)
	{
		if (mPlayingSounds[i] == NULL)
			return i;

		if (mPlayingSounds[i]->IsReleased())
		{
			delete mPlayingSounds[i];
			mPlayingSounds[i] = NULL;
			return i;
		}
	}

	return -1;
}

void MixerSoundManager::ReleaseFreeChannels()
{
	for (int i = 0; i < MAX_CHANNELS; i++)
	{
		if (mPlayingSounds[i] != NULL && mPlayingSounds[i]->IsReleased())
		{
			delete mPlayingSounds[i];
			mPlayingSounds[i] = NULL;
		}
	}
}

void MixerSoundManager::SetVolume(double theVolume)
{
	mMasterVolume = theVolume;

	for (int i = 0; i < MAX_CHANNELS; i++)
		if (mPlayingSounds[i] != NULL)
			mPlayingSounds[i]->RehupVolume();
}

bool MixerSoundManager::LoadOGGSound(unsigned int theSfxID, const std::string& theFilename)
{
	std::vector<unsigned char> aData;
	if (!PakReadFile(theFilename, aData))
		return false;

	std::shared_ptr<PCMSample> aSample = std::make_shared<PCMSample>();
	if (!DecodeOggVorbis(aData.data(), aData.size(), *aSample))
		return false;

	mSourceSounds[theSfxID] = aSample;
	return true;
}

bool MixerSoundManager::LoadAUSound(unsigned int theSfxID, const std::string& theFilename)
{
	std::vector<unsigned char> aData;
	if (!PakReadFile(theFilename, aData) || aData.size() < 24)
		return false;

	if (memcmp(aData.data(), ".snd", 4) != 0)
		return false;

	auto ReadBigEndian = [&aData](size_t theOffset) -> std::uint32_t
	{
		return ((std::uint32_t)aData[theOffset] << 24) | ((std::uint32_t)aData[theOffset + 1] << 16) |
			((std::uint32_t)aData[theOffset + 2] << 8) | (std::uint32_t)aData[theOffset + 3];
	};

	std::uint32_t aHeaderSize = ReadBigEndian(4);
	std::uint32_t aDataSize = ReadBigEndian(8);
	std::uint32_t anEncoding = ReadBigEndian(12);
	std::uint32_t aSampleRate = ReadBigEndian(16);
	std::uint32_t aChannelCount = ReadBigEndian(20);

	if (aHeaderSize > aData.size() || aChannelCount < 1 || aChannelCount > 2 || aSampleRate == 0)
		return false;
	if (aDataSize == 0xFFFFFFFF || aHeaderSize + aDataSize > aData.size())
		aDataSize = (std::uint32_t)(aData.size() - aHeaderSize);

	const unsigned char* aSrc = aData.data() + aHeaderSize;
	std::shared_ptr<PCMSample> aSample = std::make_shared<PCMSample>();
	aSample->mChannels = (int)aChannelCount;
	aSample->mSampleRate = (int)aSampleRate;
	aSample->mData.resize(aDataSize);

	switch (anEncoding)
	{
	case 1: // 8-bit mu-law, decoded exactly as the DirectSound loader did
		for (std::uint32_t i = 0; i < aDataSize; i++)
		{
			int ch = aSrc[i];

			int sign = (ch < 128) ? -1 : 1;
			ch = ch | 0x80;
			if (ch > 239)
				ch = ((0xF0 | 15) - ch) * 2;
			else if (ch > 223)
				ch = (((0xE0 | 15) - ch) * 4) + 32;
			else if (ch > 207)
				ch = (((0xD0 | 15) - ch) * 8) + 96;
			else if (ch > 191)
				ch = (((0xC0 | 15) - ch) * 16) + 224;
			else if (ch > 175)
				ch = (((0xB0 | 15) - ch) * 32) + 480;
			else if (ch > 159)
				ch = (((0xA0 | 15) - ch) * 64) + 992;
			else if (ch > 143)
				ch = (((0x90 | 15) - ch) * 128) + 2016;
			else if (ch > 128)
				ch = (((0x80 | 15) - ch) * 256) + 4064;
			else
				ch = 0xff;

			aSample->mData[i] = (std::int16_t)(sign * ch * 4);
		}
		break;

	case 2: // 8-bit signed linear PCM
		for (std::uint32_t i = 0; i < aDataSize; i++)
			aSample->mData[i] = (std::int16_t)((std::int8_t)aSrc[i] * 256);
		break;

	default:
		return false;
	}

	aSample->mFrames = (int)(aDataSize / aChannelCount);
	mSourceSounds[theSfxID] = aSample;
	return true;
}

bool MixerSoundManager::LoadWAVSound(unsigned int theSfxID, const std::string& theFilename)
{
	std::vector<unsigned char> aData;
	if (!PakReadFile(theFilename, aData) || aData.size() < 12)
		return false;

	if (memcmp(aData.data(), "RIFF", 4) != 0 || memcmp(aData.data() + 8, "WAVE", 4) != 0)
		return false;

	auto ReadU32 = [&aData](size_t theOffset) -> std::uint32_t
	{
		return (std::uint32_t)aData[theOffset] | ((std::uint32_t)aData[theOffset + 1] << 8) |
			((std::uint32_t)aData[theOffset + 2] << 16) | ((std::uint32_t)aData[theOffset + 3] << 24);
	};
	auto ReadU16 = [&aData](size_t theOffset) -> std::uint16_t
	{
		return (std::uint16_t)(aData[theOffset] | (aData[theOffset + 1] << 8));
	};

	int aChannels = 0, aSampleRate = 0, aBitsPerSample = 0, aFormat = 0;
	size_t aPos = 12;
	while (aPos + 8 <= aData.size())
	{
		std::uint32_t aChunkSize = ReadU32(aPos + 4);
		size_t aBody = aPos + 8;
		if (aBody + aChunkSize > aData.size())
			aChunkSize = (std::uint32_t)(aData.size() - aBody);

		if (memcmp(aData.data() + aPos, "fmt ", 4) == 0 && aChunkSize >= 16)
		{
			aFormat = ReadU16(aBody);
			aChannels = ReadU16(aBody + 2);
			aSampleRate = (int)ReadU32(aBody + 4);
			aBitsPerSample = ReadU16(aBody + 14);
		}
		else if (memcmp(aData.data() + aPos, "data", 4) == 0)
		{
			if (aFormat != 1 || aChannels < 1 || aChannels > 2 || aSampleRate <= 0)
				return false;

			std::shared_ptr<PCMSample> aSample = std::make_shared<PCMSample>();
			aSample->mChannels = aChannels;
			aSample->mSampleRate = aSampleRate;

			if (aBitsPerSample == 16)
			{
				size_t aCount = aChunkSize / 2;
				aSample->mData.resize(aCount);
				for (size_t i = 0; i < aCount; i++)
					aSample->mData[i] = (std::int16_t)ReadU16(aBody + i * 2);
			}
			else if (aBitsPerSample == 8)
			{
				aSample->mData.resize(aChunkSize);
				for (size_t i = 0; i < aChunkSize; i++)
					aSample->mData[i] = (std::int16_t)(((int)aData[aBody + i] - 128) * 256);
			}
			else
			{
				return false;
			}

			aSample->mFrames = (int)(aSample->mData.size() / aChannels);
			mSourceSounds[theSfxID] = aSample;
			return true;
		}

		aPos = aBody + ((aChunkSize + 1) & ~1u);
	}

	return false;
}

bool MixerSoundManager::LoadSound(unsigned int theSfxID, const std::string& theFilename)
{
	if (theSfxID >= MAX_SOURCE_SOUNDS)
		return false;

	ReleaseSound(theSfxID);

	mSourceFileNames[theSfxID] = theFilename;

	if (LoadWAVSound(theSfxID, theFilename + ".wav"))
		return true;

	if (LoadOGGSound(theSfxID, theFilename + ".ogg"))
		return true;

	if (LoadAUSound(theSfxID, theFilename + ".au"))
		return true;

	return false;
}

int MixerSoundManager::LoadSound(const std::string& theFilename)
{
	int i;
	for (i = 0; i < MAX_SOURCE_SOUNDS; i++)
		if (mSourceFileNames[i] == theFilename)
			return i;

	for (i = MAX_SOURCE_SOUNDS-1; i >= 0; i--)
	{
		if (mSourceSounds[i] == NULL)
		{
			if (!LoadSound(i, theFilename))
				return -1;
			else
				return i;
		}
	}

	return -1;
}

void MixerSoundManager::ReleaseSound(unsigned int theSfxID)
{
	if (theSfxID >= MAX_SOURCE_SOUNDS)
		return;

	mSourceSounds[theSfxID].reset();
	mSourceFileNames[theSfxID] = "";
}

int MixerSoundManager::GetFreeSoundId()
{
	for (int i=0; i<MAX_SOURCE_SOUNDS; i++)
	{
		if (mSourceSounds[i]==NULL)
			return i;
	}

	return -1;
}

int MixerSoundManager::GetNumSounds()
{
	int aCount = 0;
	for (int i=0; i<MAX_SOURCE_SOUNDS; i++)
	{
		if (mSourceSounds[i]!=NULL)
			aCount++;
	}

	return aCount;
}

bool MixerSoundManager::SetBaseVolume(unsigned int theSfxID, double theBaseVolume)
{
	if (theSfxID >= MAX_SOURCE_SOUNDS)
		return false;

	mBaseVolumes[theSfxID] = theBaseVolume;
	return true;
}

bool MixerSoundManager::SetBasePan(unsigned int theSfxID, int theBasePan)
{
	if (theSfxID >= MAX_SOURCE_SOUNDS)
		return false;

	mBasePans[theSfxID] = theBasePan;
	return true;
}

SoundInstance* MixerSoundManager::GetSoundInstance(unsigned int theSfxID)
{
	if (theSfxID >= MAX_SOURCE_SOUNDS)
		return NULL;

	int aFreeChannel = FindFreeChannel();
	if (aFreeChannel < 0)
		return NULL;

	if (mSourceSounds[theSfxID] == NULL)
		return NULL;

	mPlayingSounds[aFreeChannel] = new MixerSoundInstance(this, mSourceSounds[theSfxID]);
	mPlayingSounds[aFreeChannel]->SetBasePan(mBasePans[theSfxID]);
	mPlayingSounds[aFreeChannel]->SetBaseVolume(mBaseVolumes[theSfxID]);

	return mPlayingSounds[aFreeChannel];
}

void MixerSoundManager::ReleaseSounds()
{
	for (int i = 0; i < MAX_SOURCE_SOUNDS; i++)
		ReleaseSound(i);
}

void MixerSoundManager::ReleaseChannels()
{
	for (int i = 0; i < MAX_CHANNELS; i++)
	{
		if (mPlayingSounds[i] != NULL)
		{
			delete mPlayingSounds[i];
			mPlayingSounds[i] = NULL;
		}
	}
}

double MixerSoundManager::GetMasterVolume()
{
	return 1.0;
}

void MixerSoundManager::SetMasterVolume(double theVolume)
{
	// The DirectSound version changed the operating system's wave-out
	// volume; the portable build leaves system volume alone.
	(void)theVolume;
}

void MixerSoundManager::Flush()
{
}

void MixerSoundManager::StopAllSounds()
{
	for (int i = 0; i < MAX_CHANNELS; i++)
	{
		if (mPlayingSounds[i] != NULL)
		{
			bool isAutoRelease = mPlayingSounds[i]->mAutoRelease;
			mPlayingSounds[i]->Stop();
			mPlayingSounds[i]->mAutoRelease = isAutoRelease;
		}
	}
}
