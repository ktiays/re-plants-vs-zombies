#include "Lawn/System/LegacySaveFormat.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

void Expect(bool theCondition, const char* theMessage);

namespace
{

void TestLegacySaveHeaderGoldenBytes()
{
    const SaveFileHeader aHeader{
        .mMagicNumber = kLegacySaveMagic,
        .mBuildVersion = kLegacySaveVersion,
        .mBuildDate = 0x12345678,
    };
    const auto aBytes = EncodeLegacySaveHeader(aHeader);
    constexpr std::array<std::uint8_t, kLegacySaveHeaderSize> kExpected{
        0xAD, 0xDE, 0xED, 0xFE,
        0x02, 0x00, 0x00, 0x00,
        0x78, 0x56, 0x34, 0x12,
    };

    for (std::size_t anIndex = 0; anIndex < kExpected.size(); ++anIndex)
    {
        Expect(
            std::to_integer<std::uint8_t>(aBytes[anIndex]) ==
                kExpected[anIndex],
            "legacy save header golden bytes");
    }
}

void TestLegacySaveHeaderRoundTrip()
{
    const SaveFileHeader aSource{
        .mMagicNumber = kLegacySaveMagic,
        .mBuildVersion = kLegacySaveVersion,
        .mBuildDate = 0x89ABCDEF,
    };
    const auto aBytes = EncodeLegacySaveHeader(aSource);

    SaveFileHeader aResult;
    Expect(
        DecodeLegacySaveHeader(aBytes, aResult),
        "legacy save header decodes");
    Expect(
        aResult.mMagicNumber == aSource.mMagicNumber,
        "legacy save magic round-trips");
    Expect(
        aResult.mBuildVersion == aSource.mBuildVersion,
        "legacy save version round-trips");
    Expect(
        aResult.mBuildDate == aSource.mBuildDate,
        "legacy save date round-trips");
}

void TestLegacySaveHeaderRejectsWrongSize()
{
    const auto aBytes = EncodeLegacySaveHeader({});
    SaveFileHeader aDestination{
        .mMagicNumber = 1,
        .mBuildVersion = 2,
        .mBuildDate = 3,
    };
    Expect(
        !DecodeLegacySaveHeader(
            std::span<const std::byte>(aBytes).first(
                kLegacySaveHeaderSize - 1),
            aDestination),
        "truncated legacy save header fails");
    Expect(
        aDestination.mMagicNumber == 1 &&
            aDestination.mBuildVersion == 2 &&
            aDestination.mBuildDate == 3,
        "failed legacy save header decode preserves destination");
}

void TestLegacySaveStreamGoldenBytes()
{
    LegacySaveWriter aWriter;
    const std::array<std::byte, 3> aBlock{
        std::byte{0x10},
        std::byte{0x20},
        std::byte{0x30},
    };

    Expect(aWriter.WriteI32(-2), "writes signed save value");
    Expect(
        aWriter.WriteU32(0x89ABCDEF),
        "writes unsigned save value");
    Expect(aWriter.WriteF32(1.0F), "writes float save value");
    Expect(aWriter.WriteBool(true), "writes bool save value");
    Expect(aWriter.WriteBlock(aBlock), "writes length-prefixed block");

    constexpr std::array<std::uint8_t, 20> kExpected{
        0xFE, 0xFF, 0xFF, 0xFF,
        0xEF, 0xCD, 0xAB, 0x89,
        0x00, 0x00, 0x80, 0x3F,
        0x01,
        0x03, 0x00, 0x00, 0x00,
        0x10, 0x20, 0x30,
    };
    const auto aBytes = aWriter.GetBytes();
    Expect(aBytes.size() == kExpected.size(), "save stream byte size");
    for (std::size_t anIndex = 0; anIndex < kExpected.size(); ++anIndex)
    {
        Expect(
            std::to_integer<std::uint8_t>(aBytes[anIndex]) ==
                kExpected[anIndex],
            "save stream golden bytes");
    }
}

void TestLegacySaveStreamRoundTrip()
{
    LegacySaveWriter aWriter;
    const std::array<std::byte, 3> aSourceBlock{
        std::byte{0x10},
        std::byte{0x20},
        std::byte{0x30},
    };
    Expect(aWriter.WriteI32(-2), "writes round-trip signed value");
    Expect(
        aWriter.WriteU32(0x89ABCDEF),
        "writes round-trip unsigned value");
    Expect(aWriter.WriteF32(1.0F), "writes round-trip float value");
    Expect(aWriter.WriteBool(true), "writes round-trip bool value");
    Expect(
        aWriter.WriteBlock(aSourceBlock),
        "writes round-trip block");

    LegacySaveReader aReader(aWriter.GetBytes());
    std::int32_t aSignedValue{};
    std::uint32_t anUnsignedValue{};
    float aFloatValue{};
    bool aBoolValue{};
    std::array<std::byte, 3> aResultBlock{};

    Expect(aReader.ReadI32(aSignedValue), "reads signed save value");
    Expect(aReader.ReadU32(anUnsignedValue), "reads unsigned save value");
    Expect(aReader.ReadF32(aFloatValue), "reads float save value");
    Expect(aReader.ReadBool(aBoolValue), "reads bool save value");
    Expect(aReader.ReadBlock(aResultBlock), "reads save block");
    Expect(aSignedValue == -2, "signed save value round-trips");
    Expect(
        anUnsignedValue == 0x89ABCDEF,
        "unsigned save value round-trips");
    Expect(
        std::bit_cast<std::uint32_t>(aFloatValue) ==
            std::bit_cast<std::uint32_t>(1.0F),
        "float save value round-trips");
    Expect(aBoolValue, "bool save value round-trips");
    Expect(aResultBlock == aSourceBlock, "save block round-trips");
    Expect(aReader.GetBytesRemaining() == 0, "save reader reaches end");
    Expect(
        aReader.GetError() == LegacySaveIoError::None,
        "round-trip save stream has no error");
}

void TestLegacySaveStreamRejectsTruncatedInput()
{
    constexpr std::array<std::byte, 3> kTruncated{
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
    };
    LegacySaveReader aReader(kTruncated);
    std::uint32_t aDestination = 0x12345678;

    Expect(
        !aReader.ReadU32(aDestination),
        "truncated scalar read fails");
    Expect(
        aDestination == 0x12345678,
        "truncated scalar read preserves destination");
    Expect(
        aReader.GetError() == LegacySaveIoError::EndOfInput,
        "truncated scalar reports end of input");
}

void TestLegacySaveStreamRejectsInvalidBool()
{
    constexpr std::array<std::byte, 1> kInvalidBool{
        std::byte{0x02},
    };
    LegacySaveReader aReader(kInvalidBool);
    bool aDestination = true;

    Expect(!aReader.ReadBool(aDestination), "invalid bool read fails");
    Expect(aDestination, "invalid bool read preserves destination");
    Expect(
        aReader.GetError() == LegacySaveIoError::InvalidValue,
        "invalid bool reports invalid value");
}

void TestLegacySaveStreamRejectsWrongBlockSize()
{
    constexpr std::array<std::byte, 7> kWrongSize{
        std::byte{0x02},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0xAA},
        std::byte{0xBB},
        std::byte{0xCC},
    };
    LegacySaveReader aReader(kWrongSize);
    std::array<std::byte, 3> aDestination{
        std::byte{0x10},
        std::byte{0x20},
        std::byte{0x30},
    };
    const auto aBefore = aDestination;

