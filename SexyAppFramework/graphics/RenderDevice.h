#ifndef __SEXY_RENDERDEVICE_H__
#define __SEXY_RENDERDEVICE_H__

#include <cstdint>
#include <string>

namespace Sexy
{

// A GPU texture owned by a RenderDevice. Zero is never a valid texture.
typedef std::uint32_t RenderTextureId;

// One pre-transformed vertex. Positions are in logical screen pixels using the
// Direct3D 7 rasterization convention the framework was written for: the
// center of pixel (x, y) is at (x, y), not (x + 0.5, y + 0.5). Backends apply
// the half-pixel shift when converting to their own clip space.
struct RenderVertex
{
	float			x;
	float			y;
	std::uint32_t	color;	// 0xAARRGGBB
	float			u;
	float			v;
};

enum class RenderBlendMode : std::uint8_t
{
	Normal,		// src * srcAlpha + dst * (1 - srcAlpha)
	Additive,	// src * srcAlpha + dst
	Copy		// src
};

enum class RenderFilter : std::uint8_t
{
	Point,
	Linear
};

enum class RenderAddressMode : std::uint8_t
{
	Clamp,
	Wrap
};

enum class RenderPrimitive : std::uint8_t
{
	TriangleList,
	LineList
};

struct RenderState
{
	RenderTextureId		mTexture = 0;		// 0 draws with vertex colors only
	RenderBlendMode		mBlendMode = RenderBlendMode::Normal;
	RenderFilter		mFilter = RenderFilter::Point;
	RenderAddressMode	mAddressMode = RenderAddressMode::Clamp;
	bool				mScissorEnabled = false;
	int					mScissorX = 0;
	int					mScissorY = 0;
	int					mScissorWidth = 0;
	int					mScissorHeight = 0;

	bool operator==(const RenderState& theOther) const
	{
		return mTexture == theOther.mTexture && mBlendMode == theOther.mBlendMode && mFilter == theOther.mFilter &&
			mAddressMode == theOther.mAddressMode && mScissorEnabled == theOther.mScissorEnabled &&
			(!mScissorEnabled || (mScissorX == theOther.mScissorX && mScissorY == theOther.mScissorY &&
				mScissorWidth == theOther.mScissorWidth && mScissorHeight == theOther.mScissorHeight));
	}
	bool operator!=(const RenderState& theOther) const { return !(*this == theOther); }
};

// Where the logical scene is shown inside the window, in drawable pixels.
struct RenderViewport
{
	int				mX = 0;
	int				mY = 0;
	int				mWidth = 0;
	int				mHeight = 0;
};

// The graphics backend. The framework renders the game into a persistent
// logical render target (the "scene", normally 800x600) that survives across
// frames, because the widget system only repaints dirty regions. Presenting
// scales the scene into the window.
//
// Every platform supplies one implementation. Backends know nothing about the
// framework's image classes; they only see textures, vertices, and state.
class RenderDevice
{
public:
	virtual ~RenderDevice() {}

	virtual std::string			GetName() const = 0;

	// Resizes (and clears) the persistent scene target.
	virtual bool				SetSceneSize(int theWidth, int theHeight) = 0;

	// Textures hold 32-bit 0xAARRGGBB pixels. Updates may be issued at any
	// time; the backend guarantees that draws already submitted keep seeing
	// the previous contents. Destruction may be requested from any thread.
	virtual RenderTextureId		CreateTexture(int theWidth, int theHeight) = 0;
	virtual void				UpdateTexture(RenderTextureId theTexture, int theX, int theY, int theWidth, int theHeight, const std::uint32_t* thePixels, int thePitch) = 0;
	virtual bool				ReadTexture(RenderTextureId theTexture, std::uint32_t* thePixels) = 0;
	virtual void				DestroyTexture(RenderTextureId theTexture) = 0;

	// Draw calls are only valid between BeginScene and EndScene.
	virtual bool				BeginScene() = 0;
	virtual void				Draw(const RenderState& theState, RenderPrimitive thePrimitive, const RenderVertex* theVertices, int theVertexCount) = 0;
	virtual void				EndScene() = 0;

	// Submits the frame and, when theShow is set, displays the scene. The
	// viewport is where the scene goes in the drawable; everything outside it
	// is cleared to black. Pass theShow = false while the window is hidden so
	// the backend does not wait for a drawable.
	virtual void				Present(const RenderViewport& theViewport, bool theLinearScaling, bool theShow) = 0;

	// Size of the window's drawable in pixels.
	virtual void				GetDrawableSize(int& theWidth, int& theHeight) = 0;

	virtual void				SetVSync(bool theVSync) = 0;

	// Reads back the scene as 0xAARRGGBB pixels (theWidth x theHeight of the
	// scene). Slow; for screenshots and diagnostics.
	virtual bool				ReadScene(std::uint32_t* thePixels) = 0;
};

}

#endif //__SEXY_RENDERDEVICE_H__
