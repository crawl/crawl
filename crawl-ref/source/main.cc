#include "AppHdr.h"
#include "env.h"
#include "rng.h"
#include "dgn-voronoi.h"
#include "dgn-perlin.h"
#include <locale.h>

static wchar_t dchar(dungeon_feature_type g)
{
    switch(g)
    {
    case DNGN_FLOOR:
        return '.';
    case DNGN_ROCK_WALL:
        return '#';
    case DNGN_STONE_WALL:
        return 0x2588;
    case DNGN_STONE_ARCH:
        return 0x2229;
    case DNGN_DEEP_WATER:
        return 0x2248;
    case DNGN_SHALLOW_WATER:
        return '~';
    case DNGN_TREE:
        return 0x2663;
    default:
        return '?';
    }
}

void draw()
{
    for (int y=0; y<GYM; y++)
    {
        for (int x=0; x<GXM; x++)
            printf("%C", dchar(grd[x][y]));
        printf("\n");
    }
}

int main()
{
    setlocale(LC_CTYPE, "");
    grd.init(DNGN_ROCK_WALL);
    seed_rng();
    layout_perlin();
    draw();
    return 0;
}
