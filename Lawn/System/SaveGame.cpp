#include "Music.h"
#include "SaveGame.h"
#include "../Board.h"
#include "../Challenge.h"
#include "../SeedPacket.h"
#include "../../LawnApp.h"
#include "../CursorObject.h"
#include "../../Resources.h"
#include "../../ConstEnums.h"
#include "../MessageWidget.h"
#include "../../Sexy.TodLib/Trail.h"
#include "zlib/zlib.h"
#include "../../Sexy.TodLib/Attachment.h"
#include "../../Sexy.TodLib/Reanimator.h"
#include "../../Sexy.TodLib/TodParticle.h"
#include "../../Sexy.TodLib/EffectSystem.h"
#include "../../Sexy.TodLib/TodList.h"
#include "misc/Buffer.h"

#include <cstring>
#include <limits>

static const char* FILE_COMPILE_TIME_STRING = "Feb 16 200923:03:38";
static std::uint32_t SAVE_FILE_DATE = static_cast<std::uint32_t>(
	crc32(
		0,
		(Bytef*)FILE_COMPILE_TIME_STRING,
		static_cast<uInt>(strlen(FILE_COMPILE_TIME_STRING))));  //[0x6AA7EC]

void SaveGameContext::OpenRead(
	std::span<const std::byte> theBytes)
{
	mReader.Reset(theBytes);
	mWriter.Reset();
	mFailed = false;
	mReading = true;
}

void SaveGameContext::OpenWrite()
{
	mReader.Reset({});
	mWriter.Reset();
	mFailed = false;
	mReading = false;
}

std::uint64_t SaveGameContext::ByteLeftToRead() const
{
	return mReading ? mReader.GetBytesRemaining() : 0;
}

std::span<const std::byte> SaveGameContext::GetWrittenBytes() const
{
	return mWriter.GetBytes();
}

//0x4813D0
void SaveGameContext::SyncBytes(
	void* theDest,
	std::int32_t theReadSize)
{
	if (theReadSize < 0 || (theDest == nullptr && theReadSize != 0))
	{
		mFailed = true;
		return;
	}

	const auto aSize = static_cast<std::size_t>(theReadSize);
	if (mReading)
	{
		auto aBytes = std::span<std::byte>(
			static_cast<std::byte*>(theDest),
			aSize);
		if (!mReader.ReadBlock(aBytes))
		{
			mFailed = true;
			if (theDest != nullptr)
			{
				memset(theDest, 0, aSize);
			}
		}
	}
	else
	{
		const auto aBytes = std::span<const std::byte>(
			static_cast<const std::byte*>(theDest),
			aSize);
		if (!mWriter.WriteBlock(aBytes))
			mFailed = true;
	}
}

//0x481470
void SaveGameContext::SyncInt(std::int32_t& theInt)
{
	if (mReading)
	{
		if (!mReader.ReadI32(theInt))
		{
			mFailed = true;
			theInt = 0;
		}
	}
	else
	{
		if (!mWriter.WriteI32(theInt))
			mFailed = true;
	}
}

//0x4814C0
void SaveGameContext::SyncReanimationDef(ReanimatorDefinition*& theDefinition)
{
	if (mReading)
	{
		int aReanimType;
		SyncInt(aReanimType);
		if (aReanimType == (int)ReanimationType::REANIM_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aReanimType >= 0 && aReanimType < (int)ReanimationType::NUM_REANIMS)
		{
			ReanimatorEnsureDefinitionLoaded((ReanimationType)aReanimType, true);
			theDefinition = &gReanimatorDefArray[aReanimType];
		}
		else
		{
			mFailed = true;
		}
	}
	else
	{
		int aReanimType = (int)ReanimationType::REANIM_NONE;
		for (int i = 0; i < (int)ReanimationType::NUM_REANIMS; i++)
		{
			ReanimatorDefinition* aDef = &gReanimatorDefArray[i];
			if (theDefinition == aDef)
			{
				aReanimType = i;
				break;
			}
		}
		SyncInt(aReanimType);
	}
}

//0x481560
void SaveGameContext::SyncParticleDef(TodParticleDefinition*& theDefinition)
{
	if (mReading)
	{
		int aParticleType;
		SyncInt(aParticleType);
		if (aParticleType == (int)ParticleEffect::PARTICLE_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aParticleType >= 0 && aParticleType < (int)ParticleEffect::NUM_PARTICLES)
		{
			theDefinition = &gParticleDefArray[aParticleType];
		}
		else
		{
			mFailed = true;
		}
	}
	else
	{
		int aParticleType = (int)ParticleEffect::PARTICLE_NONE;
		for (int i = 0; i < (int)ParticleEffect::NUM_PARTICLES; i++)
		{
			TodParticleDefinition* aDef = &gParticleDefArray[i];
			if (theDefinition == aDef)
			{
				aParticleType = i;
				break;
			}
		}
		SyncInt(aParticleType);
	}
}

