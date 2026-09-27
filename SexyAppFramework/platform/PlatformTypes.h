#ifndef __SEXY_PLATFORMTYPES_H__
#define __SEXY_PLATFORMTYPES_H__

// Fixed-width aliases for the Win32 vocabulary that the reconstructed game
// sources still use. On every platform these have the widths they had on the
// original 32-bit Windows target, so persisted data keeps its layout.

#include <cstdint>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef min
#undef max
#undef GetTickCount
#undef Sleep
#undef OutputDebugString
#undef MessageBox
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>

typedef std::uint32_t	DWORD;
typedef std::uint16_t	WORD;
typedef std::uint8_t	BYTE;
typedef std::int32_t	BOOL;
typedef unsigned int	UINT;
typedef std::int32_t	LONG;
typedef std::uint32_t	ULONG;
typedef std::int64_t	LONGLONG;
typedef char*			LPSTR;
typedef const char*		LPCSTR;
typedef void*			HANDLE;
typedef void*			HWND;
typedef std::intptr_t	LPARAM;
typedef std::uintptr_t	WPARAM;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define MAKELONG(a, b)	((LONG)(((WORD)(((DWORD)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD)(b)) & 0xffff))) << 16))
#define LOWORD(l)		((WORD)(((DWORD)(l)) & 0xffff))
#define HIWORD(l)		((WORD)((((DWORD)(l)) >> 16) & 0xffff))

#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif

// 64-bit time as used by the Microsoft C runtime. Persisted in profiles.
typedef std::int64_t	__time64_t;
inline __time64_t _time64(__time64_t* theTime)
{
	__time64_t aNow = (__time64_t)std::time(nullptr);
	if (theTime != nullptr)
		*theTime = aNow;
	return aNow;
}
inline int _localtime64_s(struct tm* theResult, const __time64_t* theTime)
{
	std::time_t aTime = (std::time_t)*theTime;
	return localtime_r(&aTime, theResult) != nullptr ? 0 : 1;
}

inline int stricmp(const char* a, const char* b) { return strcasecmp(a, b); }
inline int _stricmp(const char* a, const char* b) { return strcasecmp(a, b); }
inline int strnicmp(const char* a, const char* b, size_t n) { return strncasecmp(a, b, n); }
inline int _strnicmp(const char* a, const char* b, size_t n) { return strncasecmp(a, b, n); }

// Message box flags and results used by SexyAppBase::MsgBox.
enum
{
	MB_OK				= 0x00,
	MB_OKCANCEL			= 0x01,
	MB_YESNOCANCEL		= 0x03,
	MB_YESNO			= 0x04,
	MB_ICONERROR		= 0x10,
	MB_ICONHAND			= 0x10,
	MB_ICONQUESTION		= 0x20,
	MB_ICONEXCLAMATION	= 0x30,
	MB_ICONWARNING		= 0x30,
	MB_ICONINFORMATION	= 0x40
};

enum
{
	IDOK		= 1,
	IDCANCEL	= 2,
	IDYES		= 6,
	IDNO		= 7
};
#endif

namespace Sexy
{

// Platform bridge primitives. Implemented by the active platform backend.
DWORD				GetTickCount();		// Milliseconds since an arbitrary epoch.
DWORD				timeGetTime();		// Same clock as GetTickCount().
void				Sleep(DWORD theMilliseconds);
std::uint64_t		GetCurrentThreadId();
void				OutputDebugString(const char* theString);

}

#ifndef _WIN32
#define OutputDebugStringA(x) ::Sexy::OutputDebugString(x)
#endif

#endif //__SEXY_PLATFORMTYPES_H__
