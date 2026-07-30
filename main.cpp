#include "LawnApp.h"
#include "Resources.h"
#include "Sexy.TodLib/TodStringFile.h"

#ifdef PVZ_HAS_REFERENCE_INPUT_CAPTURE
#include "pvz/platform/windows/LegacyInputCapture.h"

#include <iostream>
#include <string>
#endif

using namespace Sexy;

bool (*gAppCloseRequest)();				//[0x69E6A0]
bool (*gAppHasUsedCheatKeys)();			//[0x69E6A4]
SexyString (*gGetCurrentLevelName)();

//0x44E8F0
int WINAPI WinMain(_In_ HINSTANCE /* hInstance */, _In_opt_ HINSTANCE /* hPrevInstance */, _In_ LPSTR /* lpCmdLine */, _In_ int /* nCmdShow */)
{
	TodStringListSetColors(gLawnStringFormats, gLawnStringFormatCount);
	gGetCurrentLevelName = LawnGetCurrentLevelName;
	gAppCloseRequest = LawnGetCloseRequest;
	gAppHasUsedCheatKeys = LawnHasUsedCheatKeys;
	gExtractResourcesByName = Sexy::ExtractResourcesByName;
	gLawnApp = new LawnApp();
	gLawnApp->mChangeDirTo = (!Sexy::FileExists("properties\\resources.xml") && Sexy::FileExists("..\\properties\\resources.xml")) ? ".." : ".";
	gLawnApp->Init();
	gLawnApp->Start();
	gLawnApp->Shutdown();

	int anExitCode = 0;
#ifdef PVZ_HAS_REFERENCE_INPUT_CAPTURE
	if (!pvz::platform::windows::FinalizeLegacyInputCapture())
	{
		const std::string anError =
			"Could not save reference capture: " +
			std::string(
				pvz::platform::windows::GetLegacyInputCaptureError());
		std::cerr << anError << '\n';
		OutputDebugStringA((anError + "\n").c_str());
		anExitCode = 1;
	}
	else if (pvz::platform::windows::WasLegacyInputCaptureRequested())
	{
		const std::string aSummary =
			(pvz::platform::windows::WasLegacyBehaviorCaptureRequested()
				? "Reference behavior capture recorded: "
				: "Reference input replay captured: ") +
			std::to_string(
				pvz::platform::windows::
					GetLegacyInputCaptureFrameCount()) +
			" frames";
		std::cout << aSummary << '\n';
		OutputDebugStringA((aSummary + "\n").c_str());
	}
#endif

	if (gLawnApp)
		delete gLawnApp;

	return anExitCode;
};
