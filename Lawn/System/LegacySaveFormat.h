#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

struct SaveFileHeader
{
    std::uint32_t mMagicNumber{};
    std::uint32_t mBuildVersion{};
    std::uint32_t mBuildDate{};
};

inline constexpr std::uint32_t kLegacySaveMagic = 0xFEEDDEAD;
inline constexpr std::uint32_t kLegacySaveVersion = 2;
inline constexpr std::size_t kLegacySaveHeaderSize = 12;

[[nodiscard]] std::array<std::byte, kLegacySaveHeaderSize>
EncodeLegacySaveHeader(const SaveFileHeader& theHeader);

[[nodiscard]] bool DecodeLegacySaveHeader(
    std::span<const std::byte> theBytes,
    SaveFileHeader& theHeader);

enum class LegacySaveIoError : std::uint8_t
{
    None,
    EndOfInput,
    LengthOverflow,
    BlockSizeMismatch,
    InvalidValue,
};

class LegacySaveWriter
{
public:
    void Reset();

    [[nodiscard]] bool WriteU8(std::uint8_t theValue);
    [[nodiscard]] bool WriteU32(std::uint32_t theValue);
    [[nodiscard]] bool WriteI32(std::int32_t theValue);
    [[nodiscard]] bool WriteF32(float theValue);
    [[nodiscard]] bool WriteBool(bool theValue);
    [[nodiscard]] bool WriteBytes(std::span<const std::byte> theBytes);
    [[nodiscard]] bool WriteBlock(std::span<const std::byte> theBytes);

    [[nodiscard]] std::span<const std::byte> GetBytes() const;
    [[nodiscard]] LegacySaveIoError GetError() const;

private:
    std::vector<std::byte> mBytes;
    LegacySaveIoError mError{LegacySaveIoError::None};
};

class LegacySaveReader
{
public:
    LegacySaveReader() = default;
    explicit LegacySaveReader(std::span<const std::byte> theBytes);

    void Reset(std::span<const std::byte> theBytes);

    [[nodiscard]] bool ReadU8(std::uint8_t& theValue);
    [[nodiscard]] bool ReadU32(std::uint32_t& theValue);
    [[nodiscard]] bool ReadI32(std::int32_t& theValue);
    [[nodiscard]] bool ReadF32(float& theValue);
    [[nodiscard]] bool ReadBool(bool& theValue);
    [[nodiscard]] bool ReadBytes(std::span<std::byte> theBytes);
    [[nodiscard]] bool ReadBlock(std::span<std::byte> theBytes);

    [[nodiscard]] std::uint64_t GetBytesRemaining() const;
    [[nodiscard]] LegacySaveIoError GetError() const;

private:
    [[nodiscard]] bool CanRead(std::uint64_t theByteCount);
    void Fail(LegacySaveIoError theError);

    std::span<const std::byte> mBytes;
    std::uint64_t mOffset{};
    LegacySaveIoError mError{LegacySaveIoError::None};
};

[[nodiscard]] const char* GetLegacySaveIoErrorMessage(
    LegacySaveIoError theError);

struct LegacyGameObjectState
{
    std::int32_t mX{};
    std::int32_t mY{};
    std::int32_t mWidth{};
    std::int32_t mHeight{};
    bool mVisible{};
    std::int32_t mRow{};
    std::int32_t mRenderOrder{};
};

struct LegacyCursorObjectState
{
    LegacyGameObjectState mGameObject;
    std::int32_t mSeedBankIndex{};
    std::int32_t mType{};
    std::int32_t mImitaterType{};
    std::int32_t mCursorType{};
    std::uint32_t mCoinId{};
    std::uint32_t mGlovePlantId{};
    std::uint32_t mDuplicatorPlantId{};
    std::uint32_t mCobCannonPlantId{};
    std::int32_t mHammerDownCounter{};
    std::uint32_t mReanimCursorId{};
};

struct LegacyCursorPreviewState
{
    LegacyGameObjectState mGameObject;
    std::int32_t mGridX{};
    std::int32_t mGridY{};
};

