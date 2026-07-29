#include "LegacySaveFormat.h"

#include <bit>
#include <cstring>
#include <limits>

namespace
{

void EncodeU32(
    std::array<std::byte, kLegacySaveHeaderSize>& theBytes,
    std::size_t theOffset,
    std::uint32_t theValue)
{
    for (std::uint32_t anIndex = 0; anIndex < 4; ++anIndex)
    {
        theBytes[theOffset + anIndex] =
            static_cast<std::byte>(theValue >> (anIndex * 8));
    }
}

void AppendU32(
    std::vector<std::byte>& theBytes,
    std::uint32_t theValue)
{
    for (std::uint32_t anIndex = 0; anIndex < 4; ++anIndex)
    {
        theBytes.push_back(
            static_cast<std::byte>(theValue >> (anIndex * 8)));
    }
}

[[nodiscard]] std::uint32_t DecodeU32(
    std::span<const std::byte> theBytes,
    std::size_t theOffset)
{
    std::uint32_t aValue{};
    for (std::uint32_t anIndex = 0; anIndex < 4; ++anIndex)
    {
        aValue |=
            static_cast<std::uint32_t>(
                std::to_integer<std::uint8_t>(
                    theBytes[theOffset + anIndex]))
            << (anIndex * 8);
    }
    return aValue;
}

[[nodiscard]] bool WriteReservedPointers(
    LegacySaveWriter& theWriter)
{
    return theWriter.WriteU32(0) &&
           theWriter.WriteU32(0);
}

[[nodiscard]] bool ReadReservedPointers(
    LegacySaveReader& theReader)
{
    std::uint32_t anIgnoredValue{};
    return theReader.ReadU32(anIgnoredValue) &&
           theReader.ReadU32(anIgnoredValue);
}

[[nodiscard]] bool WriteGameObjectState(
    LegacySaveWriter& theWriter,
    const LegacyGameObjectState& theState)
{
    return WriteReservedPointers(theWriter) &&
           theWriter.WriteI32(theState.mX) &&
           theWriter.WriteI32(theState.mY) &&
           theWriter.WriteI32(theState.mWidth) &&
           theWriter.WriteI32(theState.mHeight) &&
           theWriter.WriteBool(theState.mVisible) &&
           theWriter.WriteU8(0) &&
           theWriter.WriteU8(0) &&
           theWriter.WriteU8(0) &&
           theWriter.WriteI32(theState.mRow) &&
           theWriter.WriteI32(theState.mRenderOrder);
}

[[nodiscard]] bool ReadGameObjectState(
    LegacySaveReader& theReader,
    LegacyGameObjectState& theState)
{
    LegacyGameObjectState aState;
    std::uint8_t anIgnoredByte{};
    if (!ReadReservedPointers(theReader) ||
        !theReader.ReadI32(aState.mX) ||
        !theReader.ReadI32(aState.mY) ||
        !theReader.ReadI32(aState.mWidth) ||
        !theReader.ReadI32(aState.mHeight) ||
        !theReader.ReadBool(aState.mVisible) ||
        !theReader.ReadU8(anIgnoredByte) ||
        !theReader.ReadU8(anIgnoredByte) ||
        !theReader.ReadU8(anIgnoredByte) ||
        !theReader.ReadI32(aState.mRow) ||
        !theReader.ReadI32(aState.mRenderOrder))
    {
        return false;
    }
    theState = aState;
    return true;
}

template <std::size_t Size>
[[nodiscard]] std::array<std::byte, Size> CopyEncodedRecord(
    const LegacySaveWriter& theWriter)
{
    std::array<std::byte, Size> aResult{};
    const auto aBytes = theWriter.GetBytes();
    if (aBytes.size() == aResult.size())
    {
        std::memcpy(aResult.data(), aBytes.data(), aResult.size());
    }
    return aResult;
}

} // namespace

