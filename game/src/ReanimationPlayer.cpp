#include "pvz/game/ReanimationPlayer.h"

#include "pvz/engine/Types.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>

namespace pvz::game
{
namespace
{

[[nodiscard]] char LowercaseAscii(char theCharacter)
{
    if (theCharacter >= 'A' && theCharacter <= 'Z')
        return static_cast<char>(theCharacter - 'A' + 'a');
    return theCharacter;
}

[[nodiscard]] bool NamesEqual(
    std::string_view theLeft,
    std::string_view theRight)
{
    if (theLeft.size() != theRight.size())
        return false;
    for (std::size_t anIndex = 0;
         anIndex < theLeft.size();
         ++anIndex)
    {
        if (LowercaseAscii(theLeft[anIndex]) !=
            LowercaseAscii(theRight[anIndex]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool StartsWithAsciiInsensitive(
    std::string_view theText,
    std::string_view thePrefix)
{
    if (theText.size() < thePrefix.size())
        return false;
    for (std::size_t anIndex = 0;
         anIndex < thePrefix.size();
         ++anIndex)
    {
        if (LowercaseAscii(theText[anIndex]) !=
            LowercaseAscii(thePrefix[anIndex]))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string MakeReanimationSourcePath(
    std::string_view theImageId)
{
    constexpr std::string_view kPrefix = "IMAGE_REANIM_";
    if (theImageId.size() <= kPrefix.size() ||
        !NamesEqual(
            theImageId.substr(0, kPrefix.size()),
            kPrefix))
    {
        return {};
    }

    std::string aPath("reanim\\");
    aPath.append(theImageId.substr(kPrefix.size()));
    return aPath;
}

[[nodiscard]] bool LoadReanimationImage(
    engine::IImageResources& theImages,
    std::string_view theImageId,
    engine::ImageResource& theResource,
    engine::ImageResourceDiagnostic& theDiagnostic)
{
    if (theImages.Load(
            theImageId,
            theResource,
            theDiagnostic))
    {
        return true;
    }
    if (theDiagnostic.mError !=
        engine::ImageResourceError::ResourceNotFound)
    {
        return false;
    }

    const auto aSourcePath =
        MakeReanimationSourcePath(theImageId);
    return !aSourcePath.empty() &&
           theImages.LoadSource(
               aSourcePath,
               theResource,
               theDiagnostic);
}

[[nodiscard]] float Lerp(
    float theBefore,
    float theAfter,
    float theFraction)
{
    return theBefore +
           (theAfter - theBefore) * theFraction;
}

[[nodiscard]] ReanimationTransform InterpolateTransform(
    const ReanimationTransform& theBefore,
    const ReanimationTransform& theAfter,
    float theFraction)
{
    ReanimationTransform aTransform{
        .mTranslationX = Lerp(
            theBefore.mTranslationX,
            theAfter.mTranslationX,
            theFraction),
        .mTranslationY = Lerp(
            theBefore.mTranslationY,
            theAfter.mTranslationY,
            theFraction),
        .mSkewX = Lerp(
            theBefore.mSkewX,
            theAfter.mSkewX,
            theFraction),
        .mSkewY = Lerp(
            theBefore.mSkewY,
            theAfter.mSkewY,
            theFraction),
        .mScaleX = Lerp(
            theBefore.mScaleX,
            theAfter.mScaleX,
            theFraction),
        .mScaleY = Lerp(
            theBefore.mScaleY,
            theAfter.mScaleY,
            theFraction),
        .mFrame = theBefore.mFrame,
        .mAlpha = Lerp(
            theBefore.mAlpha,
            theAfter.mAlpha,
            theFraction),
        .mImageId = theBefore.mImageId,
        .mFontId = theBefore.mFontId,
        .mText = theBefore.mText,
    };
    if (theBefore.mFrame >= 0.0F &&
        theAfter.mFrame < 0.0F &&
        theFraction > 0.0F)
    {
        aTransform.mFrame = -1.0F;
    }
    return aTransform;
}

[[nodiscard]] engine::PointF TransformPoint(
    engine::PointF thePosition,
    float theLocalX,
    float theLocalY,
    float theM00,
    float theM01,
    float theM10,
    float theM11,
    const ReanimationTransform& theTransform)
{
    return {
        .mX =
            thePosition.mX +
            theTransform.mTranslationX +
            theM00 * theLocalX +
            theM01 * theLocalY,
        .mY =
            thePosition.mY +
            theTransform.mTranslationY +
            theM10 * theLocalX +
            theM11 * theLocalY,
    };
}

template <typename Entry>
void ReleaseEntries(
    engine::IImageResources& theImages,
    std::vector<Entry>& theEntries)
{
    for (auto& anEntry : theEntries)
    {
        if (anEntry.mResource.mImage.IsValid())
            theImages.Release(anEntry.mResource.mImage);
    }
    theEntries.clear();
}

} // namespace

bool ReanimationClip::Load(
    const engine::IXmlDocumentLoader& theDocuments,
    engine::IImageResources& theImages,
    std::string_view thePath,
    ReanimationClipDiagnostic& theDiagnostic)
{
    theDiagnostic = {};
    if (IsLoaded() || !mImages.empty())
    {
        theDiagnostic.mError =
            ReanimationClipError::AlreadyLoaded;
        return false;
    }

    DefinitionLoader aLoader(theDocuments);
    ReanimationDefinition aDefinition;
    if (!aLoader.LoadReanimation(thePath, aDefinition))
    {
        theDiagnostic.mError =
            ReanimationClipError::DefinitionLoadFailed;
        theDiagnostic.mDefinitionError = aLoader.GetError();
        theDiagnostic.mSourceDiagnostic =
            aLoader.GetSourceDiagnostic();
        theDiagnostic.mDetail = aLoader.GetErrorDetail();
        return false;
    }
    if (aDefinition.mTracks.empty() ||
        aDefinition.mTracks.front().mTransforms.empty())
    {
        theDiagnostic.mError =
            ReanimationClipError::EmptyDefinition;
        return false;
    }

    const auto aTransformCount =
        aDefinition.mTracks.front().mTransforms.size();
    if (aTransformCount >
        std::numeric_limits<std::uint32_t>::max())
    {
        theDiagnostic.mError =
            ReanimationClipError::TooManyTransforms;
        return false;
    }
    for (const auto& aTrack : aDefinition.mTracks)
    {
        if (aTrack.mTransforms.size() != aTransformCount)
        {
            theDiagnostic.mError =
                ReanimationClipError::MismatchedTransformCount;
            theDiagnostic.mDetail = aTrack.mName;
            return false;
        }
    }

    std::vector<ImageEntry> anImages;
    for (const auto& aTrack : aDefinition.mTracks)
    {
        for (const auto& aTransform : aTrack.mTransforms)
        {
            if (aTransform.mImageId.empty())
                continue;
            const auto anExisting = std::find_if(
                anImages.begin(),
                anImages.end(),
                [&aTransform](const ImageEntry& theEntry)
                {
                    return theEntry.mId ==
                           aTransform.mImageId;
                });
            if (anExisting != anImages.end())
                continue;

            ImageEntry anEntry{
                .mId = aTransform.mImageId,
            };
            engine::ImageResourceDiagnostic anImageDiagnostic;
            if (!LoadReanimationImage(
                    theImages,
                    anEntry.mId,
                    anEntry.mResource,
                    anImageDiagnostic))
            {
                ReleaseEntries(theImages, anImages);
                theDiagnostic.mError =
                    ReanimationClipError::ImageLoadFailed;
                theDiagnostic.mImageDiagnostic =
                    std::move(anImageDiagnostic);
                theDiagnostic.mDetail = anEntry.mId;
                return false;
            }

            const auto& aResource = anEntry.mResource;
            if (!aResource.mImage.IsValid() ||
                aResource.mSize.mWidth == 0 ||
                aResource.mSize.mHeight == 0 ||
                aResource.mRows == 0 ||
                aResource.mColumns == 0 ||
                aResource.mSize.mWidth % aResource.mColumns != 0 ||
                aResource.mSize.mHeight % aResource.mRows != 0 ||
                aResource.mSize.mWidth >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<std::int32_t>::max()) ||
                aResource.mSize.mHeight >
                    static_cast<std::uint32_t>(
                        std::numeric_limits<std::int32_t>::max()))
            {
                if (aResource.mImage.IsValid())
                    theImages.Release(aResource.mImage);
                ReleaseEntries(theImages, anImages);
                theDiagnostic.mError =
                    ReanimationClipError::InvalidImageGeometry;
                theDiagnostic.mDetail = anEntry.mId;
                return false;
            }
            anImages.push_back(std::move(anEntry));
        }
    }

    mDefinition = std::move(aDefinition);
    mImages = std::move(anImages);
    return true;
}

void ReanimationClip::Release(
    engine::IImageResources& theImages)
{
    ReleaseEntries(theImages, mImages);
    mDefinition = {};
}

bool ReanimationClip::IsLoaded() const
{
    return !mDefinition.mTracks.empty();
}

bool ReanimationClip::FindLayer(
    std::string_view theTrackName,
    ReanimationLayer& theLayer) const
{
    const auto aTrack = std::find_if(
        mDefinition.mTracks.begin(),
        mDefinition.mTracks.end(),
        [theTrackName](const ReanimationTrack& theCandidate)
        {
            return NamesEqual(
                theCandidate.mName,
                theTrackName);
        });
    if (aTrack == mDefinition.mTracks.end() ||
        aTrack->mTransforms.empty())
    {
        return false;
    }

    std::size_t aFrameStart{};
    std::size_t aFrameCount{1};
    for (std::size_t anIndex = 0;
         anIndex < aTrack->mTransforms.size();
         ++anIndex)
    {
        if (aTrack->mTransforms[anIndex].mFrame >= 0.0F)
        {
            aFrameStart = anIndex;
            break;
        }
    }
    for (std::size_t anIndex = aFrameStart;
         anIndex < aTrack->mTransforms.size();
         ++anIndex)
    {
        if (aTrack->mTransforms[anIndex].mFrame >= 0.0F)
            aFrameCount = anIndex - aFrameStart + 1;
    }

    theLayer = {
        .mFrameStart =
            static_cast<std::uint32_t>(aFrameStart),
        .mFrameCount =
            static_cast<std::uint32_t>(aFrameCount),
    };
    return true;
}

std::uint32_t ReanimationClip::GetTrackCount() const
{
    return static_cast<std::uint32_t>(
        mDefinition.mTracks.size());
}

const engine::ImageResource* ReanimationClip::FindImage(
    std::string_view theImageId) const
{
    const auto anEntry = std::find_if(
        mImages.begin(),
        mImages.end(),
        [theImageId](const ImageEntry& theCandidate)
        {
            return theCandidate.mId == theImageId;
        });
    if (anEntry == mImages.end())
        return nullptr;
    return &anEntry->mResource;
}

bool ReanimationPlayer::Bind(
    const ReanimationClip& theClip)
{
    if (!theClip.IsLoaded() ||
        theClip.mDefinition.mTracks.empty() ||
        theClip.mDefinition.mTracks.front().mTransforms.empty() ||
        theClip.mDefinition.mTracks.front().mTransforms.size() >
            std::numeric_limits<std::uint32_t>::max())
    {
        Reset();
        return false;
    }

    mClip = &theClip;
    mLayer = {
        .mFrameStart = 0,
        .mFrameCount =
            static_cast<std::uint32_t>(
                theClip.mDefinition.mTracks.front()
                    .mTransforms.size()),
    };
    mTick = 0;
    mFramesPerSecond = 0.0F;
    mHiddenTrackPrefixes.clear();
    return true;
}

bool ReanimationPlayer::Bind(
    const ReanimationClip& theClip,
    std::string_view theLayerName)
{
    ReanimationLayer aLayer;
    if (!theClip.IsLoaded() ||
        !theClip.FindLayer(theLayerName, aLayer))
    {
        Reset();
        return false;
    }
    mClip = &theClip;
    mLayer = aLayer;
    mTick = 0;
    mFramesPerSecond = 0.0F;
    mHiddenTrackPrefixes.clear();
    return true;
}

void ReanimationPlayer::Reset()
{
    mClip = nullptr;
    mLayer = {};
    mTick = 0;
    mFramesPerSecond = 0.0F;
    mHiddenTrackPrefixes.clear();
}

void ReanimationPlayer::Update()
{
    if (mClip != nullptr)
        ++mTick;
}

void ReanimationPlayer::RestoreTick(std::uint64_t theTick)
{
    mTick = theTick;
}

bool ReanimationPlayer::SetFramesPerSecond(
    float theFramesPerSecond)
{
    if (mClip == nullptr ||
        !std::isfinite(theFramesPerSecond) ||
        theFramesPerSecond <= 0.0F)
    {
        return false;
    }
    mFramesPerSecond = theFramesPerSecond;
    return true;
}

void ReanimationPlayer::SetHiddenTrackPrefixes(
    std::span<const std::string_view> thePrefixes)
{
    mHiddenTrackPrefixes.clear();
    mHiddenTrackPrefixes.reserve(thePrefixes.size());
    for (const auto aPrefix : thePrefixes)
        mHiddenTrackPrefixes.emplace_back(aPrefix);
}

bool ReanimationPlayer::IsBound() const
{
    return mClip != nullptr;
}

std::uint64_t ReanimationPlayer::GetTick() const
{
    return mTick;
}

void ReanimationPlayer::AppendSprites(
    engine::PointF thePosition,
    engine::ColorRgba8 theColor,
    std::vector<engine::SpriteDraw>& theSprites) const
{
    if (mClip == nullptr || mLayer.mFrameCount == 0)
        return;

    const auto aFramesPerSecond =
        mFramesPerSecond > 0.0F
            ? mFramesPerSecond
            : mClip->mDefinition.mFramesPerSecond;
    const double anElapsedAnimationFrames =
        static_cast<double>(mTick) *
        static_cast<double>(aFramesPerSecond) /
        static_cast<double>(engine::kSimulationFrequencyHz);
    const double anAnimationTime = std::fmod(
        anElapsedAnimationFrames /
            static_cast<double>(mLayer.mFrameCount),
        1.0);
    const auto anInterpolatedFrameCount =
        mLayer.mFrameCount > 1
            ? mLayer.mFrameCount - 1
            : 0;
    const double anAnimationPosition =
        static_cast<double>(mLayer.mFrameStart) +
        anAnimationTime *
            static_cast<double>(anInterpolatedFrameCount);
    const double aFrameBeforeFloor =
        std::floor(anAnimationPosition);
    auto aFrameBefore =
        static_cast<std::uint32_t>(aFrameBeforeFloor);
    const auto aLastFrame =
        mLayer.mFrameStart + mLayer.mFrameCount - 1;
    aFrameBefore = std::min(aFrameBefore, aLastFrame);
    const auto aFrameAfter =
        aFrameBefore >= aLastFrame
            ? aFrameBefore
            : aFrameBefore + 1;
    const float aFraction =
        static_cast<float>(
            anAnimationPosition - aFrameBeforeFloor);

    for (const auto& aTrack : mClip->mDefinition.mTracks)
    {
        const auto isHidden = std::any_of(
            mHiddenTrackPrefixes.begin(),
            mHiddenTrackPrefixes.end(),
            [&aTrack](const std::string& thePrefix)
            {
                return StartsWithAsciiInsensitive(
                    aTrack.mName,
                    thePrefix);
            });
        if (isHidden)
            continue;

        const auto aTransform = InterpolateTransform(
            aTrack.mTransforms[aFrameBefore],
            aTrack.mTransforms[aFrameAfter],
            aFraction);
        if (aTransform.mFrame < 0.0F ||
            aTransform.mImageId.empty())
        {
            continue;
        }

        const auto* anImage =
            mClip->FindImage(aTransform.mImageId);
        if (anImage == nullptr)
            continue;

        const auto aCellWidth =
            anImage->mSize.mWidth / anImage->mColumns;
        const auto aCellHeight =
            anImage->mSize.mHeight / anImage->mRows;
        const double aRoundedFrame =
            std::floor(
                static_cast<double>(aTransform.mFrame) +
                0.5);
        const auto anImageFrame =
            static_cast<std::uint32_t>(
                std::fmod(
                    aRoundedFrame,
                    static_cast<double>(anImage->mColumns)));

        const float anAlphaValue = std::clamp(
            aTransform.mAlpha *
                static_cast<float>(theColor.mAlpha),
            0.0F,
            255.0F);
        const auto anAlpha = static_cast<std::uint8_t>(
            std::floor(anAlphaValue + 0.5F));
        if (anAlpha == 0)
            continue;

        const float aSkewX =
            -aTransform.mSkewX *
            std::numbers::pi_v<float> /
            180.0F;
        const float aSkewY =
            -aTransform.mSkewY *
            std::numbers::pi_v<float> /
            180.0F;
        const float aM00 =
            std::cos(aSkewX) * aTransform.mScaleX;
        const float aM10 =
            -std::sin(aSkewX) * aTransform.mScaleX;
        const float aM01 =
            std::sin(aSkewY) * aTransform.mScaleY;
        const float aM11 =
            std::cos(aSkewY) * aTransform.mScaleY;
        const float aWidth = static_cast<float>(aCellWidth);
        const float aHeight = static_cast<float>(aCellHeight);

        theSprites.push_back({
            .mImage = anImage->mImage,
            .mSource =
                {
                    .mOrigin =
                        {
                            static_cast<std::int32_t>(
                                anImageFrame * aCellWidth),
                            0,
                        },
                    .mSize = {aCellWidth, aCellHeight},
                },
            .mDestinationQuad =
                {
                    .mTopLeft = TransformPoint(
                        thePosition,
                        0.0F,
                        0.0F,
                        aM00,
                        aM01,
                        aM10,
                        aM11,
                        aTransform),
                    .mTopRight = TransformPoint(
                        thePosition,
                        aWidth,
                        0.0F,
                        aM00,
                        aM01,
                        aM10,
                        aM11,
                        aTransform),
                    .mBottomLeft = TransformPoint(
                        thePosition,
                        0.0F,
                        aHeight,
                        aM00,
                        aM01,
                        aM10,
                        aM11,
                        aTransform),
                    .mBottomRight = TransformPoint(
                        thePosition,
                        aWidth,
                        aHeight,
                        aM00,
                        aM01,
                        aM10,
                        aM11,
                        aTransform),
                },
            .mGeometryMode =
                engine::SpriteGeometryMode::DestinationQuad,
            .mColor =
                {
                    theColor.mRed,
                    theColor.mGreen,
                    theColor.mBlue,
                    anAlpha,
                },
            .mFilterMode = engine::FilterMode::Linear,
        });
    }
}

const char* GetReanimationClipErrorMessage(
    ReanimationClipError theError)
{
    switch (theError)
    {
    case ReanimationClipError::None:
        return "no error";
    case ReanimationClipError::AlreadyLoaded:
        return "reanimation clip is already loaded";
    case ReanimationClipError::DefinitionLoadFailed:
        return "reanimation definition could not be loaded";
    case ReanimationClipError::EmptyDefinition:
        return "reanimation definition has no frames";
    case ReanimationClipError::MismatchedTransformCount:
        return "reanimation tracks have different frame counts";
    case ReanimationClipError::TooManyTransforms:
        return "reanimation has too many transforms";
    case ReanimationClipError::ImageLoadFailed:
        return "reanimation image could not be loaded";
    case ReanimationClipError::InvalidImageGeometry:
        return "reanimation image grid is invalid";
    }
    return "unknown reanimation clip error";
}

} // namespace pvz::game
