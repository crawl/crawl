/*
 * File:      envmap.cc
 * Summary:   Functions dealing with env.map.
 */

#include "AppHdr.h"

#include "envmap.h"

#include "coord.h"
#include "coordit.h"
#include "dgnevent.h"
#include "directn.h"
#include "env.h"
#include "notes.h"
#include "overmap.h"
#include "showsymb.h"
#include "stuff.h"
#include "terrain.h"
#include "view.h"

// These are hidden from the rest of the world... use the functions
// below to get information about the map grid.
#define MAP_MAGIC_MAPPED_FLAG   0x01
#define MAP_SEEN_FLAG           0x02
#define MAP_CHANGED_FLAG        0x04
#define MAP_DETECTED_MONSTER    0x08
#define MAP_DETECTED_ITEM       0x10
#define MAP_GRID_KNOWN          0xFF

unsigned map_cell::glyph() const
{
    if (!object)
        return (' ');
    return get_symbol(object, NULL, !(flags & MAP_SEEN_FLAG));
}

bool map_cell::known() const
{
    return (object && (flags & MAP_GRID_KNOWN));
}

bool map_cell::seen() const
{
    return (object && (flags & MAP_SEEN_FLAG));
}

unsigned get_envmap_char(int x, int y)
{
    return env.map[x][y].glyph();
}

show_type get_envmap_obj(int x, int y)
{
    return (env.map[x][y].object);
}

void set_envmap_detected_item(int x, int y, bool detected)
{
    if (detected)
        env.map[x][y].flags |= MAP_DETECTED_ITEM;
    else
        env.map[x][y].flags &= ~MAP_DETECTED_ITEM;
}

bool is_envmap_detected_item(int x, int y)
{
    return (env.map[x][y].flags & MAP_DETECTED_ITEM);
}

void set_envmap_detected_mons(int x, int y, bool detected)
{
    if (detected)
        env.map[x][y].flags |= MAP_DETECTED_MONSTER;
    else
        env.map[x][y].flags &= ~MAP_DETECTED_MONSTER;
}

bool is_envmap_detected_mons(int x, int y)
{
    return (env.map[x][y].flags & MAP_DETECTED_MONSTER);
}

void set_envmap_glyph(int x, int y, show_type object, int col)
{
    map_cell &c = env.map[x][y];
    c.object = object;
    c.colour = col;
#ifdef USE_TILE
    tiles.update_minimap(x, y);
#endif
}

void set_envmap_glyph(const coord_def& c, show_type object, int col)
{
    set_envmap_glyph(c.x, c.y, object, col);
}

void set_envmap_obj(const coord_def& where, show_type obj)
{
    env.map(where).object = obj;
#ifdef USE_TILE
    tiles.update_minimap(where.x, where.y);
#endif
}

void set_envmap_col( int x, int y, int colour )
{
    env.map[x][y].colour = colour;
}

bool is_sanctuary(const coord_def& p)
{
    if (!map_bounds(p))
        return (false);
    return (testbits(env.map(p).property, FPROP_SANCTUARY_1)
            || testbits(env.map(p).property, FPROP_SANCTUARY_2));
}

bool is_bloodcovered(const coord_def& p)
{
    return (testbits(env.map(p).property, FPROP_BLOODY));
}

bool is_envmap_item(int x, int y)
{
    return (env.map[x][y].object.cls == SH_ITEM);
}

bool is_envmap_mons(int x, int y)
{
    return (env.map[x][y].object.cls == SH_MONSTER);
}

int get_envmap_col(const coord_def& p)
{
    return (env.map[p.x][p.y].colour);
}

bool is_terrain_known( int x, int y )
{
    return (env.map[x][y].known());
}

bool is_terrain_known(const coord_def &p)
{
    return (env.map(p).known());
}

bool is_terrain_seen( int x, int y )
{
    return (env.map[x][y].flags & MAP_SEEN_FLAG);
}

bool is_terrain_changed( int x, int y )
{
    return (env.map[x][y].flags & MAP_CHANGED_FLAG);
}

bool is_terrain_mapped(const coord_def &p)
{
    return (env.map(p).flags & MAP_MAGIC_MAPPED_FLAG);
}

// Used to mark dug out areas, unset when terrain is seen or mapped again.
void set_terrain_changed( int x, int y )
{
    env.map[x][y].flags |= MAP_CHANGED_FLAG;

    dungeon_events.fire_position_event(DET_FEAT_CHANGE, coord_def(x, y));
}

void set_terrain_mapped( int x, int y )
{
    env.map[x][y].flags &= (~MAP_CHANGED_FLAG);
    env.map[x][y].flags |= MAP_MAGIC_MAPPED_FLAG;
#ifdef USE_TILE
    tiles.update_minimap(x, y);
#endif
}