std::array<std::byte, kLegacySaveHeaderSize> EncodeLegacySaveHeader(
    const SaveFileHeader& theHeader)
{
    std::array<std::byte, kLegacySaveHeaderSize> aBytes{};
    EncodeU32(aBytes, 0, theHeader.mMagicNumber);
    EncodeU32(aBytes, 4, theHeader.mBuildVersion);
    EncodeU32(aBytes, 8, theHeader.mBuildDate);
    return aBytes;
}

bool DecodeLegacySaveHeader(
    std::span<const std::byte> theBytes,
    SaveFileHeader& theHeader)
{
    if (theBytes.size() != kLegacySaveHeaderSize)
        return false;

    SaveFileHeader aHeader{
        .mMagicNumber = DecodeU32(theBytes, 0),
        .mBuildVersion = DecodeU32(theBytes, 4),
        .mBuildDate = DecodeU32(theBytes, 8),
    };
    theHeader = aHeader;
    return true;
}

void LegacySaveWriter::Reset()
{
    mBytes.clear();
    mError = LegacySaveIoError::None;
}

bool LegacySaveWriter::WriteU8(std::uint8_t theValue)
{
    if (mError != LegacySaveIoError::None)
        return false;
    mBytes.push_back(static_cast<std::byte>(theValue));
    return true;
}

bool LegacySaveWriter::WriteU32(std::uint32_t theValue)
{
    if (mError != LegacySaveIoError::None)
        return false;
    AppendU32(mBytes, theValue);
    return true;
}

bool LegacySaveWriter::WriteI32(std::int32_t theValue)
{
    return WriteU32(std::bit_cast<std::uint32_t>(theValue));
}

bool LegacySaveWriter::WriteF32(float theValue)
{
    return WriteU32(std::bit_cast<std::uint32_t>(theValue));
}

bool LegacySaveWriter::WriteBool(bool theValue)
{
    return WriteU8(theValue ? std::uint8_t{1} : std::uint8_t{0});
}

bool LegacySaveWriter::WriteBytes(std::span<const std::byte> theBytes)
{
    if (mError != LegacySaveIoError::None)
        return false;
    mBytes.insert(mBytes.end(), theBytes.begin(), theBytes.end());
    return true;
}

bool LegacySaveWriter::WriteBlock(std::span<const std::byte> theBytes)
{
    if (theBytes.size() > std::numeric_limits<std::uint32_t>::max())
    {
        mError = LegacySaveIoError::LengthOverflow;
        return false;
    }
    return WriteU32(static_cast<std::uint32_t>(theBytes.size())) &&
           WriteBytes(theBytes);
}

std::span<const std::byte> LegacySaveWriter::GetBytes() const
{
    return mBytes;
}

LegacySaveIoError LegacySaveWriter::GetError() const
{
    return mError;
}

LegacySaveReader::LegacySaveReader(
    std::span<const std::byte> theBytes)
{
    Reset(theBytes);
}

void LegacySaveReader::Reset(std::span<const std::byte> theBytes)
{
    mBytes = theBytes;
    mOffset = 0;
    mError = LegacySaveIoError::None;
}

bool LegacySaveReader::ReadU8(std::uint8_t& theValue)
{
    if (!CanRead(1))
        return false;
    theValue = std::to_integer<std::uint8_t>(
        mBytes[static_cast<std::size_t>(mOffset)]);
    ++mOffset;
    return true;
}

bool LegacySaveReader::ReadU32(std::uint32_t& theValue)
{
    if (!CanRead(4))
        return false;

    std::uint32_t aValue{};
    for (std::uint32_t anIndex = 0; anIndex < 4; ++anIndex)
    {
        aValue |=
            static_cast<std::uint32_t>(
                std::to_integer<std::uint8_t>(
                    mBytes[static_cast<std::size_t>(
                        mOffset + anIndex)]))
            << (anIndex * 8);
    }
    mOffset += 4;
    theValue = aValue;
    return true;
}

bool LegacySaveReader::ReadI32(std::int32_t& theValue)
{
    std::uint32_t aValue{};
    if (!ReadU32(aValue))
        return false;
    theValue = std::bit_cast<std::int32_t>(aValue);
    return true;
}

