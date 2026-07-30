#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace pvz::engine::core
{

inline constexpr std::uint64_t kFnv1a64Offset =
    14'695'981'039'346'656'037ULL;
inline constexpr std::uint64_t kFnv1a64Prime =
    1'099'511'628'211ULL;

[[nodiscard]] std::uint64_t CalculateFnv1a64(
    std::span<const std::byte> theBytes,
    std::uint64_t theSeed = kFnv1a64Offset);

} // namespace pvz::engine::core
