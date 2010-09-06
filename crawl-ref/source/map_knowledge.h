#ifndef MAP_KNOWLEDGE_H
#define MAP_KNOWLEDGE_H

#include "show.h"
#include "mon-info.h"

#define MAP_MAGIC_MAPPED_FLAG   0x01
#define MAP_SEEN_FLAG           0x02
#define MAP_CHANGED_FLAG        0x04 // FIXME: this doesn't belong here
#define MAP_DETECTED_MONSTER    0x08
#define MAP_INVISIBLE_MONSTER   0x10
#define MAP_DETECTED_ITEM       0x20
#define MAP_VISIBLE_FLAG        0x40
#define MAP_GRID_KNOWN          0xFF

#define MAP_EMPHASIZE 0x100
#define MAP_MORE_ITEMS 0x200
#define MAP_HALOED 0x400
#define MAP_SILENCED 0x800
#define MAP_BLOODY 0x1000
#define MAP_CORRODING 0x2000

/* these flags require more space to serialize: put infrequently used ones there */
#define MAP_EXCLUDED_STAIRS 0x10000
#define MAP_MOLDY 0x20000
#define MAP_GLOWING_MOLDY 0x40000
#define MAP_SANCTUARY_1 0x80000
#define MAP_SANCTUARY_2 0x100000
#define MAP_WITHHELD 0x200000

/*
 * A map_cell stores what the player knows about a cell.
 * These go in env.map_knowledge_knowledge.
 */
struct map_cell
{
    unsigned short flags;   // Flags describing the mappedness of this square.

    map_cell() : flags(0), _feat(DNGN_UNSEEN), _feat_colour(0), _item(0), _cloud(CLOUD_NONE)
    {
        memset(&_mons, 0, sizeof(_mons));
    }

    map_cell(const map_cell& c)
    {
        memcpy(this, &c, sizeof(map_cell));
        if (!(flags & MAP_DETECTED_MONSTER) && _mons.info)
            _mons.info = new monster_info(*_mons.info);
        if (_item)
            _item = new item_info(*_item);
    }

    ~map_cell()
    {
        if (!(flags & MAP_DETECTED_MONSTER) && _mons.info)
            delete _mons.info;
        if (_item)
            delete _item;
    }

    void clear()
    {
        *this = map_cell();
    }

    // Clear prior to show update. Need to retain at least "seen" flag.
    void clear_data()
    {
        const uint8_t f = flags & MAP_SEEN_FLAG;
        clear();
        flags = f;
    }

    dungeon_feature_type feat() const
    {
        return _feat;
    }

    unsigned feat_colour() const
    {
        return _feat_colour;
    }

    void set_feature(dungeon_feature_type nfeat, unsigned colour = 0)
    {
        _feat = nfeat;
        _feat_colour = colour;
    }

    item_info* item() const
    {
        return _item;
    }

    bool detected_item() const
    {
        return !!(flags & MAP_DETECTED_ITEM);
    }

    void set_item(const item_info& ii, bool more_items)
    {
        clear_item();
        _item = new item_info(ii);
        if (more_items)
            flags |= MAP_MORE_ITEMS;
    }

    void set_detected_item()
    {
        clear_item();
        flags |= MAP_DETECTED_ITEM;
    }

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
        if (flags & MAP_DETECTED_MONSTER)
            return _mons.detected;
        else if (_mons.info)
            return _mons.info->type;
        else
            return MONS_NO_MONSTER;
    }

    monster_info* monsterinfo() const
    {
        if (flags & MAP_DETECTED_MONSTER)
            return 0;
        else
            return _mons.info;
    }

    void set_monster(const monster_info& mi)
    {
        clear_monster();
        _mons.info = new monster_info(mi);
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
        _mons.detected = mons;
        flags |= MAP_DETECTED_MONSTER;
    }

    void set_invisible_monster()
    {
        clear_monster();
        flags |= MAP_INVISIBLE_MONSTER;
    }

    void clear_monster()
    {
        if (!(flags & MAP_DETECTED_MONSTER) && _mons.info)
            delete _mons.info;
        flags &= ~(MAP_DETECTED_MONSTER | MAP_INVISIBLE_MONSTER);
        memset(&_mons, 0, sizeof(_mons));
    }

    cloud_type cloud() const
    {
        return _cloud;
    }

    void set_cloud(cloud_type ncloud)
    {
        _cloud = ncloud;
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

private:
    dungeon_feature_type _feat;
    uint8_t _feat_colour;
    item_info* _item;
    union
    {
        monster_info* info;
        monster_type detected;
    } _mons;
    cloud_type _cloud;
};

void set_terrain_mapped( int x, int y );
inline void set_terrain_mapped( const coord_def& c ) {
    set_terrain_mapped(c.x,c.y);
}
void set_terrain_seen( int x, int y );
inline void set_terrain_seen( const coord_def& c ) {
    set_terrain_seen(c.x, c.y);
}
void set_terrain_changed( int x, int y );

inline void set_terrain_changed(const coord_def &c)
{
    set_terrain_changed(c.x, c.y);
}

void set_terrain_visible(const coord_def &c);
void clear_terrain_visibility();

int count_detected_mons(void);
void map_knowledge_forget_mons(const coord_def &c);

void clear_map(bool clear_items = true, bool clear_mons = true);

#endif