//0x4815F0
void SaveGameContext::SyncTrailDef(TrailDefinition*& theDefinition)
{
	if (mReading)
	{
		int aTrailType;
		SyncInt(aTrailType);
		if (aTrailType == TrailType::TRAIL_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aTrailType >= 0 && aTrailType < TrailType::NUM_TRAILS)
		{
			theDefinition = &gTrailDefArray[aTrailType];
		}
		else
		{
			mFailed = true;
		}
	}
	else
	{
		int aTrailType = TrailType::TRAIL_NONE;
		for (int i = 0; i < TrailType::NUM_TRAILS; i++)
		{
			TrailDefinition* aDef = &gTrailDefArray[i];
			if (theDefinition == aDef)
			{
				aTrailType = i;
				break;
			}
		}
		SyncInt(aTrailType);
	}
}

//0x481690
void SaveGameContext::SyncImage(Image*& theImage)
{
	if (mReading)
	{
		ResourceId aResID;
		std::int32_t aWireResourceId{};
		SyncInt(aWireResourceId);
		aResID = static_cast<ResourceId>(aWireResourceId);
		if (aResID == Sexy::ResourceId::RESOURCE_ID_MAX)
		{
			theImage = nullptr;
		}
		else
		{
			theImage = GetImageById(aResID);
		}
	}
	else
	{
		ResourceId aResID;
		if (theImage != nullptr)
		{
			aResID = GetIdByImage(theImage);
		}
		else
		{
			aResID = Sexy::ResourceId::RESOURCE_ID_MAX;
		}
		std::int32_t aWireResourceId =
			static_cast<std::int32_t>(aResID);
		SyncInt(aWireResourceId);
	}
}

//0x481710
void SyncDataIDList(TodList<unsigned int>* theDataIDList, SaveGameContext& theContext, TodAllocator* theAllocator)
{
	try
	{
		if (theContext.mReading)
		{
			if (theDataIDList)
			{
				theDataIDList->mHead = nullptr;
				theDataIDList->mTail = nullptr;
				theDataIDList->mSize = 0;
				theDataIDList->SetAllocator(theAllocator);
			}

			int aCount;
			theContext.SyncInt(aCount);
			for (int i = 0; i < aCount; i++)
			{
				std::uint32_t aDataID{};
				theContext.SyncUint(aDataID);
				theDataIDList->AddTail(aDataID);
			}
		}
		else
		{
			int aCount = theDataIDList->mSize;
			theContext.SyncInt(aCount);
			for (TodListNode<unsigned int>* aNode = theDataIDList->mHead; aNode != nullptr; aNode = aNode->mNext)
			{
				std::uint32_t aDataID = aNode->mValue;
				theContext.SyncUint(aDataID);
			}
		}
	}
	catch (std::exception&)
	{
		return;
	}
}

//0x4817C0
void SyncParticleEmitter(TodParticleSystem* theParticleSystem, TodParticleEmitter* theParticleEmitter, SaveGameContext& theContext)
{
	int aEmitterDefIndex = 0;
	if (theContext.mReading)
	{
		theContext.SyncInt(aEmitterDefIndex);
		theParticleEmitter->mParticleSystem = theParticleSystem;
		theParticleEmitter->mEmitterDef = &theParticleSystem->mParticleDef->mEmitterDefs[aEmitterDefIndex];
	}
	else
	{
		aEmitterDefIndex = ((intptr_t)theParticleEmitter->mEmitterDef - (intptr_t)theParticleSystem->mParticleDef->mEmitterDefs) / sizeof(TodEmitterDefinition);
		theContext.SyncInt(aEmitterDefIndex);
	}

	theContext.SyncImage(theParticleEmitter->mImageOverride);
	SyncDataIDList((TodList<unsigned int>*)&theParticleEmitter->mParticleList, theContext, &theParticleSystem->mParticleHolder->mParticleListNodeAllocator);
	for (TodListNode<ParticleID>* aNode = theParticleEmitter->mParticleList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		TodParticle* aParticle = theParticleSystem->mParticleHolder->mParticles.DataArrayGet((unsigned int)aNode->mValue);
		if (theContext.mReading)
		{
			aParticle->mParticleEmitter = theParticleEmitter;
		}
	}
}

//0x481880
void SyncParticleSystem(Board* theBoard, TodParticleSystem* theParticleSystem, SaveGameContext& theContext)
{
	theContext.SyncParticleDef(theParticleSystem->mParticleDef);
	if (theContext.mReading)
	{
		theParticleSystem->mParticleHolder = theBoard->mApp->mEffectSystem->mParticleHolder;
	}

	SyncDataIDList((TodList<unsigned int>*)&theParticleSystem->mEmitterList, theContext, &theParticleSystem->mParticleHolder->mEmitterListNodeAllocator);
	for (TodListNode<ParticleEmitterID>* aNode = theParticleSystem->mEmitterList.mHead; aNode != nullptr; aNode = aNode->mNext)
	{
		TodParticleEmitter* aEmitter = theParticleSystem->mParticleHolder->mEmitters.DataArrayGet((unsigned int)aNode->mValue);
		SyncParticleEmitter(theParticleSystem, aEmitter, theContext);
	}
}

