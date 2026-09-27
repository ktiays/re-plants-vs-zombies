//#define SEXY_TRACING_ENABLED
//#define SEXY_PERF_ENABLED
//#define SEXY_MEMTRACE

#include "SexyAppBase.h"
#include "widget/WidgetManager.h"
#include "widget/Widget.h"
#include "misc/Debug.h"
#include "misc/KeyCodes.h"
#include "graphics/DisplayInterface.h"
#include "graphics/HardwareRenderer.h"
#include "graphics/DeviceImage.h"
#include "graphics/MemoryImage.h"
#include "graphics/RenderDevice.h"
#include "widget/Dialog.h"
#include "imagelib/ImageLib.h"
#include "sound/AudioMixer.h"
#include "sound/MixerSoundManager.h"
#include "sound/ModuleMusicInterface.h"
#include "sound/SoundInstance.h"
#include "sound/DummyMusicInterface.h"
#include "misc/Rect.h"
#include "misc/PropertiesParser.h"
#include "misc/PerfTimer.h"
#include "misc/MTRand.h"
#include "misc/ModVal.h"
#include "graphics/SysFont.h"
#include "misc/ResourceManager.h"
#include "misc/AutoCrit.h"
#include "paklib/PakInterface.h"
#include "platform/Platform.h"

#if defined(SEXY_RENDER_BACKEND_METAL)
#include "graphics/metal/MetalRenderDevice.h"
#endif

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <time.h>
#include <unistd.h>

#include "misc/memmgr.h"

using namespace Sexy;

SexyAppBase* Sexy::gSexyAppBase = NULL;

bool gD3DInterfacePreDrawError = false;

//HotSpot: 11 4
//Size: 32 32
static unsigned char gFingerCursorData[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xe7, 0xff, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xff, 0xc3,
	0xff, 0xff, 0xff, 0xc3, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0x1f, 0xff, 0xff,
	0xc0, 0x07, 0xff, 0xff, 0xc0, 0x03, 0xff, 0xfc, 0x40, 0x01, 0xff, 0xfc, 0x00, 0x01, 0xff,
	0xfc, 0x00, 0x01, 0xff, 0xfc, 0x00, 0x01, 0xff, 0xff, 0x00, 0x01, 0xff, 0xff, 0x00, 0x01,
	0xff, 0xff, 0x80, 0x01, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0xc0, 0x03, 0xff, 0xff, 0xc0,
	0x03, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,
	0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
	0x18, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x1b, 0x60, 0x00, 0x00, 0x1b, 0x68, 0x00,
	0x00, 0x1b, 0x6c, 0x00, 0x01, 0x9f, 0xec, 0x00, 0x01, 0xdf, 0xfc, 0x00, 0x00, 0xdf, 0xfc,
	0x00, 0x00, 0x5f, 0xfc, 0x00, 0x00, 0x7f, 0xfc, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x3f,
	0xf8, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00,
	0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00
};

//HotSpot: 15 10
//Size: 32 32
static unsigned char gDraggingCursorData[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xfc, 0x0f, 0xff, 0xff, 0xf0, 0x07, 0xff, 0xff, 0xe0,
	0x01, 0xff, 0xff, 0xe0, 0x00, 0xff, 0xff, 0xe0, 0x00, 0xff, 0xff, 0xe0, 0x00, 0xff, 0xff,
	0xe0, 0x00, 0xff, 0xfe, 0x60, 0x00, 0xff, 0xfc, 0x20, 0x00, 0xff, 0xfc, 0x00, 0x00, 0xff,
	0xfe, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x80, 0x00,
	0xff, 0xff, 0x80, 0x01, 0xff, 0xff, 0xc0, 0x01, 0xff, 0xff, 0xe0, 0x01, 0xff, 0xff, 0xf0,
	0x03, 0xff, 0xff, 0xf8, 0x03, 0xff, 0xff, 0xf8, 0x03, 0xff, 0xff, 0xf8, 0x03, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x80, 0x00, 0x00, 0x01, 0xb0, 0x00, 0x00, 0x0d, 0xb0, 0x00, 0x00, 0x0d, 0xb6, 0x00, 0x00,
	0x0d, 0xb6, 0x00, 0x00, 0x0d, 0xb6, 0x00, 0x00, 0x0d, 0xb6, 0x00, 0x00, 0x0d, 0xb6, 0x00,
	0x01, 0x8d, 0xb6, 0x00, 0x01, 0xcf, 0xfe, 0x00, 0x00, 0xef, 0xfe, 0x00, 0x00, 0xff, 0xfe,
	0x00, 0x00, 0x7f, 0xfe, 0x00, 0x00, 0x3f, 0xfe, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x1f,
	0xfc, 0x00, 0x00, 0x0f, 0xfc, 0x00, 0x00, 0x07, 0xf8, 0x00, 0x00, 0x03, 0xf8, 0x00, 0x00,
	0x03, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00
};

static DeviceImage* gFPSImage = NULL;

// Creates an SDL cursor from a Win32 32x32 monochrome AND/XOR cursor bitmap.
static SDL_Cursor* CreateMonochromeCursor(const unsigned char* theData, int theHotX, int theHotY)
{
	const int aSize = 32 * 32 / 8;
	unsigned char aData[aSize];
	unsigned char aMask[aSize];
	const unsigned char* anAndPlane = theData;
	const unsigned char* anXorPlane = theData + aSize;

	for (int i = 0; i < aSize; i++)
	{
		// Win32: AND=0,XOR=0 black; AND=0,XOR=1 white; AND=1,XOR=0 transparent; AND=1,XOR=1 inverted.
		// SDL:   D=1,M=1 black;     D=0,M=1 white;     D=0,M=0 transparent;     D=1,M=0 inverted.
		aMask[i] = (unsigned char)~anAndPlane[i];
		aData[i] = (unsigned char)~(anAndPlane[i] ^ anXorPlane[i]);
	}

	return SDL_CreateCursor(aData, aMask, 32, 32, theHotX, theHotY);
}

// Maps an SDL key to the Windows virtual-key code the framework uses.
static int SDLKeyToKeyCode(SDL_Keycode theKey, SDL_Scancode theScancode)
{
	if (theKey >= SDLK_A && theKey <= SDLK_Z)
		return 'A' + (int)(theKey - SDLK_A);
	if (theKey >= SDLK_0 && theKey <= SDLK_9)
		return '0' + (int)(theKey - SDLK_0);
	if (theKey >= SDLK_F1 && theKey <= SDLK_F12)
		return KEYCODE_F1 + (int)(theKey - SDLK_F1);

	switch (theScancode)
	{
	case SDL_SCANCODE_KP_0:			return KEYCODE_NUMPAD0;
	case SDL_SCANCODE_KP_1:			return KEYCODE_NUMPAD1;
	case SDL_SCANCODE_KP_2:			return KEYCODE_NUMPAD2;
	case SDL_SCANCODE_KP_3:			return KEYCODE_NUMPAD3;
	case SDL_SCANCODE_KP_4:			return KEYCODE_NUMPAD4;
	case SDL_SCANCODE_KP_5:			return KEYCODE_NUMPAD5;
	case SDL_SCANCODE_KP_6:			return KEYCODE_NUMPAD6;
	case SDL_SCANCODE_KP_7:			return KEYCODE_NUMPAD7;
	case SDL_SCANCODE_KP_8:			return KEYCODE_NUMPAD8;
	case SDL_SCANCODE_KP_9:			return KEYCODE_NUMPAD9;
	case SDL_SCANCODE_KP_MULTIPLY:	return KEYCODE_MULTIPLY;
	case SDL_SCANCODE_KP_PLUS:		return KEYCODE_ADD;
	case SDL_SCANCODE_KP_MINUS:		return KEYCODE_SUBTRACT;
	case SDL_SCANCODE_KP_PERIOD:	return KEYCODE_DECIMAL;
	case SDL_SCANCODE_KP_DIVIDE:	return KEYCODE_DIVIDE;
	case SDL_SCANCODE_KP_ENTER:		return KEYCODE_RETURN;
	default:						break;
	}

	switch (theKey)
	{
	case SDLK_BACKSPACE:	return KEYCODE_BACK;
	case SDLK_TAB:			return KEYCODE_TAB;
	case SDLK_CLEAR:		return KEYCODE_CLEAR;
	case SDLK_RETURN:		return KEYCODE_RETURN;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:		return KEYCODE_SHIFT;
	case SDLK_LCTRL:
	case SDLK_RCTRL:		return KEYCODE_CONTROL;
	case SDLK_LALT:
	case SDLK_RALT:			return KEYCODE_MENU;
	case SDLK_PAUSE:		return KEYCODE_PAUSE;
	case SDLK_CAPSLOCK:		return KEYCODE_CAPITAL;
	case SDLK_ESCAPE:		return KEYCODE_ESCAPE;
	case SDLK_SPACE:		return KEYCODE_SPACE;
	case SDLK_PAGEUP:		return KEYCODE_PRIOR;
	case SDLK_PAGEDOWN:		return KEYCODE_NEXT;
	case SDLK_END:			return KEYCODE_END;
	case SDLK_HOME:			return KEYCODE_HOME;
	case SDLK_LEFT:			return KEYCODE_LEFT;
	case SDLK_UP:			return KEYCODE_UP;
	case SDLK_RIGHT:		return KEYCODE_RIGHT;
	case SDLK_DOWN:			return KEYCODE_DOWN;
	case SDLK_PRINTSCREEN:	return KEYCODE_SNAPSHOT;
	case SDLK_INSERT:		return KEYCODE_INSERT;
	case SDLK_DELETE:		return KEYCODE_DELETE;
	case SDLK_HELP:			return KEYCODE_HELP;
	case SDLK_LGUI:			return KEYCODE_LWIN;
	case SDLK_RGUI:			return KEYCODE_RWIN;
	case SDLK_APPLICATION:	return KEYCODE_APPS;
	case SDLK_NUMLOCKCLEAR:	return 0x90;
	case SDLK_SCROLLLOCK:	return 0x91;
	case SDLK_SEMICOLON:	return 0xBA;
	case SDLK_EQUALS:		return 0xBB;
	case SDLK_COMMA:		return 0xBC;
	case SDLK_MINUS:		return 0xBD;
	case SDLK_PERIOD:		return 0xBE;
	case SDLK_SLASH:		return 0xBF;
	case SDLK_GRAVE:		return 0xC0;
	case SDLK_LEFTBRACKET:	return 0xDB;
	case SDLK_BACKSLASH:	return 0xDC;
	case SDLK_RIGHTBRACKET:	return 0xDD;
	case SDLK_APOSTROPHE:	return 0xDE;
	default:				return KEYCODE_UNKNOWN;
	}
}

//////////////////////////////////////////////////////////////////////////

SexyAppBase::SexyAppBase()
{
	gSexyAppBase = this;

	mOnlyAllowOneCopyToRun = false;
	mProductVersion = "1.2.0.1073";
	mChangeDirTo = ".";

	mNoDefer = false;
	mFullScreenPageFlip = true;
	mTimeLoaded = GetTickCount();
	mSEHOccured = false;
	mProdName = "Product";
	mTitle = _S("SexyApp");
	mShutdown = false;
	mExitToTop = false;
	mWidth = 640;
	mHeight = 480;
	mFullscreenBits = 32;
	mIsWindowed = true;
	mIsPhysWindowed = true;
	mFullScreenWindow = false;
	mPreferredX = -1;
	mPreferredY = -1;
	mIsScreenSaver = false;
	mAllowMonitorPowersave = true;
	mWindow = NULL;
	mRenderDevice = NULL;
	mDisplay = NULL;
	mAudioMixer = NULL;
	mAudioDevice = NULL;
	mMusicInterface = NULL;
	mFrameTime = 10;
	mNonDrawCount = 0;
	mDrawCount = 0;
	mSleepCount = 0;
	mUpdateCount = 0;
	mUpdateAppState = 0;
	mUpdateAppDepth = 0;
	mPendingUpdatesAcc = 0.0;
	mUpdateFTimeAcc = 0.0;
	mHasPendingDraw = true;
	mIsDrawing = false;
	mLastDrawWasEmpty = false;
	mLastTimeCheck = 0;
	mUpdateMultiplier = 1;
	mPaused = false;
	mFastForwardToUpdateNum = 0;
	mFastForwardToMarker = false;
	mFastForwardStep = false;
	mSoundManager = NULL;
	mCursorNum = CURSOR_POINTER;
	mMouseIn = false;
	mRunning = false;
	mActive = true;
	mProcessInTimer = false;
	mMinimized = false;
	mPhysMinimized = false;
	mWindowOccluded = false;
	mIsDisabled = false;
	mLoaded = false;
	mYieldMainThread = false;
	mLoadingFailed = false;
	mLoadingThreadStarted = false;
	mAutoStartLoadingThread = true;
	mLoadingThreadCompleted = false;
	mCursorThreadRunning = false;
	mNumLoadingThreadTasks = 0;
	mCompletedLoadingThreadTasks = 0;
	mLastDrawTick = timeGetTime();
	mNextDrawTick = timeGetTime();
	mSysCursor = true;
	mForceFullscreen = false;
	mForceWindowed = false;
	mHasFocus = true;
	mCustomCursorsEnabled = false;
	mCustomCursorDirty = false;
	mCustomCursor = NULL;
	mCustomCursorImage = NULL;
	mIsOpeningURL = false;
	mShutdownOnURLOpen = false;
	mOpeningURLTime = 0;
	mLastTimerTime = 0;
	mLastBigDelayTime = 0;
	mInitialized = false;
	mLastShutdownWasGraceful = true;
	mReadFromRegistry = false;
	mCmdLineParsed = false;
	mSkipSignatureChecks = false;
	mCtrlDown = false;
	mAltDown = false;
	mAllowAltEnter = true;
	mStepMode = 0;
	mCleanupSharedImages = false;
	mStandardWordWrap = true;
	mbAllowExtendedChars = true;
	mEnableMaximizeButton = false;
	mWriteToSexyCache = true;
	mSexyCacheBuffers = false;
	mBetaValidate = false;
	mLastUserInputTick = 0;
	mLastTime = 0;

	mMusicVolume = 0.85;
	mSfxVolume = 0.85;
	mUnmutedMusicVolume = mMusicVolume;
	mUnmutedSfxVolume = mSfxVolume;
	mMuteCount = 0;
	mAutoMuteCount = 0;
	mDemoMute = false;
	mMuteOnLostFocus = true;
	mFPSTime = 0;
	mFPSStartTick = GetTickCount();
	mFPSFlipCount = 0;
	mFPSCount = 0;
	mFPSDirtyCount = 0;
	mShowFPS = false;
	mShowFPSMode = FPS_ShowFPS;
	mDrawTime = 0;
	mScreenBltTime = 0;
	mAlphaDisabled = false;
	mDebugKeysEnabled = false;
	mNoSoundNeeded = false;
	mWantFMod = false;

	mSyncRefreshRate = 100;
	mVSyncUpdates = false;
	mVSyncBroken = false;
	mVSyncBrokenCount = 0;
	mVSyncBrokenTestStartTick = 0;
	mVSyncBrokenTestUpdates = 0;
	mWaitForVSync = false;
	mSoftVSyncWait = true;
	mUserChanged3DSetting = false;
	mAutoEnable3D = false;
	mTest3D = false;
	mMinVidMemory3D = 6;
	mRecommendedVidMemory3D = 14;
	mRelaxUpdateBacklogCount = 0;
	mWidescreenAware = false;
	mEnableWindowAspect = false;
	mWindowAspect.Set(4, 3);
	mIsWideWindow = false;
	mTabletPC = false;
	mWheelAccumulator = 0.0f;

	mCaptureIntervalMs = 0;
	mLastCaptureTick = 0;
	mCaptureCount = 0;
	mInputScriptPos = 0;
	mInputScriptWaitUntil = 0;
	mForceSoftwareRendering = false;
	mCmdLineWindowMode = 0;

	mRecordingDemoBuffer = false;
	mPlayingDemoBuffer = false;
	mManualShutdown = false;
	mDemoPrefix = "sexyapp";
	mDemoFileName = mDemoPrefix + ".dmo";

	int i;

	for (i = 0; i < NUM_CURSORS; i++)
	{
		mCursorImages[i] = NULL;
		mSystemCursors[i] = NULL;
	}

	for (i = 0; i < 256; i++)
		mAdd8BitMaxTable[i] = i;

	for (i = 256; i < 512; i++)
		mAdd8BitMaxTable[i] = 255;

	// Set default strings.  Init could read in overrides from partner.xml
	SetString("DIALOG_BUTTON_OK",		L"OK");
	SetString("DIALOG_BUTTON_CANCEL",	L"CANCEL");

	SetString("UPDATE_CHECK_TITLE",		L"Update Check");
	SetString("UPDATE_CHECK_BODY",		L"Checking if there are any updates available for this product ...");

	SetString("UP_TO_DATE_TITLE",		L"Up to Date");
	SetString("UP_TO_DATE_BODY",		L"There are no updates available for this product at this time.");
	SetString("NEW_VERSION_TITLE",		L"New Version");
	SetString("NEW_VERSION_BODY",		L"There is an update available for this product.  Would you like to visit the web site to download it?");

	mWidgetManager = new WidgetManager(this);
	mResourceManager = new ResourceManager(this);

	mPrimaryThreadId = 0;
}

