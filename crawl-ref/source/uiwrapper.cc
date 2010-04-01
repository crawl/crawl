#ifdef USE_TILE
#include "uiwrapper.h"

#ifdef USE_SDL
#include "uiwrapper-sdl.h"
#endif

WindowManager *wm = NULL;

void WindowManager::create()
{
    if (wm)
        return;

#ifdef USE_SDL
    wm = (WindowManager *) new SDLWrapper();
#endif
}

#endif // USE_TILE
