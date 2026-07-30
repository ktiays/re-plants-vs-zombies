#include "pvz/platform/macos/MetalRenderDevice.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <dispatch/dispatch.h>
#include <simd/simd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pvz::platform::macos
{
namespace
{

inline constexpr std::uint32_t kLogicalWidth = 800;
inline constexpr std::uint32_t kLogicalHeight = 600;
inline constexpr std::uint32_t kBytesPerPixel = 4;
inline constexpr std::size_t kVertexBufferCount = 3;

struct SpriteVertex
{
    simd_float2 mPosition;
    simd_float2 mTextureCoordinate;
    simd_float4 mColor;
};

static_assert(sizeof(SpriteVertex) == 32);

class MetalRenderFrame final : public engine::IRenderFrame
{
public:
    void Reset()
    {
        mClearColor = {0, 0, 0, 255};
        mDraws.clear();
    }

    [[nodiscard]] engine::SizeI GetLogicalSize() const override
    {
        return {kLogicalWidth, kLogicalHeight};
    }

    void Clear(engine::ColorRgba8 theColor) override
    {
        mClearColor = theColor;
    }

    void SubmitSprites(
        std::span<const engine::SpriteDraw> theDraws) override
    {
        mDraws.insert(mDraws.end(), theDraws.begin(), theDraws.end());
    }

    [[nodiscard]] engine::ColorRgba8 GetClearColor() const
    {
        return mClearColor;
    }

    [[nodiscard]] std::span<const engine::SpriteDraw> GetDraws() const
    {
        return mDraws;
    }

private:
    engine::ColorRgba8 mClearColor{0, 0, 0, 255};
    std::vector<engine::SpriteDraw> mDraws;
};

struct TextureSlot
{
    id<MTLTexture> mTexture;
    engine::SizeI mSize{};
    std::uint32_t mGeneration{1};
};

struct PendingUpload
{
    id<MTLBuffer> mStagingBuffer;
    id<MTLTexture> mTexture;
    MTLOrigin mDestinationOrigin{};
    MTLSize mSize{};
    NSUInteger mBytesPerRow{};
    NSUInteger mBytesPerImage{};
};

struct SpriteBatch
{
    id<MTLTexture> mTexture;
    engine::BlendMode mBlendMode{engine::BlendMode::Normal};
    engine::FilterMode mFilterMode{engine::FilterMode::Nearest};
    engine::AddressMode mAddressMode{engine::AddressMode::Clamp};
    MTLScissorRect mScissor{};
    NSUInteger mFirstVertex{};
    NSUInteger mVertexCount{};
};

[[nodiscard]] bool ScissorsEqual(
    const MTLScissorRect& theLeft,
    const MTLScissorRect& theRight)
{
    return theLeft.x == theRight.x &&
           theLeft.y == theRight.y &&
           theLeft.width == theRight.width &&
           theLeft.height == theRight.height;
}

[[nodiscard]] std::uint32_t NextGeneration(std::uint32_t theGeneration)
{
    ++theGeneration;
    if (theGeneration == 0)
        theGeneration = 1;
    return theGeneration;
}

[[nodiscard]] bool TryMultiply(
    std::size_t theLeft,
    std::size_t theRight,
    std::size_t& theResult)
{
    if (theLeft != 0 &&
        theRight > std::numeric_limits<std::size_t>::max() / theLeft)
    {
        return false;
    }
    theResult = theLeft * theRight;
    return true;
}

[[nodiscard]] bool TryAlignUp(
    std::size_t theValue,
    std::size_t theAlignment,
    std::size_t& theResult)
{
    if (theAlignment == 0)
        return false;
    const auto aRemainder = theValue % theAlignment;
    if (aRemainder == 0)
    {
        theResult = theValue;
        return true;
    }
    const auto anIncrease = theAlignment - aRemainder;
    if (theValue >
        std::numeric_limits<std::size_t>::max() - anIncrease)
    {
        return false;
    }
    theResult = theValue + anIncrease;
    return true;
}

[[nodiscard]] MTLScissorRect MakeScissor(
    const engine::SpriteDraw& theDraw,
    bool& theIsEmpty)
{
    std::int64_t aLeft = 0;
    std::int64_t aTop = 0;
    std::int64_t aRight = kLogicalWidth;
    std::int64_t aBottom = kLogicalHeight;

    if (theDraw.mClipMode == engine::ClipMode::Enabled)
    {
        aLeft = theDraw.mClip.mOrigin.mX;
        aTop = theDraw.mClip.mOrigin.mY;
        aRight =
            aLeft + static_cast<std::int64_t>(
                        theDraw.mClip.mSize.mWidth);
        aBottom =
            aTop + static_cast<std::int64_t>(
                       theDraw.mClip.mSize.mHeight);
        aLeft = std::clamp<std::int64_t>(
            aLeft,
            0,
            kLogicalWidth);
        aTop = std::clamp<std::int64_t>(
            aTop,
            0,
            kLogicalHeight);
        aRight = std::clamp<std::int64_t>(
            aRight,
            0,
            kLogicalWidth);
        aBottom = std::clamp<std::int64_t>(
            aBottom,
            0,
            kLogicalHeight);
    }

    theIsEmpty = aRight <= aLeft || aBottom <= aTop;
    return {
        static_cast<NSUInteger>(aLeft),
        static_cast<NSUInteger>(aTop),
        static_cast<NSUInteger>(std::max<std::int64_t>(
            0,
            aRight - aLeft)),
        static_cast<NSUInteger>(std::max<std::int64_t>(
            0,
            aBottom - aTop)),
    };
}

[[nodiscard]] simd_float2 RotatePoint(
    float theX,
    float theY,
    float thePivotX,
    float thePivotY,
    float theCosine,
    float theSine)
{
    const float aRelativeX = theX - thePivotX;
    const float aRelativeY = theY - thePivotY;
    return simd_make_float2(
        thePivotX + aRelativeX * theCosine - aRelativeY * theSine,
        thePivotY + aRelativeX * theSine + aRelativeY * theCosine);
}

[[nodiscard]] std::string DescribeError(
    NSString* thePrefix,
    NSError* theError)
{
    NSString* aMessage = thePrefix;
    if (theError != nil)
    {
        aMessage = [NSString stringWithFormat:
            @"%@: %@",
            thePrefix,
            theError.localizedDescription];
    }
    return std::string(aMessage.UTF8String);
}

} // namespace

struct MetalRenderDevice::Implementation
{
    id<MTLDevice> mDevice;
    id<MTLCommandQueue> mCommandQueue;
    CAMetalLayer* mLayer;
    id<CAMetalDrawable> mDrawable;
    id<MTLTexture> mLogicalTarget;
    id<MTLRenderPipelineState> mNormalSpritePipeline;
    id<MTLRenderPipelineState> mAdditiveSpritePipeline;
    id<MTLRenderPipelineState> mPresentationPipeline;
    id<MTLSamplerState> mNearestClampSampler;
    id<MTLSamplerState> mNearestRepeatSampler;
    id<MTLSamplerState> mLinearClampSampler;
    id<MTLSamplerState> mLinearRepeatSampler;
    std::array<id<MTLBuffer>, kVertexBufferCount> mVertexBuffers{};
    std::array<std::size_t, kVertexBufferCount> mVertexBufferCapacities{};
    std::size_t mNextVertexBuffer{};
    dispatch_semaphore_t mVertexBufferSemaphore{
        dispatch_semaphore_create(
            static_cast<intptr_t>(kVertexBufferCount))};
    std::vector<TextureSlot> mTextureSlots;
    std::vector<std::uint32_t> mFreeTextureSlots;
    std::vector<PendingUpload> mPendingUploads;
    MetalRenderFrame mFrame;
    std::string mLastError;
    bool mFrameActive{};

    [[nodiscard]] TextureSlot* FindTexture(
        engine::ImageHandle theImage)
    {
        if (!theImage.IsValid() ||
            theImage.mIndex >= mTextureSlots.size())
        {
            return nullptr;
        }
        auto& aSlot = mTextureSlots[theImage.mIndex];
        if (aSlot.mTexture == nil ||
            aSlot.mGeneration != theImage.mGeneration)
        {
            return nullptr;
        }
        return &aSlot;
    }

    [[nodiscard]] const TextureSlot* FindTexture(
        engine::ImageHandle theImage) const
    {
        if (!theImage.IsValid() ||
            theImage.mIndex >= mTextureSlots.size())
        {
            return nullptr;
        }
        const auto& aSlot = mTextureSlots[theImage.mIndex];
        if (aSlot.mTexture == nil ||
            aSlot.mGeneration != theImage.mGeneration)
        {
            return nullptr;
        }
        return &aSlot;
    }

    [[nodiscard]] id<MTLSamplerState> GetSampler(
        engine::FilterMode theFilterMode,
        engine::AddressMode theAddressMode) const
    {
        if (theFilterMode == engine::FilterMode::Linear)
        {
            return theAddressMode == engine::AddressMode::Repeat
                       ? mLinearRepeatSampler
                       : mLinearClampSampler;
        }
        return theAddressMode == engine::AddressMode::Repeat
                   ? mNearestRepeatSampler
                   : mNearestClampSampler;
    }

    [[nodiscard]] bool QueueUpload(
        id<MTLTexture> theTexture,
        engine::SizeI theTextureSize,
        const engine::ImageUpdate& theUpdate)
    {
        if (theTexture == nil ||
            theUpdate.mDestination.mOrigin.mX < 0 ||
            theUpdate.mDestination.mOrigin.mY < 0 ||
            theUpdate.mDestination.mSize.mWidth == 0 ||
            theUpdate.mDestination.mSize.mHeight == 0)
        {
            mLastError = "image update has an invalid destination";
            return false;
        }

        const auto aDestinationX = static_cast<std::uint64_t>(
            theUpdate.mDestination.mOrigin.mX);
        const auto aDestinationY = static_cast<std::uint64_t>(
            theUpdate.mDestination.mOrigin.mY);
        const auto anUpdateWidth = static_cast<std::uint64_t>(
            theUpdate.mDestination.mSize.mWidth);
        const auto anUpdateHeight = static_cast<std::uint64_t>(
            theUpdate.mDestination.mSize.mHeight);
        if (aDestinationX + anUpdateWidth > theTextureSize.mWidth ||
            aDestinationY + anUpdateHeight > theTextureSize.mHeight)
        {
            mLastError = "image update exceeds the texture bounds";
            return false;
        }

        std::size_t aSourceRowBytes{};
        if (!TryMultiply(
                static_cast<std::size_t>(
                    theUpdate.mDestination.mSize.mWidth),
                kBytesPerPixel,
                aSourceRowBytes) ||
            theUpdate.mSourceBytesPerRow < aSourceRowBytes)
        {
            mLastError = "image update row stride is too small";
            return false;
        }

        std::size_t aRequiredSourceBytes = aSourceRowBytes;
        if (theUpdate.mDestination.mSize.mHeight > 1)
        {
            std::size_t aPriorRowsSize{};
            if (!TryMultiply(
                    static_cast<std::size_t>(
                        theUpdate.mDestination.mSize.mHeight - 1),
                    static_cast<std::size_t>(
                        theUpdate.mSourceBytesPerRow),
                    aPriorRowsSize) ||
                aPriorRowsSize >
                    std::numeric_limits<std::size_t>::max() -
                        aSourceRowBytes)
            {
                mLastError = "image update source size overflows";
                return false;
            }
            aRequiredSourceBytes = aPriorRowsSize + aSourceRowBytes;
        }
        if (theUpdate.mPixels.size() < aRequiredSourceBytes)
        {
            mLastError = "image update does not contain enough pixels";
            return false;
        }

        const auto aMetalAlignment = static_cast<std::size_t>(
            [mDevice minimumLinearTextureAlignmentForPixelFormat:
                         MTLPixelFormatBGRA8Unorm]);
        std::size_t aStagingRowBytes{};
        std::size_t aStagingSize{};
        if (!TryAlignUp(
                aSourceRowBytes,
                std::max<std::size_t>(1, aMetalAlignment),
                aStagingRowBytes) ||
            !TryMultiply(
                aStagingRowBytes,
                static_cast<std::size_t>(
                    theUpdate.mDestination.mSize.mHeight),
                aStagingSize))
        {
            mLastError = "image update staging size overflows";
            return false;
        }

        id<MTLBuffer> aStagingBuffer =
            [mDevice newBufferWithLength:aStagingSize
                                 options:MTLResourceStorageModeShared];
        if (aStagingBuffer == nil)
        {
            mLastError = "could not allocate an image staging buffer";
            return false;
        }

        auto* aDestination = static_cast<std::byte*>(
            aStagingBuffer.contents);
        for (std::uint32_t aRow = 0;
             aRow < theUpdate.mDestination.mSize.mHeight;
             ++aRow)
        {
            std::memcpy(
                aDestination +
                    static_cast<std::size_t>(aRow) * aStagingRowBytes,
                theUpdate.mPixels.data() +
                    static_cast<std::size_t>(aRow) *
                        theUpdate.mSourceBytesPerRow,
                aSourceRowBytes);
        }

        mPendingUploads.push_back({
            .mStagingBuffer = aStagingBuffer,
            .mTexture = theTexture,
            .mDestinationOrigin = MTLOriginMake(
                static_cast<NSUInteger>(
                    theUpdate.mDestination.mOrigin.mX),
                static_cast<NSUInteger>(
                    theUpdate.mDestination.mOrigin.mY),
                0),
            .mSize = MTLSizeMake(
                static_cast<NSUInteger>(
                    theUpdate.mDestination.mSize.mWidth),
                static_cast<NSUInteger>(
                    theUpdate.mDestination.mSize.mHeight),
                1),
            .mBytesPerRow =
                static_cast<NSUInteger>(aStagingRowBytes),
            .mBytesPerImage = static_cast<NSUInteger>(aStagingSize),
        });
        return true;
    }

    [[nodiscard]] bool BuildSpriteGeometry(
        std::vector<SpriteVertex>& theVertices,
        std::vector<SpriteBatch>& theBatches)
    {
        const auto theDraws = mFrame.GetDraws();
        if (theDraws.size() >
            std::numeric_limits<std::size_t>::max() / 6)
        {
            mLastError = "sprite draw count overflows";
            return false;
        }
        theVertices.reserve(theDraws.size() * 6);
        theBatches.reserve(theDraws.size());

        for (const auto& aDraw : theDraws)
        {
            const auto* aTextureSlot = FindTexture(aDraw.mImage);
            const bool usesDestinationRect =
                aDraw.mGeometryMode ==
                engine::SpriteGeometryMode::DestinationRect;
            if (aTextureSlot == nullptr ||
                aDraw.mSource.mSize.mWidth == 0 ||
                aDraw.mSource.mSize.mHeight == 0 ||
                (usesDestinationRect &&
                 (aDraw.mDestination.mSize.mWidth <= 0.0F ||
                  aDraw.mDestination.mSize.mHeight <= 0.0F)))
            {
                continue;
            }

            bool anEmptyScissor{};
            const auto aScissor = MakeScissor(aDraw, anEmptyScissor);
            if (anEmptyScissor)
                continue;

            const float aTextureWidth =
                static_cast<float>(aTextureSlot->mSize.mWidth);
            const float aTextureHeight =
                static_cast<float>(aTextureSlot->mSize.mHeight);
            float aLeftTexture =
                static_cast<float>(aDraw.mSource.mOrigin.mX) /
                aTextureWidth;
            float aTopTexture =
                static_cast<float>(aDraw.mSource.mOrigin.mY) /
                aTextureHeight;
            float aRightTexture =
                static_cast<float>(aDraw.mSource.mOrigin.mX) /
                    aTextureWidth +
                static_cast<float>(aDraw.mSource.mSize.mWidth) /
                    aTextureWidth;
            float aBottomTexture =
                static_cast<float>(aDraw.mSource.mOrigin.mY) /
                    aTextureHeight +
                static_cast<float>(aDraw.mSource.mSize.mHeight) /
                    aTextureHeight;

            if (aDraw.mMirrorMode == engine::MirrorMode::Horizontal ||
                aDraw.mMirrorMode == engine::MirrorMode::Both)
            {
                std::swap(aLeftTexture, aRightTexture);
            }
            if (aDraw.mMirrorMode == engine::MirrorMode::Vertical ||
                aDraw.mMirrorMode == engine::MirrorMode::Both)
            {
                std::swap(aTopTexture, aBottomTexture);
            }

            const float aLeft = aDraw.mDestination.mOrigin.mX;
            const float aTop = aDraw.mDestination.mOrigin.mY;
            const float aRight =
                aLeft + aDraw.mDestination.mSize.mWidth;
            const float aBottom =
                aTop + aDraw.mDestination.mSize.mHeight;
            const float aPivotX =
                aLeft + aDraw.mRotationCenter.mX;
            const float aPivotY =
                aTop + aDraw.mRotationCenter.mY;
            const float aCosine = std::cos(aDraw.mRotationRadians);
            const float aSine = std::sin(aDraw.mRotationRadians);

            std::array<simd_float2, 4> aPositions;
            if (usesDestinationRect)
            {
                aPositions = {
                    RotatePoint(
                        aLeft,
                        aTop,
                        aPivotX,
                        aPivotY,
                        aCosine,
                        aSine),
                    RotatePoint(
                        aRight,
                        aTop,
                        aPivotX,
                        aPivotY,
                        aCosine,
                        aSine),
                    RotatePoint(
                        aLeft,
                        aBottom,
                        aPivotX,
                        aPivotY,
                        aCosine,
                        aSine),
                    RotatePoint(
                        aRight,
                        aBottom,
                        aPivotX,
                        aPivotY,
                        aCosine,
                        aSine),
                };
            }
            else
            {
                aPositions = {
                    simd_make_float2(
                        aDraw.mDestinationQuad.mTopLeft.mX,
                        aDraw.mDestinationQuad.mTopLeft.mY),
                    simd_make_float2(
                        aDraw.mDestinationQuad.mTopRight.mX,
                        aDraw.mDestinationQuad.mTopRight.mY),
                    simd_make_float2(
                        aDraw.mDestinationQuad.mBottomLeft.mX,
                        aDraw.mDestinationQuad.mBottomLeft.mY),
                    simd_make_float2(
                        aDraw.mDestinationQuad.mBottomRight.mX,
                        aDraw.mDestinationQuad.mBottomRight.mY),
                };
            }
            const bool hasInvalidPosition = std::any_of(
                aPositions.begin(),
                aPositions.end(),
                [](const simd_float2& thePosition)
                {
                    return !std::isfinite(thePosition.x) ||
                           !std::isfinite(thePosition.y);
                });
            if (hasInvalidPosition)
                continue;
            const std::array<simd_float2, 4> aTextureCoordinates{
                simd_make_float2(aLeftTexture, aTopTexture),
                simd_make_float2(aRightTexture, aTopTexture),
                simd_make_float2(aLeftTexture, aBottomTexture),
                simd_make_float2(aRightTexture, aBottomTexture),
            };
            const simd_float4 aColor = simd_make_float4(
                static_cast<float>(aDraw.mColor.mRed) / 255.0F,
                static_cast<float>(aDraw.mColor.mGreen) / 255.0F,
                static_cast<float>(aDraw.mColor.mBlue) / 255.0F,
                static_cast<float>(aDraw.mColor.mAlpha) / 255.0F);
            constexpr std::array<std::size_t, 6> kIndices{
                0,
                1,
                2,
                1,
                3,
                2,
            };
            const auto aFirstVertex =
                static_cast<NSUInteger>(theVertices.size());
            for (const auto anIndex : kIndices)
            {
                theVertices.push_back({
                    .mPosition = aPositions[anIndex],
                    .mTextureCoordinate =
                        aTextureCoordinates[anIndex],
                    .mColor = aColor,
                });
            }

            const bool aMatchesPrevious =
                !theBatches.empty() &&
                theBatches.back().mTexture ==
                    aTextureSlot->mTexture &&
                theBatches.back().mBlendMode ==
                    aDraw.mBlendMode &&
                theBatches.back().mFilterMode ==
                    aDraw.mFilterMode &&
                theBatches.back().mAddressMode ==
                    aDraw.mAddressMode &&
                ScissorsEqual(
                    theBatches.back().mScissor,
                    aScissor);
            if (aMatchesPrevious)
            {
                theBatches.back().mVertexCount += 6;
            }
            else
            {
                theBatches.push_back({
                    .mTexture = aTextureSlot->mTexture,
                    .mBlendMode = aDraw.mBlendMode,
                    .mFilterMode = aDraw.mFilterMode,
                    .mAddressMode = aDraw.mAddressMode,
                    .mScissor = aScissor,
                    .mFirstVertex = aFirstVertex,
                    .mVertexCount = 6,
                });
            }
        }
        return true;
    }
};

MetalRenderDevice::MetalRenderDevice()
    : mImplementation(std::make_unique<Implementation>())
{
}

MetalRenderDevice::~MetalRenderDevice() = default;

bool MetalRenderDevice::Initialize(void* theMetalLayer)
{
    if (theMetalLayer == nullptr)
    {
        mImplementation->mLastError =
            "SDL did not provide a Metal layer";
        return false;
    }

    mImplementation->mDevice = MTLCreateSystemDefaultDevice();
    if (mImplementation->mDevice == nil)
    {
        mImplementation->mLastError =
            "Metal is unavailable on this Mac";
        return false;
    }

    mImplementation->mCommandQueue =
        [mImplementation->mDevice newCommandQueue];
    if (mImplementation->mCommandQueue == nil)
    {
        mImplementation->mLastError =
            "could not create the Metal command queue";
        return false;
    }

    mImplementation->mLayer =
        (__bridge CAMetalLayer*)theMetalLayer;
    mImplementation->mLayer.device = mImplementation->mDevice;
    mImplementation->mLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    mImplementation->mLayer.framebufferOnly = YES;
    mImplementation->mLayer.presentsWithTransaction = NO;

    MTLTextureDescriptor* aLogicalTargetDescriptor =
        [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:
                MTLPixelFormatBGRA8Unorm
                                     width:kLogicalWidth
                                    height:kLogicalHeight
                                 mipmapped:NO];
    aLogicalTargetDescriptor.storageMode = MTLStorageModePrivate;
    aLogicalTargetDescriptor.usage =
        MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    mImplementation->mLogicalTarget =
        [mImplementation->mDevice
            newTextureWithDescriptor:aLogicalTargetDescriptor];
    if (mImplementation->mLogicalTarget == nil)
    {
        mImplementation->mLastError =
            "could not create the 800x600 Metal render target";
        return false;
    }

    NSURL* aLibraryUrl =
        [[NSBundle mainBundle]
            URLForResource:@"PvzShaders"
             withExtension:@"metallib"];
    if (aLibraryUrl == nil)
    {
        mImplementation->mLastError =
            "PvzShaders.metallib is missing from the app bundle";
        return false;
    }

    NSError* anError = nil;
    id<MTLLibrary> aLibrary =
        [mImplementation->mDevice
            newLibraryWithURL:aLibraryUrl
                        error:&anError];
    if (aLibrary == nil)
    {
        mImplementation->mLastError = DescribeError(
            @"could not load the Metal shader library",
            anError);
        return false;
    }

    id<MTLFunction> aSpriteVertex =
        [aLibrary newFunctionWithName:@"pvz_sprite_vertex"];
    id<MTLFunction> aSpriteFragment =
        [aLibrary newFunctionWithName:@"pvz_sprite_fragment"];
    id<MTLFunction> aPresentationVertex =
        [aLibrary newFunctionWithName:@"pvz_present_vertex"];
    id<MTLFunction> aPresentationFragment =
        [aLibrary newFunctionWithName:@"pvz_present_fragment"];
    if (aSpriteVertex == nil ||
        aSpriteFragment == nil ||
        aPresentationVertex == nil ||
        aPresentationFragment == nil)
    {
        mImplementation->mLastError =
            "the Metal shader library is incomplete";
        return false;
    }

    const auto aCreateSpritePipeline =
        [&](engine::BlendMode theBlendMode)
        -> id<MTLRenderPipelineState>
    {
        MTLRenderPipelineDescriptor* aDescriptor =
            [[MTLRenderPipelineDescriptor alloc] init];
        aDescriptor.label =
            theBlendMode == engine::BlendMode::Additive
                ? @"PVZ additive sprites"
                : @"PVZ normal sprites";
        aDescriptor.vertexFunction = aSpriteVertex;
        aDescriptor.fragmentFunction = aSpriteFragment;
        auto* anAttachment = aDescriptor.colorAttachments[0];
        anAttachment.pixelFormat = MTLPixelFormatBGRA8Unorm;
        anAttachment.blendingEnabled = YES;
        anAttachment.rgbBlendOperation = MTLBlendOperationAdd;
        anAttachment.alphaBlendOperation = MTLBlendOperationAdd;
        anAttachment.sourceRGBBlendFactor =
            MTLBlendFactorSourceAlpha;
        anAttachment.sourceAlphaBlendFactor =
            MTLBlendFactorSourceAlpha;
        anAttachment.destinationRGBBlendFactor =
            theBlendMode == engine::BlendMode::Additive
                ? MTLBlendFactorOne
                : MTLBlendFactorOneMinusSourceAlpha;
        anAttachment.destinationAlphaBlendFactor =
            theBlendMode == engine::BlendMode::Additive
                ? MTLBlendFactorOne
                : MTLBlendFactorOneMinusSourceAlpha;

        NSError* aPipelineError = nil;
        id<MTLRenderPipelineState> aPipeline =
            [mImplementation->mDevice
                newRenderPipelineStateWithDescriptor:aDescriptor
                                               error:&aPipelineError];
        if (aPipeline == nil)
        {
            mImplementation->mLastError = DescribeError(
                @"could not create a Metal sprite pipeline",
                aPipelineError);
        }
        return aPipeline;
    };

    mImplementation->mNormalSpritePipeline =
        aCreateSpritePipeline(engine::BlendMode::Normal);
    mImplementation->mAdditiveSpritePipeline =
        aCreateSpritePipeline(engine::BlendMode::Additive);
    if (mImplementation->mNormalSpritePipeline == nil ||
        mImplementation->mAdditiveSpritePipeline == nil)
    {
        return false;
    }

    MTLRenderPipelineDescriptor* aPresentationDescriptor =
        [[MTLRenderPipelineDescriptor alloc] init];
    aPresentationDescriptor.label = @"PVZ logical target presentation";
    aPresentationDescriptor.vertexFunction = aPresentationVertex;
    aPresentationDescriptor.fragmentFunction = aPresentationFragment;
    aPresentationDescriptor.colorAttachments[0].pixelFormat =
        MTLPixelFormatBGRA8Unorm;
    anError = nil;
    mImplementation->mPresentationPipeline =
        [mImplementation->mDevice
            newRenderPipelineStateWithDescriptor:
                aPresentationDescriptor
                                           error:&anError];
    if (mImplementation->mPresentationPipeline == nil)
    {
        mImplementation->mLastError = DescribeError(
            @"could not create the Metal presentation pipeline",
            anError);
        return false;
    }

    const auto aCreateSampler =
        [&](engine::FilterMode theFilterMode,
            engine::AddressMode theAddressMode)
        -> id<MTLSamplerState>
    {
        MTLSamplerDescriptor* aDescriptor =
            [[MTLSamplerDescriptor alloc] init];
        const auto aFilter =
            theFilterMode == engine::FilterMode::Linear
                ? MTLSamplerMinMagFilterLinear
                : MTLSamplerMinMagFilterNearest;
        const auto anAddressMode =
            theAddressMode == engine::AddressMode::Repeat
                ? MTLSamplerAddressModeRepeat
                : MTLSamplerAddressModeClampToEdge;
        aDescriptor.minFilter = aFilter;
        aDescriptor.magFilter = aFilter;
        aDescriptor.mipFilter = MTLSamplerMipFilterNotMipmapped;
        aDescriptor.sAddressMode = anAddressMode;
        aDescriptor.tAddressMode = anAddressMode;
        return [mImplementation->mDevice
            newSamplerStateWithDescriptor:aDescriptor];
    };

    mImplementation->mNearestClampSampler = aCreateSampler(
        engine::FilterMode::Nearest,
        engine::AddressMode::Clamp);
    mImplementation->mNearestRepeatSampler = aCreateSampler(
        engine::FilterMode::Nearest,
        engine::AddressMode::Repeat);
    mImplementation->mLinearClampSampler = aCreateSampler(
        engine::FilterMode::Linear,
        engine::AddressMode::Clamp);
    mImplementation->mLinearRepeatSampler = aCreateSampler(
        engine::FilterMode::Linear,
        engine::AddressMode::Repeat);
    if (mImplementation->mNearestClampSampler == nil ||
        mImplementation->mNearestRepeatSampler == nil ||
        mImplementation->mLinearClampSampler == nil ||
        mImplementation->mLinearRepeatSampler == nil)
    {
        mImplementation->mLastError =
            "could not create the Metal sprite samplers";
        return false;
    }

    mImplementation->mLastError.clear();
    return true;
}

std::string_view MetalRenderDevice::GetLastError() const
{
    return mImplementation->mLastError;
}

engine::RenderFrameResult MetalRenderDevice::BeginFrame(
    engine::IRenderFrame*& theFrame)
{
    theFrame = nullptr;
    if (mImplementation->mLayer == nil ||
        mImplementation->mCommandQueue == nil ||
        mImplementation->mFrameActive)
    {
        mImplementation->mLastError =
            "Metal frame lifecycle is invalid";
        return engine::RenderFrameResult::Failure;
    }

    @autoreleasepool
    {
        mImplementation->mDrawable =
            [mImplementation->mLayer nextDrawable];
    }
    if (mImplementation->mDrawable == nil)
        return engine::RenderFrameResult::Unavailable;

    mImplementation->mFrame.Reset();
    mImplementation->mFrameActive = true;
    mImplementation->mLastError.clear();
    theFrame = &mImplementation->mFrame;
    return engine::RenderFrameResult::Ready;
}

bool MetalRenderDevice::EndFrame()
{
    if (!mImplementation->mFrameActive ||
        mImplementation->mDrawable == nil)
    {
        mImplementation->mLastError =
            "Metal frame ended without a drawable";
        return false;
    }

    const auto aFailFrame = [&](std::string theError)
    {
        mImplementation->mLastError = std::move(theError);
        mImplementation->mDrawable = nil;
        mImplementation->mFrameActive = false;
        return false;
    };

    std::vector<SpriteVertex> aVertices;
    std::vector<SpriteBatch> aBatches;
    if (!mImplementation->BuildSpriteGeometry(
            aVertices,
            aBatches))
    {
        return aFailFrame(mImplementation->mLastError);
    }

    id<MTLCommandBuffer> aCommandBuffer =
        [mImplementation->mCommandQueue commandBuffer];
    if (aCommandBuffer == nil)
        return aFailFrame("could not create a Metal command buffer");
    aCommandBuffer.label = @"PVZ frame";

    if (!mImplementation->mPendingUploads.empty())
    {
        id<MTLBlitCommandEncoder> aBlitEncoder =
            [aCommandBuffer blitCommandEncoder];
        if (aBlitEncoder == nil)
            return aFailFrame("could not create a Metal blit encoder");
        aBlitEncoder.label = @"PVZ image uploads";
        for (const auto& anUpload :
             mImplementation->mPendingUploads)
        {
            [aBlitEncoder
                copyFromBuffer:anUpload.mStagingBuffer
                  sourceOffset:0
             sourceBytesPerRow:anUpload.mBytesPerRow
           sourceBytesPerImage:anUpload.mBytesPerImage
                    sourceSize:anUpload.mSize
                     toTexture:anUpload.mTexture
              destinationSlice:0
              destinationLevel:0
             destinationOrigin:anUpload.mDestinationOrigin];
        }
        [aBlitEncoder endEncoding];
    }

    id<MTLBuffer> aVertexBuffer = nil;
    bool aVertexSlotAcquired = false;
    if (!aVertices.empty())
    {
        dispatch_semaphore_wait(
            mImplementation->mVertexBufferSemaphore,
            DISPATCH_TIME_FOREVER);
        aVertexSlotAcquired = true;

        const auto aVertexBufferIndex =
            mImplementation->mNextVertexBuffer;
        mImplementation->mNextVertexBuffer =
            (aVertexBufferIndex + 1) % kVertexBufferCount;
        std::size_t aRequiredBytes{};
        if (!TryMultiply(
                aVertices.size(),
                sizeof(SpriteVertex),
                aRequiredBytes))
        {
            dispatch_semaphore_signal(
                mImplementation->mVertexBufferSemaphore);
            return aFailFrame("sprite vertex size overflows");
        }

        if (mImplementation
                ->mVertexBufferCapacities[aVertexBufferIndex] <
            aRequiredBytes)
        {
            std::size_t aCapacity = 64 * 1'024;
            while (aCapacity < aRequiredBytes)
            {
                if (aCapacity >
                    std::numeric_limits<std::size_t>::max() / 2)
                {
                    aCapacity = aRequiredBytes;
                    break;
                }
                aCapacity *= 2;
            }
            mImplementation->mVertexBuffers[aVertexBufferIndex] =
                [mImplementation->mDevice
                    newBufferWithLength:aCapacity
                                 options:MTLResourceStorageModeShared];
            if (mImplementation
                    ->mVertexBuffers[aVertexBufferIndex] == nil)
            {
                dispatch_semaphore_signal(
                    mImplementation->mVertexBufferSemaphore);
                return aFailFrame(
                    "could not allocate a Metal vertex buffer");
            }
            mImplementation
                ->mVertexBufferCapacities[aVertexBufferIndex] =
                aCapacity;
        }
        aVertexBuffer =
            mImplementation->mVertexBuffers[aVertexBufferIndex];
        std::memcpy(
            aVertexBuffer.contents,
            aVertices.data(),
            aRequiredBytes);
    }

    const auto aClearColor =
        mImplementation->mFrame.GetClearColor();
    MTLRenderPassDescriptor* aLogicalPass =
        [MTLRenderPassDescriptor renderPassDescriptor];
    aLogicalPass.colorAttachments[0].texture =
        mImplementation->mLogicalTarget;
    aLogicalPass.colorAttachments[0].loadAction = MTLLoadActionClear;
    aLogicalPass.colorAttachments[0].storeAction = MTLStoreActionStore;
    aLogicalPass.colorAttachments[0].clearColor = MTLClearColorMake(
        static_cast<double>(aClearColor.mRed) / 255.0,
        static_cast<double>(aClearColor.mGreen) / 255.0,
        static_cast<double>(aClearColor.mBlue) / 255.0,
        static_cast<double>(aClearColor.mAlpha) / 255.0);

    id<MTLRenderCommandEncoder> aLogicalEncoder =
        [aCommandBuffer
            renderCommandEncoderWithDescriptor:aLogicalPass];
    if (aLogicalEncoder == nil)
    {
        if (aVertexSlotAcquired)
        {
            dispatch_semaphore_signal(
                mImplementation->mVertexBufferSemaphore);
        }
        return aFailFrame(
            "could not create the logical Metal render encoder");
    }
    aLogicalEncoder.label = @"PVZ 800x600 logical pass";

    if (!aBatches.empty())
    {
        const simd_float2 aLogicalSize = simd_make_float2(
            static_cast<float>(kLogicalWidth),
            static_cast<float>(kLogicalHeight));
        [aLogicalEncoder setVertexBuffer:aVertexBuffer
                                  offset:0
                                 atIndex:0];
        [aLogicalEncoder setVertexBytes:&aLogicalSize
                                length:sizeof(aLogicalSize)
                               atIndex:1];
        for (const auto& aBatch : aBatches)
        {
            [aLogicalEncoder
                setRenderPipelineState:
                    aBatch.mBlendMode ==
                            engine::BlendMode::Additive
                        ? mImplementation
                              ->mAdditiveSpritePipeline
                        : mImplementation
                              ->mNormalSpritePipeline];
            [aLogicalEncoder setFragmentTexture:aBatch.mTexture
                                        atIndex:0];
            [aLogicalEncoder
                setFragmentSamplerState:
                    mImplementation->GetSampler(
                        aBatch.mFilterMode,
                        aBatch.mAddressMode)
                                    atIndex:0];
            [aLogicalEncoder setScissorRect:aBatch.mScissor];
            [aLogicalEncoder
                drawPrimitives:MTLPrimitiveTypeTriangle
                  vertexStart:aBatch.mFirstVertex
                  vertexCount:aBatch.mVertexCount];
        }
    }
    [aLogicalEncoder endEncoding];

    MTLRenderPassDescriptor* aPresentationPass =
        [MTLRenderPassDescriptor renderPassDescriptor];
    aPresentationPass.colorAttachments[0].texture =
        mImplementation->mDrawable.texture;
    aPresentationPass.colorAttachments[0].loadAction =
        MTLLoadActionClear;
    aPresentationPass.colorAttachments[0].storeAction =
        MTLStoreActionStore;
    aPresentationPass.colorAttachments[0].clearColor =
        MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

    id<MTLRenderCommandEncoder> aPresentationEncoder =
        [aCommandBuffer
            renderCommandEncoderWithDescriptor:aPresentationPass];
    if (aPresentationEncoder == nil)
    {
        if (aVertexSlotAcquired)
        {
            dispatch_semaphore_signal(
                mImplementation->mVertexBufferSemaphore);
        }
        return aFailFrame(
            "could not create the presentation Metal render encoder");
    }
    aPresentationEncoder.label = @"PVZ aspect-fit presentation";

    const double aDrawableWidth = static_cast<double>(
        mImplementation->mDrawable.texture.width);
    const double aDrawableHeight = static_cast<double>(
        mImplementation->mDrawable.texture.height);
    const double aScale = std::min(
        aDrawableWidth / static_cast<double>(kLogicalWidth),
        aDrawableHeight / static_cast<double>(kLogicalHeight));
    const double aPresentationWidth =
        static_cast<double>(kLogicalWidth) * aScale;
    const double aPresentationHeight =
        static_cast<double>(kLogicalHeight) * aScale;
    const MTLViewport aViewport{
        .originX =
            (aDrawableWidth - aPresentationWidth) * 0.5,
        .originY =
            (aDrawableHeight - aPresentationHeight) * 0.5,
        .width = aPresentationWidth,
        .height = aPresentationHeight,
        .znear = 0.0,
        .zfar = 1.0,
    };
    [aPresentationEncoder setViewport:aViewport];
    [aPresentationEncoder
        setRenderPipelineState:
            mImplementation->mPresentationPipeline];
    [aPresentationEncoder
        setFragmentTexture:mImplementation->mLogicalTarget
                   atIndex:0];
    [aPresentationEncoder
        setFragmentSamplerState:
            mImplementation->mLinearClampSampler
                       atIndex:0];
    [aPresentationEncoder
        drawPrimitives:MTLPrimitiveTypeTriangleStrip
          vertexStart:0
          vertexCount:4];
    [aPresentationEncoder endEncoding];

    if (aVertexSlotAcquired)
    {
        dispatch_semaphore_t aSemaphore =
            mImplementation->mVertexBufferSemaphore;
        [aCommandBuffer
            addCompletedHandler:^(id<MTLCommandBuffer>)
            {
                dispatch_semaphore_signal(aSemaphore);
            }];
    }
    [aCommandBuffer
        presentDrawable:mImplementation->mDrawable];
    [aCommandBuffer commit];

    mImplementation->mPendingUploads.clear();
    mImplementation->mDrawable = nil;
    mImplementation->mFrameActive = false;
    return true;
}

bool MetalRenderDevice::CreateImage(
    const engine::ImageDescriptor& theDescriptor,
    std::span<const std::byte> theInitialPixels,
    std::uint32_t theSourceBytesPerRow,
    engine::ImageHandle& theImage)
{
    theImage = {};
    if (mImplementation->mDevice == nil ||
        theDescriptor.mSize.mWidth == 0 ||
        theDescriptor.mSize.mHeight == 0 ||
        theDescriptor.mPixelFormat !=
            engine::ImagePixelFormat::Bgra8Unorm)
    {
        mImplementation->mLastError =
            "image descriptor is not supported";
        return false;
    }

    MTLTextureDescriptor* aDescriptor =
        [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:
                MTLPixelFormatBGRA8Unorm
                                     width:theDescriptor.mSize.mWidth
                                    height:theDescriptor.mSize.mHeight
                                 mipmapped:NO];
    aDescriptor.storageMode = MTLStorageModePrivate;
    aDescriptor.usage = MTLTextureUsageShaderRead;
    id<MTLTexture> aTexture =
        [mImplementation->mDevice
            newTextureWithDescriptor:aDescriptor];
    if (aTexture == nil)
    {
        mImplementation->mLastError =
            "could not allocate a private Metal texture";
        return false;
    }

    const engine::ImageUpdate anInitialUpdate{
        .mDestination =
            {
                .mOrigin = {0, 0},
                .mSize = theDescriptor.mSize,
            },
        .mSourceBytesPerRow = theSourceBytesPerRow,
        .mPixels = theInitialPixels,
    };
    if (!mImplementation->QueueUpload(
            aTexture,
            theDescriptor.mSize,
            anInitialUpdate))
    {
        return false;
    }

    std::uint32_t anIndex{};
    if (!mImplementation->mFreeTextureSlots.empty())
    {
        anIndex = mImplementation->mFreeTextureSlots.back();
        mImplementation->mFreeTextureSlots.pop_back();
    }
    else
    {
        if (mImplementation->mTextureSlots.size() >=
            std::numeric_limits<std::uint32_t>::max())
        {
            mImplementation->mPendingUploads.pop_back();
            mImplementation->mLastError =
                "Metal texture handle space is exhausted";
            return false;
        }
        anIndex = static_cast<std::uint32_t>(
            mImplementation->mTextureSlots.size());
        mImplementation->mTextureSlots.emplace_back();
    }

    auto& aSlot = mImplementation->mTextureSlots[anIndex];
    aSlot.mTexture = aTexture;
    aSlot.mSize = theDescriptor.mSize;
    theImage = {
        .mIndex = anIndex,
        .mGeneration = aSlot.mGeneration,
    };
    mImplementation->mLastError.clear();
    return true;
}

bool MetalRenderDevice::UpdateImage(
    engine::ImageHandle theImage,
    const engine::ImageUpdate& theUpdate)
{
    auto* aSlot = mImplementation->FindTexture(theImage);
    if (aSlot == nullptr)
    {
        mImplementation->mLastError =
            "image update uses a stale texture handle";
        return false;
    }
    if (!mImplementation->QueueUpload(
            aSlot->mTexture,
            aSlot->mSize,
            theUpdate))
    {
        return false;
    }
    mImplementation->mLastError.clear();
    return true;
}

void MetalRenderDevice::DestroyImage(engine::ImageHandle theImage)
{
    auto* aSlot = mImplementation->FindTexture(theImage);
    if (aSlot == nullptr)
        return;

    aSlot->mTexture = nil;
    aSlot->mSize = {};
    aSlot->mGeneration =
        NextGeneration(aSlot->mGeneration);
    mImplementation->mFreeTextureSlots.push_back(theImage.mIndex);
}

bool MetalRenderDevice::GetImageSize(
    engine::ImageHandle theImage,
    engine::SizeI& theSize) const
{
    const auto* aSlot =
        mImplementation->FindTexture(theImage);
    if (aSlot == nullptr)
    {
        theSize = {};
        return false;
    }
    theSize = aSlot->mSize;
    return true;
}

} // namespace pvz::platform::macos