SexyAppBase::~SexyAppBase()
{
	Shutdown();

	if (mUserChanged3DSetting && mDisplay != NULL)
		RegistryWriteBoolean("Is3D", mDisplay->mIs3D);

	DialogMap::iterator aDialogItr = mDialogMap.begin();
	while (aDialogItr != mDialogMap.end())
	{
		mWidgetManager->RemoveWidget(aDialogItr->second);
		delete aDialogItr->second;
		++aDialogItr;
	}
	mDialogMap.clear();
	mDialogList.clear();

	WaitForLoadingThread();

	delete mWidgetManager;
	delete mResourceManager;
	delete gFPSImage;
	gFPSImage = NULL;

	SharedImageMap::iterator aSharedImageItr = mSharedImageMap.begin();
	while (aSharedImageItr != mSharedImageMap.end())
	{
		SharedImage* aSharedImage = &aSharedImageItr->second;
		DBG_ASSERTE(aSharedImage->mRefCount == 0);
		delete aSharedImage->mImage;
		mSharedImageMap.erase(aSharedImageItr++);
	}

	// Stop audio output before the sound objects it reads go away.
	delete mAudioDevice;
	mAudioDevice = NULL;
	delete mMusicInterface;
	mMusicInterface = NULL;
	delete mSoundManager;
	mSoundManager = NULL;
	delete mAudioMixer;
	mAudioMixer = NULL;

	delete mDisplay;
	mDisplay = NULL;
	delete mRenderDevice;
	mRenderDevice = NULL;

	for (int i = 0; i < NUM_CURSORS; i++)
	{
		if (mSystemCursors[i] != NULL)
			SDL_DestroyCursor(mSystemCursors[i]);
	}
	if (mCustomCursor != NULL)
		SDL_DestroyCursor(mCustomCursor);

	if (mWindow != NULL)
	{
		SDL_DestroyWindow(mWindow);
		mWindow = NULL;
	}

	gSexyAppBase = NULL;

	SDL_Quit();
}

bool SexyAppBase::OpenURL(const std::string& theURL, bool shutdownOnOpen)
{
	if ((!mIsOpeningURL) || (theURL != mOpeningURL))
	{
		mShutdownOnURLOpen = shutdownOnOpen;
		mIsOpeningURL = true;
		mOpeningURL = theURL;
		mOpeningURLTime = GetTickCount();

		if (Platform::OpenURL(theURL))
		{
			// There is no reliable way to learn whether the browser came up;
			// report success immediately.
			URLOpenSucceeded(theURL);
			return true;
		}
		else
		{
			URLOpenFailed(theURL);
			return false;
		}
	}

	return true;
}

std::string SexyAppBase::GetProductVersion(const std::string& thePath)
{
	(void)thePath;
	return mProductVersion;
}

void SexyAppBase::TakeScreenshot()
{
	if (mDisplay == NULL || mDisplay->GetScreenImage() == NULL)
		return;

	std::string anImageDir = GetAppDataFolder() + "_screenshots";
	MkDir(anImageDir);
	anImageDir += "/";

	int aMaxId = 0;
	std::string anImagePrefix = "image";
	std::error_code anError;
	for (const auto& anEntry : std::filesystem::directory_iterator(anImageDir, anError))
	{
		int aNum = 0;
		if (sscanf(anEntry.path().filename().string().c_str(), (anImagePrefix + "%d.png").c_str(), &aNum) == 1)
			aMaxId = std::max(aMaxId, aNum);
	}

	WriteCapture(anImageDir + anImagePrefix + StrFormat("%d.png", aMaxId + 1));
}

void SexyAppBase::WriteCapture(const std::string& theFileName)
{
	DeviceImage* aScreen = mDisplay != NULL ? mDisplay->GetScreenImage() : NULL;
	if (aScreen == NULL)
		return;

	ulong* aBits = aScreen->GetBits();
	if (aBits == NULL)
		return;

	std::vector<ulong> anOpaque(aBits, aBits + aScreen->mWidth * aScreen->mHeight);
	for (ulong& aPixel : anOpaque)
		aPixel |= 0xFF000000;

	ImageLib::Image aSaveImage;
	aSaveImage.mBits = anOpaque.data();
	aSaveImage.mWidth = aScreen->mWidth;
	aSaveImage.mHeight = aScreen->mHeight;
	ImageLib::WritePNGImage(theFileName, &aSaveImage);
	aSaveImage.mBits = NULL;
}

void SexyAppBase::DumpProgramInfo()
{
}

// Input scripts drive the game without a person at the keyboard, for
// automated testing. One command per line, run in order:
//   wait <ms>            pause before the next command
//   move <x> <y>         move the mouse (logical coordinates)
//   click <x> <y>        left click; "rclick" for the right button
//   wclick <x> <y>       left click through the window system (window points)
//   wkey <sdl keycode>   key press through the window system
//   key <keycode>        press and release a key (Windows virtual-key code)
//   text <string>        type characters
//   capture <file.png>   write the screen to a file
//   quit                 close the game
// Lines starting with '#' are comments.
void SexyAppBase::LoadInputScript(const std::string& theFileName)
{
	mInputScript.clear();
	mInputScriptPos = 0;
	mInputScriptWaitUntil = 0;

	FILE* aFile = fopen(FixPath(theFileName).c_str(), "r");
	if (aFile == NULL)
	{
		OutputDebugString(("Unable to open input script " + theFileName + "\n").c_str());
		return;
	}

	char aLine[1024];
	while (fgets(aLine, sizeof(aLine), aFile) != NULL)
	{
		std::string aString = aLine;
		inlineTrim(aString);
		if (!aString.empty() && aString[0] != '#')
			mInputScript.push_back(aString);
	}
	fclose(aFile);
}

void SexyAppBase::RunInputScript()
{
	DWORD aNow = GetTickCount();
	while (mInputScriptPos < mInputScript.size() && (int)(aNow - mInputScriptWaitUntil) >= 0)
	{
		const std::string& aLine = mInputScript[mInputScriptPos++];
		char aCommand[64] = {0};
		int x = 0, y = 0;
		sscanf(aLine.c_str(), "%63s %d %d", aCommand, &x, &y);
		std::string anArg = aLine.size() > strlen(aCommand) ? aLine.substr(strlen(aCommand) + 1) : std::string();

		if (strcmp(aCommand, "wait") == 0)
		{
			mInputScriptWaitUntil = aNow + (DWORD)x;
		}
		else if (strcmp(aCommand, "move") == 0)
		{
			mWidgetManager->MouseMove(x, y);
		}
		else if (strcmp(aCommand, "click") == 0 || strcmp(aCommand, "rclick") == 0)
		{
			int aButton = aCommand[0] == 'r' ? -1 : 1;
			mMouseIn = true;
			mWidgetManager->MouseMove(x, y);
			mWidgetManager->MouseDown(x, y, aButton);
			mWidgetManager->MouseUp(x, y, aButton);
		}
		else if (strcmp(aCommand, "wclick") == 0)
		{
			// A click delivered through the window system, in window points,
			// to exercise the same path as a real mouse.
			SDL_Event anEvent = {};
			anEvent.type = SDL_EVENT_MOUSE_MOTION;
			anEvent.motion.windowID = SDL_GetWindowID(mWindow);
			anEvent.motion.x = (float)x;
			anEvent.motion.y = (float)y;
			SDL_PushEvent(&anEvent);

			for (int i = 0; i < 2; i++)
			{
				anEvent = {};
				anEvent.type = i == 0 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
				anEvent.button.windowID = SDL_GetWindowID(mWindow);
				anEvent.button.button = SDL_BUTTON_LEFT;
				anEvent.button.down = i == 0;
				anEvent.button.clicks = 1;
				anEvent.button.x = (float)x;
				anEvent.button.y = (float)y;
				SDL_PushEvent(&anEvent);
			}
		}
		else if (strcmp(aCommand, "wkey") == 0)
		{
			// A key press delivered through the window system (SDL keycode).
			for (int i = 0; i < 2; i++)
			{
				SDL_Event anEvent = {};
				anEvent.type = i == 0 ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
				anEvent.key.windowID = SDL_GetWindowID(mWindow);
				anEvent.key.key = (SDL_Keycode)x;
				anEvent.key.scancode = SDL_GetScancodeFromKey((SDL_Keycode)x, NULL);
				anEvent.key.down = i == 0;
				SDL_PushEvent(&anEvent);
			}
		}
		else if (strcmp(aCommand, "key") == 0)
		{
			mWidgetManager->KeyDown((KeyCode)x);
			if (x == KEYCODE_RETURN || x == KEYCODE_ESCAPE || x == KEYCODE_BACK)
				mWidgetManager->KeyChar((SexyChar)(x == KEYCODE_RETURN ? 13 : x == KEYCODE_ESCAPE ? 27 : 8));
			mWidgetManager->KeyUp((KeyCode)x);
		}
		else if (strcmp(aCommand, "text") == 0)
		{
			for (char c : anArg)
				mWidgetManager->KeyChar(c);
		}
		else if (strcmp(aCommand, "capture") == 0)
		{
			WriteCapture(anArg);
		}
		else if (strcmp(aCommand, "quit") == 0)
		{
			CloseRequestAsync();
			Shutdown();
			return;
		}
	}
}

void SexyAppBase::ShowMemoryUsage()
{
	int aTotal = 0;
	int aCount = 0;
	{
		AutoCrit anAutoCrit(mCritSect);
		for (MemoryImage* anImage : mMemoryImageSet)
		{
			aTotal += anImage->mWidth * anImage->mHeight * 4;
			aCount++;
		}
	}

	MsgBox(StrFormat("Images: %d\nApproximate image memory: %d KB", aCount, aTotal / 1024), "Video Stats", MB_OK);
}

void SexyAppBase::SplitRegistryValueName(const std::string& theValueName, std::string& theKey, std::string& theName)
{
	theKey = mRegKey;
	int aSlashPos = (int) theValueName.rfind('\\');
	if (aSlashPos != -1)
	{
		theKey += "\\" + theValueName.substr(0, aSlashPos);
		theName = theValueName.substr(aSlashPos + 1);
	}
	else
	{
		theName = theValueName;
	}
}

bool SexyAppBase::RegistryWrite(const std::string& theValueName, ulong theType, const uchar* theValue, ulong theLength)
{
	if (mRegKey.length() == 0)
		return false;

	std::string aKeyName, aValueName;
	SplitRegistryValueName(theValueName, aKeyName, aValueName);
	return mRegistry.Write(aKeyName, aValueName, theType, theValue, theLength);
}

bool SexyAppBase::RegistryWriteString(const std::string& theValueName, const std::string& theString)
{
	return RegistryWrite(theValueName, REGISTRY_TYPE_STRING, (uchar*) theString.c_str(), theString.length());
}

bool SexyAppBase::RegistryWriteInteger(const std::string& theValueName, int theValue)
{
	std::int32_t aValue = theValue;
	return RegistryWrite(theValueName, REGISTRY_TYPE_DWORD, (uchar*) &aValue, sizeof(aValue));
}

bool SexyAppBase::RegistryWriteBoolean(const std::string& theValueName, bool theValue)
{
	std::int32_t aValue = theValue ? 1 : 0;
	return RegistryWrite(theValueName, REGISTRY_TYPE_DWORD, (uchar*) &aValue, sizeof(aValue));
}

bool SexyAppBase::RegistryWriteData(const std::string& theValueName, const uchar* theValue, ulong theLength)
{
	return RegistryWrite(theValueName, REGISTRY_TYPE_BINARY, (uchar*) theValue, theLength);
}

void SexyAppBase::WriteToRegistry()
{
	RegistryWriteInteger("MusicVolume", (int) (mMusicVolume * 100));
	RegistryWriteInteger("SfxVolume", (int) (mSfxVolume * 100));
	RegistryWriteInteger("Muted", (mMuteCount - mAutoMuteCount > 0) ? 1 : 0);
	RegistryWriteInteger("ScreenMode", mIsWindowed ? 0 : 1);
	RegistryWriteInteger("PreferredX", mPreferredX);
	RegistryWriteInteger("PreferredY", mPreferredY);
	RegistryWriteInteger("CustomCursors", mCustomCursorsEnabled ? 1 : 0);
	RegistryWriteInteger("InProgress", 0);
	RegistryWriteBoolean("WaitForVSync", mWaitForVSync);
}

bool SexyAppBase::RegistryEraseKey(const SexyString& _theKeyName)
{
	std::string theKeyName = SexyStringToStringFast(_theKeyName);
	if (mRegKey.length() == 0)
		return false;

	return mRegistry.EraseKey(mRegKey + "\\" + theKeyName);
}

void SexyAppBase::RegistryEraseValue(const SexyString& _theValueName)
{
	std::string theValueName = SexyStringToStringFast(_theValueName);
	if (mRegKey.length() == 0)
		return;

	std::string aKeyName, aValueName;
	SplitRegistryValueName(theValueName, aKeyName, aValueName);
	mRegistry.EraseValue(aKeyName, aValueName);
}

bool SexyAppBase::RegistryGetSubKeys(const std::string& theKeyName, StringVector* theSubKeys)
{
	theSubKeys->clear();

	if (mRegKey.length() == 0)
		return false;

	return mRegistry.GetSubKeys(mRegKey + "\\" + theKeyName, *theSubKeys);
}

