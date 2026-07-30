#include "pvz/engine/core/ImageResourceManager.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pvz::engine::core
{
namespace
{

inline constexpr std::uint32_t kBytesPerPixel = 4;
inline constexpr std::array<std::string_view, 4> kImageExtensions{
    ".tga",
    ".jpg",
    ".png",
    ".gif",
};

enum class ResourceReadResult : std::uint8_t
{
    Found,
    NotFound,
    ReadFailed,
};

[[nodiscard]] std::string_view TrimTrailingSlashes(
    std::string_view thePath)
{
    while (!thePath.empty() &&
           (thePath.back() == '/' || thePath.back() == '\\'))
    {
        thePath.remove_suffix(1);
    }
    return thePath;
}

[[nodiscard]] std::string JoinPath(
    std::string_view theDirectory,
    std::string_view thePath)
{
    if (thePath.empty() || thePath.front() == '!')
        return std::string(thePath);

    const auto aDirectory = TrimTrailingSlashes(theDirectory);
    if (aDirectory.empty())
        return std::string(thePath);

    std::string aResult(aDirectory);
    aResult.push_back('/');
    aResult.append(thePath);
    return aResult;
}

[[nodiscard]] std::string GetFileStem(std::string_view thePath)
{
    const auto aSlash = thePath.find_last_of("/\\");
    const auto aStart =
        aSlash == std::string_view::npos ? 0 : aSlash + 1;
    const auto aDot = thePath.find_last_of('.');
    const auto anEnd =
        aDot == std::string_view::npos || aDot < aStart
            ? thePath.size()
            : aDot;
    return std::string(thePath.substr(aStart, anEnd - aStart));
}

[[nodiscard]] bool HasImageExtension(std::string_view thePath)
{
    const auto aSlash = thePath.find_last_of("/\\");
    const auto aDot = thePath.find_last_of('.');
    return aDot != std::string_view::npos &&
           (aSlash == std::string_view::npos || aDot > aSlash);
}

[[nodiscard]] ResourceReadResult ReadImageResource(
    const IResourceStore& theResources,
    std::string_view theLogicalPath,
    std::string& theResolvedPath,
    std::vector<std::byte>& theBytes)
{
    const auto aTryPath =
        [&](std::string_view theCandidate) -> ResourceReadResult
    {
        if (!theResources.Contains(theCandidate))
            return ResourceReadResult::NotFound;
        if (!theResources.ReadAll(theCandidate, theBytes))
            return ResourceReadResult::ReadFailed;
        theResolvedPath = theCandidate;
        return ResourceReadResult::Found;
    };

    if (HasImageExtension(theLogicalPath))
        return aTryPath(theLogicalPath);

    for (const auto anExtension : kImageExtensions)
    {
        std::string aCandidate(theLogicalPath);
        aCandidate.append(anExtension);
        const auto aResult = aTryPath(aCandidate);
        if (aResult != ResourceReadResult::NotFound)
            return aResult;
    }
    return ResourceReadResult::NotFound;
}

[[nodiscard]] std::string MakePrefixedAlphaPath(
    std::string_view thePath)
{
    const auto aSlash = thePath.find_last_of("/\\");
    const auto aNameStart =
        aSlash == std::string_view::npos ? 0 : aSlash + 1;
    std::string aResult(thePath.substr(0, aNameStart));
    aResult.push_back('_');
    aResult.append(thePath.substr(aNameStart));
    return aResult;
}

[[nodiscard]] std::string MakeSuffixedAlphaPath(
    std::string_view thePath)
{
    std::string aResult(thePath);
    aResult.push_back('_');
    return aResult;
}

[[nodiscard]] bool ParsePositiveInteger(
    std::string_view theText,
    std::uint32_t& theValue)
{
    if (theText.empty())
        return false;
    std::uint32_t aValue{};
    const auto [anEnd, anError] = std::from_chars(
        theText.data(),
        theText.data() + theText.size(),
        aValue);
    if (anError != std::errc{} ||
        anEnd != theText.data() + theText.size() ||
        aValue == 0)
    {
        return false;
    }
    theValue = aValue;
    return true;
}

[[nodiscard]] bool ParseColor(
    std::string_view theText,
    ColorRgba8& theColor)
{
    if (theText.empty() || theText.size() > 6)
        return false;

    std::uint32_t aValue{};
    const auto [anEnd, anError] = std::from_chars(
        theText.data(),
        theText.data() + theText.size(),
        aValue,
        16);
    if (anError != std::errc{} ||
        anEnd != theText.data() + theText.size() ||
        aValue > 0x00FF'FFFF)
    {
        return false;
    }
    theColor = {
        .mRed = static_cast<std::uint8_t>((aValue >> 16) & 0xFF),
        .mGreen = static_cast<std::uint8_t>((aValue >> 8) & 0xFF),
        .mBlue = static_cast<std::uint8_t>(aValue & 0xFF),
        .mAlpha = 255,
    };
    return true;
}

[[nodiscard]] std::uint64_t MakeHandleKey(ImageHandle theImage)
{
    return (static_cast<std::uint64_t>(theImage.mGeneration) << 32) |
           theImage.mIndex;
}

[[nodiscard]] bool ValidateDecodedImage(
    const DecodedImage& theImage)
{
    const auto aWidth = theImage.mDescriptor.mSize.mWidth;
    const auto aHeight = theImage.mDescriptor.mSize.mHeight;
    if (aWidth == 0 ||
        aHeight == 0 ||
        theImage.mDescriptor.mPixelFormat !=
            ImagePixelFormat::Bgra8Unorm ||
        aWidth >
            std::numeric_limits<std::uint32_t>::max() /
                kBytesPerPixel)
    {
        return false;
    }

    const auto aMinimumRowBytes = aWidth * kBytesPerPixel;
    if (theImage.mBytesPerRow < aMinimumRowBytes)
        return false;

    const auto aRequiredSize =
        static_cast<std::uint64_t>(theImage.mBytesPerRow) *
        aHeight;
    return aRequiredSize <= theImage.mPixels.size();
}

[[nodiscard]] bool DecodeResource(
    const IImageDecoder& theDecoder,
    std::span<const std::byte> theBytes,
    std::string_view thePath,
    DecodedImage& theImage,
    ImageResourceDiagnostic& theDiagnostic)
{
    ImageDecodeError anError{};
    if (!theDecoder.Decode(theBytes, theImage, anError) ||
        !ValidateDecodedImage(theImage))
    {
        theDiagnostic = {
            .mError = ImageResourceError::DecodeFailed,
            .mDecodeError =
                anError == ImageDecodeError::None
                    ? ImageDecodeError::InvalidData
                    : anError,
            .mPath = std::string(thePath),
        };
        return false;
    }
    return true;
}

void FillColor(
    DecodedImage& theImage,
    ColorRgba8 theColor)
{
    const auto aSize = theImage.mDescriptor.mSize;
    for (std::uint32_t aY = 0; aY < aSize.mHeight; ++aY)
    {
        auto* aRow =
            theImage.mPixels.data() +
            static_cast<std::size_t>(aY) *
                theImage.mBytesPerRow;
        for (std::uint32_t anX = 0; anX < aSize.mWidth; ++anX)
        {
            auto* aPixel =
                aRow + static_cast<std::size_t>(anX) *
                           kBytesPerPixel;
            const auto anAlpha = aPixel[0];
            aPixel[0] = static_cast<std::byte>(theColor.mBlue);
            aPixel[1] = static_cast<std::byte>(theColor.mGreen);
            aPixel[2] = static_cast<std::byte>(theColor.mRed);
            aPixel[3] = anAlpha;
        }
    }
}

[[nodiscard]] bool ApplyAlphaImage(
    DecodedImage& theBase,
    const DecodedImage& theAlpha,
    std::uint32_t theRows,
    std::uint32_t theColumns,
    bool theIsGrid)
{
    const auto aBaseSize = theBase.mDescriptor.mSize;
    const auto anAlphaSize = theAlpha.mDescriptor.mSize;
    if (!theIsGrid)
    {
        if (aBaseSize.mWidth != anAlphaSize.mWidth ||
            aBaseSize.mHeight != anAlphaSize.mHeight)
        {
            return false;
        }
    }
    else
    {
        if (theRows == 0 ||
            theColumns == 0 ||
            aBaseSize.mWidth % theColumns != 0 ||
            aBaseSize.mHeight % theRows != 0 ||
            anAlphaSize.mWidth !=
                aBaseSize.mWidth / theColumns ||
            anAlphaSize.mHeight !=
                aBaseSize.mHeight / theRows)
        {
            return false;
        }
    }

    for (std::uint32_t aY = 0; aY < aBaseSize.mHeight; ++aY)
    {
        auto* aBaseRow =
            theBase.mPixels.data() +
            static_cast<std::size_t>(aY) *
                theBase.mBytesPerRow;
        const auto anAlphaY =
            theIsGrid ? aY % anAlphaSize.mHeight : aY;
        const auto* anAlphaRow =
            theAlpha.mPixels.data() +
            static_cast<std::size_t>(anAlphaY) *
                theAlpha.mBytesPerRow;
        for (std::uint32_t anX = 0;
             anX < aBaseSize.mWidth;
             ++anX)
        {
            const auto anAlphaX =
                theIsGrid ? anX % anAlphaSize.mWidth : anX;
            aBaseRow[
                static_cast<std::size_t>(anX) *
                    kBytesPerPixel +
                3] =
                anAlphaRow[
                    static_cast<std::size_t>(anAlphaX) *
                    kBytesPerPixel];
        }
    }
    return true;
}

} // namespace

ImageResourceManager::ImageResourceManager(
    const IResourceStore& theResourceStore,
    const IXmlDocumentLoader& theDocuments,
    const IImageDecoder& theDecoder,
    IImageStore& theImages)
    : mResourceStore(theResourceStore),
      mDocuments(theDocuments),
      mDecoder(theDecoder),
      mImages(theImages)
{
}

ImageResourceManager::~ImageResourceManager()
{
    ReleaseAll();
}

bool ImageResourceManager::LoadManifest(std::string_view thePath)
{
    ResetManifestError();
    if (!mLoadedResources.empty())
    {
        FailManifest(ImageManifestError::ResourcesAreLoaded, 0);
        return false;
    }
    mDefinitions.clear();
    mManifestLoaded = false;

    std::vector<XmlNode> aRoots;
    if (!mDocuments.Load(
            thePath,
            XmlDocumentMode::SingleRoot,
            aRoots,
            mManifestDocumentDiagnostic))
    {
        FailManifest(
            ImageManifestError::SourceDocument,
            mManifestDocumentDiagnostic.mLine);
        return false;
    }
    if (aRoots.size() != 1 ||
        aRoots.front().mName != "ResourceManifest")
    {
        FailManifest(
            ImageManifestError::InvalidRoot,
            aRoots.empty() ? 0 : aRoots.front().mLine);
        return false;
    }

    std::unordered_map<std::string, Definition> aDefinitions;
    if (!ParseManifest(aRoots.front(), aDefinitions))
        return false;

    mDefinitions = std::move(aDefinitions);
    mManifestLoaded = true;
    return true;
}

bool ImageResourceManager::IsManifestLoaded() const
{
    return mManifestLoaded;
}

std::size_t ImageResourceManager::GetDefinitionCount() const
{
    return mDefinitions.size();
}

ImageManifestError ImageResourceManager::GetManifestError() const
{
    return mManifestError;
}

XmlDocumentDiagnostic
ImageResourceManager::GetManifestDocumentDiagnostic() const
{
    return mManifestDocumentDiagnostic;
}

std::uint32_t ImageResourceManager::GetManifestErrorLine() const
{
    return mManifestErrorLine;
}

bool ImageResourceManager::Load(
    std::string_view theResourceId,
    ImageResource& theResource,
    ImageResourceDiagnostic& theDiagnostic)
{
    theResource = {};
    theDiagnostic = {};
    if (!mManifestLoaded)
    {
        theDiagnostic.mError =
            ImageResourceError::ManifestNotLoaded;
        return false;
    }

    const auto aDefinition = mDefinitions.find(
        std::string(theResourceId));
    if (aDefinition == mDefinitions.end())
    {
        theDiagnostic.mError =
            ImageResourceError::ResourceNotFound;
        return false;
    }
    return LoadDefinition(
        theResourceId,
        aDefinition->second,
        theResource,
        theDiagnostic);
}

bool ImageResourceManager::LoadSource(
    std::string_view theLogicalPath,
    ImageResource& theResource,
    ImageResourceDiagnostic& theDiagnostic)
{
    theResource = {};
    theDiagnostic = {};
    if (theLogicalPath.empty())
    {
        theDiagnostic.mError = ImageResourceError::SourceNotFound;
        return false;
    }

    std::string aCacheKey(1, '\0');
    aCacheKey.append("source:");
    aCacheKey.append(theLogicalPath);
    Definition aDefinition;
    aDefinition.mPath = theLogicalPath;
    return LoadDefinition(
        aCacheKey,
        aDefinition,
        theResource,
        theDiagnostic);
}

bool ImageResourceManager::LoadDefinition(
    std::string_view theCacheKey,
    const Definition& theDefinition,
    ImageResource& theResource,
    ImageResourceDiagnostic& theDiagnostic)
{
    const auto aLoaded = mLoadedResources.find(
        std::string(theCacheKey));
    if (aLoaded != mLoadedResources.end())
    {
        if (aLoaded->second.mReferenceCount ==
            std::numeric_limits<std::uint32_t>::max())
        {
            theDiagnostic.mError = ImageResourceError::UploadFailed;
            return false;
        }
        ++aLoaded->second.mReferenceCount;
        theResource = aLoaded->second.mResource;
        return true;
    }

    const auto& aDefinitionValue = theDefinition;
    std::string aBasePath;
    std::vector<std::byte> aBaseBytes;
    const auto aBaseRead = ReadImageResource(
        mResourceStore,
        aDefinitionValue.mPath,
        aBasePath,
        aBaseBytes);
    if (aBaseRead == ResourceReadResult::ReadFailed)
    {
        theDiagnostic = {
            .mError = ImageResourceError::SourceReadFailed,
            .mPath = aDefinitionValue.mPath,
        };
        return false;
    }

    DecodedImage aBaseImage;
    const bool hasBaseImage =
        aBaseRead == ResourceReadResult::Found;
    if (hasBaseImage &&
        !DecodeResource(
            mDecoder,
            aBaseBytes,
            aBasePath,
            aBaseImage,
            theDiagnostic))
    {
        return false;
    }

    std::string anAlphaLogicalPath;
    bool anAlphaIsGrid = false;
    bool shouldTryAlpha = false;
    if (!aDefinitionValue.mAlphaPath.empty())
    {
        anAlphaLogicalPath = aDefinitionValue.mAlphaPath;
        anAlphaIsGrid = aDefinitionValue.mAlphaGrid;
        shouldTryAlpha = true;
    }
    else if (aDefinitionValue.mAutoFindAlpha)
    {
        anAlphaLogicalPath =
            MakePrefixedAlphaPath(aDefinitionValue.mPath);
        shouldTryAlpha = true;
    }

    DecodedImage anAlphaImage;
    bool hasAlphaImage = false;
    if (shouldTryAlpha)
    {
        std::string anAlphaPath;
        std::vector<std::byte> anAlphaBytes;
        auto anAlphaRead = ReadImageResource(
            mResourceStore,
            anAlphaLogicalPath,
            anAlphaPath,
            anAlphaBytes);
        if (anAlphaRead == ResourceReadResult::NotFound &&
            aDefinitionValue.mAlphaPath.empty())
        {
            anAlphaLogicalPath =
                MakeSuffixedAlphaPath(aDefinitionValue.mPath);
            anAlphaRead = ReadImageResource(
                mResourceStore,
                anAlphaLogicalPath,
                anAlphaPath,
                anAlphaBytes);
        }
        if (anAlphaRead == ResourceReadResult::ReadFailed)
        {
            theDiagnostic = {
                .mError = ImageResourceError::SourceReadFailed,
                .mPath = anAlphaLogicalPath,
            };
            return false;
        }
        if (anAlphaRead == ResourceReadResult::Found)
        {
            if (!DecodeResource(
                    mDecoder,
                    anAlphaBytes,
                    anAlphaPath,
                    anAlphaImage,
                    theDiagnostic))
            {
                return false;
            }
            hasAlphaImage = true;
        }
        else if (!aDefinitionValue.mAlphaPath.empty())
        {
            theDiagnostic = {
                .mError = ImageResourceError::SourceNotFound,
                .mPath = anAlphaLogicalPath,
            };
            return false;
        }
    }

    if (!hasBaseImage)
    {
        if (!hasAlphaImage)
        {
            theDiagnostic = {
                .mError = ImageResourceError::SourceNotFound,
                .mPath = aDefinitionValue.mPath,
            };
            return false;
        }
        aBaseImage = std::move(anAlphaImage);
        FillColor(aBaseImage, aDefinitionValue.mAlphaColor);
        hasAlphaImage = false;
    }

    if (hasAlphaImage &&
        !ApplyAlphaImage(
            aBaseImage,
            anAlphaImage,
            aDefinitionValue.mRows,
            aDefinitionValue.mColumns,
            anAlphaIsGrid))
    {
        theDiagnostic = {
            .mError = ImageResourceError::AlphaSizeMismatch,
            .mPath = anAlphaLogicalPath,
        };
        return false;
    }

    ImageHandle anImage;
    if (!mImages.CreateImage(
            aBaseImage.mDescriptor,
            aBaseImage.mPixels,
            aBaseImage.mBytesPerRow,
            anImage))
    {
        theDiagnostic = {
            .mError = ImageResourceError::UploadFailed,
            .mPath = aBasePath,
        };
        return false;
    }

    const ImageResource aResource{
        .mImage = anImage,
        .mSize = aBaseImage.mDescriptor.mSize,
        .mRows = aDefinitionValue.mRows,
        .mColumns = aDefinitionValue.mColumns,
    };
    const std::string anId(theCacheKey);
    mLoadedResources.emplace(
        anId,
        LoadedResource{
            .mResource = aResource,
            .mReferenceCount = 1,
        });
    mIdsByHandle.emplace(MakeHandleKey(anImage), anId);
    theResource = aResource;
    return true;
}

void ImageResourceManager::Release(ImageHandle theImage)
{
    const auto anId = mIdsByHandle.find(MakeHandleKey(theImage));
    if (anId == mIdsByHandle.end())
        return;
    const auto aLoaded = mLoadedResources.find(anId->second);
    if (aLoaded == mLoadedResources.end())
        return;
    if (aLoaded->second.mReferenceCount > 1)
    {
        --aLoaded->second.mReferenceCount;
        return;
    }

    mImages.DestroyImage(aLoaded->second.mResource.mImage);
    mLoadedResources.erase(aLoaded);
    mIdsByHandle.erase(anId);
}

void ImageResourceManager::ResetManifestError()
{
    mManifestError = ImageManifestError::None;
    mManifestDocumentDiagnostic = {};
    mManifestErrorLine = 0;
}

bool ImageResourceManager::ParseManifest(
    const XmlNode& theRoot,
    std::unordered_map<std::string, Definition>& theDefinitions)
{
    std::string aDefaultPath;
    std::string aDefaultIdPrefix;
    for (const auto& aGroup : theRoot.mChildren)
    {
        if (aGroup.mName != "Resources")
        {
            FailManifest(
                ImageManifestError::InvalidSection,
                aGroup.mLine);
            return false;
        }

        for (const auto& aNode : aGroup.mChildren)
        {
            if (aNode.mName == "SetDefaults")
            {
                if (const auto* aPath =
                        aNode.FindAttribute("path"))
                {
                    aDefaultPath =
                        TrimTrailingSlashes(*aPath);
                }
                if (const auto* anIdPrefix =
                        aNode.FindAttribute("idprefix"))
                {
                    aDefaultIdPrefix =
                        TrimTrailingSlashes(*anIdPrefix);
                }
                continue;
            }
            if (aNode.mName == "Sound" ||
                aNode.mName == "Font")
            {
                continue;
            }
            if (aNode.mName != "Image")
            {
                FailManifest(
                    ImageManifestError::InvalidSection,
                    aNode.mLine);
                return false;
            }

            const auto* aPath = aNode.FindAttribute("path");
            if (aPath == nullptr || aPath->empty())
            {
                FailManifest(
                    ImageManifestError::MissingAttribute,
                    aNode.mLine);
                return false;
            }

            std::string anId = aDefaultIdPrefix;
            if (const auto* anExplicitId =
                    aNode.FindAttribute("id"))
            {
                anId.append(*anExplicitId);
            }
            else
            {
                anId.append(GetFileStem(*aPath));
            }
            if (anId.empty())
            {
                FailManifest(
                    ImageManifestError::MissingAttribute,
                    aNode.mLine);
                return false;
            }

            Definition aDefinition;
            aDefinition.mPath = JoinPath(aDefaultPath, *aPath);
            aDefinition.mAutoFindAlpha =
                aNode.FindAttribute("noalpha") == nullptr;
            if (const auto* aRows = aNode.FindAttribute("rows");
                aRows != nullptr &&
                !ParsePositiveInteger(*aRows, aDefinition.mRows))
            {
                FailManifest(
                    ImageManifestError::InvalidInteger,
                    aNode.mLine);
                return false;
            }
            if (const auto* aColumns = aNode.FindAttribute("cols");
                aColumns != nullptr &&
                !ParsePositiveInteger(
                    *aColumns,
                    aDefinition.mColumns))
            {
                FailManifest(
                    ImageManifestError::InvalidInteger,
                    aNode.mLine);
                return false;
            }

            const auto* anAlphaImage =
                aNode.FindAttribute("alphaimage");
            const auto* anAlphaGrid =
                aNode.FindAttribute("alphagrid");
            if (anAlphaImage != nullptr && anAlphaGrid != nullptr)
            {
                FailManifest(
                    ImageManifestError::ConflictingAlphaSources,
                    aNode.mLine);
                return false;
            }
            if (anAlphaImage != nullptr)
            {
                aDefinition.mAlphaPath =
                    JoinPath(aDefaultPath, *anAlphaImage);
            }
            else if (anAlphaGrid != nullptr)
            {
                aDefinition.mAlphaPath =
                    JoinPath(aDefaultPath, *anAlphaGrid);
                aDefinition.mAlphaGrid = true;
            }
            if (const auto* anAlphaColor =
                    aNode.FindAttribute("alphacolor");
                anAlphaColor != nullptr &&
                !ParseColor(
                    *anAlphaColor,
                    aDefinition.mAlphaColor))
            {
                FailManifest(
                    ImageManifestError::InvalidColor,
                    aNode.mLine);
                return false;
            }

            if (!theDefinitions.emplace(
                    std::move(anId),
                    std::move(aDefinition)).second)
            {
                FailManifest(
                    ImageManifestError::DuplicateResource,
                    aNode.mLine);
                return false;
            }
        }
    }
    return true;
}

void ImageResourceManager::FailManifest(
    ImageManifestError theError,
    std::uint32_t theLine)
{
    mManifestError = theError;
    mManifestErrorLine = theLine;
    mManifestLoaded = false;
}

void ImageResourceManager::ReleaseAll()
{
    for (const auto& [anId, aLoaded] : mLoadedResources)
    {
        static_cast<void>(anId);
        mImages.DestroyImage(aLoaded.mResource.mImage);
    }
    mIdsByHandle.clear();
    mLoadedResources.clear();
}

const char* GetImageManifestErrorMessage(
    ImageManifestError theError)
{
    switch (theError)
    {
    case ImageManifestError::None:
        return "no error";
    case ImageManifestError::SourceDocument:
        return "image manifest could not be read";
    case ImageManifestError::InvalidRoot:
        return "image manifest root is invalid";
    case ImageManifestError::InvalidSection:
        return "image manifest contains an invalid section";
    case ImageManifestError::MissingAttribute:
        return "image manifest is missing a required attribute";
    case ImageManifestError::InvalidInteger:
        return "image manifest contains an invalid integer";
    case ImageManifestError::InvalidColor:
        return "image manifest contains an invalid color";
    case ImageManifestError::ConflictingAlphaSources:
        return "image manifest has conflicting alpha sources";
    case ImageManifestError::DuplicateResource:
        return "image manifest contains a duplicate resource";
    case ImageManifestError::ResourcesAreLoaded:
        return "image manifest cannot change while images are loaded";
    }
    return "unknown image manifest error";
}

const char* GetImageResourceErrorMessage(
    ImageResourceError theError)
{
    switch (theError)
    {
    case ImageResourceError::None:
        return "no error";
    case ImageResourceError::ManifestNotLoaded:
        return "image manifest is not loaded";
    case ImageResourceError::ResourceNotFound:
        return "image resource identifier is not defined";
    case ImageResourceError::SourceNotFound:
        return "image source is missing";
    case ImageResourceError::SourceReadFailed:
        return "image source could not be read";
    case ImageResourceError::DecodeFailed:
        return "image source could not be decoded";
    case ImageResourceError::AlphaSizeMismatch:
        return "image alpha dimensions do not match";
    case ImageResourceError::UploadFailed:
        return "image could not be uploaded";
    }
    return "unknown image resource error";
}

const char* GetImageDecodeErrorMessage(ImageDecodeError theError)
{
    switch (theError)
    {
    case ImageDecodeError::None:
        return "no error";
    case ImageDecodeError::EmptyInput:
        return "image input is empty";
    case ImageDecodeError::UnsupportedFormat:
        return "image format is unsupported";
    case ImageDecodeError::InvalidData:
        return "image data is invalid";
    case ImageDecodeError::DimensionsUnsupported:
        return "image dimensions are unsupported";
    case ImageDecodeError::SizeOverflow:
        return "image size overflows";
    case ImageDecodeError::AllocationFailed:
        return "image allocation failed";
    }
    return "unknown image decode error";
}

} // namespace pvz::engine::core