    Expect(
        !aReader.ReadBlock(aDestination),
        "wrong save block size fails");
    Expect(
        aDestination == aBefore,
        "wrong save block size preserves destination");
    Expect(
        aReader.GetError() ==
            LegacySaveIoError::BlockSizeMismatch,
        "wrong save block size reports schema mismatch");
}

void TestLegacySaveStreamReset()
{
    LegacySaveWriter aWriter;
    Expect(aWriter.WriteU32(1), "writes value before writer reset");
    aWriter.Reset();
    Expect(aWriter.GetBytes().empty(), "writer reset clears bytes");
    Expect(
        aWriter.GetError() == LegacySaveIoError::None,
        "writer reset clears error");

    constexpr std::array<std::byte, 4> kValue{
        std::byte{0x78},
        std::byte{0x56},
        std::byte{0x34},
        std::byte{0x12},
    };
    LegacySaveReader aReader;
    aReader.Reset(kValue);
    std::uint32_t aValue{};
    Expect(aReader.ReadU32(aValue), "reads value after reader reset");
    Expect(aValue == 0x12345678, "reader reset uses new input");
}

void TestLegacyCursorRecords()
{
    const LegacyGameObjectState aGameObject{
        .mX = -1,
        .mY = 2,
        .mWidth = 80,
        .mHeight = 81,
        .mVisible = true,
        .mRow = 4,
        .mRenderOrder = 100,
    };
    const LegacyCursorObjectState aSource{
        .mGameObject = aGameObject,
        .mSeedBankIndex = 3,
        .mType = 4,
        .mImitaterType = 5,
        .mCursorType = 6,
        .mCoinId = 0xF0000001,
        .mGlovePlantId = 0xF0000002,
        .mDuplicatorPlantId = 0xF0000003,
        .mCobCannonPlantId = 0xF0000004,
        .mHammerDownCounter = 7,
        .mReanimCursorId = 0xF0000005,
    };
    auto aBytes = EncodeLegacyCursorObjectState(aSource);

    Expect(
        aBytes.size() == kLegacyCursorObjectRecordSize,
        "cursor object record has Windows wire size");
    for (std::size_t anIndex = 0; anIndex < 8; ++anIndex)
    {
        Expect(
            aBytes[anIndex] == std::byte{0},
            "cursor object pointer slots are zero");
    }
    Expect(
        aBytes[8] == std::byte{0xFF} &&
            aBytes[9] == std::byte{0xFF} &&
            aBytes[10] == std::byte{0xFF} &&
            aBytes[11] == std::byte{0xFF},
        "cursor object signed field is little-endian");
    Expect(
        aBytes[24] == std::byte{1},
        "cursor object bool has one-byte wire representation");
    Expect(
        aBytes[25] == std::byte{0} &&
            aBytes[26] == std::byte{0} &&
            aBytes[27] == std::byte{0},
        "cursor object alignment slots are deterministic");

    LegacyCursorObjectState aResult;
    Expect(
        DecodeLegacyCursorObjectState(aBytes, aResult),
        "cursor object record decodes");
    Expect(aResult.mGameObject.mX == -1, "cursor x round-trips");
    Expect(aResult.mGameObject.mVisible, "cursor visibility round-trips");
    Expect(aResult.mGameObject.mRow == 4, "cursor row round-trips");
    Expect(aResult.mSeedBankIndex == 3, "cursor seed index round-trips");
    Expect(aResult.mType == 4, "cursor seed type round-trips");
    Expect(aResult.mCursorType == 6, "cursor type round-trips");
    Expect(aResult.mCoinId == 0xF0000001, "cursor coin id round-trips");
    Expect(
        aResult.mReanimCursorId == 0xF0000005,
        "cursor reanimation id round-trips");

    aBytes[0] = std::byte{0xAA};
    aBytes[25] = std::byte{0xBB};
    Expect(
        DecodeLegacyCursorObjectState(aBytes, aResult),
        "cursor decoder accepts ignored legacy pointer and padding bytes");

    const LegacyCursorPreviewState aPreviewSource{
        .mGameObject = aGameObject,
        .mGridX = 8,
        .mGridY = 9,
    };
    const auto aPreviewBytes =
        EncodeLegacyCursorPreviewState(aPreviewSource);
    LegacyCursorPreviewState aPreviewResult;
    Expect(
        aPreviewBytes.size() == kLegacyCursorPreviewRecordSize,
        "cursor preview record has Windows wire size");
    Expect(
        DecodeLegacyCursorPreviewState(
            aPreviewBytes,
            aPreviewResult),
        "cursor preview record decodes");
    Expect(aPreviewResult.mGridX == 8, "preview grid x round-trips");
    Expect(aPreviewResult.mGridY == 9, "preview grid y round-trips");
}