bool SexyAppBase::RegistryRead(const std::string& theValueName, ulong* theType, uchar* theValue, ulong* theLength)
{
	if (mRegKey.length() == 0)
		return false;

	std::string aKeyName, aValueName;
	SplitRegistryValueName(theValueName, aKeyName, aValueName);

	RegistryStore::Value aValue;
	if (!mRegistry.Read(aKeyName, aValueName, aValue))
		return false;

	*theType = aValue.mType;
	if (aValue.mData.size() > *theLength)
	{
		*theLength = (ulong)aValue.mData.size();
		return false;
	}

	*theLength = (ulong)aValue.mData.size();
	if (!aValue.mData.empty())
		memcpy(theValue, aValue.mData.data(), aValue.mData.size());
	return true;
}

bool SexyAppBase::RegistryReadString(const std::string& theKey, std::string* theString)
{
	char aStr[1024];

	ulong aType;
	ulong aLen = sizeof(aStr) - 1;
	if (!RegistryRead(theKey, &aType, (uchar*) aStr, &aLen))
		return false;

	if (aType != REGISTRY_TYPE_STRING)
		return false;

	aStr[aLen] = 0;

	*theString = aStr;
	return true;
}

bool SexyAppBase::RegistryReadInteger(const std::string& theKey, int* theValue)
{
	ulong aType;
	std::int32_t aLong = 0;
	ulong aLen = 4;
	if (!RegistryRead(theKey, &aType, (uchar*) &aLong, &aLen))
		return false;

	if (aType != REGISTRY_TYPE_DWORD)
		return false;

	*theValue = aLong;
	return true;
}

bool SexyAppBase::RegistryReadBoolean(const std::string& theKey, bool* theValue)
{
	int aValue;
	if (!RegistryReadInteger(theKey, &aValue))
		return false;

	*theValue = aValue != 0;
	return true;
}

bool SexyAppBase::RegistryReadData(const std::string& theKey, uchar* theValue, ulong* theLength)
{
	ulong aType;
	if (!RegistryRead(theKey, &aType, (uchar*) theValue, theLength))
		return false;

	if (aType != REGISTRY_TYPE_BINARY)
		return false;

	return true;
}

void SexyAppBase::ReadFromRegistry()
{
	mReadFromRegistry = true;
	mRegKey = SexyStringToString(GetString("RegistryKey", StringToSexyString(mRegKey)));

	if (mRegKey.length() == 0)
		return;

	int anInt;
	if (RegistryReadInteger("MusicVolume", &anInt))
		mMusicVolume = anInt / 100.0;

	if (RegistryReadInteger("SfxVolume", &anInt))
		mSfxVolume = anInt / 100.0;

	if (RegistryReadInteger("Muted", &anInt))
		mMuteCount = anInt;

	if (RegistryReadInteger("ScreenMode", &anInt))
		mIsWindowed = anInt == 0;

	RegistryReadInteger("PreferredX", &mPreferredX);
	RegistryReadInteger("PreferredY", &mPreferredY);

	if (RegistryReadInteger("CustomCursors", &anInt))
		EnableCustomCursors(anInt != 0);

	RegistryReadBoolean("WaitForVSync", &mWaitForVSync);

	if (RegistryReadInteger("InProgress", &anInt))
		mLastShutdownWasGraceful = anInt == 0;

	if (!IsScreenSaver())
		RegistryWriteInteger("InProgress", 1);
}

bool SexyAppBase::WriteBytesToFile(const std::string& theFileName, const void *theData, unsigned long theDataLen)
{
	MkDir(GetFileDir(theFileName));
	FILE* aFP = fopen(FixPath(theFileName).c_str(), "w+b");

	if (aFP == NULL)
		return false;

	fwrite(theData, 1, theDataLen, aFP);
	fclose(aFP);
	return true;
}

bool SexyAppBase::ReadBufferFromFile(const std::string& theFileName, Buffer* theBuffer, bool dontWriteToDemo)
{
	(void)dontWriteToDemo;

	std::vector<unsigned char> aData;
	if (!PakReadFile(theFileName, aData))
		return false;

	theBuffer->Clear();
	theBuffer->SetData(aData.data(), (int)aData.size());
	return true;
}

bool SexyAppBase::FileExists(const std::string& theFileName)
{
	PFILE* aFP = p_fopen(theFileName.c_str(), "rb");
	if (aFP == NULL)
		return false;

	p_fclose(aFP);
	return true;
}

bool SexyAppBase::EraseFile(const std::string& theFileName)
{
	std::error_code anError;
	return std::filesystem::remove(FixPath(theFileName), anError);
}

void SexyAppBase::SEHOccured()
{
	SetMusicVolume(0);
	if (mWindow != NULL)
		SDL_HideWindow(mWindow);
	mSEHOccured = true;
	EnforceCursor();
}

std::string SexyAppBase::GetGameSEHInfo()
{
	int aSecLoaded = (GetTickCount() - mTimeLoaded) / 1000;

	char aTimeStr[16];
	snprintf(aTimeStr, sizeof(aTimeStr), "%02d:%02d:%02d", (aSecLoaded/60/60), (aSecLoaded/60)%60, aSecLoaded%60);

	std::string anInfoString =
		"Product: " + mProdName + "\r\n" +
		"Version: " + mProductVersion + "\r\n";

	anInfoString +=
		"Time Loaded: " + std::string(aTimeStr) + "\r\n"
		"Fullscreen: " + (mIsWindowed ? std::string("No") : std::string("Yes")) + "\r\n";

	return anInfoString;
}

void SexyAppBase::GetSEHWebParams(DefinesMap*){}

void SexyAppBase::ShutdownHook()
{
}

void SexyAppBase::Shutdown()
{
	if ((mPrimaryThreadId != 0) && (GetCurrentThreadId() != mPrimaryThreadId))
	{
		mLoadingFailed = true;
	}
	else if (!mShutdown)
	{
		mExitToTop = true;
		mShutdown = true;
		ShutdownHook();

		if (mMusicInterface != NULL)
			mMusicInterface->StopAllMusic();

		if (mWindow != NULL)
			SDL_HideWindow(mWindow);

		RestoreScreenResolution();

		if (mReadFromRegistry)
			WriteToRegistry();
	}
}

void SexyAppBase::RestoreScreenResolution()
{
	mFullScreenWindow = false;
}

void SexyAppBase::DoExit(int theCode)
{
	RestoreScreenResolution();
	exit(theCode);
}

bool SexyAppBase::DoUpdateFrames()
{
	SEXY_AUTO_PERF("SexyAppBase::DoUpdateFrames");

	if ((mLoadingThreadCompleted) && (!mLoaded))
	{
		mLoaded = true;
		mYieldMainThread = false;
		LoadingThreadCompleted();
	}

	UpdateFrames();
	return true;
}

void SexyAppBase::Redraw(Rect* theClipRect)
{
	SEXY_AUTO_PERF("SexyAppBase::Redraw");

	// Do mIsDrawing check because we could enter here at a bad time if any windows messages
	//  are processed during WidgetManager->Draw
	if ((mIsDrawing) || (mShutdown))
		return;

	mDisplay->Redraw(theClipRect);
	mFPSFlipCount++;

	if (!mCaptureDir.empty() && mCaptureIntervalMs > 0)
	{
		DWORD aTick = GetTickCount();
		if (aTick - mLastCaptureTick >= (DWORD)mCaptureIntervalMs)
		{
			mLastCaptureTick = aTick;
			WriteCapture(mCaptureDir + StrFormat("/capture%05d.png", mCaptureCount++));
		}
	}
}

///////////////////////////// FPS Stuff
static PerfTimer gFPSTimer;
static int gFrameCount;
static int gFPSDisplay;
static bool gForceDisplay = false;
static void CalculateFPS()
{
	gFrameCount++;

	static SysFont aFont(gSexyAppBase,"Tahoma",8);
	if (gFPSImage==NULL)
	{
		gFPSImage = new DeviceImage(gSexyAppBase->mDisplay);
		gFPSImage->Create(50,aFont.GetHeight()+4);
		gFPSImage->SetImageMode(false,false);
		gFPSImage->SetVolatile(true);
		gFPSImage->mPurgeBits = false;
	}

	if (gFPSTimer.GetDuration() >= 1000 || gForceDisplay)
	{
		gFPSTimer.Stop();
		if (!gForceDisplay)
			gFPSDisplay = (int)(gFrameCount*1000/gFPSTimer.GetDuration() + 0.5f);
		else
		{
			gForceDisplay = false;
			gFPSDisplay = 0;
		}

		gFPSTimer.Start();
		gFrameCount = 0;

		Graphics aDrawG(gFPSImage);
		aDrawG.SetFont(&aFont);
		SexyString aFPS = StrFormat(_S("FPS: %d"), gFPSDisplay);
		aDrawG.SetColor(0x000000);
		aDrawG.FillRect(0,0,gFPSImage->GetWidth(),gFPSImage->GetHeight());
		aDrawG.SetColor(0xFFFFFF);
		aDrawG.DrawString(aFPS,2,aFont.GetAscent());
		gFPSImage->mBitsChangedCount++;
	}
}

///////////////////////////// FPS Stuff to draw mouse coords
static void FPSDrawCoords(int theX, int theY)
{
	static SysFont aFont(gSexyAppBase,"Tahoma",8);
	if (gFPSImage==NULL)
	{
		gFPSImage = new DeviceImage(gSexyAppBase->mDisplay);
		gFPSImage->Create(50,aFont.GetHeight()+4);
		gFPSImage->SetImageMode(false,false);
		gFPSImage->SetVolatile(true);
		gFPSImage->mPurgeBits = false;
	}

	Graphics aDrawG(gFPSImage);
	aDrawG.SetFont(&aFont);
	SexyString aFPS = StrFormat(_S("%d,%d"),theX,theY);
	aDrawG.SetColor(0x000000);
	aDrawG.FillRect(0,0,gFPSImage->GetWidth(),gFPSImage->GetHeight());
	aDrawG.SetColor(0xFFFFFF);
	aDrawG.DrawString(aFPS,2,aFont.GetAscent());
	gFPSImage->mBitsChangedCount++;
}

bool SexyAppBase::DrawDirtyStuff()
{
	SEXY_AUTO_PERF("SexyAppBase::DrawDirtyStuff");
	MTAutoDisallowRand aDisallowRand;

	if (mShowFPS)
	{
		switch(mShowFPSMode)
		{
			case FPS_ShowFPS: CalculateFPS(); break;
			case FPS_ShowCoords:
				if (mWidgetManager!=NULL)
					FPSDrawCoords(mWidgetManager->mLastMouseX, mWidgetManager->mLastMouseY);
				break;
		}
	}

	DWORD aStartTime = timeGetTime();

	mIsDrawing = true;
	bool drewScreen = mWidgetManager->DrawScreen();
	mIsDrawing = false;

	if ((drewScreen || (aStartTime - mLastDrawTick >= 1000) || (mCustomCursorDirty)) &&
		((int) (aStartTime - mNextDrawTick) >= 0))
	{
		mLastDrawWasEmpty = false;

		mDrawCount++;

		DWORD aMidTime = timeGetTime();

		mFPSCount++;
		mFPSTime += aMidTime - aStartTime;

		mDrawTime += aMidTime - aStartTime;

		if (mShowFPS)
		{
			Graphics g(mDisplay->GetScreenImage());
			g.DrawImage(gFPSImage,mWidth-gFPSImage->GetWidth()-10,mHeight-gFPSImage->GetHeight()-10);
		}

		DWORD aPreScreenBltTime = timeGetTime();
		mLastDrawTick = aPreScreenBltTime;

		Redraw(NULL);

		// This is our one UpdateFTimeAcc if we are vsynched
		UpdateFTimeAcc();

		DWORD aEndTime = timeGetTime();

		mScreenBltTime = aEndTime - aPreScreenBltTime;

		if ((mLoadingThreadStarted) && (!mLoadingThreadCompleted))
		{
			int aTotalTime = aEndTime - aStartTime;

			mNextDrawTick += 35 + std::max(aTotalTime, 15);

			if ((int) (aEndTime - mNextDrawTick) >= 0)
				mNextDrawTick = aEndTime;
		}
		else
			mNextDrawTick = aEndTime;

		mHasPendingDraw = false;
		mCustomCursorDirty = false;

		return true;
	}
	else
	{
		// Anything drawn into the scene must still be submitted.
		if (drewScreen && Is3DAccelerated())
			mDisplay->mRenderer->Flush();

		mHasPendingDraw = false;
		mLastDrawWasEmpty = true;
		return false;
	}
}

void SexyAppBase::LogScreenSaverError(const std::string &theError)
{
	(void)theError;
}

void SexyAppBase::BeginPopup()
{
	if (mMouseIn)
		SDL_ShowCursor();
}

void SexyAppBase::EndPopup()
{
	ClearUpdateBacklog();
	ClearKeysDown();

	if (mWidgetManager->mDownButtons)
	{
		mWidgetManager->DoMouseUps();
		SDL_CaptureMouse(false);
	}

	EnforceCursor();
}

int SexyAppBase::MsgBox(const std::string& theText, const std::string& theTitle, int theFlags)
{
	BeginPopup();
	int aResult = Platform::ShowMessageBox(theTitle, theText, theFlags);
	EndPopup();

	return aResult;
}

int SexyAppBase::MsgBox(const std::wstring& theText, const std::wstring& theTitle, int theFlags)
{
	return MsgBox(WStringToString(theText), WStringToString(theTitle), theFlags);
}

void SexyAppBase::Popup(const std::string& theString)
{
	MsgBox(theString, SexyStringToString(GetString("FATAL_ERROR", _S("FATAL ERROR"))), MB_OK | MB_ICONERROR);
}

void SexyAppBase::Popup(const std::wstring& theString)
{
	Popup(WStringToString(theString));
}

void SexyAppBase::SafeDeleteWidget(Widget* theWidget)
{
	WidgetSafeDeleteInfo aWidgetSafeDeleteInfo;
	aWidgetSafeDeleteInfo.mUpdateAppDepth = mUpdateAppDepth;
	aWidgetSafeDeleteInfo.mWidget = theWidget;
	mSafeDeleteList.push_back(aWidgetSafeDeleteInfo);
}

void SexyAppBase::HandleNotifyGameMessage(int theType)
{
	if (theType == 0 && mWindow != NULL) // bring to front message
	{
		SDL_RestoreWindow(mWindow);
		SDL_RaiseWindow(mWindow);
	}
}

void SexyAppBase::RehupFocus()
{
	bool wantHasFocus = mActive && !mMinimized;

	if (wantHasFocus != mHasFocus)
	{
		mHasFocus = wantHasFocus;

		if (mHasFocus)
		{
			if (mMuteOnLostFocus)
				Unmute(true);

			mWidgetManager->GotFocus();
			GotFocus();
		}
		else
		{
			if (mMuteOnLostFocus)
				Mute(true);

			mWidgetManager->LostFocus();
			LostFocus();

			SDL_CaptureMouse(false);
			mWidgetManager->DoMouseUps();
		}
	}
}

void SexyAppBase::ClearKeysDown()
{
	if (mWidgetManager != NULL) // fix stuck alt-key problem
	{
		for (int aKeyNum = 0; aKeyNum < 0xFF; aKeyNum++)
			mWidgetManager->mKeyDown[aKeyNum] = false;
	}
	mCtrlDown = false;
	mAltDown = false;
}

bool SexyAppBase::IsAltKeyUsed(int theChar)
{
	int aChar = tolower(theChar);
	switch (aChar)
	{
		case 13: // alt-enter
		case 'r':
			return true;
		default:
			return false;
	}
}

