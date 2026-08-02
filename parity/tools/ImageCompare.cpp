#include "pvz/engine/image/PortableImageDecoder.h"
#include "pvz/parity/ImageComparison.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

inline constexpr std::uint64_t kMaximumFileBytes =
    256ULL * 1'024ULL * 1'024ULL;

struct Options
{
    std::filesystem::path mReferencePath;
    std::filesystem::path mCandidatePath;
    pvz::parity::ImageCrop mReferenceCrop;
    pvz::parity::ImageCrop mCandidateCrop;
    std::uint8_t mChannelTolerance{};
    std::uint64_t mMaximumDifferentPixels{};
    std::optional<std::filesystem::path> mDifferencePath;
    std::optional<std::filesystem::path> mNormalizedReferencePath;
    std::optional<std::filesystem::path> mNormalizedCandidatePath;
};

void PrintUsage()
{
    std::cerr
        << "usage: pvz_image_compare [options] reference-image "
           "candidate-image\n"
           "options:\n"
           "  --reference-crop=x,y,width,height\n"
           "  --candidate-crop=x,y,width,height\n"
           "  --channel-tolerance=0..255\n"
           "  --max-different-pixels=count\n"
           "  --write-diff=path.ppm\n"
           "  --write-normalized-reference=path.ppm\n"
           "  --write-normalized-candidate=path.ppm\n";
}

[[nodiscard]] bool ParseUnsigned(
    std::string_view theText,
    std::uint64_t& theValue)
{
    if (theText.empty())
        return false;
    std::uint64_t aValue{};
    const auto [anEnd, anError] = std::from_chars(
        theText.data(),
        theText.data() + theText.size(),
        aValue);
    if (anError != std::errc{} ||
        anEnd != theText.data() + theText.size())
    {
        return false;
    }
    theValue = aValue;
    return true;
}

[[nodiscard]] bool ParseCrop(
    std::string_view theText,
    pvz::parity::ImageCrop& theCrop)
{
    std::array<std::uint32_t, 4> aValues{};
    for (std::size_t anIndex = 0;
         anIndex < aValues.size();
         ++anIndex)
    {
        const auto aSeparator = theText.find(',');
        const auto aPart = theText.substr(0, aSeparator);
        std::uint64_t aValue{};
        if (!ParseUnsigned(aPart, aValue) ||
            aValue > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }
        aValues[anIndex] = static_cast<std::uint32_t>(aValue);
        if (anIndex + 1U == aValues.size())
        {
            if (aSeparator != std::string_view::npos)
                return false;
        }
        else
        {
            if (aSeparator == std::string_view::npos)
                return false;
            theText.remove_prefix(aSeparator + 1U);
        }
    }
    theCrop = {
        .mX = aValues[0],
        .mY = aValues[1],
        .mWidth = aValues[2],
        .mHeight = aValues[3],
    };
    return true;
}

[[nodiscard]] bool ReadOptionValue(
    std::string_view theArgument,
    std::string_view theName,
    std::string_view& theValue)
{
    if (!theArgument.starts_with(theName) ||
        theArgument.size() <= theName.size() ||
        theArgument[theName.size()] != '=')
    {
        return false;
    }
    theValue = theArgument.substr(theName.size() + 1U);
    return !theValue.empty();
}

[[nodiscard]] bool ParseOptions(
    int theArgumentCount,
    char** theArguments,
    Options& theOptions)
{
    std::vector<std::filesystem::path> aPaths;
    for (int anIndex = 1;
         anIndex < theArgumentCount;
         ++anIndex)
    {
        const std::string_view anArgument(theArguments[anIndex]);
        std::string_view aValue;
        if (ReadOptionValue(
                anArgument,
                "--reference-crop",
                aValue))
        {
            if (!ParseCrop(aValue, theOptions.mReferenceCrop))
                return false;
        }
        else if (ReadOptionValue(
                     anArgument,
                     "--candidate-crop",
                     aValue))
        {
            if (!ParseCrop(aValue, theOptions.mCandidateCrop))
                return false;
        }
        else if (ReadOptionValue(
                     anArgument,
                     "--channel-tolerance",
                     aValue))
        {
            std::uint64_t aTolerance{};
            if (!ParseUnsigned(aValue, aTolerance) ||
                aTolerance > 255U)
            {
                return false;
            }
            theOptions.mChannelTolerance =
                static_cast<std::uint8_t>(aTolerance);
        }
        else if (ReadOptionValue(
                     anArgument,
                     "--max-different-pixels",
                     aValue))
        {
            if (!ParseUnsigned(
                    aValue,
                    theOptions.mMaximumDifferentPixels))
            {
                return false;
            }
        }
        else if (ReadOptionValue(
                     anArgument,
                     "--write-diff",
                     aValue))
        {
            theOptions.mDifferencePath =
                std::filesystem::path(aValue);
        }
        else if (ReadOptionValue(
                     anArgument,
                     "--write-normalized-reference",
                     aValue))
        {
            theOptions.mNormalizedReferencePath =
                std::filesystem::path(aValue);
        }
        else if (ReadOptionValue(
                     anArgument,
                     "--write-normalized-candidate",
                     aValue))
        {
            theOptions.mNormalizedCandidatePath =
                std::filesystem::path(aValue);
        }
        else if (anArgument.starts_with("--"))
        {
            return false;
        }
        else
        {
            aPaths.emplace_back(anArgument);
        }
    }
    if (aPaths.size() != 2)
        return false;
    theOptions.mReferencePath = std::move(aPaths[0]);
    theOptions.mCandidatePath = std::move(aPaths[1]);
    return true;
}

