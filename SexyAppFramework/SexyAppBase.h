#ifndef __SEXYAPPBASE_H__
#define __SEXYAPPBASE_H__

#include "Common.h"
#include "misc/Rect.h"
#include "graphics/Color.h"
#include "widget/ButtonListener.h"
#include "widget/DialogListener.h"
#include "misc/Buffer.h"
#include "misc/CritSect.h"
#include "misc/RegistryStore.h"
#include "graphics/SharedImage.h"
#include "misc/Ratio.h"

#include <atomic>

extern bool gD3DInterfacePreDrawError;

struct SDL_Window;
struct SDL_Cursor;
union SDL_Event;

namespace ImageLib
{
	class Image;
};

namespace Sexy
{

class WidgetManager;
class DisplayInterface;
class RenderDevice;
class AudioMixer;
class AudioDevice;
class Image;
class DeviceImage;
class Widget;
class SoundManager;
class MusicInterface;
class MemoryImage;
class Dialog;

class ResourceManager;

class WidgetSafeDeleteInfo
{
public:
	int						mUpdateAppDepth;
	Widget*					mWidget;
};


typedef std::list<WidgetSafeDeleteInfo> WidgetSafeDeleteList;
typedef std::set<MemoryImage*> MemoryImageSet;
typedef std::map<int, Dialog*> DialogMap;
typedef std::list<Dialog*> DialogList;
typedef std::vector<std::string> StringVector;

typedef std::map<std::string, SexyString> StringSexyStringMap;
typedef std::map<std::string, std::string> StringStringMap;
typedef std::map<std::string, std::wstring> StringWStringMap;
typedef std::map<std::string, bool> StringBoolMap;
typedef std::map<std::string, int> StringIntMap;
typedef std::map<std::string, double> StringDoubleMap;
typedef std::map<std::string, StringVector> StringStringVectorMap;

enum
{
	CURSOR_POINTER,
	CURSOR_HAND,
	CURSOR_DRAGGING,
	CURSOR_TEXT,
	CURSOR_CIRCLE_SLASH,
	CURSOR_SIZEALL,
	CURSOR_SIZENESW,
	CURSOR_SIZENS,
	CURSOR_SIZENWSE,
	CURSOR_SIZEWE,
	CURSOR_WAIT,
	CURSOR_NONE,
	CURSOR_CUSTOM,
	NUM_CURSORS
};

enum {
	FPS_ShowFPS,
	FPS_ShowCoords,
	Num_FPS_Types
};

enum
{
	UPDATESTATE_MESSAGES,
	UPDATESTATE_PROCESS_1,
	UPDATESTATE_PROCESS_2,
	UPDATESTATE_PROCESS_DONE
};

class SexyAppBase : public ButtonListener, public DialogListener
{
public:
	ulong					mRandSeed;

	std::string				mCompanyName;
	std::string				mFullCompanyName;
	std::string				mProdName;
	SexyString				mTitle;
	std::string				mRegKey;
	std::string				mChangeDirTo;

	int						mRelaxUpdateBacklogCount; // app doesn't try to catch up for this many frames
	int						mPreferredX;
	int						mPreferredY;
	int						mWidth;
	int						mHeight;
	int						mFullscreenBits;
	double					mMusicVolume;
	double					mSfxVolume;
	bool					mNoSoundNeeded;
	bool					mWantFMod;
	bool					mCmdLineParsed;
	bool					mSkipSignatureChecks;
	bool					mStandardWordWrap;
	bool					mbAllowExtendedChars;

