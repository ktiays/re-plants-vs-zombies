#ifndef __PLAYERINFO_H__
#define __PLAYERINFO_H__

#define MAX_NUM_ZOMBATARS 100
#define MAX_POTTED_PLANTS 200
#define PURCHASE_COUNT_OFFSET 1000

#include <cstdint>
#include <string>
#include "../../ConstEnums.h"

class PottedPlant
{
public:
    enum FacingDirection
    {
        FACING_RIGHT,
        FACING_LEFT
    };

public:
    SeedType            mSeedType;                  //+0x0
    GardenType          mWhichZenGarden;            //+0x4
    std::int32_t        mX;                         //+0x8
    std::int32_t        mY;                         //+0xC
    FacingDirection     mFacing;                    //+0x10

    std::int64_t        mLastWateredTime;           //+0x18
    DrawVariation       mDrawVariation;             //+0x20
    PottedPlantAge      mPlantAge;                  //+0x24
    std::int32_t        mTimesFed;                  //+0x28
    std::int32_t        mFeedingsPerGrow;           //+0x2C
    PottedPlantNeed     mPlantNeed;                 //+0x30

    std::int64_t        mLastNeedFulfilledTime;     //+0x38
    std::int64_t        mLastFertilizedTime;        //+0x40
    std::int64_t        mLastChocolateTime;         //+0x48
    std::int64_t        mFutureAttribute[1];        //+0x50

public:
    void                InitializePottedPlant(SeedType theSeedType);
};

class Zombatar
{
public:

    std::int32_t mSkin;
    std::int32_t mSkinColor;
    std::int32_t mClothes;
    std::int32_t mClothesColor;
    std::int32_t mTidbits;
    std::int32_t mTidbitsColor;
    std::int32_t mAccessories;
    std::int32_t mAccessoriesColor;
    std::int32_t mFacialHair;
    std::int32_t mFacialHairColor;
    std::int32_t mHair;
    std::int32_t mHairColor;
    std::int32_t mEyewear;
    std::int32_t mEyewearColor;
    std::int32_t mHat;
    std::int32_t mHatColor;
    std::int32_t mBackdrop;
    std::int32_t mBackdropColor;
};

class DataSync;
class PlayerInfo
{
public:
    std::string         mName;                              //+GOTY @Patoke: 0x0
    std::uint32_t       mUseSeq;                            //+GOTY @Patoke: 0x1C
    std::uint32_t       mId;                                //+GOTY @Patoke: 0x20
	bool                mEarnedAchievements[20];            //+GOTY @Patoke: 0x24
	bool                mShownAchievements[20];             //+GOTY @Patoke: 0x38
	std::int32_t        mLevel;                             //+GOTY @Patoke: 0x4C
	std::int32_t        mCoins;                             //+GOTY @Patoke: 0x50
	std::int32_t        mFinishedAdventure;                 //+GOTY @Patoke: 0x54
	std::int32_t        mChallengeRecords[100];             //+GOTY @Patoke: 0x58
	std::int32_t        mPurchases[80];                     //+GOTY @Patoke: 0x1E8
	std::int32_t        mPlayTimeActivePlayer;              //+GOTY @Patoke: 0x328
	std::int32_t        mPlayTimeInactivePlayer;            //+GOTY @Patoke: 0x32C
	std::int32_t        mHasUsedCheatKeys;                  //+GOTY @Patoke: 0x330
	std::int32_t        mHasWokenStinky;                    //+GOTY @Patoke: 0x334
	std::int32_t        mDidntPurchasePacketUpgrade;        //+GOTY @Patoke: 0x338
	std::int32_t        mLastStinkyChocolateTime;           //+GOTY @Patoke: 0x33C
	std::int32_t        mStinkyPosX;                        //+GOTY @Patoke: 0x340
	std::int32_t        mStinkyPosY;                        //+GOTY @Patoke: 0x344
	std::int32_t        mHasUnlockedMinigames;              //+GOTY @Patoke: 0x348
	std::int32_t        mHasUnlockedPuzzleMode;             //+GOTY @Patoke: 0x34C
	std::int32_t        mHasNewMiniGame;                    //+GOTY @Patoke: 0x350
	std::int32_t        mHasNewScaryPotter;                 //+GOTY @Patoke: 0x354
	std::int32_t        mHasNewIZombie;                     //+GOTY @Patoke: 0x358
	std::int32_t        mHasNewSurvival;                    //+GOTY @Patoke: 0x35C
	std::int32_t        mHasUnlockedSurvivalMode;           //+GOTY @Patoke: 0x360
	std::int32_t        mNeedsMessageOnGameSelector;        //+GOTY @Patoke: 0x364
	std::int32_t        mNeedsMagicTacoReward;              //+GOTY @Patoke: 0x368
	std::int32_t        mHasSeenStinky;                     //+GOTY @Patoke: 0x36C
	std::int32_t        mHasSeenUpsell;                     //+GOTY @Patoke: 0x370
	std::int32_t        mPlaceHolderPlayerStats;            //+GOTY @Patoke: 0x374
	std::int32_t        mNumPottedPlants;                   //+GOTY @Patoke: 0x378
	bool                mShownZombatarDesktopMessage;       //+GOTY @Patoke: 0x37C
	bool                mAcceptedZombatarULA;               //+GOTY @Patoke: 0x37D
    PottedPlant         mPottedPlant[MAX_POTTED_PLANTS];    //+GOTY @Patoke: 0x380
    std::int32_t        mNumZombatars;                      //+GOTY @Patoke: 0x4840
    Zombatar            mZombatars[MAX_NUM_ZOMBATARS];      //+GOTY @Patoke: 0x4844
    bool                mMiniGamesCompleted[20];            //+GOTY @Patoke: 0x6464

public:
    PlayerInfo();

    void                Reset();
    /*inline*/ void     AddCoins(int theAmount);
    void                SyncSummary(DataSync& theSync);
    void                SyncDetails(DataSync& theSync);
    void                DeleteUserFiles();
    void                LoadDetails();
    void                SaveDetails();
    inline int          GetLevel() const { return mLevel; }
    inline void         SetLevel(int theLevel) { mLevel = theLevel; }
    /*inline*/ void     ResetChallengeRecord(GameMode theGameMode);
    void                DeleteZombatarFromIndex(int* theIndex);
};

#endif
