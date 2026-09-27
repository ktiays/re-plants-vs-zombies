#ifndef __SEXY_HARDWARERENDERER_H__
#define __SEXY_HARDWARERENDERER_H__

#include "Common.h"
#include "MemoryImage.h"
#include "RenderDevice.h"
#include "misc/CritSect.h"
#include "misc/SexyMatrix.h"

#include <vector>

namespace Sexy
{

class TriVertex;
class Image;

///////////////////////////////////////////////////////////////////////////////
// Flags for MemoryImage::mD3DFlags. The values are those of the original
// Direct3D renderer because game code sets them directly.
enum D3DImageFlags
{
	D3DImageFlag_MinimizeNumSubdivisions	=			0x0001,
	D3DImageFlag_Use64By64Subdivisions		=			0x0002,
	D3DImageFlag_UseA4R4G4B4				=			0x0004,
	D3DImageFlag_UseA8R8G8B8				=			0x0008
};

///////////////////////////////////////////////////////////////////////////////
// Per-image GPU state stored in MemoryImage::mD3DData.
struct DeviceTextureData
{
	RenderTextureId			mTexture = 0;
	int						mWidth = 0;
	int						mHeight = 0;
	int						mBitsChangedCount = -1;
};

///////////////////////////////////////////////////////////////////////////////
// The accelerated drawing path. This is the backend-neutral replacement for
// the framework's Direct3D 7 interface: it keeps the same entry points and
// reproduces the same vertex generation, then hands batches of triangles to
// the platform's RenderDevice.
class HardwareRenderer
{
public:
	RenderDevice*			mDevice;
	int						mWidth;
	int						mHeight;

	typedef std::set<MemoryImage*> ImageSet;
	ImageSet				mImageSet;
	CritSect				mImageSetCritSect;

	typedef std::list<SexyMatrix3> TransformStack;
	TransformStack			mTransformStack;

	bool					mSceneBegun;
	RenderAddressMode		mAddressMode;

protected:
	RenderState				mBatchState;
	RenderPrimitive			mBatchPrimitive;
	std::vector<RenderVertex> mBatch;

	RenderState				MakeState(int theDrawMode, MemoryImage* theImage, bool theLinearFilter);
	void					AddVertices(const RenderState& theState, RenderPrimitive thePrimitive, const RenderVertex* theVertices, int theCount);
	void					AddQuad(const RenderState& theState, const RenderVertex theStrip[4]);
	void					FlushBatch();
	void					DrawTexturedQuads(MemoryImage* theImage, const SexyMatrix3& theTrans, const Rect& theSrcRect, const Color& theColor, int theDrawMode, bool theLinearFilter, const Rect* theClipRect, float theX, float theY, bool center);

public:
	HardwareRenderer(RenderDevice* theDevice, int theWidth, int theHeight);
	virtual ~HardwareRenderer();

	void					Cleanup();
	void					PushTransform(const SexyMatrix3 &theTransform, bool concatenate = true);
	void					PopTransform();

	bool					PreDraw();
	void					Flush();
	void					RemoveMemoryImage(MemoryImage *theImage);

	// Texture address mode used by subsequent textured draws (the original
	// renderer exposed this through raw device texture-stage state).
	void					SetTextureAddressMode(RenderAddressMode theMode);

	bool					CreateImageTexture(MemoryImage *theImage);
	bool					RecoverBits(MemoryImage* theImage);
	void					Blt(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode, bool linearFilter = false);
	void					BltClipF(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect *theClipRect, const Color& theColor, int theDrawMode);
	void					BltMirror(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode, bool linearFilter = false);
	void					StretchBlt(Image* theImage,  const Rect& theDestRect, const Rect& theSrcRect, const Rect* theClipRect, const Color &theColor, int theDrawMode, bool fastStretch, bool mirror = false);
	void					BltRotated(Image* theImage, float theX, float theY, const Rect* theClipRect, const Color& theColor, int theDrawMode, double theRot, float theRotCenterX, float theRotCenterY, const Rect& theSrcRect);
	void					BltTransformed(Image* theImage, const Rect* theClipRect, const Color& theColor, int theDrawMode, const Rect &theSrcRect, const SexyMatrix3 &theTransform, bool linearFilter, float theX = 0, float theY = 0, bool center = false);
	void					DrawLine(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode);
	void					FillRect(const Rect& theRect, const Color& theColor, int theDrawMode);
	void					DrawTriangle(const TriVertex &p1, const TriVertex &p2, const TriVertex &p3, const Color &theColor, int theDrawMode);
	void					DrawTriangleTex(const TriVertex &p1, const TriVertex &p2, const TriVertex &p3, const Color &theColor, int theDrawMode, Image *theTexture, bool blend = true);
	void					DrawTrianglesTex(const TriVertex theVertices[][3], int theNumTriangles, const Color &theColor, int theDrawMode, Image *theTexture, float tx = 0, float ty = 0, bool blend = true);
	void					DrawTrianglesTexStrip(const TriVertex theVertices[], int theNumTriangles, const Color &theColor, int theDrawMode, Image *theTexture, float tx = 0, float ty = 0, bool blend = true);
	void					FillPoly(const Point theVertices[], int theNumVertices, const Rect *theClipRect, const Color &theColor, int theDrawMode, int tx, int ty);

	// Draws a full-scene image without blending (software presentation).
	void					BltScene(RenderTextureId theTexture, int theWidth, int theHeight);
};

}

#endif //__SEXY_HARDWARERENDERER_H__
