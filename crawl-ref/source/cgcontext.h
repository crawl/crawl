#ifndef CG_CONTEXT_H
#define CG_CONTEXT_H

#ifdef USE_TILE

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
    virtual ~GraphicsContext() {};

    // Static Alloc/deallocators
    static GraphicsContext *create();

    // Loading
    virtual int load_image( const char *file ) = 0;

    // Locking
    virtual int lock() = 0;
    virtual void unlock() = 0;

    // Dimentions
    virtual int height() = 0;
    virtual int width() = 0;
    virtual short int pitch() = 0;
    virtual unsigned char bytes_per_pixel() = 0;

    // Context Accessors
    virtual void *pixels() = 0;
    virtual void *native_surface() = 0;
    virtual unsigned int color_key() = 0;
    virtual void get_rgba(unsigned int pixel,   unsigned char *r,
                                                unsigned char *g,
                                                unsigned char *b,
                                                unsigned char *a) = 0;
    virtual void get_rgb(unsigned int pixel,    unsigned char *r,
                                                unsigned char *g,
                                                unsigned char *b) = 0;

    // TODO: Maybe this should have mandator accessors?
    ui_palette *palette;
};

#endif //USE_TILE
#endif //include guard
