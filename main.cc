#include "AppHdr.h"
#include "env.h"

static char dchar(dungeon_feature_type g)
{
    switch(g)
    {
    case DNGN_FLOOR:
        return '.';
    case DNGN_ROCK_WALL:
        return '#';
    default:
        return '?';
    }
}

void draw()
{
    for (int y=0; y<GYM; y++)
    {
        for (int x=0; x<GXM; x++)
            printf("%c", dchar(grd[x][y]));
        printf("\n");
    }
}

int main()
{
    grd.init(DNGN_ROCK_WALL);
    draw();
    return 0;
}