//0x4818F0
void SyncReanimation(Board* theBoard, Reanimation* theReanimation, SaveGameContext& theContext)
{
	theContext.SyncReanimationDef(theReanimation->mDefinition);
	if (theContext.mFailed || theReanimation->mDefinition == nullptr)
	{
		theContext.mFailed = true;
		return;
	}
	if (theContext.mReading)
	{
		theReanimation->mReanimationHolder = theBoard->mApp->mEffectSystem->mReanimationHolder;
	}

	const int aTrackCount =
		theReanimation->mDefinition->mTracks.count;
	if (aTrackCount < 0)
	{
		theContext.mFailed = true;
		return;
	}

	if (aTrackCount != 0)
	{
		const std::uint64_t aByteCount =
			static_cast<std::uint64_t>(aTrackCount) *
			static_cast<std::uint64_t>(
				sizeof(ReanimatorTrackInstance));
		if (aByteCount >
			static_cast<std::uint64_t>(
				std::numeric_limits<std::int32_t>::max()))
		{
			theContext.mFailed = true;
			return;
		}
		const auto aSize = static_cast<std::int32_t>(aByteCount);
		if (theContext.mReading)
		{
			theReanimation->mTrackInstances = (ReanimatorTrackInstance*)FindGlobalAllocator(aSize)->Calloc(aSize);
		}
		theContext.SyncBytes(theReanimation->mTrackInstances, aSize);

		for (int aTrackIndex = 0;
			 aTrackIndex < aTrackCount;
			 aTrackIndex++)
		{
			ReanimatorTrackInstance& aTrackInstance = theReanimation->mTrackInstances[aTrackIndex];
			theContext.SyncImage(aTrackInstance.mImageOverride);

			if (theContext.mReading)
			{
				aTrackInstance.mBlendTransform.mText = "";
				TOD_ASSERT(aTrackInstance.mBlendTransform.mFont == nullptr);
				TOD_ASSERT(aTrackInstance.mBlendTransform.mImage == nullptr);
			}
			else
			{
				TOD_ASSERT(aTrackInstance.mBlendTransform.mText[0] == NULL);
				TOD_ASSERT(aTrackInstance.mBlendTransform.mFont == nullptr);
				TOD_ASSERT(aTrackInstance.mBlendTransform.mImage == nullptr);
			}
		}
	}
}

void SyncTrail(Board* theBoard, Trail* theTrail, SaveGameContext& theContext)
{
	theContext.SyncTrailDef(theTrail->mDefinition);
	if (theContext.mReading)
	{
		theTrail->mTrailHolder = theBoard->mApp->mEffectSystem->mTrailHolder;
	}
}

template <typename T> inline static void SyncDataArray(SaveGameContext& theContext, DataArray<T>& theDataArray)
{
	theContext.SyncUint(theDataArray.mFreeListHead);
	theContext.SyncUint(theDataArray.mMaxUsedCount);
	theContext.SyncUint(theDataArray.mSize);
	if (theContext.mFailed ||
		theDataArray.mMaxUsedCount > theDataArray.mMaxSize ||
		theDataArray.mSize > theDataArray.mMaxUsedCount ||
		theDataArray.mFreeListHead > theDataArray.mMaxUsedCount)
	{
		theContext.mFailed = true;
		return;
	}

	const std::uint64_t aByteCount =
		static_cast<std::uint64_t>(theDataArray.mMaxUsedCount) *
		static_cast<std::uint64_t>(sizeof(*theDataArray.mBlock));
	if (aByteCount >
		static_cast<std::uint64_t>(
			std::numeric_limits<std::int32_t>::max()))
	{
		theContext.mFailed = true;
		return;
	}

	theContext.SyncBytes(
		theDataArray.mBlock,
		static_cast<std::int32_t>(aByteCount));
}

static LegacyGameObjectState CaptureGameObjectState(
	const GameObject& theObject)
{
	return LegacyGameObjectState{
		.mX = static_cast<std::int32_t>(theObject.mX),
		.mY = static_cast<std::int32_t>(theObject.mY),
		.mWidth = static_cast<std::int32_t>(theObject.mWidth),
		.mHeight = static_cast<std::int32_t>(theObject.mHeight),
		.mVisible = theObject.mVisible,
		.mRow = static_cast<std::int32_t>(theObject.mRow),
		.mRenderOrder =
			static_cast<std::int32_t>(theObject.mRenderOrder),
	};
}

static void ApplyGameObjectState(
	GameObject& theObject,
	const LegacyGameObjectState& theState)
{
	theObject.mX = static_cast<int>(theState.mX);
	theObject.mY = static_cast<int>(theState.mY);
	theObject.mWidth = static_cast<int>(theState.mWidth);
	theObject.mHeight = static_cast<int>(theState.mHeight);
	theObject.mVisible = theState.mVisible;
	theObject.mRow = static_cast<int>(theState.mRow);
	theObject.mRenderOrder = static_cast<int>(theState.mRenderOrder);
}