int count_detected_mons()
{
    int count = 0;
    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        // Don't expose new dug out areas:
        // Note: assumptions are being made here about how
        // terrain can change (eg it used to be solid, and
        // thus monster/item free).
        if (is_terrain_changed(*ri))
            continue;

        if (is_envmap_detected_mons(*ri))
            count++;
    }

    return (count);
}

void clear_map(bool clear_detected_items, bool clear_detected_monsters)
{
    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        const coord_def p = *ri;
        // Don't expose new dug out areas:
        // Note: assumptions are being made here about how
        // terrain can change (eg it used to be solid, and
        // thus monster/item free).

        // This reasoning doesn't make sense when it comes to *clearing*
        // the map! (jpeg)

        if (get_envmap_char(p) == 0)
            continue;

        if (is_envmap_item(p))
            continue;

        if (!clear_detected_items && is_envmap_detected_item(p))
            continue;

        if (!clear_detected_monsters && is_envmap_detected_mons(p))
            continue;

#ifdef USE_TILE
        if (is_terrain_mapped(p) && !is_envmap_detected_mons(p))
            continue;
#endif

        set_envmap_obj(p, show_type(is_terrain_seen(p) || is_terrain_mapped(p)
                                    ? grd(p) : DNGN_UNSEEN));
        set_envmap_detected_mons(p, false);
        set_envmap_detected_item(p, false);

#ifdef USE_TILE
        if (is_terrain_mapped(p))
        {
            dungeon_feature_type feature = grd(p);

            unsigned int feat_symbol;
            unsigned short feat_colour;
            get_show_symbol(show_type(feature), &feat_symbol, &feat_colour);

            unsigned int fg;
            unsigned int bg;
            tileidx_unseen(fg, bg, feat_symbol, p);
            env.tile_bk_bg(p) = bg;
            env.tile_bk_fg(p) = fg;
        }
        else
        {
            env.tile_bk_bg(p) = is_terrain_seen(p) ?
                tile_idx_unseen_terrain(p.x, p.y, grd(p)) :
                tileidx_feature(DNGN_UNSEEN, p.x, p.y);
            env.tile_bk_fg(p) = 0;
        }
#endif
    }
}

static void _automap_from( int x, int y, int mutated )
{
    if (mutated)
        magic_mapping(8 * mutated, 25, true, false,
                      true, true, coord_def(x,y));
}

void reautomap_level( )
{
    int passive = player_mutation_level(MUT_PASSIVE_MAPPING);

    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
            if (env.map[x][y].flags & MAP_SEEN_FLAG)
                _automap_from(x, y, passive);
}

void set_terrain_seen( int x, int y )
{
    const dungeon_feature_type feat = grd[x][y];

    // First time we've seen a notable feature.
    if (!(env.map[x][y].flags & MAP_SEEN_FLAG))
    {
        _automap_from(x, y, player_mutation_level(MUT_PASSIVE_MAPPING));

        const bool boring = !is_notable_terrain(feat)
            // A portal deeper into the Zigguart is boring.
            || (feat == DNGN_ENTER_PORTAL_VAULT
                && you.level_type == LEVEL_PORTAL_VAULT)
            // Altars in the temple are boring.
            || (feat_is_altar(feat)
                && player_in_branch(BRANCH_ECUMENICAL_TEMPLE))
            // Only note the first entrance to the Abyss/Pan/Hell
            // which is found.
            || ((feat == DNGN_ENTER_ABYSS || feat == DNGN_ENTER_PANDEMONIUM
                 || feat == DNGN_ENTER_HELL)
                && overmap_knows_num_portals(feat) > 1)
            // There are at least three Zot entrances, and they're always
            // on D:27, so ignore them.
            || feat == DNGN_ENTER_ZOT;

        if (!boring)
        {
            coord_def pos(x, y);
            std::string desc =
                feature_description(pos, false, DESC_NOCAP_A);

            take_note(Note(NOTE_SEEN_FEAT, 0, 0, desc.c_str()));
        }
    }

#ifdef USE_TILE
    env.map[x][y].flags &= ~(MAP_DETECTED_ITEM);
    env.map[x][y].flags &= ~(MAP_DETECTED_MONSTER);
#endif

    env.map[x][y].flags &= (~MAP_CHANGED_FLAG);
    env.map[x][y].flags |= MAP_SEEN_FLAG;
}

void clear_envmap_grid( const coord_def& p )
{
    env.map(p).clear();
}

void clear_envmap_grid( int x, int y )
{
    env.map[x][y].clear();
}
