#include "AppHdr.h"

#ifdef USE_TILE_LOCAL
#include "tilefont.h"

const VColour term_colours[NUM_TERM_COLOURS] =
{
    VColour(  0,   0,   0), // BLACK
    VColour(  0,  82, 255), // BLUE
    VColour(100, 185,  70), // GREEN
    VColour(  0, 180, 180), // CYAN
    VColour(255,  48,   0), // RED
    VColour(238,  92, 238), // MAGENTA
    VColour(165,  91,   0), // BROWN
    VColour(162, 162, 162), // LIGHTGREY
    VColour( 82,  82,  82), // DARKGREY
    VColour( 82, 102, 255), // LIGHTBLUE
    VColour( 82, 255,  82), // LIGHTGREEN
    VColour( 82, 255, 255), // LIGHTCYAN
    VColour(255,  82,  82), // LIGHTRED
    VColour(255,  82, 255), // LIGHTMAGENTA
    VColour(255, 255,  82), // YELLOW
    VColour(255, 255, 255)  // WHITE
};

#endif // USE_TILE_LOCAL