bool SexyAppBase::DebugKeyDown(int theKey)
{
	if ((theKey == 'R') && (mWidgetManager->mKeyDown[KEYCODE_MENU]))
	{
#ifndef RELEASEFINAL
		if (!ReparseModValues())
		{
			for (int aKeyNum = 0; aKeyNum < 0xFF; aKeyNum++) // prevent alt from getting stuck
				mWidgetManager->mKeyDown[aKeyNum] = false;
		}
#endif
	}
	else if (theKey == KEYCODE_F3)
	{
		if(mWidgetManager->mKeyDown[KEYCODE_SHIFT])
		{
			mShowFPS = true;
			if (++mShowFPSMode >= Num_FPS_Types)
				mShowFPSMode = 0;
		}
		else
			mShowFPS = !mShowFPS;

		mWidgetManager->MarkAllDirty();

		if (mShowFPS)
		{
			gFPSTimer.Start();
			gFrameCount = 0;
			gFPSDisplay = 0;
			gForceDisplay = true;
		}
	}
	else if (theKey == KEYCODE_F8)
	{
		if(mWidgetManager->mKeyDown[KEYCODE_SHIFT])
		{
			Set3DAcclerated(!Is3DAccelerated());

			char aBuf[512];
			snprintf(aBuf, sizeof(aBuf), "3D-Mode: %s",Is3DAccelerated()?"ON":"OFF");
			MsgBox(aBuf,"Mode Switch",MB_OK);
			mLastTime = timeGetTime();
		}
		else
			ShowMemoryUsage();

		return true;
	}
	else if (theKey == KEYCODE_F10)
	{
#ifndef RELEASEFINAL
		if (mWidgetManager->mKeyDown[KEYCODE_CONTROL])
		{
			if (mUpdateMultiplier==0.25)
				mUpdateMultiplier = 1.0;
			else
				mUpdateMultiplier = 0.25;
		}
		else if(mWidgetManager->mKeyDown[KEYCODE_SHIFT])
		{
			mStepMode = 0;
			ClearUpdateBacklog();
		}
		else
			mStepMode = 1;
#endif

		return true;
	}
	else if (theKey == KEYCODE_F11)
	{
		if (mWidgetManager->mKeyDown[KEYCODE_SHIFT])
			DumpProgramInfo();
		else
			TakeScreenshot();

		return true;
	}
	else if (theKey == KEYCODE_F2)
	{
		bool isPerfOn = !SexyPerf::IsPerfOn();
		if (isPerfOn)
		{
			ClearUpdateBacklog();
			SexyPerf::BeginPerf();
		}
		else
		{
			SexyPerf::EndPerf();
			MsgBox(SexyPerf::GetResults().c_str(), "Perf Results", MB_OK);
			ClearUpdateBacklog();
		}
	}
	else
		return false;

	return false;
}

void SexyAppBase::CloseRequestAsync()
{
}

// Converts a window-space mouse position to logical screen coordinates.
static void MapMouse(SexyAppBase* theApp, float theX, float theY, int& theOutX, int& theOutY)
{
	float aPixelScale = SDL_GetWindowPixelDensity(theApp->mWindow);
	if (aPixelScale <= 0.0f)
		aPixelScale = 1.0f;
	theApp->mDisplay->WindowToLogical(theX, theY, aPixelScale, theOutX, theOutY);
}

void SexyAppBase::ProcessEvent(const SDL_Event& theEvent)
{
	switch (theEvent.type)
	{
	case SDL_EVENT_QUIT:
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		CloseRequestAsync();
		// This should short-circuit any modal loops.
		mManualShutdown = true;
		Shutdown();
		break;

	case SDL_EVENT_WINDOW_FOCUS_GAINED:
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		mActive = theEvent.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
		if ((!mSEHOccured) && (!mShutdown))
		{
			RehupFocus();

			if ((mActive) && (!mIsWindowed))
				mWidgetManager->MarkAllDirty();

			if ((mIsOpeningURL) && (!mActive))
				URLOpenSucceeded(mOpeningURL);
		}
		if (!mActive)
			ClearKeysDown();
		break;

	case SDL_EVENT_WINDOW_MINIMIZED:
	case SDL_EVENT_WINDOW_RESTORED:
	case SDL_EVENT_WINDOW_MAXIMIZED:
	{
		bool isMinimized = theEvent.type == SDL_EVENT_WINDOW_MINIMIZED;
		mPhysMinimized = isMinimized;
		if ((!mShutdown) && (isMinimized != mMinimized))
		{
			mMinimized = isMinimized;

			// We don't want any sounds (or music) playing while its minimized
			if (mMinimized)
			{
				if (mMuteOnLostFocus)
					Mute(true);
			}
			else
			{
				if (mMuteOnLostFocus)
					Unmute(true);

				mWidgetManager->MarkAllDirty();
			}
		}

		RehupFocus();
		break;
	}

	case SDL_EVENT_WINDOW_OCCLUDED:
		mWindowOccluded = true;
		break;

	case SDL_EVENT_WINDOW_EXPOSED:
	case SDL_EVENT_WINDOW_SHOWN:
		mWindowOccluded = false;
		mWidgetManager->MarkAllDirty();
		break;

	case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
	case SDL_EVENT_WINDOW_RESIZED:
	case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
	case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
		if (mDisplay != NULL)
			mDisplay->UpdateViewport();
		if (theEvent.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN || theEvent.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)
			mIsWindowed = theEvent.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN;
		mWidgetManager->MarkAllDirty();
		break;

	case SDL_EVENT_WINDOW_MOVED:
		if (mIsWindowed && mWindow != NULL && !(SDL_GetWindowFlags(mWindow) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED)))
		{
			mPreferredX = theEvent.window.data1;
			mPreferredY = theEvent.window.data2;
		}
		break;

	case SDL_EVENT_WINDOW_MOUSE_ENTER:
		if (!mMouseIn)
		{
			mMouseIn = true;
			EnforceCursor();
		}
		break;

	case SDL_EVENT_WINDOW_MOUSE_LEAVE:
		if (mMouseIn && (mWidgetManager->mDownButtons == 0))
		{
			float aX = 0, aY = 0;
			SDL_GetMouseState(&aX, &aY);
			int x, y;
			MapMouse(this, aX, aY, x, y);
			mWidgetManager->RemapMouse(x, y);
			mWidgetManager->MouseExit(x, y);
			mMouseIn = false;
			EnforceCursor();
		}
		break;

	case SDL_EVENT_MOUSE_MOTION:
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
	{
		if (mSEHOccured)
			break;

		float aWindowX = theEvent.type == SDL_EVENT_MOUSE_MOTION ? theEvent.motion.x : theEvent.button.x;
		float aWindowY = theEvent.type == SDL_EVENT_MOUSE_MOTION ? theEvent.motion.y : theEvent.button.y;
		int x, y;
		MapMouse(this, aWindowX, aWindowY, x, y);
		mWidgetManager->RemapMouse(x, y);

		mLastUserInputTick = mLastTimerTime;

		mWidgetManager->MouseMove(x, y);
		if (mDisplay != NULL)
			mDisplay->SetCursorPos(x, y);

		if (!mMouseIn)
		{
			mMouseIn = true;
			EnforceCursor();
		}

		if (theEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
		{
			bool isDoubleClick = theEvent.button.clicks >= 2 && (theEvent.button.clicks % 2) == 0;
			SDL_CaptureMouse(true);
			switch (theEvent.button.button)
			{
			case SDL_BUTTON_LEFT:	mWidgetManager->MouseDown(x, y, isDoubleClick ? 2 : 1); break;
			case SDL_BUTTON_RIGHT:	mWidgetManager->MouseDown(x, y, isDoubleClick ? -2 : -1); break;
			case SDL_BUTTON_MIDDLE:	mWidgetManager->MouseDown(x, y, 3); break;
			default: break;
			}
		}
		else if (theEvent.type == SDL_EVENT_MOUSE_BUTTON_UP)
		{
			switch (theEvent.button.button)
			{
			case SDL_BUTTON_LEFT:
				if ((mWidgetManager->mDownButtons & ~1) == 0)
					SDL_CaptureMouse(false);
				mWidgetManager->MouseUp(x, y, 1);
				break;
			case SDL_BUTTON_RIGHT:
				if ((mWidgetManager->mDownButtons & ~2) == 0)
					SDL_CaptureMouse(false);
				mWidgetManager->MouseUp(x, y, -1);
				break;
			case SDL_BUTTON_MIDDLE:
				if ((mWidgetManager->mDownButtons & ~4) == 0)
					SDL_CaptureMouse(false);
				mWidgetManager->MouseUp(x, y, 3);
				break;
			default:
				break;
			}
		}
		break;
	}

	case SDL_EVENT_MOUSE_WHEEL:
	{
		float aDelta = theEvent.wheel.y;
		if (theEvent.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
			aDelta = -aDelta;

		mWheelAccumulator += aDelta;
		int aNotches = (int)mWheelAccumulator;
		if (aNotches != 0)
		{
			mWheelAccumulator -= aNotches;
			mWidgetManager->MouseWheel(aNotches);
		}
		break;
	}

	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
	{
		bool keyDown = theEvent.type == SDL_EVENT_KEY_DOWN;
		SDL_Keycode aKey = theEvent.key.key;
		int aKeyCode = SDLKeyToKeyCode(aKey, theEvent.key.scancode);

		if (aKeyCode == KEYCODE_CONTROL)
			mCtrlDown = keyDown;
		if (aKeyCode == KEYCODE_MENU)
			mAltDown = keyDown;

		mLastUserInputTick = mLastTimerTime;

		if (!keyDown)
		{
			if (aKeyCode != KEYCODE_UNKNOWN)
				mWidgetManager->KeyUp((KeyCode) aKeyCode);
			break;
		}

		bool isCommandDown = (theEvent.key.mod & SDL_KMOD_GUI) != 0;
		bool isAltDown = (theEvent.key.mod & SDL_KMOD_ALT) != 0;
		bool isCtrlDown = (theEvent.key.mod & SDL_KMOD_CTRL) != 0;

		// Alt+Enter (and Command+Enter or Command+Ctrl+F on macOS) toggle fullscreen.
		if ((aKeyCode == KEYCODE_RETURN && (isAltDown || isCommandDown)) ||
			(aKey == SDLK_F && isCommandDown && isCtrlDown))
		{
			if (!theEvent.key.repeat && !mForceFullscreen && !mForceWindowed && mAllowAltEnter)
			{
				SwitchScreenMode(!mIsWindowed);
				ClearKeysDown();
			}
			break;
		}

		if (aKeyCode == 'D' && isCtrlDown && isAltDown)
		{
			mDebugKeysEnabled = !mDebugKeysEnabled;
		}

		if (mDebugKeysEnabled)
		{
			if (DebugKeyDown(aKeyCode))
				break;
		}

		if (aKeyCode != KEYCODE_UNKNOWN)
			mWidgetManager->KeyDown((KeyCode) aKeyCode);

		// Windows generated WM_CHAR for these keys; text input events do not.
		SexyChar aChar = 0;
		switch (aKeyCode)
		{
		case KEYCODE_BACK:		aChar = 8; break;
		case KEYCODE_TAB:		aChar = 9; break;
		case KEYCODE_RETURN:	aChar = 13; break;
		case KEYCODE_ESCAPE:	aChar = 27; break;
		default:
			if (isCtrlDown && aKeyCode >= 'A' && aKeyCode <= 'Z')
				aChar = (SexyChar)(aKeyCode - 'A' + 1);
			break;
		}

		if (aChar != 0)
			mWidgetManager->KeyChar(aChar);
		break;
	}

	case SDL_EVENT_TEXT_INPUT:
	{
		mLastUserInputTick = mLastTimerTime;

		// Decode UTF-8; the framework's text is single-byte Latin-1.
		std::wstring aText = UTF8StringToWString(theEvent.text.text != NULL ? theEvent.text.text : "");
		for (wchar_t aChar : aText)
		{
			if ((unsigned long)aChar >= 32 && (unsigned long)aChar < 256 && aChar != 127)
				mWidgetManager->KeyChar((SexyChar)(unsigned char)aChar);
		}
		break;
	}

	default:
		break;
	}
}

// Processes pending window-system events. When singleMessage is set only one
// event is handled, matching the original deferred-message pump. Returns true
// if more events are waiting.
bool SexyAppBase::ProcessEvents(bool singleMessage)
{
	SDL_Event anEvent;
	while (SDL_PollEvent(&anEvent))
	{
		ProcessEvent(anEvent);

		if (singleMessage || mShutdown)
			break;
	}

	return SDL_HasEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
}

void SexyAppBase::ClearUpdateBacklog(bool relaxForASecond)
{
	mLastTimeCheck = timeGetTime();
	mUpdateFTimeAcc = 0.0;

	if (relaxForASecond)
		mRelaxUpdateBacklogCount = 1000;
}

bool SexyAppBase::IsScreenSaver()
{
	return mIsScreenSaver;
}

bool SexyAppBase::AppCanRestore()
{
	return !mIsDisabled;
}


Dialog* SexyAppBase::NewDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode)
{	
	Dialog* aDialog = new Dialog(NULL, NULL, theDialogId, isModal, theDialogHeader,	theDialogLines, theDialogFooter, theButtonMode);		
	return aDialog;
}

Dialog* SexyAppBase::DoDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode)
{
	KillDialog(theDialogId);

	Dialog* aDialog = NewDialog(theDialogId, isModal, theDialogHeader, theDialogLines, theDialogFooter, theButtonMode);		

	AddDialog(theDialogId, aDialog);

	return aDialog;
}


Dialog*	SexyAppBase::GetDialog(int theDialogId)
{
	DialogMap::iterator anItr = mDialogMap.find(theDialogId);

	if (anItr != mDialogMap.end())	
		return anItr->second;

	return NULL;
}

bool SexyAppBase::KillDialog(int theDialogId, bool removeWidget, bool deleteWidget)
{
	DialogMap::iterator anItr = mDialogMap.find(theDialogId);

	if (anItr != mDialogMap.end())
	{
		Dialog* aDialog = anItr->second;

		// set the result to something else so DoMainLoop knows that the dialog is gone 
		// in case nobody else sets mResult		
		if (aDialog->mResult == -1) 
			aDialog->mResult = 0;
		
		DialogList::iterator aListItr = std::find(mDialogList.begin(),mDialogList.end(),aDialog);
		if (aListItr != mDialogList.end())
			mDialogList.erase(aListItr);
		
		mDialogMap.erase(anItr);

		if (removeWidget || deleteWidget)
		mWidgetManager->RemoveWidget(aDialog);

		if (aDialog->IsModal())
		{			
			ModalClose();
			mWidgetManager->RemoveBaseModal(aDialog);
		}				

		if (deleteWidget)
		SafeDeleteWidget(aDialog);
		
		return true;
	}

	return false;
}

bool SexyAppBase::KillDialog(int theDialogId)
{
	return KillDialog(theDialogId,true,true);
}

bool SexyAppBase::KillDialog(Dialog* theDialog)
{
	return KillDialog(theDialog->mId);
}

int SexyAppBase::GetDialogCount()
{
	return mDialogMap.size();
}

void SexyAppBase::AddDialog(int theDialogId, Dialog* theDialog)
{
	KillDialog(theDialogId);

	if (theDialog->mWidth == 0)
	{
		// Set the dialog position ourselves
		int aWidth = mWidth/2;
		theDialog->Resize((mWidth - aWidth)/2, mHeight / 5, aWidth, theDialog->GetPreferredHeight(aWidth));
	}

	mDialogMap.insert(DialogMap::value_type(theDialogId, theDialog));
	mDialogList.push_back(theDialog);

	mWidgetManager->AddWidget(theDialog);
	if (theDialog->IsModal())
	{
		mWidgetManager->AddBaseModal(theDialog);
		ModalOpen();
	}
}

