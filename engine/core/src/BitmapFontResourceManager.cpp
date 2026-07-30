#include "pvz/engine/core/BitmapFontResourceManager.h"

#include <algorithm>
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
    if (thePath.empty())
        return std::string(thePath);
    const auto aDirectory = TrimTrailingSlashes(theDirectory);
    if (aDirectory.empty())
        return std::string(thePath);

    std::string aResult(aDirectory);
    aResult.push_back('/');
    aResult.append(thePath);
    return aResult;
}

[[nodiscard]] std::string_view GetDirectory(
    std::string_view thePath)
{
    const auto aSlash = thePath.find_last_of("/\\");
    if (aSlash == std::string_view::npos)
        return {};
    return thePath.substr(0, aSlash);
}

[[nodiscard]] std::uint64_t MakeHandleKey(FontHandle theFont)
{
    return (static_cast<std::uint64_t>(theFont.mGeneration) << 32) |
           theFont.mIndex;
}

[[nodiscard]] bool IsAtlasRectangleValid(
    const RectI& theRectangle,
    SizeI theAtlasSize)
{
    if (theRectangle.mOrigin.mX < 0 ||
        theRectangle.mOrigin.mY < 0)
    {
        return false;
    }
    const auto anX = static_cast<std::uint64_t>(
        theRectangle.mOrigin.mX);
    const auto aY = static_cast<std::uint64_t>(
        theRectangle.mOrigin.mY);
    return anX + theRectangle.mSize.mWidth <=
               theAtlasSize.mWidth &&
           aY + theRectangle.mSize.mHeight <=
               theAtlasSize.mHeight;
}

[[nodiscard]] std::int32_t GetKerning(
    const BitmapFontGlyph* theGlyph,
    char32_t theNextCharacter)
{
    if (theGlyph == nullptr || theNextCharacter == U'\0')
        return 0;
    const auto aKerning = theGlyph->mKerning.find(theNextCharacter);
    return aKerning == theGlyph->mKerning.end()
               ? 0
               : aKerning->second;
}

[[nodiscard]] const BitmapFontGlyph* FindGlyph(
    const BitmapFontLayerDescriptor& theLayer,
    char32_t theCharacter)
{
    const auto aGlyph = theLayer.mGlyphs.find(theCharacter);
    return aGlyph == theLayer.mGlyphs.end()
               ? nullptr
               : &aGlyph->second;
}

[[nodiscard]] ColorRgba8 MultiplyColor(
    ColorRgba8 theColor,
    ColorRgba8 theMultiplier)
{
    const auto aMultiply =
        [](std::uint8_t theLeft, std::uint8_t theRight)
        {
            return static_cast<std::uint8_t>(
                (static_cast<std::uint32_t>(theLeft) *
                 theRight) /
                255);
        };
    return {
        .mRed = aMultiply(theColor.mRed, theMultiplier.mRed),
        .mGreen =
            aMultiply(theColor.mGreen, theMultiplier.mGreen),
        .mBlue = aMultiply(theColor.mBlue, theMultiplier.mBlue),
        .mAlpha =
            aMultiply(theColor.mAlpha, theMultiplier.mAlpha),
    };
}

[[nodiscard]] FontMetrics CalculateMetrics(
    const BitmapFontDescriptor& theDescriptor)
{
    FontMetrics aMetrics{
        .mDefaultPointSize = theDescriptor.mDefaultPointSize,
    };
    bool isFirstLayer = true;
    for (const auto& aLayer : theDescriptor.mLayers)
    {
        aMetrics.mAscent =
            std::max(aMetrics.mAscent, aLayer.mAscent);
        if (isFirstLayer)
        {
            aMetrics.mAscentPadding = aLayer.mAscentPadding;
            aMetrics.mLineSpacingOffset =
                aLayer.mLineSpacingOffset;
        }
        else
        {
            aMetrics.mAscentPadding = std::min(
                aMetrics.mAscentPadding,
                aLayer.mAscentPadding);
            aMetrics.mLineSpacingOffset = std::max(
                aMetrics.mLineSpacingOffset,
                aLayer.mLineSpacingOffset);
        }

        for (const auto& [aCharacter, aGlyph] : aLayer.mGlyphs)
        {
            static_cast<void>(aCharacter);
            const auto aHeight =
                static_cast<std::int64_t>(
                    aGlyph.mSource.mSize.mHeight) +
                aGlyph.mOffset.mY;
            if (aHeight > aMetrics.mHeight &&
                aHeight <=
                    std::numeric_limits<std::int32_t>::max())
            {
                aMetrics.mHeight =
                    static_cast<std::int32_t>(aHeight);
            }
        }
        isFirstLayer = false;
    }
    return aMetrics;
}

} // namespace

