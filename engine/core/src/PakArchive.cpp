#include "pvz/engine/core/PakArchive.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <optional>
#include <system_error>
#include <utility>

namespace pvz::engine::core
{
namespace
{

inline constexpr std::uint8_t kXorKey = 0xF7;
inline constexpr std::uint8_t kEndFlag = 0x80;
inline constexpr std::uint32_t kPakMagic = 0xBAC04AC0;
inline constexpr std::uint32_t kSupportedVersion = 0;
inline constexpr std::uint64_t kMaximumEntryCount = 1'000'000;

class HeaderReader
{
public:
    explicit HeaderReader(std::span<const std::byte> theBytes)
        : mBytes(theBytes)
    {
    }

    [[nodiscard]] bool ReadU8(std::uint8_t& theValue)
    {
        if (!CanRead(1))
            return false;

        theValue = Decrypt(mOffset);
        ++mOffset;
        return true;
    }

    [[nodiscard]] bool ReadU32(std::uint32_t& theValue)
    {
        if (!CanRead(4))
            return false;

        theValue = static_cast<std::uint32_t>(Decrypt(mOffset)) |
                   (static_cast<std::uint32_t>(Decrypt(mOffset + 1)) << 8) |
                   (static_cast<std::uint32_t>(Decrypt(mOffset + 2)) << 16) |
                   (static_cast<std::uint32_t>(Decrypt(mOffset + 3)) << 24);
        mOffset += 4;
        return true;
    }

    [[nodiscard]] bool ReadU64(std::uint64_t& theValue)
    {
        std::uint32_t aLow{};
        std::uint32_t aHigh{};
        if (!ReadU32(aLow) || !ReadU32(aHigh))
            return false;

        theValue = static_cast<std::uint64_t>(aLow) |
                   (static_cast<std::uint64_t>(aHigh) << 32);
        return true;
    }

    [[nodiscard]] bool ReadString(
        std::uint8_t theLength,
        std::string& theValue)
    {
        if (!CanRead(theLength))
            return false;

        theValue.clear();
        theValue.reserve(theLength);
        for (std::uint32_t anIndex = 0; anIndex < theLength; ++anIndex)
        {
            const auto aCharacter = static_cast<char>(
                Decrypt(mOffset + anIndex));
            if (aCharacter == '\0')
                return false;
            theValue.push_back(aCharacter);
        }
        mOffset += theLength;
        return true;
    }

    [[nodiscard]] std::uint64_t GetOffset() const
    {
        return mOffset;
    }

private:
    [[nodiscard]] bool CanRead(std::uint64_t theByteCount) const
    {
        const auto aSize = static_cast<std::uint64_t>(mBytes.size());
        return mOffset <= aSize && theByteCount <= aSize - mOffset;
    }

    [[nodiscard]] std::uint8_t Decrypt(std::uint64_t theOffset) const
    {
        return static_cast<std::uint8_t>(
            std::to_integer<std::uint8_t>(
                mBytes[static_cast<std::size_t>(theOffset)]) ^
            kXorKey);
    }