static void SyncCursorObjectRecord(
	SaveGameContext& theContext,
	CursorObject& theCursor)
{
	if (theContext.mReading)
	{
		std::array<std::byte, kLegacyCursorObjectRecordSize> aBytes{};
		theContext.SyncBytes(
			aBytes.data(),
			static_cast<std::int32_t>(aBytes.size()));
		if (theContext.mFailed)
			return;

		LegacyCursorObjectState aState;
		if (!DecodeLegacyCursorObjectState(aBytes, aState))
		{
			theContext.mFailed = true;
			return;
		}

		ApplyGameObjectState(theCursor, aState.mGameObject);
		theCursor.mSeedBankIndex =
			static_cast<int>(aState.mSeedBankIndex);
		theCursor.mType = static_cast<SeedType>(aState.mType);
		theCursor.mImitaterType =
			static_cast<SeedType>(aState.mImitaterType);
		theCursor.mCursorType =
			static_cast<CursorType>(aState.mCursorType);
		theCursor.mCoinID = static_cast<CoinID>(aState.mCoinId);
		theCursor.mGlovePlantID =
			static_cast<PlantID>(aState.mGlovePlantId);
		theCursor.mDuplicatorPlantID =
			static_cast<PlantID>(aState.mDuplicatorPlantId);
		theCursor.mCobCannonPlantID =
			static_cast<PlantID>(aState.mCobCannonPlantId);
		theCursor.mHammerDownCounter =
			static_cast<int>(aState.mHammerDownCounter);
		theCursor.mReanimCursorID =
			static_cast<ReanimationID>(aState.mReanimCursorId);
		return;
	}

	const LegacyCursorObjectState aState{
		.mGameObject = CaptureGameObjectState(theCursor),
		.mSeedBankIndex =
			static_cast<std::int32_t>(theCursor.mSeedBankIndex),
		.mType = static_cast<std::int32_t>(theCursor.mType),
		.mImitaterType =
			static_cast<std::int32_t>(theCursor.mImitaterType),
		.mCursorType =
			static_cast<std::int32_t>(theCursor.mCursorType),
		.mCoinId = static_cast<std::uint32_t>(theCursor.mCoinID),
		.mGlovePlantId =
			static_cast<std::uint32_t>(theCursor.mGlovePlantID),
		.mDuplicatorPlantId =
			static_cast<std::uint32_t>(theCursor.mDuplicatorPlantID),
		.mCobCannonPlantId =
			static_cast<std::uint32_t>(theCursor.mCobCannonPlantID),
		.mHammerDownCounter =
			static_cast<std::int32_t>(theCursor.mHammerDownCounter),
		.mReanimCursorId =
			static_cast<std::uint32_t>(theCursor.mReanimCursorID),
	};
	auto aBytes = EncodeLegacyCursorObjectState(aState);
	theContext.SyncBytes(
		aBytes.data(),
		static_cast<std::int32_t>(aBytes.size()));
}

static void SyncCursorPreviewRecord(
	SaveGameContext& theContext,
	CursorPreview& theCursor)
{
	if (theContext.mReading)
	{
		std::array<std::byte, kLegacyCursorPreviewRecordSize> aBytes{};
		theContext.SyncBytes(
			aBytes.data(),
			static_cast<std::int32_t>(aBytes.size()));
		if (theContext.mFailed)
			return;

		LegacyCursorPreviewState aState;
		if (!DecodeLegacyCursorPreviewState(aBytes, aState))
		{
			theContext.mFailed = true;
			return;
		}

		ApplyGameObjectState(theCursor, aState.mGameObject);
		theCursor.mGridX = static_cast<int>(aState.mGridX);
		theCursor.mGridY = static_cast<int>(aState.mGridY);
		return;
	}

	const LegacyCursorPreviewState aState{
		.mGameObject = CaptureGameObjectState(theCursor),
		.mGridX = static_cast<std::int32_t>(theCursor.mGridX),
		.mGridY = static_cast<std::int32_t>(theCursor.mGridY),
	};
	auto aBytes = EncodeLegacyCursorPreviewState(aState);
	theContext.SyncBytes(
		aBytes.data(),
		static_cast<std::int32_t>(aBytes.size()));
}

