#include "DeviceImage.h"
#include "DisplayInterface.h"
#include "HardwareRenderer.h"
#include "Graphics.h"
#include "SexyAppBase.h"
#include "misc/Debug.h"

using namespace Sexy;

DeviceImage::DeviceImage(DisplayInterface* theDisplay) :
	MemoryImage(theDisplay->mApp)
{
	mDisplay = theDisplay;
	Init();
}

DeviceImage::DeviceImage() :
	MemoryImage(gSexyAppBase)
{
	mDisplay = gSexyAppBase->mDisplay;
	Init();
}

DeviceImage::~DeviceImage()
{
	if (mDisplay != NULL)
		mDisplay->RemoveDeviceImage(this);
}

void DeviceImage::Init()
{
	if (mDisplay != NULL)
		mDisplay->AddDeviceImage(this);

	mDrawToBits = false;
	mVideoMemory = false;
	mWantDDSurface = false;
}

bool DeviceImage::Check3D(Image *theImage)
{
	DeviceImage *anImage = dynamic_cast<DeviceImage*>(theImage);
	if (anImage != NULL)
		return Check3D(anImage);
	else
		return false;
}

bool DeviceImage::Check3D(DeviceImage *theImage)
{
	return theImage->mDisplay != NULL && theImage->mDisplay->mIs3D && theImage->IsScreen();
}

bool DeviceImage::IsScreen() const
{
	return mDisplay != NULL && mDisplay->mScreenImage == this;
}

void DeviceImage::SetVideoMemory(bool wantVideoMemory)
{
	mVideoMemory = wantVideoMemory;
}

ulong* DeviceImage::GetBits()
{
	if (Check3D(this))
	{
		// The accelerated screen only exists on the GPU; read it back.
		if (mBits == NULL)
		{
			mBits = new ulong[mWidth*mHeight + 1];
			mBits[mWidth*mHeight] = MEMORYCHECK_ID;
		}

		mDisplay->mRenderer->Flush();
		if (!mDisplay->mDevice->ReadScene(mBits))
			memset(mBits, 0, mWidth*mHeight*sizeof(ulong));
		return mBits;
	}

	return MemoryImage::GetBits();
}

bool DeviceImage::PolyFill3D(const Point theVertices[], int theNumVertices, const Rect *theClipRect, const Color &theColor, int theDrawMode, int tx, int ty)
{
	if (Check3D(this))
	{
		mDisplay->mRenderer->FillPoly(theVertices,theNumVertices,theClipRect,theColor,theDrawMode,tx,ty);
		return true;
	}
	else
		return false;
}

void DeviceImage::FillRect(const Rect& theRect, const Color& theColor, int theDrawMode)
{
	if (Check3D(this))
	{
		mDisplay->mRenderer->FillRect(theRect,theColor,theDrawMode);
		return;
	}

	MemoryImage::FillRect(theRect, theColor, theDrawMode);
}

void DeviceImage::DrawLine(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode)
{
	if (Check3D(this))
	{
		mDisplay->mRenderer->DrawLine(theStartX,theStartY,theEndX,theEndY,theColor,theDrawMode);
		return;
	}

	MemoryImage::DrawLine(theStartX, theStartY, theEndX, theEndY, theColor, theDrawMode);
}

void DeviceImage::DrawLineAA(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode)
{
	if (Check3D(this))
	{
		mDisplay->mRenderer->DrawLine(theStartX,theStartY,theEndX,theEndY,theColor,theDrawMode);
		return;
	}

	MemoryImage::DrawLineAA(theStartX, theStartY, theEndX, theEndY, theColor, theDrawMode);
}

void DeviceImage::Blt(Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode)
{
	theImage->mDrawn = true;

	DBG_ASSERTE((theColor.mRed >= 0) && (theColor.mRed <= 255));
	DBG_ASSERTE((theColor.mGreen >= 0) && (theColor.mGreen <= 255));
	DBG_ASSERTE((theColor.mBlue >= 0) && (theColor.mBlue <= 255));
	DBG_ASSERTE((theColor.mAlpha >= 0) && (theColor.mAlpha <= 255));

	CommitBits();

	if (Check3D(this))
	{
		mDisplay->mRenderer->Blt(theImage,theX,theY,theSrcRect,theColor,theDrawMode);
		return;
	}

	MemoryImage::Blt(theImage, theX, theY, theSrcRect, theColor, theDrawMode);
}

