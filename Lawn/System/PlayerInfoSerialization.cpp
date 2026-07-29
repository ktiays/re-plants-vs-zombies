#include "DataSync.h"
#include "PlayerInfo.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace
{

constexpr std::int32_t kUserVersion = 12;

template<typename Enum>
void SyncEnum32(DataSync& theSync, Enum& theValue)
{
    static_assert(std::is_enum_v<Enum>);

    std::int32_t aWireValue = static_cast<std::int32_t>(theValue);
    theSync.SyncSLong(aWireValue);
    if (theSync.GetReader())
        theValue = static_cast<Enum>(aWireValue);
}

void SyncPottedPlant(DataSync& theSync, PottedPlant& thePlant)
{
    SyncEnum32(theSync, thePlant.mSeedType);
    SyncEnum32(theSync, thePlant.mWhichZenGarden);
    theSync.SyncSLong(thePlant.mX);
    theSync.SyncSLong(thePlant.mY);
    SyncEnum32(theSync, thePlant.mFacing);

    // Version 12 stored the native MSVC layout. Preserve its two four-byte
    // alignment gaps explicitly without depending on the current compiler.
    std::uint32_t aReservedWord = 0;
    theSync.SyncLong(aReservedWord);

    theSync.SyncInt64(thePlant.mLastWateredTime);
    SyncEnum32(theSync, thePlant.mDrawVariation);
    SyncEnum32(theSync, thePlant.mPlantAge);
    theSync.SyncSLong(thePlant.mTimesFed);
    theSync.SyncSLong(thePlant.mFeedingsPerGrow);
    SyncEnum32(theSync, thePlant.mPlantNeed);

    aReservedWord = 0;
    theSync.SyncLong(aReservedWord);

    theSync.SyncInt64(thePlant.mLastNeedFulfilledTime);
    theSync.SyncInt64(thePlant.mLastFertilizedTime);
    theSync.SyncInt64(thePlant.mLastChocolateTime);
    theSync.SyncInt64(thePlant.mFutureAttribute[0]);
}

void SyncZombatar(DataSync& theSync, Zombatar& theZombatar)
{
    theSync.SyncSLong(theZombatar.mSkin);
    theSync.SyncSLong(theZombatar.mSkinColor);
    theSync.SyncSLong(theZombatar.mClothes);
    theSync.SyncSLong(theZombatar.mClothesColor);
    theSync.SyncSLong(theZombatar.mTidbits);
    theSync.SyncSLong(theZombatar.mTidbitsColor);
    theSync.SyncSLong(theZombatar.mAccessories);
    theSync.SyncSLong(theZombatar.mAccessoriesColor);
    theSync.SyncSLong(theZombatar.mFacialHair);
    theSync.SyncSLong(theZombatar.mFacialHairColor);
    theSync.SyncSLong(theZombatar.mHair);
    theSync.SyncSLong(theZombatar.mHairColor);
    theSync.SyncSLong(theZombatar.mEyewear);
    theSync.SyncSLong(theZombatar.mEyewearColor);
    theSync.SyncSLong(theZombatar.mHat);
    theSync.SyncSLong(theZombatar.mHatColor);
    theSync.SyncSLong(theZombatar.mBackdrop);
    theSync.SyncSLong(theZombatar.mBackdropColor);
}

void ValidateSerializedCount(
    DataSync& theSync,
    std::int32_t theCount,
    std::int32_t theMaximum,
    const char* theDescription)
{
    if (theCount >= 0 && theCount <= theMaximum)
        return;

    if (theSync.GetReader())
        throw DataReaderException();

    throw std::out_of_range(theDescription);
}

} // namespace

PlayerInfo::PlayerInfo()
{
    Reset();
}

void PlayerInfo::SyncSummary(DataSync& theSync)
{
    theSync.SyncString(mName);
    theSync.SyncLong(mUseSeq);
    theSync.SyncLong(mId);
}