bool LegacySaveReader::ReadF32(float& theValue)
{
    std::uint32_t aValue{};
    if (!ReadU32(aValue))
        return false;
    theValue = std::bit_cast<float>(aValue);
    return true;
}

bool LegacySaveReader::ReadBool(bool& theValue)
{
    std::uint8_t aValue{};
    if (!ReadU8(aValue))
        return false;
    if (aValue > 1)
    {
        Fail(LegacySaveIoError::InvalidValue);
        return false;
    }
    theValue = aValue == 1;
    return true;
}

bool LegacySaveReader::ReadBytes(std::span<std::byte> theBytes)
{
    if (!CanRead(static_cast<std::uint64_t>(theBytes.size())))
        return false;
    if (!theBytes.empty())
    {
        std::memcpy(
            theBytes.data(),
            mBytes.data() + static_cast<std::size_t>(mOffset),
            theBytes.size());
    }
    mOffset += static_cast<std::uint64_t>(theBytes.size());
    return true;
}

bool LegacySaveReader::ReadBlock(std::span<std::byte> theBytes)
{
    std::uint32_t aBlockSize{};
    if (!ReadU32(aBlockSize))
        return false;
    if (static_cast<std::uint64_t>(aBlockSize) !=
        static_cast<std::uint64_t>(theBytes.size()))
    {
        Fail(LegacySaveIoError::BlockSizeMismatch);
        return false;
    }
    return ReadBytes(theBytes);
}

std::uint64_t LegacySaveReader::GetBytesRemaining() const
{
    return static_cast<std::uint64_t>(mBytes.size()) - mOffset;
}

LegacySaveIoError LegacySaveReader::GetError() const
{
    return mError;
}

bool LegacySaveReader::CanRead(std::uint64_t theByteCount)
{
    if (mError != LegacySaveIoError::None)
        return false;

    const auto aSize = static_cast<std::uint64_t>(mBytes.size());
    if (mOffset > aSize || theByteCount > aSize - mOffset)
    {
        Fail(LegacySaveIoError::EndOfInput);
        return false;
    }
    return true;
}

void LegacySaveReader::Fail(LegacySaveIoError theError)
{
    if (mError == LegacySaveIoError::None)
        mError = theError;
}

const char* GetLegacySaveIoErrorMessage(LegacySaveIoError theError)
{
    switch (theError)
    {
    case LegacySaveIoError::None:
        return "no error";
    case LegacySaveIoError::EndOfInput:
        return "legacy save input ended unexpectedly";
    case LegacySaveIoError::LengthOverflow:
        return "legacy save length is too large";
    case LegacySaveIoError::BlockSizeMismatch:
        return "legacy save block size does not match the expected schema";
    case LegacySaveIoError::InvalidValue:
        return "legacy save value is invalid";
    }
    return "unknown legacy save error";
}

std::array<std::byte, kLegacyCursorObjectRecordSize>
EncodeLegacyCursorObjectState(const LegacyCursorObjectState& theState)
{
    LegacySaveWriter aWriter;
    const bool aSuccess =
        WriteGameObjectState(aWriter, theState.mGameObject) &&
        aWriter.WriteI32(theState.mSeedBankIndex) &&
        aWriter.WriteI32(theState.mType) &&
        aWriter.WriteI32(theState.mImitaterType) &&
        aWriter.WriteI32(theState.mCursorType) &&
        aWriter.WriteU32(theState.mCoinId) &&
        aWriter.WriteU32(theState.mGlovePlantId) &&
        aWriter.WriteU32(theState.mDuplicatorPlantId) &&
        aWriter.WriteU32(theState.mCobCannonPlantId) &&
        aWriter.WriteI32(theState.mHammerDownCounter) &&
        aWriter.WriteU32(theState.mReanimCursorId);
    if (!aSuccess)
        return {};
    return CopyEncodedRecord<kLegacyCursorObjectRecordSize>(aWriter);
}

