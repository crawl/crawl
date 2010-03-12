#include <cstddef>

#ifdef USE_TILE
#include "cgcontext.h"

#ifdef USE_SDL
#include "cgcontext-sdl.h"
#endif

GraphicsContext *GraphicsContext::create()
{
#ifdef USE_SDL
    return (GraphicsContext *) new SDLGraphicsContext();
#endif

    // If we've gotten here, no imaging library has been defined
    return (NULL);
}

#endif // USE_TILE