void TestLegacyCursorRecordRejectsInvalidBool()
{
    auto aBytes = EncodeLegacyCursorObjectState({});
    aBytes[24] = std::byte{2};
    LegacyCursorObjectState aDestination{
        .mSeedBankIndex = 42,
    };

    Expect(
        !DecodeLegacyCursorObjectState(aBytes, aDestination),
        "cursor record rejects invalid bool");
    Expect(
        aDestination.mSeedBankIndex == 42,
        "failed cursor decode preserves destination");
}

void TestLegacyMusicRecord()
{
    const LegacyMusicState aSource{
        .mCurMusicTune = -1,
        .mCurMusicFileMain = 1,
        .mCurMusicFileDrums = 2,
        .mCurMusicFileHihats = 3,
        .mBurstOverride = 4,
        .mBaseBpm = 120.5F,
        .mBaseModSpeed = 1.25F,
        .mMusicBurstState = 2,
        .mBurstStateCounter = 5,
        .mMusicDrumsState = 3,
        .mQueuedDrumTrackPackedOrder = 6,
        .mDrumsStateCounter = 7,
        .mPauseOffset = 8,
        .mPauseOffsetDrums = 9,
        .mPaused = true,
        .mMusicDisabled = false,
        .mFadeOutCounter = 10,
        .mFadeOutDuration = 11,
    };
    const auto aBytes = EncodeLegacyMusicState(aSource);

    Expect(
        aBytes.size() == kLegacyMusicRecordSize,
        "music record has Windows wire size");
    for (std::size_t anIndex = 0; anIndex < 8; ++anIndex)
    {
        Expect(
            aBytes[anIndex] == std::byte{0},
            "music pointer slots are zero");
    }
    Expect(
        aBytes[64] == std::byte{1} &&
            aBytes[65] == std::byte{0},
        "music bools use one byte each");
    Expect(
        aBytes[66] == std::byte{0} &&
            aBytes[67] == std::byte{0},
        "music alignment slots are deterministic");

    LegacyMusicState aResult;
    Expect(
        DecodeLegacyMusicState(aBytes, aResult),
        "music record decodes");
    Expect(aResult.mCurMusicTune == -1, "music tune round-trips");
    Expect(
        aResult.mCurMusicFileHihats == 3,
        "music file round-trips");
    Expect(
        std::bit_cast<std::uint32_t>(aResult.mBaseBpm) ==
            std::bit_cast<std::uint32_t>(120.5F),
        "music bpm round-trips");
    Expect(
        std::bit_cast<std::uint32_t>(aResult.mBaseModSpeed) ==
            std::bit_cast<std::uint32_t>(1.25F),
        "music mod speed round-trips");
    Expect(aResult.mPaused, "music paused state round-trips");
    Expect(!aResult.mMusicDisabled, "music disabled state round-trips");
    Expect(
        aResult.mFadeOutDuration == 11,
        "music fade duration round-trips");
}

