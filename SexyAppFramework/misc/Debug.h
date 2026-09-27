#ifndef __DEBUG_INCLUDED__
#define __DEBUG_INCLUDED__

#include "Common.h"
#include <cassert>

extern bool gInAssert;

#ifdef SEXY_TRACING_ENABLED
void SexyTrace(const char *theStr);
#define SEXY_TRACE(theStr) SexyTrace(theStr)
#else
#define SEXY_TRACE(theStr)
#endif

extern void SexyTraceFmt(const SexyChar* fmt ...);
extern void OutputDebug(const SexyChar* fmt ...);

#ifdef NDEBUG

#define DBG_ASSERTE(exp)	((void)0)
#define DBG_ASSERT(exp)		((void)0)

#else

// Reports a failed framework assertion. The shipping game compiled these out,
// and the reconstructed sources trip some of them in ways the original
// tolerated, so they are logged (once per site) instead of aborting unless
// PVZ_STRICT_ASSERTS is set.
extern void SexyAssertFailed(const char* theExpression, const char* theFile, int theLine);

#define DBG_ASSERTE(exp)	{ if (!(exp)) { gInAssert = true; SexyAssertFailed(#exp, __FILE__, __LINE__); gInAssert = false; } }
#define DBG_ASSERT(exp)		DBG_ASSERTE(exp)

#endif // NDEBUG

#endif //__DEBUG_INCLUDED__