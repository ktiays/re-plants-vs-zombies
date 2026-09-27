#include "MetalRenderDevice.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <Foundation/Foundation.h>
#include <simd/simd.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#if !__has_feature(objc_arc)
#error "MetalRenderDevice.mm must be compiled with ARC"
#endif

using namespace Sexy;

namespace
{

// Shaders are compiled at startup so the build does not depend on the
// offline Metal toolchain.
const char* kShaderSource = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct SceneVertex
{
	float2 position [[attribute(0)]];
	float4 color    [[attribute(1)]];
	float2 uv       [[attribute(2)]];
};

struct SceneFragmentIn
{
	float4 position [[position]];
	float4 color;
	float2 uv;
};

// Positions arrive in logical pixels with Direct3D 7 pixel centers (the
// center of pixel x is at x); Metal pixel centers are at x + 0.5.
vertex SceneFragmentIn scene_vertex(SceneVertex in [[stage_in]], constant float2& theSceneSize [[buffer(1)]])
{
	SceneFragmentIn out;
	float2 p = (in.position + 0.5) / theSceneSize;
	out.position = float4(p.x * 2.0 - 1.0, 1.0 - p.y * 2.0, 0.0, 1.0);
	out.color = in.color;
	out.uv = in.uv;
	return out;
}

fragment float4 scene_fragment(SceneFragmentIn in [[stage_in]], texture2d<float> theTexture [[texture(0)]], sampler theSampler [[sampler(0)]])
{
	return theTexture.sample(theSampler, in.uv) * in.color;
}

struct PresentFragmentIn
{
	float4 position [[position]];
	float2 uv;
};

vertex PresentFragmentIn present_vertex(uint theVertexId [[vertex_id]])
{
	const float2 aCorners[4] = { float2(0, 0), float2(0, 1), float2(1, 0), float2(1, 1) };
	PresentFragmentIn out;
	float2 c = aCorners[theVertexId];
	out.position = float4(c.x * 2.0 - 1.0, 1.0 - c.y * 2.0, 0.0, 1.0);
	out.uv = c;
	return out;
}

fragment float4 present_fragment(PresentFragmentIn in [[stage_in]], texture2d<float> theScene [[texture(0)]], sampler theSampler [[sampler(0)]])
{
	return float4(theScene.sample(theSampler, in.uv).rgb, 1.0);
}
)METAL";

constexpr int kMaxFramesInFlight = 3;
constexpr NSUInteger kVertexChunkSize = 1024 * 1024;

struct TextureEntry
{
	id<MTLTexture>		mTexture;
	int					mWidth = 0;
	int					mHeight = 0;
	std::uint64_t		mLastUsedFrame = 0;
};

struct FrameResources
{
	std::vector<id<MTLBuffer>> mVertexChunks;
	size_t				mChunkIndex = 0;
	NSUInteger			mChunkOffset = 0;
};

class MetalRenderDevice : public RenderDevice
{
public:
	SDL_Window*						mWindow = nullptr;
	SDL_MetalView					mView = nullptr;
	CAMetalLayer*					mLayer = nil;
	id<MTLDevice>					mDevice = nil;
	id<MTLCommandQueue>				mQueue = nil;
	id<MTLRenderPipelineState>		mScenePipelines[3];
	id<MTLRenderPipelineState>		mPresentPipeline = nil;
	id<MTLSamplerState>				mSamplers[2][2];
	id<MTLTexture>					mWhiteTexture = nil;
	id<MTLTexture>					mScene = nil;
	int								mSceneWidth = 0;
	int								mSceneHeight = 0;
	bool							mSceneNeedsClear = true;
	MTLStorageMode					mTextureStorageMode = MTLStorageModeShared;

	std::mutex						mTextureMutex;
	std::unordered_map<RenderTextureId, TextureEntry> mTextures;
	RenderTextureId					mNextTextureId = 1;

	dispatch_semaphore_t			mFrameSemaphore;
	FrameResources					mFrames[kMaxFramesInFlight];
	std::uint64_t					mFrameSerial = 1;
	std::atomic<std::uint64_t>		mCompletedFrame{0};
	bool							mFrameBegun = false;
	id<MTLCommandBuffer>			mCommandBuffer = nil;
	id<MTLRenderCommandEncoder>		mEncoder = nil;

