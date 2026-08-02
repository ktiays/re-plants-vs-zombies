#include "pvz/parity/ImageComparison.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <utility>

namespace pvz::parity
{
namespace
{

inline constexpr std::uint32_t kBytesPerPixel = 4;
inline constexpr std::int64_t kCoordinateOne = 65'536;
inline constexpr std::int64_t kCoordinateHalf = 32'768;

[[nodiscard]] bool ValidateImage(
    const engine::DecodedImage& theImage,
    ImageComparisonError& theError)
{
    if (theImage.mDescriptor.mPixelFormat !=
        engine::ImagePixelFormat::Bgra8Unorm)
    {
        theError = ImageComparisonError::UnsupportedPixelFormat;
        return false;
    }
    const auto aWidth = theImage.mDescriptor.mSize.mWidth;
    const auto aHeight = theImage.mDescriptor.mSize.mHeight;
    if (aWidth == 0 || aHeight == 0)
    {
        theError = ImageComparisonError::InvalidDimensions;
        return false;
    }
    const auto aMinimumStride =
        static_cast<std::uint64_t>(aWidth) * kBytesPerPixel;
    if (aMinimumStride >
            std::numeric_limits<std::uint32_t>::max() ||
        theImage.mBytesPerRow < aMinimumStride)
    {
        theError = ImageComparisonError::InvalidStride;
        return false;
    }
    const auto aRequiredBytes =
        static_cast<std::uint64_t>(theImage.mBytesPerRow) *
            (aHeight - 1U) +
        aMinimumStride;
    if (aRequiredBytes > theImage.mPixels.size())
    {
        theError = ImageComparisonError::BufferTooSmall;
        return false;
    }
    return true;
}

[[nodiscard]] bool ValidateNormalizedImage(
    const engine::DecodedImage& theImage,
    ImageComparisonError& theError)
{
    if (!ValidateImage(theImage, theError))
        return false;
    if (theImage.mDescriptor.mSize.mWidth !=
            kGoldenImageWidth ||
        theImage.mDescriptor.mSize.mHeight !=
            kGoldenImageHeight)
    {
        theError = ImageComparisonError::InvalidNormalizedImage;
        return false;
    }
    return true;
}

[[nodiscard]] std::size_t PixelOffset(
    const engine::DecodedImage& theImage,
    std::uint32_t theX,
    std::uint32_t theY,
    std::uint32_t theChannel)
{
    const auto anOffset =
        static_cast<std::uint64_t>(theY) *
            theImage.mBytesPerRow +
        static_cast<std::uint64_t>(theX) *
            kBytesPerPixel +
        theChannel;
    return static_cast<std::size_t>(anOffset);
}

[[nodiscard]] std::uint8_t Channel(
    const engine::DecodedImage& theImage,
    std::uint32_t theX,
    std::uint32_t theY,
    std::uint32_t theChannel)
{
    return std::to_integer<std::uint8_t>(
        theImage.mPixels[
            PixelOffset(theImage, theX, theY, theChannel)]);
}

struct SampleCoordinate
{
    std::uint32_t mLower{};
    std::uint32_t mUpper{};
    std::uint32_t mUpperWeight{};
};

[[nodiscard]] SampleCoordinate CalculateCoordinate(
    std::uint32_t theDestination,
    std::uint32_t theDestinationSize,
    std::uint32_t theSourceSize)
{
    const auto aNumerator =
        static_cast<std::int64_t>(2U * theDestination + 1U) *
        static_cast<std::int64_t>(theSourceSize) *
        kCoordinateOne;
    auto aCoordinate =
        aNumerator /
            static_cast<std::int64_t>(2U * theDestinationSize) -
        kCoordinateHalf;
    const auto aMaximum =
        static_cast<std::int64_t>(theSourceSize - 1U) *
        kCoordinateOne;
    aCoordinate = std::clamp(aCoordinate, std::int64_t{0}, aMaximum);
    const auto aLower = static_cast<std::uint32_t>(
        aCoordinate / kCoordinateOne);
    const auto anUpper = std::min(aLower + 1U, theSourceSize - 1U);
    const auto anUpperWeight = static_cast<std::uint32_t>(
        aCoordinate % kCoordinateOne);
    return {
        .mLower = aLower,
        .mUpper = anUpper,
        .mUpperWeight = anUpperWeight,
    };
}

[[nodiscard]] std::uint8_t InterpolateChannel(
    const engine::DecodedImage& theImage,
    ImageCrop theCrop,
    SampleCoordinate theX,
    SampleCoordinate theY,
    std::uint32_t theChannel)
{
    const auto anXWeight =
        static_cast<std::uint64_t>(theX.mUpperWeight);
    const auto anXInverse =
        static_cast<std::uint64_t>(kCoordinateOne) - anXWeight;
    const auto aYWeight =
        static_cast<std::uint64_t>(theY.mUpperWeight);
    const auto aYInverse =
        static_cast<std::uint64_t>(kCoordinateOne) - aYWeight;
    const auto aTop =
        static_cast<std::uint64_t>(Channel(
            theImage,
            theCrop.mX + theX.mLower,
            theCrop.mY + theY.mLower,
            theChannel)) *
            anXInverse +
        static_cast<std::uint64_t>(Channel(
            theImage,
            theCrop.mX + theX.mUpper,
            theCrop.mY + theY.mLower,
            theChannel)) *
            anXWeight;
    const auto aBottom =
        static_cast<std::uint64_t>(Channel(
            theImage,
            theCrop.mX + theX.mLower,
            theCrop.mY + theY.mUpper,
            theChannel)) *
            anXInverse +
        static_cast<std::uint64_t>(Channel(
            theImage,
            theCrop.mX + theX.mUpper,
            theCrop.mY + theY.mUpper,
            theChannel)) *
            anXWeight;
    const auto aValue =
        (aTop * aYInverse + aBottom * aYWeight +
         (std::uint64_t{1} << 31U)) >>
        32U;
    return static_cast<std::uint8_t>(aValue);
}

[[nodiscard]] std::uint8_t AbsoluteDifference(
    std::uint8_t theLeft,
    std::uint8_t theRight)
{
    return theLeft >= theRight
        ? static_cast<std::uint8_t>(theLeft - theRight)
        : static_cast<std::uint8_t>(theRight - theLeft);
}

void AppendPpmHeader(std::vector<std::byte>& thePpm)
{
    const std::string aHeader =
        "P6\n" + std::to_string(kGoldenImageWidth) + " " +
        std::to_string(kGoldenImageHeight) + "\n255\n";
    thePpm.reserve(
        aHeader.size() +
        static_cast<std::size_t>(kGoldenImageWidth) *
            kGoldenImageHeight * 3U);
    for (const auto aCharacter : aHeader)
    {
        thePpm.push_back(
            static_cast<std::byte>(
                static_cast<unsigned char>(aCharacter)));
    }
}

} // namespace

bool NormalizeGoldenImage(
    const engine::DecodedImage& theSource,
    ImageCrop theCrop,
    engine::DecodedImage& theNormalized,
    ImageComparisonError& theError)
{
    theError = ImageComparisonError::None;
    if (!ValidateImage(theSource, theError))
        return false;
    const auto aSourceWidth =
        theSource.mDescriptor.mSize.mWidth;
    const auto aSourceHeight =
        theSource.mDescriptor.mSize.mHeight;
    if (theCrop.mWidth == 0 && theCrop.mHeight == 0)
    {
        theCrop = {
            .mX = 0,
            .mY = 0,
            .mWidth = aSourceWidth,
            .mHeight = aSourceHeight,
        };
    }
    if (theCrop.mWidth == 0 || theCrop.mHeight == 0 ||
        static_cast<std::uint64_t>(theCrop.mX) +
                theCrop.mWidth >
            aSourceWidth ||
        static_cast<std::uint64_t>(theCrop.mY) +
                theCrop.mHeight >
            aSourceHeight)
    {
        theError = ImageComparisonError::InvalidCrop;
        return false;
    }
    if (static_cast<std::uint64_t>(theCrop.mWidth) *
            kGoldenImageHeight !=
        static_cast<std::uint64_t>(theCrop.mHeight) *
            kGoldenImageWidth)
    {
        theError = ImageComparisonError::AspectRatioMismatch;
        return false;
    }

    engine::DecodedImage aNormalized;
    aNormalized.mDescriptor = {
        .mSize = {
            .mWidth = kGoldenImageWidth,
            .mHeight = kGoldenImageHeight,
        },
        .mPixelFormat = engine::ImagePixelFormat::Bgra8Unorm,
    };
    aNormalized.mBytesPerRow =
        kGoldenImageWidth * kBytesPerPixel;
    aNormalized.mPixels.resize(
        static_cast<std::size_t>(aNormalized.mBytesPerRow) *
        kGoldenImageHeight);

    if (theCrop.mWidth == kGoldenImageWidth &&
        theCrop.mHeight == kGoldenImageHeight)
    {
        for (std::uint32_t aY = 0;
             aY < kGoldenImageHeight;
             ++aY)
        {
            const auto aSourceOffset = PixelOffset(
                theSource,
                theCrop.mX,
                theCrop.mY + aY,
                0);
            const auto aDestinationOffset =
                static_cast<std::size_t>(aY) *
                aNormalized.mBytesPerRow;
            std::copy_n(
                theSource.mPixels.begin() +
                    static_cast<std::ptrdiff_t>(aSourceOffset),
                aNormalized.mBytesPerRow,
                aNormalized.mPixels.begin() +
                    static_cast<std::ptrdiff_t>(aDestinationOffset));
        }
        theNormalized = std::move(aNormalized);
        return true;
    }

    for (std::uint32_t aY = 0;
         aY < kGoldenImageHeight;
         ++aY)
    {
        const auto aSourceY = CalculateCoordinate(
            aY,
            kGoldenImageHeight,
            theCrop.mHeight);
        for (std::uint32_t anX = 0;
             anX < kGoldenImageWidth;
             ++anX)
        {
            const auto aSourceX = CalculateCoordinate(
                anX,
                kGoldenImageWidth,
                theCrop.mWidth);
            for (std::uint32_t aChannel = 0;
                 aChannel < kBytesPerPixel;
                 ++aChannel)
            {
                aNormalized.mPixels[PixelOffset(
                    aNormalized,
                    anX,
                    aY,
                    aChannel)] =
                    static_cast<std::byte>(InterpolateChannel(
                        theSource,
                        theCrop,
                        aSourceX,
                        aSourceY,
                        aChannel));
            }
        }
    }
    theNormalized = std::move(aNormalized);
    return true;
}

bool CompareGoldenImages(
    const engine::DecodedImage& theReference,
    const engine::DecodedImage& theCandidate,
    std::uint8_t theChannelTolerance,
    ImageDifference& theDifference,
    ImageComparisonError& theError)
{
    theError = ImageComparisonError::None;
    if (!ValidateNormalizedImage(theReference, theError) ||
        !ValidateNormalizedImage(theCandidate, theError))
    {
        return false;
    }

    ImageDifference aDifference;
    aDifference.mComparedPixels =
        static_cast<std::uint64_t>(kGoldenImageWidth) *
        kGoldenImageHeight;
    for (std::uint32_t aY = 0;
         aY < kGoldenImageHeight;
         ++aY)
    {
        for (std::uint32_t anX = 0;
             anX < kGoldenImageWidth;
             ++anX)
        {
            bool isPixelDifferent = false;
            for (std::uint32_t aChannel = 0;
                 aChannel < kBytesPerPixel;
                 ++aChannel)
            {
                const auto aDelta = AbsoluteDifference(
                    Channel(
                        theReference,
                        anX,
                        aY,
                        aChannel),
                    Channel(
                        theCandidate,
                        anX,
                        aY,
                        aChannel));
                aDifference.mMaximumChannelDifference = std::max(
                    aDifference.mMaximumChannelDifference,
                    aDelta);
                aDifference.mTotalAbsoluteDifference += aDelta;
                aDifference.mTotalSquaredDifference +=
                    static_cast<std::uint64_t>(aDelta) * aDelta;
                isPixelDifferent =
                    isPixelDifferent ||
                    aDelta > theChannelTolerance;
            }
            if (!isPixelDifferent)
                continue;
            ++aDifference.mDifferentPixels;
            if (!aDifference.mHasDifference)
            {
                aDifference.mHasDifference = true;
                aDifference.mMinimumX = anX;
                aDifference.mMinimumY = aY;
                aDifference.mMaximumX = anX;
                aDifference.mMaximumY = aY;
            }
            else
            {
                aDifference.mMinimumX =
                    std::min(aDifference.mMinimumX, anX);
                aDifference.mMinimumY =
                    std::min(aDifference.mMinimumY, aY);
                aDifference.mMaximumX =
                    std::max(aDifference.mMaximumX, anX);
                aDifference.mMaximumY =
                    std::max(aDifference.mMaximumY, aY);
            }
        }
    }
    theDifference = aDifference;
    return true;
}

bool BuildDifferencePpm(
    const engine::DecodedImage& theReference,
    const engine::DecodedImage& theCandidate,
    std::vector<std::byte>& thePpm,
    ImageComparisonError& theError)
{
    theError = ImageComparisonError::None;
    if (!ValidateNormalizedImage(theReference, theError) ||
        !ValidateNormalizedImage(theCandidate, theError))
    {
        return false;
    }
    std::vector<std::byte> aPpm;
    AppendPpmHeader(aPpm);
    for (std::uint32_t aY = 0;
         aY < kGoldenImageHeight;
         ++aY)
    {
        for (std::uint32_t anX = 0;
             anX < kGoldenImageWidth;
             ++anX)
        {
            const auto anAlphaDelta = AbsoluteDifference(
                Channel(theReference, anX, aY, 3),
                Channel(theCandidate, anX, aY, 3));
            for (const auto aChannel :
                 std::array<std::uint32_t, 3>{2, 1, 0})
            {
                const auto aDelta = std::max(
                    AbsoluteDifference(
                        Channel(
                            theReference,
                            anX,
                            aY,
                            aChannel),
                        Channel(
                            theCandidate,
                            anX,
                            aY,
                            aChannel)),
                    anAlphaDelta);
                const auto anAmplified = std::min(
                    static_cast<std::uint32_t>(aDelta) * 4U,
                    std::uint32_t{255});
                aPpm.push_back(static_cast<std::byte>(anAmplified));
            }
        }
    }
    thePpm = std::move(aPpm);
    return true;
}

bool BuildNormalizedPpm(
    const engine::DecodedImage& theImage,
    std::vector<std::byte>& thePpm,
    ImageComparisonError& theError)
{
    theError = ImageComparisonError::None;
    if (!ValidateNormalizedImage(theImage, theError))
        return false;
    std::vector<std::byte> aPpm;
    AppendPpmHeader(aPpm);
    for (std::uint32_t aY = 0;
         aY < kGoldenImageHeight;
         ++aY)
    {
        for (std::uint32_t anX = 0;
             anX < kGoldenImageWidth;
             ++anX)
        {
            for (const auto aChannel :
                 std::array<std::uint32_t, 3>{2, 1, 0})
            {
                aPpm.push_back(static_cast<std::byte>(
                    Channel(theImage, anX, aY, aChannel)));
            }
        }
    }
    thePpm = std::move(aPpm);
    return true;
}

} // namespace pvz::parity
