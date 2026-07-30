#include "pvz/engine/core/XmlDocument.h"
#include "pvz/game/ReanimationPlayer.h"

#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

void Expect(bool theCondition, const char* theMessage);

namespace
{

class FixtureXmlLoader final
    : public pvz::engine::IXmlDocumentLoader
{
public:
    explicit FixtureXmlLoader(std::string_view theXml)
        : mXml(theXml)
    {
    }

    [[nodiscard]] bool Load(
        std::string_view thePath,
        pvz::engine::XmlDocumentMode theMode,
        std::vector<pvz::engine::XmlNode>& theRoots,
        pvz::engine::XmlDocumentDiagnostic& theDiagnostic)
        const override
    {
        static_cast<void>(thePath);
        if (theMode != pvz::engine::XmlDocumentMode::Fragment)
            return false;

        pvz::engine::core::XmlDocument aDocument;
        if (!aDocument.ParseFragment(mXml))
        {
            theDiagnostic = {
                .mError =
                    pvz::engine::XmlDocumentError::ParserFailed,
            };
            return false;
        }
        const auto aRoots = aDocument.GetRoots();
        theRoots.assign(aRoots.begin(), aRoots.end());
        theDiagnostic = {};
        return true;
    }

private:
    std::string mXml;
};

class FakeImageResources final
    : public pvz::engine::IImageResources
{
public:
    [[nodiscard]] bool Load(
        std::string_view theResourceId,
        pvz::engine::ImageResource& theResource,
        pvz::engine::ImageResourceDiagnostic& theDiagnostic) override
    {
        ++mLoadCount;
        mLoadedIds.emplace_back(theResourceId);
        if (theResourceId == mFailId ||
            theResourceId == mSourceOnlyId)
        {
            theResource = {};
            theDiagnostic = {
                .mError =
                    pvz::engine::ImageResourceError::
                        ResourceNotFound,
            };
            return false;
        }

        const auto anIndex = mLoadCount;
        theResource = {
            .mImage =
                {
                    .mIndex = anIndex,
                    .mGeneration = 1,
                },
            .mSize =
                theResourceId == "IMAGE_BODY"
                    ? pvz::engine::SizeI{20, 10}
                    : pvz::engine::SizeI{10, 10},
            .mRows = 1,
            .mColumns =
                theResourceId == "IMAGE_BODY" ? 2U : 1U,
        };
        theDiagnostic = {};
        return true;
    }

    [[nodiscard]] bool LoadSource(
        std::string_view theLogicalPath,
        pvz::engine::ImageResource& theResource,
        pvz::engine::ImageResourceDiagnostic& theDiagnostic) override
    {
        ++mSourceLoadCount;
        mLastSourcePath = theLogicalPath;
        if (theLogicalPath != mExpectedSourcePath)
        {
            theResource = {};
            theDiagnostic = {
                .mError =
                    pvz::engine::ImageResourceError::
                        SourceNotFound,
            };
            return false;
        }
        theResource = {
            .mImage = {.mIndex = 90, .mGeneration = 1},
            .mSize = {12, 14},
            .mRows = 1,
            .mColumns = 1,
        };
        theDiagnostic = {};
        return true;
    }

    void Release(pvz::engine::ImageHandle theImage) override
    {
        if (theImage.IsValid())
            ++mReleaseCount;
    }

    std::string mFailId;
    std::string mSourceOnlyId;
    std::string mExpectedSourcePath;
    std::string mLastSourcePath;
    std::vector<std::string> mLoadedIds;
    std::uint32_t mLoadCount{};
    std::uint32_t mSourceLoadCount{};
    std::uint32_t mReleaseCount{};
};

[[nodiscard]] bool NearlyEqual(
    float theLeft,
    float theRight,
    float theTolerance = 0.001F)
{
    return std::fabs(theLeft - theRight) <= theTolerance;
}

[[nodiscard]] std::string_view GetPlaybackFixture()
{
    return
        "<fps>10</fps>"
        "<track><name>anim_full_idle</name>"
        "<t><f>0</f></t><t><f>0</f></t></track>"
        "<track><name>body</name>"
        "<t><x>0</x><y>0</y><kx>0</kx><ky>0</ky>"
        "<sx>1</sx><sy>1</sy><f>5</f><a>1</a>"
        "<i>IMAGE_BODY</i></t>"
        "<t><x>10</x><y>20</y><kx>90</kx><ky>0</ky>"
        "<sx>2</sx><sy>1</sy><f>5</f><a>0.5</a></t>"
        "</track>"
        "<track><name>disappearing</name>"
        "<t><f>0</f><i>IMAGE_GHOST</i></t>"
        "<t><f>-1</f></t></track>";
}

void TestClipLoadingAndLayerSelection()
{
    FixtureXmlLoader aDocuments(GetPlaybackFixture());
    FakeImageResources anImages;
    pvz::game::ReanimationClip aClip;
    pvz::game::ReanimationClipDiagnostic aDiagnostic;

    Expect(
        aClip.Load(
            aDocuments,
            anImages,
            "fixture.reanim",
            aDiagnostic),
        "reanimation clip loads definition and image resources");
    Expect(aClip.IsLoaded(), "loaded reanimation clip is available");
    Expect(
        aClip.GetTrackCount() == 3,
        "reanimation clip retains track order");
    Expect(
        anImages.mLoadCount == 2,
        "reanimation clip loads each unique image once");

    pvz::game::ReanimationLayer aLayer;
    Expect(
        aClip.FindLayer("ANIM_FULL_IDLE", aLayer),
        "reanimation layer lookup is case-insensitive");
    Expect(
        aLayer.mFrameStart == 0 &&
            aLayer.mFrameCount == 2,
        "reanimation layer exposes fixed-width frame bounds");

    aClip.Release(anImages);
    Expect(!aClip.IsLoaded(), "released clip clears definition");
    Expect(
        anImages.mReleaseCount == 2,
        "released clip returns all image handles");
}

void TestPlaybackInterpolationAndAffineQuad()
{
    FixtureXmlLoader aDocuments(GetPlaybackFixture());
    FakeImageResources anImages;
    pvz::game::ReanimationClip aClip;
    pvz::game::ReanimationClipDiagnostic aDiagnostic;
    Expect(
        aClip.Load(
            aDocuments,
            anImages,
            "fixture.reanim",
            aDiagnostic),
        "playback fixture loads");

    pvz::game::ReanimationPlayer aPlayer;
    Expect(
        aPlayer.Bind(aClip, "anim_full_idle"),
        "player binds to named layer");

    std::vector<pvz::engine::SpriteDraw> aStartSprites;
    aPlayer.AppendSprites(
        {100.0F, 200.0F},
        {255, 255, 255, 200},
        aStartSprites);
    Expect(
        aStartSprites.size() == 2,
        "visible image tracks emit sprites in definition order");
    if (!aStartSprites.empty())
    {
        Expect(
            aStartSprites[0].mSource.mOrigin.mX == 10 &&
                aStartSprites[0].mSource.mSize.mWidth == 10,
            "reanimation image frame wraps across atlas columns");
        Expect(
            aStartSprites[0].mGeometryMode ==
                pvz::engine::SpriteGeometryMode::
                    DestinationQuad,
            "reanimation emits portable transformed quads");
    }

    for (std::uint32_t aTick = 0; aTick < 10; ++aTick)
        aPlayer.Update();
    Expect(
        aPlayer.GetTick() == 10,
        "player advances with a fixed-width 100 Hz tick");

    std::vector<pvz::engine::SpriteDraw> aSprites;
    aPlayer.AppendSprites(
        {100.0F, 200.0F},
        {255, 255, 255, 200},
        aSprites);
    Expect(
        aSprites.size() == 1,
        "disappearing image frame truncates during interpolation");
    if (aSprites.size() == 1)
    {
        const auto& aDraw = aSprites.front();
        Expect(
            aDraw.mColor.mAlpha == 150,
            "interpolated transform alpha multiplies draw alpha");
        Expect(
            NearlyEqual(
                aDraw.mDestinationQuad.mTopLeft.mX,
                105.0F) &&
                NearlyEqual(
                    aDraw.mDestinationQuad.mTopLeft.mY,
                    210.0F),
            "interpolated translation anchors the quad");
        Expect(
            NearlyEqual(
                aDraw.mDestinationQuad.mTopRight.mX,
                115.6066F) &&
                NearlyEqual(
                    aDraw.mDestinationQuad.mTopRight.mY,
                    220.6066F),
            "independent x skew transforms the top edge");
        Expect(
            NearlyEqual(
                aDraw.mDestinationQuad.mBottomLeft.mX,
                105.0F) &&
                NearlyEqual(
                    aDraw.mDestinationQuad.mBottomLeft.mY,
                    220.0F),
            "independent y skew transforms the side edge");
    }

    pvz::game::ReanimationPlayer aRestoredPlayer;
    Expect(
        aRestoredPlayer.Bind(aClip, "anim_full_idle"),
        "restored player binds");
    aRestoredPlayer.RestoreTick(10);
    std::vector<pvz::engine::SpriteDraw> aRestoredSprites;
    aRestoredPlayer.AppendSprites(
        {100.0F, 200.0F},
        {255, 255, 255, 200},
        aRestoredSprites);
    Expect(
        aRestoredSprites.size() == aSprites.size() &&
            !aRestoredSprites.empty() &&
            NearlyEqual(
                aRestoredSprites[0]
                    .mDestinationQuad.mBottomRight.mX,
                aSprites[0]
                    .mDestinationQuad.mBottomRight.mX),
        "restored tick reproduces deterministic geometry");

    aClip.Release(anImages);
}

void TestClipFailuresAreTransactional()
{
    FixtureXmlLoader aDocuments(
        "<fps>12</fps>"
        "<track><name>anim</name><t><f>0</f></t></track>"
        "<track><name>first</name>"
        "<t><f>0</f><i>IMAGE_BODY</i></t></track>"
        "<track><name>second</name>"
        "<t><f>0</f><i>IMAGE_MISSING</i></t></track>");
    FakeImageResources anImages;
    anImages.mFailId = "IMAGE_MISSING";
    pvz::game::ReanimationClip aClip;
    pvz::game::ReanimationClipDiagnostic aDiagnostic;
    Expect(
        !aClip.Load(
            aDocuments,
            anImages,
            "missing-image.reanim",
            aDiagnostic),
        "missing reanimation image rejects clip");
    Expect(
        aDiagnostic.mError ==
            pvz::game::ReanimationClipError::ImageLoadFailed &&
            aDiagnostic.mDetail == "IMAGE_MISSING",
        "missing image reports stable clip diagnostic");
    Expect(
        !aClip.IsLoaded() &&
            anImages.mReleaseCount == 1,
        "failed clip load releases prior image handles");

    FixtureXmlLoader mismatchedDocuments(
        "<track><name>a</name><t></t></track>"
        "<track><name>b</name><t></t><t></t></track>");
    FakeImageResources unusedImages;
    Expect(
        !aClip.Load(
            mismatchedDocuments,
            unusedImages,
            "mismatched.reanim",
            aDiagnostic) &&
            aDiagnostic.mError ==
                pvz::game::ReanimationClipError::
                    MismatchedTransformCount,
        "mismatched reanimation tracks fail before resource loads");
    Expect(
        unusedImages.mLoadCount == 0,
        "invalid clip does not acquire image resources");
}

void TestConventionBasedReanimationImageLoading()
{
    FixtureXmlLoader aDocuments(
        "<track><name>anim</name><t><f>0</f></t></track>"
        "<track><name>body</name><t><f>0</f>"
        "<i>IMAGE_REANIM_SOURCE_ONLY</i></t></track>");
    FakeImageResources anImages;
    anImages.mSourceOnlyId = "IMAGE_REANIM_SOURCE_ONLY";
    anImages.mExpectedSourcePath = "reanim\\SOURCE_ONLY";
    pvz::game::ReanimationClip aClip;
    pvz::game::ReanimationClipDiagnostic aDiagnostic;
    Expect(
        aClip.Load(
            aDocuments,
            anImages,
            "source-only.reanim",
            aDiagnostic),
        "reanimation image falls back to its retail source convention");
    Expect(
        anImages.mSourceLoadCount == 1 &&
            anImages.mLastSourcePath ==
                "reanim\\SOURCE_ONLY",
        "reanimation source fallback remains behind image protocol");
    aClip.Release(anImages);
}

} // namespace

void RunReanimationPlayerTests()
{
    TestClipLoadingAndLayerSelection();
    TestPlaybackInterpolationAndAffineQuad();
    TestClipFailuresAreTransactional();
    TestConventionBasedReanimationImageLoading();
}
