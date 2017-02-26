#pragma once

#include "enum.h"
#include "mon-info.h"
#include "trap-type.h"

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

    cloud_type type:8;
    colour_t colour;
    uint8_t duration; // decay/20, clamped to 0-3
    unsigned short tile;
    coord_def pos;
    killer_type killer;
};

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

/* these flags require more space to serialize: put infrequently used ones there */
#define MAP_EXCLUDED_STAIRS  0x10000
#define MAP_MOLDY            0x20000
#define MAP_GLOWING_MOLDY    0x40000
#define MAP_SANCTUARY_1      0x80000
#define MAP_SANCTUARY_2     0x100000
#define MAP_WITHHELD        0x200000
#define MAP_LIQUEFIED       0x400000
#define MAP_ORB_HALOED      0x800000
#define MAP_UMBRAED        0x1000000
#define MAP_QUAD_HALOED    0X4000000
#define MAP_DISJUNCT       0X8000000
#if TAG_MAJOR_VERSION == 34
#define MAP_HOT           0x10000000
#endif

/*
 * A map_cell stores what the player knows about a cell.
 * These go in env.map_knowledge.
 */
struct map_cell
{
    map_cell() : flags(0), _feat(DNGN_UNSEEN), _feat_colour(0),
                 _trap(TRAP_UNASSIGNED), _cloud(0), _item(0), _mons(0)
    {
    }

    map_cell(const map_cell& c)
    {
        memcpy(this, &c, sizeof(map_cell));
        if (_cloud)
            _cloud = new cloud_info(*_cloud);
        if (_mons)
            _mons = new monster_info(*_mons);
        if (_item)
            _item = new item_info(*_item);
    }

    ~map_cell()
    {
        if (_cloud)
            delete _cloud;
        if (!(flags & MAP_DETECTED_MONSTER) && _mons)
            delete _mons;
        if (_item)
            delete _item;
    }

    map_cell& operator=(const map_cell& c)
    {
        if (&c == this)
            return *this;
        if (_cloud)
            delete _cloud;
        if (_mons)
            delete _mons;
        if (_item)
            delete _item;
        memcpy(this, &c, sizeof(map_cell));
        if (_cloud)
            _cloud = new cloud_info(*_cloud);
        if (_mons)
            _mons = new monster_info(*_mons);
        if (_item)
            _item = new item_info(*_item);
        return *this;
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
        return dungeon_feature_type(uint8_t(_feat));
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

    item_info* item() const
    {
        return _item;
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

    void set_item(const item_info& ii, bool more_items)
    {
        clear_item();
        _item = new item_info(ii);
        if (more_items)
            flags |= MAP_MORE_ITEMS;
    }

    void set_detected_item();

    void clear_item()
    {
        if (_item)
        {
            delete _item;
            _item = 0;
        }
        flags &= ~(MAP_DETECTED_ITEM | MAP_MORE_ITEMS);
    }

    monster_type monster() const
    {
        if (_mons)
            return _mons->type;
        else
            return MONS_NO_MONSTER;
    }

    monster_info* monsterinfo() const
    {
        return _mons;
    }

    void set_monster(const monster_info& mi)
    {
        clear_monster();
        _mons = new monster_info(mi);
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
        _mons = new monster_info(MONS_SENSED);
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
        if (_mons)
            delete _mons;
        flags &= ~(MAP_DETECTED_MONSTER | MAP_INVISIBLE_MONSTER);
        _mons = 0;
    }

    cloud_type cloud() const
    {
        if (_cloud)
            return _cloud->type;
        else
            return CLOUD_NONE;
    }

    unsigned cloud_colour() const
    {
        if (_cloud)
            return _cloud->colour;
        else
            return 0;
    }

    cloud_info* cloudinfo() const
    {
        return _cloud;
    }

    void set_cloud(const cloud_info& ci)
    {
        if (_cloud)
            delete _cloud;
        _cloud = new cloud_info(ci);
    }

    void clear_cloud()
    {
        if (_cloud)
        {
            delete _cloud;
            _cloud = 0;
        }
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
    dungeon_feature_type _feat:8;
    colour_t _feat_colour;
    trap_type _trap:8;
    cloud_info* _cloud;
    item_info* _item;
    monster_info* _mons;
};

void set_terrain_mapped(const coord_def c);
void set_terrain_seen(const coord_def c);

void set_terrain_visible(const coord_def c);
void clear_terrain_visibility();

int count_detected_mons();

void update_cloud_knowledge();

/**
 * @brief Clear non-terrain knowledge from the map.
 *
 * Cloud knowledge is always cleared. Item and monster knowledge clears depend
 * on @p clear_items and @p clear_mons, respectively.
 *
 * @param clear_items
 *  Clear item knowledge?
 * @param clear_mons
 *  Clear monster knowledge?
 */
void clear_map(bool clear_items = true, bool clear_mons = true);

/**
 * @brief If a travel trail exists, clear it; otherwise clear the map.
 *
 * When clearing the map, all non-terrain knowledge is wiped.
 *
 * @see clear_map() and clear_travel_trail()
 */
void clear_map_or_travel_trail();

map_feature get_cell_map_feature(const map_cell& cell);

void reautomap_level();

