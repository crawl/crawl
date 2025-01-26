#pragma once

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
    // TODO: should this be tileidx_t?
    unsigned short tile;
    coord_def pos;
    killer_type killer;
};

/*
 * A map_cell stores what the player knows about a cell.
 * These go in env.map_knowledge.
 * TODO: this can shrink to 32 bytes by shrinking enums
 */
struct map_cell
{
    map_cell() : flags(0), _feat(DNGN_UNSEEN), _feat_colour(0),
                 _trap(TRAP_UNASSIGNED),
                 _cloud(nullptr), _item(nullptr), _mons(nullptr)
    {
    }

    ~map_cell() = default;

    // copy constructor
    map_cell(const map_cell& o): flags(o.flags), _feat(o._feat),
                                    _feat_colour(o._feat_colour),
                                    _trap(o._trap)
    {
        _cloud = o._cloud ? make_unique<cloud_info>(*o._cloud) : nullptr;
        _item = o._item ? make_unique<item_def>(*o._item) : nullptr;
        _mons = o._mons ? make_unique<monster_info>(*o._mons) : nullptr;
    }

    // copy assignment
    map_cell& operator=(const map_cell& o)
    {
        if (this == &o)
            return *this;

        flags = o.flags;
        _feat = o._feat;
        _feat_colour = o._feat_colour;
        _trap = o._trap;
        _cloud = o._cloud ? make_unique<cloud_info>(*o._cloud) : nullptr;
        _item = o._item ? make_unique<item_def>(*o._item) : nullptr;
        _mons = o._mons ? make_unique<monster_info>(*o._mons) : nullptr;

        return *this;
    }

    // move constructor
    map_cell(map_cell&& o) noexcept:
        flags(o.flags),
        _feat(o._feat),
        _feat_colour(o._feat_colour),
        _trap(o._trap),
        _cloud(std::move(o._cloud)),
        _item(std::move(o._item)),
        _mons(std::move(o._mons))

    {
    }

    // move assignment
    map_cell& operator=(map_cell&& o) noexcept
    {
        flags = o.flags;
        _feat = o._feat;
        _feat_colour = o._feat_colour;
        _trap = o._trap;
        _cloud = std::move(o._cloud);
        _item = std::move(o._item);
        _mons = std::move(o._mons);
        return *this;
    }

    friend bool operator==(const map_cell &lhs, const map_cell &rhs) {
        // TODO: consider providing a proper equality operator
        // item_def and monster_info currently lack such operators
        // Which makes it impossible for map_cell to provide one
        // As far as I can tell, packed_cell is the only user of this
        // And packed_cell operator== doesn't *seem* to be used
        return &lhs == &rhs;
    }

    friend bool operator!=(const map_cell &lhs, const map_cell &rhs) {
        return !(lhs == rhs);
    }

    void clear()
    {
        *this = map_cell();
    }

    // Clear prior to show update. Need to retain at least "seen" flag.
    void clear_data()
    {
        const uint32_t f = flags & (MAP_SEEN_FLAG | MAP_CHANGED_FLAG
                                    | MAP_INVISIBLE_UPDATE);
        clear();
        flags = f;
    }

    dungeon_feature_type feat() const
    {
        // Ugh; MSVC makes the bit field signed even though that means it can't
        // actually hold all the enum values. That seems to be in contradiction
        // of the standard (§9.6 [class.bit] paragraph 4) but what can you do?
        return static_cast<dungeon_feature_type>(static_cast<uint8_t>(_feat));
    }

    unsigned feat_colour() const
    {
        return _feat_colour;
    }

    void set_feature(dungeon_feature_type nfeat, unsigned colour = 0,
                     trap_type tr = TRAP_UNASSIGNED)
    {
        _feat = nfeat;
        _feat_colour = colour;
        _trap = tr;
    }

    item_def* item() const
    {
        return _item.get();
    }

    bool detected_item() const
    {
        const bool ret = !!(flags & MAP_DETECTED_ITEM);
        // TODO: change to an ASSERT when the underlying crash goes away
        if (ret && !_item)
        {
            //clear_item();
            return false;
        }
        return ret;
    }

    void set_item(const item_def& ii, bool more_items)
    {
        clear_item();
        _item = make_unique<item_def>(ii);
        if (more_items)
            flags |= MAP_MORE_ITEMS;
    }

    void set_detected_item();

    void clear_item()
    {
        // TODO: internal callers are doing a bit of duplicate work here
        _item.reset();
        flags &= ~(MAP_DETECTED_ITEM | MAP_MORE_ITEMS);
    }

    monster_type monster() const
    {
        return _mons ? _mons->type : MONS_NO_MONSTER;
    }

    monster_info* monsterinfo() const
    {
        return _mons.get();
    }

    void set_monster(const monster_info& mi)
    {
        clear_monster();
        _mons = make_unique<monster_info>(mi);
    }

    bool detected_monster() const
    {
        return !!(flags & MAP_DETECTED_MONSTER);
    }

    bool invisible_monster() const
    {
        return !!(flags & MAP_INVISIBLE_MONSTER);
    }

    void set_detected_monster(monster_type mons)
    {
        clear_monster();
        _mons = make_unique<monster_info>(MONS_SENSED);
        _mons->base_type = mons;
        flags |= MAP_DETECTED_MONSTER;
    }

    void set_invisible_monster()
    {
        clear_monster();
        flags |= MAP_INVISIBLE_MONSTER;
    }

    void clear_monster()
    {
        // TODO: internal callers are doing a bit of duplicate work here
        _mons.reset();
        flags &= ~(MAP_DETECTED_MONSTER | MAP_INVISIBLE_MONSTER);
    }

    cloud_type cloud() const
    {
        return _cloud ? _cloud->type : CLOUD_NONE;
    }

    // TODO: should this be colour_t?
    unsigned cloud_colour() const
    {
        return _cloud ? _cloud->colour : static_cast<colour_t>(0);
    }

    cloud_info* cloudinfo() const
    {
        return _cloud.get();
    }

    void set_cloud(const cloud_info& ci)
    {
        _cloud = make_unique<cloud_info>(ci);
    }

    void clear_cloud()
    {
        _cloud.reset();
    }

    bool update_cloud_state();

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
        return _trap;
    }

public:
    uint32_t flags;   // Flags describing the mappedness of this square.
private:
    // TODO: shrink enums, shrink/re-order cloud_info and inline it
    dungeon_feature_type _feat:8;
    colour_t _feat_colour;
    trap_type _trap:8;
    unique_ptr<cloud_info> _cloud;
    unique_ptr<item_def> _item;
    unique_ptr<monster_info> _mons;
};
