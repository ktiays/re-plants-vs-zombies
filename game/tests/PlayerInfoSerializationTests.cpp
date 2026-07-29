#include "Lawn/System/DataSync.h"
#include "Lawn/System/PlayerInfo.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

void Expect(bool theCondition, const char* theMessage);

namespace
{

constexpr std::size_t kPottedPlantWireSize = 88;
constexpr std::size_t kZombatarWireSize = 72;
constexpr std::size_t kFirstPottedPlantOffset = 820;
constexpr std::size_t kPottedPlantFirstReservedOffset =
    kFirstPottedPlantOffset + 20;
constexpr std::size_t kPottedPlantSecondReservedOffset =
    kFirstPottedPlantOffset + 52;
constexpr std::array<std::int32_t Zombatar::*, 18> kZombatarFields{
    &Zombatar::mSkin,
    &Zombatar::mSkinColor,
    &Zombatar::mClothes,
    &Zombatar::mClothesColor,
    &Zombatar::mTidbits,
    &Zombatar::mTidbitsColor,
    &Zombatar::mAccessories,
    &Zombatar::mAccessoriesColor,
    &Zombatar::mFacialHair,
    &Zombatar::mFacialHairColor,
    &Zombatar::mHair,
    &Zombatar::mHairColor,
    &Zombatar::mEyewear,
    &Zombatar::mEyewearColor,
    &Zombatar::mHat,
    &Zombatar::mHatColor,
    &Zombatar::mBackdrop,
    &Zombatar::mBackdropColor,
};

void FillProfileRecords(PlayerInfo& theProfile)
{
    theProfile.mNumPottedPlants = 1;
    auto& aPlant = theProfile.mPottedPlant[0];
    aPlant.mSeedType = SeedType::SEED_SUNFLOWER;
    aPlant.mWhichZenGarden = GardenType::GARDEN_MUSHROOM;
    aPlant.mX = -17;
    aPlant.mY = 29;
    aPlant.mFacing = PottedPlant::FacingDirection::FACING_LEFT;
    aPlant.mLastWateredTime = INT64_C(0x0123456789ABCDEF);
    aPlant.mDrawVariation = DrawVariation::VARIATION_ZEN_GARDEN;
    aPlant.mPlantAge = PottedPlantAge::PLANTAGE_MEDIUM;
    aPlant.mTimesFed = 4;
    aPlant.mFeedingsPerGrow = 5;
    aPlant.mPlantNeed = PottedPlantNeed::PLANTNEED_PHONOGRAPH;
    aPlant.mLastNeedFulfilledTime = -2;
    aPlant.mLastFertilizedTime = INT64_C(0x1020304050607080);
    aPlant.mLastChocolateTime = -3;
    aPlant.mFutureAttribute[0] = INT64_C(0x1112131415161718);

    theProfile.mNumZombatars = 1;
    auto& aZombatar = theProfile.mZombatars[0];
    std::int32_t aValue = -9;
    for (auto aField : kZombatarFields)
        aZombatar.*aField = aValue++;
}

void TestPlayerProfileRecordRoundTrip()
{
    PlayerInfo aSource;
    FillProfileRecords(aSource);
    aSource.mEarnedAchievements[3] = true;
    aSource.mShownAchievements[5] = true;
    aSource.mAcceptedZombatarULA = true;
    aSource.mMiniGamesCompleted[7] = true;
    aSource.mShownZombatarDesktopMessage = true;

    DataWriter aWriter;
    aWriter.OpenMemory();
    DataSync aWriteSync(aWriter);
    aSource.SyncDetails(aWriteSync);

    constexpr std::size_t kExpectedWireSize =
        kFirstPottedPlantOffset +
        kPottedPlantWireSize +
        20 +
        20 +
        1 +
        4 +
        kZombatarWireSize +
        20 +
        1;
    Expect(
        aWriter.GetDataLen() == kExpectedWireSize,
        "player profile uses stable record byte sizes");

    const auto* aBytes =
        static_cast<const std::uint8_t*>(aWriter.GetDataPtr());
    for (std::size_t anIndex = 0; anIndex < 4; ++anIndex)
    {
        Expect(
            aBytes[kPottedPlantFirstReservedOffset + anIndex] == 0,
            "first legacy potted-plant padding word is explicit");
        Expect(
            aBytes[kPottedPlantSecondReservedOffset + anIndex] == 0,
            "second legacy potted-plant padding word is explicit");
    }

    DataReader aReader;
    aReader.OpenMemory(
        aWriter.GetDataPtr(),
        aWriter.GetDataLen(),
        false);
    DataSync aReadSync(aReader);
    PlayerInfo aResult;
    aResult.SyncDetails(aReadSync);

    Expect(aResult.mNumPottedPlants == 1, "potted-plant count round-trips");
    const auto& aPlant = aResult.mPottedPlant[0];
    Expect(aPlant.mSeedType == SeedType::SEED_SUNFLOWER, "plant seed");
    Expect(aPlant.mWhichZenGarden == GardenType::GARDEN_MUSHROOM, "garden");
    Expect(aPlant.mX == -17 && aPlant.mY == 29, "plant position");
    Expect(
        aPlant.mFacing == PottedPlant::FacingDirection::FACING_LEFT,
        "plant facing");
    Expect(
        aPlant.mLastWateredTime == INT64_C(0x0123456789ABCDEF),
        "plant 64-bit timestamp");
    Expect(
        aPlant.mLastNeedFulfilledTime == -2 &&
            aPlant.mLastChocolateTime == -3,
        "plant signed timestamps");
    Expect(
        aPlant.mFutureAttribute[0] == INT64_C(0x1112131415161718),
        "plant future attribute");

    Expect(aResult.mNumZombatars == 1, "Zombatar count round-trips");
    std::int32_t aExpectedValue = -9;
    for (auto aField : kZombatarFields)
    {
        Expect(
            aResult.mZombatars[0].*aField == aExpectedValue++,
            "Zombatar fixed-width field round-trips");
    }

    Expect(aResult.mEarnedAchievements[3], "earned achievement");
    Expect(aResult.mShownAchievements[5], "shown achievement");
    Expect(aResult.mAcceptedZombatarULA, "Zombatar EULA");
    Expect(aResult.mMiniGamesCompleted[7], "minigame completion");
    Expect(
        aResult.mShownZombatarDesktopMessage,
        "Zombatar desktop message");
}

void TestPlayerProfileRejectsInvalidCounts()
{
    PlayerInfo aProfile;
    DataWriter aWriter;
    aWriter.OpenMemory();
    DataSync aWriteSync(aWriter);
    aProfile.SyncDetails(aWriteSync);

    aWriter.SetLong(
        static_cast<std::uint32_t>(MAX_POTTED_PLANTS + 1),
        kFirstPottedPlantOffset - sizeof(std::uint32_t));

    DataReader aReader;
    aReader.OpenMemory(
        aWriter.GetDataPtr(),
        aWriter.GetDataLen(),
        false);
    DataSync aReadSync(aReader);

    bool aThrew = false;
    try
    {
        PlayerInfo aResult;
        aResult.SyncDetails(aReadSync);
    }
    catch (const DataReaderException&)
    {
        aThrew = true;
    }
    Expect(aThrew, "invalid potted-plant count is rejected");

    aProfile.mNumZombatars = MAX_NUM_ZOMBATARS + 1;
    bool aWriterThrew = false;
    try
    {
        DataWriter anInvalidWriter;
        anInvalidWriter.OpenMemory();
        DataSync anInvalidSync(anInvalidWriter);
        aProfile.SyncDetails(anInvalidSync);
    }
    catch (const std::out_of_range&)
    {
        aWriterThrew = true;
    }
    Expect(aWriterThrew, "invalid Zombatar count is rejected on write");
}

void TestPlayerSummaryRoundTrip()
{
    PlayerInfo aSource;
    aSource.mName = "Portable Player";
    aSource.mUseSeq = UINT32_C(0x89ABCDEF);
    aSource.mId = UINT32_C(0x10203040);

    DataWriter aWriter;
    aWriter.OpenMemory();
    DataSync aWriteSync(aWriter);
    aSource.SyncSummary(aWriteSync);

    DataReader aReader;
    aReader.OpenMemory(
        aWriter.GetDataPtr(),
        aWriter.GetDataLen(),
        false);
    DataSync aReadSync(aReader);
    PlayerInfo aResult;
    aResult.SyncSummary(aReadSync);

    Expect(aResult.mName == aSource.mName, "profile name round-trips");
    Expect(aResult.mUseSeq == aSource.mUseSeq, "profile use sequence");
    Expect(aResult.mId == aSource.mId, "profile identifier");
}

} // namespace

void RunPlayerInfoSerializationTests()
{
    TestPlayerProfileRecordRoundTrip();
    TestPlayerProfileRejectsInvalidCounts();
    TestPlayerSummaryRoundTrip();
}
