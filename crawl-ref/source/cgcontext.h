#ifndef CG_CONTEXT_H
#define CG_CONTEXT_H

#ifdef USE_TILE

#ifdef USE_SDL
class SDL_Surface;
#endif

typedef struct{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char unused;
} ui_color;

typedef struct{
    int ncolors;
    ui_color *colors;
} ui_palette;

class GraphicsContext {
public:
    // Class
    GraphicsContext();
    ~GraphicsContext();

    // Loading
    int load_image( const char *file );

    // Locking
    int lock();
    void unlock();

    // Dimentions
    int height();
    int width();
    short int pitch();
    unsigned char bytes_per_pixel();

    // Context Accessors
    void *pixels();
    void *native_surface();
    unsigned int color_key();
    void get_rgba(unsigned int pixel,   unsigned char *r,
                                        unsigned char *g,
                                        unsigned char *b,
                                        unsigned char *a);
    void get_rgb(unsigned int pixel,    unsigned char *r,
                                        unsigned char *g,
                                        unsigned char *b);

    ui_palette *palette;

protected:
    // Palette functions
    void create_palette();
    void destroy_palette();

#ifdef USE_SDL
    SDL_Surface *surf;
#endif

};

#endif //USE_TILE
#endif //include guard