void SexyAppBase::AddDialog(Dialog* theDialog)
{
	AddDialog(theDialog->mId, theDialog);
}

void SexyAppBase::ModalOpen()
{
}

void SexyAppBase::ModalClose()
{
}

void SexyAppBase::DialogButtonPress(int theDialogId, int theButtonId)
{	
	if (theButtonId == Dialog::ID_YES)
		ButtonPress(2000 + theDialogId);
	else if (theButtonId == Dialog::ID_NO)
		ButtonPress(3000 + theDialogId);	
}

void SexyAppBase::DialogButtonDepress(int theDialogId, int theButtonId)
{
	if (theButtonId == Dialog::ID_YES)
		ButtonDepress(2000 + theDialogId);
	else if (theButtonId == Dialog::ID_NO)
		ButtonDepress(3000 + theDialogId);
}

void SexyAppBase::GotFocus()
{	
}

void SexyAppBase::LostFocus()
{	
}

void SexyAppBase::URLOpenFailed(const std::string& theURL)
{
	(void)theURL;
	mIsOpeningURL = false;
}

void SexyAppBase::URLOpenSucceeded(const std::string& theURL)
{	
	(void)theURL;
	mIsOpeningURL = false;

	if (mShutdownOnURLOpen)
		Shutdown();	
}

void SexyAppBase::WaitForLoadingThread()
{
	while ((mLoadingThreadStarted) && (!mLoadingThreadCompleted))
		Sleep(20);
}

void SexyAppBase::SetCursorImage(int theCursorNum, Image* theImage)
{
	if ((theCursorNum >= 0) && (theCursorNum < NUM_CURSORS))
	{
		mCursorImages[theCursorNum] = theImage;
		EnforceCursor();
	}
}

double SexyAppBase::GetLoadingThreadProgress()
{
	if (mLoaded)
		return 1.0;
	if (!mLoadingThreadStarted)
		return 0.0;
	if (mNumLoadingThreadTasks == 0)
		return 0.0;
	return std::min(mCompletedLoadingThreadTasks / (double) mNumLoadingThreadTasks, 1.0);
}

bool SexyAppBase::WriteBufferToFile(const std::string& theFileName, const Buffer* theBuffer)
{
	return WriteBytesToFile(theFileName,theBuffer->GetDataPtr(),theBuffer->GetDataLen());
}


void SexyAppBase::UpdateFrames()
{
	mUpdateCount++;	

	if (!mMinimized)
	{		
		if (mWidgetManager->UpdateFrame())
			++mFPSDirtyCount;
	}

	mMusicInterface->Update();	
	CleanSharedImages();
}

void SexyAppBase::DoUpdateFramesF(float theFrac)
{
	if ((mVSyncUpdates) && (!mMinimized))
		mWidgetManager->UpdateFrameF(theFrac);	
}

void SexyAppBase::Done3dTesting()
{
}

// return file name that you want to upload
std::string	SexyAppBase::NotifyCrashHook()
{
	return "";
}


void SexyAppBase::DeleteNativeImageData()
{
	MemoryImageSet::iterator anItr = mMemoryImageSet.begin();
	while (anItr != mMemoryImageSet.end())
	{
		MemoryImage* aMemoryImage = *anItr;		
		aMemoryImage->DeleteNativeData();			
		++anItr;
	}
}

void SexyAppBase::DeleteExtraImageData()
{
	AutoCrit anAutoCrit(mCritSect);
	MemoryImageSet::iterator anItr = mMemoryImageSet.begin();
	while (anItr != mMemoryImageSet.end())
	{
		MemoryImage* aMemoryImage = *anItr;
		aMemoryImage->DeleteExtraBuffers();
		++anItr;
	}
}

void SexyAppBase::ReInitImages()
{
	MemoryImageSet::iterator anItr = mMemoryImageSet.begin();
	while (anItr != mMemoryImageSet.end())
	{
		MemoryImage* aMemoryImage = *anItr;				
		aMemoryImage->ReInit();
		++anItr;
	}
}


void SexyAppBase::LoadingThreadProc()
{
}

void SexyAppBase::LoadingThreadCompleted()
{
}

void SexyAppBase::SetAlphaDisabled(bool isDisabled)
{
	if (mAlphaDisabled != isDisabled)
	{
		mAlphaDisabled = isDisabled;
		mDisplay->SetVideoOnlyDraw(mAlphaDisabled);		
		mWidgetManager->mImage = mDisplay->GetScreenImage();
		mWidgetManager->MarkAllDirty();
	}
}

void SexyAppBase::ProcessSafeDeleteList()
{
	MTAutoDisallowRand aDisallowRand;

	WidgetSafeDeleteList::iterator anItr = mSafeDeleteList.begin();
	while (anItr != mSafeDeleteList.end())
	{
		WidgetSafeDeleteInfo* aWidgetSafeDeleteInfo = &(*anItr);
		if (mUpdateAppDepth <= aWidgetSafeDeleteInfo->mUpdateAppDepth)
		{
			delete aWidgetSafeDeleteInfo->mWidget;
			anItr = mSafeDeleteList.erase(anItr);
		}
		else
			++anItr;
	}	
}

void SexyAppBase::UpdateFTimeAcc()
{
	DWORD aCurTime = timeGetTime();

	if (mLastTimeCheck != 0)
	{				
		int aDeltaTime = aCurTime - mLastTimeCheck;		

		mUpdateFTimeAcc = std::min(mUpdateFTimeAcc + aDeltaTime, 200.0);

		if (mRelaxUpdateBacklogCount > 0)				
			mRelaxUpdateBacklogCount = std::max(mRelaxUpdateBacklogCount - aDeltaTime, 0);				
	}

	mLastTimeCheck = aCurTime;
}

bool SexyAppBase::UpdateApp()
{
	bool updated;
	for (;;)
	{
		if (!UpdateAppStep(&updated))
			return false;
		if (updated)
			return true;
	}
}

void SexyAppBase::PreTerminate()
{
}


bool SexyAppBase::LoadProperties(const std::string& theFileName, bool required, bool checkSig)
{
	Buffer aBuffer;
	if (!ReadBufferFromFile(theFileName, &aBuffer))
	{
		if (!required)
			return true;
		else
		{
			Popup(GetString("UNABLE_OPEN_PROPERTIES", _S("Unable to open properties file ")) + StringToSexyString(theFileName));
			return false;
		}
	}
	if (checkSig)
	{
		//if (!CheckSignature(aBuffer, theFileName))
		//{
			//Popup(GetString("PROPERTIES_SIG_FAILED", _S("Signature check failed on ")) + StringToSexyString(theFileName + "'"));
			//return false;
		//}
	}

	PropertiesParser aPropertiesParser(this);

	// Load required language-file properties
		if (!aPropertiesParser.ParsePropertiesBuffer(aBuffer))
		{
			Popup(aPropertiesParser.GetErrorText());		
			return false;
		}
		else
			return true;
}

bool SexyAppBase::LoadProperties()
{
	// Load required language-file properties
	return LoadProperties("properties\\default.xml", true, false);
}

void SexyAppBase::LoadResourceManifest()
{
	if (!mResourceManager->ParseResourcesFile("properties\\resources.xml"))
		ShowResourceError(true);
}

void SexyAppBase::ShowResourceError(bool doExit)
{
	Popup(mResourceManager->GetErrorText());	
	if (doExit)
		DoExit(0);
}

bool SexyAppBase::GetBoolean(const std::string& theId)
{
	StringBoolMap::iterator anItr = mBoolProperties.find(theId);
	DBG_ASSERTE(anItr != mBoolProperties.end());
	
	if (anItr != mBoolProperties.end())	
		return anItr->second;
	else
		return false;
}

bool SexyAppBase::GetBoolean(const std::string& theId, bool theDefault)
{
	StringBoolMap::iterator anItr = mBoolProperties.find(theId);	
	
	if (anItr != mBoolProperties.end())	
		return anItr->second;
	else
		return theDefault;	
}

int SexyAppBase::GetInteger(const std::string& theId)
{
	StringIntMap::iterator anItr = mIntProperties.find(theId);
	DBG_ASSERTE(anItr != mIntProperties.end());
	
	if (anItr != mIntProperties.end())	
		return anItr->second;
	else
		return false;
}

int SexyAppBase::GetInteger(const std::string& theId, int theDefault)
{
	StringIntMap::iterator anItr = mIntProperties.find(theId);	
	
	if (anItr != mIntProperties.end())	
		return anItr->second;
	else
		return theDefault;	
}

double SexyAppBase::GetDouble(const std::string& theId)
{
	StringDoubleMap::iterator anItr = mDoubleProperties.find(theId);
	DBG_ASSERTE(anItr != mDoubleProperties.end());
	
	if (anItr != mDoubleProperties.end())	
		return anItr->second;
	else
		return false;
}

double SexyAppBase::GetDouble(const std::string& theId, double theDefault)
{
	StringDoubleMap::iterator anItr = mDoubleProperties.find(theId);	
	
	if (anItr != mDoubleProperties.end())	
		return anItr->second;
	else
		return theDefault;	
}

SexyString SexyAppBase::GetString(const std::string& theId)
{
	StringWStringMap::iterator anItr = mStringProperties.find(theId);
	DBG_ASSERTE(anItr != mStringProperties.end());
	
	if (anItr != mStringProperties.end())	
		return WStringToSexyString(anItr->second);
	else
		return _S("");
}

SexyString SexyAppBase::GetString(const std::string& theId, const SexyString& theDefault)
{
	StringWStringMap::iterator anItr = mStringProperties.find(theId);	
	
	if (anItr != mStringProperties.end())	
		return WStringToSexyString(anItr->second);
	else
		return theDefault;	
}

StringVector SexyAppBase::GetStringVector(const std::string& theId)
{
	StringStringVectorMap::iterator anItr = mStringVectorProperties.find(theId);
	DBG_ASSERTE(anItr != mStringVectorProperties.end());
	
	if (anItr != mStringVectorProperties.end())	
		return anItr->second;
	else
		return StringVector();
}

void SexyAppBase::SetString(const std::string& theId, const std::wstring& theValue)
{
	std::pair<StringWStringMap::iterator, bool> aPair = mStringProperties.insert(StringWStringMap::value_type(theId, theValue));
	if (!aPair.second) // Found it, change value
		aPair.first->second = theValue;
}


void SexyAppBase::SetBoolean(const std::string& theId, bool theValue)
{
	std::pair<StringBoolMap::iterator, bool> aPair = mBoolProperties.insert(StringBoolMap::value_type(theId, theValue));
	if (!aPair.second) // Found it, change value
		aPair.first->second = theValue;
}

void SexyAppBase::SetInteger(const std::string& theId, int theValue)
{
	std::pair<StringIntMap::iterator, bool> aPair = mIntProperties.insert(StringIntMap::value_type(theId, theValue));
	if (!aPair.second) // Found it, change value
		aPair.first->second = theValue;
}

void SexyAppBase::SetDouble(const std::string& theId, double theValue)
{
	std::pair<StringDoubleMap::iterator, bool> aPair = mDoubleProperties.insert(StringDoubleMap::value_type(theId, theValue));
	if (!aPair.second) // Found it, change value
		aPair.first->second = theValue;
}

void SexyAppBase::ParseCmdLine(const std::string& theCmdLine)
{
	// Command line example:  -play -demofile="game demo.dmo"
	// Results in HandleCmdLineParam("-play", ""); HandleCmdLineParam("-demofile", "game demo.dmo");
	std::string aCurParamName;
	std::string aCurParamValue;

	//int aSpacePos = 0; // Unused
	bool inQuote = false;
	bool onValue = false;

	for (size_t i = 0; i < theCmdLine.length(); i++)
	{
		char c = theCmdLine[i];
		bool atEnd = false;
		
		if (c == '"')
		{
			inQuote = !inQuote;

			if (!inQuote)
				atEnd = true;
		}
		else if ((c == ' ') && (!inQuote))
			atEnd = true;
		else if (c == '=')
			onValue = true;
		else if (onValue)
			aCurParamValue += c;
		else
			aCurParamName += c;
		
		if (i == theCmdLine.length() - 1)
			atEnd = true;
		
		if (atEnd && !aCurParamName.empty())
		{
			HandleCmdLineParam(aCurParamName, aCurParamValue);
			aCurParamName = "";
			aCurParamValue = "";
			onValue = false;
		}	
	}
}

void SexyAppBase::PreDisplayHook()
{
}

void SexyAppBase::PreDDInterfaceInitHook()
{
}

void SexyAppBase::PostDDInterfaceInitHook()
{
}

bool SexyAppBase::ChangeDirHook(const char *theIntendedPath)
{
	(void)theIntendedPath;
	return false;
}


void SexyAppBase::InitPropertiesHook()
{
}

void SexyAppBase::InitHook()
{
}


void SexyAppBase::SetCursor(int theCursorNum)
{
	mCursorNum = theCursorNum;
	EnforceCursor();
}

int SexyAppBase::GetCursor()
{
	return mCursorNum;
}

void SexyAppBase::EnableCustomCursors(bool enabled)
{
	mCustomCursorsEnabled = enabled;
	EnforceCursor();
}


Sexy::DeviceImage* SexyAppBase::CreateCrossfadeImage(Sexy::Image* theImage1, const Rect& theRect1, Sexy::Image* theImage2, const Rect& theRect2, double theFadeFactor)
{
	MemoryImage* aMemoryImage1 = dynamic_cast<MemoryImage*>(theImage1);
	MemoryImage* aMemoryImage2 = dynamic_cast<MemoryImage*>(theImage2);

	if ((aMemoryImage1 == NULL) || (aMemoryImage2 == NULL))
		return NULL;

	if ((theRect1.mX < 0) || (theRect1.mY < 0) || 
		(theRect1.mX + theRect1.mWidth > theImage1->GetWidth()) ||
		(theRect1.mY + theRect1.mHeight > theImage1->GetHeight()))
	{
		DBG_ASSERTE("Crossfade Rect1 out of bounds");
		return NULL;
	}

	if ((theRect2.mX < 0) || (theRect2.mY < 0) || 
		(theRect2.mX + theRect2.mWidth > theImage2->GetWidth()) ||
		(theRect2.mY + theRect2.mHeight > theImage2->GetHeight()))
	{
		DBG_ASSERTE("Crossfade Rect2 out of bounds");
		return NULL;
	}

	int aWidth = theRect1.mWidth;
	int aHeight = theRect1.mHeight;

	DeviceImage* anImage = new DeviceImage(mDisplay);
	anImage->Create(aWidth, aHeight);

	ulong* aDestBits = anImage->GetBits();
	ulong* aSrcBits1 = aMemoryImage1->GetBits();
	ulong* aSrcBits2 = aMemoryImage2->GetBits();

	int aSrc1Width = aMemoryImage1->GetWidth();
	int aSrc2Width = aMemoryImage2->GetWidth();
	ulong aMult = (int) (theFadeFactor*256);
	ulong aOMM = (256 - aMult);

	for (int y = 0; y < aHeight; y++)
	{
		ulong* s1 = &aSrcBits1[(y+theRect1.mY)*aSrc1Width+theRect1.mX];
		ulong* s2 = &aSrcBits2[(y+theRect2.mY)*aSrc2Width+theRect2.mX];
		ulong* d = &aDestBits[y*aWidth];

		for (int x = 0; x < aWidth; x++)
		{
			ulong p1 = *s1++;
			ulong p2 = *s2++;

			//p1 = 0;
			//p2 = 0xFFFFFFFF;

			*d++ = 
				((((p1 & 0x000000FF)*aOMM + (p2 & 0x000000FF)*aMult)>>8) & 0x000000FF) |
				((((p1 & 0x0000FF00)*aOMM + (p2 & 0x0000FF00)*aMult)>>8) & 0x0000FF00) |
				((((p1 & 0x00FF0000)*aOMM + (p2 & 0x00FF0000)*aMult)>>8) & 0x00FF0000) |
				((((p1 >> 24)*aOMM + (p2 >> 24)*aMult)<<16) & 0xFF000000);
		}
	}

	anImage->BitsChanged();
	
	return anImage;
}