void DeviceImage::BltF(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect &theClipRect, const Color& theColor, int theDrawMode)
{
	theImage->mDrawn = true;

	if (Check3D(this))
	{
		FRect aClipRect(theClipRect.mX,theClipRect.mY,theClipRect.mWidth,theClipRect.mHeight);
		FRect aDestRect(theX,theY,theSrcRect.mWidth,theSrcRect.mHeight);

		FRect anIntersect = aDestRect.Intersection(aClipRect);
		if (anIntersect.mWidth!=aDestRect.mWidth || anIntersect.mHeight!=aDestRect.mHeight)
		{
			if (anIntersect.mWidth!=0 && anIntersect.mHeight!=0)
				mDisplay->mRenderer->BltClipF(theImage,theX,theY,theSrcRect,&theClipRect,theColor,theDrawMode);
		}
		else
			mDisplay->mRenderer->Blt(theImage,theX,theY,theSrcRect,theColor,theDrawMode,true);

		return;
	}
	else
		BltRotated(theImage,theX,theY,theSrcRect,theClipRect,theColor,theDrawMode,0,0,0);
}

void DeviceImage::BltRotated(Image* theImage, float theX, float theY, const Rect &theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, double theRot, float theRotCenterX, float theRotCenterY)
{
	theImage->mDrawn = true;

	CommitBits();

	if (Check3D(this))
	{
		mDisplay->mRenderer->BltRotated(theImage,theX,theY,&theClipRect,theColor,theDrawMode,theRot,theRotCenterX,theRotCenterY,theSrcRect);
		return;
	}

	MemoryImage::BltRotated(theImage, theX, theY, theSrcRect, theClipRect, theColor, theDrawMode, theRot, theRotCenterX, theRotCenterY);
}

void DeviceImage::StretchBlt(Image* theImage, const Rect& theDestRect, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch)
{
	theImage->mDrawn = true;

	CommitBits();

	if (Check3D(this))
	{
		mDisplay->mRenderer->StretchBlt(theImage,theDestRect,theSrcRect,&theClipRect,theColor,theDrawMode,fastStretch);
		return;
	}

	MemoryImage::StretchBlt(theImage, theDestRect, theSrcRect, theClipRect, theColor, theDrawMode, fastStretch);
}

void DeviceImage::BltMatrix(Image* theImage, float x, float y, const SexyMatrix3 &theMatrix, const Rect& theClipRect, const Color& theColor, int theDrawMode, const Rect &theSrcRect, bool blend)
{
	theImage->mDrawn = true;

	if (Check3D(this))
	{
		mDisplay->mRenderer->BltTransformed(theImage,&theClipRect,theColor,theDrawMode,theSrcRect,theMatrix,blend,x,y,true);
		return;
	}

	MemoryImage::BltMatrix(theImage, x, y, theMatrix, theClipRect, theColor, theDrawMode, theSrcRect, blend);
}

void DeviceImage::BltTrianglesTex(Image *theTexture, const TriVertex theVertices[][3], int theNumTriangles, const Rect& theClipRect, const Color &theColor, int theDrawMode, float tx, float ty, bool blend)
{
	theTexture->mDrawn = true;

	if (Check3D(this))
	{
		mDisplay->mRenderer->DrawTrianglesTex(theVertices,theNumTriangles,theColor,theDrawMode,theTexture,tx,ty,blend);
		return;
	}

	MemoryImage::BltTrianglesTex(theTexture, theVertices, theNumTriangles, theClipRect, theColor, theDrawMode, tx, ty, blend);
}

