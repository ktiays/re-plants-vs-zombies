#include "Lawn/System/DataSync.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

void Expect(bool theCondition, const char* theMessage);

namespace
{

void TestGoldenLegacyEncoding()
{
    DataWriter aWriter;
    aWriter.OpenMemory();
    DataSync aSync(aWriter);

    std::uint32_t anUnsignedValue = 0x89ABCDEF;
    std::int32_t aSignedValue = -2;
    std::uint64_t anUnsigned64Value = 0x0123456789ABCDEF;
    std::int64_t aSigned64Value = -3;
    std::uint16_t anUnsignedShort = 0x1234;
    std::int16_t aSignedShort = -3;
    std::uint8_t anUnsignedByte = 0xAB;
    std::int8_t aSignedByte = -4;
    bool aBoolean = true;
    float aFloat = 1.0F;
    double aDouble = -2.5;
    std::string aString = "PvZ";

    aSync.SyncLong(anUnsignedValue);
    aSync.SyncSLong(aSignedValue);
    aSync.SyncUInt64(anUnsigned64Value);
    aSync.SyncInt64(aSigned64Value);
    aSync.SyncShort(anUnsignedShort);
    aSync.SyncSShort(aSignedShort);
    aSync.SyncByte(anUnsignedByte);
    aSync.SyncSByte(aSignedByte);
    aSync.SyncBool(aBoolean);
    aSync.SyncFloat(aFloat);
    aSync.SyncDouble(aDouble);
    aSync.SyncString(aString);

    constexpr std::array<std::uint8_t, 48> kExpected{
        0xEF, 0xCD, 0xAB, 0x89,
        0xFE, 0xFF, 0xFF, 0xFF,
        0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01,
        0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x34, 0x12,
        0xFD, 0xFF,
        0xAB,
        0xFC,
        0x01,
        0x00, 0x00, 0x80, 0x3F,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xC0,
        0x03, 0x00, 0x50, 0x76, 0x5A,
    };

    Expect(aWriter.GetDataLen() == kExpected.size(), "legacy byte count");
    const auto* aBytes =
        static_cast<const std::uint8_t*>(aWriter.GetDataPtr());
    for (std::size_t anIndex = 0; anIndex < kExpected.size(); ++anIndex)
    {
        Expect(
            aBytes[anIndex] == kExpected[anIndex],
            "legacy golden little-endian bytes");
    }
}

void TestLegacyRoundTrip()
{
    DataWriter aWriter;
    aWriter.OpenMemory();
    DataSync aWriteSync(aWriter);

    std::uint32_t anUnsignedValue = 0x89ABCDEF;
    std::int32_t aSignedValue = -2;
    std::uint64_t anUnsigned64Value = 0x0123456789ABCDEF;
    std::int64_t aSigned64Value = -3;
    std::uint16_t anUnsignedShort = 0x1234;
    std::int16_t aSignedShort = -3;
    std::uint8_t anUnsignedByte = 0xAB;
    std::int8_t aSignedByte = -4;
    bool aBoolean = true;
    float aFloat = 1.0F;
    double aDouble = -2.5;
    std::string aString = "Plants vs. Zombies";

    aWriteSync.SyncLong(anUnsignedValue);
    aWriteSync.SyncSLong(aSignedValue);
    aWriteSync.SyncUInt64(anUnsigned64Value);
    aWriteSync.SyncInt64(aSigned64Value);
    aWriteSync.SyncShort(anUnsignedShort);
    aWriteSync.SyncSShort(aSignedShort);
    aWriteSync.SyncByte(anUnsignedByte);
    aWriteSync.SyncSByte(aSignedByte);
    aWriteSync.SyncBool(aBoolean);
    aWriteSync.SyncFloat(aFloat);
    aWriteSync.SyncDouble(aDouble);
    aWriteSync.SyncString(aString);

    DataReader aReader;
    aReader.OpenMemory(
        aWriter.GetDataPtr(),
        aWriter.GetDataLen(),
        false);
    DataSync aReadSync(aReader);

    std::uint32_t aReadUnsignedValue{};
    std::int32_t aReadSignedValue{};
    std::uint64_t aReadUnsigned64Value{};
    std::int64_t aReadSigned64Value{};
    std::uint16_t aReadUnsignedShort{};
    std::int16_t aReadSignedShort{};
    std::uint8_t aReadUnsignedByte{};
    std::int8_t aReadSignedByte{};
    bool aReadBoolean{};
    float aReadFloat{};
    double aReadDouble{};
    std::string aReadString;

    aReadSync.SyncLong(aReadUnsignedValue);
    aReadSync.SyncSLong(aReadSignedValue);
    aReadSync.SyncUInt64(aReadUnsigned64Value);
    aReadSync.SyncInt64(aReadSigned64Value);
    aReadSync.SyncShort(aReadUnsignedShort);
    aReadSync.SyncSShort(aReadSignedShort);
    aReadSync.SyncByte(aReadUnsignedByte);
    aReadSync.SyncSByte(aReadSignedByte);
    aReadSync.SyncBool(aReadBoolean);
    aReadSync.SyncFloat(aReadFloat);
    aReadSync.SyncDouble(aReadDouble);
    aReadSync.SyncString(aReadString);

    Expect(
        aReadUnsignedValue == anUnsignedValue,
        "legacy unsigned value round-trips");
    Expect(
        aReadSignedValue == aSignedValue,
        "legacy signed value round-trips");
    Expect(
        aReadUnsigned64Value == anUnsigned64Value,
        "legacy unsigned 64-bit value round-trips");
    Expect(
        aReadSigned64Value == aSigned64Value,
        "legacy signed 64-bit value round-trips");
    Expect(
        aReadUnsignedShort == anUnsignedShort,
        "legacy unsigned short round-trips");
    Expect(
        aReadSignedShort == aSignedShort,
        "legacy signed short round-trips");
    Expect(
        aReadUnsignedByte == anUnsignedByte,
        "legacy unsigned byte round-trips");
    Expect(
        aReadSignedByte == aSignedByte,
        "legacy signed byte round-trips");
    Expect(aReadBoolean == aBoolean, "legacy boolean round-trips");
    Expect(aReadFloat == aFloat, "legacy float round-trips");
    Expect(aReadDouble == aDouble, "legacy double round-trips");
    Expect(aReadString == aString, "legacy string round-trips");
}

void TestSmallDestinationIsNotOverwritten()
{
    DataWriter aWriter;
    aWriter.OpenMemory();
    aWriter.WriteLong(0x7F);

    DataReader aReader;
    aReader.OpenMemory(
        aWriter.GetDataPtr(),
        aWriter.GetDataLen(),
        false);
    DataSync aSync(aReader);

    struct GuardedByte
    {
        std::uint8_t mBefore{0x11};
        std::uint8_t mValue{};
        std::uint8_t mAfter{0x22};
    };

    GuardedByte aGuardedByte;
    aSync.SyncLong(aGuardedByte.mValue);
    Expect(aGuardedByte.mValue == 0x7F, "small destination receives value");
    Expect(
        aGuardedByte.mBefore == 0x11 && aGuardedByte.mAfter == 0x22,
        "small destination guards remain intact");
}

void TestReaderBoundsAndRewind()
{
    constexpr std::array<std::uint8_t, 3> kTruncated{
        0x01,
        0x02,
        0x03,
    };
    DataReader aTruncatedReader;
    aTruncatedReader.OpenMemory(
        kTruncated.data(),
        kTruncated.size(),
        false);

    bool aThrew = false;
    try
    {
        static_cast<void>(aTruncatedReader.ReadLong());
    }
    catch (const DataReaderException&)
    {
        aThrew = true;
    }
    Expect(aThrew, "truncated legacy value throws");

    constexpr std::array<std::uint8_t, 2> kRewindBytes{0x10, 0x20};
    DataReader aRewindReader;
    aRewindReader.OpenMemory(
        kRewindBytes.data(),
        kRewindBytes.size(),
        false);
    Expect(aRewindReader.ReadByte() == 0x10, "rewind first read");
    aRewindReader.Rewind(1);
    Expect(aRewindReader.ReadByte() == 0x10, "rewind rereads byte");
    Expect(aRewindReader.ReadByte() == 0x20, "rewind preserves base pointer");
}

void TestWriterPatchOperations()
{
    DataWriter aWriter;
    aWriter.OpenMemory();
    aWriter.WriteLong(0);
    aWriter.WriteShort(0);
    aWriter.WriteByte(0);
    aWriter.SetLong(0x89ABCDEF, 0);
    aWriter.SetShort(0x1234, 4);
    aWriter.SetByte(0xAB, 6);

    DataReader aReader;
    aReader.OpenMemory(
        aWriter.GetDataPtr(),
        aWriter.GetDataLen(),
        false);
    Expect(aReader.ReadLong() == 0x89ABCDEF, "patch 32-bit value");
    Expect(aReader.ReadShort() == 0x1234, "patch 16-bit value");
    Expect(aReader.ReadByte() == 0xAB, "patch 8-bit value");
}

} // namespace

void RunLegacyDataSyncTests()
{
    TestGoldenLegacyEncoding();
    TestLegacyRoundTrip();
    TestSmallDestinationIsNotOverwritten();
    TestReaderBoundsAndRewind();
    TestWriterPatchOperations();
}