static void SyncMusicRecord(
	SaveGameContext& theContext,
	Music& theMusic)
{
	if (theContext.mReading)
	{
		std::array<std::byte, kLegacyMusicRecordSize> aBytes{};
		theContext.SyncBytes(
			aBytes.data(),
			static_cast<std::int32_t>(aBytes.size()));
		if (theContext.mFailed)
			return;

		LegacyMusicState aState;
		if (!DecodeLegacyMusicState(aBytes, aState))
		{
			theContext.mFailed = true;
			return;
		}

		theMusic.mCurMusicTune =
			static_cast<MusicTune>(aState.mCurMusicTune);
		theMusic.mCurMusicFileMain =
			static_cast<MusicFile>(aState.mCurMusicFileMain);
		theMusic.mCurMusicFileDrums =
			static_cast<MusicFile>(aState.mCurMusicFileDrums);
		theMusic.mCurMusicFileHihats =
			static_cast<MusicFile>(aState.mCurMusicFileHihats);
		theMusic.mBurstOverride =
			static_cast<int>(aState.mBurstOverride);
		theMusic.mBaseBPM = aState.mBaseBpm;
		theMusic.mBaseModSpeed = aState.mBaseModSpeed;
		theMusic.mMusicBurstState =
			static_cast<MusicBurstState>(aState.mMusicBurstState);
		theMusic.mBurstStateCounter =
			static_cast<int>(aState.mBurstStateCounter);
		theMusic.mMusicDrumsState =
			static_cast<MusicDrumsState>(aState.mMusicDrumsState);
		theMusic.mQueuedDrumTrackPackedOrder =
			static_cast<int>(aState.mQueuedDrumTrackPackedOrder);
		theMusic.mDrumsStateCounter =
			static_cast<int>(aState.mDrumsStateCounter);
		theMusic.mPauseOffset =
			static_cast<int>(aState.mPauseOffset);
		theMusic.mPauseOffsetDrums =
			static_cast<int>(aState.mPauseOffsetDrums);
		theMusic.mPaused = aState.mPaused;
		theMusic.mMusicDisabled = aState.mMusicDisabled;
		theMusic.mFadeOutCounter =
			static_cast<int>(aState.mFadeOutCounter);
		theMusic.mFadeOutDuration =
			static_cast<int>(aState.mFadeOutDuration);
		return;
	}

	const LegacyMusicState aState{
		.mCurMusicTune =
			static_cast<std::int32_t>(theMusic.mCurMusicTune),
		.mCurMusicFileMain =
			static_cast<std::int32_t>(theMusic.mCurMusicFileMain),
		.mCurMusicFileDrums =
			static_cast<std::int32_t>(theMusic.mCurMusicFileDrums),
		.mCurMusicFileHihats =
			static_cast<std::int32_t>(theMusic.mCurMusicFileHihats),
		.mBurstOverride =
			static_cast<std::int32_t>(theMusic.mBurstOverride),
		.mBaseBpm = theMusic.mBaseBPM,
		.mBaseModSpeed = theMusic.mBaseModSpeed,
		.mMusicBurstState =
			static_cast<std::int32_t>(theMusic.mMusicBurstState),
		.mBurstStateCounter =
			static_cast<std::int32_t>(theMusic.mBurstStateCounter),
		.mMusicDrumsState =
			static_cast<std::int32_t>(theMusic.mMusicDrumsState),
		.mQueuedDrumTrackPackedOrder = static_cast<std::int32_t>(
			theMusic.mQueuedDrumTrackPackedOrder),
		.mDrumsStateCounter =
			static_cast<std::int32_t>(theMusic.mDrumsStateCounter),
		.mPauseOffset =
			static_cast<std::int32_t>(theMusic.mPauseOffset),
		.mPauseOffsetDrums =
			static_cast<std::int32_t>(theMusic.mPauseOffsetDrums),
		.mPaused = theMusic.mPaused,
		.mMusicDisabled = theMusic.mMusicDisabled,
		.mFadeOutCounter =
			static_cast<std::int32_t>(theMusic.mFadeOutCounter),
		.mFadeOutDuration =
			static_cast<std::int32_t>(theMusic.mFadeOutDuration),
	};
	auto aBytes = EncodeLegacyMusicState(aState);
	theContext.SyncBytes(
		aBytes.data(),
		static_cast<std::int32_t>(aBytes.size()));
}