    std::span<const std::byte> mBytes;
    std::uint64_t mOffset{};
};

[[nodiscard]] char UppercaseAscii(char theCharacter)
{
    if (theCharacter >= 'a' && theCharacter <= 'z')
        return static_cast<char>(theCharacter - 'a' + 'A');
    return theCharacter;
}

[[nodiscard]] std::optional<std::string> NormalizePath(
    std::string_view thePath)
{
    std::vector<std::string> aSegments;
    std::string aSegment;

    const auto aCommitSegment = [&]() -> bool {
        if (aSegment.empty() || aSegment == ".")
        {
            aSegment.clear();
            return true;
        }

        if (aSegment == "..")
        {
            if (aSegments.empty())
                return false;
            aSegments.pop_back();
            aSegment.clear();
            return true;
        }

        aSegments.push_back(std::move(aSegment));
        aSegment.clear();
        return true;
    };

    for (const char aCharacter : thePath)
    {
        if (aCharacter == '\0')
            return std::nullopt;

        if (aCharacter == '/' || aCharacter == '\\')
        {
            if (!aCommitSegment())
                return std::nullopt;
            continue;
        }

        aSegment.push_back(UppercaseAscii(aCharacter));
    }

    if (!aCommitSegment() || aSegments.empty())
        return std::nullopt;

    std::string aNormalized;
    for (const auto& aPathSegment : aSegments)
    {
        if (!aNormalized.empty())
            aNormalized.push_back('/');
        aNormalized.append(aPathSegment);
    }
    return aNormalized;
}

} // namespace

bool PakArchive::LoadFromFile(const std::filesystem::path& thePath)
{
    Reset(PakError::None);

    std::error_code anErrorCode;
    const auto aFileSize = std::filesystem::file_size(thePath, anErrorCode);
    if (anErrorCode)
    {
        mError = PakError::FileOpenFailed;
        return false;
    }

    if (aFileSize > std::numeric_limits<std::size_t>::max() ||
        aFileSize >
            static_cast<std::uintmax_t>(
                std::numeric_limits<std::streamsize>::max()))
    {
        mError = PakError::FileSizeUnsupported;
        return false;
    }

    std::ifstream aFile(thePath, std::ios::binary);
    if (!aFile)
    {
        mError = PakError::FileOpenFailed;
        return false;
    }

    std::vector<std::byte> aBytes(static_cast<std::size_t>(aFileSize));
    aFile.read(
        reinterpret_cast<char*>(aBytes.data()),
        static_cast<std::streamsize>(aBytes.size()));
    if (!aFile ||
        aFile.gcount() != static_cast<std::streamsize>(aBytes.size()))
    {
        mError = PakError::FileReadFailed;
        return false;
    }

    return LoadOwnedBytes(std::move(aBytes));
}

bool PakArchive::LoadFromBytes(std::span<const std::byte> theBytes)
{
    Reset(PakError::None);
    return LoadOwnedBytes(
        std::vector<std::byte>(theBytes.begin(), theBytes.end()));
}

bool PakArchive::IsLoaded() const
{
    return !mArchiveBytes.empty() && mError == PakError::None;
}

PakError PakArchive::GetError() const
{
    return mError;
}

std::span<const PakEntry> PakArchive::GetEntries() const
{
    return mEntries;
}

std::uint64_t PakArchive::GetPayloadSize() const
{
    return mPayloadSize;
}

const PakEntry* PakArchive::FindEntry(std::string_view thePath) const
{
    const auto aNormalizedPath = NormalizePath(thePath);
    if (!aNormalizedPath)
        return nullptr;

    const auto anEntry = mEntryIndex.find(*aNormalizedPath);
    if (anEntry == mEntryIndex.end())
        return nullptr;

    return &mEntries[anEntry->second];
}

bool PakArchive::ReadEntry(
    std::string_view thePath,
    std::vector<std::byte>& theBytes) const
{
    const auto* anEntry = FindEntry(thePath);
    return anEntry != nullptr && ReadEntry(*anEntry, theBytes);
}

bool PakArchive::ReadEntry(
    const PakEntry& theEntry,
    std::vector<std::byte>& theBytes) const
{
    const auto anArchiveSize =
        static_cast<std::uint64_t>(mArchiveBytes.size());
    if (theEntry.mDataOffset > anArchiveSize ||
        theEntry.mDataSize > anArchiveSize - theEntry.mDataOffset ||
        theEntry.mDataSize > std::numeric_limits<std::size_t>::max())
    {
        return false;
    }

    std::vector<std::byte> aResult(
        static_cast<std::size_t>(theEntry.mDataSize));
    for (std::uint64_t anIndex = 0; anIndex < theEntry.mDataSize; ++anIndex)
    {
        const auto aSourceIndex = static_cast<std::size_t>(
            theEntry.mDataOffset + anIndex);
        aResult[static_cast<std::size_t>(anIndex)] =
            mArchiveBytes[aSourceIndex] ^ static_cast<std::byte>(kXorKey);
    }

    theBytes = std::move(aResult);
    return true;
}

bool PakArchive::LoadOwnedBytes(std::vector<std::byte> theBytes)
{
    HeaderReader aReader(theBytes);
    std::uint32_t aMagic{};
    std::uint32_t aVersion{};
    if (!aReader.ReadU32(aMagic) || !aReader.ReadU32(aVersion))
    {
        mError = PakError::TruncatedHeader;
        return false;
    }
    if (aMagic != kPakMagic)
    {
        mError = PakError::InvalidMagic;
        return false;
    }
    if (aVersion != kSupportedVersion)
    {
        mError = PakError::UnsupportedVersion;
        return false;
    }

    std::vector<PakEntry> anEntries;
    std::unordered_map<std::string, std::size_t> anEntryIndex;
    std::uint64_t aPayloadSize{};

    for (;;)
    {
        std::uint8_t aFlags{};
        if (!aReader.ReadU8(aFlags))
        {
            mError = PakError::TruncatedHeader;
            return false;
        }
        if ((aFlags & kEndFlag) != 0)
            break;

        if (anEntries.size() >= kMaximumEntryCount)
        {
            mError = PakError::PayloadOutOfBounds;
            return false;
        }

        std::uint8_t aPathLength{};
        std::string aPath;
        std::uint32_t aDataSize{};
        std::uint64_t aWindowsFileTime{};
        if (!aReader.ReadU8(aPathLength) ||
            aPathLength == 0 ||
            !aReader.ReadString(aPathLength, aPath) ||
            !aReader.ReadU32(aDataSize) ||
            !aReader.ReadU64(aWindowsFileTime))
        {
            mError = PakError::TruncatedHeader;
            return false;
        }

        const auto aNormalizedPath = NormalizePath(aPath);
        if (!aNormalizedPath)
        {
            mError = PakError::InvalidEntryPath;
            return false;
        }
        if (anEntryIndex.contains(*aNormalizedPath))
        {
            mError = PakError::DuplicateEntry;
            return false;
        }

        if (aDataSize >
            std::numeric_limits<std::uint64_t>::max() - aPayloadSize)
        {
            mError = PakError::PayloadOutOfBounds;
            return false;
        }

        const auto anEntryIndexValue = anEntries.size();
        anEntries.push_back(PakEntry{
            .mPath = std::move(aPath),
            .mWindowsFileTime = aWindowsFileTime,
            .mDataOffset = aPayloadSize,
            .mDataSize = aDataSize,
        });
        anEntryIndex.emplace(*aNormalizedPath, anEntryIndexValue);
        aPayloadSize += aDataSize;
    }

    const auto aPayloadOffset = aReader.GetOffset();
    const auto anArchiveSize = static_cast<std::uint64_t>(theBytes.size());
    if (aPayloadOffset > anArchiveSize ||
        aPayloadSize > anArchiveSize - aPayloadOffset)
    {
        mError = PakError::PayloadOutOfBounds;
        return false;
    }
    if (aPayloadOffset + aPayloadSize != anArchiveSize)
    {
        mError = PakError::TrailingData;
        return false;
    }

    for (auto& anEntry : anEntries)
        anEntry.mDataOffset += aPayloadOffset;

    mArchiveBytes = std::move(theBytes);
    mEntries = std::move(anEntries);
    mEntryIndex = std::move(anEntryIndex);
    mPayloadSize = aPayloadSize;
    mError = PakError::None;
    return true;
}

void PakArchive::Reset(PakError theError)
{
    mArchiveBytes.clear();
    mEntries.clear();
    mEntryIndex.clear();
    mPayloadSize = 0;
    mError = theError;
}

const char* GetPakErrorMessage(PakError theError)
{
    switch (theError)
    {
    case PakError::None:
        return "no error";
    case PakError::FileOpenFailed:
        return "could not open PAK file";
    case PakError::FileSizeUnsupported:
        return "PAK file is too large for this process";
    case PakError::FileReadFailed:
        return "could not read complete PAK file";
    case PakError::TruncatedHeader:
        return "PAK header is truncated";
    case PakError::InvalidMagic:
        return "PAK magic value is invalid";
    case PakError::UnsupportedVersion:
        return "PAK version is unsupported";
    case PakError::InvalidEntryPath:
        return "PAK contains an invalid entry path";
    case PakError::DuplicateEntry:
        return "PAK contains duplicate normalized paths";
    case PakError::PayloadOutOfBounds:
        return "PAK payload is outside the archive";
    case PakError::TrailingData:
        return "PAK contains unaccounted trailing data";
    }
    return "unknown PAK error";
}

} // namespace pvz::engine::core