	// Current encoder state, to avoid redundant state changes.
	RenderBlendMode					mBoundBlend = RenderBlendMode::Normal;
	bool							mHasBoundBlend = false;

public:
	~MetalRenderDevice() override
	{
		// A begun frame holds a frames-in-flight slot; submit it so the slot
		// is released before waiting for the GPU to go idle.
		if (mFrameBegun)
			Present(RenderViewport(), false, false);

		// Wait for frames in flight.
		for (int i = 0; i < kMaxFramesInFlight; i++)
			dispatch_semaphore_wait(mFrameSemaphore, DISPATCH_TIME_FOREVER);
		for (int i = 0; i < kMaxFramesInFlight; i++)
			dispatch_semaphore_signal(mFrameSemaphore);

		mTextures.clear();
		mLayer = nil;
		if (mView != nullptr)
			SDL_Metal_DestroyView(mView);
	}

	bool Init(SDL_Window* theWindow)
	{
		mWindow = theWindow;
		mView = SDL_Metal_CreateView(theWindow);
		if (mView == nullptr)
			return false;

		mLayer = (__bridge CAMetalLayer*)SDL_Metal_GetLayer(mView);
		if (mLayer == nil)
			return false;

		mDevice = mLayer.device;
		if (mDevice == nil)
			mDevice = MTLCreateSystemDefaultDevice();
		if (mDevice == nil)
			return false;

		mLayer.device = mDevice;
		mLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
		mLayer.framebufferOnly = YES;
		CGColorSpaceRef aColorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
		mLayer.colorspace = aColorSpace;
		CGColorSpaceRelease(aColorSpace);
		mLayer.maximumDrawableCount = 3;

		mTextureStorageMode = mDevice.hasUnifiedMemory ? MTLStorageModeShared : MTLStorageModeManaged;

		mQueue = [mDevice newCommandQueue];
		mFrameSemaphore = dispatch_semaphore_create(kMaxFramesInFlight);

		NSError* anError = nil;
		id<MTLLibrary> aLibrary = [mDevice newLibraryWithSource:[NSString stringWithUTF8String:kShaderSource] options:nil error:&anError];
		if (aLibrary == nil)
		{
			std::fprintf(stderr, "Metal shader compilation failed: %s\n", anError.localizedDescription.UTF8String);
			return false;
		}

		MTLVertexDescriptor* aVertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
		aVertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
		aVertexDescriptor.attributes[0].offset = offsetof(RenderVertex, x);
		aVertexDescriptor.attributes[0].bufferIndex = 0;
		aVertexDescriptor.attributes[1].format = MTLVertexFormatUChar4Normalized_BGRA;
		aVertexDescriptor.attributes[1].offset = offsetof(RenderVertex, color);
		aVertexDescriptor.attributes[1].bufferIndex = 0;
		aVertexDescriptor.attributes[2].format = MTLVertexFormatFloat2;
		aVertexDescriptor.attributes[2].offset = offsetof(RenderVertex, u);
		aVertexDescriptor.attributes[2].bufferIndex = 0;
		aVertexDescriptor.layouts[0].stride = sizeof(RenderVertex);

		id<MTLFunction> aSceneVertex = [aLibrary newFunctionWithName:@"scene_vertex"];
		id<MTLFunction> aSceneFragment = [aLibrary newFunctionWithName:@"scene_fragment"];
		for (int aMode = 0; aMode < 3; aMode++)
		{
			MTLRenderPipelineDescriptor* aDesc = [[MTLRenderPipelineDescriptor alloc] init];
			aDesc.vertexFunction = aSceneVertex;
			aDesc.fragmentFunction = aSceneFragment;
			aDesc.vertexDescriptor = aVertexDescriptor;
			MTLRenderPipelineColorAttachmentDescriptor* aColor = aDesc.colorAttachments[0];
			aColor.pixelFormat = MTLPixelFormatBGRA8Unorm;

			switch ((RenderBlendMode)aMode)
			{
			case RenderBlendMode::Normal:
				aColor.blendingEnabled = YES;
				aColor.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
				aColor.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
				aColor.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
				aColor.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
				break;
			case RenderBlendMode::Additive:
				aColor.blendingEnabled = YES;
				aColor.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
				aColor.destinationRGBBlendFactor = MTLBlendFactorOne;
				aColor.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
				aColor.destinationAlphaBlendFactor = MTLBlendFactorOne;
				break;
			case RenderBlendMode::Copy:
				aColor.blendingEnabled = NO;
				break;
			}

			mScenePipelines[aMode] = [mDevice newRenderPipelineStateWithDescriptor:aDesc error:&anError];
			if (mScenePipelines[aMode] == nil)
			{
				std::fprintf(stderr, "Metal pipeline creation failed: %s\n", anError.localizedDescription.UTF8String);
				return false;
			}
		}

		MTLRenderPipelineDescriptor* aPresentDesc = [[MTLRenderPipelineDescriptor alloc] init];
		aPresentDesc.vertexFunction = [aLibrary newFunctionWithName:@"present_vertex"];
		aPresentDesc.fragmentFunction = [aLibrary newFunctionWithName:@"present_fragment"];
		aPresentDesc.colorAttachments[0].pixelFormat = mLayer.pixelFormat;
		mPresentPipeline = [mDevice newRenderPipelineStateWithDescriptor:aPresentDesc error:&anError];
		if (mPresentPipeline == nil)
		{
			std::fprintf(stderr, "Metal present pipeline creation failed: %s\n", anError.localizedDescription.UTF8String);
			return false;
		}

		for (int aFilter = 0; aFilter < 2; aFilter++)
		{
			for (int anAddress = 0; anAddress < 2; anAddress++)
			{
				MTLSamplerDescriptor* aDesc = [[MTLSamplerDescriptor alloc] init];
				aDesc.minFilter = aFilter ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
				aDesc.magFilter = aDesc.minFilter;
				aDesc.mipFilter = MTLSamplerMipFilterNotMipmapped;
				MTLSamplerAddressMode aMode = anAddress ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeClampToEdge;
				aDesc.sAddressMode = aMode;
				aDesc.tAddressMode = aMode;
				mSamplers[aFilter][anAddress] = [mDevice newSamplerStateWithDescriptor:aDesc];
			}
		}

		MTLTextureDescriptor* aWhiteDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:1 height:1 mipmapped:NO];
		aWhiteDesc.storageMode = mTextureStorageMode;
		mWhiteTexture = [mDevice newTextureWithDescriptor:aWhiteDesc];
		std::uint32_t aWhite = 0xFFFFFFFF;
		[mWhiteTexture replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&aWhite bytesPerRow:4];

