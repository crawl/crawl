#include "cgcontext-sdl.h"

#include <SDL/SDL.h>
#include <SDL_image/SDL_image.h>

#ifdef USE_TILE

/*
 * This file should only call functions that operate specifically
 * on Graphics Contexts.  If it needs to do something else, like
 * get an SDL-specific environment variable, call a UIWrapper
 * member function for increased modularity.
*/

GraphicsContext::GraphicsContext():
    palette(NULL),
    surf(NULL)
{
}

int GraphicsContext::loadImage( const char *file )
{
    if(surf) SDL_FreeSurface(surf);
    
    surf = IMG_Load(file);
    
    if(!surf) return -1;
    
    createPalette();
    
    return 0;
}

int GraphicsContext::lock()
{
    return SDL_LockSurface(surf);
}

void GraphicsContext::unlock()
{
    SDL_UnlockSurface(surf);
}

int GraphicsContext::height()
{
    if( !surf ) return -1;
    return surf->h;
}

int GraphicsContext::width()
{
    if( !surf ) return -1;
    return surf->w;
}

short int GraphicsContext::pitch()
{
    if( !surf ) return -1;
    return surf->pitch;
}

unsigned char GraphicsContext::bytesPerPixel()
{
    if( !surf ) return -1;
    return surf->format->BytesPerPixel;
}

void *GraphicsContext::pixels()
{
    return surf->pixels;
}

unsigned int GraphicsContext::colorKey()
{
    if( !surf ) return -1;
    return surf->format->colorkey;
}

void GraphicsContext::getRGBA(unsigned int pixel, unsigned char *r,
        unsigned char *g, unsigned char *b, unsigned char *a)
{
    if( !surf ) return;
    SDL_GetRGBA(pixel, surf->format, r, g, b, a);
}

void GraphicsContext::getRGB(unsigned int pixel,     unsigned char *r,
                                    unsigned char *g,
                                    unsigned char *b)
{
    if( !surf ) return;
    SDL_GetRGB(pixel, surf->format, r, g, b);
}

void GraphicsContext::createPalette()
{
    // TODO: Figure out if this is really safe
    // ui_palette and ui_color are defined in the same way as
    // their SDL counterparts, so we just cast pointers here
    if( !surf ) return;
    
    // Per SDL spec, if bitsPerPixel>8 palette is NULL
    if( surf->format->BitsPerPixel > 8 ) return;
    
    // Otherwise do some magic to get access
    // TODO: Figure some way to do this elegantly that still allows
    // quick access to color pixels and doesn't use pointer casting
    palette = (ui_palette*)surf->format->palette;
    palette->colors = (ui_color*)surf->format->palette->colors;
}

void GraphicsContext::destroyPalette()
{
    if(palette) palette->colors = NULL;
    palette = NULL;
}

GraphicsContext::~GraphicsContext()
{
    destroyPalette();
    if( surf ) SDL_FreeSurface(surf);
}

#endif

