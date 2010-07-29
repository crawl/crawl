#include "AppHdr.h"

#include "map_knowledge.h"

#include "coordit.h"
#include "dgn-overview.h"
#include "dgnevent.h"
#include "directn.h"
#include "env.h"
#include "feature.h"
#include "los.h"
#include "mon-util.h"
#include "notes.h"
#include "options.h"
#include "show.h"
#include "showsymb.h"
#include "stuff.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tilepick.h"
 #include "tileview.h"
#endif
#include "view.h"

void map_knowledge_forget_mons(const coord_def& c)
{
    if (!env.map_knowledge(c).detected_monster())
        return;

    env.map_knowledge(c).clear_monster();
}

// Used to mark dug out areas, unset when terrain is seen or mapped again.
void set_terrain_changed( int x, int y )
{
    env.map_knowledge[x][y].flags |= MAP_CHANGED_FLAG;

    dungeon_events.fire_position_event(DET_FEAT_CHANGE, coord_def(x, y));

    los_terrain_changed(coord_def(x,y));
}

void set_terrain_mapped( int x, int y )
{
    const coord_def gc(x, y);
    map_cell* cell = &env.map_knowledge(gc);
    cell->flags &= (~MAP_CHANGED_FLAG);
    cell->flags |= MAP_MAGIC_MAPPED_FLAG;
#ifdef USE_TILE
    tiles.update_minimap(gc);
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
        if (env.map_knowledge(*ri).changed())
            continue;

        if (env.map_knowledge(*ri).detected_monster())
            count++;
    }

    return (count);
}

void clear_map(bool clear_detected_items, bool clear_detected_monsters)
{
    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        const coord_def p = *ri;
        map_cell& cell = env.map_knowledge(p);
        if (!cell.known() || cell.visible())
            continue;

        if (!clear_detected_items || !cell.detected_item())
            cell.clear_item();

        if ((!clear_detected_monsters || !cell.detected_monster())
                && !mons_class_is_stationary(cell.monster()))
            cell.clear_monster();
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
    // In unmappable areas, this doesn't work since
    // map knowledge gets wiped each turn.
    if (!(cell->flags & MAP_SEEN_FLAG) && player_in_mappable_area())
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
                && overview_knows_num_portals(feat) > 1)
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

    cell->flags &= (~MAP_CHANGED_FLAG);
    cell->flags |= MAP_SEEN_FLAG;

#ifdef USE_TILE
    tiles.update_minimap(x, y);
#endif
}

void set_terrain_visible(const coord_def &c)
{
    map_cell* cell = &env.map_knowledge(c);
    set_terrain_seen(c);
    if (!(cell->flags & MAP_VISIBLE_FLAG))
    {
        cell->flags |= MAP_VISIBLE_FLAG;
        env.visible.insert(c);
    }
    cell->flags &=~ (MAP_DETECTED_MONSTER | MAP_DETECTED_ITEM);
}

void clear_terrain_visibility()
{
    for (std::set<coord_def>::iterator i = env.visible.begin(); i != env.visible.end(); ++i)
        env.map_knowledge(*i).flags &=~ MAP_VISIBLE_FLAG;
    env.visible.clear();
}
