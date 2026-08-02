#include "pvz/parity/ImageComparison.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{

int gFailures = 0;

void Expect(bool theCondition, std::string_view theMessage)
{
    if (theCondition)
        return;
    ++gFailures;
    std::cerr << "FAIL: " << theMessage << '\n';
}

[[nodiscard]] pvz::engine::DecodedImage MakeImage(
    std::uint32_t theWidth,
    std::uint32_t theHeight,
    std::uint8_t theBlue = 0,
    std::uint8_t theGreen = 0,
    std::uint8_t theRed = 0,
    std::uint8_t theAlpha = 255)
{
    pvz::engine::DecodedImage anImage;
    anImage.mDescriptor = {
        .mSize = {
            .mWidth = theWidth,
            .mHeight = theHeight,
        },
        .mPixelFormat =
            pvz::engine::ImagePixelFormat::Bgra8Unorm,
    };
    anImage.mBytesPerRow = theWidth * 4U;
    anImage.mPixels.resize(
        static_cast<std::size_t>(anImage.mBytesPerRow) *
        theHeight);
    for (std::uint32_t aY = 0; aY < theHeight; ++aY)
    {
        for (std::uint32_t anX = 0; anX < theWidth; ++anX)
        {
            const auto anOffset =
                static_cast<std::size_t>(aY) *
                    anImage.mBytesPerRow +
                static_cast<std::size_t>(anX) * 4U;
            anImage.mPixels[anOffset] =
                static_cast<std::byte>(theBlue);
            anImage.mPixels[anOffset + 1U] =
                static_cast<std::byte>(theGreen);
            anImage.mPixels[anOffset + 2U] =
                static_cast<std::byte>(theRed);
            anImage.mPixels[anOffset + 3U] =
                static_cast<std::byte>(theAlpha);
        }
    }
    return anImage;
}

void SetChannel(
    pvz::engine::DecodedImage& theImage,
    std::uint32_t theX,
    std::uint32_t theY,
    std::uint32_t theChannel,
    std::uint8_t theValue)
{
    const auto anOffset =
        static_cast<std::size_t>(theY) *
            theImage.mBytesPerRow +
        static_cast<std::size_t>(theX) * 4U +
        theChannel;
    theImage.mPixels[anOffset] =
        static_cast<std::byte>(theValue);
}

[[nodiscard]] std::uint8_t GetChannel(
    const pvz::engine::DecodedImage& theImage,
    std::uint32_t theX,
    std::uint32_t theY,
    std::uint32_t theChannel)
{
    const auto anOffset =
        static_cast<std::size_t>(theY) *
            theImage.mBytesPerRow +
        static_cast<std::size_t>(theX) * 4U +
        theChannel;
    return std::to_integer<std::uint8_t>(
        theImage.mPixels[anOffset]);
}

void TestExactCropNormalization()
{
    auto aSource = MakeImage(802, 600, 1, 2, 3, 4);
    SetChannel(aSource, 1, 0, 2, 90);
    SetChannel(aSource, 800, 599, 0, 70);

    pvz::engine::DecodedImage aNormalized;
    pvz::parity::ImageComparisonError anError{};
    Expect(
        pvz::parity::NormalizeGoldenImage(
            aSource,
            {
                .mX = 1,
                .mY = 0,
                .mWidth = 800,
                .mHeight = 600,
            },
            aNormalized,
            anError),
        "exact 800x600 crop normalizes");
    Expect(
        aNormalized.mDescriptor.mSize.mWidth == 800 &&
            aNormalized.mDescriptor.mSize.mHeight == 600 &&
            aNormalized.mBytesPerRow == 3'200,
        "normalization produces the fixed golden dimensions");
    Expect(
        GetChannel(aNormalized, 0, 0, 2) == 90 &&
            GetChannel(aNormalized, 799, 599, 0) == 70,
        "exact crop copies the requested pixels without resampling");
}

void TestTwoTimesBilinearNormalization()
{
    auto aSource = MakeImage(1'600, 1'200);
    SetChannel(aSource, 0, 0, 0, 0);
    SetChannel(aSource, 1, 0, 0, 100);
    SetChannel(aSource, 0, 1, 0, 200);
    SetChannel(aSource, 1, 1, 0, 100);

    pvz::engine::DecodedImage aNormalized;
    pvz::parity::ImageComparisonError anError{};
    Expect(
        pvz::parity::NormalizeGoldenImage(
            aSource,
            {},
            aNormalized,
            anError),
        "two-times screenshot normalizes");
    Expect(
        GetChannel(aNormalized, 0, 0, 0) == 100 &&
            GetChannel(aNormalized, 0, 0, 3) == 255,
        "two-times normalization samples the logical pixel center");
}