void TestLegacyMessageRecord()
{
    LegacyMessageState aSource;
    aSource.mLabel[0] = static_cast<std::uint8_t>('H');
    aSource.mLabel[1] = static_cast<std::uint8_t>('i');
    aSource.mDisplayTime = 10;
    aSource.mDuration = 20;
    aSource.mMessageStyle = 3;
    aSource.mTextReanimIds[0] = 0xF0000001;
    aSource.mTextReanimIds[127] = 0xF0000002;
    aSource.mReanimType = 4;
    aSource.mSlideOffTime = 30;
    aSource.mLabelNext[0] = static_cast<std::uint8_t>('N');
    aSource.mMessageStyleNext = 5;

    auto aBytes = EncodeLegacyMessageState(aSource);
    Expect(
        aBytes.size() == kLegacyMessageRecordSize,
        "message record has Windows wire size");
    Expect(
        aBytes[0] == std::byte{0} &&
            aBytes[1] == std::byte{0} &&
            aBytes[2] == std::byte{0} &&
            aBytes[3] == std::byte{0},
        "message app pointer slot is zero");
    Expect(
        aBytes[4] == std::byte{'H'} &&
            aBytes[5] == std::byte{'i'},
        "message text uses fixed bytes");

    LegacyMessageState aResult;
    Expect(
        DecodeLegacyMessageState(aBytes, aResult),
        "message record decodes");
    Expect(
        aResult.mLabel[0] == static_cast<std::uint8_t>('H') &&
            aResult.mLabel[1] == static_cast<std::uint8_t>('i'),
        "message label round-trips");
    Expect(aResult.mDisplayTime == 10, "message display time round-trips");
    Expect(aResult.mDuration == 20, "message duration round-trips");
    Expect(aResult.mMessageStyle == 3, "message style round-trips");
    Expect(
        aResult.mTextReanimIds[127] == 0xF0000002,
        "message reanimation ids round-trip");
    Expect(aResult.mReanimType == 4, "message reanimation type round-trips");
    Expect(
        aResult.mLabelNext[0] == static_cast<std::uint8_t>('N'),
        "next message label round-trips");
    Expect(
        aResult.mMessageStyleNext == 5,
        "next message style round-trips");

    aBytes[0] = std::byte{0xAA};
    Expect(
        DecodeLegacyMessageState(aBytes, aResult),
        "message decoder accepts ignored legacy app pointer");
}