void PlayerInfo::SyncDetails(DataSync& theSync)
{
    if (theSync.GetReader())
        Reset();

    std::int32_t aVersion = kUserVersion;
    theSync.SyncLong(aVersion);
    theSync.SetVersion(aVersion);
    if (aVersion != kUserVersion)
        return;

    theSync.SyncLong(mLevel);
    theSync.SyncLong(mCoins);
    theSync.SyncLong(mFinishedAdventure);
    for (auto& aRecord : mChallengeRecords)
        theSync.SyncLong(aRecord);
    for (auto& aPurchase : mPurchases)
        theSync.SyncLong(aPurchase);

    theSync.SyncLong(mPlayTimeActivePlayer);
    theSync.SyncLong(mPlayTimeInactivePlayer);
    theSync.SyncLong(mHasUsedCheatKeys);
    theSync.SyncLong(mHasWokenStinky);
    theSync.SyncLong(mDidntPurchasePacketUpgrade);
    theSync.SyncLong(mLastStinkyChocolateTime);
    theSync.SyncLong(mStinkyPosX);
    theSync.SyncLong(mStinkyPosY);
    theSync.SyncLong(mHasUnlockedMinigames);
    theSync.SyncLong(mHasUnlockedPuzzleMode);
    theSync.SyncLong(mHasNewMiniGame);
    theSync.SyncLong(mHasNewScaryPotter);
    theSync.SyncLong(mHasNewIZombie);
    theSync.SyncLong(mHasNewSurvival);
    theSync.SyncLong(mHasUnlockedSurvivalMode);
    theSync.SyncLong(mNeedsMessageOnGameSelector);
    theSync.SyncLong(mNeedsMagicTacoReward);
    theSync.SyncLong(mHasSeenStinky);
    theSync.SyncLong(mHasSeenUpsell);
    theSync.SyncLong(mPlaceHolderPlayerStats);

    theSync.SyncLong(mNumPottedPlants);
    ValidateSerializedCount(
        theSync,
        mNumPottedPlants,
        MAX_POTTED_PLANTS,
        "potted plant count is outside the profile format");
    for (std::int32_t anIndex = 0; anIndex < mNumPottedPlants; ++anIndex)
        SyncPottedPlant(theSync, mPottedPlant[anIndex]);

    for (auto& anAchievement : mEarnedAchievements)
        theSync.SyncBool(anAchievement);
    for (auto& anAchievement : mShownAchievements)
        theSync.SyncBool(anAchievement);

    theSync.SyncBool(mAcceptedZombatarULA);
    theSync.SyncLong(mNumZombatars);
    ValidateSerializedCount(
        theSync,
        mNumZombatars,
        MAX_NUM_ZOMBATARS,
        "Zombatar count is outside the profile format");
    for (std::int32_t anIndex = 0; anIndex < mNumZombatars; ++anIndex)
        SyncZombatar(theSync, mZombatars[anIndex]);

    for (auto& aCompletedState : mMiniGamesCompleted)
        theSync.SyncBool(aCompletedState);

    theSync.SyncBool(mShownZombatarDesktopMessage);
}

void PlayerInfo::Reset()
{
    mLevel = 1;
    mCoins = 0;
    mFinishedAdventure = 0;
    std::memset(mChallengeRecords, 0, sizeof(mChallengeRecords));
    std::memset(mPurchases, 0, sizeof(mPurchases));
    mPlayTimeActivePlayer = 0;
    mPlayTimeInactivePlayer = 0;
    mHasUsedCheatKeys = 0;
    mHasWokenStinky = 0;
    mDidntPurchasePacketUpgrade = 0;
    mLastStinkyChocolateTime = 0;
    mStinkyPosX = 0;
    mStinkyPosY = 0;
    mHasUnlockedMinigames = 0;
    mHasUnlockedPuzzleMode = 0;
    mHasNewMiniGame = 0;
    mHasNewScaryPotter = 0;
    mHasNewIZombie = 0;
    mHasNewSurvival = 0;
    mHasUnlockedSurvivalMode = 0;
    mNeedsMessageOnGameSelector = 0;
    mNeedsMagicTacoReward = 0;
    mHasSeenStinky = 0;
    mHasSeenUpsell = 0;
    mAcceptedZombatarULA = false;
    mPlaceHolderPlayerStats = 0;

    for (std::size_t anIndex = 0; anIndex < 20; ++anIndex)
    {
        mEarnedAchievements[anIndex] = false;
        mShownAchievements[anIndex] = false;
    }

    mNumZombatars = 0;

    std::memset(mPottedPlant, 0, sizeof(mPottedPlant));
    mNumPottedPlants = 0;
    std::memset(mMiniGamesCompleted, 0, sizeof(mMiniGamesCompleted));
    mShownZombatarDesktopMessage = false;
}