BitmapFontResourceManager::BitmapFontResourceManager(
    const IResourceStore& theResourceStore,
    const IXmlDocumentLoader& theDocuments,
    IImageResources& theImages)
    : mResourceStore(theResourceStore),
      mDocuments(theDocuments),
      mImages(theImages)
{
}

BitmapFontResourceManager::~BitmapFontResourceManager()
{
    ReleaseAll();
}

bool BitmapFontResourceManager::LoadManifest(
    std::string_view thePath)
{
    mManifestError = FontManifestError::None;
    mManifestDocumentDiagnostic = {};
    mManifestErrorLine = 0;
    if (!mLoadedResources.empty())
    {
        FailManifest(FontManifestError::ResourcesAreLoaded, 0);
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
            FontManifestError::SourceDocument,
            mManifestDocumentDiagnostic.mLine);
        return false;
    }
    if (aRoots.size() != 1 ||
        aRoots.front().mName != "ResourceManifest")
    {
        FailManifest(
            FontManifestError::InvalidRoot,
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

bool BitmapFontResourceManager::IsManifestLoaded() const
{
    return mManifestLoaded;
}

std::size_t BitmapFontResourceManager::GetDefinitionCount() const
{
    return mDefinitions.size();
}

std::vector<std::string>
BitmapFontResourceManager::GetDefinitionIds() const
{
    std::vector<std::string> anIds;
    anIds.reserve(mDefinitions.size());
    for (const auto& [anId, aDefinition] : mDefinitions)
    {
        static_cast<void>(aDefinition);
        anIds.push_back(anId);
    }
    std::ranges::sort(anIds);
    return anIds;
}

FontManifestError
BitmapFontResourceManager::GetManifestError() const
{
    return mManifestError;
}

XmlDocumentDiagnostic
BitmapFontResourceManager::GetManifestDocumentDiagnostic() const
{
    return mManifestDocumentDiagnostic;
}

std::uint32_t
BitmapFontResourceManager::GetManifestErrorLine() const
{
    return mManifestErrorLine;
}

bool BitmapFontResourceManager::Load(
    std::string_view theResourceId,
    FontResource& theResource,
    FontResourceDiagnostic& theDiagnostic)
{
    theResource = {};
    theDiagnostic = {};
    if (!mManifestLoaded)
    {
        theDiagnostic.mError =
            FontResourceError::ManifestNotLoaded;
        return false;
    }
    const auto aDefinition =
        mDefinitions.find(std::string(theResourceId));
    if (aDefinition == mDefinitions.end())
    {
        theDiagnostic.mError =
            FontResourceError::ResourceNotFound;
        return false;
    }
    const auto aLoaded =
        mLoadedResources.find(std::string(theResourceId));
    if (aLoaded != mLoadedResources.end())
    {
        if (aLoaded->second.mReferenceCount ==
            std::numeric_limits<std::uint32_t>::max())
        {
            theDiagnostic.mError =
                FontResourceError::NumericOverflow;
            return false;
        }
        ++aLoaded->second.mReferenceCount;
        theResource = aLoaded->second.mResource;
        return true;
    }

    const auto& aPath = aDefinition->second.mPath;
    std::vector<std::byte> aBytes;
    if (!mResourceStore.Contains(aPath))
    {
        theDiagnostic = {
            .mError = FontResourceError::SourceNotFound,
            .mPath = aPath,
        };
        return false;
    }
    if (!mResourceStore.ReadAll(aPath, aBytes))
    {
        theDiagnostic = {
            .mError = FontResourceError::SourceReadFailed,
            .mPath = aPath,
        };
        return false;
    }

    BitmapFontDescriptor aDescriptor;
    BitmapFontDescriptorDiagnostic aDescriptorDiagnostic;
    const BitmapFontDescriptorParser aParser;
    if (!aParser.Parse(
            aBytes,
            aDescriptor,
            aDescriptorDiagnostic))
    {
        theDiagnostic = {
            .mError =
                aDescriptorDiagnostic.mError ==
                        BitmapFontDescriptorError::
                            UnsupportedCommand
                    ? FontResourceError::
                          UnsupportedDescriptorCommand
                    : FontResourceError::DescriptorMalformed,
            .mLine = aDescriptorDiagnostic.mLine,
            .mPath = aPath,
            .mCommand = aDescriptorDiagnostic.mCommand,
        };
        return false;
    }
    const auto aMetrics = CalculateMetrics(aDescriptor);

    std::vector<LoadedLayer> aLoadedLayers;
    aLoadedLayers.reserve(aDescriptor.mLayers.size());
    for (auto& aLayer : aDescriptor.mLayers)
    {
        const auto anAtlasPath =
            JoinPath(GetDirectory(aPath), aLayer.mImagePath);
        ImageResource anAtlas;
        ImageResourceDiagnostic anImageDiagnostic;
        if (!mImages.LoadSource(
                anAtlasPath,
                anAtlas,
                anImageDiagnostic))
        {
            ReleaseLayers(aLoadedLayers);
            theDiagnostic = {
                .mError = FontResourceError::AtlasLoadFailed,
                .mPath = anAtlasPath,
                .mImageDiagnostic =
                    std::move(anImageDiagnostic),
            };
            return false;
        }
        bool isValid = true;
        for (const auto& [aCharacter, aGlyph] : aLayer.mGlyphs)
        {
            static_cast<void>(aCharacter);
            if (!IsAtlasRectangleValid(
                    aGlyph.mSource,
                    anAtlas.mSize))
            {
                isValid = false;
                break;
            }
        }
        if (!isValid)
        {
            mImages.Release(anAtlas.mImage);
            ReleaseLayers(aLoadedLayers);
            theDiagnostic = {
                .mError =
                    FontResourceError::AtlasBoundsInvalid,
                .mPath = anAtlasPath,
            };
            return false;
        }
        aLoadedLayers.push_back(
            LoadedLayer{
                .mDescriptor = std::move(aLayer),
                .mAtlas = anAtlas,
            });
    }

    if (mNextHandleIndex ==
        std::numeric_limits<std::uint32_t>::max())
    {
        ReleaseLayers(aLoadedLayers);
        theDiagnostic.mError = FontResourceError::NumericOverflow;
        return false;
    }

    std::uint64_t aGlyphCount{};
    std::uint64_t aKerningCount{};
    for (const auto& aLayer : aLoadedLayers)
    {
        aGlyphCount += aLayer.mDescriptor.mGlyphs.size();
        for (const auto& [aCharacter, aGlyph] :
             aLayer.mDescriptor.mGlyphs)
        {
            static_cast<void>(aCharacter);
            aKerningCount += aGlyph.mKerning.size();
        }
    }
    if (aDescriptor.mLayers.size() >
            std::numeric_limits<std::uint32_t>::max() ||
        aGlyphCount >
            std::numeric_limits<std::uint32_t>::max() ||
        aKerningCount >
            std::numeric_limits<std::uint32_t>::max())
    {
        ReleaseLayers(aLoadedLayers);
        theDiagnostic.mError = FontResourceError::NumericOverflow;
        return false;
    }

    const FontHandle aHandle{
        .mIndex = mNextHandleIndex++,
        .mGeneration = 1,
    };
    FontResource aResource{
        .mFont = aHandle,
        .mMetrics = aMetrics,
        .mLayerCount = static_cast<std::uint32_t>(
            aLoadedLayers.size()),
        .mGlyphCount = static_cast<std::uint32_t>(aGlyphCount),
        .mKerningPairCount =
            static_cast<std::uint32_t>(aKerningCount),
    };
    const std::string anId(theResourceId);
    mLoadedResources.emplace(
        anId,
        LoadedResource{
            .mResource = aResource,
            .mLayers = std::move(aLoadedLayers),
            .mReferenceCount = 1,
        });
    mIdsByHandle.emplace(MakeHandleKey(aHandle), anId);
    theResource = aResource;
    return true;
}

void BitmapFontResourceManager::Release(FontHandle theFont)
{
    const auto anId = mIdsByHandle.find(MakeHandleKey(theFont));
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

    ReleaseLayers(aLoaded->second.mLayers);
    mLoadedResources.erase(aLoaded);
    mIdsByHandle.erase(anId);
}

bool BitmapFontResourceManager::MeasureText(
    FontHandle theFont,
    std::u32string_view theText,
    TextMetrics& theMetrics) const
{
    theMetrics = {};
    const auto* aFont = FindLoaded(theFont);
    if (aFont == nullptr)
        return false;

    std::int64_t anAdvance{};
    char32_t aPreviousCharacter{};
    for (const char32_t aCharacter : theText)
    {
        std::int64_t aMaximum{};
        for (const auto& aLayer : aFont->mLayers)
        {
            const auto* aGlyph =
                FindGlyph(aLayer.mDescriptor, aCharacter);
            const auto* aPreviousGlyph = FindGlyph(
                aLayer.mDescriptor,
                aPreviousCharacter);
            const auto aLayerAdvance =
                static_cast<std::int64_t>(
                    aGlyph == nullptr ? 0 : aGlyph->mAdvance) +
                GetKerning(aPreviousGlyph, aCharacter);
            aMaximum = std::max(aMaximum, aLayerAdvance);
        }
        if (aMaximum >
                std::numeric_limits<std::int32_t>::max() -
                    anAdvance ||
            aMaximum <
                std::numeric_limits<std::int32_t>::min() -
                    anAdvance)
        {
            return false;
        }
        anAdvance += aMaximum;
        aPreviousCharacter = aCharacter;
    }
    theMetrics.mAdvance = static_cast<std::int32_t>(anAdvance);
    return true;
}

bool BitmapFontResourceManager::AppendTextSprites(
    FontHandle theFont,
    std::u32string_view theText,
    PointF theBaseline,
    ColorRgba8 theColor,
    std::vector<SpriteDraw>& theSprites) const
{
    const auto* aFont = FindLoaded(theFont);
    if (aFont == nullptr ||
        (!aFont->mLayers.empty() &&
         theText.size() >
             std::numeric_limits<std::size_t>::max() /
                 aFont->mLayers.size()))
    {
        return false;
    }

    std::vector<SpriteDraw> aResult;
    aResult.reserve(theText.size() * aFont->mLayers.size());
    std::int64_t aCursor{};
    for (std::size_t aCharacterIndex = 0;
         aCharacterIndex < theText.size();
         ++aCharacterIndex)
    {
        const auto aCharacter = theText[aCharacterIndex];
        const auto aNextCharacter =
            aCharacterIndex + 1 < theText.size()
                ? theText[aCharacterIndex + 1]
                : U'\0';
        std::int64_t aMaximum{};
        for (const auto& aLayer : aFont->mLayers)
        {
            const auto* aGlyph =
                FindGlyph(aLayer.mDescriptor, aCharacter);
            if (aGlyph == nullptr)
                continue;

            const auto aLayerAdvance =
                static_cast<std::int64_t>(aGlyph->mAdvance) +
                GetKerning(aGlyph, aNextCharacter);
            aMaximum = std::max(aMaximum, aLayerAdvance);
            if (aGlyph->mSource.mSize.mWidth == 0 ||
                aGlyph->mSource.mSize.mHeight == 0)
            {
                continue;
            }

            const auto aDestinationX =
                static_cast<std::int64_t>(
                    aLayer.mDescriptor.mOffset.mX) +
                aGlyph->mOffset.mX +
                aCursor;
            const auto aDestinationY =
                -static_cast<std::int64_t>(
                    aLayer.mDescriptor.mAscent) +
                aLayer.mDescriptor.mOffset.mY +
                aGlyph->mOffset.mY;
            if (aDestinationX <
                    std::numeric_limits<std::int32_t>::min() ||
                aDestinationX >
                    std::numeric_limits<std::int32_t>::max() ||
                aDestinationY <
                    std::numeric_limits<std::int32_t>::min() ||
                aDestinationY >
                    std::numeric_limits<std::int32_t>::max())
            {
                return false;
            }

            aResult.push_back(
                SpriteDraw{
                    .mImage = aLayer.mAtlas.mImage,
                    .mSource = aGlyph->mSource,
                    .mDestination =
                        {
                            .mOrigin =
                                {
                                    theBaseline.mX +
                                        static_cast<float>(
                                            aDestinationX),
                                    theBaseline.mY +
                                        static_cast<float>(
                                            aDestinationY),
                                },
                            .mSize =
                                {
                                    static_cast<float>(
                                        aGlyph->mSource.mSize
                                            .mWidth),
                                    static_cast<float>(
                                        aGlyph->mSource.mSize
                                            .mHeight),
                                },
                        },
                    .mColor = MultiplyColor(
                        theColor,
                        aLayer.mDescriptor.mColorMultiplier),
                });
        }
        if (aMaximum >
                std::numeric_limits<std::int32_t>::max() -
                    aCursor ||
            aMaximum <
                std::numeric_limits<std::int32_t>::min() -
                    aCursor)
        {
            return false;
        }
        aCursor += aMaximum;
    }

    theSprites.insert(
        theSprites.end(),
        aResult.begin(),
        aResult.end());
    return true;
}

bool BitmapFontResourceManager::ParseManifest(
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
                FontManifestError::InvalidSection,
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
                    aDefaultIdPrefix = *anIdPrefix;
                }
                continue;
            }
            if (aNode.mName == "Image" ||
                aNode.mName == "Sound")
            {
                continue;
            }
            if (aNode.mName != "Font")
            {
                FailManifest(
                    FontManifestError::InvalidSection,
                    aNode.mLine);
                return false;
            }

            const auto* anId = aNode.FindAttribute("id");
            const auto* aPath = aNode.FindAttribute("path");
            if (anId == nullptr ||
                anId->empty() ||
                aPath == nullptr ||
                aPath->empty())
            {
                FailManifest(
                    FontManifestError::MissingAttribute,
                    aNode.mLine);
                return false;
            }
            std::string aResourceId = aDefaultIdPrefix;
            aResourceId.append(*anId);
            if (!theDefinitions.emplace(
                    std::move(aResourceId),
                    Definition{
                        .mPath = JoinPath(
                            aDefaultPath,
                            *aPath),
                    }).second)
            {
                FailManifest(
                    FontManifestError::DuplicateResource,
                    aNode.mLine);
                return false;
            }
        }
    }
    return true;
}

