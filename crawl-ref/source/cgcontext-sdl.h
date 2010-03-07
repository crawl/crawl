#ifndef CGCONTEXT_SDL_H
#define CGCONTEXT_SDL_H

#ifdef USE_TILE
#ifdef USE_SDL

class SDL_Surface;

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
    int loadImage( const char *file );

    // Locking
    int lock();
    void unlock();

    // Dimentions
    int height();
    int width();
    short int pitch();
    unsigned char bytesPerPixel();

    // Context Accessors
    void *pixels();
    unsigned int colorKey();
    void getRGBA(unsigned int pixel,    unsigned char *r,
                                        unsigned char *g,
                                        unsigned char *b,
                                        unsigned char *a);
    void getRGB(unsigned int pixel,     unsigned char *r,
                                        unsigned char *g,
                                        unsigned char *b);

    ui_palette *palette;

protected:
    // Palette functions
    void createPalette();
    void destroyPalette();

    SDL_Surface *surf;
};

#endif //USE_SDL
#endif //USE_TILE
#endif //include guard
