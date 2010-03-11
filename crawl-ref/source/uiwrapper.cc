#ifdef USE_TILE
#include "uiwrapper.h"

#ifdef USE_SDL
#include "uiwrapper-sdl.h"
#endif

UIWrapper *wrapper = NULL;

void create_ui_wrapper()
{
    if (wrapper)
        return;

#ifdef USE_SDL
    wrapper = (UIWrapper *) new SDLWrapper();
#endif

}

void destroy_ui_wrapper()
{
    if (!wrapper)
        return;

    delete wrapper;
}

#endif // USE_TILE
