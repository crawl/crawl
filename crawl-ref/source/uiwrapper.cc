#ifdef USE_TILE
#include "uiwrapper.h"

#ifdef USE_SDL
#include "uiwrapper-sdl.h"
#endif

UIWrapper *wrapper = NULL;

void UIWrapper::create()
{
    if (wrapper)
        return;

#ifdef USE_SDL
    wrapper = (UIWrapper *) new SDLWrapper();
#endif
}

#endif // USE_TILE
