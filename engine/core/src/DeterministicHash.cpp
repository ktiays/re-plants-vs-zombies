#include "pvz/engine/core/DeterministicHash.h"

namespace pvz::engine::core
{

std::uint64_t CalculateFnv1a64(
    std::span<const std::byte> theBytes,
    std::uint64_t theSeed)
{
    auto aHash = theSeed;
    for (const auto aByte : theBytes)
    {
        aHash ^= std::to_integer<std::uint8_t>(aByte);
        aHash *= kFnv1a64Prime;
    }
    return aHash;
}

} // namespace pvz::engine::core