struct LegacyMusicState
{
    std::int32_t mCurMusicTune{};
    std::int32_t mCurMusicFileMain{};
    std::int32_t mCurMusicFileDrums{};
    std::int32_t mCurMusicFileHihats{};
    std::int32_t mBurstOverride{};
    float mBaseBpm{};
    float mBaseModSpeed{};
    std::int32_t mMusicBurstState{};
    std::int32_t mBurstStateCounter{};
    std::int32_t mMusicDrumsState{};
    std::int32_t mQueuedDrumTrackPackedOrder{};
    std::int32_t mDrumsStateCounter{};
    std::int32_t mPauseOffset{};
    std::int32_t mPauseOffsetDrums{};
    bool mPaused{};
    bool mMusicDisabled{};
    std::int32_t mFadeOutCounter{};
    std::int32_t mFadeOutDuration{};
};

inline constexpr std::size_t kLegacyMessageTextLength = 128;

struct LegacyMessageState
{
    std::array<std::uint8_t, kLegacyMessageTextLength> mLabel{};
    std::int32_t mDisplayTime{};
    std::int32_t mDuration{};
    std::int32_t mMessageStyle{};
    std::array<std::uint32_t, kLegacyMessageTextLength>
        mTextReanimIds{};
    std::int32_t mReanimType{};
    std::int32_t mSlideOffTime{};
    std::array<std::uint8_t, kLegacyMessageTextLength> mLabelNext{};
    std::int32_t mMessageStyleNext{};
};

struct LegacySeedPacketState
{
    LegacyGameObjectState mGameObject;
    std::int32_t mRefreshCounter{};
    std::int32_t mRefreshTime{};
    std::int32_t mIndex{};
    std::int32_t mOffsetX{};
    std::int32_t mPacketType{};
    std::int32_t mImitaterType{};
    std::int32_t mSlotMachineCountDown{};
    std::int32_t mSlotMachiningNextSeed{};
    float mSlotMachiningPosition{};
    bool mActive{};
    bool mRefreshing{};
    std::int32_t mTimesUsed{};
};

inline constexpr std::size_t kLegacySeedPacketCount = 10;

struct LegacySeedBankState
{
    LegacyGameObjectState mGameObject;
    std::int32_t mNumPackets{};
    std::array<LegacySeedPacketState, kLegacySeedPacketCount>
        mSeedPackets{};
    std::int32_t mCutSceneDarken{};
    std::int32_t mConveyorBeltCounter{};
};

inline constexpr std::size_t kLegacyCursorObjectRecordSize = 76;
inline constexpr std::size_t kLegacyCursorPreviewRecordSize = 44;
inline constexpr std::size_t kLegacyMusicRecordSize = 76;
inline constexpr std::size_t kLegacyMessageRecordSize = 796;
inline constexpr std::size_t kLegacySeedPacketRecordSize = 80;
inline constexpr std::size_t kLegacySeedBankRecordSize = 848;

[[nodiscard]] std::array<std::byte, kLegacyCursorObjectRecordSize>
EncodeLegacyCursorObjectState(const LegacyCursorObjectState& theState);

[[nodiscard]] bool DecodeLegacyCursorObjectState(
    std::span<const std::byte> theBytes,
    LegacyCursorObjectState& theState);

[[nodiscard]] std::array<std::byte, kLegacyCursorPreviewRecordSize>
EncodeLegacyCursorPreviewState(const LegacyCursorPreviewState& theState);

[[nodiscard]] bool DecodeLegacyCursorPreviewState(
    std::span<const std::byte> theBytes,
    LegacyCursorPreviewState& theState);

[[nodiscard]] std::array<std::byte, kLegacyMusicRecordSize>
EncodeLegacyMusicState(const LegacyMusicState& theState);

[[nodiscard]] bool DecodeLegacyMusicState(
    std::span<const std::byte> theBytes,
    LegacyMusicState& theState);

[[nodiscard]] std::array<std::byte, kLegacyMessageRecordSize>
EncodeLegacyMessageState(const LegacyMessageState& theState);

[[nodiscard]] bool DecodeLegacyMessageState(
    std::span<const std::byte> theBytes,
    LegacyMessageState& theState);

[[nodiscard]] std::array<std::byte, kLegacySeedBankRecordSize>
EncodeLegacySeedBankState(const LegacySeedBankState& theState);

[[nodiscard]] bool DecodeLegacySeedBankState(
    std::span<const std::byte> theBytes,
    LegacySeedBankState& theState);
