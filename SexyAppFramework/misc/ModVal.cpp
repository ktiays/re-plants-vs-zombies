#include "ModVal.h"

// Run-time constant editing scanned the Windows executable image for M()
// markers. The portable build compiles M() to plain constants (see
// SEXY_DISABLE_MODVAL), so these entry points only pass values through.

using namespace Sexy;

int Sexy::ModVal(const char* theFileName, int theInt)
{
	(void)theFileName;
	return theInt;
}

double Sexy::ModVal(const char* theFileName, double theDouble)
{
	(void)theFileName;
	return theDouble;
}

float Sexy::ModVal(const char* theFileName, float theFloat)
{
	(void)theFileName;
	return theFloat;
}

const char* Sexy::ModVal(const char* theFileName, const char* theStr)
{
	(void)theFileName;
	return theStr;
}

bool Sexy::ReparseModValues()
{
	return false;
}

void Sexy::AddModValEnum(const std::string& theEnumName, int theVal)
{
	(void)theEnumName;
	(void)theVal;
}