bool DecodeLegacyCursorObjectState(
    std::span<const std::byte> theBytes,
    LegacyCursorObjectState& theState)
{
    if (theBytes.size() != kLegacyCursorObjectRecordSize)
        return false;

    LegacySaveReader aReader(theBytes);
    LegacyCursorObjectState aState;
    if (!ReadGameObjectState(aReader, aState.mGameObject) ||
        !aReader.ReadI32(aState.mSeedBankIndex) ||
        !aReader.ReadI32(aState.mType) ||
        !aReader.ReadI32(aState.mImitaterType) ||
        !aReader.ReadI32(aState.mCursorType) ||
        !aReader.ReadU32(aState.mCoinId) ||
        !aReader.ReadU32(aState.mGlovePlantId) ||
        !aReader.ReadU32(aState.mDuplicatorPlantId) ||
        !aReader.ReadU32(aState.mCobCannonPlantId) ||
        !aReader.ReadI32(aState.mHammerDownCounter) ||
        !aReader.ReadU32(aState.mReanimCursorId) ||
        aReader.GetBytesRemaining() != 0)
    {
        return false;
    }
    theState = aState;
    return true;
}

std::array<std::byte, kLegacyCursorPreviewRecordSize>
EncodeLegacyCursorPreviewState(const LegacyCursorPreviewState& theState)
{
    LegacySaveWriter aWriter;
    const bool aSuccess =
        WriteGameObjectState(aWriter, theState.mGameObject) &&
        aWriter.WriteI32(theState.mGridX) &&
        aWriter.WriteI32(theState.mGridY);
    if (!aSuccess)
        return {};
    return CopyEncodedRecord<kLegacyCursorPreviewRecordSize>(aWriter);
}

bool DecodeLegacyCursorPreviewState(
    std::span<const std::byte> theBytes,
    LegacyCursorPreviewState& theState)
{
    if (theBytes.size() != kLegacyCursorPreviewRecordSize)
        return false;

    LegacySaveReader aReader(theBytes);
    LegacyCursorPreviewState aState;
    if (!ReadGameObjectState(aReader, aState.mGameObject) ||
        !aReader.ReadI32(aState.mGridX) ||
        !aReader.ReadI32(aState.mGridY) ||
        aReader.GetBytesRemaining() != 0)
    {
        return false;
    }
    theState = aState;
    return true;
}

std::array<std::byte, kLegacyMusicRecordSize>
EncodeLegacyMusicState(const LegacyMusicState& theState)
{
    LegacySaveWriter aWriter;
    const bool aSuccess =
        WriteReservedPointers(aWriter) &&
        aWriter.WriteI32(theState.mCurMusicTune) &&
        aWriter.WriteI32(theState.mCurMusicFileMain) &&
        aWriter.WriteI32(theState.mCurMusicFileDrums) &&
        aWriter.WriteI32(theState.mCurMusicFileHihats) &&
        aWriter.WriteI32(theState.mBurstOverride) &&
        aWriter.WriteF32(theState.mBaseBpm) &&
        aWriter.WriteF32(theState.mBaseModSpeed) &&
        aWriter.WriteI32(theState.mMusicBurstState) &&
        aWriter.WriteI32(theState.mBurstStateCounter) &&
        aWriter.WriteI32(theState.mMusicDrumsState) &&
        aWriter.WriteI32(theState.mQueuedDrumTrackPackedOrder) &&
        aWriter.WriteI32(theState.mDrumsStateCounter) &&
        aWriter.WriteI32(theState.mPauseOffset) &&
        aWriter.WriteI32(theState.mPauseOffsetDrums) &&
        aWriter.WriteBool(theState.mPaused) &&
        aWriter.WriteBool(theState.mMusicDisabled) &&
        aWriter.WriteU8(0) &&
        aWriter.WriteU8(0) &&
        aWriter.WriteI32(theState.mFadeOutCounter) &&
        aWriter.WriteI32(theState.mFadeOutDuration);
    if (!aSuccess)
        return {};
    return CopyEncodedRecord<kLegacyMusicRecordSize>(aWriter);
}

