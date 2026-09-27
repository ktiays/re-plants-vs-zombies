#ifndef _H_CritSect
#define _H_CritSect

#include "Common.h"

#include <mutex>

namespace Sexy
{

// A recursive lock, as Win32 critical sections are.
class CritSect 
{
private:
	mutable std::recursive_mutex mMutex;
	friend class AutoCrit;

public:
	CritSect(void) {}
	~CritSect(void) {}

	void Lock() const { mMutex.lock(); }
	void Unlock() const { mMutex.unlock(); }
};

}

#endif // _H_CritSect