[[nodiscard]] bool ReadFile(
    const std::filesystem::path& thePath,
    std::vector<std::byte>& theBytes)
{
    std::ifstream aStream(thePath, std::ios::binary | std::ios::ate);
    if (!aStream)
        return false;
    const auto aSize = aStream.tellg();
    if (aSize < 0 ||
        static_cast<std::uint64_t>(aSize) > kMaximumFileBytes)
    {
        return false;
    }
    std::vector<std::byte> aBytes(
        static_cast<std::size_t>(aSize));
    aStream.seekg(0);
    if (!aBytes.empty())
    {
        aStream.read(
            reinterpret_cast<char*>(aBytes.data()),
            static_cast<std::streamsize>(aBytes.size()));
    }
    if (!aStream)
        return false;
    theBytes = std::move(aBytes);
    return true;
}

[[nodiscard]] bool WriteFile(
    const std::filesystem::path& thePath,
    std::span<const std::byte> theBytes)
{
    std::ofstream aStream(
        thePath,
        std::ios::binary | std::ios::trunc);
    if (!aStream)
        return false;
    if (!theBytes.empty())
    {
        aStream.write(
            reinterpret_cast<const char*>(theBytes.data()),
            static_cast<std::streamsize>(theBytes.size()));
    }
    return static_cast<bool>(aStream);
}

[[nodiscard]] const char* ErrorName(
    pvz::parity::ImageComparisonError theError)
{
    using Error = pvz::parity::ImageComparisonError;
    switch (theError)
    {
    case Error::None:
        return "none";
    case Error::UnsupportedPixelFormat:
        return "unsupported-pixel-format";
    case Error::InvalidDimensions:
        return "invalid-dimensions";
    case Error::InvalidStride:
        return "invalid-stride";
    case Error::BufferTooSmall:
        return "buffer-too-small";
    case Error::InvalidCrop:
        return "invalid-crop";
    case Error::AspectRatioMismatch:
        return "aspect-ratio-mismatch";
    case Error::InvalidNormalizedImage:
        return "invalid-normalized-image";
    }
    return "unknown";
}

[[nodiscard]] const char* DecodeErrorName(
    pvz::engine::ImageDecodeError theError)
{
    using Error = pvz::engine::ImageDecodeError;
    switch (theError)
    {
    case Error::None:
        return "none";
    case Error::EmptyInput:
        return "empty-input";
    case Error::UnsupportedFormat:
        return "unsupported-format";
    case Error::InvalidData:
        return "invalid-data";
    case Error::DimensionsUnsupported:
        return "dimensions-unsupported";
    case Error::SizeOverflow:
        return "size-overflow";
    case Error::AllocationFailed:
        return "allocation-failed";
    }
    return "unknown";
}

[[nodiscard]] bool DecodeImage(
    const std::filesystem::path& thePath,
    pvz::engine::DecodedImage& theImage)
{
    std::vector<std::byte> aBytes;
    if (!ReadFile(thePath, aBytes))
    {
        std::cerr << thePath.string()
                  << ": could not read image\n";
        return false;
    }
    const pvz::engine::image::PortableImageDecoder aDecoder;
    pvz::engine::ImageDecodeError anError{};
    if (!aDecoder.Decode(aBytes, theImage, anError))
    {
        std::cerr << thePath.string()
                  << ": decode failed: "
                  << DecodeErrorName(anError) << '\n';
        return false;
    }
    return true;
}

[[nodiscard]] bool WritePpm(
    const std::optional<std::filesystem::path>& thePath,
    const pvz::engine::DecodedImage& theReference,
    const pvz::engine::DecodedImage* theCandidate)
{
    if (!thePath.has_value())
        return true;
    std::vector<std::byte> aPpm;
    pvz::parity::ImageComparisonError anError{};
    const auto wasBuilt = theCandidate == nullptr
        ? pvz::parity::BuildNormalizedPpm(
              theReference,
              aPpm,
              anError)
        : pvz::parity::BuildDifferencePpm(
              theReference,
              *theCandidate,
              aPpm,
              anError);
    if (!wasBuilt)
    {
        std::cerr << thePath->string()
                  << ": PPM generation failed: "
                  << ErrorName(anError) << '\n';
        return false;
    }
    if (!WriteFile(*thePath, aPpm))
    {
        std::cerr << thePath->string()
                  << ": could not write PPM\n";
        return false;
    }
    return true;
}

