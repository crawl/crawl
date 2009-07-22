#ifndef TILE_COLOUR_H
#define TILE_COLOUR_H

class tile_colour
{
public:
    tile_colour() {};
    tile_colour(unsigned char _r, unsigned char _g, unsigned char _b,
        unsigned char _a) : r(_r), g(_g), b(_b), a(_a) {}

    bool operator==(const tile_colour &rhs) const;
    bool operator!=(const tile_colour &rhs) const;
    const tile_colour &operator=(const tile_colour &rhs);

    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;

    static tile_colour background;
    static tile_colour transparent;
    static tile_colour black;
};

bool write_png(const char *filename, tile_colour *pixels,
               unsigned int width, unsigned int height);
#endif
