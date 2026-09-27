#include "DisplayInterface.h"
#include "DeviceImage.h"
#include "HardwareRenderer.h"
#include "SexyAppBase.h"
#include "misc/AutoCrit.h"

#include <cmath>

using namespace Sexy;

DisplayInterface::DisplayInterface(SexyAppBase* theApp, RenderDevice* theDevice)
{
	mApp = theApp;
	mDevice = theDevice;
	mRenderer = NULL;
	mIs3D = false;
	mInRedraw = false;
	mWidth = theApp->mWidth;
	mHeight = theApp->mHeight;
	mAspect.Set(mWidth, mHeight);
	mDisplayWidth = mWidth;
	mDisplayHeight = mHeight;
	mPresentationRect = Rect(0, 0, mWidth, mHeight);
	mRefreshRate = 60;
	mMillisecondsPerFrame = 1000 / mRefreshRate;
	mInitialized = false;
	mIsWindowed = true;
	mScreenImage = NULL;
	mScreenTexture = 0;
	mLinearScaling = true;
	mCursorX = 0;
	mCursorY = 0;
	mNextCursorX = 0;
	mNextCursorY = 0;
	mCursorImage = NULL;

	// The software rasterizer's "native" format is 32-bit 8888.
	mRGBBits = 32;
	mRedMask = 0x00FF0000;
	mGreenMask = 0x0000FF00;
	mBlueMask = 0x000000FF;
	mRedBits = 8;
	mGreenBits = 8;
	mBlueBits = 8;
	mRedShift = 16;
	mGreenShift = 8;
	mBlueShift = 0;
}

DisplayInterface::~DisplayInterface()
{
	Cleanup();

	// Images still alive keep a pointer to us; detach them.
	AutoCrit anAutoCrit(mCritSect);
	for (DeviceImage* anImage : mDeviceImageSet)
		anImage->mDisplay = NULL;
	mDeviceImageSet.clear();
}

std::string DisplayInterface::ResultToString(int theResult)
{
	switch (theResult)
	{
	case RESULT_OK:					return "RESULT_OK";
	case RESULT_FAIL:				return "RESULT_FAIL";
	case RESULT_DD_CREATE_FAIL:		return "RESULT_DD_CREATE_FAIL";
	case RESULT_SURFACE_FAIL:		return "RESULT_SURFACE_FAIL";
	case RESULT_EXCLUSIVE_FAIL:		return "RESULT_EXCLUSIVE_FAIL";
	case RESULT_DISPCHANGE_FAIL:	return "RESULT_DISPCHANGE_FAIL";
	case RESULT_INVALID_COLORDEPTH:	return "RESULT_INVALID_COLORDEPTH";
	case RESULT_3D_FAIL:			return "RESULT_3D_FAIL";
	default:						return "RESULT_UNKNOWN";
	}
}

void DisplayInterface::AddDeviceImage(DeviceImage* theImage)
{
	AutoCrit anAutoCrit(mCritSect);
	mDeviceImageSet.insert(theImage);
}

void DisplayInterface::RemoveDeviceImage(DeviceImage* theImage)
{
	AutoCrit anAutoCrit(mCritSect);
	mDeviceImageSet.erase(theImage);
}

void DisplayInterface::Remove3DData(MemoryImage* theImage)
{
	if (mRenderer != NULL)
		mRenderer->RemoveMemoryImage(theImage);
}

void DisplayInterface::Cleanup()
{
	mInitialized = false;

	if (mRenderer != NULL)
	{
		mRenderer->Cleanup();
		delete mRenderer;
		mRenderer = NULL;
	}

	if (mScreenTexture != 0)
	{
		mDevice->DestroyTexture(mScreenTexture);
		mScreenTexture = 0;
	}

	if (mScreenImage != NULL)
	{
		delete mScreenImage;
		mScreenImage = NULL;
	}
}

DeviceImage* DisplayInterface::GetScreenImage()
{
	return mScreenImage;
}

