#include "platform/Platform.h"
#include "platform/PlatformTypes.h"

#include <SDL3/SDL.h>

#include <cstdio>

using namespace Sexy;

DWORD Sexy::GetTickCount()
{
	return (DWORD)SDL_GetTicks();
}

DWORD Sexy::timeGetTime()
{
	return (DWORD)SDL_GetTicks();
}

void Sexy::Sleep(DWORD theMilliseconds)
{
	SDL_Delay(theMilliseconds);
}

std::uint64_t Sexy::GetCurrentThreadId()
{
	return (std::uint64_t)SDL_GetCurrentThreadID();
}

void Sexy::OutputDebugString(const char* theString)
{
	if (theString != nullptr)
		std::fputs(theString, stderr);
}

std::string Platform::GetResourceDirectory()
{
	const char* aBasePath = SDL_GetBasePath();
	return aBasePath != nullptr ? std::string(aBasePath) : std::string("./");
}

std::string Platform::GetUserDataDirectory(const std::string& theCompany, const std::string& theProduct)
{
	char* aPrefPath = SDL_GetPrefPath(theCompany.c_str(), theProduct.c_str());
	if (aPrefPath == nullptr)
		return std::string();

	std::string aPath = aPrefPath;
	SDL_free(aPrefPath);
	return aPath;
}

bool Platform::OpenURL(const std::string& theURL)
{
	return SDL_OpenURL(theURL.c_str());
}

int Platform::ShowMessageBox(const std::string& theTitle, const std::string& theText, int theFlags)
{
	std::fprintf(stderr, "[%s] %s\n", theTitle.c_str(), theText.c_str());

	// Automated runs must not block on a modal dialog.
	if (SDL_getenv("SEXY_NONINTERACTIVE") != nullptr)
	{
		switch (theFlags & 0x0F)
		{
		case MB_YESNO:
		case MB_YESNOCANCEL:	return IDYES;
		default:				return IDOK;
		}
	}

	SDL_MessageBoxFlags aFlags = SDL_MESSAGEBOX_INFORMATION;
	switch (theFlags & 0xF0)
	{
	case MB_ICONERROR:			aFlags = SDL_MESSAGEBOX_ERROR; break;
	case MB_ICONEXCLAMATION:	aFlags = SDL_MESSAGEBOX_WARNING; break;
	default:					break;
	}

	SDL_MessageBoxButtonData aButtons[3];
	int aNumButtons = 0;
	switch (theFlags & 0x0F)
	{
	case MB_YESNO:
		aButtons[aNumButtons++] = { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, IDYES, "Yes" };
		aButtons[aNumButtons++] = { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, IDNO, "No" };
		break;
	case MB_YESNOCANCEL:
		aButtons[aNumButtons++] = { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, IDYES, "Yes" };
		aButtons[aNumButtons++] = { 0, IDNO, "No" };
		aButtons[aNumButtons++] = { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, IDCANCEL, "Cancel" };
		break;
	case MB_OKCANCEL:
		aButtons[aNumButtons++] = { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, IDOK, "OK" };
		aButtons[aNumButtons++] = { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, IDCANCEL, "Cancel" };
		break;
	default:
		aButtons[aNumButtons++] = { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, IDOK, "OK" };
		break;
	}

	SDL_MessageBoxData aData = {};
	aData.flags = aFlags;
	aData.window = nullptr;
	aData.title = theTitle.c_str();
	aData.message = theText.c_str();
	aData.numbuttons = aNumButtons;
	aData.buttons = aButtons;

	int aButtonId = IDCANCEL;
	if (!SDL_ShowMessageBox(&aData, &aButtonId))
	{
		return (theFlags & 0x0F) == MB_OK ? IDOK : IDCANCEL;
	}

	if (aButtonId < 0)
		aButtonId = (theFlags & 0x0F) == MB_OK ? IDOK : IDCANCEL;
	return aButtonId;
}

void Platform::SetClipboardText(const std::string& theText)
{
	SDL_SetClipboardText(theText.c_str());
}

std::string Platform::GetClipboardText()
{
	char* aText = SDL_GetClipboardText();
	if (aText == nullptr)
		return std::string();

	std::string aResult = aText;
	SDL_free(aText);
	return aResult;
}
