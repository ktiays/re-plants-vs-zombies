#ifndef __AUTOCRIT_INCLUDED__
#define __AUTOCRIT_INCLUDED__

#include "Common.h"
#include "CritSect.h"

namespace Sexy
{

class AutoCrit
{
	const CritSect&			mCritSect;

public:
	AutoCrit(const CritSect& theCritSect) : 
		mCritSect(theCritSect)
	{ 
		mCritSect.Lock();
	}

	~AutoCrit()
	{ 
		mCritSect.Unlock();
	}

	AutoCrit(const AutoCrit&) = delete;
	AutoCrit& operator=(const AutoCrit&) = delete;
};

}

#endif //__AUTOCRIT_INCLUDED__
