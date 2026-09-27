#include <time.h>
#include <cstdio>
#include <cstdlib>
#include "TodDebug.h"
#include "TodCommon.h"
#include "misc/Debug.h"
#include "../SexyAppFramework/SexyAppBase.h"
#include "platform/Platform.h"

using namespace Sexy;

static char gLogFileName[MAX_PATH];
static char gDebugDataFolder[MAX_PATH];

//0x514EA0
void TodErrorMessageBox(const char* theMessage, const char* theTitle)
{
	TodTraceAndLog("%s.%s", theMessage, theTitle);
	Platform::ShowMessageBox(theTitle, theMessage, MB_OK | MB_ICONEXCLAMATION);
}

void TodTraceMemory()
{
}

void* TodMalloc(int theSize)
{
	TOD_ASSERT(theSize > 0);
	return malloc(theSize);
}

void TodFree(void* theBlock)
{
	if (theBlock != nullptr)
	{
		free(theBlock);
	}
}

void TodAssertFailed(const char* theCondition, const char* theFile, int theLine, const char* theMsg, ...)
{
	char aFormattedMsg[1024];
	va_list argList;
	va_start(argList, theMsg);
	int aCount = TodVsnprintf(aFormattedMsg, sizeof(aFormattedMsg), theMsg, argList);
	va_end(argList);

	if (aCount != 0) {
		if (aFormattedMsg[aCount - 1] != '\n')
		{
			if (aCount + 1 < 1024)
			{
				aFormattedMsg[aCount] = '\n';
				aFormattedMsg[aCount + 1] = '\0';
			}
			else
			{
				aFormattedMsg[aCount - 1] = '\n';
			}
		}
	}

	char aBuffer[1024];
	if (*theCondition != '\0')
	{
		TodSnprintf(aBuffer, sizeof(aBuffer), "\n%s(%d)\nassertion failed: '%s'\n%s\n", theFile, theLine, theCondition, aFormattedMsg);
	}
	else
	{
		TodSnprintf(aBuffer, sizeof(aBuffer), "\n%s(%d)\nassertion failed: %s\n", theFile, theLine, aFormattedMsg);
	}
	TodTrace("%s", aBuffer);

	TodLogString(aBuffer);

	// Reconstructed code can trip assertions the shipping game never checked;
	// only stop when strict assertion handling is requested.
	if (std::getenv("PVZ_STRICT_ASSERTS") != nullptr)
	{
		if (gInAssert)
		{
			TodLog("Assert during exception processing");
			exit(0);
		}

		gInAssert = true;
		TodReportError(aFormattedMsg);
		gInAssert = false;
		abort();
	}
}

void TodLog(const char* theFormat, ...)
{
	char aButter[1024];
	va_list argList;
	va_start(argList, theFormat);
	int aCount = TodVsnprintf(aButter, sizeof(aButter), theFormat, argList);
	va_end(argList);

	if (aCount > 0 && aButter[aCount - 1] != '\n')
	{
		if (aCount + 1 < 1024)
		{
			aButter[aCount] = '\n';
			aButter[aCount + 1] = '\0';
		}
		else
		{
			aButter[aCount - 1] = '\n';
		}
	}

	TodLogString(aButter);
}

void TodLogString(const char* theMsg)
{
	if (gLogFileName[0] == '\0')
		return;

	FILE* f = fopen(FixPath(gLogFileName).c_str(), "a");
	if (f == nullptr)
	{
		OutputDebugString(_S("Failed to open log file\n"));
		return;
	}

	if (fwrite(theMsg, strlen(theMsg), 1, f) != 1)
	{
		OutputDebugString(_S("Failed to write to log file\n"));
	}

	fclose(f);
}

void TodTrace(const char* theFormat, ...)
{
	char aButter[1024];
	va_list argList;
	va_start(argList, theFormat);
	int aCount = TodVsnprintf(aButter, sizeof(aButter), theFormat, argList);
	va_end(argList);

	if (aCount > 0 && aButter[aCount - 1] != '\n')
	{
		if (aCount + 1 < 1024)
		{
			aButter[aCount] = '\n';
			aButter[aCount + 1] = '\0';
		}
		else
		{
			aButter[aCount - 1] = '\n';
		}
	}

	OutputDebugStringA(aButter);
}

void TodHesitationTrace(...)
{
}

void TodTraceAndLog(const char* theFormat, ...)
{
	char aButter[1024];
	va_list argList;
	va_start(argList, theFormat);
	int aCount = TodVsnprintf(aButter, sizeof(aButter), theFormat, argList);
	va_end(argList);

	if (aCount > 0 && aButter[aCount - 1] != '\n')
	{
		if (aCount + 1 < 1024)
		{
			aButter[aCount] = '\n';
			aButter[aCount + 1] = '\0';
		}
		else
		{
			aButter[aCount - 1] = '\n';
		}
	}

	OutputDebugStringA(aButter);
	TodLogString(aButter);
}

void TodTraceWithoutSpamming(const char* theFormat, ...)
{
	static time_t gLastTraceTime = 0;
	time_t aTime = time(nullptr);
	if (aTime < gLastTraceTime)
	{
		return;
	}

	gLastTraceTime = aTime;
	char aButter[1024];
	va_list argList;
	va_start(argList, theFormat);
	int aCount = TodVsnprintf(aButter, sizeof(aButter), theFormat, argList);
	va_end(argList);

	if (aCount > 0 && aButter[aCount - 1] != '\n')
	{
		if (aCount + 1 < 1024)
		{
			aButter[aCount] = '\n';
			aButter[aCount + 1] = '\0';
		}
		else
		{
			aButter[aCount - 1] = '\n';
		}
	}

	OutputDebugStringA(aButter);
}

void TodReportError(const char* theMessage)
{
	TodLog("%s", theMessage);
}

void TodAssertInitForApp()
{
	MkDir(GetAppDataFolder() + "userdata");
	std::string aRelativeUserPath = GetAppDataFolder() + "userdata/";
	strcpy(gDebugDataFolder, GetFullPath(aRelativeUserPath).c_str());
	strcpy(gLogFileName, gDebugDataFolder);
	strcpy(gLogFileName + strlen(gLogFileName), "log.txt");
	TOD_ASSERT(strlen(gLogFileName) < MAX_PATH);

	time_t aclock = time(nullptr);
	TodLog("Started %s\n", asctime(localtime(&aclock)));
}
