#include "tile_colour.h"

tile_colour tile_colour::background(71, 108, 108, 255);
tile_colour tile_colour::transparent(0, 0, 0, 0);
tile_colour tile_colour::black(0, 0, 0, 255);

bool tile_colour::operator==(const tile_colour &rhs) const
{
    return (r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a);
}

bool tile_colour::operator!=(const tile_colour &rhs) const
{
    return (r != rhs.r || g != rhs.g || b != rhs.b || a != rhs.a);
}

const tile_colour &tile_colour::operator=(const tile_colour &rhs)
{
    r = rhs.r;
    g = rhs.g;
    b = rhs.b;
    a = rhs.a;

    return *this;
}
