#ifndef __SEXY_METALRENDERDEVICE_H__
#define __SEXY_METALRENDERDEVICE_H__

#include "graphics/RenderDevice.h"

struct SDL_Window;

namespace Sexy
{

// Creates the Metal backend for an SDL window created with SDL_WINDOW_METAL.
// Returns NULL if Metal is unavailable.
RenderDevice* CreateMetalRenderDevice(SDL_Window* theWindow);

}

#endif //__SEXY_METALRENDERDEVICE_H__