int DisplayInterface::Init(bool isWindowed)
{
	Cleanup();

	AutoCrit anAutoCrit(mCritSect);

	mIsWindowed = isWindowed;
	mWidth = mApp->mWidth;
	mHeight = mApp->mHeight;
	mAspect.Set(mWidth, mHeight);
	mDisplayWidth = mWidth;
	mDisplayHeight = mHeight;
	mPresentationRect = Rect(0, 0, mWidth, mHeight);

	if (!mDevice->SetSceneSize(mWidth, mHeight))
	{
		mErrorString = "Unable to create the scene render target.";
		return RESULT_SURFACE_FAIL;
	}

	mRenderer = new HardwareRenderer(mDevice, mWidth, mHeight);

	mScreenImage = new DeviceImage(this);
	mScreenImage->Create(mWidth, mHeight);
	mScreenImage->mPurgeBits = false;
	mScreenImage->SetImageMode(false, false);
	mScreenImage->mHasTrans = false;
	mScreenImage->mHasAlpha = false;
	if (!mIs3D)
		memset(mScreenImage->GetBits(), 0, mWidth * mHeight * sizeof(ulong));

	mScreenTexture = mDevice->CreateTexture(mWidth, mHeight);
	if (mScreenTexture == 0)
	{
		mErrorString = "Unable to create the screen texture.";
		return RESULT_SURFACE_FAIL;
	}

	UpdateViewport();

	mInitialized = true;
	return RESULT_OK;
}

void DisplayInterface::UpdateViewport()
{
	int aDrawableWidth = 0;
	int aDrawableHeight = 0;
	mDevice->GetDrawableSize(aDrawableWidth, aDrawableHeight);
	if (aDrawableWidth <= 0 || aDrawableHeight <= 0)
	{
		aDrawableWidth = mWidth;
		aDrawableHeight = mHeight;
	}

	// Fit the scene into the drawable, preserving its aspect ratio.
	double aScale = std::min((double)aDrawableWidth / mWidth, (double)aDrawableHeight / mHeight);
	int aWidth = std::max(1, (int)std::lround(mWidth * aScale));
	int aHeight = std::max(1, (int)std::lround(mHeight * aScale));
	mViewport.mX = (aDrawableWidth - aWidth) / 2;
	mViewport.mY = (aDrawableHeight - aHeight) / 2;
	mViewport.mWidth = aWidth;
	mViewport.mHeight = aHeight;

	// Integer multiples are shown with nearest filtering so pixels stay crisp.
	mLinearScaling = (aWidth % mWidth) != 0 || (aHeight % mHeight) != 0;
}

void DisplayInterface::WindowToLogical(float theWindowX, float theWindowY, float thePixelScale, int& theX, int& theY)
{
	float aPixelX = theWindowX * thePixelScale;
	float aPixelY = theWindowY * thePixelScale;

	if (mViewport.mWidth <= 0 || mViewport.mHeight <= 0)
	{
		theX = (int)aPixelX;
		theY = (int)aPixelY;
		return;
	}

	theX = (int)std::floor((aPixelX - mViewport.mX) * mWidth / mViewport.mWidth);
	theY = (int)std::floor((aPixelY - mViewport.mY) * mHeight / mViewport.mHeight);
}

bool DisplayInterface::Redraw(Rect* theClipRect)
{
	(void)theClipRect;

	AutoCrit anAutoCrit(mCritSect);

	if (!mInitialized)
		return false;

	mInRedraw = true;

	if (!mIs3D)
	{
		// Software rendering: the screen image holds the frame.
		ulong* aBits = mScreenImage->GetBits();
		mDevice->UpdateTexture(mScreenTexture, 0, 0, mWidth, mHeight, aBits, mWidth);
		mRenderer->BltScene(mScreenTexture, mWidth, mHeight);
	}

	mRenderer->Flush();

	mCursorX = mNextCursorX;
	mCursorY = mNextCursorY;

	UpdateViewport();
	mDevice->Present(mViewport, mLinearScaling, !mApp->mPhysMinimized && !mApp->mWindowOccluded);

	mInRedraw = false;
	return true;
}

void DisplayInterface::SetVideoOnlyDraw(bool videoOnly)
{
	(void)videoOnly;
}

bool DisplayInterface::SetCursorImage(Image* theImage)
{
	AutoCrit anAutoCrit(mCritSect);

	if (mCursorImage != theImage)
	{
		mCursorImage = theImage;
		return true;
	}

	return false;
}

void DisplayInterface::SetCursorPos(int theCursorX, int theCursorY)
{
	mNextCursorX = theCursorX;
	mNextCursorY = theCursorY;
}
