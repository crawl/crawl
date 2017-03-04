#include "AppHdr.h"

#include "map-knowledge.h"

#include "cloud.h"
#include "coordit.h"
#include "directn.h"
#include "env.h"
#include "god-passive.h" // passive_t::auto_map
#include "level-state-type.h"
#include "notes.h"
#include "religion.h"
#include "terrain.h"
#ifdef USE_TILE
 #include "tilepick.h"
 #include "tileview.h"
#endif
#include "travel.h"
#include "view.h"

void set_terrain_mapped(const coord_def gc)
{
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

    return count;
}

void clear_map(bool clear_items, bool clear_mons)
{
    for (rectangle_iterator ri(BOUNDARY_BORDER - 1); ri; ++ri)
    {
        const coord_def p = *ri;
        map_cell& cell = env.map_knowledge(p);
        if (!cell.known() || cell.visible())
            continue;

        cell.clear_cloud();

        if (clear_items)
            cell.clear_item();

        if (clear_mons && !mons_class_is_stationary(cell.monster()))
            cell.clear_monster();

#ifdef USE_TILE
        tile_reset_fg(p);
#endif
    }
}

void clear_map_or_travel_trail()
{
    if (Options.show_travel_trail && env.travel_trail.size())
    {
        mpr("Clearing travel trail.");
        clear_travel_trail();
    }
    else
    {
        mpr("Clearing level map.");
        clear_map();
        crawl_view.set_player_at(you.pos());
    }
}

static void _automap_from(int x, int y, int mutated)
{
    if (mutated)
    {
        const bool godly = have_passive(passive_t::auto_map);
        magic_mapping(8 * mutated,
                      godly ? 25 + you.piety / 8 : 25,
                      true, godly,
                      true, coord_def(x,y));
    }
}

static int _map_quality()
{
    int passive = player_mutation_level(MUT_PASSIVE_MAPPING);
    // the explanation of this 51 vs max_piety of 200 is left as
    // an exercise to the reader
    if (have_passive(passive_t::auto_map))
        passive = max(passive, you.piety / 51);
    return passive;
}

void reautomap_level()
{
    int passive = _map_quality();

    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
            if (env.map_knowledge[x][y].flags & MAP_SEEN_FLAG)
                _automap_from(x, y, passive);
}

void set_terrain_seen(const coord_def pos)
{
    const dungeon_feature_type feat = grd(pos);
    map_cell* cell = &env.map_knowledge(pos);

    // First time we've seen a notable feature.
    if (!(cell->flags & MAP_SEEN_FLAG))
    {
        _automap_from(pos.x, pos.y, _map_quality());

        if (!is_boring_terrain(feat))
        {
            string desc = feature_description_at(pos, false, DESC_A);
            take_note(Note(NOTE_SEEN_FEAT, 0, 0, desc));
        }
    }

    cell->flags &= (~MAP_CHANGED_FLAG);
    cell->flags |= MAP_SEEN_FLAG;

#ifdef USE_TILE
    tiles.update_minimap(pos);
#endif
}

void set_terrain_visible(const coord_def c)
{
    map_cell* cell = &env.map_knowledge(c);
    set_terrain_seen(c);
    if (!(cell->flags & MAP_VISIBLE_FLAG))
    {
        cell->flags |= MAP_VISIBLE_FLAG;
        env.visible.insert(c);
    }
    cell->flags &= ~(MAP_DETECTED_MONSTER | MAP_DETECTED_ITEM);
}

void clear_terrain_visibility()
{
    for (auto c : env.visible)
        env.map_knowledge(c).flags &= ~MAP_VISIBLE_FLAG;
    env.visible.clear();
}

void map_cell::set_detected_item()
{
    clear_item();
    flags |= MAP_DETECTED_ITEM;
    _item = new item_info();
    _item->base_type = OBJ_DETECTED;
    _item->rnd       = 1;
}

static bool _floor_mf(map_feature mf)
{
    return mf == MF_FLOOR || mf == MF_WATER || mf == MF_DEEP_WATER
           || mf == MF_LAVA;
}

/**
 * What map feature is present in the given map cell, for minimap purposes?
 *
 * @param cell  The cell in question.
 * @return      The most important feature in the given cell.
 */
map_feature get_cell_map_feature(const map_cell& cell)
{
    // known but not seen monster
    if (cell.detected_monster())
        return MF_MONS_HOSTILE; // hostile by default

    // known but not seen item
    if (cell.detected_item())
        return MF_ITEM;

    const map_feature base_feature = get_feature_def(cell.feat()).minimap;

    // handle magic mapping etc effects (known but not seen)
    if ((base_feature == MF_WALL || base_feature == MF_FLOOR)
        && cell.known() && !cell.seen())
    {
        return (base_feature == MF_WALL) ? MF_MAP_WALL : MF_MAP_FLOOR;
    }

    // exciting features get precedence...
    if (!_floor_mf(base_feature))
        return base_feature;

    // ... but some things can take precedence over the floor

    // first clouds...
    // XXX: should items have higher priority? (pro: easier to spot un-grabbed
    // items, con: harder to spot what's blocking auto-travel)
    if (cell.cloud())
    {
        show_type show;
        show.cls = SH_CLOUD;
        const map_feature cloud_feature = get_feature_def(show).minimap;
        if (cloud_feature != MF_SKIP) // XXX: does this ever happen?
            return cloud_feature;
    }

    // then items...
    if (cell.item())
    {
        const map_feature item_feature = get_feature_def(*cell.item()).minimap;
        if (item_feature != MF_SKIP) // can this happen?
            return item_feature;
    }

    // then firewood.
    if (cell.monster() != MONS_NO_MONSTER
        && mons_class_is_firewood(cell.monster()))
    {
        return MF_MONS_NO_EXP;
    }

    if (base_feature == MF_SKIP) // can this happen?
        return MF_UNSEEN;
    return base_feature;
}

/// Iter over all known-but-unseen clouds & remove known-gone clouds.
void update_cloud_knowledge()
{
    for (int x = X_BOUND_1; x <= X_BOUND_2; ++x)
    {
        for (int y = Y_BOUND_1; y <= Y_BOUND_2; ++y)
        {
            if (env.map_knowledge[x][y].update_cloud_state())
            {
#ifdef USE_TILE
                tile_draw_map_cell({x, y}, true);
#endif
#ifdef USE_TILE_WEB
                tiles.mark_for_redraw({x, y});
#endif
            }
        }
    }
}

/// If there's a cloud in this cell that we know should be gone, remove it.
/// Returns true if a cloud was removed.
bool map_cell::update_cloud_state()
{
    if (visible())
        return false; // we're already up-to-date

    // player non-opaque clouds vanish instantly out of los
    if (_cloud && _cloud->killer == KILL_YOU_MISSILE
        && !is_opaque_cloud(_cloud->type))
    {
        clear_cloud();
        return true;
    }

    // still winds KOs all clouds, even those out of LOS
    if (_cloud && env.level_state & LSTATE_STILL_WINDS)
    {
        clear_cloud();
        return true;
    }

    // TODO: track decay & vanish appropriately (based on some worst case?)
    return false;
}
