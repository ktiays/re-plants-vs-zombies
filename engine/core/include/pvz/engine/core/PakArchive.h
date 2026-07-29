#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pvz::engine::core
{

enum class PakError : std::uint8_t
{
    None,
    FileOpenFailed,
    FileSizeUnsupported,
    FileReadFailed,
    TruncatedHeader,
    InvalidMagic,
    UnsupportedVersion,
    InvalidEntryPath,
    DuplicateEntry,
    PayloadOutOfBounds,
    TrailingData,
};

struct PakEntry
{
    std::string mPath;
    std::uint64_t mWindowsFileTime{};
    std::uint64_t mDataOffset{};
    std::uint64_t mDataSize{};
};

class PakArchive
{
public:
    [[nodiscard]] bool LoadFromFile(const std::filesystem::path& thePath);
    [[nodiscard]] bool LoadFromBytes(std::span<const std::byte> theBytes);

    [[nodiscard]] bool IsLoaded() const;
    [[nodiscard]] PakError GetError() const;
    [[nodiscard]] std::span<const PakEntry> GetEntries() const;
    [[nodiscard]] std::uint64_t GetPayloadSize() const;

    [[nodiscard]] const PakEntry* FindEntry(
        std::string_view thePath) const;
    [[nodiscard]] bool ReadEntry(
        std::string_view thePath,
        std::vector<std::byte>& theBytes) const;
    [[nodiscard]] bool ReadEntry(
        const PakEntry& theEntry,
        std::vector<std::byte>& theBytes) const;

private:
    [[nodiscard]] bool LoadOwnedBytes(std::vector<std::byte> theBytes);
    void Reset(PakError theError);

    std::vector<std::byte> mArchiveBytes;
    std::vector<PakEntry> mEntries;
    std::unordered_map<std::string, std::size_t> mEntryIndex;
    std::uint64_t mPayloadSize{};
    PakError mError{PakError::None};
};

[[nodiscard]] const char* GetPakErrorMessage(PakError theError);

} // namespace pvz::engine::core
