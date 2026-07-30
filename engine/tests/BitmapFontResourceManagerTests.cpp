#include "pvz/engine/core/BitmapFontResourceManager.h"
#include "pvz/engine/core/ResourceXmlDocumentLoader.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

void Expect(bool theCondition, const char* theMessage);

namespace
{

class MemoryResourceStore final : public pvz::engine::IResourceStore
{
public:
    void AddText(std::string thePath, std::string_view theText)
    {
        std::vector<std::byte> aBytes;
        aBytes.reserve(theText.size());
        for (const char aCharacter : theText)
        {
            aBytes.push_back(static_cast<std::byte>(
                static_cast<unsigned char>(aCharacter)));
        }
        mResources.emplace(std::move(thePath), std::move(aBytes));
    }

    [[nodiscard]] bool Contains(
        std::string_view thePath) const override
    {
        return mResources.contains(std::string(thePath));
    }

    [[nodiscard]] bool GetSize(
        std::string_view thePath,
        std::uint64_t& theSize) const override
    {
        const auto aResource = mResources.find(std::string(thePath));
        if (aResource == mResources.end())
            return false;
        theSize = aResource->second.size();
        return true;
    }

    [[nodiscard]] bool ReadAll(
        std::string_view thePath,
        std::vector<std::byte>& theBytes) const override
    {
        const auto aResource = mResources.find(std::string(thePath));
        if (aResource == mResources.end())
            return false;
        theBytes = aResource->second;
        return true;
    }

private:
    std::unordered_map<std::string, std::vector<std::byte>>
        mResources;
};

class FakeImageResources final : public pvz::engine::IImageResources
{
public:
    [[nodiscard]] bool Load(
        std::string_view theResourceId,
        pvz::engine::ImageResource& theResource,
        pvz::engine::ImageResourceDiagnostic& theDiagnostic) override
    {
        static_cast<void>(theResourceId);
        theResource = {};
        theDiagnostic = {
            .mError =
                pvz::engine::ImageResourceError::ResourceNotFound,
        };
        return false;
    }

    [[nodiscard]] bool LoadSource(
        std::string_view theLogicalPath,
        pvz::engine::ImageResource& theResource,
        pvz::engine::ImageResourceDiagnostic& theDiagnostic) override
    {
        mLastPath = theLogicalPath;
        ++mLoadCount;
        theResource = {
            .mImage =
                {
                    .mIndex = 7,
                    .mGeneration = 1,
                },
            .mSize = {64, 16},
        };
        theDiagnostic = {};
        return true;
    }

    void Release(pvz::engine::ImageHandle theImage) override
    {
        if (theImage.IsValid())
            ++mReleaseCount;
    }

