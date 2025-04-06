#pragma once

#include <cstdint>
#include <libutil.h>

#include "enum.h"
#include "mon-info.h"
#include "tag-version.h"
#include "trap-type.h"

#define MAP_MAGIC_MAPPED_FLAG   0x01
#define MAP_SEEN_FLAG           0x02
#define MAP_CHANGED_FLAG        0x04 // FIXME: this doesn't belong here
#define MAP_DETECTED_MONSTER    0x08
#define MAP_INVISIBLE_MONSTER   0x10
#define MAP_DETECTED_ITEM       0x20
#define MAP_VISIBLE_FLAG        0x40
#define MAP_GRID_KNOWN          0xFF

#define MAP_EMPHASIZE          0x100
#define MAP_MORE_ITEMS         0x200
#define MAP_HALOED             0x400
#define MAP_SILENCED           0x800
#define MAP_BLOODY            0x1000
#define MAP_CORRODING         0x2000
#define MAP_INVISIBLE_UPDATE  0x4000 // Used for invis redraws by show_init()
#define MAP_ICY               0x8000

/* these flags require more space to serialize: put infrequently used ones there */
#define MAP_EXCLUDED_STAIRS  0x10000
#define MAP_SANCTUARY_1      0x80000
#define MAP_SANCTUARY_2     0x100000
#define MAP_WITHHELD        0x200000
#define MAP_LIQUEFIED       0x400000
#define MAP_ORB_HALOED      0x800000
#define MAP_UMBRAED        0x1000000
#define MAP_QUAD_HALOED    0X4000000
#define MAP_DISJUNCT       0X8000000
#define MAP_BLASPHEMY     0X10000000
#define MAP_BFB_CORPSE    0X20000000

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


    cloud_type type:8;
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

/*
 * A map_cell stores what the player knows about a cell.
 * These go in env.map_knowledge.
 */
struct map_cell
{
    map_cell() noexcept
    {
    }

    ~map_cell() = default;

    // copy constructor
    map_cell(const map_cell& o) noexcept
    {
        *this = o;
    }

    // copy assignment
    map_cell& operator=(const map_cell& o) noexcept = default;

    dungeon_feature_type feat() const
    {
        COMPILE_CHECK(NUM_FEATURES <= 256);
        return static_cast<dungeon_feature_type>(_feat);
    }

    unsigned feat_colour() const
    {
        return _feat_colour;
    }

    void set_feature(dungeon_feature_type nfeat, unsigned colour = 0,
                     trap_type tr = TRAP_UNASSIGNED)
    {
        _feat = static_cast<uint8_t>(nfeat);
        _feat_colour = colour;
        _trap = static_cast<uint8_t>(tr);
    }

    bool detected_item() const
    {
        const bool ret = !!(flags & MAP_DETECTED_ITEM);
        // TODO: change to an ASSERT when the underlying crash goes away
        if (ret && _item_index == UINT16_MAX)
        {
            //clear_item();
            return false;
        }
        return ret;
    }

    bool detected_monster() const
    {
        return !!(flags & MAP_DETECTED_MONSTER);
    }

    bool invisible_monster() const
    {
        return !!(flags & MAP_INVISIBLE_MONSTER);
    }

    bool known() const
    {
        return !!(flags & MAP_GRID_KNOWN);
    }

    bool seen() const
    {
        return !!(flags & MAP_SEEN_FLAG);
    }

    bool visible() const
    {
        return !!(flags & MAP_VISIBLE_FLAG);
    }

    bool changed() const
    {
        return !!(flags & MAP_CHANGED_FLAG);
    }

    bool mapped() const
    {
        return !!(flags & MAP_MAGIC_MAPPED_FLAG);
    }

    trap_type trap() const
    {
        COMPILE_CHECK(NUM_TRAPS <= 256);
        return static_cast<trap_type>(_trap);
    }
public:
    uint32_t flags = 0;   // Flags describing the mappedness of this square.
private:
    // Maybe shrink/re-order cloud_info and inline it?

    uint16_t _cloud_index = UINT16_MAX;
    uint16_t _item_index = UINT16_MAX;
    uint16_t _mons_index = UINT16_MAX;
    uint8_t _feat = (uint8_t)DNGN_UNSEEN;
    colour_t _feat_colour = 0;
    uint8_t _trap = (uint8_t)TRAP_UNASSIGNED;

    friend class MapKnowledge;
};
