#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/PakArchive.h"
#include "pvz/engine/core/XmlDocument.h"

#if defined(PVZ_HAS_IMAGE_CODECS)
#include "pvz/engine/core/BitmapFontResourceManager.h"
#include "pvz/engine/core/ImageResourceManager.h"
#include "pvz/engine/core/ResourceXmlDocumentLoader.h"
#include "pvz/engine/image/PortableImageDecoder.h"
#endif

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{

[[nodiscard]] char LowercaseAscii(char theCharacter)
{
    if (theCharacter >= 'A' && theCharacter <= 'Z')
        return static_cast<char>(theCharacter - 'A' + 'a');
    return theCharacter;
}

[[nodiscard]] bool EndsWithAsciiInsensitive(
    std::string_view theText,
    std::string_view theSuffix)
{
    if (theText.size() < theSuffix.size())
        return false;

    const auto aStart = theText.size() - theSuffix.size();
    for (std::size_t anIndex = 0; anIndex < theSuffix.size(); ++anIndex)
    {
        if (LowercaseAscii(theText[aStart + anIndex]) !=
            LowercaseAscii(theSuffix[anIndex]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool IsXmlSource(std::string_view thePath)
{
    return EndsWithAsciiInsensitive(thePath, ".xml") ||
           EndsWithAsciiInsensitive(thePath, ".reanim");
}

[[nodiscard]] bool IsImageSource(std::string_view thePath)
{
    return EndsWithAsciiInsensitive(thePath, ".png") ||
           EndsWithAsciiInsensitive(thePath, ".jpg") ||
           EndsWithAsciiInsensitive(thePath, ".jpeg") ||
           EndsWithAsciiInsensitive(thePath, ".tga") ||
           EndsWithAsciiInsensitive(thePath, ".gif");
}

[[nodiscard]] std::string_view GetSourceLine(
    std::string_view theText,
    std::uint32_t theLine)
{
    if (theLine == 0)
        return {};

    std::uint32_t aCurrentLine = 1;
    std::size_t aLineStart = 0;
    for (std::size_t anIndex = 0; anIndex < theText.size(); ++anIndex)
    {
        if (aCurrentLine == theLine && theText[anIndex] == '\n')
            return theText.substr(aLineStart, anIndex - aLineStart);
        if (theText[anIndex] == '\n')
        {
            ++aCurrentLine;
            aLineStart = anIndex + 1;
        }
    }
    if (aCurrentLine == theLine)
        return theText.substr(aLineStart);
    return {};
}

[[nodiscard]] bool XmlNodesEqual(
    const pvz::engine::core::XmlNode& theLeft,
    const pvz::engine::core::XmlNode& theRight)
{
    if (theLeft.mName != theRight.mName ||
        theLeft.mValue != theRight.mValue ||
        theLeft.mLine != theRight.mLine ||
        theLeft.mAttributes.size() != theRight.mAttributes.size() ||
        theLeft.mChildren.size() != theRight.mChildren.size())
    {
        return false;
    }

    for (std::size_t anIndex = 0;
         anIndex < theLeft.mAttributes.size();
         ++anIndex)
    {
        if (theLeft.mAttributes[anIndex].mName !=
                theRight.mAttributes[anIndex].mName ||
            theLeft.mAttributes[anIndex].mValue !=
                theRight.mAttributes[anIndex].mValue)
        {
            return false;
        }
    }

    for (std::size_t anIndex = 0;
         anIndex < theLeft.mChildren.size();
         ++anIndex)
    {
        if (!XmlNodesEqual(
                theLeft.mChildren[anIndex],
                theRight.mChildren[anIndex]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool XmlDocumentsEqual(
    const pvz::engine::core::XmlDocument& theLeft,
    const pvz::engine::core::XmlDocument& theRight)
{
    const auto aLeftRoots = theLeft.GetRoots();
    const auto aRightRoots = theRight.GetRoots();
    if (aLeftRoots.size() != aRightRoots.size())
        return false;
    for (std::size_t anIndex = 0; anIndex < aLeftRoots.size(); ++anIndex)
    {
        if (!XmlNodesEqual(aLeftRoots[anIndex], aRightRoots[anIndex]))
            return false;
    }
    return true;
}

[[nodiscard]] bool ValidateXmlSources(
    const pvz::engine::core::PakArchive& theArchive)
{
    std::size_t aSourceCount{};
    std::size_t aNodeCount{};
    std::uint64_t aCacheByteCount{};
    std::vector<std::byte> aBytes;
    for (const auto& anEntry : theArchive.GetEntries())
    {
        if (!IsXmlSource(anEntry.mPath))
            continue;

        ++aSourceCount;
        if (!theArchive.ReadEntry(anEntry, aBytes))
        {
            std::cerr << anEntry.mPath << ": could not read entry\n";
            return false;
        }

        const std::string anXml(
            reinterpret_cast<const char*>(aBytes.data()),
            aBytes.size());
        pvz::engine::core::XmlDocument aDocument;
        if (!aDocument.ParseFragment(anXml))
        {
            const auto anErrorLine = aDocument.GetErrorLine();
            std::cerr
                << anEntry.mPath << ':' << anErrorLine << ": "
                << pvz::engine::core::GetXmlDocumentErrorMessage(
                       aDocument.GetError());
            if (aDocument.GetParserError() !=
                pvz::engine::core::XmlError::None)
            {
                std::cerr
                    << ": "
                    << pvz::engine::core::GetXmlErrorMessage(
                           aDocument.GetParserError());
            }
            std::cerr << '\n';
            const auto aSourceLine = GetSourceLine(anXml, anErrorLine);
            if (!aSourceLine.empty())
                std::cerr << "  " << aSourceLine << '\n';
            return false;
        }
        aNodeCount += aDocument.GetNodeCount();

        pvz::engine::core::BinaryStateWriter aCacheWriter;
        if (!aDocument.WriteCache(aCacheWriter))
        {
            std::cerr << anEntry.mPath << ": could not write XML cache\n";
            return false;
        }
        pvz::engine::core::BinaryStateReader aCacheReader(
            aCacheWriter.GetBytes());
        pvz::engine::core::XmlDocument aCachedDocument;
        if (!aCachedDocument.ReadCache(aCacheReader) ||
            !XmlDocumentsEqual(aDocument, aCachedDocument))
        {
            std::cerr
                << anEntry.mPath
                << ": XML cache round-trip did not preserve the document\n";
            return false;
        }
        aCacheByteCount += aCacheWriter.GetBytesWritten();
    }

    std::cout << "xml-sources=" << aSourceCount
              << " xml-nodes=" << aNodeCount
              << " xml-cache-bytes=" << aCacheByteCount << '\n';
    return true;
}

#if defined(PVZ_HAS_IMAGE_CODECS)
class ArchiveResourceStore final : public pvz::engine::IResourceStore
{
public:
    explicit ArchiveResourceStore(
        const pvz::engine::core::PakArchive& theArchive)
        : mArchive(theArchive)
    {
    }

    [[nodiscard]] bool Contains(
        std::string_view thePath) const override
    {
        return mArchive.FindEntry(thePath) != nullptr;
    }

    [[nodiscard]] bool GetSize(
        std::string_view thePath,
        std::uint64_t& theSize) const override
    {
        const auto* anEntry = mArchive.FindEntry(thePath);
        if (anEntry == nullptr)
            return false;
        theSize = anEntry->mDataSize;
        return true;
    }

    [[nodiscard]] bool ReadAll(
        std::string_view thePath,
        std::vector<std::byte>& theBytes) const override
    {
        return mArchive.ReadEntry(thePath, theBytes);
    }

private:
    const pvz::engine::core::PakArchive& mArchive;
};

class ValidationImageStore final : public pvz::engine::IImageStore
{
public:
    [[nodiscard]] bool CreateImage(
        const pvz::engine::ImageDescriptor& theDescriptor,
        std::span<const std::byte> theInitialPixels,
        std::uint32_t theSourceBytesPerRow,
        pvz::engine::ImageHandle& theImage) override
    {
        const auto aWidth = theDescriptor.mSize.mWidth;
        const auto aHeight = theDescriptor.mSize.mHeight;
        if (aWidth == 0 ||
            aHeight == 0 ||
            theDescriptor.mPixelFormat !=
                pvz::engine::ImagePixelFormat::Bgra8Unorm ||
            aWidth >
                std::numeric_limits<std::uint32_t>::max() / 4 ||
            theSourceBytesPerRow < aWidth * 4)
        {
            return false;
        }
        const auto aRequiredBytes =
            static_cast<std::uint64_t>(theSourceBytesPerRow) *
            aHeight;
        if (aRequiredBytes > theInitialPixels.size() ||
            mNextIndex ==
                std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        theImage = {
            .mIndex = mNextIndex++,
            .mGeneration = 1,
        };
        mSizes.emplace(theImage.mIndex, theDescriptor.mSize);
        return true;
    }

    [[nodiscard]] bool UpdateImage(
        pvz::engine::ImageHandle theImage,
        const pvz::engine::ImageUpdate& theUpdate) override
    {
        static_cast<void>(theImage);
        static_cast<void>(theUpdate);
        return false;
    }

    void DestroyImage(pvz::engine::ImageHandle theImage) override
    {
        mSizes.erase(theImage.mIndex);
    }

    [[nodiscard]] bool GetImageSize(
        pvz::engine::ImageHandle theImage,
        pvz::engine::SizeI& theSize) const override
    {
        if (theImage.mGeneration != 1)
            return false;
        const auto aSize = mSizes.find(theImage.mIndex);
        if (aSize == mSizes.end())
            return false;
        theSize = aSize->second;
        return true;
    }

private:
    std::unordered_map<std::uint32_t, pvz::engine::SizeI> mSizes;
    std::uint32_t mNextIndex{1};
};

[[nodiscard]] bool ValidateImageSources(
    const pvz::engine::core::PakArchive& theArchive)
{
    pvz::engine::image::PortableImageDecoder aDecoder;
    std::size_t aDecodedCount{};
    std::size_t anUnsupportedCount{};
    std::uint64_t aPixelCount{};
    std::vector<std::byte> aBytes;
    for (const auto& anEntry : theArchive.GetEntries())
    {
        if (!IsImageSource(anEntry.mPath))
            continue;
        if (!theArchive.ReadEntry(anEntry, aBytes))
        {
            std::cerr << anEntry.mPath << ": could not read entry\n";
            return false;
        }

        pvz::engine::DecodedImage anImage;
        pvz::engine::ImageDecodeError anError{};
        if (!aDecoder.Decode(aBytes, anImage, anError))
        {
            if (anError ==
                pvz::engine::ImageDecodeError::UnsupportedFormat)
            {
                ++anUnsupportedCount;
                continue;
            }
            std::cerr
                << anEntry.mPath
                << ": image decode error "
                << static_cast<std::uint32_t>(anError)
                << '\n';
            return false;
        }
        ++aDecodedCount;
        const auto aPixels =
            static_cast<std::uint64_t>(
                anImage.mDescriptor.mSize.mWidth) *
            anImage.mDescriptor.mSize.mHeight;
        if (aPixels >
            std::numeric_limits<std::uint64_t>::max() -
                aPixelCount)
        {
            std::cerr << "decoded image pixel count overflows\n";
            return false;
        }
        aPixelCount += aPixels;
    }

    std::cout << "decoded-images=" << aDecodedCount
              << " unsupported-images=" << anUnsupportedCount
              << " decoded-pixels=" << aPixelCount << '\n';
    return true;
}

[[nodiscard]] bool ValidateFontResources(
    const pvz::engine::core::PakArchive& theArchive)
{
    ArchiveResourceStore aResources(theArchive);
    pvz::engine::core::ResourceXmlDocumentLoader aDocuments(
        aResources);
    pvz::engine::image::PortableImageDecoder aDecoder;
    ValidationImageStore anImageStore;
    pvz::engine::core::ImageResourceManager anImages(
        aResources,
        aDocuments,
        aDecoder,
        anImageStore);
    if (!anImages.LoadManifest("properties/resources.xml"))
    {
        std::cerr
            << pvz::engine::core::GetImageManifestErrorMessage(
                   anImages.GetManifestError())
            << " at line "
            << anImages.GetManifestErrorLine()
            << '\n';
        return false;
    }

    pvz::engine::core::BitmapFontResourceManager aFonts(
        aResources,
        aDocuments,
        anImages);
    if (!aFonts.LoadManifest("properties/resources.xml"))
    {
        std::cerr
            << pvz::engine::core::GetFontManifestErrorMessage(
                   aFonts.GetManifestError())
            << " at line "
            << aFonts.GetManifestErrorLine()
            << '\n';
        return false;
    }

    std::uint64_t aLayerCount{};
    std::uint64_t aGlyphCount{};
    std::uint64_t aKerningCount{};
    std::vector<pvz::engine::FontResource> aLoadedFonts;
    const auto anIds = aFonts.GetDefinitionIds();
    aLoadedFonts.reserve(anIds.size());
    for (const auto& anId : anIds)
    {
        pvz::engine::FontResource aFont;
        pvz::engine::FontResourceDiagnostic aDiagnostic;
        if (!aFonts.Load(anId, aFont, aDiagnostic))
        {
            std::cerr
                << anId << ": "
                << pvz::engine::core::GetFontResourceErrorMessage(
                       aDiagnostic.mError);
            if (!aDiagnostic.mPath.empty())
                std::cerr << ": " << aDiagnostic.mPath;
            if (aDiagnostic.mLine != 0)
                std::cerr << ':' << aDiagnostic.mLine;
            if (!aDiagnostic.mCommand.empty())
                std::cerr << ": " << aDiagnostic.mCommand;
            if (aDiagnostic.mImageDiagnostic.mError !=
                pvz::engine::ImageResourceError::None)
            {
                std::cerr
                    << ": "
                    << pvz::engine::core::
                           GetImageResourceErrorMessage(
                               aDiagnostic.mImageDiagnostic
                                   .mError);
            }
            std::cerr << '\n';
            for (const auto& aLoaded : aLoadedFonts)
                aFonts.Release(aLoaded.mFont);
            return false;
        }
        aLayerCount += aFont.mLayerCount;
        aGlyphCount += aFont.mGlyphCount;
        aKerningCount += aFont.mKerningPairCount;
        aLoadedFonts.push_back(aFont);
    }
    for (const auto& aFont : aLoadedFonts)
        aFonts.Release(aFont.mFont);

    std::cout
        << "fonts=" << aLoadedFonts.size()
        << " font-layers=" << aLayerCount
        << " font-glyphs=" << aGlyphCount
        << " kerning-pairs=" << aKerningCount
        << '\n';
    return true;
}
#endif

} // namespace

int main(int theArgumentCount, char** theArguments)
{
    if (theArgumentCount < 2 || theArgumentCount > 4)
    {
        std::cerr
            << "usage: pvz_pak_inspect <path-to-main.pak> "
               "[--validate-xml|--list-images|"
               "--print-resource-manifest|--validate-images|"
               "--validate-fonts|"
               "--print-entry <entry-path>]\n";
        return 2;
    }
    const std::string_view anOption =
        theArgumentCount >= 3
            ? std::string_view(theArguments[2])
            : std::string_view{};
    const bool shouldValidateXml = anOption == "--validate-xml";
    const bool shouldListImages = anOption == "--list-images";
    const bool shouldPrintResourceManifest =
        anOption == "--print-resource-manifest";
    const bool shouldValidateImages =
        anOption == "--validate-images";
    const bool shouldValidateFonts =
        anOption == "--validate-fonts";
    const bool shouldPrintEntry =
        anOption == "--print-entry";
    if ((shouldPrintEntry && theArgumentCount != 4) ||
        (!shouldPrintEntry && theArgumentCount == 4) ||
        (theArgumentCount >= 3 &&
        !shouldValidateXml &&
        !shouldListImages &&
        !shouldPrintResourceManifest &&
        !shouldValidateImages &&
        !shouldValidateFonts &&
        !shouldPrintEntry))
    {
        std::cerr << "invalid option or arguments";
        if (theArgumentCount >= 3)
            std::cerr << ": " << theArguments[2];
        std::cerr << '\n';
        return 2;
    }

    pvz::engine::core::PakArchive anArchive;
    if (!anArchive.LoadFromFile(std::filesystem::path(theArguments[1])))
    {
        std::cerr
            << pvz::engine::core::GetPakErrorMessage(anArchive.GetError())
            << '\n';
        return 1;
    }

    if (shouldPrintEntry)
    {
        std::vector<std::byte> anEntryBytes;
        if (!anArchive.ReadEntry(theArguments[3], anEntryBytes))
        {
            std::cerr << "entry is missing: " << theArguments[3] << '\n';
            return 1;
        }
        std::cout.write(
            reinterpret_cast<const char*>(anEntryBytes.data()),
            static_cast<std::streamsize>(anEntryBytes.size()));
        return 0;
    }

    std::cout << "entries=" << anArchive.GetEntries().size()
              << " payload-bytes=" << anArchive.GetPayloadSize() << '\n';

    const auto* aResourceManifest =
        anArchive.FindEntry("properties/resources.xml");
    if (aResourceManifest == nullptr)
    {
        std::cerr << "properties/resources.xml is missing\n";
        return 1;
    }

    std::cout << "resources.xml-bytes=" << aResourceManifest->mDataSize
              << '\n';
    if (shouldListImages)
    {
        for (const auto& anEntry : anArchive.GetEntries())
        {
            if (IsImageSource(anEntry.mPath))
                std::cout << anEntry.mPath << '\n';
        }
    }
    if (shouldPrintResourceManifest)
    {
        std::vector<std::byte> aManifestBytes;
        if (!anArchive.ReadEntry(
                *aResourceManifest,
                aManifestBytes))
        {
            std::cerr << "could not read properties/resources.xml\n";
            return 1;
        }
        std::cout.write(
            reinterpret_cast<const char*>(aManifestBytes.data()),
            static_cast<std::streamsize>(aManifestBytes.size()));
    }
    if (shouldValidateXml && !ValidateXmlSources(anArchive))
        return 1;
    if (shouldValidateImages)
    {
#if defined(PVZ_HAS_IMAGE_CODECS)
        if (!ValidateImageSources(anArchive))
            return 1;
#else
        std::cerr << "image codecs are not enabled in this build\n";
        return 1;
#endif
    }
    if (shouldValidateFonts)
    {
#if defined(PVZ_HAS_IMAGE_CODECS)
        if (!ValidateFontResources(anArchive))
            return 1;
#else
        std::cerr << "image codecs are not enabled in this build\n";
        return 1;
#endif
    }
    return 0;
}
