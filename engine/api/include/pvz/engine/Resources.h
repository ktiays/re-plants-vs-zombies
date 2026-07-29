#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace pvz::engine
{

class IResourceStore
{
public:
    virtual ~IResourceStore() = default;

    [[nodiscard]] virtual bool Contains(
        std::string_view thePath) const = 0;
    [[nodiscard]] virtual bool GetSize(
        std::string_view thePath,
        std::uint64_t& theSize) const = 0;
    [[nodiscard]] virtual bool ReadAll(
        std::string_view thePath,
        std::vector<std::byte>& theBytes) const = 0;
};

} // namespace pvz::engine
