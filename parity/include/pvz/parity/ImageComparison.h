#pragma once

#include "pvz/engine/Image.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pvz::parity
{

inline constexpr std::uint32_t kGoldenImageWidth = 800;
inline constexpr std::uint32_t kGoldenImageHeight = 600;

struct ImageCrop
{
    std::uint32_t mX{};
    std::uint32_t mY{};
    std::uint32_t mWidth{};
    std::uint32_t mHeight{};
};

enum class ImageComparisonError : std::uint8_t
{
    None,
    UnsupportedPixelFormat,
    InvalidDimensions,
    InvalidStride,
    BufferTooSmall,
    InvalidCrop,
    AspectRatioMismatch,
    InvalidNormalizedImage,
};

struct ImageDifference
{
    std::uint64_t mComparedPixels{};
    std::uint64_t mDifferentPixels{};
    std::uint64_t mTotalAbsoluteDifference{};
    std::uint64_t mTotalSquaredDifference{};
    std::uint8_t mMaximumChannelDifference{};
    bool mHasDifference{};
    std::uint32_t mMinimumX{};
    std::uint32_t mMinimumY{};
    std::uint32_t mMaximumX{};
    std::uint32_t mMaximumY{};
};

[[nodiscard]] bool NormalizeGoldenImage(
    const engine::DecodedImage& theSource,
    ImageCrop theCrop,
    engine::DecodedImage& theNormalized,
    ImageComparisonError& theError);

[[nodiscard]] bool CompareGoldenImages(
    const engine::DecodedImage& theReference,
    const engine::DecodedImage& theCandidate,
    std::uint8_t theChannelTolerance,
    ImageDifference& theDifference,
    ImageComparisonError& theError);

[[nodiscard]] bool BuildDifferencePpm(
    const engine::DecodedImage& theReference,
    const engine::DecodedImage& theCandidate,
    std::vector<std::byte>& thePpm,
    ImageComparisonError& theError);

[[nodiscard]] bool BuildNormalizedPpm(
    const engine::DecodedImage& theImage,
    std::vector<std::byte>& thePpm,
    ImageComparisonError& theError);

} // namespace pvz::parity
