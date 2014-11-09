#ifndef TILE_COLOUR_H
#define TILE_COLOUR_H

enum COLORS
{
    BLACK,
    BLUE,
    GREEN,
    CYAN,
    RED,
    MAGENTA,
    BROWN,
    LIGHTGRAY,
    LIGHTGREY = LIGHTGRAY,
    DARKGRAY,
    DARKGREY = DARKGRAY,
    LIGHTBLUE,
    LIGHTGREEN,
    LIGHTCYAN,
    LIGHTRED,
    LIGHTMAGENTA,
    YELLOW,
    WHITE,
    MAX_TERM_COLOUR,
    MAX_COLOUR = MAX_TERM_COLOUR
};

class tile_colour
{
public:
    tile_colour() {};
    tile_colour(unsigned char _r, unsigned char _g, unsigned char _b,
        unsigned char _a) : r(_r), g(_g), b(_b), a(_a) {}

    bool operator==(const tile_colour &rhs) const;
    bool operator!=(const tile_colour &rhs) const;
    const tile_colour &operator=(const tile_colour &rhs);

    unsigned char &operator[](int idx);
    unsigned char operator[](int idx) const;

    bool is_transparent() const { return a == 0; }

    // Get the HSV/HSL hue, from 0..360.
    int get_hue() const;
    // Set the hue, from 0..360.
    void set_hue(int h);
    // Change the saturation to 0.
    void desaturate();
    // Change the luminance by lum_percent %.
    void change_lum(int lum_percent);

    int get_max_rgb() const;
    int get_min_rgb() const;

    // Set the color from HSV.  hue is 0..360.  min_rgb and max_rgb are 0..255.
    void set_from_hue(int hue, int min_rgb, int max_rgb);

    // Set the color from HSL.  hue is 0..360.  sat and lum are 0..1.
    void set_from_hsl(int hue, float sat, float lum);
    // Get the HSL saturation, from 0..1.
    float get_sat() const;
    // Get the HSL luminance, from 0..1.
    float get_lum() const;

    // The order of these fields is significant -- see tile::load
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