	bool					mOnlyAllowOneCopyToRun;
	CritSect				mCritSect;
	bool					mBetaValidate;
	uchar					mAdd8BitMaxTable[512];
	WidgetManager*			mWidgetManager;
	DialogMap				mDialogMap;
	DialogList				mDialogList;
	std::uint64_t			mPrimaryThreadId;
	bool					mSEHOccured;
	bool					mShutdown;
	bool					mExitToTop;
	bool					mIsWindowed;
	bool					mIsPhysWindowed;
	bool					mFullScreenWindow;
	bool					mForceFullscreen;
	bool					mForceWindowed;
	bool					mInitialized;
	bool					mProcessInTimer;
	DWORD					mTimeLoaded;
	SDL_Window*				mWindow;
	bool					mIsScreenSaver;
	bool					mAllowMonitorPowersave;
	bool					mNoDefer;
	bool					mFullScreenPageFlip;
	bool					mTabletPC;
	RenderDevice*			mRenderDevice;
	DisplayInterface*		mDisplay;
	AudioMixer*				mAudioMixer;
	AudioDevice*			mAudioDevice;
	RegistryStore			mRegistry;
	bool					mAlphaDisabled;
	MusicInterface*			mMusicInterface;
	bool					mReadFromRegistry;
	std::string				mRegisterLink;
	std::string				mProductVersion;
	Image*					mCursorImages[NUM_CURSORS];
	SDL_Cursor*				mSystemCursors[NUM_CURSORS];
	SDL_Cursor*				mCustomCursor;
	Image*					mCustomCursorImage;
	bool					mIsOpeningURL;
	bool					mShutdownOnURLOpen;
	std::string				mOpeningURL;
	DWORD					mOpeningURLTime;
	DWORD					mLastTimerTime;
	DWORD					mLastBigDelayTime;
	double					mUnmutedMusicVolume;
	double					mUnmutedSfxVolume;
	int						mMuteCount;
	int						mAutoMuteCount;
	bool					mDemoMute;
	bool					mMuteOnLostFocus;
	MemoryImageSet			mMemoryImageSet;
	SharedImageMap			mSharedImageMap;
	bool					mCleanupSharedImages;

	int						mNonDrawCount;
	int						mFrameTime;

	bool					mIsDrawing;
	bool					mLastDrawWasEmpty;
	bool					mHasPendingDraw;
	double					mPendingUpdatesAcc;
	double					mUpdateFTimeAcc;
	DWORD					mLastTimeCheck;
	DWORD					mLastTime;
	DWORD					mLastUserInputTick;

	int						mSleepCount;
	int						mDrawCount;
	int						mUpdateCount;
	int						mUpdateAppState;
	int						mUpdateAppDepth;
	double					mUpdateMultiplier;
	bool					mPaused;
	int						mFastForwardToUpdateNum;
	bool					mFastForwardToMarker;
	bool					mFastForwardStep;
	DWORD					mLastDrawTick;
	DWORD					mNextDrawTick;
	int						mStepMode;  // 0 = off, 1 = step, 2 = waiting for step

	int						mCursorNum;
	SoundManager*			mSoundManager;
	WidgetSafeDeleteList	mSafeDeleteList;
	bool					mMouseIn;
	bool					mRunning;
	bool					mActive;
	bool					mMinimized;
	bool					mPhysMinimized;
	bool					mWindowOccluded;
	bool					mIsDisabled;
	bool					mHasFocus;
	int						mDrawTime;
	ulong					mFPSStartTick;
	int						mFPSFlipCount;
	int						mFPSDirtyCount;
	int						mFPSTime;
	int						mFPSCount;
	bool					mShowFPS;
	int						mShowFPSMode;
	int						mScreenBltTime;
	bool					mAutoStartLoadingThread;
	bool					mLoadingThreadStarted;
	std::atomic<bool>		mLoadingThreadCompleted;
	bool					mLoaded;
	bool					mYieldMainThread;
	bool					mLoadingFailed;
	bool					mCursorThreadRunning;
	bool					mSysCursor;
	bool					mCustomCursorsEnabled;
	bool					mCustomCursorDirty;
	bool					mLastShutdownWasGraceful;
	bool					mIsWideWindow;
	bool					mWriteToSexyCache;
	bool					mSexyCacheBuffers;

	int						mNumLoadingThreadTasks;
	int						mCompletedLoadingThreadTasks;

	// Demo recording/playback of program control is not supported by the
	// portable framework; these remain so existing code keeps compiling.
	bool					mRecordingDemoBuffer;
	bool					mPlayingDemoBuffer;
	bool					mManualShutdown;
	std::string				mDemoPrefix;
	std::string				mDemoFileName;

	bool					mDebugKeysEnabled;
	bool					mEnableMaximizeButton;
	bool					mCtrlDown;
	bool					mAltDown;
	bool					mAllowAltEnter;

	int						mSyncRefreshRate;
	bool					mVSyncUpdates;
	bool					mVSyncBroken;
	int						mVSyncBrokenCount;
	DWORD					mVSyncBrokenTestStartTick;
	DWORD					mVSyncBrokenTestUpdates;
	bool					mWaitForVSync;
	bool					mSoftVSyncWait;
	bool					mUserChanged3DSetting;
	bool					mAutoEnable3D;
	bool					mTest3D;
	DWORD					mMinVidMemory3D;
	DWORD					mRecommendedVidMemory3D;

