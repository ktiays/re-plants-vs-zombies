#include "OggDecoder.h"

#include "ivorbiscodec.h"
#include "ivorbisfile.h"

#include <algorithm>
#include <cstring>

using namespace Sexy;

namespace
{

struct MemoryFile
{
	const unsigned char*	mData = nullptr;
	size_t					mSize = 0;
	size_t					mPos = 0;
};

size_t MemoryRead(void* thePtr, size_t theSize, size_t theCount, void* theSource)
{
	MemoryFile* aFile = (MemoryFile*)theSource;
	if (theSize == 0)
		return 0;

	size_t aBytes = std::min(theSize * theCount, aFile->mSize - aFile->mPos);
	memcpy(thePtr, aFile->mData + aFile->mPos, aBytes);
	aFile->mPos += aBytes;
	return aBytes / theSize;
}

int MemorySeek(void* theSource, ogg_int64_t theOffset, int theOrigin)
{
	MemoryFile* aFile = (MemoryFile*)theSource;
	ogg_int64_t aNewPos;
	switch (theOrigin)
	{
	case SEEK_SET:	aNewPos = theOffset; break;
	case SEEK_CUR:	aNewPos = (ogg_int64_t)aFile->mPos + theOffset; break;
	case SEEK_END:	aNewPos = (ogg_int64_t)aFile->mSize + theOffset; break;
	default:		return -1;
	}

	if (aNewPos < 0 || aNewPos > (ogg_int64_t)aFile->mSize)
		return -1;

	aFile->mPos = (size_t)aNewPos;
	return 0;
}

int MemoryClose(void*)
{
	return 0;
}

long MemoryTell(void* theSource)
{
	return (long)((MemoryFile*)theSource)->mPos;
}

const ov_callbacks kMemoryCallbacks = { MemoryRead, MemorySeek, MemoryClose, MemoryTell };

}

bool Sexy::DecodeOggVorbis(const unsigned char* theData, size_t theSize, PCMSample& theSample)
{
	MemoryFile aFile;
	aFile.mData = theData;
	aFile.mSize = theSize;

	OggVorbis_File aVorbisFile;
	if (ov_open_callbacks(&aFile, &aVorbisFile, NULL, 0, kMemoryCallbacks) < 0)
		return false;

	vorbis_info* anInfo = ov_info(&aVorbisFile, -1);
	if (anInfo == NULL || anInfo->channels < 1 || anInfo->channels > 2)
	{
		ov_clear(&aVorbisFile);
		return false;
	}

	theSample.mChannels = anInfo->channels;
	theSample.mSampleRate = (int)anInfo->rate;

	ogg_int64_t aTotal = ov_pcm_total(&aVorbisFile, -1);
	theSample.mData.clear();
	if (aTotal > 0)
		theSample.mData.reserve((size_t)aTotal * theSample.mChannels);

	char aBuffer[16384];
	int aSection = 0;
	for (;;)
	{
		long aBytes = ov_read(&aVorbisFile, aBuffer, sizeof(aBuffer), &aSection);
		if (aBytes <= 0)
			break;

		const std::int16_t* aSamples = (const std::int16_t*)aBuffer;
		theSample.mData.insert(theSample.mData.end(), aSamples, aSamples + aBytes / 2);
	}

	ov_clear(&aVorbisFile);
	theSample.mFrames = (int)(theSample.mData.size() / theSample.mChannels);
	return theSample.mFrames > 0;
}

struct OggStream::Impl
{
	std::vector<unsigned char>	mData;
	MemoryFile					mFile;
	OggVorbis_File				mVorbisFile;
	bool						mOpen = false;
};

OggStream::OggStream() : mImpl(new Impl())
{
}

OggStream::~OggStream()
{
	if (mImpl->mOpen)
		ov_clear(&mImpl->mVorbisFile);
}

bool OggStream::Open(std::vector<unsigned char> theData)
{
	if (mImpl->mOpen)
	{
		ov_clear(&mImpl->mVorbisFile);
		mImpl->mOpen = false;
	}

	mImpl->mData = std::move(theData);
	mImpl->mFile.mData = mImpl->mData.data();
	mImpl->mFile.mSize = mImpl->mData.size();
	mImpl->mFile.mPos = 0;

	if (ov_open_callbacks(&mImpl->mFile, &mImpl->mVorbisFile, NULL, 0, kMemoryCallbacks) < 0)
		return false;

	mImpl->mOpen = true;
	vorbis_info* anInfo = ov_info(&mImpl->mVorbisFile, -1);
	if (anInfo == NULL || anInfo->channels < 1 || anInfo->channels > 2)
		return false;

	mChannels = anInfo->channels;
	mSampleRate = (int)anInfo->rate;
	return true;
}

int OggStream::Read(std::int16_t* theBuffer, int theFrames)
{
	if (!mImpl->mOpen || mChannels == 0)
		return 0;

	int aBytesWanted = theFrames * mChannels * 2;
	int aBytesRead = 0;
	int aSection = 0;
	while (aBytesRead < aBytesWanted)
	{
		long aBytes = ov_read(&mImpl->mVorbisFile, (char*)theBuffer + aBytesRead, aBytesWanted - aBytesRead, &aSection);
		if (aBytes <= 0)
			break;
		aBytesRead += (int)aBytes;
	}

	return aBytesRead / (mChannels * 2);
}

bool OggStream::SeekToFrame(std::int64_t theFrame)
{
	return mImpl->mOpen && ov_pcm_seek(&mImpl->mVorbisFile, theFrame) == 0;
}

std::int64_t OggStream::TellFrame()
{
	return mImpl->mOpen ? ov_pcm_tell(&mImpl->mVorbisFile) : 0;
}