void TestNormalizationRejectsAmbiguousGeometry()
{
    auto aWrongAspect = MakeImage(801, 600);
    pvz::engine::DecodedImage aNormalized;
    pvz::parity::ImageComparisonError anError{};
    Expect(
        !pvz::parity::NormalizeGoldenImage(
            aWrongAspect,
            {},
            aNormalized,
            anError) &&
            anError ==
                pvz::parity::ImageComparisonError::
                    AspectRatioMismatch,
        "normalization rejects implicit aspect-ratio distortion");
    Expect(
        !pvz::parity::NormalizeGoldenImage(
            aWrongAspect,
            {
                .mX = 0,
                .mY = 0,
                .mWidth = 800,
                .mHeight = 0,
            },
            aNormalized,
            anError) &&
            anError ==
                pvz::parity::ImageComparisonError::InvalidCrop,
        "normalization rejects half-specified crops");

    auto anInvalidStride = MakeImage(800, 600);
    anInvalidStride.mBytesPerRow = 3'199;
    Expect(
        !pvz::parity::NormalizeGoldenImage(
            anInvalidStride,
            {},
            aNormalized,
            anError) &&
            anError ==
                pvz::parity::ImageComparisonError::InvalidStride,
        "normalization rejects undersized row strides");

    auto aShortBuffer = MakeImage(800, 600);
    aShortBuffer.mPixels.pop_back();
    Expect(
        !pvz::parity::NormalizeGoldenImage(
            aShortBuffer,
            {},
            aNormalized,
            anError) &&
            anError ==
                pvz::parity::ImageComparisonError::BufferTooSmall,
        "normalization rejects truncated pixel buffers");
}

void TestDifferenceMetricsAndTolerance()
{
    auto aReference = MakeImage(800, 600);
    auto aCandidate = aReference;
    SetChannel(aCandidate, 10, 20, 0, 5);
    SetChannel(aCandidate, 10, 20, 1, 2);
    SetChannel(aCandidate, 10, 20, 2, 9);

    pvz::parity::ImageDifference aDifference;
    pvz::parity::ImageComparisonError anError{};
    Expect(
        pvz::parity::CompareGoldenImages(
            aReference,
            aCandidate,
            4,
            aDifference,
            anError),
        "normalized images compare");
    Expect(
        aDifference.mComparedPixels == 480'000 &&
            aDifference.mDifferentPixels == 1 &&
            aDifference.mMaximumChannelDifference == 9 &&
            aDifference.mTotalAbsoluteDifference == 16 &&
            aDifference.mTotalSquaredDifference == 110,
        "difference metrics retain fixed-width channel totals");
    Expect(
        aDifference.mHasDifference &&
            aDifference.mMinimumX == 10 &&
            aDifference.mMinimumY == 20 &&
            aDifference.mMaximumX == 10 &&
            aDifference.mMaximumY == 20,
        "difference bounds isolate the changed pixel");

    Expect(
        pvz::parity::CompareGoldenImages(
            aReference,
            aCandidate,
            9,
            aDifference,
            anError) &&
            aDifference.mDifferentPixels == 0 &&
            !aDifference.mHasDifference &&
            aDifference.mMaximumChannelDifference == 9,
        "channel tolerance affects mismatch classification, not metrics");
}

void TestPpmOutputUsesRgbAndAmplifiedDifference()
{
    auto aReference = MakeImage(800, 600, 1, 2, 3);
    auto aCandidate = aReference;
    SetChannel(aCandidate, 0, 0, 0, 6);
    SetChannel(aCandidate, 0, 0, 1, 4);
    SetChannel(aCandidate, 0, 0, 2, 12);
    SetChannel(aCandidate, 1, 0, 3, 0);

    std::vector<std::byte> aNormalizedPpm;
    std::vector<std::byte> aDifferencePpm;
    pvz::parity::ImageComparisonError anError{};
    Expect(
        pvz::parity::BuildNormalizedPpm(
            aReference,
            aNormalizedPpm,
            anError) &&
            pvz::parity::BuildDifferencePpm(
                aReference,
                aCandidate,
                aDifferencePpm,
                anError),
        "normalized and difference PPMs build");
    constexpr std::size_t kHeaderSize = 15;
    Expect(
        aNormalizedPpm.size() ==
                kHeaderSize + 800U * 600U * 3U &&
            aNormalizedPpm[kHeaderSize] == std::byte{3} &&
            aNormalizedPpm[kHeaderSize + 1U] == std::byte{2} &&
            aNormalizedPpm[kHeaderSize + 2U] == std::byte{1},
        "normalized PPM stores RGB channel order");
    Expect(
        aDifferencePpm[kHeaderSize] == std::byte{36} &&
            aDifferencePpm[kHeaderSize + 1U] == std::byte{8} &&
            aDifferencePpm[kHeaderSize + 2U] == std::byte{20},
        "difference PPM amplifies absolute RGB differences");
    Expect(
        aDifferencePpm[kHeaderSize + 3U] == std::byte{255} &&
            aDifferencePpm[kHeaderSize + 4U] == std::byte{255} &&
            aDifferencePpm[kHeaderSize + 5U] == std::byte{255},
        "difference PPM makes alpha-only differences visible");
}

} // namespace

int main()
{
    TestExactCropNormalization();
    TestTwoTimesBilinearNormalization();
    TestNormalizationRejectsAmbiguousGeometry();
    TestDifferenceMetricsAndTolerance();
    TestPpmOutputUsesRgbAndAmplifiedDifference();
    if (gFailures != 0)
    {
        std::cerr << gFailures << " image comparison test(s) failed\n";
        return 1;
    }
    std::cout << "image comparison tests passed\n";
    return 0;
}