static void SyncMessageRecord(
	SaveGameContext& theContext,
	MessageWidget& theMessage)
{
	static_assert(
		MAX_MESSAGE_LENGTH == kLegacyMessageTextLength,
		"message wire schema must match the runtime array count");
	static_assert(
		sizeof(SexyChar) == sizeof(std::uint8_t),
		"legacy message adapter requires one-byte runtime characters");

	if (theContext.mReading)
	{
		std::array<std::byte, kLegacyMessageRecordSize> aBytes{};
		theContext.SyncBytes(
			aBytes.data(),
			static_cast<std::int32_t>(aBytes.size()));
		if (theContext.mFailed)
			return;

		LegacyMessageState aState;
		if (!DecodeLegacyMessageState(aBytes, aState))
		{
			theContext.mFailed = true;
			return;
		}

		for (std::size_t anIndex = 0;
			 anIndex < kLegacyMessageTextLength;
			 ++anIndex)
		{
			theMessage.mLabel[anIndex] =
				static_cast<SexyChar>(aState.mLabel[anIndex]);
			theMessage.mTextReanimID[anIndex] =
				static_cast<ReanimationID>(
					aState.mTextReanimIds[anIndex]);
			theMessage.mLabelNext[anIndex] =
				static_cast<SexyChar>(aState.mLabelNext[anIndex]);
		}
		theMessage.mDisplayTime =
			static_cast<int>(aState.mDisplayTime);
		theMessage.mDuration = static_cast<int>(aState.mDuration);
		theMessage.mMessageStyle =
			static_cast<MessageStyle>(aState.mMessageStyle);
		theMessage.mReanimType =
			static_cast<ReanimationType>(aState.mReanimType);
		theMessage.mSlideOffTime =
			static_cast<int>(aState.mSlideOffTime);
		theMessage.mMessageStyleNext =
			static_cast<MessageStyle>(aState.mMessageStyleNext);
		return;
	}

	LegacyMessageState aState;
	for (std::size_t anIndex = 0;
		 anIndex < kLegacyMessageTextLength;
		 ++anIndex)
	{
		aState.mLabel[anIndex] = static_cast<std::uint8_t>(
			static_cast<unsigned char>(theMessage.mLabel[anIndex]));
		aState.mTextReanimIds[anIndex] =
			static_cast<std::uint32_t>(
				theMessage.mTextReanimID[anIndex]);
		aState.mLabelNext[anIndex] = static_cast<std::uint8_t>(
			static_cast<unsigned char>(
				theMessage.mLabelNext[anIndex]));
	}
	aState.mDisplayTime =
		static_cast<std::int32_t>(theMessage.mDisplayTime);
	aState.mDuration =
		static_cast<std::int32_t>(theMessage.mDuration);
	aState.mMessageStyle =
		static_cast<std::int32_t>(theMessage.mMessageStyle);
	aState.mReanimType =
		static_cast<std::int32_t>(theMessage.mReanimType);
	aState.mSlideOffTime =
		static_cast<std::int32_t>(theMessage.mSlideOffTime);
	aState.mMessageStyleNext =
		static_cast<std::int32_t>(theMessage.mMessageStyleNext);

	auto aBytes = EncodeLegacyMessageState(aState);
	theContext.SyncBytes(
		aBytes.data(),
		static_cast<std::int32_t>(aBytes.size()));
}

static LegacySeedPacketState CaptureSeedPacketState(
	const SeedPacket& thePacket)
{
	return LegacySeedPacketState{
		.mGameObject = CaptureGameObjectState(thePacket),
		.mRefreshCounter =
			static_cast<std::int32_t>(thePacket.mRefreshCounter),
		.mRefreshTime =
			static_cast<std::int32_t>(thePacket.mRefreshTime),
		.mIndex = static_cast<std::int32_t>(thePacket.mIndex),
		.mOffsetX = static_cast<std::int32_t>(thePacket.mOffsetX),
		.mPacketType =
			static_cast<std::int32_t>(thePacket.mPacketType),
		.mImitaterType =
			static_cast<std::int32_t>(thePacket.mImitaterType),
		.mSlotMachineCountDown = static_cast<std::int32_t>(
			thePacket.mSlotMachineCountDown),
		.mSlotMachiningNextSeed = static_cast<std::int32_t>(
			thePacket.mSlotMachiningNextSeed),
		.mSlotMachiningPosition = thePacket.mSlotMachiningPosition,
		.mActive = thePacket.mActive,
		.mRefreshing = thePacket.mRefreshing,
		.mTimesUsed =
			static_cast<std::int32_t>(thePacket.mTimesUsed),
	};
}

static void ApplySeedPacketState(
	SeedPacket& thePacket,
	const LegacySeedPacketState& theState)
{
	ApplyGameObjectState(thePacket, theState.mGameObject);
	thePacket.mRefreshCounter =
		static_cast<int>(theState.mRefreshCounter);
	thePacket.mRefreshTime =
		static_cast<int>(theState.mRefreshTime);
	thePacket.mIndex = static_cast<int>(theState.mIndex);
	thePacket.mOffsetX = static_cast<int>(theState.mOffsetX);
	thePacket.mPacketType =
		static_cast<SeedType>(theState.mPacketType);
	thePacket.mImitaterType =
		static_cast<SeedType>(theState.mImitaterType);
	thePacket.mSlotMachineCountDown =
		static_cast<int>(theState.mSlotMachineCountDown);
	thePacket.mSlotMachiningNextSeed =
		static_cast<SeedType>(theState.mSlotMachiningNextSeed);
	thePacket.mSlotMachiningPosition =
		theState.mSlotMachiningPosition;
	thePacket.mActive = theState.mActive;
	thePacket.mRefreshing = theState.mRefreshing;
	thePacket.mTimesUsed = static_cast<int>(theState.mTimesUsed);
}