	bool					mWidescreenAware;
	Rect					mScreenBounds;
	bool					mEnableWindowAspect;
	Ratio					mWindowAspect;

	StringWStringMap		mStringProperties;
	StringBoolMap			mBoolProperties;
	StringIntMap			mIntProperties;
	StringDoubleMap			mDoubleProperties;
	StringStringVectorMap	mStringVectorProperties;
	ResourceManager*		mResourceManager;

	StringVector			mCmdLineArgs;
	float					mWheelAccumulator;

	// Diagnostics: periodically write the screen to PNG files.
	std::string				mCaptureDir;
	int						mCaptureIntervalMs;
	DWORD					mLastCaptureTick;
	int						mCaptureCount;

	// Diagnostics: scripted input for automated runs (see RunInputScript).
	StringVector			mInputScript;
	size_t					mInputScriptPos;
	DWORD					mInputScriptWaitUntil;
	bool					mForceSoftwareRendering;
	int						mCmdLineWindowMode;	// 0 = no preference, 1 = windowed, 2 = fullscreen

protected:
	void					RehupFocus();
	void					ClearKeysDown();
	bool					ProcessEvents(bool singleMessage);
	void					ProcessEvent(const SDL_Event& theEvent);
	void					UpdateFTimeAcc();
	virtual bool			Process(bool allowSleep = true);
	virtual void			UpdateFrames();
	virtual bool			DoUpdateFrames();
	virtual void			DoUpdateFramesF(float theFrac);
	virtual void			MakeWindow();
	virtual void			EnforceCursor();
	virtual void			ReInitImages();
	virtual void			DeleteNativeImageData();
	virtual void			DeleteExtraImageData();

	// Loading thread methods
	virtual void			LoadingThreadCompleted();
	static void				LoadingThreadProcStub(void *theArg);

	void					WaitForLoadingThread();
	void					ProcessSafeDeleteList();
	void					RestoreScreenResolution();
	void					DoExit(int theCode);

	void					TakeScreenshot();
	void					WriteCapture(const std::string& theFileName);
	void					LoadInputScript(const std::string& theFileName);
	void					RunInputScript();
	void					DumpProgramInfo();
	void					ShowMemoryUsage();

	// Registry helpers
	bool					RegistryRead(const std::string& theValueName, ulong* theType, uchar* theValue, ulong* theLength);
	bool					RegistryWrite(const std::string& theValueName, ulong theType, const uchar* theValue, ulong theLength);
	void					SplitRegistryValueName(const std::string& theValueName, std::string& theKey, std::string& theName);

	// Sets the OS cursor for one of the CURSOR_* constants.
	void					SetSystemCursor(int theCursorNum);
	// Replaces the OS cursor used for a CURSOR_* constant with a 0xAARRGGBB bitmap.
	void					SetCursorBitmap(int theCursorNum, const std::uint32_t* thePixels, int theWidth, int theHeight, int theHotX, int theHotY);
	void					UpdateCustomCursor(Image* theImage);

public:
	SexyAppBase();
	virtual ~SexyAppBase();

	// Common overrides:
	virtual MusicInterface*	CreateMusicInterface();
	virtual void			InitHook();
	virtual void			ShutdownHook();
	virtual void			PreTerminate();
	virtual void			LoadingThreadProc();
	virtual void			WriteToRegistry();
	virtual void			ReadFromRegistry();
	virtual Dialog*			NewDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode);
	virtual void			PreDisplayHook();

	// Public methods
	virtual void			BeginPopup();
	virtual void			EndPopup();
	virtual int				MsgBox(const std::string &theText, const std::string &theTitle = "Message", int theFlags = MB_OK);
	virtual int				MsgBox(const std::wstring &theText, const std::wstring &theTitle = L"Message", int theFlags = MB_OK);
	virtual void			Popup(const std::string& theString);
	virtual void			Popup(const std::wstring& theString);
	virtual void			LogScreenSaverError(const std::string &theError);
	virtual void			SafeDeleteWidget(Widget* theWidget);

