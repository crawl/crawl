#ifndef SDL_CG_CONTEXT_H
#define SDL_CG_CONTEXT_H

#ifdef USE_TILE
#include "cgcontext.h"

class SDL_Surface;

class SDLGraphicsContext : public GraphicsContext
{
public:
    // Class
    SDLGraphicsContext();
    virtual ~SDLGraphicsContext();

    // Loading
    virtual int load_image( const char *file );

    // Locking
    virtual int lock();
    virtual void unlock();

    // Dimentions
    virtual int height();
    virtual int width();
    virtual short int pitch();
    virtual unsigned char bytes_per_pixel();

    // Context Accessors
    virtual void *pixels();
    virtual void *native_surface();
    virtual unsigned int color_key();
    virtual void get_rgba(unsigned int pixel,   unsigned char *r,
                                                unsigned char *g,
                                                unsigned char *b,
                                                unsigned char *a);
    virtual void get_rgb(unsigned int pixel,    unsigned char *r,
                                                unsigned char *g,
                                                unsigned char *b);

    // TODO: Maybe this should have mandator accessors?
    ui_palette *palette;

protected:
    // Palette functions
    void create_palette();
    void destroy_palette();

    SDL_Surface *surf;
};

#endif //USE_TILE
#endif //include guard