void SexyAppBase::ColorizeImage(Image* theImage, const Color& theColor)
{
	MemoryImage* aSrcMemoryImage = dynamic_cast<MemoryImage*>(theImage);

	if (aSrcMemoryImage == NULL)
		return;

	ulong* aBits;	
	int aNumColors;

	if (aSrcMemoryImage->mColorTable == NULL)
	{
		aBits = aSrcMemoryImage->GetBits();		
		aNumColors = theImage->GetWidth()*theImage->GetHeight();				
	}
	else
	{
		aBits = aSrcMemoryImage->mColorTable;		
		aNumColors = 256;				
	}
						
	if ((theColor.mAlpha <= 255) && (theColor.mRed <= 255) && 
		(theColor.mGreen <= 255) && (theColor.mBlue <= 255))
	{
		for (int i = 0; i < aNumColors; i++)
		{
			ulong aColor = aBits[i];

			aBits[i] = 
				((((aColor & 0xFF000000) >> 8) * theColor.mAlpha) & 0xFF000000) |
				((((aColor & 0x00FF0000) * theColor.mRed) >> 8) & 0x00FF0000) |
				((((aColor & 0x0000FF00) * theColor.mGreen) >> 8) & 0x0000FF00)|
				((((aColor & 0x000000FF) * theColor.mBlue) >> 8) & 0x000000FF);
		}
	}
	else
	{
		for (int i = 0; i < aNumColors; i++)
		{
			ulong aColor = aBits[i];

			int aAlpha = ((aColor >> 24) * theColor.mAlpha) / 255;
			int aRed = (((aColor >> 16) & 0xFF) * theColor.mRed) / 255;
			int aGreen = (((aColor >> 8) & 0xFF) * theColor.mGreen) / 255;
			int aBlue = ((aColor & 0xFF) * theColor.mBlue) / 255;

			if (aAlpha > 255)
				aAlpha = 255;
			if (aRed > 255)
				aRed = 255;
			if (aGreen > 255)
				aGreen = 255;
			if (aBlue > 255)
				aBlue = 255;

			aBits[i] = (aAlpha << 24) | (aRed << 16) | (aGreen << 8) | (aBlue);
		}
	}	

	aSrcMemoryImage->BitsChanged();
}

DeviceImage* SexyAppBase::CreateColorizedImage(Image* theImage, const Color& theColor)
{
	MemoryImage* aSrcMemoryImage = dynamic_cast<MemoryImage*>(theImage);

	if (aSrcMemoryImage == NULL)
		return NULL;

	DeviceImage* anImage = new DeviceImage(mDisplay);
	
	anImage->Create(theImage->GetWidth(), theImage->GetHeight());
	
	ulong* aSrcBits;
	ulong* aDestBits;
	int aNumColors;

	if (aSrcMemoryImage->mColorTable == NULL)
	{
		aSrcBits = aSrcMemoryImage->GetBits();
		aDestBits = anImage->GetBits();
		aNumColors = theImage->GetWidth()*theImage->GetHeight();				
	}
	else
	{
		aSrcBits = aSrcMemoryImage->mColorTable;
		aDestBits = anImage->mColorTable = new ulong[256];
		aNumColors = 256;
		
		anImage->mColorIndices = new uchar[anImage->mWidth*theImage->mHeight];
		memcpy(anImage->mColorIndices, aSrcMemoryImage->mColorIndices, anImage->mWidth*theImage->mHeight);
	}
						
	if ((theColor.mAlpha <= 255) && (theColor.mRed <= 255) && 
		(theColor.mGreen <= 255) && (theColor.mBlue <= 255))
	{
		for (int i = 0; i < aNumColors; i++)
		{
			ulong aColor = aSrcBits[i];

			aDestBits[i] = 
				((((aColor & 0xFF000000) >> 8) * theColor.mAlpha) & 0xFF000000) |
				((((aColor & 0x00FF0000) * theColor.mRed) >> 8) & 0x00FF0000) |
				((((aColor & 0x0000FF00) * theColor.mGreen) >> 8) & 0x0000FF00)|
				((((aColor & 0x000000FF) * theColor.mBlue) >> 8) & 0x000000FF);
		}
	}
	else
	{
		for (int i = 0; i < aNumColors; i++)
		{
			ulong aColor = aSrcBits[i];

			int aAlpha = ((aColor >> 24) * theColor.mAlpha) / 255;
			int aRed = (((aColor >> 16) & 0xFF) * theColor.mRed) / 255;
			int aGreen = (((aColor >> 8) & 0xFF) * theColor.mGreen) / 255;
			int aBlue = ((aColor & 0xFF) * theColor.mBlue) / 255;

			if (aAlpha > 255)
				aAlpha = 255;
			if (aRed > 255)
				aRed = 255;
			if (aGreen > 255)
				aGreen = 255;
			if (aBlue > 255)
				aBlue = 255;

			aDestBits[i] = (aAlpha << 24) | (aRed << 16) | (aGreen << 8) | (aBlue);
		}
	}	

	anImage->BitsChanged();

	return anImage;
}

DeviceImage* SexyAppBase::CopyImage(Image* theImage, const Rect& theRect)
{
	DeviceImage* anImage = new DeviceImage(mDisplay);

	anImage->Create(theRect.mWidth, theRect.mHeight);
	
	Graphics g(anImage);
	g.DrawImage(theImage, -theRect.mX, -theRect.mY);

	anImage->CopyAttributes(theImage);

	return anImage;
}

DeviceImage* SexyAppBase::CopyImage(Image* theImage)
{
	return CopyImage(theImage, Rect(0, 0, theImage->GetWidth(), theImage->GetHeight()));
}

void SexyAppBase::MirrorImage(Image* theImage)
{
	MemoryImage* aSrcMemoryImage = dynamic_cast<MemoryImage*>(theImage);	

	ulong* aSrcBits = aSrcMemoryImage->GetBits();

	int aPhysSrcWidth = aSrcMemoryImage->mWidth;
	for (int y = 0; y < aSrcMemoryImage->mHeight; y++)
	{
		ulong* aLeftBits = aSrcBits + (y * aPhysSrcWidth);		
		ulong* aRightBits = aLeftBits + (aPhysSrcWidth - 1);

		for (int x = 0; x < (aPhysSrcWidth >> 1); x++)
		{
			ulong aSwap = *aLeftBits;

			*(aLeftBits++) = *aRightBits;
			*(aRightBits--) = aSwap;
		}
	}

	aSrcMemoryImage->BitsChanged();	
}

void SexyAppBase::FlipImage(Image* theImage)
{
	MemoryImage* aSrcMemoryImage = dynamic_cast<MemoryImage*>(theImage);

	ulong* aSrcBits = aSrcMemoryImage->GetBits();

	int aPhysSrcHeight = aSrcMemoryImage->mHeight;
	int aPhysSrcWidth = aSrcMemoryImage->mWidth;
	for (int x = 0; x < aPhysSrcWidth; x++)
	{
		ulong* aTopBits    = aSrcBits + x;
		ulong* aBottomBits = aTopBits + (aPhysSrcWidth * (aPhysSrcHeight - 1));

		for (int y = 0; y < (aPhysSrcHeight >> 1); y++)
		{
			ulong aSwap = *aTopBits;

			*aTopBits = *aBottomBits;
			aTopBits += aPhysSrcWidth;
			*aBottomBits = aSwap;
			aBottomBits -= aPhysSrcWidth;
		}
	}

	aSrcMemoryImage->BitsChanged();	
}

void SexyAppBase::RotateImageHue(Sexy::MemoryImage *theImage, int theDelta)
{
	while (theDelta < 0)
		theDelta += 256;

	int aSize = theImage->mWidth * theImage->mHeight;
	DWORD *aPtr = theImage->GetBits();
	for (int i=0; i<aSize; i++)
	{
		DWORD aPixel = *aPtr;
		int alpha = aPixel&0xff000000;
		int r = (aPixel>>16)&0xff;
		int g = (aPixel>>8) &0xff;
		int b = aPixel&0xff;

		int maxval = std::max(r, std::max(g, b));
		int minval = std::min(r, std::min(g, b));
		int h = 0;
		int s = 0;
		int l = (minval+maxval)/2;
		int delta = maxval - minval;

		if (delta != 0)
		{			
			s = (delta * 256) / ((l <= 128) ? (minval + maxval) : (512 - maxval - minval));
			
			if (r == maxval)
				h = (g == minval ? 1280 + (((maxval-b) * 256) / delta) :  256 - (((maxval - g) * 256) / delta));
			else if (g == maxval)
				h = (b == minval ?  256 + (((maxval-r) * 256) / delta) :  768 - (((maxval - b) * 256) / delta));
			else
				h = (r == minval ?  768 + (((maxval-g) * 256) / delta) : 1280 - (((maxval - r) * 256) / delta));
			
			h /= 6;
		}

		h += theDelta;
		if (h >= 256)
			h -= 256;

		double v= (l < 128) ? (l * (255+s))/255 :
				(l+s-l*s/255);
		
		int y = (int) (2*l-v);

		int aColorDiv = (6 * h) / 256;
		int x = (int)(y+(v-y)*((h - (aColorDiv * 256 / 6)) * 6)/255);
		if (x > 255)
			x = 255;

		int z = (int) (v-(v-y)*((h - (aColorDiv * 256 / 6)) * 6)/255);
		if (z < 0)
			z = 0;
		
		switch (aColorDiv)
		{
			case 0: r = (int) v; g = x; b = y; break;
			case 1: r = z; g= (int) v; b = y; break;
			case 2: r = y; g= (int) v; b = x; break;
			case 3: r = y; g = z; b = (int) v; break;
			case 4: r = x; g = y; b = (int) v; break;
			case 5: r = (int) v; g = y; b = z; break;
			default: r = (int) v; g = x; b = y; break;
		}

		*aPtr++ = alpha | (r<<16) | (g << 8) | (b);	 

	}

	theImage->BitsChanged();
}

ulong SexyAppBase::HSLToRGB(int h, int s, int l)
{
	int r;
	int g;
	int b;

	double v= (l < 128) ? (l * (255+s))/255 :
			(l+s-l*s/255);
	
	int y = (int) (2*l-v);

	int aColorDiv = (6 * h) / 256;
	int x = (int)(y+(v-y)*((h - (aColorDiv * 256 / 6)) * 6)/255);
	if (x > 255)
		x = 255;

	int z = (int) (v-(v-y)*((h - (aColorDiv * 256 / 6)) * 6)/255);
	if (z < 0)
		z = 0;
	
	switch (aColorDiv)
	{
		case 0: r = (int) v; g = x; b = y; break;
		case 1: r = z; g= (int) v; b = y; break;
		case 2: r = y; g= (int) v; b = x; break;
		case 3: r = y; g = z; b = (int) v; break;
		case 4: r = x; g = y; b = (int) v; break;
		case 5: r = (int) v; g = y; b = z; break;
		default: r = (int) v; g = x; b = y; break;
	}

	return 0xFF000000 | (r << 16) | (g << 8) | (b);
}

ulong SexyAppBase::RGBToHSL(int r, int g, int b)
{					
	int maxval = std::max(r, std::max(g, b));
	int minval = std::min(r, std::min(g, b));
	int hue = 0;
	int saturation = 0;
	int luminosity = (minval+maxval)/2;
	int delta = maxval - minval;

	if (delta != 0)
	{			
		saturation = (delta * 256) / ((luminosity <= 128) ? (minval + maxval) : (512 - maxval - minval));
		
		if (r == maxval)
			hue = (g == minval ? 1280 + (((maxval-b) * 256) / delta) :  256 - (((maxval - g) * 256) / delta));
		else if (g == maxval)
			hue = (b == minval ?  256 + (((maxval-r) * 256) / delta) :  768 - (((maxval - b) * 256) / delta));
		else
			hue = (r == minval ?  768 + (((maxval-g) * 256) / delta) : 1280 - (((maxval - r) * 256) / delta));
		
		hue /= 6;
	}

	return 0xFF000000 | (hue) | (saturation << 8) | (luminosity << 16);	 
}

void SexyAppBase::HSLToRGB(const ulong* theSource, ulong* theDest, int theSize)
{
	for (int i = 0; i < theSize; i++)
	{
		ulong src = theSource[i];
		theDest[i] = (src & 0xFF000000) | (HSLToRGB((src & 0xFF), (src >> 8) & 0xFF, (src >> 16) & 0xFF) & 0x00FFFFFF);
	}
}

void SexyAppBase::RGBToHSL(const ulong* theSource, ulong* theDest, int theSize)
{
	for (int i = 0; i < theSize; i++)
	{
		ulong src = theSource[i];
		theDest[i] = (src & 0xFF000000) | (RGBToHSL(((src >> 16) & 0xFF), (src >> 8) & 0xFF, (src & 0xFF)) & 0x00FFFFFF);
	}
}

void SexyAppBase::PrecacheAdditive(MemoryImage* theImage)
{
	theImage->GetRLAdditiveData(mDisplay);
}

void SexyAppBase::PrecacheAlpha(MemoryImage* theImage)
{
	theImage->GetRLAlphaData();
}

void SexyAppBase::PrecacheNative(MemoryImage* theImage)
{
	theImage->GetNativeAlphaData(mDisplay);
}


void SexyAppBase::PlaySample(int theSoundNum)
{
	if (!mSoundManager)
		return;

	SoundInstance* aSoundInstance = mSoundManager->GetSoundInstance(theSoundNum);
	if (aSoundInstance != NULL)
	{
		aSoundInstance->Play(false, true);
	}
}


void SexyAppBase::PlaySample(int theSoundNum, int thePan)
{
	if (!mSoundManager)
		return;

	SoundInstance* aSoundInstance = mSoundManager->GetSoundInstance(theSoundNum);
	if (aSoundInstance != NULL)
	{
		aSoundInstance->SetPan(thePan);
		aSoundInstance->Play(false, true);
	}
}

bool SexyAppBase::IsMuted()
{
	return mMuteCount > 0;
}

void SexyAppBase::Mute(bool autoMute)
{	
	mMuteCount++;
	if (autoMute)
		mAutoMuteCount++;

	SetMusicVolume(mMusicVolume);
	SetSfxVolume(mSfxVolume);
}

void SexyAppBase::Unmute(bool autoMute)
{	
	if (mMuteCount > 0)
	{
		mMuteCount--;
		if (autoMute)
			mAutoMuteCount--;
	}

	SetMusicVolume(mMusicVolume);
	SetSfxVolume(mSfxVolume);
}


double SexyAppBase::GetMusicVolume()
{
	return mMusicVolume;
}

