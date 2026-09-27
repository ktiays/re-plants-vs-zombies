#include "HardwareRenderer.h"
#include "Graphics.h"
#include "TriVertex.h"
#include "SexyAppBase.h"
#include "misc/AutoCrit.h"
#include "misc/SexyVector.h"

using namespace Sexy;

static inline std::uint32_t ToVertexColor(const Color& theColor)
{
	return ((std::uint32_t)(theColor.mAlpha & 0xFF) << 24) | ((std::uint32_t)(theColor.mRed & 0xFF) << 16) |
		((std::uint32_t)(theColor.mGreen & 0xFF) << 8) | (std::uint32_t)(theColor.mBlue & 0xFF);
}

#define GetColorFromTriVertex(theVertex, theColor) ((theVertex).color ? (std::uint32_t)(theVertex).color : (theColor))

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
HardwareRenderer::HardwareRenderer(RenderDevice* theDevice, int theWidth, int theHeight)
{
	mDevice = theDevice;
	mWidth = theWidth;
	mHeight = theHeight;
	mSceneBegun = false;
	mAddressMode = RenderAddressMode::Clamp;
	mBatchPrimitive = RenderPrimitive::TriangleList;
	mBatch.reserve(4096);
}

HardwareRenderer::~HardwareRenderer()
{
	Cleanup();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void HardwareRenderer::Cleanup()
{
	Flush();

	AutoCrit aCrit(mImageSetCritSect);
	for (MemoryImage* anImage : mImageSet)
	{
		DeviceTextureData* aData = (DeviceTextureData*)anImage->mD3DData;
		if (aData != NULL)
		{
			if (aData->mTexture != 0)
				mDevice->DestroyTexture(aData->mTexture);
			delete aData;
			anImage->mD3DData = NULL;
		}
	}
	mImageSet.clear();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool HardwareRenderer::PreDraw()
{
	if (gSexyAppBase->mPhysMinimized)
		return false;

	if (!mSceneBegun)
	{
		if (!mDevice->BeginScene())
			return false;

		mSceneBegun = true;
		mAddressMode = RenderAddressMode::Clamp;
	}

	return true;
}

void HardwareRenderer::Flush()
{
	FlushBatch();

	if (mSceneBegun)
	{
		mDevice->EndScene();
		mSceneBegun = false;
	}
}

void HardwareRenderer::FlushBatch()
{
	if (!mBatch.empty())
	{
		mDevice->Draw(mBatchState, mBatchPrimitive, mBatch.data(), (int)mBatch.size());
		mBatch.clear();
	}
}

void HardwareRenderer::AddVertices(const RenderState& theState, RenderPrimitive thePrimitive, const RenderVertex* theVertices, int theCount)
{
	if (!mBatch.empty() && (theState != mBatchState || thePrimitive != mBatchPrimitive))
		FlushBatch();

	mBatchState = theState;
	mBatchPrimitive = thePrimitive;
	mBatch.insert(mBatch.end(), theVertices, theVertices + theCount);
}

// Adds a quad given in Direct3D triangle-strip order.
void HardwareRenderer::AddQuad(const RenderState& theState, const RenderVertex theStrip[4])
{
	RenderVertex aList[6] = { theStrip[0], theStrip[1], theStrip[2], theStrip[2], theStrip[1], theStrip[3] };
	AddVertices(theState, RenderPrimitive::TriangleList, aList, 6);
}

RenderState HardwareRenderer::MakeState(int theDrawMode, MemoryImage* theImage, bool theLinearFilter)
{
	RenderState aState;
	aState.mBlendMode = (theDrawMode == Graphics::DRAWMODE_ADDITIVE) ? RenderBlendMode::Additive : RenderBlendMode::Normal;
	aState.mFilter = theLinearFilter ? RenderFilter::Linear : RenderFilter::Point;
	aState.mAddressMode = mAddressMode;
	if (theImage != NULL && theImage->mD3DData != NULL)
		aState.mTexture = ((DeviceTextureData*)theImage->mD3DData)->mTexture;
	return aState;
}

void HardwareRenderer::SetTextureAddressMode(RenderAddressMode theMode)
{
	mAddressMode = theMode;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool HardwareRenderer::CreateImageTexture(MemoryImage *theImage)
{
	bool wantPurge = false;

	if (theImage->mD3DData == NULL)
	{
		theImage->mD3DData = new DeviceTextureData();

		// The actual purging was deferred
		wantPurge = theImage->mPurgeBits;

		AutoCrit aCrit(mImageSetCritSect);
		mImageSet.insert(theImage);
	}

	DeviceTextureData* aData = (DeviceTextureData*)theImage->mD3DData;
	int aWidth = theImage->GetWidth();
	int aHeight = theImage->GetHeight();
	if (aWidth <= 0 || aHeight <= 0)
		return false;

	if (aData->mTexture == 0 || aData->mWidth != aWidth || aData->mHeight != aHeight || aData->mBitsChangedCount != theImage->mBitsChangedCount)
	{
		theImage->DeleteSWBuffers(); // don't need these buffers for 3d drawing
		theImage->CommitBits();

		if (aData->mTexture != 0 && (aData->mWidth != aWidth || aData->mHeight != aHeight))
		{
			mDevice->DestroyTexture(aData->mTexture);
			aData->mTexture = 0;
		}

		if (aData->mTexture == 0)
		{
			aData->mTexture = mDevice->CreateTexture(aWidth, aHeight);
			if (aData->mTexture == 0)
				return false;
			aData->mWidth = aWidth;
			aData->mHeight = aHeight;
		}

		if (theImage->mColorTable != NULL && theImage->mColorIndices != NULL)
		{
			std::vector<std::uint32_t> aPixels((size_t)aWidth * aHeight);
			for (size_t i = 0; i < aPixels.size(); i++)
				aPixels[i] = theImage->mColorTable[theImage->mColorIndices[i]];
			mDevice->UpdateTexture(aData->mTexture, 0, 0, aWidth, aHeight, aPixels.data(), aWidth);
		}
		else
		{
			// Mark the texture current first so GetBits does not try to
			// recover pixels from the texture we are about to fill.
			aData->mBitsChangedCount = -1;
			ulong* aBits = theImage->GetBits();
			mDevice->UpdateTexture(aData->mTexture, 0, 0, aWidth, aHeight, aBits, aWidth);
		}

		aData->mBitsChangedCount = theImage->mBitsChangedCount;
	}

	if (wantPurge)
		theImage->PurgeBits();

	return true;
}

bool HardwareRenderer::RecoverBits(MemoryImage* theImage)
{
	DeviceTextureData* aData = (DeviceTextureData*)theImage->mD3DData;
	if (aData == NULL || aData->mTexture == 0)
		return false;

	if (aData->mBitsChangedCount != theImage->mBitsChangedCount) // bits have changed since texture was created
		return false;

	if (aData->mWidth != theImage->mWidth || aData->mHeight != theImage->mHeight || theImage->mBits == NULL)
		return false;

	return mDevice->ReadTexture(aData->mTexture, theImage->mBits);
}

void HardwareRenderer::RemoveMemoryImage(MemoryImage *theImage)
{
	if (theImage->mD3DData != NULL)
	{
		DeviceTextureData* aData = (DeviceTextureData*)theImage->mD3DData;
		if (aData->mTexture != 0)
		{
			// Draws already in the batch may still reference this texture.
			if (!mBatch.empty() && mBatchState.mTexture == aData->mTexture)
				FlushBatch();
			mDevice->DestroyTexture(aData->mTexture);
		}
		delete aData;
		theImage->mD3DData = NULL;

		AutoCrit aCrit(mImageSetCritSect);
		mImageSet.erase(theImage);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void HardwareRenderer::PushTransform(const SexyMatrix3 &theTransform, bool concatenate)
{
	if (mTransformStack.empty() || !concatenate)
		mTransformStack.push_back(theTransform);
	else
	{
		SexyMatrix3 &aTrans = mTransformStack.back();
		mTransformStack.push_back(theTransform*aTrans);
	}
}

void HardwareRenderer::PopTransform()
{
	if (!mTransformStack.empty())
		mTransformStack.pop_back();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void HardwareRenderer::Blt(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode, bool linearFilter)
{
	if (!mTransformStack.empty())
	{
		BltClipF(theImage,theX,theY,theSrcRect,NULL,theColor,theDrawMode);
		return;
	}

	if (!PreDraw())
		return;

	MemoryImage* aSrcMemoryImage = (MemoryImage*) theImage;
	if (!CreateImageTexture(aSrcMemoryImage))
		return;

	if ((theSrcRect.mWidth <= 0) || (theSrcRect.mHeight <= 0))
		return;

	DeviceTextureData* aData = (DeviceTextureData*)aSrcMemoryImage->mD3DData;
	RenderState aState = MakeState(theDrawMode, aSrcMemoryImage, linearFilter);
	std::uint32_t aColor = ToVertexColor(theColor);

	float u1 = (float)theSrcRect.mX / aData->mWidth;
	float v1 = (float)theSrcRect.mY / aData->mHeight;
	float u2 = (float)(theSrcRect.mX + theSrcRect.mWidth) / aData->mWidth;
	float v2 = (float)(theSrcRect.mY + theSrcRect.mHeight) / aData->mHeight;

	float x = theX - 0.5f;
	float y = theY - 0.5f;
	float aWidth = (float)theSrcRect.mWidth;
	float aHeight = (float)theSrcRect.mHeight;

	RenderVertex aVertex[4] =
	{
		{ x,			y,				aColor, u1, v1 },
		{ x,			y + aHeight,	aColor, u1, v2 },
		{ x + aWidth,	y,				aColor, u2, v1 },
		{ x + aWidth,	y + aHeight,	aColor, u2, v2 }
	};
	AddQuad(aState, aVertex);
}

void HardwareRenderer::BltMirror(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode, bool linearFilter)
{
	SexyTransform2D aTransform;

	aTransform.Translate(-theSrcRect.mWidth,0);
	aTransform.Scale(-1, 1);
	aTransform.Translate(theX, theY);

	BltTransformed(theImage,NULL,theColor,theDrawMode,theSrcRect,aTransform,linearFilter);
}

void HardwareRenderer::BltClipF(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect *theClipRect, const Color& theColor, int theDrawMode)
{
	SexyTransform2D aTransform;
	aTransform.Translate(theX, theY);

	BltTransformed(theImage,theClipRect,theColor,theDrawMode,theSrcRect,aTransform,true);
}

void HardwareRenderer::StretchBlt(Image* theImage,  const Rect& theDestRect, const Rect& theSrcRect, const Rect* theClipRect, const Color &theColor, int theDrawMode, bool fastStretch, bool mirror)
{
	float xScale = (float)theDestRect.mWidth / theSrcRect.mWidth;
	float yScale = (float)theDestRect.mHeight / theSrcRect.mHeight;

	SexyTransform2D aTransform;
	if (mirror)
	{
		aTransform.Translate(-theSrcRect.mWidth,0);
		aTransform.Scale(-xScale, yScale);
	}
	else
		aTransform.Scale(xScale, yScale);

	aTransform.Translate(theDestRect.mX, theDestRect.mY);
	BltTransformed(theImage,theClipRect,theColor,theDrawMode,theSrcRect,aTransform,!fastStretch);
}

void HardwareRenderer::BltRotated(Image* theImage, float theX, float theY, const Rect* theClipRect, const Color& theColor, int theDrawMode, double theRot, float theRotCenterX, float theRotCenterY, const Rect &theSrcRect)
{
	SexyTransform2D aTransform;

	aTransform.Translate(-theRotCenterX, -theRotCenterY);
	aTransform.RotateRad(theRot);
	aTransform.Translate(theX+theRotCenterX,theY+theRotCenterY);

	BltTransformed(theImage,theClipRect,theColor,theDrawMode,theSrcRect,aTransform,true);
}

void HardwareRenderer::DrawTexturedQuads(MemoryImage* theImage, const SexyMatrix3& theTrans, const Rect& theSrcRect, const Color& theColor, int theDrawMode, bool theLinearFilter, const Rect* theClipRect, float theX, float theY, bool center)
{
	if ((theSrcRect.mWidth <= 0) || (theSrcRect.mHeight <= 0))
		return;

	DeviceTextureData* aData = (DeviceTextureData*)theImage->mD3DData;
	float startx = 0, starty = 0;
	float pixelcorrect = 0.5f;

	if (center)
	{
		startx = -theSrcRect.mWidth/2.0f;
		starty = -theSrcRect.mHeight/2.0f;
		pixelcorrect = 0.0f;
	}

	float u1 = (float)theSrcRect.mX / aData->mWidth;
	float v1 = (float)theSrcRect.mY / aData->mHeight;
	float u2 = (float)(theSrcRect.mX + theSrcRect.mWidth) / aData->mWidth;
	float v2 = (float)(theSrcRect.mY + theSrcRect.mHeight) / aData->mHeight;

	float x = startx;
	float y = starty;
	float aWidth = (float)theSrcRect.mWidth;
	float aHeight = (float)theSrcRect.mHeight;

	SexyVector2 p[4] = { SexyVector2(x, y), SexyVector2(x,y+aHeight), SexyVector2(x+aWidth, y) , SexyVector2(x+aWidth, y+aHeight) };
	SexyVector2 tp[4];

	for (int i=0; i<4; i++)
	{
		tp[i] = theTrans*p[i];
		tp[i].x -= pixelcorrect - theX;
		tp[i].y -= pixelcorrect - theY;
	}

	RenderState aState = MakeState(theDrawMode, theImage, theLinearFilter);

	if (theClipRect != NULL)
	{
		int left = theClipRect->mX;
		int right = left + theClipRect->mWidth;
		int top = theClipRect->mY;
		int bottom = top + theClipRect->mHeight;
		for (int i=0; i<4; i++)
		{
			if (tp[i].x<left || tp[i].x>=right || tp[i].y<top || tp[i].y>=bottom)
			{
				aState.mScissorEnabled = true;
				aState.mScissorX = theClipRect->mX;
				aState.mScissorY = theClipRect->mY;
				aState.mScissorWidth = theClipRect->mWidth;
				aState.mScissorHeight = theClipRect->mHeight;
				break;
			}
		}
	}

	std::uint32_t aColor = ToVertexColor(theColor);
	RenderVertex aVertex[4] =
	{
		{ tp[0].x, tp[0].y, aColor, u1, v1 },
		{ tp[1].x, tp[1].y, aColor, u1, v2 },
		{ tp[2].x, tp[2].y, aColor, u2, v1 },
		{ tp[3].x, tp[3].y, aColor, u2, v2 }
	};
	AddQuad(aState, aVertex);
}

void HardwareRenderer::BltTransformed(Image* theImage, const Rect* theClipRect, const Color& theColor, int theDrawMode, const Rect &theSrcRect, const SexyMatrix3 &theTransform, bool linearFilter, float theX, float theY, bool center)
{
	if (!PreDraw())
		return;

	MemoryImage* aSrcMemoryImage = (MemoryImage*) theImage;
	if (!CreateImageTexture(aSrcMemoryImage))
		return;

	if (!mTransformStack.empty())
	{
		// Force linear filtering in the case of a global transform
		if (theX!=0 || theY!=0)
		{
			SexyTransform2D aTransform;
			if (center)
				aTransform.Translate(-theSrcRect.mWidth/2.0f,-theSrcRect.mHeight/2.0f);

			aTransform = theTransform * aTransform;
			aTransform.Translate(theX,theY);
			aTransform = mTransformStack.back() * aTransform;

			DrawTexturedQuads(aSrcMemoryImage, aTransform, theSrcRect, theColor, theDrawMode, true, theClipRect, 0, 0, false);
		}
		else
		{
			SexyTransform2D aTransform = mTransformStack.back()*theTransform;
			DrawTexturedQuads(aSrcMemoryImage, aTransform, theSrcRect, theColor, theDrawMode, true, theClipRect, theX, theY, center);
		}
	}
	else
	{
		DrawTexturedQuads(aSrcMemoryImage, theTransform, theSrcRect, theColor, theDrawMode, linearFilter, theClipRect, theX, theY, center);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void HardwareRenderer::DrawLine(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode)
{
	if (!PreDraw())
		return;

	float x1, y1, x2, y2;
	std::uint32_t aColor = ToVertexColor(theColor);

	if (!mTransformStack.empty())
	{
		SexyVector2 p1((float)theStartX,(float)theStartY);
		SexyVector2 p2((float)theEndX,(float)theEndY);
		p1 = mTransformStack.back()*p1;
		p2 = mTransformStack.back()*p2;

		x1 = p1.x;
		y1 = p1.y;
		x2 = p2.x;
		y2 = p2.y;
	}
	else
	{
		x1 = (float)theStartX;
		y1 = (float)theStartY;
		x2 = (float)theEndX;
		y2 = (float)theEndY;
	}

	// The original drew a three-point line strip whose final short segment
	// lights the end pixel.
	RenderVertex aVertex[4] =
	{
		{ x1,			y1,			aColor, 0, 0 },
		{ x2,			y2,			aColor, 0, 0 },
		{ x2,			y2,			aColor, 0, 0 },
		{ x2 + 0.5f,	y2 + 0.5f,	aColor, 0, 0 }
	};

	AddVertices(MakeState(theDrawMode, NULL, false), RenderPrimitive::LineList, aVertex, 4);
}

void HardwareRenderer::FillRect(const Rect& theRect, const Color& theColor, int theDrawMode)
{
	if (!PreDraw())
		return;

	std::uint32_t aColor = ToVertexColor(theColor);
	float x = theRect.mX - 0.5f;
	float y = theRect.mY - 0.5f;
	float aWidth = (float)theRect.mWidth;
	float aHeight = (float)theRect.mHeight;

	RenderVertex aVertex[4] =
	{
		{ x,			y,			aColor, 0, 0 },
		{ x,			y+aHeight,	aColor, 0, 0 },
		{ x+aWidth,		y,			aColor, 0, 0 },
		{ x+aWidth,		y+aHeight,	aColor, 0, 0 }
	};

	if (!mTransformStack.empty())
	{
		SexyVector2 p[4] = { SexyVector2(x, y), SexyVector2(x,y+aHeight), SexyVector2(x+aWidth, y) , SexyVector2(x+aWidth, y+aHeight) };

		for (int i=0; i<4; i++)
		{
			p[i] = mTransformStack.back()*p[i];
			p[i].x -= 0.5f;
			p[i].y -= 0.5f;
			aVertex[i].x = p[i].x;
			aVertex[i].y = p[i].y;
		}
	}

	AddQuad(MakeState(theDrawMode, NULL, false), aVertex);
}

void HardwareRenderer::DrawTriangle(const TriVertex &p1, const TriVertex &p2, const TriVertex &p3, const Color &theColor, int theDrawMode)
{
	if (!PreDraw())
		return;

	std::uint32_t aColor = ToVertexColor(theColor);
	RenderVertex aVertex[3] =
	{
		{ p1.x, p1.y, GetColorFromTriVertex(p1, aColor), 0, 0 },
		{ p2.x, p2.y, GetColorFromTriVertex(p2, aColor), 0, 0 },
		{ p3.x, p3.y, GetColorFromTriVertex(p3, aColor), 0, 0 }
	};

	AddVertices(MakeState(theDrawMode, NULL, false), RenderPrimitive::TriangleList, aVertex, 3);
}

void HardwareRenderer::FillPoly(const Point theVertices[], int theNumVertices, const Rect *theClipRect, const Color &theColor, int theDrawMode, int tx, int ty)
{
	if (theNumVertices<3)
		return;

	if (!PreDraw())
		return;

	std::uint32_t aColor = ToVertexColor(theColor);
	RenderState aState = MakeState(theDrawMode, NULL, false);
	if (theClipRect != NULL)
	{
		aState.mScissorEnabled = true;
		aState.mScissorX = theClipRect->mX;
		aState.mScissorY = theClipRect->mY;
		aState.mScissorWidth = theClipRect->mWidth;
		aState.mScissorHeight = theClipRect->mHeight;
	}

	std::vector<RenderVertex> aFan;
	aFan.reserve(theNumVertices);
	for (int i=0; i<theNumVertices; i++)
	{
		RenderVertex aVert = { (float)(theVertices[i].mX + tx), (float)(theVertices[i].mY + ty), aColor, 0, 0 };
		if (!mTransformStack.empty())
		{
			SexyVector2 v(aVert.x,aVert.y);
			v = mTransformStack.back()*v;
			aVert.x = v.x;
			aVert.y = v.y;
		}

		aFan.push_back(aVert);
	}

	std::vector<RenderVertex> aList;
	aList.reserve((theNumVertices - 2) * 3);
	for (int i = 1; i < theNumVertices - 1; i++)
	{
		aList.push_back(aFan[0]);
		aList.push_back(aFan[i]);
		aList.push_back(aFan[i + 1]);
	}

	AddVertices(aState, RenderPrimitive::TriangleList, aList.data(), (int)aList.size());
}

void HardwareRenderer::DrawTriangleTex(const TriVertex &p1, const TriVertex &p2, const TriVertex &p3, const Color &theColor, int theDrawMode, Image *theTexture, bool blend)
{
	TriVertex aVertices[1][3] = {{p1, p2, p3}};
	DrawTrianglesTex(aVertices,1,theColor,theDrawMode,theTexture,0,0,blend);
}

void HardwareRenderer::DrawTrianglesTex(const TriVertex theVertices[][3], int theNumTriangles, const Color &theColor, int theDrawMode, Image *theTexture, float tx, float ty, bool blend)
{
	if (!PreDraw())
		return;

	MemoryImage* aSrcMemoryImage = (MemoryImage*)theTexture;
	if (!CreateImageTexture(aSrcMemoryImage))
		return;

	RenderState aState = MakeState(theDrawMode, aSrcMemoryImage, blend);
	std::uint32_t aColor = ToVertexColor(theColor);

	RenderVertex aVertexCache[100 * 3];
	int aVertexCount = 0;

	for (int aTriangleNum = 0; aTriangleNum < theNumTriangles; aTriangleNum++)
	{
		const TriVertex* aTriVerts = theVertices[aTriangleNum];
		for (int i = 0; i < 3; i++)
		{
			RenderVertex& aVertex = aVertexCache[aVertexCount++];
			aVertex.x = aTriVerts[i].x + tx;
			aVertex.y = aTriVerts[i].y + ty;
			aVertex.color = GetColorFromTriVertex(aTriVerts[i], aColor);
			aVertex.u = aTriVerts[i].u;
			aVertex.v = aTriVerts[i].v;
		}

		if (aVertexCount == 100 * 3)
		{
			AddVertices(aState, RenderPrimitive::TriangleList, aVertexCache, aVertexCount);
			aVertexCount = 0;
		}
	}

	if (aVertexCount > 0)
		AddVertices(aState, RenderPrimitive::TriangleList, aVertexCache, aVertexCount);
}

void HardwareRenderer::DrawTrianglesTexStrip(const TriVertex theVertices[], int theNumTriangles, const Color &theColor, int theDrawMode, Image *theTexture, float tx, float ty, bool blend)
{
	TriVertex aList[100][3];
	int aTriNum = 0;
	while (aTriNum < theNumTriangles)
	{
		int aMaxTriangles = std::min(100,theNumTriangles - aTriNum);
		for (int i=0; i<aMaxTriangles; i++)
		{
			aList[i][0] = theVertices[aTriNum];
			aList[i][1] = theVertices[aTriNum+1];
			aList[i][2] = theVertices[aTriNum+2];
			aTriNum++;
		}
		DrawTrianglesTex(aList,aMaxTriangles,theColor,theDrawMode,theTexture, tx, ty, blend);
	}
}

void HardwareRenderer::BltScene(RenderTextureId theTexture, int theWidth, int theHeight)
{
	if (!PreDraw())
		return;

	RenderState aState;
	aState.mTexture = theTexture;
	aState.mBlendMode = RenderBlendMode::Copy;
	aState.mFilter = RenderFilter::Point;

	float x = -0.5f;
	float y = -0.5f;
	RenderVertex aVertex[4] =
	{
		{ x,				y,					0xFFFFFFFF, 0, 0 },
		{ x,				y + theHeight,		0xFFFFFFFF, 0, 1 },
		{ x + theWidth,		y,					0xFFFFFFFF, 1, 0 },
		{ x + theWidth,		y + theHeight,		0xFFFFFFFF, 1, 1 }
	};
	AddQuad(aState, aVertex);
}
