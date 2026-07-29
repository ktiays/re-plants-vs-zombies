#include "DataSync.h"
#include "PlayerInfo.h"
#include "../LawnCommon.h"
#include "../Widget/ChallengeScreen.h"
#include "../../Sexy.TodLib/TodDebug.h"
#include "../../Sexy.TodLib/TodCommon.h"
#include "misc/Buffer.h"
#include "../../SexyAppFramework/SexyAppBase.h"

//0x469400
void PlayerInfo::LoadDetails()
{
	try
	{
		Buffer aBuffer;
		std::string aFileName = GetAppDataFolder() + StrFormat("userdata/user%d.dat", mId);
		if (!gSexyAppBase->ReadBufferFromFile(aFileName, &aBuffer, false))
		{
			return;
		}

		DataReader aReader;
		aReader.OpenMemory(aBuffer.GetDataPtr(), aBuffer.GetDataLen(), false);
		DataSync aSync(aReader);
		SyncDetails(aSync);
	}
	catch (DataReaderException&)
	{
		TodTrace("Failed to player data, resetting it\n");
		Reset();
	}
}

//0x4695F0
// GOTY @Patoke: 0x46D750
void PlayerInfo::SaveDetails()
{
	DataWriter aWriter;
	aWriter.OpenMemory();
	DataSync aSync(aWriter);
	SyncDetails(aSync);

	MkDir(GetAppDataFolder() + "userdata");
	std::string aFileName = GetAppDataFolder() + StrFormat("userdata/user%d.dat", mId);
	gSexyAppBase->WriteBytesToFile(aFileName, aWriter.GetDataPtr(), aWriter.GetDataLen());
}

//0x469810
void PlayerInfo::DeleteUserFiles()
{
	std::string aFilename = GetAppDataFolder() + StrFormat("userdata/user%d.dat", mId);
	gSexyAppBase->EraseFile(aFilename);

	for (int i = 0; i < (int)GameMode::NUM_GAME_MODES; i++)
	{
		std::string aFileName = GetSavedGameName((GameMode)i, mId);
		gSexyAppBase->EraseFile(aFileName);
	}
}

void PlayerInfo::AddCoins(int theAmount)
{
	mCoins += theAmount;
	if (mCoins > 99999)
	{
		mCoins = 99999;
	}
	else if (mCoins < 0)
	{
		mCoins = 0;
	}
}

void PlayerInfo::ResetChallengeRecord(GameMode theGameMode)
{
	int aGameMode = (int)theGameMode - (int)GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1;
	TOD_ASSERT(aGameMode >= 0 && aGameMode <= NUM_CHALLENGE_MODES);
	mChallengeRecords[aGameMode] = 0;
}

//0x469A00
void PottedPlant::InitializePottedPlant(SeedType theSeedType)
{
	memset(this, 0, sizeof(PottedPlant));
	mSeedType = theSeedType;
	mDrawVariation = DrawVariation::VARIATION_NORMAL;
	mLastWateredTime = 0;
	mFacing = (FacingDirection)RandRangeInt((int)FacingDirection::FACING_RIGHT, (int)FacingDirection::FACING_LEFT);
	mPlantAge = PottedPlantAge::PLANTAGE_SPROUT;
	mTimesFed = 0;
	mWhichZenGarden = GardenType::GARDEN_MAIN;
	mFeedingsPerGrow = RandRangeInt(3, 5);
	mPlantNeed = PottedPlantNeed::PLANTNEED_NONE;
	mLastNeedFulfilledTime = 0;
	mLastFertilizedTime = 0;
	mLastChocolateTime = 0;
}

void PlayerInfo::DeleteZombatarFromIndex(int* theIndex)
{
	if (*theIndex < 0 || *theIndex > mNumZombatars - 1)
	{
		TodTraceAndLog("Attempting to delete invalid Zombatar at index %d", *theIndex);
		return;
	}

	if (*theIndex == mNumZombatars - 1)
	{
		// if zombatar is at the end of the list, we can just invalidate it
		mNumZombatars = mNumZombatars - 1;
	}
	else
	{
		if (*theIndex > 0)
		{
			// if the zombatar is in the middle of the list, we need to shift the rest of the list down
			memcpy(&mZombatars[*theIndex], &mZombatars[*theIndex + 1], sizeof(Zombatar) * (mNumZombatars - *theIndex));
		}
		else
		{
			// if the zombatar is at the beginning of the list, we can just shift the rest of the list down
			memcpy(mZombatars, &mZombatars[1], sizeof(Zombatar) * mNumZombatars - sizeof(Zombatar));
		}

		--mNumZombatars;
	}
}
