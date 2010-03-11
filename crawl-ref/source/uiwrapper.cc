#ifdef USE_TILE
#include "uiwrapper.h"

#ifdef USE_SDL
#include "uiwrapper-sdl.h"
UIWrapper *wrapper = (UIWrapper *) new SDLWrapper();
#endif

#endif // USE_TILE
