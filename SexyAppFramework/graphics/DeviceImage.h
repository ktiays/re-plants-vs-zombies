#ifndef __SEXY_DEVICEIMAGE_H__
#define __SEXY_DEVICEIMAGE_H__

#include "MemoryImage.h"

namespace Sexy
{

class DisplayInterface;

// An image that belongs to a DisplayInterface. Every image the framework
// creates is a DeviceImage. When hardware acceleration is on, drawing onto the
// screen image goes through the HardwareRenderer; every other destination is
// rasterized in software by MemoryImage.
class DeviceImage : public MemoryImage
{
public:
	DisplayInterface*		mDisplay;
	bool					mDrawToBits;

	// Retained for source compatibility with code written against the
	// DirectDraw image; surfaces no longer exist.
	bool					mVideoMemory;
	bool					mWantDDSurface;

private:
	void					Init();

public:
	DeviceImage();
	DeviceImage(DisplayInterface* theDisplay);
	virtual ~DeviceImage();

	static bool				Check3D(DeviceImage *theImage);
	static bool				Check3D(Image *theImage);

	bool					IsScreen() const;

	bool					LockSurface() { return false; }
	bool					UnlockSurface() { return false; }
	virtual void			SetVideoMemory(bool wantVideoMemory);
	bool					GenerateDDSurface() { return false; }

	virtual ulong*			GetBits();

	virtual bool			PolyFill3D(const Point theVertices[], int theNumVertices, const Rect *theClipRect, const Color &theColor, int theDrawMode, int tx, int ty);
	virtual void			FillRect(const Rect& theRect, const Color& theColor, int theDrawMode);
	virtual void			DrawLine(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode);
	virtual void			DrawLineAA(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode);
	virtual void			Blt(Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode);
	virtual void			BltF(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect &theClipRect, const Color& theColor, int theDrawMode);
	virtual void			BltRotated(Image* theImage, float theX, float theY, const Rect &theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, double theRot, float theRotCenterX, float theRotCenterY);
	virtual void			StretchBlt(Image* theImage, const Rect& theDestRect, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch);
	virtual void			BltMatrix(Image* theImage, float x, float y, const SexyMatrix3 &theMatrix, const Rect& theClipRect, const Color& theColor, int theDrawMode, const Rect &theSrcRect, bool blend);
	virtual void			BltTrianglesTex(Image *theTexture, const TriVertex theVertices[][3], int theNumTriangles, const Rect& theClipRect, const Color &theColor, int theDrawMode, float tx, float ty, bool blend);
	virtual void			BltMirror(Image* theImage, int theX, int theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode);
	virtual void			StretchBltMirror(Image* theImage, const Rect& theDestRect, const Rect& theSrcRect, const Rect& theClipRect, const Color& theColor, int theDrawMode, bool fastStretch);
	virtual void			FillScanLinesWithCoverage(Span* theSpans, int theSpanCount, const Color& theColor, int theDrawMode, const BYTE* theCoverage, int theCoverX, int theCoverY, int theCoverWidth, int theCoverHeight);
};

}

#endif //__SEXY_DEVICEIMAGE_H__