bool DecodeLegacyMusicState(
    std::span<const std::byte> theBytes,
    LegacyMusicState& theState)
{
    if (theBytes.size() != kLegacyMusicRecordSize)
        return false;

    LegacySaveReader aReader(theBytes);
    LegacyMusicState aState;
    std::uint8_t anIgnoredByte{};
    if (!ReadReservedPointers(aReader) ||
        !aReader.ReadI32(aState.mCurMusicTune) ||
        !aReader.ReadI32(aState.mCurMusicFileMain) ||
        !aReader.ReadI32(aState.mCurMusicFileDrums) ||
        !aReader.ReadI32(aState.mCurMusicFileHihats) ||
        !aReader.ReadI32(aState.mBurstOverride) ||
        !aReader.ReadF32(aState.mBaseBpm) ||
        !aReader.ReadF32(aState.mBaseModSpeed) ||
        !aReader.ReadI32(aState.mMusicBurstState) ||
        !aReader.ReadI32(aState.mBurstStateCounter) ||
        !aReader.ReadI32(aState.mMusicDrumsState) ||
        !aReader.ReadI32(aState.mQueuedDrumTrackPackedOrder) ||
        !aReader.ReadI32(aState.mDrumsStateCounter) ||
        !aReader.ReadI32(aState.mPauseOffset) ||
        !aReader.ReadI32(aState.mPauseOffsetDrums) ||
        !aReader.ReadBool(aState.mPaused) ||
        !aReader.ReadBool(aState.mMusicDisabled) ||
        !aReader.ReadU8(anIgnoredByte) ||
        !aReader.ReadU8(anIgnoredByte) ||
        !aReader.ReadI32(aState.mFadeOutCounter) ||
        !aReader.ReadI32(aState.mFadeOutDuration) ||
        aReader.GetBytesRemaining() != 0)
    {
        return false;
    }
    theState = aState;
    return true;
}

std::array<std::byte, kLegacyMessageRecordSize>
EncodeLegacyMessageState(const LegacyMessageState& theState)
{
    LegacySaveWriter aWriter;
    bool aSuccess = aWriter.WriteU32(0);
    for (const auto aCharacter : theState.mLabel)
        aSuccess = aSuccess && aWriter.WriteU8(aCharacter);
    aSuccess =
        aSuccess &&
        aWriter.WriteI32(theState.mDisplayTime) &&
        aWriter.WriteI32(theState.mDuration) &&
        aWriter.WriteI32(theState.mMessageStyle);
    for (const auto aReanimId : theState.mTextReanimIds)
        aSuccess = aSuccess && aWriter.WriteU32(aReanimId);
    aSuccess =
        aSuccess &&
        aWriter.WriteI32(theState.mReanimType) &&
        aWriter.WriteI32(theState.mSlideOffTime);
    for (const auto aCharacter : theState.mLabelNext)
        aSuccess = aSuccess && aWriter.WriteU8(aCharacter);
    aSuccess =
        aSuccess &&
        aWriter.WriteI32(theState.mMessageStyleNext);

    if (!aSuccess)
        return {};
    return CopyEncodedRecord<kLegacyMessageRecordSize>(aWriter);
}

bool DecodeLegacyMessageState(
    std::span<const std::byte> theBytes,
    LegacyMessageState& theState)
{
    if (theBytes.size() != kLegacyMessageRecordSize)
        return false;

    LegacySaveReader aReader(theBytes);
    LegacyMessageState aState;
    std::uint32_t anIgnoredPointer{};
    if (!aReader.ReadU32(anIgnoredPointer))
        return false;
    for (auto& aCharacter : aState.mLabel)
    {
        if (!aReader.ReadU8(aCharacter))
            return false;
    }
    if (!aReader.ReadI32(aState.mDisplayTime) ||
        !aReader.ReadI32(aState.mDuration) ||
        !aReader.ReadI32(aState.mMessageStyle))
    {
        return false;
    }
    for (auto& aReanimId : aState.mTextReanimIds)
    {
        if (!aReader.ReadU32(aReanimId))
            return false;
    }
    if (!aReader.ReadI32(aState.mReanimType) ||
        !aReader.ReadI32(aState.mSlideOffTime))
    {
        return false;
    }
    for (auto& aCharacter : aState.mLabelNext)
    {
        if (!aReader.ReadU8(aCharacter))
            return false;
    }
    if (!aReader.ReadI32(aState.mMessageStyleNext) ||
        aReader.GetBytesRemaining() != 0)
    {
        return false;
    }

    theState = aState;
    return true;
}