		return true;
	}

	std::string GetName() const override
	{
		return std::string("Metal (") + mDevice.name.UTF8String + ")";
	}

	bool SetSceneSize(int theWidth, int theHeight) override
	{
		if (theWidth <= 0 || theHeight <= 0)
			return false;

		if (mScene != nil && theWidth == mSceneWidth && theHeight == mSceneHeight)
		{
			mSceneNeedsClear = true;
			return true;
		}

		MTLTextureDescriptor* aDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:theWidth height:theHeight mipmapped:NO];
		aDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
		aDesc.storageMode = MTLStorageModePrivate;
		mScene = [mDevice newTextureWithDescriptor:aDesc];
		mSceneWidth = theWidth;
		mSceneHeight = theHeight;
		mSceneNeedsClear = true;
		return mScene != nil;
	}

	id<MTLTexture> NewTexture(int theWidth, int theHeight)
	{
		MTLTextureDescriptor* aDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:theWidth height:theHeight mipmapped:NO];
		aDesc.usage = MTLTextureUsageShaderRead;
		aDesc.storageMode = mTextureStorageMode;
		return [mDevice newTextureWithDescriptor:aDesc];
	}

	RenderTextureId CreateTexture(int theWidth, int theHeight) override
	{
		if (theWidth <= 0 || theHeight <= 0 || theWidth > 16384 || theHeight > 16384)
			return 0;

		id<MTLTexture> aTexture = NewTexture(theWidth, theHeight);
		if (aTexture == nil)
			return 0;

		std::lock_guard<std::mutex> aLock(mTextureMutex);
		RenderTextureId anId = mNextTextureId++;
		if (mNextTextureId == 0)
			mNextTextureId = 1;
		TextureEntry& anEntry = mTextures[anId];
		anEntry.mTexture = aTexture;
		anEntry.mWidth = theWidth;
		anEntry.mHeight = theHeight;
		anEntry.mLastUsedFrame = 0;
		return anId;
	}

	void UpdateTexture(RenderTextureId theTexture, int theX, int theY, int theWidth, int theHeight, const std::uint32_t* thePixels, int thePitch) override
	{
		std::lock_guard<std::mutex> aLock(mTextureMutex);
		auto anItr = mTextures.find(theTexture);
		if (anItr == mTextures.end() || thePixels == nullptr)
			return;

		TextureEntry& anEntry = anItr->second;
		if (theWidth <= 0 || theHeight <= 0 || theX < 0 || theY < 0 || theX + theWidth > anEntry.mWidth || theY + theHeight > anEntry.mHeight)
			return;

		// A texture referenced by a frame the GPU has not finished must not be
		// written; give the id a fresh texture instead.
		if (anEntry.mLastUsedFrame != 0 && anEntry.mLastUsedFrame > mCompletedFrame.load())
		{
			id<MTLTexture> aNewTexture = NewTexture(anEntry.mWidth, anEntry.mHeight);
			if (aNewTexture == nil)
				return;

			bool aFullUpdate = theX == 0 && theY == 0 && theWidth == anEntry.mWidth && theHeight == anEntry.mHeight;
			if (!aFullUpdate)
			{
				std::vector<std::uint32_t> aPixels((size_t)anEntry.mWidth * anEntry.mHeight);
				[anEntry.mTexture getBytes:aPixels.data() bytesPerRow:anEntry.mWidth * 4 fromRegion:MTLRegionMake2D(0, 0, anEntry.mWidth, anEntry.mHeight) mipmapLevel:0];
				[aNewTexture replaceRegion:MTLRegionMake2D(0, 0, anEntry.mWidth, anEntry.mHeight) mipmapLevel:0 withBytes:aPixels.data() bytesPerRow:anEntry.mWidth * 4];
			}

			anEntry.mTexture = aNewTexture;
			anEntry.mLastUsedFrame = 0;
		}

		[anEntry.mTexture replaceRegion:MTLRegionMake2D(theX, theY, theWidth, theHeight) mipmapLevel:0 withBytes:thePixels bytesPerRow:(NSUInteger)thePitch * 4];
	}

	bool ReadTexture(RenderTextureId theTexture, std::uint32_t* thePixels) override
	{
		std::lock_guard<std::mutex> aLock(mTextureMutex);
		auto anItr = mTextures.find(theTexture);
		if (anItr == mTextures.end() || thePixels == nullptr)
			return false;

		TextureEntry& anEntry = anItr->second;
		[anEntry.mTexture getBytes:thePixels bytesPerRow:anEntry.mWidth * 4 fromRegion:MTLRegionMake2D(0, 0, anEntry.mWidth, anEntry.mHeight) mipmapLevel:0];
		return true;
	}

	void DestroyTexture(RenderTextureId theTexture) override
	{
		// Command buffers retain the textures they reference, so the Metal
		// object stays alive until in-flight frames are done with it.
		std::lock_guard<std::mutex> aLock(mTextureMutex);
		mTextures.erase(theTexture);
	}

	void BeginFrame()
	{
		if (mFrameBegun)
			return;

		dispatch_semaphore_wait(mFrameSemaphore, DISPATCH_TIME_FOREVER);
		mFrameBegun = true;

		FrameResources& aFrame = mFrames[mFrameSerial % kMaxFramesInFlight];
		aFrame.mChunkIndex = 0;
		aFrame.mChunkOffset = 0;

		mCommandBuffer = [mQueue commandBuffer];
	}

	bool BeginScene() override
	{
		if (mScene == nil)
			return false;

		BeginFrame();

		if (mEncoder != nil)
			return true;

		MTLRenderPassDescriptor* aPass = [MTLRenderPassDescriptor renderPassDescriptor];
		aPass.colorAttachments[0].texture = mScene;
		aPass.colorAttachments[0].loadAction = mSceneNeedsClear ? MTLLoadActionClear : MTLLoadActionLoad;
		aPass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
		aPass.colorAttachments[0].storeAction = MTLStoreActionStore;
		mSceneNeedsClear = false;

		mEncoder = [mCommandBuffer renderCommandEncoderWithDescriptor:aPass];
		if (mEncoder == nil)
			return false;

		MTLViewport aViewport = { 0.0, 0.0, (double)mSceneWidth, (double)mSceneHeight, 0.0, 1.0 };
		[mEncoder setViewport:aViewport];
		[mEncoder setCullMode:MTLCullModeNone];
		simd_float2 aSize = { (float)mSceneWidth, (float)mSceneHeight };
		[mEncoder setVertexBytes:&aSize length:sizeof(aSize) atIndex:1];
		mHasBoundBlend = false;
		return true;
	}

	bool AllocateVertices(NSUInteger theSize, id<MTLBuffer> __strong& theBuffer, NSUInteger& theOffset)
	{
		FrameResources& aFrame = mFrames[mFrameSerial % kMaxFramesInFlight];
		for (;;)
		{
			if (aFrame.mChunkIndex >= aFrame.mVertexChunks.size())
			{
				NSUInteger aChunkSize = std::max(kVertexChunkSize, theSize);
				id<MTLBuffer> aBuffer = [mDevice newBufferWithLength:aChunkSize options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
				if (aBuffer == nil)
					return false;
				aFrame.mVertexChunks.push_back(aBuffer);
			}

			id<MTLBuffer> aChunk = aFrame.mVertexChunks[aFrame.mChunkIndex];
			if (aFrame.mChunkOffset + theSize <= aChunk.length)
			{
				theBuffer = aChunk;
				theOffset = aFrame.mChunkOffset;
				aFrame.mChunkOffset += (theSize + 15) & ~(NSUInteger)15;
				return true;
			}

			aFrame.mChunkIndex++;
			aFrame.mChunkOffset = 0;
			if (aFrame.mChunkIndex < aFrame.mVertexChunks.size() && aFrame.mVertexChunks[aFrame.mChunkIndex].length < theSize)
			{
				aFrame.mVertexChunks[aFrame.mChunkIndex] = [mDevice newBufferWithLength:std::max(kVertexChunkSize, theSize) options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
			}
		}
	}

	void Draw(const RenderState& theState, RenderPrimitive thePrimitive, const RenderVertex* theVertices, int theVertexCount) override
	{
		if (mEncoder == nil || theVertexCount <= 0)
			return;

		MTLScissorRect aScissor = { 0, 0, (NSUInteger)mSceneWidth, (NSUInteger)mSceneHeight };
		if (theState.mScissorEnabled)
		{
			int aLeft = std::max(theState.mScissorX, 0);
			int aTop = std::max(theState.mScissorY, 0);
			int aRight = std::min(theState.mScissorX + theState.mScissorWidth, mSceneWidth);
			int aBottom = std::min(theState.mScissorY + theState.mScissorHeight, mSceneHeight);
			if (aRight <= aLeft || aBottom <= aTop)
				return;
			aScissor = { (NSUInteger)aLeft, (NSUInteger)aTop, (NSUInteger)(aRight - aLeft), (NSUInteger)(aBottom - aTop) };
		}

		id<MTLTexture> aTexture = mWhiteTexture;
		if (theState.mTexture != 0)
		{
			std::lock_guard<std::mutex> aLock(mTextureMutex);
			auto anItr = mTextures.find(theState.mTexture);
			if (anItr == mTextures.end())
				return;
			anItr->second.mLastUsedFrame = mFrameSerial;
			aTexture = anItr->second.mTexture;
		}

		NSUInteger aSize = (NSUInteger)theVertexCount * sizeof(RenderVertex);
		id<MTLBuffer> aBuffer = nil;
		NSUInteger anOffset = 0;
		if (!AllocateVertices(aSize, aBuffer, anOffset))
			return;
		std::memcpy((char*)aBuffer.contents + anOffset, theVertices, aSize);

		if (!mHasBoundBlend || mBoundBlend != theState.mBlendMode)
		{
			[mEncoder setRenderPipelineState:mScenePipelines[(int)theState.mBlendMode]];
			mBoundBlend = theState.mBlendMode;
			mHasBoundBlend = true;
		}

		[mEncoder setScissorRect:aScissor];
		[mEncoder setFragmentTexture:aTexture atIndex:0];
		[mEncoder setFragmentSamplerState:mSamplers[theState.mFilter == RenderFilter::Linear ? 1 : 0][theState.mAddressMode == RenderAddressMode::Wrap ? 1 : 0] atIndex:0];
		[mEncoder setVertexBuffer:aBuffer offset:anOffset atIndex:0];
		[mEncoder drawPrimitives:(thePrimitive == RenderPrimitive::LineList ? MTLPrimitiveTypeLine : MTLPrimitiveTypeTriangle) vertexStart:0 vertexCount:(NSUInteger)theVertexCount];
	}

	void EndScene() override
	{
		if (mEncoder != nil)
		{
			[mEncoder endEncoding];
			mEncoder = nil;
		}
	}

	void GetDrawableSize(int& theWidth, int& theHeight) override
	{
		if (!SDL_GetWindowSizeInPixels(mWindow, &theWidth, &theHeight))
		{
			theWidth = 0;
			theHeight = 0;
		}
	}

	void Present(const RenderViewport& theViewport, bool theLinearScaling, bool theShow) override
	{
		@autoreleasepool
		{
			EndScene();
			BeginFrame();

			int aWidth = 0, aHeight = 0;
			GetDrawableSize(aWidth, aHeight);
			if (theShow && aWidth > 0 && aHeight > 0)
			{
				CGSize aSize = CGSizeMake(aWidth, aHeight);
				if (!CGSizeEqualToSize(mLayer.drawableSize, aSize))
					mLayer.drawableSize = aSize;

				id<CAMetalDrawable> aDrawable = [mLayer nextDrawable];
				if (aDrawable != nil)
				{
					MTLRenderPassDescriptor* aPass = [MTLRenderPassDescriptor renderPassDescriptor];
					aPass.colorAttachments[0].texture = aDrawable.texture;
					aPass.colorAttachments[0].loadAction = MTLLoadActionClear;
					aPass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
					aPass.colorAttachments[0].storeAction = MTLStoreActionStore;

					id<MTLRenderCommandEncoder> anEncoder = [mCommandBuffer renderCommandEncoderWithDescriptor:aPass];
					if (mScene != nil && theViewport.mWidth > 0 && theViewport.mHeight > 0)
					{
						MTLViewport aViewport = { (double)theViewport.mX, (double)theViewport.mY, (double)theViewport.mWidth, (double)theViewport.mHeight, 0.0, 1.0 };
						[anEncoder setViewport:aViewport];
						[anEncoder setRenderPipelineState:mPresentPipeline];
						[anEncoder setFragmentTexture:mScene atIndex:0];
						[anEncoder setFragmentSamplerState:mSamplers[theLinearScaling ? 1 : 0][0] atIndex:0];
						[anEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
					}
					[anEncoder endEncoding];
					[mCommandBuffer presentDrawable:aDrawable];
				}
			}

			std::uint64_t aFrame = mFrameSerial;
			dispatch_semaphore_t aSemaphore = mFrameSemaphore;
			std::atomic<std::uint64_t>* aCompleted = &mCompletedFrame;
			[mCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer>) {
				std::uint64_t aPrevious = aCompleted->load();
				while (aPrevious < aFrame && !aCompleted->compare_exchange_weak(aPrevious, aFrame))
				{
				}
				dispatch_semaphore_signal(aSemaphore);
			}];
			[mCommandBuffer commit];

			mCommandBuffer = nil;
			mFrameBegun = false;
			mFrameSerial++;
		}
	}

	void SetVSync(bool theVSync) override
	{
		mLayer.displaySyncEnabled = theVSync ? YES : NO;
	}

	bool ReadScene(std::uint32_t* thePixels) override
	{
		if (mScene == nil || thePixels == nullptr)
			return false;

		bool wasInScene = mEncoder != nil;
		EndScene();
		BeginFrame();

		NSUInteger aBytesPerRow = (NSUInteger)mSceneWidth * 4;
		id<MTLBuffer> aReadback = [mDevice newBufferWithLength:aBytesPerRow * mSceneHeight options:MTLResourceStorageModeShared];
		id<MTLBlitCommandEncoder> aBlit = [mCommandBuffer blitCommandEncoder];
		[aBlit copyFromTexture:mScene sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(mSceneWidth, mSceneHeight, 1)
			toBuffer:aReadback destinationOffset:0 destinationBytesPerRow:aBytesPerRow destinationBytesPerImage:aBytesPerRow * mSceneHeight];
		[aBlit endEncoding];
		[mCommandBuffer commit];
		[mCommandBuffer waitUntilCompleted];
		std::memcpy(thePixels, aReadback.contents, aBytesPerRow * mSceneHeight);

		// Continue the frame in a new command buffer.
		mCommandBuffer = [mQueue commandBuffer];
		if (wasInScene)
			BeginScene();
		return true;
	}
};

}

RenderDevice* Sexy::CreateMetalRenderDevice(SDL_Window* theWindow)
{
	MetalRenderDevice* aDevice = new MetalRenderDevice();
	if (!aDevice->Init(theWindow))
	{
		delete aDevice;
		return nullptr;
	}
	return aDevice;
}
