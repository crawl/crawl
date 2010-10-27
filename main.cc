#include "AppHdr.h"
#include "env.h"
#include <locale.h>

static wchar_t dchar(dungeon_feature_type g)
{
    switch(g)
    {
    case DNGN_FLOOR:
        return '.';
    case DNGN_ROCK_WALL:
        return '#';
    case DNGN_STONE_ARCH:
        return 0x2229;
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
    draw();
    return 0;
}