void TestLegacySeedBankRecord()
{
    LegacySeedBankState aSource;
    aSource.mGameObject.mX = 10;
    aSource.mGameObject.mVisible = true;
    aSource.mNumPackets = 5;
    aSource.mSeedPackets[0].mGameObject.mX = 20;
    aSource.mSeedPackets[0].mGameObject.mVisible = true;
    aSource.mSeedPackets[0].mRefreshCounter = 30;
    aSource.mSeedPackets[0].mPacketType = 4;
    aSource.mSeedPackets[0].mSlotMachiningPosition = 1.5F;
    aSource.mSeedPackets[0].mActive = true;
    aSource.mSeedPackets[0].mRefreshing = false;
    aSource.mSeedPackets[0].mTimesUsed = 40;
    aSource.mSeedPackets[9].mIndex = 9;
    aSource.mCutSceneDarken = 50;
    aSource.mConveyorBeltCounter = 60;

    const auto aBytes = EncodeLegacySeedBankState(aSource);
    Expect(
        aBytes.size() == kLegacySeedBankRecordSize,
        "seed bank record has Windows wire size");
    Expect(
        kLegacySeedPacketRecordSize == 80,
        "seed packet wire size remains explicit");
    Expect(
        aBytes[24] == std::byte{1},
        "seed bank visibility uses one byte");
    Expect(
        aBytes[40 + 72] == std::byte{1} &&
            aBytes[40 + 73] == std::byte{0},
        "seed packet bools use one byte each");
    Expect(
        aBytes[40 + 74] == std::byte{0} &&
            aBytes[40 + 75] == std::byte{0},
        "seed packet alignment slots are deterministic");

    LegacySeedBankState aResult;
    Expect(
        DecodeLegacySeedBankState(aBytes, aResult),
        "seed bank record decodes");
    Expect(aResult.mNumPackets == 5, "seed bank packet count round-trips");
    Expect(
        aResult.mSeedPackets[0].mRefreshCounter == 30,
        "seed packet refresh counter round-trips");
    Expect(
        std::bit_cast<std::uint32_t>(
            aResult.mSeedPackets[0].mSlotMachiningPosition) ==
            std::bit_cast<std::uint32_t>(1.5F),
        "seed packet position round-trips");
    Expect(
        aResult.mSeedPackets[0].mActive,
        "seed packet active state round-trips");
    Expect(
        aResult.mSeedPackets[9].mIndex == 9,
        "last seed packet round-trips");
    Expect(
        aResult.mConveyorBeltCounter == 60,
        "seed bank conveyor counter round-trips");
}

} // namespace

void RunLegacySaveFormatTests()
{
    TestLegacySaveHeaderGoldenBytes();
    TestLegacySaveHeaderRoundTrip();
    TestLegacySaveHeaderRejectsWrongSize();
    TestLegacySaveStreamGoldenBytes();
    TestLegacySaveStreamRoundTrip();
    TestLegacySaveStreamRejectsTruncatedInput();
    TestLegacySaveStreamRejectsInvalidBool();
    TestLegacySaveStreamRejectsWrongBlockSize();
    TestLegacySaveStreamReset();
    TestLegacyCursorRecords();
    TestLegacyCursorRecordRejectsInvalidBool();
    TestLegacyMusicRecord();
    TestLegacyMessageRecord();
    TestLegacySeedBankRecord();
}