    std::string mLastPath;
    std::uint32_t mLoadCount{};
    std::uint32_t mReleaseCount{};
};

[[nodiscard]] std::string_view GetTestDescriptor()
{
    return
        "Define Chars ('A', 'V', ' ');"
        "Define Widths (8, 9, 4);"
        "Define Rects ((0,0,8,10),(8,0,9,10),(0,0,0,0));"
        "Define Offsets ((1,2),(0,0),(0,0));"
        "Define Pairs (\"AV\");"
        "Define Kerns (-2);"
        "CreateLayer Main;"
        "LayerSetImage Main 'Atlas';"
        "LayerSetAscent Main 8;"
        "LayerSetCharWidths Main Chars Widths;"
        "LayerSetImageMap Main Chars Rects;"
        "LayerSetCharOffsets Main Chars Offsets;"
        "LayerSetKerningPairs Main Pairs Kerns;"
        "LayerSetAscentPadding Main 1;"
        "LayerSetLineSpacingOffset Main -1;"
        "LayerSetPointSize Main 10;"
        "LayerSetColorMult Main 0;"
        "SetDefaultPointSize 10;";
}

void TestLoadingMeasurementAndSprites()
{
    MemoryResourceStore aResources;
    aResources.AddText(
        "properties/resources.xml",
        "<ResourceManifest><Resources>"
        "<SetDefaults path=\"data\" idprefix=\"FONT_\"/>"
        "<Font id=\"TEST\" path=\"Test.txt\"/>"
        "</Resources></ResourceManifest>");
    aResources.AddText("data/Test.txt", GetTestDescriptor());

    pvz::engine::core::ResourceXmlDocumentLoader aDocuments(
        aResources);
    FakeImageResources anImages;
    pvz::engine::core::BitmapFontResourceManager aManager(
        aResources,
        aDocuments,
        anImages);

    Expect(
        aManager.LoadManifest("properties/resources.xml"),
        "font manifest loads");
    Expect(
        aManager.GetDefinitionCount() == 1,
        "font manifest definition count");
    const auto anIds = aManager.GetDefinitionIds();
    Expect(
        anIds.size() == 1 && anIds.front() == "FONT_TEST",
        "font manifest exposes stable resource ids");

    pvz::engine::FontResource aFont;
    pvz::engine::FontResourceDiagnostic aDiagnostic;
    Expect(
        aManager.Load("FONT_TEST", aFont, aDiagnostic),
        "bitmap font loads");
    Expect(
        anImages.mLastPath == "data/Atlas",
        "font atlas path is descriptor-relative");
    Expect(aFont.mLayerCount == 1, "font layer count");
    Expect(aFont.mGlyphCount == 3, "font glyph count");
    Expect(aFont.mKerningPairCount == 1, "font kerning count");
    Expect(aFont.mMetrics.mAscent == 8, "font ascent");
    Expect(aFont.mMetrics.mHeight == 12, "font glyph height");
    Expect(
        aFont.mMetrics.mLineSpacingOffset == -1,
        "font line spacing offset");

    pvz::engine::TextMetrics aMetrics;
    Expect(
        aManager.MeasureText(aFont.mFont, U"AVA", aMetrics),
        "text measurement succeeds");
    Expect(aMetrics.mAdvance == 23, "kerning affects text advance");

    std::vector<pvz::engine::SpriteDraw> aSprites;
    Expect(
        aManager.AppendTextSprites(
            aFont.mFont,
            U"AVA",
            {100.0F, 50.0F},
            {255, 200, 100, 255},
            aSprites),
        "text sprites are generated");
    Expect(aSprites.size() == 3, "one sprite per visible glyph");
    if (aSprites.size() == 3)
    {
        Expect(
            aSprites[0].mDestination.mOrigin.mX == 101.0F &&
                aSprites[0].mDestination.mOrigin.mY == 44.0F,
            "glyph offset and baseline are applied");
        Expect(
            aSprites[1].mDestination.mOrigin.mX == 106.0F,
            "kerning advances the next glyph");
        Expect(
            aSprites[2].mDestination.mOrigin.mX == 116.0F,
            "following glyph position is stable");
        Expect(
            aSprites[0].mColor.mRed == 0 &&
                aSprites[0].mColor.mGreen == 0 &&
                aSprites[0].mColor.mBlue == 0 &&
                aSprites[0].mColor.mAlpha == 255,
            "layer color multiplication is applied");
    }

    pvz::engine::FontResource aCachedFont;
    Expect(
        aManager.Load("FONT_TEST", aCachedFont, aDiagnostic),
        "bitmap font cache retains resources");
    Expect(anImages.mLoadCount == 1, "font atlas uploads once");
    aManager.Release(aCachedFont.mFont);
    Expect(anImages.mReleaseCount == 0, "first font release retains atlas");
    aManager.Release(aFont.mFont);
    Expect(anImages.mReleaseCount == 1, "last font release releases atlas");
    Expect(
        !aManager.MeasureText(aFont.mFont, U"A", aMetrics),
        "released font handle is rejected");
}

void TestUnsupportedCommandDiagnostic()
{
    MemoryResourceStore aResources;
    aResources.AddText(
        "manifest.xml",
        "<ResourceManifest><Resources>"
        "<SetDefaults path=\"data\" idprefix=\"FONT_\"/>"
        "<Font id=\"BAD\" path=\"Bad.txt\"/>"
        "</Resources></ResourceManifest>");
    aResources.AddText(
        "data/Bad.txt",
        "CreateLayer Main;"
        "LayerSetImage Main 'Atlas';"
        "LayerSetPointSize Main 10;"
        "LayerSetCharWidths Main ('A') (8);"
        "SetDefaultPointSize 10;"
        "LayerSetSpacing Main 1;");

    pvz::engine::core::ResourceXmlDocumentLoader aDocuments(
        aResources);
    FakeImageResources anImages;
    pvz::engine::core::BitmapFontResourceManager aManager(
        aResources,
        aDocuments,
        anImages);
    Expect(aManager.LoadManifest("manifest.xml"), "bad font manifest loads");

    pvz::engine::FontResource aFont;
    pvz::engine::FontResourceDiagnostic aDiagnostic;
    Expect(
        !aManager.Load("FONT_BAD", aFont, aDiagnostic),
        "unsupported descriptor command fails");
    Expect(
        aDiagnostic.mError ==
            pvz::engine::FontResourceError::
                UnsupportedDescriptorCommand,
        "unsupported descriptor command is diagnosed");
    Expect(
        aDiagnostic.mCommand == "LayerSetSpacing",
        "unsupported command name is preserved");
}

} // namespace

void RunBitmapFontResourceManagerTests()
{
    TestLoadingMeasurementAndSprites();
    TestUnsupportedCommandDiagnostic();
}
