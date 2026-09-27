#ifndef __SEXY_DISPLAYINTERFACE_H__
#define __SEXY_DISPLAYINTERFACE_H__

#include "Common.h"
#include "NativeDisplay.h"
#include "RenderDevice.h"
#include "misc/CritSect.h"
#include "misc/Rect.h"
#include "misc/Ratio.h"

namespace Sexy
{

class SexyAppBase;
class DeviceImage;
class Image;
class MemoryImage;
class HardwareRenderer;

typedef std::set<DeviceImage*> DeviceImageSet;

// Owns the screen image and presents it through the platform RenderDevice.
// Replaces the DirectDraw display interface: in software mode the screen is a
// MemoryImage whose pixels are uploaded once per presented frame; in
// hardware-accelerated mode the screen is drawn directly by the
// HardwareRenderer into the device's persistent scene.
class DisplayInterface : public NativeDisplay
{
public:
	enum
	{
		RESULT_OK					= 0,
		RESULT_FAIL					= 1,
		RESULT_DD_CREATE_FAIL		= 2,
		RESULT_SURFACE_FAIL			= 3,
		RESULT_EXCLUSIVE_FAIL		= 4,
		RESULT_DISPCHANGE_FAIL		= 5,
		RESULT_INVALID_COLORDEPTH	= 6,
		RESULT_3D_FAIL				= 7
	};

	SexyAppBase*			mApp;
	RenderDevice*			mDevice;
	HardwareRenderer*		mRenderer;
	bool					mIs3D;

	CritSect				mCritSect;
	bool					mInRedraw;
	int						mWidth;
	int						mHeight;
	Ratio					mAspect;
	int						mDisplayWidth;
	int						mDisplayHeight;

	Rect					mPresentationRect;
	RenderViewport			mViewport;
	DWORD					mRefreshRate;
	DWORD					mMillisecondsPerFrame;

	bool					mInitialized;
	bool					mIsWindowed;
	DeviceImage*			mScreenImage;
	DeviceImageSet			mDeviceImageSet;
	RenderTextureId			mScreenTexture;
	bool					mLinearScaling;

	int						mCursorX;
	int						mCursorY;
	int						mNextCursorX;
	int						mNextCursorY;
	Image*					mCursorImage;

	std::string				mErrorString;

public:
	DisplayInterface(SexyAppBase* theApp, RenderDevice* theDevice);
	virtual ~DisplayInterface();

	static std::string		ResultToString(int theResult);

	void					AddDeviceImage(DeviceImage* theImage);
	void					RemoveDeviceImage(DeviceImage* theImage);
	void					Remove3DData(MemoryImage* theImage);
	void					Cleanup();

	DeviceImage*			GetScreenImage();
	int						Init(bool isWindowed);
	bool					Redraw(Rect* theClipRect = NULL);
	void					SetVideoOnlyDraw(bool videoOnly);

	// Recomputes where the scene is shown inside the window's drawable.
	void					UpdateViewport();
	// Converts window coordinates (in points) to logical screen coordinates.
	void					WindowToLogical(float theWindowX, float theWindowY, float thePixelScale, int& theX, int& theY);

	bool					SetCursorImage(Image* theImage);
	void					SetCursorPos(int theCursorX, int theCursorY);
};

}

#endif //__SEXY_DISPLAYINTERFACE_H__