BitmapFontResourceManager::LoadedResource*
BitmapFontResourceManager::FindLoaded(FontHandle theFont)
{
    const auto anId = mIdsByHandle.find(MakeHandleKey(theFont));
    if (anId == mIdsByHandle.end())
        return nullptr;
    const auto aLoaded = mLoadedResources.find(anId->second);
    return aLoaded == mLoadedResources.end()
               ? nullptr
               : &aLoaded->second;
}

const BitmapFontResourceManager::LoadedResource*
BitmapFontResourceManager::FindLoaded(FontHandle theFont) const
{
    const auto anId = mIdsByHandle.find(MakeHandleKey(theFont));
    if (anId == mIdsByHandle.end())
        return nullptr;
    const auto aLoaded = mLoadedResources.find(anId->second);
    return aLoaded == mLoadedResources.end()
               ? nullptr
               : &aLoaded->second;
}

void BitmapFontResourceManager::FailManifest(
    FontManifestError theError,
    std::uint32_t theLine)
{
    mManifestError = theError;
    mManifestErrorLine = theLine;
    mManifestLoaded = false;
}

void BitmapFontResourceManager::ReleaseLayers(
    std::vector<LoadedLayer>& theLayers)
{
    for (const auto& aLayer : theLayers)
        mImages.Release(aLayer.mAtlas.mImage);
    theLayers.clear();
}

