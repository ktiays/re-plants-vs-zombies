#pragma once

#include <climits>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace pvz::engine
{

inline constexpr std::uint32_t kSimulationFrequencyHz = 100;
inline constexpr std::uint32_t kSimulationTickMicroseconds = 10'000;

using TickIndex = std::uint64_t;
using AssetId = std::uint64_t;

struct GameTick
{
    TickIndex mIndex{};
};

struct PointI
{
    std::int32_t mX{};
    std::int32_t mY{};
};

struct PointF
{
    float mX{};
    float mY{};
};

struct SizeI
{
    std::uint32_t mWidth{};
    std::uint32_t mHeight{};
};

struct SizeF
{
    float mWidth{};
    float mHeight{};
};

struct RectI
{
    PointI mOrigin{};
    SizeI mSize{};
};

struct RectF
{
    PointF mOrigin{};
    SizeF mSize{};
};

struct ColorRgba8
{
    std::uint8_t mRed{};
    std::uint8_t mGreen{};
    std::uint8_t mBlue{};
    std::uint8_t mAlpha{};
};

enum class LifecycleResult : std::uint8_t
{
    Success,
    Failure,
};

static_assert(CHAR_BIT == 8);
static_assert(sizeof(std::uint8_t) == 1);
static_assert(sizeof(std::uint16_t) == 2);
static_assert(sizeof(std::uint32_t) == 4);
static_assert(sizeof(std::uint64_t) == 8);
static_assert(sizeof(std::int32_t) == 4);
static_assert(sizeof(float) == 4);
static_assert(std::numeric_limits<float>::is_iec559);
static_assert(sizeof(PointF) == 8);
static_assert(sizeof(SizeF) == 8);
static_assert(sizeof(RectF) == 16);
static_assert(sizeof(ColorRgba8) == 4);
static_assert(std::is_trivially_copyable_v<GameTick>);

} // namespace pvz::engine
