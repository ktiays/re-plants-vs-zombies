#include "pvz/engine/core/BinaryStateIO.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>

namespace
{

} // namespace

int gFailureCount{};

void RunPakArchiveTests();
void RunBitmapFontResourceManagerTests();
void RunImageResourceManagerTests();
void RunXmlDocumentTests();
void RunXmlPullParserTests();

void Expect(bool theCondition, const char* theMessage)
{
    if (theCondition)
        return;

    std::cerr << "FAIL: " << theMessage << '\n';
    ++gFailureCount;
}

namespace
{

void TestGoldenEncoding()
{
    pvz::engine::core::BinaryStateWriter aWriter;

    Expect(aWriter.WriteU8(0xAB), "write u8");
    Expect(aWriter.WriteU16(0x1234), "write u16");
    Expect(aWriter.WriteU32(0x89ABCDEF), "write u32");
    Expect(aWriter.WriteU64(0x0123456789ABCDEF), "write u64");
    Expect(aWriter.WriteI32(-2), "write i32");
    Expect(aWriter.WriteI64(-3), "write i64");
    Expect(aWriter.WriteBool(true), "write bool");
    Expect(aWriter.WriteUtf8("PvZ"), "write UTF-8 string");

    constexpr std::array<std::uint8_t, 35> kExpected{
        0xAB,
        0x34, 0x12,
        0xEF, 0xCD, 0xAB, 0x89,
        0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01,
        0xFE, 0xFF, 0xFF, 0xFF,
        0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x01,
        0x03, 0x00, 0x00, 0x00,
        0x50, 0x76, 0x5A,
    };

    const auto aBytes = aWriter.GetBytes();
    Expect(aBytes.size() == kExpected.size(), "golden byte count");
    if (aBytes.size() != kExpected.size())
        return;

    for (std::size_t aIndex = 0; aIndex < kExpected.size(); ++aIndex)
    {
        Expect(
            std::to_integer<std::uint8_t>(aBytes[aIndex]) ==
                kExpected[aIndex],
            "golden little-endian bytes");
    }
}

void TestRoundTrip()
{
    pvz::engine::core::BinaryStateWriter aWriter;
    Expect(aWriter.WriteU8(42), "round-trip write u8");
    Expect(aWriter.WriteU16(65'000), "round-trip write u16");
    Expect(aWriter.WriteU32(4'000'000'000), "round-trip write u32");
    Expect(
        aWriter.WriteU64(18'000'000'000'000'000'000ULL),
        "round-trip write u64");
    Expect(aWriter.WriteI32(-2'000'000'000), "round-trip write i32");
    Expect(
        aWriter.WriteI64(-8'000'000'000'000'000'000LL),
        "round-trip write i64");
    Expect(aWriter.WriteBool(false), "round-trip write bool");
    Expect(aWriter.WriteUtf8("Plants vs. Zombies"), "round-trip write text");

    pvz::engine::core::BinaryStateReader aReader(aWriter.GetBytes());
    std::uint8_t aU8{};
    std::uint16_t aU16{};
    std::uint32_t aU32{};
    std::uint64_t aU64{};
    std::int32_t anI32{};
    std::int64_t anI64{};
    bool aBool{true};
    std::string aText;

    Expect(aReader.ReadU8(aU8) && aU8 == 42, "round-trip read u8");
    Expect(aReader.ReadU16(aU16) && aU16 == 65'000, "round-trip read u16");
    Expect(
        aReader.ReadU32(aU32) && aU32 == 4'000'000'000,
        "round-trip read u32");
    Expect(
        aReader.ReadU64(aU64) && aU64 == 18'000'000'000'000'000'000ULL,
        "round-trip read u64");
    Expect(
        aReader.ReadI32(anI32) && anI32 == -2'000'000'000,
        "round-trip read i32");
    Expect(
        aReader.ReadI64(anI64) &&
            anI64 == -8'000'000'000'000'000'000LL,
        "round-trip read i64");
    Expect(aReader.ReadBool(aBool) && !aBool, "round-trip read bool");
    Expect(
        aReader.ReadUtf8(aText) && aText == "Plants vs. Zombies",
        "round-trip read text");
    Expect(aReader.GetBytesRemaining() == 0, "round-trip consumes input");
    Expect(
        aReader.GetError() == pvz::engine::StateIoError::None,
        "round-trip has no error");
}

void TestMalformedInput()
{
    constexpr std::array<std::byte, 3> kTruncated{
        std::byte{0x04},
        std::byte{0x00},
        std::byte{0x00},
    };
    pvz::engine::core::BinaryStateReader aReader(kTruncated);
    std::uint32_t aValue{123};

    Expect(!aReader.ReadU32(aValue), "truncated u32 fails");
    Expect(aValue == 123, "failed read does not modify destination");
    Expect(
        aReader.GetError() == pvz::engine::StateIoError::EndOfInput,
        "truncated u32 reports end of input");

    constexpr std::array<std::byte, 1> kInvalidBool{std::byte{0x02}};
    pvz::engine::core::BinaryStateReader aBoolReader(kInvalidBool);
    bool aBool{};
    Expect(!aBoolReader.ReadBool(aBool), "invalid bool fails");
    Expect(
        aBoolReader.GetError() == pvz::engine::StateIoError::InvalidValue,
        "invalid bool reports invalid value");
}

} // namespace

void RunApplicationRunnerTests();

int main()
{
    RunApplicationRunnerTests();
    TestGoldenEncoding();
    TestRoundTrip();
    TestMalformedInput();
    RunBitmapFontResourceManagerTests();
    RunImageResourceManagerTests();
    RunPakArchiveTests();
    RunXmlDocumentTests();
    RunXmlPullParserTests();

    if (gFailureCount != 0)
    {
        std::cerr << gFailureCount << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "Binary state I/O tests passed\n";
    return 0;
}