void BitmapFontResourceManager::ReleaseAll()
{
    for (auto& [anId, aLoaded] : mLoadedResources)
    {
        static_cast<void>(anId);
        ReleaseLayers(aLoaded.mLayers);
    }
    mIdsByHandle.clear();
    mLoadedResources.clear();
}

const char* GetFontManifestErrorMessage(FontManifestError theError)
{
    switch (theError)
    {
    case FontManifestError::None:
        return "no error";
    case FontManifestError::SourceDocument:
        return "font manifest could not be read";
    case FontManifestError::InvalidRoot:
        return "font manifest root is invalid";
    case FontManifestError::InvalidSection:
        return "font manifest contains an invalid section";
    case FontManifestError::MissingAttribute:
        return "font manifest is missing a required attribute";
    case FontManifestError::DuplicateResource:
        return "font manifest contains a duplicate resource";
    case FontManifestError::ResourcesAreLoaded:
        return "font manifest cannot change while fonts are loaded";
    }
    return "unknown font manifest error";
}

const char* GetFontResourceErrorMessage(FontResourceError theError)
{
    switch (theError)
    {
    case FontResourceError::None:
        return "no error";
    case FontResourceError::ManifestNotLoaded:
        return "font manifest is not loaded";
    case FontResourceError::ResourceNotFound:
        return "font resource identifier is not defined";
    case FontResourceError::SourceNotFound:
        return "font descriptor is missing";
    case FontResourceError::SourceReadFailed:
        return "font descriptor could not be read";
    case FontResourceError::DescriptorMalformed:
        return "font descriptor is malformed";
    case FontResourceError::UnsupportedDescriptorCommand:
        return "font descriptor command is unsupported";
    case FontResourceError::AtlasLoadFailed:
        return "font atlas could not be loaded";
    case FontResourceError::AtlasBoundsInvalid:
        return "font glyph exceeds its atlas";
    case FontResourceError::HandleInvalid:
        return "font handle is invalid";
    case FontResourceError::NumericOverflow:
        return "font value exceeds its fixed-width representation";
    }
    return "unknown font resource error";
}

} // namespace pvz::engine::core