void SexyAppBase::SetMusicVolume(double theVolume)
{
	mMusicVolume = theVolume;

	if (mMusicInterface != NULL)
		mMusicInterface->SetVolume((mMuteCount > 0) ? 0.0 : mMusicVolume);
}

double SexyAppBase::GetSfxVolume()
{
	return mSfxVolume;
}

void SexyAppBase::SetSfxVolume(double theVolume)
{
	mSfxVolume = theVolume;

	if (mSoundManager != NULL)
		mSoundManager->SetVolume((mMuteCount > 0) ? 0.0 : mSfxVolume);
}

double SexyAppBase::GetMasterVolume()
{
	return mSoundManager->GetMasterVolume();
}

void SexyAppBase::SetMasterVolume(double theMasterVolume)
{
	mSfxVolume = theMasterVolume;
	mSoundManager->SetMasterVolume(mSfxVolume);
}

void SexyAppBase::AddMemoryImage(MemoryImage* theMemoryImage)
{
	AutoCrit anAutoCrit(mCritSect);
	mMemoryImageSet.insert(theMemoryImage);
}

void SexyAppBase::RemoveMemoryImage(MemoryImage* theMemoryImage)
{
	AutoCrit anAutoCrit(mCritSect);
	MemoryImageSet::iterator anItr = mMemoryImageSet.find(theMemoryImage);
	if (anItr != mMemoryImageSet.end())
		mMemoryImageSet.erase(anItr);

	Remove3DData(theMemoryImage);
}

void SexyAppBase::Remove3DData(MemoryImage* theMemoryImage)
{
	if (mDisplay)
		mDisplay->Remove3DData(theMemoryImage);
}


SharedImageRef SexyAppBase::SetSharedImage(const std::string& theFileName, const std::string& theVariant, DeviceImage* theImage, bool* isNew)
{
	std::string anUpperFileName = StringToUpper(theFileName);
	std::string anUpperVariant = StringToUpper(theVariant);

	std::pair<SharedImageMap::iterator, bool> aResultPair;
	SharedImageRef aSharedImageRef;
	
	{
		AutoCrit anAutoCrit(mCritSect);
		aResultPair = mSharedImageMap.insert(SharedImageMap::value_type(SharedImageMap::key_type(anUpperFileName, anUpperVariant), SharedImage()));
		aSharedImageRef = &aResultPair.first->second;
	}

	if (isNew != NULL)
		*isNew = aResultPair.second;

	if (aResultPair.second)
	{
		aSharedImageRef.mSharedImage->mImage = theImage;
	}

	return aSharedImageRef;
}

SharedImageRef SexyAppBase::GetSharedImage(const std::string& theFileName, const std::string& theVariant, bool* isNew)
{	
	std::string anUpperFileName = StringToUpper(theFileName);
	std::string anUpperVariant = StringToUpper(theVariant);

	std::pair<SharedImageMap::iterator, bool> aResultPair;
	SharedImageRef aSharedImageRef;

	{
		AutoCrit anAutoCrit(mCritSect);	
		aResultPair = mSharedImageMap.insert(SharedImageMap::value_type(SharedImageMap::key_type(anUpperFileName, anUpperVariant), SharedImage()));
		aSharedImageRef = &aResultPair.first->second;
	}

	if (isNew != NULL)
		*isNew = aResultPair.second;

	if (aResultPair.second)
	{
		// Pass in a '!' as the first char of the file name to create a new image
		if ((theFileName.length() > 0) && (theFileName[0] == '!'))
			aSharedImageRef.mSharedImage->mImage = new DeviceImage(mDisplay);
		else
			aSharedImageRef.mSharedImage->mImage = GetImage(theFileName,false);
	}

	return aSharedImageRef;
}

void SexyAppBase::CleanSharedImages()
{
	AutoCrit anAutoCrit(mCritSect);	

	if (mCleanupSharedImages)
	{
		// Delete shared images with reference counts of 0
		// This doesn't occur in ~SharedImageRef because sometimes we can not only access the image
		//  through the SharedImageRef returned by GetSharedImage, but also by calling GetSharedImage
		//  again with the same params -- so we can have instances where we do the 'final' deref on
		//  an image but immediately re-request it via GetSharedImage
		SharedImageMap::iterator aSharedImageItr = mSharedImageMap.begin();
		while (aSharedImageItr != mSharedImageMap.end())
		{
			SharedImage* aSharedImage = &aSharedImageItr->second;
			if (aSharedImage->mRefCount == 0)
			{
				delete aSharedImage->mImage;
				mSharedImageMap.erase(aSharedImageItr++);
			}
			else
				++aSharedImageItr;
		}

		mCleanupSharedImages = false;
	}
}

void SexyAppBase::MakeWindow()
{
	if (mWindow == NULL)
	{
		if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_InitSubSystem(SDL_INIT_VIDEO))
		{
			Popup(std::string("Unable to initialize video: ") + SDL_GetError());
			DoExit(1);
		}

		SDL_WindowFlags aFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
#if defined(SEXY_RENDER_BACKEND_METAL)
		aFlags |= SDL_WINDOW_METAL;
#endif

		mWindow = SDL_CreateWindow(SexyStringToStringFast(mTitle).c_str(), mWidth, mHeight, aFlags);
		if (mWindow == NULL)
		{
			Popup(std::string("Unable to create the game window: ") + SDL_GetError());
			DoExit(1);
		}

		SDL_SetWindowMinimumSize(mWindow, mWidth / 2, mHeight / 2);
		float anAspect = (float)mWidth / mHeight;
		SDL_SetWindowAspectRatio(mWindow, anAspect, anAspect);

		if (mPreferredX != -1 && mPreferredY != -1)
			SDL_SetWindowPosition(mWindow, mPreferredX, mPreferredY);
		else
			SDL_SetWindowPosition(mWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

#if defined(SEXY_RENDER_BACKEND_METAL)
		mRenderDevice = CreateMetalRenderDevice(mWindow);
#endif
		if (mRenderDevice == NULL)
		{
			Popup("Unable to initialize the graphics device.");
			DoExit(1);
		}

		mDisplay = new DisplayInterface(this, mRenderDevice);

		// Hardware acceleration is on unless the player turned it off.
		bool is3D = true;
		RegistryReadBoolean("Is3D", &is3D);
		mDisplay->mIs3D = is3D && !mForceSoftwareRendering;

		SDL_StartTextInput(mWindow);
	}

	mIsPhysWindowed = mIsWindowed;
	SDL_SetWindowFullscreen(mWindow, !mIsWindowed);

	int aResult = InitDDInterface();
	if (aResult != DisplayInterface::RESULT_OK)
	{
		if (Is3DAccelerated())
		{
			Set3DAcclerated(false);
			return;
		}

		Popup(GetString("FAILED_INIT_DIRECTDRAW", _S("Failed to initialize the display: ")) + StringToSexyString(DisplayInterface::ResultToString(aResult) + " " + mDisplay->mErrorString));
		DoExit(1);
	}

	mRenderDevice->SetVSync(true);

	bool isActive = mActive;
	mActive = (SDL_GetWindowFlags(mWindow) & SDL_WINDOW_INPUT_FOCUS) != 0 || !mInitialized;

	mPhysMinimized = false;
	if (mMinimized)
	{
		if (mMuteOnLostFocus)
			Unmute(true);

		mMinimized = false;
		isActive = mActive; // set this here so we don't call RehupFocus again.
		RehupFocus();
	}

	if (isActive != mActive)
		RehupFocus();

	ReInitImages();

	mWidgetManager->mImage = mDisplay->GetScreenImage();
	mWidgetManager->MarkAllDirty();
}

void SexyAppBase::LoadingThreadProcStub(void *theArg)
{
	SexyAppBase* aSexyApp = (SexyAppBase*) theArg;

	aSexyApp->LoadingThreadProc();

	char aStr[256];
	snprintf(aStr, sizeof(aStr), "Resource Loading Time: %u\r\n", (unsigned)(GetTickCount() - aSexyApp->mTimeLoaded));
	OutputDebugString(aStr);

	aSexyApp->mLoadingThreadCompleted = true;
}

void SexyAppBase::StartLoadingThread()
{
	if (!mLoadingThreadStarted)
	{
		mYieldMainThread = true;
		mLoadingThreadStarted = true;
		std::thread(LoadingThreadProcStub, this).detach();
	}
}

void SexyAppBase::SwitchScreenMode(bool wantWindowed, bool is3d, bool force)
{
	if (mForceFullscreen)
		wantWindowed = false;

	if (mIsWindowed == wantWindowed && !force)
	{
		Set3DAcclerated(is3d);
		return;
	}

	// Set 3d acceleration preference
	Set3DAcclerated(is3d,false);

	mIsWindowed = wantWindowed;

	MakeWindow();

	if (!mIsOpeningURL)
	{
		SDL_ShowWindow(mWindow);
		SDL_RaiseWindow(mWindow);
	}
	else
	{
		SDL_ShowWindow(mWindow);
	}

	mLastTime = timeGetTime();
}

void SexyAppBase::SwitchScreenMode(bool wantWindowed)
{
	SwitchScreenMode(wantWindowed, Is3DAccelerated());
}

void SexyAppBase::SwitchScreenMode()
{
	SwitchScreenMode(mIsWindowed, Is3DAccelerated(), true);
}

void SexyAppBase::SetSystemCursor(int theCursorNum)
{
	if (theCursorNum == CURSOR_NONE || theCursorNum == CURSOR_CUSTOM)
	{
		SDL_HideCursor();
		return;
	}

	if (theCursorNum < 0 || theCursorNum >= NUM_CURSORS)
		theCursorNum = CURSOR_POINTER;

	if (mSystemCursors[theCursorNum] == NULL)
	{
		switch (theCursorNum)
		{
		case CURSOR_HAND:			mSystemCursors[theCursorNum] = CreateMonochromeCursor(gFingerCursorData, 11, 4); break;
		case CURSOR_DRAGGING:		mSystemCursors[theCursorNum] = CreateMonochromeCursor(gDraggingCursorData, 15, 10); break;
		case CURSOR_TEXT:			mSystemCursors[theCursorNum] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT); break;
		case CURSOR_CIRCLE_SLASH:	mSystemCursors[theCursorNum] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED); break;
		case CURSOR_SIZEALL:		mSystemCursors[theCursorNum] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE); break;
		case CURSOR_SIZENESW:		mSystemCursors[theCursorNum] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE); break;
		case CURSOR_SIZENS:			mSystemCursors[theCursorNum] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE); break;
		case CURSOR_SIZENWSE:		mSystemCursors[theCursorNum] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE); break;
		case CURSOR_SIZEWE:			mSystemCursors[theCursorNum] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE); break;
		case CURSOR_WAIT:			mSystemCursors[theCursorNum] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT); break;
		default:					mSystemCursors[theCursorNum] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT); break;
		}
	}

	if (mSystemCursors[theCursorNum] != NULL)
		SDL_SetCursor(mSystemCursors[theCursorNum]);
	SDL_ShowCursor();
}

void SexyAppBase::SetCursorBitmap(int theCursorNum, const std::uint32_t* thePixels, int theWidth, int theHeight, int theHotX, int theHotY)
{
	if (theCursorNum < 0 || theCursorNum >= NUM_CURSORS)
		return;

	SDL_Surface* aSurface = SDL_CreateSurfaceFrom(theWidth, theHeight, SDL_PIXELFORMAT_ARGB8888, (void*)thePixels, theWidth * 4);
	if (aSurface == NULL)
		return;

	SDL_Cursor* aCursor = SDL_CreateColorCursor(aSurface, theHotX, theHotY);
	SDL_DestroySurface(aSurface);
	if (aCursor == NULL)
		return;

	if (mSystemCursors[theCursorNum] != NULL)
		SDL_DestroyCursor(mSystemCursors[theCursorNum]);
	mSystemCursors[theCursorNum] = aCursor;
	EnforceCursor();
}

void SexyAppBase::UpdateCustomCursor(Image* theImage)
{
	if (theImage == mCustomCursorImage && mCustomCursor != NULL)
	{
		SDL_SetCursor(mCustomCursor);
		SDL_ShowCursor();
		return;
	}

	MemoryImage* aMemoryImage = dynamic_cast<MemoryImage*>(theImage);
	if (aMemoryImage == NULL || aMemoryImage->GetBits() == NULL)
	{
		SetSystemCursor(CURSOR_POINTER);
		return;
	}

	SDL_Surface* aSurface = SDL_CreateSurfaceFrom(aMemoryImage->mWidth, aMemoryImage->mHeight, SDL_PIXELFORMAT_ARGB8888, aMemoryImage->GetBits(), aMemoryImage->mWidth * 4);
	if (aSurface == NULL)
		return;

	SDL_Cursor* aCursor = SDL_CreateColorCursor(aSurface, aMemoryImage->mWidth / 2, aMemoryImage->mHeight / 2);
	SDL_DestroySurface(aSurface);
	if (aCursor == NULL)
		return;

	SDL_SetCursor(aCursor);
	SDL_ShowCursor();
	if (mCustomCursor != NULL)
		SDL_DestroyCursor(mCustomCursor);
	mCustomCursor = aCursor;
	mCustomCursorImage = theImage;
}

void SexyAppBase::EnforceCursor()
{
	if (mDisplay == NULL || mWindow == NULL)
		return;

	bool wantSysCursor = true;

	if ((mSEHOccured) || (!mMouseIn))
	{
		SetSystemCursor(CURSOR_POINTER);
		if (mDisplay->SetCursorImage(NULL))
			mCustomCursorDirty = true;
	}
	else
	{
		if ((mCursorImages[mCursorNum] == NULL) ||
			((!mCustomCursorsEnabled) && (mCursorNum != CURSOR_CUSTOM)))
		{
			SetSystemCursor(mCursorNum);

			if (mDisplay->SetCursorImage(NULL))
				mCustomCursorDirty = true;
		}
		else
		{
			if (mDisplay->SetCursorImage(mCursorImages[mCursorNum]))
				mCustomCursorDirty = true;

			UpdateCustomCursor(mCursorImages[mCursorNum]);
			wantSysCursor = false;
		}
	}

	mSysCursor = wantSysCursor;
}