std::array<std::byte, kLegacySeedBankRecordSize>
EncodeLegacySeedBankState(const LegacySeedBankState& theState)
{
    LegacySaveWriter aWriter;
    bool aSuccess =
        WriteGameObjectState(aWriter, theState.mGameObject) &&
        aWriter.WriteI32(theState.mNumPackets);
    for (const auto& aPacket : theState.mSeedPackets)
    {
        aSuccess =
            aSuccess &&
            WriteGameObjectState(aWriter, aPacket.mGameObject) &&
            aWriter.WriteI32(aPacket.mRefreshCounter) &&
            aWriter.WriteI32(aPacket.mRefreshTime) &&
            aWriter.WriteI32(aPacket.mIndex) &&
            aWriter.WriteI32(aPacket.mOffsetX) &&
            aWriter.WriteI32(aPacket.mPacketType) &&
            aWriter.WriteI32(aPacket.mImitaterType) &&
            aWriter.WriteI32(aPacket.mSlotMachineCountDown) &&
            aWriter.WriteI32(aPacket.mSlotMachiningNextSeed) &&
            aWriter.WriteF32(aPacket.mSlotMachiningPosition) &&
            aWriter.WriteBool(aPacket.mActive) &&
            aWriter.WriteBool(aPacket.mRefreshing) &&
            aWriter.WriteU8(0) &&
            aWriter.WriteU8(0) &&
            aWriter.WriteI32(aPacket.mTimesUsed);
    }
    aSuccess =
        aSuccess &&
        aWriter.WriteI32(theState.mCutSceneDarken) &&
        aWriter.WriteI32(theState.mConveyorBeltCounter);

    if (!aSuccess)
        return {};
    return CopyEncodedRecord<kLegacySeedBankRecordSize>(aWriter);
}

bool DecodeLegacySeedBankState(
    std::span<const std::byte> theBytes,
    LegacySeedBankState& theState)
{
    if (theBytes.size() != kLegacySeedBankRecordSize)
        return false;

    LegacySaveReader aReader(theBytes);
    LegacySeedBankState aState;
    if (!ReadGameObjectState(aReader, aState.mGameObject) ||
        !aReader.ReadI32(aState.mNumPackets))
    {
        return false;
    }

    for (auto& aPacket : aState.mSeedPackets)
    {
        std::uint8_t anIgnoredByte{};
        if (!ReadGameObjectState(aReader, aPacket.mGameObject) ||
            !aReader.ReadI32(aPacket.mRefreshCounter) ||
            !aReader.ReadI32(aPacket.mRefreshTime) ||
            !aReader.ReadI32(aPacket.mIndex) ||
            !aReader.ReadI32(aPacket.mOffsetX) ||
            !aReader.ReadI32(aPacket.mPacketType) ||
            !aReader.ReadI32(aPacket.mImitaterType) ||
            !aReader.ReadI32(aPacket.mSlotMachineCountDown) ||
            !aReader.ReadI32(aPacket.mSlotMachiningNextSeed) ||
            !aReader.ReadF32(aPacket.mSlotMachiningPosition) ||
            !aReader.ReadBool(aPacket.mActive) ||
            !aReader.ReadBool(aPacket.mRefreshing) ||
            !aReader.ReadU8(anIgnoredByte) ||
            !aReader.ReadU8(anIgnoredByte) ||
            !aReader.ReadI32(aPacket.mTimesUsed))
        {
            return false;
        }
    }

    if (!aReader.ReadI32(aState.mCutSceneDarken) ||
        !aReader.ReadI32(aState.mConveyorBeltCounter) ||
        aReader.GetBytesRemaining() != 0)
    {
        return false;
    }

    theState = aState;
    return true;
}
