#include "pvz/platform/macos/MetalRenderDevice.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace pvz::platform::macos
{
namespace
{

class MetalRenderFrame final : public engine::IRenderFrame
{
public:
    void Reset()
    {
        mClearColor = {0, 0, 0, 255};
        mHasUnsupportedDraws = false;
    }

    [[nodiscard]] engine::SizeI GetLogicalSize() const override
    {
        return {800, 600};
    }

    void Clear(engine::ColorRgba8 theColor) override
    {
        mClearColor = theColor;
    }

    void SubmitSprites(
        std::span<const engine::SpriteDraw> theDraws) override
    {
        if (!theDraws.empty())
            mHasUnsupportedDraws = true;
    }

    [[nodiscard]] engine::ColorRgba8 GetClearColor() const
    {
        return mClearColor;
    }

    [[nodiscard]] bool HasUnsupportedDraws() const
    {
        return mHasUnsupportedDraws;
    }

private:
    engine::ColorRgba8 mClearColor{0, 0, 0, 255};
    bool mHasUnsupportedDraws{};
};

} // namespace

struct MetalRenderDevice::Implementation
{
    id<MTLDevice> mDevice;
    id<MTLCommandQueue> mCommandQueue;
    CAMetalLayer* mLayer;
    id<CAMetalDrawable> mDrawable;
    MetalRenderFrame mFrame;
    std::string mLastError;
    bool mFrameActive{};
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
        mImplementation->mLastError = "SDL did not provide a Metal layer";
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

    if (mImplementation->mFrame.HasUnsupportedDraws())
    {
        mImplementation->mLastError =
            "Metal sprite pipelines are not implemented yet";
        mImplementation->mDrawable = nil;
        mImplementation->mFrameActive = false;
        return false;
    }

    const auto aColor = mImplementation->mFrame.GetClearColor();
    MTLRenderPassDescriptor* aPass =
        [MTLRenderPassDescriptor renderPassDescriptor];
    aPass.colorAttachments[0].texture =
        mImplementation->mDrawable.texture;
    aPass.colorAttachments[0].loadAction = MTLLoadActionClear;
    aPass.colorAttachments[0].storeAction = MTLStoreActionStore;
    aPass.colorAttachments[0].clearColor = MTLClearColorMake(
        static_cast<double>(aColor.mRed) / 255.0,
        static_cast<double>(aColor.mGreen) / 255.0,
        static_cast<double>(aColor.mBlue) / 255.0,
        static_cast<double>(aColor.mAlpha) / 255.0);

    id<MTLCommandBuffer> aCommandBuffer =
        [mImplementation->mCommandQueue commandBuffer];
    if (aCommandBuffer == nil)
    {
        mImplementation->mLastError =
            "could not create a Metal command buffer";
        mImplementation->mDrawable = nil;
        mImplementation->mFrameActive = false;
        return false;
    }

    id<MTLRenderCommandEncoder> anEncoder =
        [aCommandBuffer renderCommandEncoderWithDescriptor:aPass];
    if (anEncoder == nil)
    {
        mImplementation->mLastError =
            "could not create a Metal render encoder";
        mImplementation->mDrawable = nil;
        mImplementation->mFrameActive = false;
        return false;
    }

    [anEncoder endEncoding];
    [aCommandBuffer presentDrawable:mImplementation->mDrawable];
    [aCommandBuffer commit];
    mImplementation->mDrawable = nil;
    mImplementation->mFrameActive = false;
    return true;
}

} // namespace pvz::platform::macos