// Copies theSrcRect of theImage into a new image, flipped horizontally.
static MemoryImage* CreateMirroredImage(Image* theImage, const Rect& theSrcRect)
{
	MemoryImage* aSrcImage = dynamic_cast<MemoryImage*>(theImage);
	if (aSrcImage == NULL || theSrcRect.mWidth <= 0 || theSrcRect.mHeight <= 0)
		return NULL;

	ulong* aSrcBits = aSrcImage->GetBits();
	if (aSrcBits == NULL)
		return NULL;

	MemoryImage* aMirror = new MemoryImage(aSrcImage->mApp);
	aMirror->Create(theSrcRect.mWidth, theSrcRect.mHeight);
	ulong* aDestBits = aMirror->GetBits();

	for (int y = 0; y < theSrcRect.mHeight; y++)
	{
		const ulong* aSrcRow = aSrcBits + (theSrcRect.mY + y) * aSrcImage->mWidth + theSrcRect.mX;
		ulong* aDestRow = aDestBits + y * theSrcRect.mWidth;
		for (int x = 0; x < theSrcRect.mWidth; x++)
			aDestRow[theSrcRect.mWidth - 1 - x] = aSrcRow[x];
	}

	aMirror->mHasAlpha = aSrcImage->mHasAlpha;
	aMirror->mHasTrans = aSrcImage->mHasTrans;
	aMirror->BitsChanged();
	return aMirror;
}

void DeviceImage::BltMirror(Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode)
{
	DBG_ASSERTE((theColor.mRed >= 0) && (theColor.mRed <= 255));
	DBG_ASSERTE((theColor.mGreen >= 0) && (theColor.mGreen <= 255));
	DBG_ASSERTE((theColor.mBlue >= 0) && (theColor.mBlue <= 255));
	DBG_ASSERTE((theColor.mAlpha >= 0) && (theColor.mAlpha <= 255));

	theImage->mDrawn = true;
	CommitBits();

	if (Check3D(this))
	{
		mDisplay->mRenderer->BltMirror(theImage,theX,theY,theSrcRect,theColor,theDrawMode);
		return;
	}

	MemoryImage* aMirror = CreateMirroredImage(theImage, theSrcRect);
	if (aMirror == NULL)
		return;

	MemoryImage::Blt(aMirror, theX, theY, Rect(0, 0, theSrcRect.mWidth, theSrcRect.mHeight), theColor, theDrawMode);
	delete aMirror;
}

void DeviceImage::StretchBltMirror(Image* theImage, const Rect& theDestRect, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch)
{
	theImage->mDrawn = true;
	CommitBits();

	if (Check3D(this))
	{
		mDisplay->mRenderer->StretchBlt(theImage,theDestRect,theSrcRect,&theClipRect,theColor,theDrawMode,fastStretch,true);
		return;
	}

	MemoryImage* aMirror = CreateMirroredImage(theImage, theSrcRect);
	if (aMirror == NULL)
		return;

	MemoryImage::StretchBlt(aMirror, theDestRect, Rect(0, 0, theSrcRect.mWidth, theSrcRect.mHeight), theClipRect, theColor, theDrawMode, fastStretch);
	delete aMirror;
}

void DeviceImage::FillScanLinesWithCoverage(Span* theSpans, int theSpanCount, const Color& theColor, int theDrawMode, const BYTE* theCoverage, int theCoverX, int theCoverY, int theCoverWidth, int theCoverHeight)
{
	if (theSpanCount == 0)
		return;

	if (Check3D(this))
	{
		// Rasterize the covered spans into a temporary image, then draw it.
		int l = theSpans[0].mX, t = theSpans[0].mY;
		int r = l + theSpans[0].mWidth, b = t;
		for (int i = 1; i < theSpanCount; ++i)
		{
			l = std::min(theSpans[i].mX, l);
			r = std::max(theSpans[i].mX + theSpans[i].mWidth - 1, r);
			t = std::min(theSpans[i].mY, t);
			b = std::max(theSpans[i].mY, b);
		}
		for (int i = 0; i < theSpanCount; ++i)
		{
			theSpans[i].mX -= l;
			theSpans[i].mY -= t;
		}

		MemoryImage aTempImage;
		aTempImage.Create(r-l+1, b-t+1);
		aTempImage.FillScanLinesWithCoverage(theSpans, theSpanCount, theColor, theDrawMode, theCoverage, theCoverX - l, theCoverY - t, theCoverWidth, theCoverHeight);
		Blt(&aTempImage, l, t, Rect(0, 0, r-l+1, b-t+1), Color::White, theDrawMode);
		return;
	}

	MemoryImage::FillScanLinesWithCoverage(theSpans, theSpanCount, theColor, theDrawMode, theCoverage, theCoverX, theCoverY, theCoverWidth, theCoverHeight);
}