[[nodiscard]] pvz::parity::ImageCrop ResolveCrop(
    pvz::parity::ImageCrop theCrop,
    const pvz::engine::DecodedImage& theImage)
{
    if (theCrop.mWidth == 0 && theCrop.mHeight == 0)
    {
        theCrop.mWidth = theImage.mDescriptor.mSize.mWidth;
        theCrop.mHeight = theImage.mDescriptor.mSize.mHeight;
    }
    return theCrop;
}

void PrintImage(
    std::string_view theName,
    const pvz::engine::DecodedImage& theImage,
    pvz::parity::ImageCrop theCrop)
{
    std::cout
        << theName << "-size="
        << theImage.mDescriptor.mSize.mWidth << 'x'
        << theImage.mDescriptor.mSize.mHeight
        << " crop=" << theCrop.mX << ',' << theCrop.mY
        << ',' << theCrop.mWidth << ',' << theCrop.mHeight
        << '\n';
}

} // namespace

int main(int theArgumentCount, char** theArguments)
{
    Options anOptions;
    if (!ParseOptions(
            theArgumentCount,
            theArguments,
            anOptions))
    {
        PrintUsage();
        return 2;
    }

    pvz::engine::DecodedImage aReference;
    pvz::engine::DecodedImage aCandidate;
    if (!DecodeImage(anOptions.mReferencePath, aReference) ||
        !DecodeImage(anOptions.mCandidatePath, aCandidate))
    {
        return 2;
    }
    const auto aReferenceCrop = ResolveCrop(
        anOptions.mReferenceCrop,
        aReference);
    const auto aCandidateCrop = ResolveCrop(
        anOptions.mCandidateCrop,
        aCandidate);
    PrintImage("reference", aReference, aReferenceCrop);
    PrintImage("candidate", aCandidate, aCandidateCrop);

    pvz::engine::DecodedImage aNormalizedReference;
    pvz::engine::DecodedImage aNormalizedCandidate;
    pvz::parity::ImageComparisonError anError{};
    if (!pvz::parity::NormalizeGoldenImage(
            aReference,
            aReferenceCrop,
            aNormalizedReference,
            anError))
    {
        std::cerr << "reference normalization failed: "
                  << ErrorName(anError) << '\n';
        return 2;
    }
    if (!pvz::parity::NormalizeGoldenImage(
            aCandidate,
            aCandidateCrop,
            aNormalizedCandidate,
            anError))
    {
        std::cerr << "candidate normalization failed: "
                  << ErrorName(anError) << '\n';
        return 2;
    }

    pvz::parity::ImageDifference aDifference;
    if (!pvz::parity::CompareGoldenImages(
            aNormalizedReference,
            aNormalizedCandidate,
            anOptions.mChannelTolerance,
            aDifference,
            anError))
    {
        std::cerr << "comparison failed: "
                  << ErrorName(anError) << '\n';
        return 2;
    }
    if (!WritePpm(
            anOptions.mNormalizedReferencePath,
            aNormalizedReference,
            nullptr) ||
        !WritePpm(
            anOptions.mNormalizedCandidatePath,
            aNormalizedCandidate,
            nullptr) ||
        !WritePpm(
            anOptions.mDifferencePath,
            aNormalizedReference,
            &aNormalizedCandidate))
    {
        return 2;
    }

    const auto aChannelCount =
        aDifference.mComparedPixels * 4U;
    const auto aMeanAbsoluteDifference =
        static_cast<double>(
            aDifference.mTotalAbsoluteDifference) /
        static_cast<double>(aChannelCount);
    const auto aRootMeanSquaredDifference = std::sqrt(
        static_cast<double>(
            aDifference.mTotalSquaredDifference) /
        static_cast<double>(aChannelCount));
    std::cout
        << "normalized-size="
        << pvz::parity::kGoldenImageWidth << 'x'
        << pvz::parity::kGoldenImageHeight << '\n'
        << "different-pixels="
        << aDifference.mDifferentPixels
        << " compared-pixels="
        << aDifference.mComparedPixels
        << " max-channel-difference="
        << static_cast<std::uint32_t>(
               aDifference.mMaximumChannelDifference)
        << " mean-absolute-difference="
        << std::fixed << std::setprecision(6)
        << aMeanAbsoluteDifference
        << " root-mean-square-difference="
        << aRootMeanSquaredDifference;
    if (aDifference.mHasDifference)
    {
        std::cout
            << " difference-bounds="
            << aDifference.mMinimumX << ','
            << aDifference.mMinimumY << ','
            << aDifference.mMaximumX -
                   aDifference.mMinimumX + 1U
            << ','
            << aDifference.mMaximumY -
                   aDifference.mMinimumY + 1U;
    }
    else
    {
        std::cout << " difference-bounds=none";
    }
    std::cout << '\n';

    const auto isMatch =
        aDifference.mDifferentPixels <=
        anOptions.mMaximumDifferentPixels;
    std::cout
        << "image-comparison="
        << (isMatch ? "match" : "mismatch")
        << " channel-tolerance="
        << static_cast<std::uint32_t>(
               anOptions.mChannelTolerance)
        << " max-different-pixels="
        << anOptions.mMaximumDifferentPixels << '\n';
    return isMatch ? 0 : 1;
}
