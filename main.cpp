#include "LawnApp.h"
#include "Resources.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "platform/Platform.h"

#include <cstdlib>
#include <filesystem>

using namespace Sexy;

bool (*gAppCloseRequest)();				//[0x69E6A0]
bool (*gAppHasUsedCheatKeys)();			//[0x69E6A4]
SexyString (*gGetCurrentLevelName)();

// Finds the directory that contains the user-supplied game data (main.pak and
// properties/). Checked in order: $PVZ_GAME_DIR, the working directory, the
// executable's directory (Contents/Resources inside a macOS bundle), and the
// directories above it, which covers a bundle placed next to the data.
static std::string FindGameDirectory()
{
	namespace fs = std::filesystem;

	auto HasGameData = [](const fs::path& theDir)
	{
		std::error_code anError;
		return fs::exists(theDir / "main.pak", anError) || fs::exists(theDir / "properties" / "resources.xml", anError);
	};

	if (const char* anEnvDir = std::getenv("PVZ_GAME_DIR"))
	{
		if (HasGameData(anEnvDir))
			return anEnvDir;
	}

	std::error_code anError;
	fs::path aCurDir = fs::current_path(anError);
	if (!anError && HasGameData(aCurDir))
		return aCurDir.string();

	fs::path aDir = Platform::GetResourceDirectory();
	for (int i = 0; i < 5 && !aDir.empty(); i++)
	{
		if (HasGameData(aDir))
			return aDir.string();

		fs::path aParent = aDir.parent_path();
		if (aParent == aDir)
			break;
		aDir = aParent;
	}

	return ".";
}

//0x44E8F0
int main(int argc, char* argv[])
{
	TodStringListSetColors(gLawnStringFormats, gLawnStringFormatCount);
	gGetCurrentLevelName = LawnGetCurrentLevelName;
	gAppCloseRequest = LawnGetCloseRequest;
	gAppHasUsedCheatKeys = LawnHasUsedCheatKeys;
	gExtractResourcesByName = Sexy::ExtractResourcesByName;
	gLawnApp = new LawnApp();
	gLawnApp->SetCommandLine(argc, argv);
	gLawnApp->mChangeDirTo = FindGameDirectory();
	gLawnApp->Init();
	gLawnApp->Start();
	gLawnApp->Shutdown();

	if (gLawnApp)
		delete gLawnApp;

	return 0;
}
