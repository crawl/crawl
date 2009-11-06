#ifndef MAPCELL_H
#define MAPCELL_H

#include "show.h"

/*
 * A map_cell stores what the player knows about a cell.
 * These go in env.map.
 *
 * TODO: Remove property -- that should go in env.grid.
 */

struct map_cell
{
    show_type object;       // The object: monster, item, feature, or cloud.
    unsigned short flags;   // Flags describing the mappedness of this square.
    unsigned short colour;
    unsigned long property; // Flags for blood, sanctuary, ...

    map_cell() : object(), flags(0), colour(0), property(0) { }
    void clear() { flags = colour = 0; object = show_type(); }

    unsigned glyph() const;
    bool known() const;
    bool seen() const;
};

#endif

