#include "AppHdr.h"

#include "map_knowledge.h"

#include "coordit.h"
#include "dgnevent.h"
#include "directn.h"
#include "env.h"
#include "feature.h"
#include "mon-util.h"
#include "notes.h"
#include "options.h"
#include "overmap.h"
#include "show.h"
#include "showsymb.h"
#include "stuff.h"
#include "terrain.h"
#include "view.h"

// These are hidden from the rest of the world... use the functions
// below to get information about the map grid.
#define MAP_MAGIC_MAPPED_FLAG   0x01
#define MAP_SEEN_FLAG           0x02
#define MAP_CHANGED_FLAG        0x04 // FIXME: this doesn't belong here
#define MAP_DETECTED_MONSTER    0x08
#define MAP_DETECTED_ITEM       0x10
#define MAP_GRID_KNOWN          0xFF

unsigned map_cell::glyph(bool clean_map) const
{
    if (!object)
        return (' ');
    if ((clean_map && object.is_cleanable_monster())
         || object.cls < SH_MONSTER)
    {
        const feature_def &fdef = get_feature_def(object);
        return ((flags & MAP_SEEN_FLAG) ? fdef.symbol : fdef.magic_symbol);
    }
    else
        return (mons_char(object.mons));
}

dungeon_feature_type map_cell::feat() const
{
    return (object.feat);
}

show_item_type map_cell::item() const
{
    return (object.item);
}

bool map_cell::known() const
{
    return (object && (flags & MAP_GRID_KNOWN));
}

bool map_cell::seen() const
{
    return (object && (flags & MAP_SEEN_FLAG));
}

unsigned get_map_knowledge_char(int x, int y)
{
    return env.map_knowledge[x][y].glyph(Options.clean_map);
}

show_type get_map_knowledge_obj(int x, int y)
{
    return (env.map_knowledge[x][y].object);
}

void set_map_knowledge_detected_item(int x, int y, bool detected)
{
    if (detected)
    {
        env.map_knowledge[x][y].flags |= MAP_DETECTED_ITEM;
        env.map_knowledge[x][y].object.colour = Options.detected_item_colour;
    }
    else
        env.map_knowledge[x][y].flags &= ~MAP_DETECTED_ITEM;
}

bool is_map_knowledge_detected_item(int x, int y)
{
    return (env.map_knowledge[x][y].flags & MAP_DETECTED_ITEM);
}

void set_map_knowledge_detected_mons(int x, int y, bool detected)
{
    if (detected)
    {
        env.map_knowledge[x][y].flags |= MAP_DETECTED_MONSTER;
        env.map_knowledge[x][y].object.colour = Options.detected_monster_colour;
    }
    else
        env.map_knowledge[x][y].flags &= ~MAP_DETECTED_MONSTER;
}

bool is_map_knowledge_detected_mons(int x, int y)
{
    return (env.map_knowledge[x][y].flags & MAP_DETECTED_MONSTER);
}

void set_map_knowledge_obj(const coord_def& where, show_type obj)
{
    env.map_knowledge(where).object = obj;
#ifdef USE_TILE
    tiles.update_minimap(where.x, where.y);
#endif
}

bool is_map_knowledge_item(int x, int y)
{
    return (env.map_knowledge[x][y].object.cls == SH_ITEM);
}

bool is_map_knowledge_mons(int x, int y)
{
    return (env.map_knowledge[x][y].object.cls == SH_MONSTER);
}

int get_map_knowledge_col(const coord_def& p)
{
    return (env.map_knowledge[p.x][p.y].object.colour);
}

bool is_terrain_known( int x, int y )
{
    return (env.map_knowledge[x][y].known());
}

bool is_terrain_known(const coord_def &p)
{
    return (env.map_knowledge(p).known());
}

bool is_terrain_seen( int x, int y )
{
    return (env.map_knowledge[x][y].flags & MAP_SEEN_FLAG);
}

bool is_terrain_changed( int x, int y )
{
    return (env.map_knowledge[x][y].flags & MAP_CHANGED_FLAG);
}

bool is_terrain_mapped(const coord_def &p)
{
    return (env.map_knowledge(p).flags & MAP_MAGIC_MAPPED_FLAG);
}

// Used to mark dug out areas, unset when terrain is seen or mapped again.
void set_terrain_changed( int x, int y )
{
    env.map_knowledge[x][y].flags |= MAP_CHANGED_FLAG;

    dungeon_events.fire_position_event(DET_FEAT_CHANGE, coord_def(x, y));
}

void set_terrain_mapped( int x, int y )
{
    map_cell* cell = &env.map_knowledge[x][y];
    cell->flags &= (~MAP_CHANGED_FLAG);
    cell->flags |= MAP_MAGIC_MAPPED_FLAG;
    cell->object.colour = get_feature_def(cell->object).map_colour;
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

        if (is_map_knowledge_detected_mons(*ri))
            count++;
    }

    return (count);
}

void clear_map(bool clear_detected_items, bool clear_detected_monsters)
{
    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        const coord_def p = *ri;
        if (!is_terrain_known(p))
            continue;
        if (is_map_knowledge_item(p))
            continue;
        if (!clear_detected_items && is_map_knowledge_detected_item(p))
            continue;
        if (!clear_detected_monsters && is_map_knowledge_detected_mons(p))
            continue;

#ifdef USE_TILE
        if (is_terrain_mapped(p) && !is_map_knowledge_detected_mons(p))
            continue;
#endif

        show_type plain = env.map_knowledge(p).object;

        // If it's an immobile monster or a feature, don't erase.
        if ((plain.cls != SH_MONSTER || plain.is_cleanable_monster())
            && plain.cls != SH_FEATURE)
        {
            plain = show_type(plain.feat);
        }

        set_map_knowledge_obj(p, to_knowledge(plain));
        set_map_knowledge_detected_mons(p, false);
        set_map_knowledge_detected_item(p, false);

#ifdef USE_TILE
        // FIXME: shouldn't be referencing env.grid here.
        if (is_terrain_mapped(p))
        {
            unsigned int fg;
            unsigned int bg;
            tileidx_unseen(fg, bg, get_feat_symbol(grd(p)), p);
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
            if (env.map_knowledge[x][y].flags & MAP_SEEN_FLAG)
                _automap_from(x, y, passive);
}

void set_terrain_seen( int x, int y )
{
    const dungeon_feature_type feat = grd[x][y];
    map_cell* cell = &env.map_knowledge[x][y];

    // First time we've seen a notable feature.
    if (!(cell->flags & MAP_SEEN_FLAG))
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
    cell->flags &= ~(MAP_DETECTED_ITEM);
    cell->flags &= ~(MAP_DETECTED_MONSTER);
#endif

    cell->flags &= (~MAP_CHANGED_FLAG);
    cell->flags |= MAP_SEEN_FLAG;
    cell->object.colour = get_feature_def(cell->object).seen_colour;
}

void clear_map_knowledge_grid( const coord_def& p )
{
    env.map_knowledge(p).clear();
}

void clear_map_knowledge_grid( int x, int y )
{
    env.map_knowledge[x][y].clear();
}