	virtual void			URLOpenFailed(const std::string& theURL);
	virtual void			URLOpenSucceeded(const std::string& theURL);
	virtual bool			OpenURL(const std::string& theURL, bool shutdownOnOpen = false);
	virtual std::string		GetProductVersion(const std::string& thePath);

	virtual void			SEHOccured();
	virtual std::string		GetGameSEHInfo();
	virtual void			GetSEHWebParams(DefinesMap* theDefinesMap);
	virtual void			Shutdown();

	void					SetCommandLine(int argc, char** argv);
	virtual void			DoParseCmdLine();
	virtual void			ParseCmdLine(const std::string& theCmdLine);
	virtual void			HandleCmdLineParam(const std::string& theParamName, const std::string& theParamValue);
	virtual void			HandleNotifyGameMessage(int theType);
	virtual void			HandleGameAlreadyRunning();

	virtual void			Start();
	virtual void			Init();
	virtual void			PreDDInterfaceInitHook();
	virtual void			PostDDInterfaceInitHook();
	virtual bool			ChangeDirHook(const char *theIntendedPath);
	virtual void			PlaySample(int theSoundNum);
	virtual void			PlaySample(int theSoundNum, int thePan);

	virtual double			GetMasterVolume();
	virtual double			GetMusicVolume();
	virtual double			GetSfxVolume();
	virtual bool			IsMuted();

	virtual void			SetMasterVolume(double theVolume);
	virtual void			SetMusicVolume(double theVolume);
	virtual void			SetSfxVolume(double theVolume);
	virtual void			Mute(bool autoMute = false);
	virtual void			Unmute(bool autoMute = false);

	void					StartLoadingThread();
	virtual double			GetLoadingThreadProgress();

	void					CopyToClipboard(const std::string& theString);
	std::string				GetClipboard();

	void					SetCursor(int theCursorNum);
	int						GetCursor();
	void					EnableCustomCursors(bool enabled);
	virtual DeviceImage*	GetImage(const std::string& theFileName, bool commitBits = true);
	virtual SharedImageRef	SetSharedImage(const std::string& theFileName, const std::string& theVariant, DeviceImage* theImage, bool* isNew);
	virtual SharedImageRef	GetSharedImage(const std::string& theFileName, const std::string& theVariant = "", bool* isNew = NULL);

	void					CleanSharedImages();
	void					PrecacheAdditive(MemoryImage* theImage);
	void					PrecacheAlpha(MemoryImage* theImage);
	void					PrecacheNative(MemoryImage* theImage);
	void					SetCursorImage(int theCursorNum, Image* theImage);

	DeviceImage*			CreateCrossfadeImage(Image* theImage1, const Rect& theRect1, Image* theImage2, const Rect& theRect2, double theFadeFactor);
	void					ColorizeImage(Image* theImage, const Color& theColor);
	DeviceImage*			CreateColorizedImage(Image* theImage, const Color& theColor);
	DeviceImage*			CopyImage(Image* theImage, const Rect& theRect);
	DeviceImage*			CopyImage(Image* theImage);
	void					MirrorImage(Image* theImage);
	void					FlipImage(Image* theImage);
	void					RotateImageHue(Sexy::MemoryImage *theImage, int theDelta);
	ulong					HSLToRGB(int h, int s, int l);
	ulong					RGBToHSL(int r, int g, int b);
	void					HSLToRGB(const ulong* theSource, ulong* theDest, int theSize);
	void					RGBToHSL(const ulong* theSource, ulong* theDest, int theSize);

	void					AddMemoryImage(MemoryImage* theMemoryImage);
	void					RemoveMemoryImage(MemoryImage* theMemoryImage);
	void					Remove3DData(MemoryImage* theMemoryImage);
	virtual void			SwitchScreenMode();
	virtual void			SwitchScreenMode(bool wantWindowed);
	virtual void			SwitchScreenMode(bool wantWindowed, bool is3d, bool force = false);
	virtual void			SetAlphaDisabled(bool isDisabled);