bool SexyAppBase::Process(bool allowSleep)
{
	if (mLoadingFailed)
		Shutdown();

	bool isVSynched = (mVSyncUpdates) && (!mLastDrawWasEmpty) && (!mVSyncBroken) &&
		((!mIsPhysWindowed) || (mIsPhysWindowed && mWaitForVSync && !mSoftVSyncWait));
	double aFrameFTime;
	double anUpdatesPerUpdateF;

	if (mVSyncUpdates)
	{
		aFrameFTime = (1000.0 / mSyncRefreshRate) / mUpdateMultiplier;
		anUpdatesPerUpdateF = (float) (1000.0 / (mFrameTime * mSyncRefreshRate));
	}
	else
	{
		aFrameFTime = mFrameTime / mUpdateMultiplier;
		anUpdatesPerUpdateF = 1.0;
	}

	// Make sure we're not paused
	if ((!mPaused) && (mUpdateMultiplier > 0))
	{
		ulong aStartTime = timeGetTime();

		int aCumSleepTime = 0;

		// When we are VSynching, only calculate this FTimeAcc right after drawing
		if (!isVSynched)
			UpdateFTimeAcc();

		// mNonDrawCount is used to make sure we draw the screen at least
		// 10 times per second, even if it means we have to slow down
		// the updates to make it draw 10 times per second in "game time"

		bool didUpdate = false;

		if (mUpdateAppState == UPDATESTATE_PROCESS_1)
		{
			if ((++mNonDrawCount < (int) ceil(10*mUpdateMultiplier)) || (!mLoaded))
			{
				bool doUpdate = false;

				if (isVSynched)
				{
					// Synch'ed to vertical refresh, so update as soon as possible after draw
					doUpdate = (!mHasPendingDraw) || (mUpdateFTimeAcc >= (int) (aFrameFTime * 0.75));
				}
				else if (mUpdateFTimeAcc >= aFrameFTime)
				{
					doUpdate = true;
				}

				if (doUpdate)
				{
					// Do VSyncBroken test.  This test fails if we're in fullscreen and
					// "don't vsync" has been forced in Advanced settings up Display Properties
					if (mUpdateMultiplier == 1.0)
					{
						mVSyncBrokenTestUpdates++;
						if (mVSyncBrokenTestUpdates >= (DWORD) ((1000+mFrameTime-1)/mFrameTime))
						{
							// It has to be running 33% fast to be "broken" (25% = 1/0.800)
							if (aStartTime - mVSyncBrokenTestStartTick <= 800)
							{
								// The test has to fail 3 times in a row before we decide that
								//  vsync is broken overall
								mVSyncBrokenCount++;
								if (mVSyncBrokenCount >= 3)
									mVSyncBroken = true;
							}
							else
								mVSyncBrokenCount = 0;

							mVSyncBrokenTestStartTick = aStartTime;
							mVSyncBrokenTestUpdates = 0;
						}
					}

					bool hadRealUpdate = DoUpdateFrames();
					if (hadRealUpdate)
						mUpdateAppState = UPDATESTATE_PROCESS_2;

					mHasPendingDraw = true;
					didUpdate = true;
				}
			}
		}
		else if (mUpdateAppState == UPDATESTATE_PROCESS_2)
		{
			mUpdateAppState = UPDATESTATE_PROCESS_DONE;

			mPendingUpdatesAcc += anUpdatesPerUpdateF;
			mPendingUpdatesAcc -= 1.0;
			ProcessSafeDeleteList();

			// Process any extra updates
			while (mPendingUpdatesAcc >= 1.0)
			{
				++mNonDrawCount;
				bool hasRealUpdate = DoUpdateFrames();
				DBG_ASSERTE(hasRealUpdate);

				if (!hasRealUpdate)
					break;

				ProcessSafeDeleteList();
				mPendingUpdatesAcc -= 1.0;
			}

			DoUpdateFramesF((float) anUpdatesPerUpdateF);
			ProcessSafeDeleteList();

			// Don't let mUpdateFTimeAcc dip below 0
			//  Subtract an extra 0.2ms, because sometimes refresh rates have some
			//  fractional component that gets truncated, and it's better to take off
			//  too much to keep our timing tending toward occuring right after
			//  redraws
			if (isVSynched)
				mUpdateFTimeAcc = std::max(mUpdateFTimeAcc - aFrameFTime - 0.2f, 0.0);
			else
				mUpdateFTimeAcc -= aFrameFTime;

			if (mRelaxUpdateBacklogCount > 0)
				mUpdateFTimeAcc = 0;

			didUpdate = true;
		}

		if (!didUpdate)
		{
			mUpdateAppState = UPDATESTATE_PROCESS_DONE;

			mNonDrawCount = 0;

			if (mHasPendingDraw)
			{
				DrawDirtyStuff();
			}
			else
			{
				// Let us take into account the time it took to draw dirty stuff
				int aTimeToNextFrame = (int) (aFrameFTime - mUpdateFTimeAcc);
				if (aTimeToNextFrame > 0)
				{
					if (!allowSleep)
						return false;

					// Wait till next processing cycle
					++mSleepCount;
					Sleep(aTimeToNextFrame);

					aCumSleepTime += aTimeToNextFrame;
				}
			}
		}

		if (mYieldMainThread)
		{
			// This is to make sure that the title screen doesn't take up any more than
			// 1/3 of the processor time

			ulong anEndTime = timeGetTime();
			int anElapsedTime = (anEndTime - aStartTime) - aCumSleepTime;
			int aLoadingYieldSleepTime = std::min(250, (anElapsedTime * 2) - aCumSleepTime);

			if (aLoadingYieldSleepTime >= 0)
			{
				if (!allowSleep)
					return false;

				Sleep(aLoadingYieldSleepTime);
			}
		}
	}

	ProcessSafeDeleteList();
	return true;
}

void SexyAppBase::DoMainLoop()
{
	while (!mShutdown)
	{
		if (mExitToTop)
			mExitToTop = false;
		UpdateApp();
	}
}

bool SexyAppBase::UpdateAppStep(bool* updated)
{
	if (updated != NULL)
		*updated = false;

	if (mExitToTop)
		return false;

	if (mUpdateAppState == UPDATESTATE_PROCESS_DONE)
		mUpdateAppState = UPDATESTATE_MESSAGES;

	mUpdateAppDepth++;

	// We update in two stages to avoid doing a Process if our loop termination
	//  condition has already been met by processing windows messages
	if (mUpdateAppState == UPDATESTATE_MESSAGES)
	{
		RunInputScript();
		if (!ProcessEvents(true))
		{
			mUpdateAppState = UPDATESTATE_PROCESS_1;
		}
	}
	else
	{
		// Process changes state by itself
		if (mStepMode)
		{
			if (mStepMode==2)
			{
				Sleep(mFrameTime);
				mUpdateAppState = UPDATESTATE_PROCESS_DONE; // skip actual update until next step
			}
			else
			{
				mStepMode = 2;
				DoUpdateFrames();
				DoUpdateFramesF(1.0f);
				DrawDirtyStuff();
			}
		}
		else
		{
			int anOldUpdateCnt = mUpdateCount;
			Process();
			if (updated != NULL)
				*updated = mUpdateCount != anOldUpdateCnt;
		}
	}

	mUpdateAppDepth--;

	return true;
}

int SexyAppBase::InitDDInterface()
{
	PreDDInterfaceInitHook();
	DeleteNativeImageData();
	int aResult = mDisplay->Init(mIsPhysWindowed);
	if ( DisplayInterface::RESULT_OK == aResult )
	{
		mScreenBounds.mX = ( mWidth - mDisplay->mWidth ) / 2;
		mScreenBounds.mY = ( mHeight - mDisplay->mHeight ) / 2;
		mScreenBounds.mWidth = mDisplay->mWidth;
		mScreenBounds.mHeight = mDisplay->mHeight;
		mWidgetManager->Resize(mScreenBounds, mDisplay->mPresentationRect);
		PostDDInterfaceInitHook();
	}
	return aResult;
}

void SexyAppBase::Start()
{
	if (mShutdown)
		return;

	if (mAutoStartLoadingThread)
		StartLoadingThread();

	SDL_ShowWindow(mWindow);
	SDL_RaiseWindow(mWindow);

	DWORD aStartTime = timeGetTime();

	mRunning = true;
	mLastTime = aStartTime;
	mLastUserInputTick = aStartTime;
	mLastTimerTime = aStartTime;

	DoMainLoop();
	ProcessSafeDeleteList();

	mRunning = false;

	WaitForLoadingThread();

	char aString[256];
	snprintf(aString, sizeof(aString), "Seconds       = %g\r\n", (timeGetTime() - aStartTime) / 1000.0);
	OutputDebugString(aString);
	snprintf(aString, sizeof(aString), "Sleep Count   = %d\r\n", mSleepCount);
	OutputDebugString(aString);
	snprintf(aString, sizeof(aString), "Update Count  = %d\r\n", mUpdateCount);
	OutputDebugString(aString);
	snprintf(aString, sizeof(aString), "Draw Count    = %d\r\n", mDrawCount);
	OutputDebugString(aString);

	PreTerminate();

	WriteToRegistry();
}

void SexyAppBase::SetCommandLine(int argc, char** argv)
{
	mCmdLineArgs.clear();
	for (int i = 1; i < argc; i++)
		mCmdLineArgs.push_back(argv[i]);
}

void SexyAppBase::DoParseCmdLine()
{
	for (const std::string& anArg : mCmdLineArgs)
	{
		// macOS passes a process serial number when launched from Finder.
		if (anArg.compare(0, 5, "-psn_") == 0)
			continue;

		size_t anEquals = anArg.find('=');
		if (anEquals == std::string::npos)
			HandleCmdLineParam(anArg, "");
		else
		{
			std::string aValue = anArg.substr(anEquals + 1);
			if (aValue.size() >= 2 && aValue.front() == '"' && aValue.back() == '"')
				aValue = aValue.substr(1, aValue.size() - 2);
			HandleCmdLineParam(anArg.substr(0, anEquals), aValue);
		}
	}

	mCmdLineParsed = true;
}

void SexyAppBase::HandleCmdLineParam(const std::string& theParamName, const std::string& theParamValue)
{
	if (theParamName == "-crash")
	{
		// Try to access NULL
		volatile char* a = 0;
		*a = '!';
	}
	else if (theParamName == "-screensaver")
	{
		mIsScreenSaver = true;
	}
	else if (theParamName == "-changedir")
	{
		mChangeDirTo = theParamValue;
	}
	else if (theParamName == "-nosound")
	{
		mNoSoundNeeded = true;
	}
	else if (theParamName == "-windowed")
	{
		mCmdLineWindowMode = 1;
	}
	else if (theParamName == "-fullscreen")
	{
		mCmdLineWindowMode = 2;
	}
	else if (theParamName == "-capturedir")
	{
		mCaptureDir = theParamValue;
	}
	else if (theParamName == "-captureinterval")
	{
		mCaptureIntervalMs = atoi(theParamValue.c_str());
	}
	else if (theParamName == "-inputscript")
	{
		LoadInputScript(theParamValue);
	}
	else if (theParamName == "-software")
	{
		mForceSoftwareRendering = true;
	}
	else
	{
		Popup(GetString("INVALID_COMMANDLINE_PARAM", _S("Invalid command line parameter: ")) + StringToSexyString(theParamName));
		DoExit(0);
	}
}

MusicInterface* SexyAppBase::CreateMusicInterface()
{
	if (mNoSoundNeeded || mAudioMixer == NULL)
		return new DummyMusicInterface();
	else
		return new ModuleMusicInterface(mAudioMixer);
}

void SexyAppBase::Init()
{
	mPrimaryThreadId = GetCurrentThreadId();

	if (mShutdown)
		return;

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		Platform::ShowMessageBox("Fatal Error", std::string("Unable to initialize SDL: ") + SDL_GetError(), MB_OK | MB_ICONERROR);
		DoExit(1);
	}

	if (!mCmdLineParsed)
		DoParseCmdLine();

	if (IsScreenSaver())	
		mOnlyAllowOneCopyToRun = false;	

	// Change to the game directory first: on Windows the process started
	// there, and the properties loaded next are relative paths.
	if (!ChangeDirHook(mChangeDirTo.c_str()))
	{
		if (chdir(FixPath(mChangeDirTo).c_str()) != 0)
			OutputDebugString(("Unable to change directory to " + mChangeDirTo + "\n").c_str());
	}

	InitPropertiesHook();

	// Per-user data (profiles, saves) and the settings store. The product
	// name may carry a company prefix ("PopCap\\PlantsVsZombies").
	std::string aCompany = mFullCompanyName.empty() ? mCompanyName : mFullCompanyName;
	std::string aProduct = mProdName;
	size_t aSeparator = aProduct.find_last_of("\\/");
	if (aSeparator != std::string::npos)
	{
		if (mFullCompanyName.empty())
			aCompany = aProduct.substr(0, aSeparator);
		aProduct = aProduct.substr(aSeparator + 1);
	}

	std::string aDataPath = Platform::GetUserDataDirectory(aCompany, aProduct);
	if (!aDataPath.empty())
	{
		SetAppDataFolder(aDataPath);
		mRegistry.Load(GetAppDataFolder() + "registry.ini");
	}

	ReadFromRegistry();	

	if (mCmdLineWindowMode != 0)
		mIsWindowed = mCmdLineWindowMode == 1;

	if (!gPakInterface->AddPakFile("main.pak"))
		OutputDebugString("main.pak not found; reading resources from the file system\n");

	mRandSeed = GetTickCount();
	SRand(mRandSeed);

	srand(GetTickCount());

	// Let app do something before showing window, or switching to fullscreen mode
	// PreDisplayHook must call mWidgetManager->Resize if it changes mWidth or mHeight.
	PreDisplayHook();

	mWidgetManager->Resize(Rect(0, 0, mWidth, mHeight), Rect(0, 0, mWidth, mHeight));

	MakeWindow();

	mAudioMixer = new AudioMixer(44100);
	if (!mNoSoundNeeded)
	{
		mAudioDevice = CreatePlatformAudioDevice();
		if (!mAudioDevice->Start(mAudioMixer))
		{
			delete mAudioDevice;
			mAudioDevice = NULL;
		}
	}

	if (mSoundManager == NULL)
		mSoundManager = new MixerSoundManager(mAudioMixer);

	SetSfxVolume(mSfxVolume);

	mMusicInterface = CreateMusicInterface();

	SetMusicVolume(mMusicVolume);

	if (IsScreenSaver())
	{
		SetCursor(CURSOR_NONE);
	}

	InitHook();

	mInitialized = true;
}

void SexyAppBase::HandleGameAlreadyRunning()
{
}

void SexyAppBase::CopyToClipboard(const std::string& theString)
{
	Platform::SetClipboardText(theString);
}

std::string	SexyAppBase::GetClipboard()
{
	return Platform::GetClipboardText();
}

Sexy::DeviceImage* SexyAppBase::GetImage(const std::string& theFileName, bool commitBits)
{
	ImageLib::Image* aLoadedImage = ImageLib::GetImage(theFileName, true);

	if (aLoadedImage == NULL)
		return NULL;

	DeviceImage* anImage = new DeviceImage(mDisplay);
	anImage->mFilePath = theFileName;
	anImage->SetBits(aLoadedImage->GetBits(), aLoadedImage->GetWidth(), aLoadedImage->GetHeight(), commitBits);
	anImage->mFilePath = theFileName;
	delete aLoadedImage;

	return anImage;
}

bool SexyAppBase::Is3DAccelerated()
{
	return mDisplay != NULL && mDisplay->mIs3D;
}

bool SexyAppBase::Is3DAccelerationSupported()
{
	return mRenderDevice != NULL;
}

bool SexyAppBase::Is3DAccelerationRecommended()
{
	return mRenderDevice != NULL;
}

void SexyAppBase::Set3DAcclerated(bool is3D, bool reinit)
{
	if (mDisplay->mIs3D == is3D)
		return;

	mUserChanged3DSetting = true;
	mDisplay->mIs3D = is3D;

	if (reinit)
	{
		int aResult = InitDDInterface();

		if (is3D && aResult != DisplayInterface::RESULT_OK)
		{
			Set3DAcclerated(false, reinit);
			return;
		}
		else if (aResult != DisplayInterface::RESULT_OK)
		{
			Popup(GetString("FAILED_INIT_DIRECTDRAW", _S("Failed to initialize the display: ")) + StringToSexyString(DisplayInterface::ResultToString(aResult) + " " + mDisplay->mErrorString));
			DoExit(1);
		}

		ReInitImages();

		mWidgetManager->mImage = mDisplay->GetScreenImage();
		mWidgetManager->MarkAllDirty();
	}
}
