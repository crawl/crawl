#include <stdio.h>
#include <string.h>

// This is a standalone utility to convert old-style palettized BMPs 
// to transparent PNGs.

#include "tile.h"
#include <SDL_main.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: %s (filename.bmp)\n", argv[0]);
        return -1;
    }

    char dest[1024];
    strcpy(dest, argv[1]);

    size_t len = strlen(dest);
    if (!strcmp(&dest[len-4], ".bmp"))
    {
        dest[len-3] = 'p';
        dest[len-2] = 'n';
        dest[len-1] = 'g';
    }
    else if (!strcmp(&dest[len-4], ".png"))
    {
        // ok as-is.
    }
    else
    {
        printf("File '%s' does not end in bmp or png.\n", argv[1]);
        return -2;
    }

    tile conv;
    if (!conv.load(argv[1]))
    {
        printf("Failed to load '%s'.\n", argv[1]);
        return -3;
    }

    conv.replace_colour(tile_colour::background, tile_colour::transparent);

    if (!write_png(dest, &conv.get_pixel(0,0), conv.width(), conv.height()))
    {
        printf("Failed to write dest '%s'.\n", dest);
        return -4;
    }

    printf("Converted '%s'.\n", argv[1]);

    return 0;
}