	virtual Dialog*			DoDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode);
	virtual Dialog*			GetDialog(int theDialogId);
	virtual void			AddDialog(int theDialogId, Dialog* theDialog);
	virtual void			AddDialog(Dialog* theDialog);
	virtual bool			KillDialog(int theDialogId, bool removeWidget, bool deleteWidget);
	virtual bool			KillDialog(int theDialogId);
	virtual bool			KillDialog(Dialog* theDialog);
	virtual int				GetDialogCount();
	virtual void			ModalOpen();
	virtual void			ModalClose();
	virtual void			DialogButtonPress(int theDialogId, int theButtonId);
	virtual void			DialogButtonDepress(int theDialogId, int theButtonId);

	virtual void			GotFocus();
	virtual void			LostFocus();
	virtual bool			IsAltKeyUsed(int theChar);
	virtual bool			DebugKeyDown(int theKey);
	virtual void			CloseRequestAsync();
	bool					Is3DAccelerated();
	bool					Is3DAccelerationSupported();
	bool					Is3DAccelerationRecommended();
	void					Set3DAcclerated(bool is3D, bool reinit = true);
	virtual void			Done3dTesting();
	virtual std::string		NotifyCrashHook(); // return file name that you want to upload

	virtual bool			DrawDirtyStuff();
	virtual void			Redraw(Rect* theClipRect);

	// Properties access methods
	bool					LoadProperties(const std::string& theFileName, bool required, bool checkSig);
	bool					LoadProperties();
	virtual void			InitPropertiesHook();

	// Resource access methods
	void					LoadResourceManifest();
	void					ShowResourceError(bool doExit = false);

	bool					GetBoolean(const std::string& theId);
	bool					GetBoolean(const std::string& theId, bool theDefault);
	int						GetInteger(const std::string& theId);
	int						GetInteger(const std::string& theId, int theDefault);
	double					GetDouble(const std::string& theId);
	double					GetDouble(const std::string& theId, double theDefault);
	SexyString				GetString(const std::string& theId);
	SexyString				GetString(const std::string& theId, const SexyString& theDefault);

	StringVector			GetStringVector(const std::string& theId);

	void					SetBoolean(const std::string& theId, bool theValue);
	void					SetInteger(const std::string& theId, int theValue);
	void					SetDouble(const std::string& theId, double theValue);
	void					SetString(const std::string& theId, const std::wstring& theValue);

	// Demo compatibility (no-ops)
	void					DemoSyncBuffer(Buffer* theBuffer) { (void)theBuffer; }
	void					DemoSyncString(std::string* theString) { (void)theString; }
	void					DemoSyncInt(int* theInt) { (void)theInt; }
	void					DemoSyncBool(bool* theBool) { (void)theBool; }
	void					DemoAssertStringEqual(const std::string& theString) { (void)theString; }
	void					DemoAssertIntEqual(int theInt) { (void)theInt; }
	void					DemoAddMarker(const std::string& theString) { (void)theString; }

	// Registry access methods
	bool					RegistryGetSubKeys(const std::string& theKeyName, StringVector* theSubKeys);
	bool					RegistryReadString(const std::string& theValueName, std::string* theString);
	bool					RegistryReadInteger(const std::string& theValueName, int* theValue);
	bool					RegistryReadBoolean(const std::string& theValueName, bool* theValue);
	bool					RegistryReadData(const std::string& theValueName, uchar* theValue, ulong* theLength);
	bool					RegistryWriteString(const std::string& theValueName, const std::string& theString);
	bool					RegistryWriteInteger(const std::string& theValueName, int theValue);
	bool					RegistryWriteBoolean(const std::string& theValueName, bool theValue);
	bool					RegistryWriteData(const std::string& theValueName, const uchar* theValue, ulong theLength);
	bool					RegistryEraseKey(const SexyString& theKeyName);
	void					RegistryEraseValue(const SexyString& theValueName);

	// File access methods
	bool					WriteBufferToFile(const std::string& theFileName, const Buffer* theBuffer);
	bool					ReadBufferFromFile(const std::string& theFileName, Buffer* theBuffer, bool dontWriteToDemo = false);//UNICODE
	bool					WriteBytesToFile(const std::string& theFileName, const void *theData, unsigned long theDataLen);
	bool					FileExists(const std::string& theFileName);
	bool					EraseFile(const std::string& theFileName);

	// Misc methods
	virtual void			DoMainLoop();
	virtual bool			UpdateAppStep(bool* updated);
	virtual bool			UpdateApp();
	int						InitDDInterface();
	void					ClearUpdateBacklog(bool relaxForASecond = false);
	bool					IsScreenSaver();
	virtual bool			AppCanRestore();
};

extern SexyAppBase* gSexyAppBase;

};

#endif //__SEXYAPPBASE_H__
