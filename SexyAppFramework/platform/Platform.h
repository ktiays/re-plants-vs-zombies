#ifndef __SEXY_PLATFORM_H__
#define __SEXY_PLATFORM_H__

#include <string>
#include <cstdint>

// Operating-system services needed by the framework that are not part of the
// window/input/render/audio backends. Every function here has one
// implementation per platform backend (see platform/sdl for the SDL version).

namespace Sexy
{
namespace Platform
{

// Directory that holds main.pak and properties/. Relative resource paths used
// by the game are resolved against the process working directory, which the
// application changes to this directory during startup.
std::string			GetResourceDirectory();

// Per-user writable directory for profiles, saves, and settings. Always ends
// with a path separator.
std::string			GetUserDataDirectory(const std::string& theCompany, const std::string& theProduct);

bool				OpenURL(const std::string& theURL);

// Returns IDOK/IDCANCEL/IDYES/IDNO according to theFlags (MB_* constants).
int					ShowMessageBox(const std::string& theTitle, const std::string& theText, int theFlags);

void				SetClipboardText(const std::string& theText);
std::string			GetClipboardText();

}
}

#endif //__SEXY_PLATFORM_H__