static void SyncSeedBankRecord(
	SaveGameContext& theContext,
	SeedBank& theSeedBank)
{
	static_assert(
		SEEDBANK_MAX == kLegacySeedPacketCount,
		"seed bank wire schema must match the runtime packet count");

	if (theContext.mReading)
	{
		std::array<std::byte, kLegacySeedBankRecordSize> aBytes{};
		theContext.SyncBytes(
			aBytes.data(),
			static_cast<std::int32_t>(aBytes.size()));
		if (theContext.mFailed)
			return;

		LegacySeedBankState aState;
		if (!DecodeLegacySeedBankState(aBytes, aState))
		{
			theContext.mFailed = true;
			return;
		}

		ApplyGameObjectState(theSeedBank, aState.mGameObject);
		theSeedBank.mNumPackets =
			static_cast<int>(aState.mNumPackets);
		for (std::size_t anIndex = 0;
			 anIndex < kLegacySeedPacketCount;
			 ++anIndex)
		{
			ApplySeedPacketState(
				theSeedBank.mSeedPackets[anIndex],
				aState.mSeedPackets[anIndex]);
		}
		theSeedBank.mCutSceneDarken =
			static_cast<int>(aState.mCutSceneDarken);
		theSeedBank.mConveyorBeltCounter =
			static_cast<int>(aState.mConveyorBeltCounter);
		return;
	}

	LegacySeedBankState aState;
	aState.mGameObject = CaptureGameObjectState(theSeedBank);
	aState.mNumPackets =
		static_cast<std::int32_t>(theSeedBank.mNumPackets);
	for (std::size_t anIndex = 0;
		 anIndex < kLegacySeedPacketCount;
		 ++anIndex)
	{
		aState.mSeedPackets[anIndex] =
			CaptureSeedPacketState(
				theSeedBank.mSeedPackets[anIndex]);
	}
	aState.mCutSceneDarken =
		static_cast<std::int32_t>(theSeedBank.mCutSceneDarken);
	aState.mConveyorBeltCounter =
		static_cast<std::int32_t>(
			theSeedBank.mConveyorBeltCounter);

	auto aBytes = EncodeLegacySeedBankState(aState);
	theContext.SyncBytes(
		aBytes.data(),
		static_cast<std::int32_t>(aBytes.size()));
}

static void SyncUnmigratedNativeBlock(
	SaveGameContext& theContext,
	void* theData,
	std::size_t theByteCount)
{
	if (theByteCount >
		static_cast<std::size_t>(
			std::numeric_limits<std::int32_t>::max()))
	{
		theContext.mFailed = true;
		return;
	}
	theContext.SyncBytes(
		theData,
		static_cast<std::int32_t>(theByteCount));
}

//0x4819D0
void SyncBoard(SaveGameContext& theContext, Board* theBoard)
{
	const auto* aBoardBytes =
		reinterpret_cast<const std::byte*>(theBoard);
	const auto* aPausedBytes =
		reinterpret_cast<const std::byte*>(&theBoard->mPaused);
	const auto anOffset = aPausedBytes - aBoardBytes;
	if (anOffset < 0 ||
		static_cast<std::size_t>(anOffset) > sizeof(Board))
	{
		theContext.mFailed = true;
		return;
	}
	SyncUnmigratedNativeBlock(
		theContext,
		&theBoard->mPaused,
		sizeof(Board) - static_cast<std::size_t>(anOffset));

	SyncDataArray(theContext, theBoard->mZombies);													//0x482190
	SyncDataArray(theContext, theBoard->mPlants);													//0x482280
	SyncDataArray(theContext, theBoard->mProjectiles);												//0x482370
	SyncDataArray(theContext, theBoard->mCoins);													//0x482460
	SyncDataArray(theContext, theBoard->mLawnMowers);												//0x482550
	SyncDataArray(theContext, theBoard->mGridItems);												//0x482650
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticleSystems);	//0x482740
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mEmitters);			//0x482830
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticles);			//0x482920
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations);	//0x482920
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mTrailHolder->mTrails);				//0x482650
	SyncDataArray(theContext, theBoard->mApp->mEffectSystem->mAttachmentHolder->mAttachments);		//0x482A10

	{
		TodParticleSystem* aParticle = nullptr;
		while (theBoard->mApp->mEffectSystem->mParticleHolder->mParticleSystems.IterateNext(aParticle))
		{
			SyncParticleSystem(theBoard, aParticle, theContext);
		}
	}
	{
		Reanimation* aReanimation = nullptr;
		while (theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations.IterateNext(aReanimation))
		{
			SyncReanimation(theBoard, aReanimation, theContext);
		}
	}
	{
		Trail* aTrail = nullptr;
		while (theBoard->mApp->mEffectSystem->mTrailHolder->mTrails.IterateNext(aTrail))
		{
			SyncTrail(theBoard, aTrail, theContext);
		}
	}

	SyncCursorObjectRecord(theContext, *theBoard->mCursorObject);
	SyncCursorPreviewRecord(theContext, *theBoard->mCursorPreview);
	SyncMessageRecord(theContext, *theBoard->mAdvice);
	SyncSeedBankRecord(theContext, *theBoard->mSeedBank);
	SyncUnmigratedNativeBlock(
		theContext,
		theBoard->mChallenge,
		sizeof(Challenge));
	SyncMusicRecord(theContext, *theBoard->mApp->mMusic);
	
	std::uint32_t aMarker = kLegacySaveMagic;
	theContext.SyncUint(aMarker);
	if (theContext.mReading && aMarker != kLegacySaveMagic)
		theContext.mFailed = true;
}

