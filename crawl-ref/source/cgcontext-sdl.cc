#ifdef USE_TILE

#ifdef USE_SDL
#include "cgcontext-sdl.h"
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

/*
 * This file should only call functions that operate specifically
 * on Graphics Contexts.  If it needs to do something else, like
 * get an SDL-specific environment variable, call a UIWrapper
 * member function for increased modularity.
*/

SDLGraphicsContext::SDLGraphicsContext():
    palette(NULL),
    surf(NULL)
{
}

int SDLGraphicsContext::load_image( const char *file )
{
    if (surf) SDL_FreeSurface(surf);

    FILE *imgfile = fopen(file, "rb");
    if (imgfile)
    {
        SDL_RWops *rw = SDL_RWFromFP(imgfile, 0);
        if (rw)
        {
            surf = IMG_Load_RW(rw, 0);
            SDL_RWclose(rw);
        }
        fclose(imgfile);
    }

    if (!surf)
        return (-1);

    create_palette();

    return (0);
}

int SDLGraphicsContext::lock()
{
    return (SDL_LockSurface(surf));
}

void SDLGraphicsContext::unlock()
{
    SDL_UnlockSurface(surf);
}

int SDLGraphicsContext::height()
{
    if ( !surf )
        return -1;
    return (surf->h);
}

int SDLGraphicsContext::width()
{
    if ( !surf )
        return -1;
    return (surf->w);
}

short int SDLGraphicsContext::pitch()
{
    if ( !surf )
        return (-1);
    return (surf->pitch);
}

unsigned char SDLGraphicsContext::bytes_per_pixel()
{
    if ( !surf )
        return (-1);
    return (surf->format->BytesPerPixel);
}

void *SDLGraphicsContext::pixels()
{
    return (surf->pixels);
}

void *SDLGraphicsContext::native_surface()
{
    return ((void *)surf);
}

unsigned int SDLGraphicsContext::color_key()
{
    if ( !surf )
        return (-1);
    return (surf->format->colorkey);
}

void SDLGraphicsContext::get_rgba(unsigned int pixel, unsigned char *r,
        unsigned char *g, unsigned char *b, unsigned char *a)
{
    if ( !surf )
        return;
    SDL_GetRGBA(pixel, surf->format, r, g, b, a);
}

void SDLGraphicsContext::get_rgb(unsigned int pixel,     unsigned char *r,
                                    unsigned char *g,
                                    unsigned char *b)
{
    if ( !surf )
        return;
    SDL_GetRGB(pixel, surf->format, r, g, b);
}

void SDLGraphicsContext::create_palette()
{
    // TODO: Figure out if this is really safe
    // ui_palette and ui_color are defined in the same way as
    // their SDL counterparts, so we just cast pointers here
    if ( !surf )
        return;

    // Per SDL spec, if bitsPerPixel>8 palette is NULL
    if ( surf->format->BitsPerPixel > 8 )
        return;

    // Otherwise do some magic to get access
    // TODO: Figure some way to do this elegantly that still allows
    // quick access to color pixels and doesn't use pointer casting
    palette = (ui_palette*)surf->format->palette;
    palette->colors = (ui_color*)surf->format->palette->colors;
}

void SDLGraphicsContext::destroy_palette()
{
    if (palette)
        palette->colors = NULL;
    palette = NULL;
}

SDLGraphicsContext::~SDLGraphicsContext()
{
    destroy_palette();
    if ( surf )
        SDL_FreeSurface(surf);
}

#endif // USE_SDL
#endif // USE_TILE
