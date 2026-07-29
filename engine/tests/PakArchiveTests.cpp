#include "pvz/engine/core/PakArchive.h"
#include "pvz/engine/core/PakResourceStore.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

void Expect(bool theCondition, const char* theMessage);

namespace
{

inline constexpr std::uint8_t kXorKey = 0xF7;

void AppendU8(std::vector<std::byte>& theBytes, std::uint8_t theValue)
{
    theBytes.push_back(static_cast<std::byte>(theValue));
}

void AppendU32(std::vector<std::byte>& theBytes, std::uint32_t theValue)
{
    for (std::uint32_t aShift = 0; aShift < 32; aShift += 8)
    {
        theBytes.push_back(
            static_cast<std::byte>((theValue >> aShift) & 0xFF));
    }
}

void AppendU64(std::vector<std::byte>& theBytes, std::uint64_t theValue)
{
    for (std::uint32_t aShift = 0; aShift < 64; aShift += 8)
    {
        theBytes.push_back(
            static_cast<std::byte>((theValue >> aShift) & 0xFF));
    }
}

struct TestEntry
{
    std::string_view mPath;
    std::uint64_t mTimestamp;
    std::span<const std::byte> mPayload;
};

std::vector<std::byte> BuildPak(std::span<const TestEntry> theEntries)
{
    std::vector<std::byte> aBytes;
    AppendU32(aBytes, 0xBAC04AC0);
    AppendU32(aBytes, 0);

    for (const auto& anEntry : theEntries)
    {
        AppendU8(aBytes, 0);
        AppendU8(aBytes, static_cast<std::uint8_t>(anEntry.mPath.size()));
        for (const char aCharacter : anEntry.mPath)
            AppendU8(aBytes, static_cast<std::uint8_t>(aCharacter));
        AppendU32(
            aBytes,
            static_cast<std::uint32_t>(anEntry.mPayload.size()));
        AppendU64(aBytes, anEntry.mTimestamp);
    }
    AppendU8(aBytes, 0x80);

    for (const auto& anEntry : theEntries)
        aBytes.insert(
            aBytes.end(),
            anEntry.mPayload.begin(),
            anEntry.mPayload.end());

    for (auto& aByte : aBytes)
        aByte ^= static_cast<std::byte>(kXorKey);
    return aBytes;
}

void TestPakLoadAndRead()
{
    constexpr std::array<std::byte, 3> kImage{
        std::byte{0x89},
        std::byte{0x50},
        std::byte{0x4E},
    };
    constexpr std::array<std::byte, 4> kSound{
        std::byte{0x4F},
        std::byte{0x67},
        std::byte{0x67},
        std::byte{0x53},
    };
    const std::array<TestEntry, 2> anEntries{{
        {"images\\Test.PNG", 123, kImage},
        {"data/../sounds/beep.ogg", 456, kSound},
    }};
    const auto aPakBytes = BuildPak(anEntries);

    pvz::engine::core::PakArchive anArchive;
    Expect(anArchive.LoadFromBytes(aPakBytes), "synthetic PAK loads");
    Expect(anArchive.IsLoaded(), "synthetic PAK reports loaded");
    Expect(anArchive.GetEntries().size() == 2, "PAK entry count");
    Expect(anArchive.GetPayloadSize() == 7, "PAK payload size");

    const auto* anImageEntry =
        anArchive.FindEntry("IMAGES/test.png");
    Expect(anImageEntry != nullptr, "PAK lookup ignores case and slash style");
    if (anImageEntry != nullptr)
    {
        Expect(anImageEntry->mDataSize == 3, "PAK image size");
        Expect(anImageEntry->mWindowsFileTime == 123, "PAK timestamp");
    }

    const auto* aSoundEntry =
        anArchive.FindEntry("sounds\\BEEP.OGG");
    Expect(aSoundEntry != nullptr, "PAK lookup resolves parent segments");

    std::vector<std::byte> aPayload;
    Expect(
        anArchive.ReadEntry("images/test.png", aPayload),
        "PAK entry reads");
    Expect(
        std::ranges::equal(aPayload, kImage),
        "PAK entry is XOR-decoded");

    pvz::engine::core::PakResourceStore aResourceStore;
    Expect(
        aResourceStore.LoadFromBytes(aPakBytes),
        "PAK resource store loads");
    Expect(
        aResourceStore.Contains("SOUNDS/beep.ogg"),
        "resource protocol finds PAK entry");
    std::uint64_t aResourceSize{};
    Expect(
        aResourceStore.GetSize("sounds/beep.ogg", aResourceSize) &&
            aResourceSize == kSound.size(),
        "resource protocol returns PAK entry size");
    Expect(
        aResourceStore.ReadAll("sounds/beep.ogg", aPayload) &&
            std::ranges::equal(aPayload, kSound),
        "resource protocol reads PAK entry");
}

void TestPakValidation()
{
    constexpr std::array<std::byte, 1> kPayload{std::byte{0x01}};
    const std::array<TestEntry, 1> anEntry{{
        {"data/item.bin", 0, kPayload},
    }};

    auto anInvalidMagic = BuildPak(anEntry);
    anInvalidMagic[0] ^= std::byte{0x01};
    pvz::engine::core::PakArchive anArchive;
    Expect(
        !anArchive.LoadFromBytes(anInvalidMagic),
        "invalid PAK magic is rejected");
    Expect(
        anArchive.GetError() == pvz::engine::core::PakError::InvalidMagic,
        "invalid PAK magic error");

    auto aTruncatedPak = BuildPak(anEntry);
    aTruncatedPak.pop_back();
    Expect(
        !anArchive.LoadFromBytes(aTruncatedPak),
        "truncated PAK payload is rejected");
    Expect(
        anArchive.GetError() ==
            pvz::engine::core::PakError::PayloadOutOfBounds,
        "truncated PAK payload error");

    const std::array<TestEntry, 2> duplicateEntries{{
        {"images/test.png", 0, kPayload},
        {"IMAGES\\TEST.PNG", 0, kPayload},
    }};
    Expect(
        !anArchive.LoadFromBytes(BuildPak(duplicateEntries)),
        "duplicate normalized PAK paths are rejected");
    Expect(
        anArchive.GetError() ==
            pvz::engine::core::PakError::DuplicateEntry,
        "duplicate PAK path error");

    const std::array<TestEntry, 1> escapingEntry{{
        {"../outside.bin", 0, kPayload},
    }};
    Expect(
        !anArchive.LoadFromBytes(BuildPak(escapingEntry)),
        "escaping PAK path is rejected");
    Expect(
        anArchive.GetError() ==
            pvz::engine::core::PakError::InvalidEntryPath,
        "escaping PAK path error");
}

} // namespace

void RunPakArchiveTests()
{
    TestPakLoadAndRead();
    TestPakValidation();
}
