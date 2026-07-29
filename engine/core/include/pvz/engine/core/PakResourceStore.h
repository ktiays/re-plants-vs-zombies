#pragma once

#include "pvz/engine/Resources.h"
#include "pvz/engine/core/PakArchive.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace pvz::engine::core
{

class PakResourceStore final : public IResourceStore
{
public:
    [[nodiscard]] bool LoadFromFile(const std::filesystem::path& thePath);
    [[nodiscard]] bool LoadFromBytes(std::span<const std::byte> theBytes);

    [[nodiscard]] bool Contains(
        std::string_view thePath) const override;
    [[nodiscard]] bool GetSize(
        std::string_view thePath,
        std::uint64_t& theSize) const override;
    [[nodiscard]] bool ReadAll(
        std::string_view thePath,
        std::vector<std::byte>& theBytes) const override;

    [[nodiscard]] PakError GetError() const;
    [[nodiscard]] const PakArchive& GetArchive() const;

private:
    PakArchive mArchive;
};

} // namespace pvz::engine::core
