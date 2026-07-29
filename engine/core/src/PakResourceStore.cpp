#include "pvz/engine/core/PakResourceStore.h"

namespace pvz::engine::core
{

bool PakResourceStore::LoadFromFile(const std::filesystem::path& thePath)
{
    return mArchive.LoadFromFile(thePath);
}

bool PakResourceStore::LoadFromBytes(std::span<const std::byte> theBytes)
{
    return mArchive.LoadFromBytes(theBytes);
}

bool PakResourceStore::Contains(std::string_view thePath) const
{
    return mArchive.FindEntry(thePath) != nullptr;
}

bool PakResourceStore::GetSize(
    std::string_view thePath,
    std::uint64_t& theSize) const
{
    const auto* anEntry = mArchive.FindEntry(thePath);
    if (anEntry == nullptr)
        return false;

    theSize = anEntry->mDataSize;
    return true;
}

bool PakResourceStore::ReadAll(
    std::string_view thePath,
    std::vector<std::byte>& theBytes) const
{
    return mArchive.ReadEntry(thePath, theBytes);
}

PakError PakResourceStore::GetError() const
{
    return mArchive.GetError();
}

const PakArchive& PakResourceStore::GetArchive() const
{
    return mArchive;
}

} // namespace pvz::engine::core
