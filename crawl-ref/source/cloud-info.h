#pragma once

#include <cstdint>

#include "cloud-type.h"
#include "coord-def.h"
#include "defines.h"
#include "killer-type.h"

struct cloud_info
{
    cloud_info() : type(CLOUD_NONE), colour(0), duration(3), tile(0), pos(0, 0),
        killer(KILL_NONE)
    { }

    cloud_info(cloud_type t, colour_t c,
        uint8_t dur, unsigned short til, coord_def gc,
        killer_type kill)
        : type(t), colour(c), duration(dur), tile(til), pos(gc), killer(kill)
    { }

    friend bool operator==(const cloud_info &lhs, const cloud_info &rhs) {
        return lhs.type == rhs.type
            && lhs.colour == rhs.colour
            && lhs.duration == rhs.duration
            && lhs.tile == rhs.tile
            && lhs.pos == rhs.pos
            && lhs.killer == rhs.killer;
    }

    friend bool operator!=(const cloud_info &lhs, const cloud_info &rhs) {
        return !(lhs == rhs);
    }


    cloud_type type : 8;
    colour_t colour;
    uint8_t duration; // decay/20, clamped to 0-3
    union
    {
        // TODO: should this be tileidx_t?
        unsigned short tile;
        uint16_t next_free;
    };
    coord_def pos;
    killer_type killer;
};