//0x481CE0
void FixBoardAfterLoad(Board* theBoard)
{
	{
		Plant* aPlant = nullptr;
		while (theBoard->mPlants.IterateNext(aPlant))
		{
			aPlant->mApp = theBoard->mApp;
			aPlant->mBoard = theBoard;
		}
	}
	{
		Zombie* aZombie = nullptr;
		while (theBoard->mZombies.IterateNext(aZombie))
		{
			aZombie->mApp = theBoard->mApp;
			aZombie->mBoard = theBoard;
		}
	}
	{
		Projectile* aProjectile = nullptr;
		while (theBoard->mProjectiles.IterateNext(aProjectile))
		{
			aProjectile->mApp = theBoard->mApp;
			aProjectile->mBoard = theBoard;
		}
	}
	{
		Coin* aCoin = nullptr;
		while (theBoard->mCoins.IterateNext(aCoin))
		{
			aCoin->mApp = theBoard->mApp;
			aCoin->mBoard = theBoard;
		}
	}
	{
		LawnMower* aLawnMower = nullptr;
		while (theBoard->mLawnMowers.IterateNext(aLawnMower))
		{
			aLawnMower->mApp = theBoard->mApp;
			aLawnMower->mBoard = theBoard;
		}
	}
	{
		GridItem* aGridItem = nullptr;
		while (theBoard->mGridItems.IterateNext(aGridItem))
		{
			aGridItem->mApp = theBoard->mApp;
			aGridItem->mBoard = theBoard;
		}
	}

	theBoard->mAdvice->mApp = theBoard->mApp;
	theBoard->mCursorObject->mApp = theBoard->mApp;
	theBoard->mCursorObject->mBoard = theBoard;
	theBoard->mCursorPreview->mApp = theBoard->mApp;
	theBoard->mCursorPreview->mBoard = theBoard;
	theBoard->mSeedBank->mApp = theBoard->mApp;
	theBoard->mSeedBank->mBoard = theBoard;
	for (int i = 0; i < SEEDBANK_MAX; i++)
	{
		theBoard->mSeedBank->mSeedPackets[i].mApp = theBoard->mApp;
		theBoard->mSeedBank->mSeedPackets[i].mBoard = theBoard;
	}
	theBoard->mChallenge->mApp = theBoard->mApp;
	theBoard->mChallenge->mBoard = theBoard;
	theBoard->mApp->mMusic->mApp = theBoard->mApp;
	theBoard->mApp->mMusic->mMusicInterface = theBoard->mApp->mMusicInterface;
}

//0x481FE0
// GOTY @Patoke: 0x48CBC0
bool LawnLoadGame(Board* theBoard, const std::string& theFilePath)
{
	Buffer aBuffer;
	if (!gSexyAppBase->ReadBufferFromFile(theFilePath, &aBuffer, false))
	{
		return false;
	}

	SaveGameContext aContext;
	const int aBufferSize = aBuffer.GetDataLen();
	if (aBufferSize < 0)
	{
		return false;
	}
	aContext.OpenRead(std::span<const std::byte>(
		reinterpret_cast<const std::byte*>(aBuffer.GetDataPtr()),
		static_cast<std::size_t>(aBufferSize)));

	std::array<std::byte, kLegacySaveHeaderSize> aHeaderBytes{};
	aContext.SyncBytes(
		aHeaderBytes.data(),
		static_cast<std::int32_t>(aHeaderBytes.size()));
	SaveFileHeader aHeader;
	if (aContext.mFailed ||
		!DecodeLegacySaveHeader(aHeaderBytes, aHeader) ||
		aHeader.mMagicNumber != kLegacySaveMagic ||
		aHeader.mBuildVersion != kLegacySaveVersion ||
		aHeader.mBuildDate != SAVE_FILE_DATE)
	{
		return false;
	}

	SyncBoard(aContext, theBoard);
	if (aContext.mFailed)
	{
		return false;
	}

	TodTrace("Loaded save game");
	FixBoardAfterLoad(theBoard);
	theBoard->mApp->mGameScene = GameScenes::SCENE_PLAYING;
	return true;
}

//0x4820D0
bool LawnSaveGame(Board* theBoard, const std::string& theFilePath)
{
	SaveGameContext aContext;
	aContext.OpenWrite();

	SaveFileHeader aHeader;
	aHeader.mMagicNumber = kLegacySaveMagic;
	aHeader.mBuildVersion = kLegacySaveVersion;
	aHeader.mBuildDate = SAVE_FILE_DATE;

	auto aHeaderBytes = EncodeLegacySaveHeader(aHeader);
	aContext.SyncBytes(
		aHeaderBytes.data(),
		static_cast<std::int32_t>(aHeaderBytes.size()));
	SyncBoard(aContext, theBoard);
	if (aContext.mFailed)
		return false;

	const auto aBytes = aContext.GetWrittenBytes();
	if (aBytes.size() > std::numeric_limits<unsigned long>::max())
		return false;

	return gSexyAppBase->WriteBytesToFile(
		theFilePath,
		aBytes.data(),
		static_cast<unsigned long>(aBytes.size()));
}
