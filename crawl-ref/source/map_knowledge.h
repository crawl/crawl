#ifndef MAP_KNOWLEDGE_H
#define MAP_KNOWLEDGE_H

#include "show.h"

#define MAP_MAGIC_MAPPED_FLAG   0x01
#define MAP_SEEN_FLAG           0x02
#define MAP_CHANGED_FLAG        0x04 // FIXME: this doesn't belong here
#define MAP_DETECTED_MONSTER    0x08
#define MAP_DETECTED_ITEM       0x10
#define MAP_VISIBLE_FLAG            0x20
#define MAP_GRID_KNOWN          0xFF

/*
 * A map_cell stores what the player knows about a cell.
 * These go in env.map_knowledge_knowledge.
 */
struct map_cell
{
    show_type object;       // The object: monster, item, feature, or cloud.
    unsigned short flags;   // Flags describing the mappedness of this square.

    map_cell() : object(), flags(0) { }
    void clear() { flags = 0; object = show_type(); }

    dungeon_feature_type feat() const
    {
        return object.feat;
    }

    show_item_type item() const
    {
        return object.item;
    }

    monster_type monster() const
    {
        return object.mons;
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

    bool detected_item() const
    {
        return !!(flags & MAP_DETECTED_ITEM);
    }

    bool detected_monster() const
    {
        return !!(flags & MAP_DETECTED_MONSTER);
    }

    void set_detected_item(bool detected)
    {
        if (detected)
            flags |= MAP_DETECTED_ITEM;
        else
            flags &= ~MAP_DETECTED_ITEM;
    }

    void set_detected_monster(bool detected)
    {
        if (detected)
            flags |= MAP_DETECTED_MONSTER;
        else
            flags &= ~MAP_DETECTED_MONSTER;
    }
};

void set_map_knowledge_obj(const coord_def& where, show_type object);

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
