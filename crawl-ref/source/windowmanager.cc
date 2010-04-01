#ifdef USE_TILE
#include "windowmanager.h"

#ifdef USE_SDL
#include "windowmanager-sdl.h"
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
